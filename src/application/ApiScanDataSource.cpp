/**
 * @file ApiScanDataSource.cpp
 * @brief Implémente la source de données cliente basée sur l'API HTTP locale.
 *
 * Cette implémentation consomme les endpoints du serveur C++, traduit les réponses JSON en
 * structures applicatives et met en cache les images nécessaires à l'affichage GTK.
 */

#include "application/ApiScanDataSource.hpp"

#include <algorithm>
#include <regex>
#include <stdexcept>

namespace {

/**
 * @brief Extrait les objets contenus dans un tableau JSON `items` simple.
 *
 * Objectif projet :
 * Lire les réponses de l'API locale sans ajouter de dépendance JSON côté client desktop. Le
 * parsing reste volontairement limité au format produit par `ScanGUIServer`.
 */
std::vector<std::string> extract_items_objects(const std::string& json) {
    std::vector<std::string> objects;
    const std::string marker = "\"items\":";
    const auto markerPos = json.find(marker);
    if (markerPos == std::string::npos) {
        return objects;
    }
    const auto arrayStart = json.find('[', markerPos);
    if (arrayStart == std::string::npos) {
        return objects;
    }

    int depth = 0;
    std::size_t start = std::string::npos;
    for (std::size_t i = arrayStart + 1; i < json.size(); ++i) {
        if (json[i] == '{') {
            if (depth == 0) {
                start = i;
            }
            ++depth;
        } else if (json[i] == '}') {
            --depth;
            if (depth == 0 && start != std::string::npos) {
                objects.push_back(json.substr(start, i - start + 1));
                start = std::string::npos;
            }
        } else if (json[i] == ']' && depth == 0) {
            break;
        }
    }
    return objects;
}

/**
 * @brief Lit un champ chaîne dans un objet JSON simple.
 *
 * @param json Objet JSON produit par l'API locale.
 * @param key Nom du champ attendu.
 * @return Valeur du champ ou chaîne vide si absent.
 */
std::string string_field(const std::string& json, const std::string& key) {
    const std::regex re("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        return match[1].str();
    }
    return "";
}

int int_field(const std::string& json, const std::string& key, int fallback = 0) {
    const std::regex re("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        try {
            return std::stoi(match[1].str());
        } catch (...) {
        }
    }
    return fallback;
}

long long long_field(const std::string& json, const std::string& key) {
    const std::regex re("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        try {
            return std::stoll(match[1].str());
        } catch (...) {
        }
    }
    return 0;
}

std::string extension_for_mime(const std::string& mimeType) {
    if (mimeType == "image/png") return ".png";
    if (mimeType == "image/webp") return ".webp";
    return ".jpg";
}

} // namespace

ApiScanDataSource::ApiScanDataSource(std::string baseUrl, std::filesystem::path cacheRoot)
    : baseUrl_(std::move(baseUrl)), cacheRoot_(std::move(cacheRoot)) {
    while (!baseUrl_.empty() && baseUrl_.back() == '/') {
        baseUrl_.pop_back();
    }
}

std::vector<ScanSummary> ApiScanDataSource::list_scans() const {
    const std::string json = http_.get_text(endpoint("/api/scans"));
    std::vector<ScanSummary> scans;
    for (const auto& object : extract_items_objects(json)) {
        const std::string id = string_field(object, "id");
        if (!id.empty()) {
            scans.push_back({id, string_field(object, "title"), int_field(object, "chapterCount"), int_field(object, "pageCount")});
        }
    }
    return scans;
}

std::vector<int> ApiScanDataSource::list_chapters(const std::string& scanId) const {
    const std::string json = http_.get_text(endpoint("/api/scans/" + scanId + "/chapters"));
    std::vector<int> chapters;
    for (const auto& object : extract_items_objects(json)) {
        const int chapter = int_field(object, "chapter");
        if (chapter > 0) {
            chapters.push_back(chapter);
        }
    }
    return chapters;
}

std::vector<ScanPageInfo> ApiScanDataSource::list_pages(const std::string& scanId, int chapter) const {
    const std::string json = http_.get_text(endpoint("/api/scans/" + scanId + "/chapters/" + std::to_string(chapter) + "/pages"));
    std::vector<ScanPageInfo> pages;
    for (const auto& object : extract_items_objects(json)) {
        ScanPageInfo page;
        page.chapter = int_field(object, "chapter");
        page.page = int_field(object, "page");
        page.mimeType = string_field(object, "mimeType");
        page.sizeBytes = long_field(object, "sizeBytes");
        page.imageUrl = string_field(object, "imageUrl");
        if (page.chapter > 0 && page.page > 0) {
            pages.push_back(page);
        }
    }
    return pages;
}

/**
 * @brief Télécharge si nécessaire une page API dans le cache local.
 *
 * Objectif projet :
 * Transformer une page distante en fichier local afin de rester compatible avec l'affichage
 * GTK basé sur `Gdk::Pixbuf::create_from_file`.
 *
 * @param scanId Identifiant du scan demandé.
 * @param progress Position chapitre/page à rendre disponible.
 * @return Chemin de l'image en cache.
 */
std::filesystem::path ApiScanDataSource::materialize_page(const std::string& scanId, ScanProgress progress) const {
    progress.normalize();
    const auto pages = list_pages(scanId, progress.chapter);
    auto it = std::find_if(pages.begin(), pages.end(), [&](const ScanPageInfo& page) {
        return page.page == progress.page;
    });
    if (it == pages.end()) {
        throw std::runtime_error("Page introuvable via l'API: " + scanId);
    }

    const auto output = cacheRoot_ / scanId / std::to_string(progress.chapter) / (std::to_string(progress.page) + extension_for_mime(it->mimeType));
    if (!std::filesystem::exists(output)) {
        http_.download_file(endpoint(it->imageUrl), output);
    }
    return output;
}

ScanProgress ApiScanDataSource::load_progress(const std::string& scanId, const std::string& profile) const {
    const std::string json = http_.get_text(endpoint("/api/scans/" + scanId + "/progress?profile=" + profile));
    ScanProgress progress{int_field(json, "chapter", 1), int_field(json, "page", 1)};
    progress.normalize();
    return progress;
}

void ApiScanDataSource::save_progress(const std::string& scanId, ScanProgress progress, const std::string& profile) const {
    progress.normalize();
    const std::string body = "{\"chapter\":" + std::to_string(progress.chapter) + ",\"page\":" + std::to_string(progress.page) + "}";
    (void)http_.post_json(endpoint("/api/scans/" + scanId + "/progress?profile=" + profile), body);
}

/**
 * @brief Construit une URL absolue à partir d'un chemin d'API.
 *
 * @param path Chemin relatif, absolu local ou URL déjà complète.
 * @return URL prête pour le client HTTP.
 */
std::string ApiScanDataSource::endpoint(const std::string& path) const {
    if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) {
        return path;
    }
    if (!path.empty() && path.front() == '/') {
        return baseUrl_ + path;
    }
    return baseUrl_ + '/' + path;
}
