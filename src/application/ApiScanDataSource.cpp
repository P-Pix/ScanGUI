/**
 * @file ApiScanDataSource.cpp
 * @brief Source de donnees cliente basee sur l'API HTTP locale ScanGUIServer.
 */
#include "application/ApiScanDataSource.hpp"
#include "infrastructure/SimpleJson.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

std::vector<std::string> extract_items_objects(const std::string& json) {
    return scangui::json::extract_object_array(json, "items");
}

std::string string_field(const std::string& json, const std::string& key, std::string fallback = {}) {
    return scangui::json::string_field(json, key, std::move(fallback));
}

int int_field(const std::string& json, const std::string& key, int fallback = 0) {
    return scangui::json::int_field(json, key, fallback);
}

long long long_field(const std::string& json, const std::string& key) {
    return scangui::json::long_field(json, key);
}

bool bool_field(const std::string& json, const std::string& key, bool fallback = false) {
    return scangui::json::extract_bool(json, key).value_or(fallback);
}

std::string extension_for_mime(const std::string& mimeType) {
    if (mimeType == "image/png") return ".png";
    if (mimeType == "image/webp") return ".webp";
    return ".jpg";
}

} // namespace

ApiScanDataSource::ApiScanDataSource(std::string baseUrl, std::filesystem::path cacheRoot)
    : baseUrl_(std::move(baseUrl)), cacheRoot_(std::move(cacheRoot)) {
    while (!baseUrl_.empty() && baseUrl_.back() == '/') baseUrl_.pop_back();
}

/**
 * @brief Charge les scans depuis `/api/scans` et les convertit vers le modèle GUI.
 *
 * @param profile Profil transmis au serveur pour inclure progression et favoris.
 * @return Liste normalisée de scans.
 */
std::vector<ScanSummary> ApiScanDataSource::list_scans(const std::string& profile) const {
    const std::string json = http_.get_text(endpoint("/api/scans?profile=" + url_encode(profile)));
    std::vector<ScanSummary> scans;
    for (const auto& object : extract_items_objects(json)) {
        const std::string id = string_field(object, "id");
        if (id.empty()) continue;
        ScanSummary summary{id, string_field(object, "title"), int_field(object, "chapterCount"), int_field(object, "pageCount")};
        summary.progress = {int_field(object, "progressChapter", 1), int_field(object, "progressPage", 1)};
        summary.progress.normalize();
        summary.favorite = bool_field(object, "favorite");
        summary.lastReadAt = string_field(object, "lastReadAt");
        summary.coverImageUrl = string_field(object, "coverImageUrl");
        scans.push_back(summary);
    }
    return scans;
}

/** @brief Charge les chapitres d'un scan depuis l'API. */
std::vector<int> ApiScanDataSource::list_chapters(const std::string& scanId) const {
    const std::string json = http_.get_text(endpoint("/api/scans/" + url_encode(scanId) + "/chapters"));
    std::vector<int> chapters;
    for (const auto& object : extract_items_objects(json)) {
        const int chapter = int_field(object, "chapter");
        if (chapter > 0) chapters.push_back(chapter);
    }
    return chapters;
}

/** @brief Charge les pages d'un chapitre et leurs URLs d'image. */
std::vector<ScanPageInfo> ApiScanDataSource::list_pages(const std::string& scanId, int chapter) const {
    const std::string json = http_.get_text(endpoint("/api/scans/" + url_encode(scanId) + "/chapters/" + std::to_string(chapter) + "/pages"));
    std::vector<ScanPageInfo> pages;
    for (const auto& object : extract_items_objects(json)) {
        ScanPageInfo page;
        page.chapter = int_field(object, "chapter");
        page.page = int_field(object, "page");
        page.mimeType = string_field(object, "mimeType");
        page.sizeBytes = long_field(object, "sizeBytes");
        page.imageUrl = string_field(object, "imageUrl");
        if (page.chapter > 0 && page.page > 0) pages.push_back(page);
    }
    return pages;
}

/**
 * @brief Télécharge l'image d'une page dans le cache local si nécessaire.
 *
 * Objectif projet :
 * Conserver un affichage GTK basé sur des fichiers même lorsque les images viennent de l'API.
 *
 * @return Chemin du fichier en cache.
 */
std::filesystem::path ApiScanDataSource::materialize_page(const std::string& scanId, ScanProgress progress) const {
    progress.normalize();
    const auto pages = list_pages(scanId, progress.chapter);
    auto it = std::find_if(pages.begin(), pages.end(), [&](const ScanPageInfo& page) { return page.page == progress.page; });
    if (it == pages.end()) throw std::runtime_error("Page introuvable via l'API: " + scanId);

    const auto output = cacheRoot_ / scanId / std::to_string(progress.chapter) / (std::to_string(progress.page) + extension_for_mime(it->mimeType));
    if (!std::filesystem::exists(output)) http_.download_file(endpoint(it->imageUrl), output);
    return output;
}

/** @brief Récupère la progression d'un profil depuis le serveur. */
ScanProgress ApiScanDataSource::load_progress(const std::string& scanId, const std::string& profile) const {
    const std::string json = http_.get_text(endpoint("/api/scans/" + url_encode(scanId) + "/progress?profile=" + url_encode(profile)));
    ScanProgress progress{int_field(json, "chapter", 1), int_field(json, "page", 1)};
    progress.normalize();
    return progress;
}

/**
 * @brief Envoie la progression courante au serveur via l'endpoint `/progress`.
 */
