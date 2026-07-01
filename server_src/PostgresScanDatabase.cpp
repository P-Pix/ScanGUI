/**
 * @file PostgresScanDatabase.cpp
 * @brief Implémente le repository PostgreSQL du serveur local de scans.
 *
 * Ce fichier encapsule libpq, initialise le schéma applicatif et traduit les résultats SQL
 * vers les records utilisés par le contrôleur API et l'indexeur.
 */

#include "server/PostgresScanDatabase.hpp"

#include <libpq-fe.h>

#include <stdexcept>
#include <utility>

namespace {

/**
 * @brief Lit une cellule PostgreSQL sous forme de chaîne sûre.
 *
 * Une valeur SQL NULL est convertie en chaîne vide pour éviter de répéter cette protection
 * dans toutes les conversions de records.
 */
std::string value(PGresult* result, int row, int column) {
    if (PQgetisnull(result, row, column)) {
        return "";
    }
    return PQgetvalue(result, row, column);
}

int int_value(PGresult* result, int row, int column) {
    const std::string text = value(result, row, column);
    return text.empty() ? 0 : std::stoi(text);
}

long long long_value(PGresult* result, int row, int column) {
    const std::string text = value(result, row, column);
    return text.empty() ? 0 : std::stoll(text);
}

/**
 * @brief Vérifie le statut libpq attendu et transforme l'erreur SQL en exception C++.
 *
 * Objectif projet :
 * Centraliser la gestion des échecs PostgreSQL afin que chaque requête échouée libère son
 * résultat et remonte un message exploitable par le serveur.
 */
void expect_status(PGconn* connection, PGresult* result, ExecStatusType expected, const std::string& sql) {
    if (PQresultStatus(result) != expected) {
        const std::string error = PQerrorMessage(connection);
        PQclear(result);
        throw std::runtime_error("PostgreSQL error for query [" + sql + "]: " + error);
    }
}

} // namespace

PostgresScanDatabase::PostgresScanDatabase(std::string connectionString) {
    connection_ = PQconnectdb(connectionString.c_str());
    if (connection_ == nullptr || PQstatus(connection_) != CONNECTION_OK) {
        const std::string error = connection_ ? PQerrorMessage(connection_) : "PQconnectdb returned null";
        if (connection_ != nullptr) {
            PQfinish(connection_);
            connection_ = nullptr;
        }
        throw std::runtime_error("Impossible de se connecter a PostgreSQL: " + error);
    }
}

PostgresScanDatabase::~PostgresScanDatabase() {
    if (connection_ != nullptr) {
        PQfinish(connection_);
        connection_ = nullptr;
    }
}

/**
 * @brief Crée le schéma PostgreSQL minimal si nécessaire.
 *
 * Objectif projet :
 * Permettre au serveur local de démarrer sur une base vide sans migration externe obligatoire
 * pour les usages personnels et de développement.
 */
