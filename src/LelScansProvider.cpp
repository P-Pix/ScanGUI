/**
 * @file LelScansProvider.cpp
 * @brief Implémente les règles de parsing propres aux URLs et pages lelscans.
 *
 * Les traitements sont volontairement isolés dans ce provider car ils dépendent de la forme
 * des URLs et du HTML de la source externe.
 */

#include "infrastructure/LelScansProvider.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <vector>

namespace {

std::string trim_quotes_and_spaces(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return c == '"' || c == '\'' || std::isspace(c);
    }), value.end());
    return value;
}

std::string path_from_url(std::string url) {
    const std::string host = "lelscans.net";
    auto hostPosition = url.find(host);
    if (hostPosition != std::string::npos) {
        auto pathPosition = url.find('/', hostPosition + host.size());
        if (pathPosition == std::string::npos) {
            return "/";
        }
        return url.substr(pathPosition);
    }
    if (!url.empty() && url.front() == '/') {
        return url;
    }
    return "/" + url;
}

std::vector<std::string> split_path(std::string path) {
    path.erase(std::remove(path.begin(), path.end(), '\r'), path.end());
    path.erase(std::remove(path.begin(), path.end(), '\n'), path.end());
    if (!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }
    if (!path.empty() && path.back() == '/') {
        path.pop_back();
    }

    std::vector<std::string> parts;
    std::stringstream stream(path);
    std::string part;
    while (std::getline(stream, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

std::optional<int> to_int(const std::string& value) {
    if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); })) {
        return std::nullopt;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

bool LelScansProvider::supports(const std::string& url) const {
    return url.find("lelscans.net") != std::string::npos;
}

/**
 * @brief Interprète une URL de lecture lelscans en métadonnées initiales.
 *
 * Objectif projet :
 * Déduire le dossier local, l'URL de base et la progression de départ à partir de l'URL
 * collée par l'utilisateur dans la fenêtre de téléchargement.
 */
std::optional<LelScansProvider::ParsedUrl> LelScansProvider::parse_reader_url(const std::string& url) const {
    static const std::regex readerRegex(R"(https?://(?:www\.)?lelscans\.net/scan-([^/]+)/([0-9]+)(?:/([0-9]+))?)");
    std::smatch match;
    if (!std::regex_search(url, match, readerRegex)) {
        return std::nullopt;
    }

    ParsedUrl parsed;
    parsed.folderName = match[1].str();
    parsed.baseUrl = "https://lelscans.net/scan-" + parsed.folderName + "/";
    parsed.progress.chapter = std::stoi(match[2].str());
    parsed.progress.page = match[3].matched ? std::stoi(match[3].str()) : 1;
    parsed.progress.normalize();
    return parsed;
}

std::string LelScansProvider::page_url(const ScanMetadata& metadata) const {
    std::string url = metadata.downloadUrl;
    if (!url.empty() && url.back() != '/') {
        url.push_back('/');
    }
    return url + std::to_string(metadata.downloadProgress.chapter) + "/" + std::to_string(metadata.downloadProgress.page);
}

std::optional<ScanProgress> LelScansProvider::parse_next_progress(const std::string& baseUrl, const std::string& nextUrl) const {
    if (nextUrl.empty() || nextUrl == "#main_hot" || nextUrl == "/1") {
        return std::nullopt;
    }

    const auto baseParts = split_path(path_from_url(baseUrl));
    const auto nextParts = split_path(path_from_url(nextUrl));
    if (baseParts.empty() || nextParts.size() < 2) {
        return std::nullopt;
    }

    if (nextParts.front() != baseParts.front()) {
        return std::nullopt;
    }

    auto chapter = to_int(nextParts[1]);
    if (!chapter) {
        return std::nullopt;
    }

    ScanProgress progress;
    progress.chapter = *chapter;
    progress.page = 1;
    if (nextParts.size() >= 3) {
        progress.page = to_int(nextParts[2]).value_or(1);
    }
    progress.normalize();
    return progress;
}

/**
 * @brief Extrait l'URL de l'image principale depuis le HTML d'une page.
 *
 * Le parsing est limité au format actuellement attendu de la source et retourne une chaîne
 * vide si l'image n'est pas trouvée plutôt que de deviner.
 */
std::string LelScansProvider::extract_image_url(const std::string& html) const {
    static const std::regex imageRegex(R"(<img[^>]+src=["']([^"']*?/mangas/[^"']+?\.(?:jpg|jpeg|png|webp))["'])", std::regex::icase);
    std::smatch match;
    if (!std::regex_search(html, match, imageRegex) || match.size() < 2) {
        return "";
    }
    return absolute_url(match[1].str());
}

/**
 * @brief Extrait le lien de page suivante depuis le bloc image.
 *
 * @param html Contenu HTML de la page courante.
 * @return URL relative ou absolue de la page suivante, ou chaîne vide si absente.
 */
std::string LelScansProvider::extract_next_url(const std::string& html) const {
    auto imageDivPosition = html.find("<div id=\"image\"");
    std::string scoped = imageDivPosition == std::string::npos ? html : html.substr(imageDivPosition);

    static const std::regex hrefRegex(R"(<a[^>]+href=["']([^"']+)["'])", std::regex::icase);
    std::smatch match;
    if (!std::regex_search(scoped, match, hrefRegex) || match.size() < 2) {
        return "";
    }
    return trim_quotes_and_spaces(match[1].str());
}

std::string LelScansProvider::absolute_url(const std::string& url) const {
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        return url;
    }
    if (!url.empty() && url.front() == '/') {
        return "https://lelscans.net" + url;
    }
    return "https://lelscans.net/" + url;
}