void ApiScanDataSource::save_progress(const std::string& scanId, ScanProgress progress, const std::string& profile) const {
    progress.normalize();
    const std::string body = "{\"chapter\":" + std::to_string(progress.chapter) + ",\"page\":" + std::to_string(progress.page) + "}";
    (void)http_.post_json(endpoint("/api/scans/" + url_encode(scanId) + "/progress?profile=" + url_encode(profile)), body);
}

std::vector<ProfileSummary> ApiScanDataSource::list_profiles() const {
    const std::string json = http_.get_text(endpoint("/api/profiles"));
    std::vector<ProfileSummary> profiles;
    for (const auto& object : extract_items_objects(json)) {
        const std::string id = string_field(object, "id");
        if (!id.empty()) profiles.push_back({id, string_field(object, "displayName", id), string_field(object, "avatarColor", "#3b82f6")});
    }
    if (profiles.empty()) profiles.push_back({"default", "Default", "#3b82f6"});
    return profiles;
}

void ApiScanDataSource::save_profile(const ProfileSummary& profile) const {
    const std::string id = profile.id.empty() ? "default" : profile.id;
    const std::string body = "{\"id\":\"" + scangui::json::escape(id) + "\",\"displayName\":\"" + scangui::json::escape(profile.displayName.empty() ? id : profile.displayName) + "\",\"avatarColor\":\"" + scangui::json::escape(profile.avatarColor.empty() ? "#3b82f6" : profile.avatarColor) + "\"}";
    (void)http_.post_json(endpoint("/api/profiles"), body);
}

std::vector<std::string> ApiScanDataSource::list_favorites(const std::string& profile) const {
    const std::string json = http_.get_text(endpoint("/api/profiles/" + url_encode(profile) + "/favorites"));
    std::vector<std::string> favorites;
    auto root = scangui::json::parse(json);
    if (!root) return favorites;
    const auto* items = scangui::json::find_field(*root, "items");
    if (items == nullptr || !items->is_array()) return favorites;
    for (const auto& item : items->as_array()) if (item.is_string()) favorites.push_back(item.as_string());
    return favorites;
}

void ApiScanDataSource::set_favorite(const std::string& scanId, bool favorite, const std::string& profile) const {
    const std::string body = std::string("{\"favorite\":") + (favorite ? "true" : "false") + "}";
    (void)http_.post_json(endpoint("/api/scans/" + url_encode(scanId) + "/favorite?profile=" + url_encode(profile)), body);
}

std::vector<BookmarkSummary> ApiScanDataSource::list_bookmarks(const std::string& scanId, const std::string& profile) const {
    const std::string json = http_.get_text(endpoint("/api/scans/" + url_encode(scanId) + "/bookmarks?profile=" + url_encode(profile)));
    std::vector<BookmarkSummary> bookmarks;
    for (const auto& object : extract_items_objects(json)) {
        bookmarks.push_back({long_field(object, "id"), string_field(object, "scanId", scanId), int_field(object, "chapter", 1), int_field(object, "page", 1), string_field(object, "note"), string_field(object, "createdAt")});
    }
    return bookmarks;
}

BookmarkSummary ApiScanDataSource::add_bookmark(const std::string& scanId, ScanProgress progress, const std::string& note, const std::string& profile) const {
    progress.normalize();
    const std::string body = "{\"chapter\":" + std::to_string(progress.chapter) + ",\"page\":" + std::to_string(progress.page) + ",\"note\":\"" + scangui::json::escape(note) + "\"}";
    const std::string json = http_.post_json(endpoint("/api/scans/" + url_encode(scanId) + "/bookmarks?profile=" + url_encode(profile)), body);
    return {long_field(json, "id"), string_field(json, "scanId", scanId), int_field(json, "chapter", progress.chapter), int_field(json, "page", progress.page), string_field(json, "note", note), string_field(json, "createdAt")};
}

std::vector<HistoryEntry> ApiScanDataSource::list_history(const std::string& profile, int limit) const {
    const std::string json = http_.get_text(endpoint("/api/profiles/" + url_encode(profile) + "/history?limit=" + std::to_string(limit)));
    std::vector<HistoryEntry> history;
    for (const auto& object : extract_items_objects(json)) {
        history.push_back({string_field(object, "scanId"), string_field(object, "title"), int_field(object, "chapter", 1), int_field(object, "page", 1), string_field(object, "readAt")});
    }
    return history;
}

std::vector<SearchResultSummary> ApiScanDataSource::search_text(const std::string& query, int limit) const {
    if (query.empty()) return {};
    const std::string json = http_.get_text(endpoint("/api/search?q=" + url_encode(query) + "&limit=" + std::to_string(limit)));
    std::vector<SearchResultSummary> results;
    for (const auto& object : extract_items_objects(json)) {
        results.push_back({string_field(object, "scanId"), string_field(object, "title"), int_field(object, "chapter", 1), int_field(object, "page", 1), string_field(object, "snippet"), "api"});
    }
    return results;
}

std::string ApiScanDataSource::endpoint(const std::string& path) const {
    if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) return path;
    if (!path.empty() && path.front() == '/') return baseUrl_ + path;
    return baseUrl_ + '/' + path;
}

/**
 * @brief Encode une valeur destinée à une query string ou un segment d'URL.
 */
std::string ApiScanDataSource::url_encode(const std::string& value) {
    std::ostringstream out;
    out << std::hex << std::uppercase;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out << static_cast<char>(c);
        else out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return out.str();
}
