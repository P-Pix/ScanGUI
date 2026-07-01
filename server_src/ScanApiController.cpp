/**
 * @file ScanApiController.cpp
 * @brief Routage REST, securite locale, front web et recherche OCR de ScanGUI.
 */
#include "server/ScanApiController.hpp"
#include "server/OcrIndexer.hpp"
#include "infrastructure/SimpleJson.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace {

std::string bjson(bool value) { return value ? "true" : "false"; }

std::optional<int> parse_int(const std::string& value) {
    if (value.empty()) return std::nullopt;
    for (unsigned char c : value) if (!std::isdigit(c)) return std::nullopt;
    try { int v = std::stoi(value); return v > 0 ? std::optional<int>(v) : std::nullopt; } catch (...) { return std::nullopt; }
}

int query_int(const HttpRequest& request, const std::string& key, int fallback) {
    auto it = request.query.find(key);
    if (it == request.query.end()) return fallback;
    auto parsed = parse_int(it->second);
    return parsed.value_or(fallback);
}

std::string query_string(const HttpRequest& request, const std::string& key, std::string fallback = {}) {
    auto it = request.query.find(key);
    return it != request.query.end() && !it->second.empty() ? it->second : fallback;
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string url_path_escape(const std::string& value) {
    const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out.push_back(static_cast<char>(c));
        else { out.push_back('%'); out.push_back(hex[(c >> 4) & 0x0f]); out.push_back(hex[c & 0x0f]); }
    }
    return out;
}

std::string scan_to_json(const ScanSummaryRecord& scan) {
    return "{\"id\":\"" + scangui::json::escape(scan.id) +
        "\",\"title\":\"" + scangui::json::escape(scan.title) +
        "\",\"folderName\":\"" + scangui::json::escape(scan.folderName) +
        "\",\"downloadUrl\":\"" + scangui::json::escape(scan.downloadUrl) +
        "\",\"chapterCount\":" + std::to_string(scan.chapterCount) +
        ",\"pageCount\":" + std::to_string(scan.pageCount) +
        ",\"progressChapter\":" + std::to_string(scan.progressChapter) +
        ",\"progressPage\":" + std::to_string(scan.progressPage) +
        ",\"favorite\":" + bjson(scan.favorite) +
        ",\"lastReadAt\":\"" + scangui::json::escape(scan.lastReadAt) +
        "\",\"coverImageUrl\":\"" + scangui::json::escape(scan.coverImageUrl) + "\"}";
}

std::string bookmark_to_json(const BookmarkRecord& bookmark) {
    return "{\"id\":" + std::to_string(bookmark.id) +
        ",\"scanId\":\"" + scangui::json::escape(bookmark.scanId) +
        "\",\"chapter\":" + std::to_string(bookmark.chapter) +
        ",\"page\":" + std::to_string(bookmark.page) +
        ",\"note\":\"" + scangui::json::escape(bookmark.note) +
        "\",\"createdAt\":\"" + scangui::json::escape(bookmark.createdAt) + "\"}";
}

std::string read_file_binary(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Impossible de lire le fichier: " + path.string());
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string static_content_type(const std::filesystem::path& path) {
    std::string extension = lower_ascii(path.extension().string());
    if (extension == ".html") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js") return "application/javascript; charset=utf-8";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    return "text/plain; charset=utf-8";
}

std::string summarize_text(const std::string& text) {
    if (text.empty()) return "";
    std::string compact;
    bool lastSpace = false;
    for (unsigned char c : text) {
        if (std::isspace(c)) {
            if (!lastSpace) compact.push_back(' ');
            lastSpace = true;
        } else {
            compact.push_back(static_cast<char>(c));
            lastSpace = false;
        }
        if (compact.size() >= 900) break;
    }
    if (compact.size() > 420) compact = compact.substr(0, 420) + "...";
    return compact;
}

} // namespace

ScanApiController::ScanApiController(
    std::filesystem::path scanRoot,
    PostgresScanDatabase& database,
    std::string bindHost,
    std::string adminToken,
    std::filesystem::path webRoot)
    : scanRoot_(std::filesystem::absolute(std::move(scanRoot))),
      webRoot_(std::filesystem::absolute(std::move(webRoot))),
      database_(database),
      bindHost_(std::move(bindHost)),
      adminToken_(std::move(adminToken)) {}

