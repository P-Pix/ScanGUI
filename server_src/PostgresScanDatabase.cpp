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

CREATE TABLE IF NOT EXISTS reading_progress (
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    profile TEXT NOT NULL DEFAULT 'default',
    chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
    page_number INTEGER NOT NULL CHECK (page_number > 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (scan_id, profile)
);

CREATE INDEX IF NOT EXISTS idx_pages_scan_chapter ON pages(scan_id, chapter_number, page_number);
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
std::vector<ScanSummaryRecord> PostgresScanDatabase::list_scans() const {
    PGresult* result = exec_params(R"SQL(
SELECT s.id, s.title, s.folder_name, s.root_path, s.download_url,
       COALESCE(COUNT(DISTINCT c.chapter_number), 0) AS chapter_count,
       COALESCE(COUNT(p.page_number), 0) AS page_count
FROM scans s
LEFT JOIN chapters c ON c.scan_id = s.id
LEFT JOIN pages p ON p.scan_id = s.id
GROUP BY s.id, s.title, s.folder_name, s.root_path, s.download_url
ORDER BY lower(s.title)
)SQL", {});

    std::vector<ScanSummaryRecord> scans;
    for (int row = 0; row < PQntuples(result); ++row) {
        scans.push_back({
            value(result, row, 0), value(result, row, 1), value(result, row, 2), value(result, row, 3), value(result, row, 4),
            int_value(result, row, 5), int_value(result, row, 6)
        });
    }
    PQclear(result);
    return scans;
}

std::optional<ScanSummaryRecord> PostgresScanDatabase::get_scan(const std::string& scanId) const {
    PGresult* result = exec_params(R"SQL(
SELECT s.id, s.title, s.folder_name, s.root_path, s.download_url,
       COALESCE(COUNT(DISTINCT c.chapter_number), 0) AS chapter_count,
       COALESCE(COUNT(p.page_number), 0) AS page_count
FROM scans s
LEFT JOIN chapters c ON c.scan_id = s.id
LEFT JOIN pages p ON p.scan_id = s.id
WHERE s.id = $1
GROUP BY s.id, s.title, s.folder_name, s.root_path, s.download_url
)SQL", {scanId});
    if (PQntuples(result) == 0) {
        PQclear(result);
        return std::nullopt;
    }
    ScanSummaryRecord scan{
        value(result, 0, 0), value(result, 0, 1), value(result, 0, 2), value(result, 0, 3), value(result, 0, 4),
        int_value(result, 0, 5), int_value(result, 0, 6)
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

void PostgresScanDatabase::save_progress(const std::string& scanId, const std::string& profile, ScanProgress progress) {
    progress.normalize();
    clear_result(exec_params(
        "INSERT INTO reading_progress(scan_id, profile, chapter_number, page_number, updated_at) "
        "VALUES($1, $2, $3, $4, now()) "
        "ON CONFLICT(scan_id, profile) DO UPDATE SET "
        "chapter_number = EXCLUDED.chapter_number, page_number = EXCLUDED.page_number, updated_at = now()",
        {scanId, profile, std::to_string(progress.chapter), std::to_string(progress.page)}
    ));
}

void PostgresScanDatabase::exec_command(const std::string& sql) const {
    PGresult* result = PQexec(connection_, sql.c_str());
    expect_status(connection_, result, PGRES_COMMAND_OK, sql);
    PQclear(result);
}

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
