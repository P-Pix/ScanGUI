/**
 * @file Download_Scan.cpp
 * @brief Implémente l'adaptateur de téléchargement conservé pour compatibilité.
 *
 * La classe garde les noms historiques utilisés par l'interface tout en s'appuyant sur
 * libcurl et sur le provider de parsing spécifique.
 */

#include "Download_Scan.hpp"

#include <stdexcept>
#include <utility>

Download_Scan::Download_Scan() = default;

Download_Scan::Download_Scan(std::string website) : website_(std::move(website)) {
}

Download_Scan::~Download_Scan() = default;

void Download_Scan::set_website(std::string website) {
    website_ = std::move(website);
}

std::string Download_Scan::get_website() const {
    return website_;
}

/**
 * @brief Télécharge le HTML d'une page source via le client HTTP contrôlé.
 */
std::string Download_Scan::get_website_content(const std::string& site) const {
    return httpClient_.get_text(site);
}

/**
 * @brief Télécharge l'image d'une page vers le fichier local demandé.
 */
void Download_Scan::download_picture_page(const std::string& site, const std::filesystem::path& file_name) const {
    if (!lelScansProvider_.supports(site)) {
        throw std::runtime_error("Source non supportee pour le telechargement: " + site);
    }

    const std::string html = httpClient_.get_text(site);
    const std::string imageUrl = lelScansProvider_.extract_image_url(html);
    if (imageUrl.empty()) {
        throw std::runtime_error("Aucune image trouvee sur la page: " + site);
    }

    httpClient_.download_file(imageUrl, file_name);
}

std::string Download_Scan::next_page() {
    return "";
}

std::string Download_Scan::previous_page() {
    return "";
}

std::string Download_Scan::get_next_page_url(const std::string& url) const {
    if (!lelScansProvider_.supports(url)) {
        return "";
    }
    return lelScansProvider_.extract_next_url(httpClient_.get_text(url));
}