/**
 * @brief Route une requête HTTP vers l'endpoint ScanGUI correspondant.
 *
 * Le mutex protège le repository PostgreSQL partagé par les threads clients du serveur maison.
 *
 * @param request Requête HTTP déjà décodée par `HttpServer`.
 * @return Réponse JSON, fichier ou erreur HTTP.
 */
HttpResponse ScanApiController::handle(const HttpRequest& request) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    try {
        const auto parts = split_path(request.path);
        if (request.method == "GET" && (request.path == "/" || (parts.size() >= 1 && parts[0] == "web"))) return serve_web(request);
        if (request.method == "GET" && parts == std::vector<std::string>{"api", "health"}) return health();
        if (request.method == "GET" && parts == std::vector<std::string>{"api", "version"}) return version();
        if (request.method == "GET" && parts == std::vector<std::string>{"api", "stats"}) return stats();
        if (request.method == "GET" && parts == std::vector<std::string>{"api", "search"}) return search(request);
        if (request.method == "GET" && parts == std::vector<std::string>{"api", "offline", "manifest"}) return offline_manifest(request);
        if (request.method == "POST" && parts == std::vector<std::string>{"api", "admin", "sync"}) return sync_library(request);
        if (request.method == "POST" && parts == std::vector<std::string>{"api", "admin", "ocr"}) return ocr_library(request);

        if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "profiles") {
            if (request.method == "GET" && parts.size() == 2) return list_profiles();
            if (request.method == "POST" && parts.size() == 2) return create_profile(request.body);
            if (request.method == "GET" && parts.size() == 4 && parts[3] == "favorites") return list_favorites(parts[2]);
            if (request.method == "GET" && parts.size() == 4 && parts[3] == "history") return list_history(parts[2], request);
        }

        if (request.method == "GET" && parts == std::vector<std::string>{"api", "scans"}) return list_scans(request);

        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "scans") {
            const auto& scanId = parts[2];
            if (request.method == "GET" && parts.size() == 3) return get_scan(scanId, request);
            if (request.method == "GET" && parts.size() == 4 && parts[3] == "chapters") return list_chapters(scanId);
            if (request.method == "GET" && parts.size() == 6 && parts[3] == "chapters" && parts[5] == "summary") {
                auto chapter = parse_int(parts[4]);
                if (!chapter) return HttpResponse::bad_request("Invalid chapter number");
                return chapter_summary(scanId, *chapter);
            }
            if (request.method == "GET" && parts.size() == 6 && parts[3] == "chapters" && parts[5] == "pages") {
                auto chapter = parse_int(parts[4]);
                if (!chapter) return HttpResponse::bad_request("Invalid chapter number");
                return list_pages(scanId, *chapter, request);
            }
            if (request.method == "GET" && parts.size() == 8 && parts[3] == "chapters" && parts[5] == "pages" && parts[7] == "image") {
                auto chapter = parse_int(parts[4]);
                auto page = parse_int(parts[6]);
                if (!chapter || !page) return HttpResponse::bad_request("Invalid chapter or page number");
                return get_image(scanId, *chapter, *page);
            }
            if ((request.method == "GET" || request.method == "POST" || request.method == "PUT") && parts.size() == 4 && parts[3] == "progress") {
                const std::string profile = query_string(request, "profile", "default");
                return request.method == "GET" ? get_progress(scanId, profile) : save_progress(scanId, profile, request.body);
            }
            if ((request.method == "POST" || request.method == "PUT") && parts.size() == 4 && parts[3] == "favorite") return set_favorite(scanId, query_string(request, "profile", "default"), request.body);
            if (request.method == "GET" && parts.size() == 4 && parts[3] == "bookmarks") return list_bookmarks(scanId, query_string(request, "profile", "default"));
            if (request.method == "POST" && parts.size() == 4 && parts[3] == "bookmarks") return add_bookmark(scanId, query_string(request, "profile", "default"), request.body);
        }

        return HttpResponse::not_found();
    } catch (const std::exception& error) {
        return HttpResponse::server_error(error.what());
    }
}