void PostgresScanDatabase::initialize_schema() {
    exec_command(R"SQL(
CREATE TABLE IF NOT EXISTS scans (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    folder_name TEXT NOT NULL,
    root_path TEXT NOT NULL,
    download_url TEXT NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS chapters (
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
    path TEXT NOT NULL,
    PRIMARY KEY (scan_id, chapter_number)
);

CREATE TABLE IF NOT EXISTS pages (
    scan_id TEXT NOT NULL,
    chapter_number INTEGER NOT NULL,
    page_number INTEGER NOT NULL CHECK (page_number > 0),
    file_path TEXT NOT NULL,
    mime_type TEXT NOT NULL,
    size_bytes BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (scan_id, chapter_number, page_number),
    FOREIGN KEY (scan_id, chapter_number) REFERENCES chapters(scan_id, chapter_number) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS profiles (
    id TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    avatar_color TEXT NOT NULL DEFAULT '#3b82f6',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

INSERT INTO profiles(id, display_name, avatar_color)
VALUES('default', 'Default', '#3b82f6')
ON CONFLICT(id) DO NOTHING;

CREATE TABLE IF NOT EXISTS reading_progress (
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    profile TEXT NOT NULL DEFAULT 'default',
    chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
    page_number INTEGER NOT NULL CHECK (page_number > 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (scan_id, profile)
);

CREATE TABLE IF NOT EXISTS favorites (
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    profile TEXT NOT NULL DEFAULT 'default',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (scan_id, profile)
);

CREATE TABLE IF NOT EXISTS bookmarks (
    id BIGSERIAL PRIMARY KEY,
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    profile TEXT NOT NULL DEFAULT 'default',
    chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
    page_number INTEGER NOT NULL CHECK (page_number > 0),
    note TEXT NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS reading_history (
    id BIGSERIAL PRIMARY KEY,
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    profile TEXT NOT NULL DEFAULT 'default',
    chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
    page_number INTEGER NOT NULL CHECK (page_number > 0),
    read_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS page_texts (
    scan_id TEXT NOT NULL,
    chapter_number INTEGER NOT NULL,
    page_number INTEGER NOT NULL,
    page_text TEXT NOT NULL DEFAULT '',
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (scan_id, chapter_number, page_number),
    FOREIGN KEY (scan_id, chapter_number, page_number) REFERENCES pages(scan_id, chapter_number, page_number) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_pages_scan_chapter ON pages(scan_id, chapter_number, page_number);
CREATE INDEX IF NOT EXISTS idx_history_profile ON reading_history(profile, read_at DESC);
CREATE INDEX IF NOT EXISTS idx_page_texts_scan_chapter ON page_texts(scan_id, chapter_number, page_number);
)SQL");
}

void PostgresScanDatabase::begin() {
    exec_command("BEGIN");
}

void PostgresScanDatabase::commit() {
    exec_command("COMMIT");
}

/**
 * @brief Annule une transaction sans propager d'exception.
 *
 * Ce helper est utilisé dans les blocs de synchronisation afin de préserver l'exception
 * initiale tout en tentant de remettre la connexion dans un état cohérent.
 */
void PostgresScanDatabase::rollback_noexcept() {
    try {
        exec_command("ROLLBACK");
    } catch (...) {
    }
}

void PostgresScanDatabase::upsert_scan(const ScanSummaryRecord& scan) {
    clear_result(exec_params(
        "INSERT INTO scans(id, title, folder_name, root_path, download_url, updated_at) "
        "VALUES($1, $2, $3, $4, $5, now()) "
        "ON CONFLICT(id) DO UPDATE SET "
        "title = EXCLUDED.title, folder_name = EXCLUDED.folder_name, root_path = EXCLUDED.root_path, "
        "download_url = EXCLUDED.download_url, updated_at = now()",
        {scan.id, scan.title, scan.folderName, scan.rootPath, scan.downloadUrl}
    ));
}

void PostgresScanDatabase::delete_scan_content(const std::string& scanId) {
    clear_result(exec_params("DELETE FROM chapters WHERE scan_id = $1", {scanId}));
}

void PostgresScanDatabase::insert_chapter(const std::string& scanId, int chapterNumber, const std::filesystem::path& path) {
    clear_result(exec_params(
        "INSERT INTO chapters(scan_id, chapter_number, path) VALUES($1, $2, $3) "
        "ON CONFLICT(scan_id, chapter_number) DO UPDATE SET path = EXCLUDED.path",
        {scanId, std::to_string(chapterNumber), path.string()}
    ));
}

void PostgresScanDatabase::insert_page(const std::string& scanId, const PageRecord& page) {
    clear_result(exec_params(
        "INSERT INTO pages(scan_id, chapter_number, page_number, file_path, mime_type, size_bytes) "
        "VALUES($1, $2, $3, $4, $5, $6) "
        "ON CONFLICT(scan_id, chapter_number, page_number) DO UPDATE SET "
        "file_path = EXCLUDED.file_path, mime_type = EXCLUDED.mime_type, size_bytes = EXCLUDED.size_bytes",
        {scanId, std::to_string(page.chapter), std::to_string(page.page), page.filePath.string(), page.mimeType, std::to_string(page.sizeBytes)}
    ));
}

/**
 * @brief Liste les scans indexés avec leurs compteurs de chapitres et pages.
 *
 * Objectif projet :
 * Alimenter l'endpoint de bibliothèque avec une vue déjà agrégée côté base, sans recalculer
 * ces compteurs dans le contrôleur.
 *
 * @return Scans triés par titre lisible.
 */
std::vector<ScanSummaryRecord> PostgresScanDatabase::list_scans(const std::string& profile) const {
    PGresult* result = exec_params(R"SQL(
SELECT s.id, s.title, s.folder_name, s.root_path, s.download_url,
       COALESCE(COUNT(DISTINCT c.chapter_number), 0) AS chapter_count,
       COALESCE(COUNT(pg.page_number), 0) AS page_count,
       COALESCE(rp.chapter_number, 1) AS progress_chapter,
       COALESCE(rp.page_number, 1) AS progress_page,
       EXISTS(SELECT 1 FROM favorites f WHERE f.scan_id = s.id AND f.profile = $1) AS favorite,
       COALESCE((SELECT h.read_at::text FROM reading_history h WHERE h.scan_id = s.id AND h.profile = $1 ORDER BY h.read_at DESC LIMIT 1), '') AS last_read_at,
       COALESCE((SELECT '/api/scans/' || s.id || '/chapters/' || p2.chapter_number || '/pages/' || p2.page_number || '/image'
                 FROM pages p2 WHERE p2.scan_id = s.id ORDER BY p2.chapter_number, p2.page_number LIMIT 1), '') AS cover_image_url
FROM scans s
LEFT JOIN chapters c ON c.scan_id = s.id
LEFT JOIN pages pg ON pg.scan_id = s.id
LEFT JOIN reading_progress rp ON rp.scan_id = s.id AND rp.profile = $1
GROUP BY s.id, s.title, s.folder_name, s.root_path, s.download_url, rp.chapter_number, rp.page_number
ORDER BY lower(s.title)
)SQL", {profile});

    std::vector<ScanSummaryRecord> scans;
    for (int row = 0; row < PQntuples(result); ++row) {
        scans.push_back({
            value(result, row, 0), value(result, row, 1), value(result, row, 2), value(result, row, 3), value(result, row, 4),
            int_value(result, row, 5), int_value(result, row, 6), int_value(result, row, 7), int_value(result, row, 8),
            value(result, row, 9) == "t", value(result, row, 10), value(result, row, 11)
        });
    }
    PQclear(result);
    return scans;
}

std::optional<ScanSummaryRecord> PostgresScanDatabase::get_scan(const std::string& scanId, const std::string& profile) const {
    PGresult* result = exec_params(R"SQL(
SELECT s.id, s.title, s.folder_name, s.root_path, s.download_url,
       COALESCE(COUNT(DISTINCT c.chapter_number), 0) AS chapter_count,
       COALESCE(COUNT(pg.page_number), 0) AS page_count,
       COALESCE(rp.chapter_number, 1) AS progress_chapter,
       COALESCE(rp.page_number, 1) AS progress_page,
       EXISTS(SELECT 1 FROM favorites f WHERE f.scan_id = s.id AND f.profile = $2) AS favorite,
       COALESCE((SELECT h.read_at::text FROM reading_history h WHERE h.scan_id = s.id AND h.profile = $2 ORDER BY h.read_at DESC LIMIT 1), '') AS last_read_at,
       COALESCE((SELECT '/api/scans/' || s.id || '/chapters/' || p2.chapter_number || '/pages/' || p2.page_number || '/image'
                 FROM pages p2 WHERE p2.scan_id = s.id ORDER BY p2.chapter_number, p2.page_number LIMIT 1), '') AS cover_image_url
FROM scans s
LEFT JOIN chapters c ON c.scan_id = s.id
LEFT JOIN pages pg ON pg.scan_id = s.id
LEFT JOIN reading_progress rp ON rp.scan_id = s.id AND rp.profile = $2
WHERE s.id = $1
GROUP BY s.id, s.title, s.folder_name, s.root_path, s.download_url, rp.chapter_number, rp.page_number
)SQL", {scanId, profile});
    if (PQntuples(result) == 0) {
        PQclear(result);
        return std::nullopt;
    }
    ScanSummaryRecord scan{
        value(result, 0, 0), value(result, 0, 1), value(result, 0, 2), value(result, 0, 3), value(result, 0, 4),
        int_value(result, 0, 5), int_value(result, 0, 6), int_value(result, 0, 7), int_value(result, 0, 8),
        value(result, 0, 9) == "t", value(result, 0, 10), value(result, 0, 11)
    };
    PQclear(result);
    return scan;
}

std::vector<ChapterRecord> PostgresScanDatabase::list_chapters(const std::string& scanId) const {
    PGresult* result = exec_params(R"SQL(
SELECT c.chapter_number, COALESCE(COUNT(p.page_number), 0) AS page_count
FROM chapters c
LEFT JOIN pages p ON p.scan_id = c.scan_id AND p.chapter_number = c.chapter_number
WHERE c.scan_id = $1
GROUP BY c.chapter_number
ORDER BY c.chapter_number
)SQL", {scanId});
    std::vector<ChapterRecord> chapters;
    for (int row = 0; row < PQntuples(result); ++row) {
        chapters.push_back({int_value(result, row, 0), int_value(result, row, 1)});
    }
    PQclear(result);
    return chapters;
}

std::vector<PageRecord> PostgresScanDatabase::list_pages(const std::string& scanId, int chapterNumber) const {
    PGresult* result = exec_params(
        "SELECT chapter_number, page_number, file_path, mime_type, size_bytes FROM pages "
        "WHERE scan_id = $1 AND chapter_number = $2 ORDER BY page_number",
        {scanId, std::to_string(chapterNumber)}
    );
    std::vector<PageRecord> pages;
    for (int row = 0; row < PQntuples(result); ++row) {
        pages.push_back({int_value(result, row, 0), int_value(result, row, 1), value(result, row, 2), value(result, row, 3), long_value(result, row, 4)});
    }
    PQclear(result);
    return pages;
}

std::optional<PageRecord> PostgresScanDatabase::get_page(const std::string& scanId, int chapterNumber, int pageNumber) const {
    PGresult* result = exec_params(
        "SELECT chapter_number, page_number, file_path, mime_type, size_bytes FROM pages "
        "WHERE scan_id = $1 AND chapter_number = $2 AND page_number = $3",
        {scanId, std::to_string(chapterNumber), std::to_string(pageNumber)}
    );
    if (PQntuples(result) == 0) {
        PQclear(result);
        return std::nullopt;
    }
    PageRecord page{int_value(result, 0, 0), int_value(result, 0, 1), value(result, 0, 2), value(result, 0, 3), long_value(result, 0, 4)};
    PQclear(result);
    return page;
}

ScanProgress PostgresScanDatabase::get_progress(const std::string& scanId, const std::string& profile) const {
    PGresult* result = exec_params(
        "SELECT chapter_number, page_number FROM reading_progress WHERE scan_id = $1 AND profile = $2",
        {scanId, profile}
    );
    ScanProgress progress{1, 1};
    if (PQntuples(result) > 0) {
        progress.chapter = int_value(result, 0, 0);
        progress.page = int_value(result, 0, 1);
        progress.normalize();
    }
    PQclear(result);
    return progress;
}

/**
 * @brief Enregistre la progression et ajoute une entrée d'historique.
 *
 * Objectif projet :
 * Garantir que la reprise rapide et l'écran d'historique restent cohérents après chaque
 * navigation sauvegardée par le client.
 */
void PostgresScanDatabase::save_progress(const std::string& scanId, const std::string& profile, ScanProgress progress) {
    progress.normalize();
    upsert_profile(ProfileRecord{profile, profile, "#3b82f6", ""});
    clear_result(exec_params(
        "INSERT INTO reading_progress(scan_id, profile, chapter_number, page_number, updated_at) "
        "VALUES($1, $2, $3, $4, now()) "
        "ON CONFLICT(scan_id, profile) DO UPDATE SET "
        "chapter_number = EXCLUDED.chapter_number, page_number = EXCLUDED.page_number, updated_at = now()",
        {scanId, profile, std::to_string(progress.chapter), std::to_string(progress.page)}
    ));
    clear_result(exec_params(
        "INSERT INTO reading_history(scan_id, profile, chapter_number, page_number) VALUES($1, $2, $3, $4)",
        {scanId, profile, std::to_string(progress.chapter), std::to_string(progress.page)}
    ));
}

std::vector<ProfileRecord> PostgresScanDatabase::list_profiles() const {
    PGresult* result = exec_params("SELECT id, display_name, avatar_color, created_at::text FROM profiles ORDER BY lower(display_name)", {});
    std::vector<ProfileRecord> profiles;
    for (int row = 0; row < PQntuples(result); ++row) {
        profiles.push_back({value(result, row, 0), value(result, row, 1), value(result, row, 2), value(result, row, 3)});
    }
    PQclear(result);
    return profiles;
}

void PostgresScanDatabase::upsert_profile(const ProfileRecord& profile) {
    const std::string id = profile.id.empty() ? "default" : profile.id;
    const std::string displayName = profile.displayName.empty() ? id : profile.displayName;
    const std::string avatarColor = profile.avatarColor.empty() ? "#3b82f6" : profile.avatarColor;
    clear_result(exec_params("INSERT INTO profiles(id, display_name, avatar_color) VALUES($1, $2, $3) ON CONFLICT(id) DO UPDATE SET display_name = EXCLUDED.display_name, avatar_color = EXCLUDED.avatar_color", {id, displayName, avatarColor}));
}

std::vector<std::string> PostgresScanDatabase::list_favorites(const std::string& profile) const {
    PGresult* result = exec_params("SELECT scan_id FROM favorites WHERE profile = $1 ORDER BY created_at DESC", {profile});
    std::vector<std::string> favorites;
    for (int row = 0; row < PQntuples(result); ++row) favorites.push_back(value(result, row, 0));
    PQclear(result);
    return favorites;
}


bool PostgresScanDatabase::is_favorite(const std::string& scanId, const std::string& profile) const {
    PGresult* result = exec_params("SELECT 1 FROM favorites WHERE scan_id = $1 AND profile = $2", {scanId, profile});
    const bool found = PQntuples(result) > 0;
    PQclear(result);
    return found;
}

std::string PostgresScanDatabase::last_read_at(const std::string& scanId, const std::string& profile) const {
    PGresult* result = exec_params("SELECT read_at::text FROM reading_history WHERE scan_id = $1 AND profile = $2 ORDER BY read_at DESC LIMIT 1", {scanId, profile});
    std::string valueText;
    if (PQntuples(result) > 0) valueText = value(result, 0, 0);
    PQclear(result);
    return valueText;
}

void PostgresScanDatabase::set_favorite(const std::string& scanId, const std::string& profile, bool favorite) {
    upsert_profile(ProfileRecord{profile, profile, "#3b82f6", ""});
    if (favorite) clear_result(exec_params("INSERT INTO favorites(scan_id, profile) VALUES($1, $2) ON CONFLICT(scan_id, profile) DO NOTHING", {scanId, profile}));
    else clear_result(exec_params("DELETE FROM favorites WHERE scan_id = $1 AND profile = $2", {scanId, profile}));
}

std::vector<BookmarkRecord> PostgresScanDatabase::list_bookmarks(const std::string& scanId, const std::string& profile) const {
    PGresult* result = exec_params("SELECT id, scan_id, chapter_number, page_number, note, created_at::text FROM bookmarks WHERE scan_id = $1 AND profile = $2 ORDER BY chapter_number, page_number, created_at DESC", {scanId, profile});
    std::vector<BookmarkRecord> bookmarks;
    for (int row = 0; row < PQntuples(result); ++row) bookmarks.push_back({long_value(result,row,0), value(result,row,1), int_value(result,row,2), int_value(result,row,3), value(result,row,4), value(result,row,5)});
    PQclear(result);
    return bookmarks;
}

BookmarkRecord PostgresScanDatabase::add_bookmark(const std::string& scanId, const std::string& profile, ScanProgress progress, const std::string& note) {
    progress.normalize();
    upsert_profile(ProfileRecord{profile, profile, "#3b82f6", ""});
    PGresult* result = exec_params("INSERT INTO bookmarks(scan_id, profile, chapter_number, page_number, note) VALUES($1,$2,$3,$4,$5) RETURNING id, scan_id, chapter_number, page_number, note, created_at::text", {scanId, profile, std::to_string(progress.chapter), std::to_string(progress.page), note});
    BookmarkRecord bookmark{long_value(result,0,0), value(result,0,1), int_value(result,0,2), int_value(result,0,3), value(result,0,4), value(result,0,5)};
    PQclear(result);
    return bookmark;
}

std::vector<HistoryRecord> PostgresScanDatabase::list_history(const std::string& profile, int limit) const {
    if (limit < 1) limit = 20;
    PGresult* result = exec_params("SELECT h.scan_id, s.title, h.chapter_number, h.page_number, h.read_at::text FROM reading_history h JOIN scans s ON s.id = h.scan_id WHERE h.profile = $1 ORDER BY h.read_at DESC LIMIT " + std::to_string(limit), {profile});
    std::vector<HistoryRecord> history;
    for (int row = 0; row < PQntuples(result); ++row) history.push_back({value(result,row,0), value(result,row,1), int_value(result,row,2), int_value(result,row,3), value(result,row,4)});
    PQclear(result);
    return history;
}

void PostgresScanDatabase::upsert_page_text(const std::string& scanId, int chapter, int page, const std::string& text) {
    clear_result(exec_params(
        "INSERT INTO page_texts(scan_id, chapter_number, page_number, page_text, updated_at) VALUES($1,$2,$3,$4,now()) "
        "ON CONFLICT(scan_id, chapter_number, page_number) DO UPDATE SET page_text = EXCLUDED.page_text, updated_at = now()",
        {scanId, std::to_string(chapter), std::to_string(page), text}
    ));
}

std::vector<SearchResultRecord> PostgresScanDatabase::search_page_text(const std::string& query, int limit) const {
    if (limit < 1) limit = 50;
    if (limit > 200) limit = 200;
    const std::string pattern = "%" + query + "%";
    PGresult* result = exec_params(
        "SELECT pt.scan_id, s.title, pt.chapter_number, pt.page_number, substring(pt.page_text for 260) "
        "FROM page_texts pt JOIN scans s ON s.id = pt.scan_id "
        "WHERE pt.page_text ILIKE $1 "
        "ORDER BY s.title, pt.chapter_number, pt.page_number LIMIT " + std::to_string(limit),
        {pattern}
    );
    std::vector<SearchResultRecord> rows;
    for (int row = 0; row < PQntuples(result); ++row) {
        rows.push_back({value(result,row,0), value(result,row,1), int_value(result,row,2), int_value(result,row,3), value(result,row,4)});
    }
    PQclear(result);
    return rows;
}

std::vector<PageTextRecord> PostgresScanDatabase::chapter_text(const std::string& scanId, int chapter) const {
    PGresult* result = exec_params(
        "SELECT scan_id, chapter_number, page_number, page_text FROM page_texts WHERE scan_id = $1 AND chapter_number = $2 ORDER BY page_number",
        {scanId, std::to_string(chapter)}
    );
    std::vector<PageTextRecord> rows;
    for (int row = 0; row < PQntuples(result); ++row) {
        rows.push_back({value(result,row,0), int_value(result,row,1), int_value(result,row,2), value(result,row,3)});
    }
    PQclear(result);
    return rows;
}

LibraryStatsRecord PostgresScanDatabase::stats() const {
    PGresult* result = exec_params("SELECT (SELECT COUNT(*) FROM scans),(SELECT COUNT(*) FROM chapters),(SELECT COUNT(*) FROM pages),(SELECT COUNT(*) FROM profiles),(SELECT COUNT(*) FROM favorites),(SELECT COUNT(*) FROM bookmarks),(SELECT COUNT(*) FROM page_texts)", {});
    LibraryStatsRecord stats;
    if (PQntuples(result)>0) { stats.scans=int_value(result,0,0); stats.chapters=int_value(result,0,1); stats.pages=int_value(result,0,2); stats.profiles=int_value(result,0,3); stats.favorites=int_value(result,0,4); stats.bookmarks=int_value(result,0,5); stats.pageTexts=int_value(result,0,6); }
    PQclear(result);
    return stats;
}

void PostgresScanDatabase::exec_command(const std::string& sql) const {
    PGresult* result = PQexec(connection_, sql.c_str());
    expect_status(connection_, result, PGRES_COMMAND_OK, sql);
    PQclear(result);
}

/**
 * @brief Exécute une requête SQL paramétrée et vérifie son statut.
 *
 * Sécurité : les paramètres sont transmis à libpq séparément du SQL pour éviter les injections
 * sur les valeurs issues des routes API ou des noms de scans.
 *
 * @throws std::runtime_error en cas d'échec PostgreSQL.
 */
PGresult* PostgresScanDatabase::exec_params(const std::string& sql, const std::vector<std::string>& params) const {
    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& param : params) {
        values.push_back(param.c_str());
    }
    PGresult* result = PQexecParams(
        connection_, sql.c_str(), static_cast<int>(values.size()), nullptr,
        values.empty() ? nullptr : values.data(), nullptr, nullptr, 0
    );
    const ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        const std::string error = PQerrorMessage(connection_);
        PQclear(result);
        throw std::runtime_error("PostgreSQL error for query [" + sql + "]: " + error);
    }
    return result;
}

void PostgresScanDatabase::clear_result(PGresult* result) {
    if (result != nullptr) {
        PQclear(result);
    }
}