HttpResponse ScanApiController::health() const {
    return HttpResponse::json(200, "{\"status\":\"ok\",\"service\":\"ScanGUIServer\"}");
}

HttpResponse ScanApiController::version() const {
    return HttpResponse::json(200, "{\"service\":\"ScanGUIServer\",\"version\":\"6.0-full-evolution\",\"apiVersion\":2}");
}

HttpResponse ScanApiController::stats() const {
    auto statsRecord = database_.stats();
    return HttpResponse::json(200,
        "{\"scans\":" + std::to_string(statsRecord.scans) +
        ",\"chapters\":" + std::to_string(statsRecord.chapters) +
        ",\"pages\":" + std::to_string(statsRecord.pages) +
        ",\"profiles\":" + std::to_string(statsRecord.profiles) +
        ",\"favorites\":" + std::to_string(statsRecord.favorites) +
        ",\"bookmarks\":" + std::to_string(statsRecord.bookmarks) +
        ",\"pageTexts\":" + std::to_string(statsRecord.pageTexts) + "}");
}

/**
 * @brief Exécute l'indexation disque vers PostgreSQL après validation admin.
 */
HttpResponse ScanApiController::sync_library(const HttpRequest& request) {
    if (!admin_request_allowed(request)) return HttpResponse::bad_request("Admin sync refused: local bind and valid token required when configured");
    ScanLibraryIndexer indexer(scanRoot_, database_);
    const auto report = indexer.sync();
    return HttpResponse::json(200, "{\"status\":\"synced\",\"scans\":" + std::to_string(report.scans) + ",\"chapters\":" + std::to_string(report.chapters) + ",\"pages\":" + std::to_string(report.pages) + "}");
}

HttpResponse ScanApiController::ocr_library(const HttpRequest& request) {
    if (!admin_request_allowed(request)) return HttpResponse::bad_request("OCR refused: local bind and valid token required when configured");
    OcrIndexer indexer(database_);
    const auto report = indexer.index_all();
    return HttpResponse::json(200, "{\"status\":\"indexed\",\"pagesVisited\":" + std::to_string(report.pagesVisited) + ",\"indexedPages\":" + std::to_string(report.indexedPages) + ",\"skippedPages\":" + std::to_string(report.skippedPages) + ",\"failedPages\":" + std::to_string(report.failedPages) + "}");
}

/**
 * @brief Sérialise le manifeste complet nécessaire à une synchronisation offline.
 */
HttpResponse ScanApiController::offline_manifest(const HttpRequest& request) const {
    const std::string profile = query_string(request, "profile", "default");
    const auto scans = database_.list_scans(profile);
    std::string json = "{\"profile\":\"" + scangui::json::escape(profile) + "\",\"items\":[";
    for (std::size_t i = 0; i < scans.size(); ++i) {
        if (i) json += ',';
        json += scan_to_json(scans[i]);
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

HttpResponse ScanApiController::list_scans(const HttpRequest& request) const {
    const std::string profile = query_string(request, "profile", "default");
    const std::string q = lower_ascii(query_string(request, "q", ""));
    const bool favoritesOnly = query_string(request, "favorites", "0") == "1";
    const std::string sort = query_string(request, "sort", "title");
    int limit = query_int(request, "limit", 0);
    int offset = query_int(request, "offset", 0);

    auto scans = database_.list_scans(profile);
    scans.erase(std::remove_if(scans.begin(), scans.end(), [&](const ScanSummaryRecord& scan) {
        if (favoritesOnly && !scan.favorite) return true;
        if (!q.empty() && lower_ascii(scan.title).find(q) == std::string::npos && lower_ascii(scan.id).find(q) == std::string::npos) return true;
        return false;
    }), scans.end());

    if (sort == "progress") {
        std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.progressChapter, a.progressPage, a.title) > std::tie(b.progressChapter, b.progressPage, b.title); });
    } else if (sort == "chapters") {
        std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.chapterCount, a.title) > std::tie(b.chapterCount, b.title); });
    } else if (sort == "pages") {
        std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.pageCount, a.title) > std::tie(b.pageCount, b.title); });
    } else if (sort == "lastRead") {
        std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.lastReadAt, a.title) > std::tie(b.lastReadAt, b.title); });
    }

    const int total = static_cast<int>(scans.size());
    if (offset < 0) offset = 0;
    if (offset > total) offset = total;
    auto begin = scans.begin() + offset;
    auto end = scans.end();
    if (limit > 0 && offset + limit < total) end = scans.begin() + offset + limit;

    std::string json = "{\"profile\":\"" + scangui::json::escape(profile) + "\",\"total\":" + std::to_string(total) + ",\"items\":[";
    bool first = true;
    for (auto it = begin; it != end; ++it) {
        if (!first) json += ',';
        first = false;
        json += scan_to_json(*it);
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

HttpResponse ScanApiController::get_scan(const std::string& scanId, const HttpRequest& request) const {
    auto scan = database_.get_scan(scanId, query_string(request, "profile", "default"));
    return scan ? HttpResponse::json(200, scan_to_json(*scan)) : HttpResponse::not_found("Scan not found");
}

HttpResponse ScanApiController::list_chapters(const std::string& scanId) const {
    if (!database_.get_scan(scanId)) return HttpResponse::not_found("Scan not found");
    const auto chapters = database_.list_chapters(scanId);
    std::string json = "{\"items\":[";
    for (std::size_t i = 0; i < chapters.size(); ++i) {
        if (i) json += ',';
        json += "{\"chapter\":" + std::to_string(chapters[i].number) + ",\"pageCount\":" + std::to_string(chapters[i].pageCount) + "}";
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

HttpResponse ScanApiController::list_pages(const std::string& scanId, int chapter, const HttpRequest& request) const {
    if (!database_.get_scan(scanId)) return HttpResponse::not_found("Scan not found");
    auto pages = database_.list_pages(scanId, chapter);
    const int total = static_cast<int>(pages.size());
    int offset = query_int(request, "offset", 0);
    int limit = query_int(request, "limit", 0);
    if (offset < 0) offset = 0;
    if (offset > total) offset = total;
    auto begin = pages.begin() + offset;
    auto end = pages.end();
    if (limit > 0 && offset + limit < total) end = pages.begin() + offset + limit;

    std::string json = "{\"total\":" + std::to_string(total) + ",\"items\":[";
    bool first = true;
    for (auto it = begin; it != end; ++it) {
        if (!first) json += ',';
        first = false;
        const std::string imageUrl = "/api/scans/" + url_path_escape(scanId) + "/chapters/" + std::to_string(it->chapter) + "/pages/" + std::to_string(it->page) + "/image";
        json += "{\"chapter\":" + std::to_string(it->chapter) +
            ",\"page\":" + std::to_string(it->page) +
            ",\"mimeType\":\"" + scangui::json::escape(it->mimeType) +
            "\",\"sizeBytes\":" + std::to_string(it->sizeBytes) +
            ",\"imageUrl\":\"" + scangui::json::escape(imageUrl) + "\"}";
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

/**
 * @brief Sert l'image correspondant à une page précise.
 *
 * Sécurité : le chemin renvoyé par PostgreSQL est validé avec `is_safe_page_path` avant toute
 * lecture afin d'empêcher un path traversal ou une ligne de base corrompue de sortir de `scan`.
 */
HttpResponse ScanApiController::get_image(const std::string& scanId, int chapter, int page) const {
    auto record = database_.get_page(scanId, chapter, page);
    if (!record) return HttpResponse::not_found("Page not found");
    if (!is_safe_page_path(record->filePath)) return HttpResponse::bad_request("Unsafe image path rejected");
    return HttpResponse::file(200, record->mimeType, record->filePath.string());
}

HttpResponse ScanApiController::get_progress(const std::string& scanId, const std::string& profile) const {
    if (!database_.get_scan(scanId)) return HttpResponse::not_found("Scan not found");
    auto progress = database_.get_progress(scanId, profile);
    return HttpResponse::json(200, "{\"scanId\":\"" + scangui::json::escape(scanId) + "\",\"profile\":\"" + scangui::json::escape(profile) + "\",\"chapter\":" + std::to_string(progress.chapter) + ",\"page\":" + std::to_string(progress.page) + "}");
}

/**
 * @brief Enregistre la progression de lecture reçue en JSON.
 *
 * Les champs `chapter` et `page` sont obligatoires ; un payload incomplet est rejeté en 400
 * avant l'écriture PostgreSQL.
 */
HttpResponse ScanApiController::save_progress(const std::string& scanId, const std::string& profile, const std::string& body) {
    if (!database_.get_scan(scanId)) return HttpResponse::not_found("Scan not found");
    auto chapter = scangui::json::extract_int(body, "chapter");
    auto page = scangui::json::extract_int(body, "page");
    if (!chapter || !page) return HttpResponse::bad_request("Expected JSON body: {\"chapter\":1,\"page\":1}");
    database_.save_progress(scanId, profile, ScanProgress{*chapter, *page});
    return get_progress(scanId, profile);
}

HttpResponse ScanApiController::list_profiles() const {
    const auto profiles = database_.list_profiles();
    std::string json = "{\"items\":[";
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        if (i) json += ',';
        const auto& profile = profiles[i];
        json += "{\"id\":\"" + scangui::json::escape(profile.id) +
            "\",\"displayName\":\"" + scangui::json::escape(profile.displayName) +
            "\",\"avatarColor\":\"" + scangui::json::escape(profile.avatarColor) +
            "\",\"createdAt\":\"" + scangui::json::escape(profile.createdAt) + "\"}";
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

HttpResponse ScanApiController::create_profile(const std::string& body) {
    const std::string id = scangui::json::string_field(body, "id", "default");
    database_.upsert_profile(ProfileRecord{id, scangui::json::string_field(body, "displayName", id), scangui::json::string_field(body, "avatarColor", "#3b82f6"), ""});
    return HttpResponse::json(200, "{\"status\":\"saved\",\"id\":\"" + scangui::json::escape(id) + "\"}");
}

HttpResponse ScanApiController::list_favorites(const std::string& profile) const {
    const auto favorites = database_.list_favorites(profile);
    std::string json = "{\"profile\":\"" + scangui::json::escape(profile) + "\",\"items\":[";
    for (std::size_t i = 0; i < favorites.size(); ++i) {
        if (i) json += ',';
        json += "\"" + scangui::json::escape(favorites[i]) + "\"";
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

HttpResponse ScanApiController::set_favorite(const std::string& scanId, const std::string& profile, const std::string& body) {
    if (!database_.get_scan(scanId)) return HttpResponse::not_found("Scan not found");
    const bool favorite = scangui::json::extract_bool(body, "favorite").value_or(true);
    database_.set_favorite(scanId, profile, favorite);
    return HttpResponse::json(200, "{\"scanId\":\"" + scangui::json::escape(scanId) + "\",\"profile\":\"" + scangui::json::escape(profile) + "\",\"favorite\":" + bjson(favorite) + "}");
}

HttpResponse ScanApiController::list_bookmarks(const std::string& scanId, const std::string& profile) const {
    if (!database_.get_scan(scanId)) return HttpResponse::not_found("Scan not found");
    const auto bookmarks = database_.list_bookmarks(scanId, profile);
    std::string json = "{\"items\":[";
    for (std::size_t i = 0; i < bookmarks.size(); ++i) {
        if (i) json += ',';
        json += bookmark_to_json(bookmarks[i]);
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

HttpResponse ScanApiController::add_bookmark(const std::string& scanId, const std::string& profile, const std::string& body) {
    if (!database_.get_scan(scanId)) return HttpResponse::not_found("Scan not found");
    auto chapter = scangui::json::extract_int(body, "chapter");
    auto page = scangui::json::extract_int(body, "page");
    if (!chapter || !page) return HttpResponse::bad_request("Expected JSON body: {\"chapter\":1,\"page\":1,\"note\":\"optional\"}");
    auto bookmark = database_.add_bookmark(scanId, profile, ScanProgress{*chapter, *page}, scangui::json::string_field(body, "note"));
    return HttpResponse::json(200, bookmark_to_json(bookmark));
}

HttpResponse ScanApiController::list_history(const std::string& profile, const HttpRequest& request) const {
    const auto history = database_.list_history(profile, query_int(request, "limit", 20));
    std::string json = "{\"items\":[";
    for (std::size_t i = 0; i < history.size(); ++i) {
        if (i) json += ',';
        const auto& item = history[i];
        json += "{\"scanId\":\"" + scangui::json::escape(item.scanId) +
            "\",\"title\":\"" + scangui::json::escape(item.title) +
            "\",\"chapter\":" + std::to_string(item.chapter) +
            ",\"page\":" + std::to_string(item.page) +
            ",\"readAt\":\"" + scangui::json::escape(item.readAt) + "\"}";
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

/**
 * @brief Recherche dans les textes OCR indexés et renvoie des extraits JSON.
 */
HttpResponse ScanApiController::search(const HttpRequest& request) const {
    const std::string query = query_string(request, "q", "");
    if (query.empty()) return HttpResponse::bad_request("Missing q query parameter");
    const auto rows = database_.search_page_text(query, query_int(request, "limit", 50));
    std::string json = "{\"query\":\"" + scangui::json::escape(query) + "\",\"items\":[";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (i) json += ',';
        const auto& row = rows[i];
        json += "{\"scanId\":\"" + scangui::json::escape(row.scanId) +
            "\",\"title\":\"" + scangui::json::escape(row.title) +
            "\",\"chapter\":" + std::to_string(row.chapter) +
            ",\"page\":" + std::to_string(row.page) +
            ",\"snippet\":\"" + scangui::json::escape(row.snippet) + "\"}";
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

HttpResponse ScanApiController::chapter_summary(const std::string& scanId, int chapter) const {
    if (!database_.get_scan(scanId)) return HttpResponse::not_found("Scan not found");
    const auto texts = database_.chapter_text(scanId, chapter);
    std::string joined;
    for (const auto& pageText : texts) {
        if (!joined.empty()) joined += "\n";
        joined += pageText.text;
    }
    return HttpResponse::json(200, "{\"scanId\":\"" + scangui::json::escape(scanId) + "\",\"chapter\":" + std::to_string(chapter) + ",\"summary\":\"" + scangui::json::escape(summarize_text(joined)) + "\"}");
}

HttpResponse ScanApiController::serve_web(const HttpRequest& request) const {
    std::filesystem::path relative = "index.html";
    const auto parts = split_path(request.path);
    if (!parts.empty() && parts[0] == "web") {
        relative.clear();
        for (std::size_t i = 1; i < parts.size(); ++i) relative /= parts[i];
        if (relative.empty()) relative = "index.html";
    }
    const auto file = std::filesystem::weakly_canonical(webRoot_ / relative);
    const auto root = std::filesystem::weakly_canonical(webRoot_);
    const std::string fileText = file.string();
    const std::string rootText = root.string();
    if (fileText.rfind(rootText, 0) != 0 || !std::filesystem::exists(file) || !std::filesystem::is_regular_file(file)) return HttpResponse::not_found("Web asset not found");
    HttpResponse response = HttpResponse::binary(200, static_content_type(file), read_file_binary(file));
    response.headers["Cache-Control"] = "no-cache";
    return response;
}

/**
 * @brief Vérifie qu'une image reste strictement dans le dossier racine des scans.
 *
 * Objectif projet :
 * Protéger l'endpoint `/image` contre les chemins arbitraires, qu'ils proviennent d'une URL
 * malveillante ou d'une ligne PostgreSQL incohérente.
 */
bool ScanApiController::is_safe_page_path(const std::filesystem::path& path) const {
    if (!std::filesystem::exists(path)) return false;
    const auto root = std::filesystem::weakly_canonical(scanRoot_);
    const auto file = std::filesystem::weakly_canonical(path);
    const std::string rootText = root.string();
    const std::string fileText = file.string();
    return fileText == rootText || fileText.rfind(rootText + std::string(1, std::filesystem::path::preferred_separator), 0) == 0;
}

/**
 * @brief Contrôle l'hôte d'écoute et le token optionnel des routes admin.
 */
bool ScanApiController::admin_request_allowed(const HttpRequest& request) const {
    const bool local = bindHost_ == "127.0.0.1" || bindHost_ == "localhost";
    if (!local) return false;
    if (adminToken_.empty()) return true;
    auto it = request.headers.find("x-scangui-admin-token");
    return it != request.headers.end() && it->second == adminToken_;
}
