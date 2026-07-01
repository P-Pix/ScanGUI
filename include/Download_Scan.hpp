/**
 * @file Download_Scan.hpp
 * @brief Déclare l'adaptateur historique de téléchargement de page de scan.
 *
 * Cette classe maintient l'API de l'ancien code tout en utilisant libcurl et le provider de
 * parsing dédié au lieu d'appels shell externes.
 */

#ifndef DOWNLOAD_SCAN_HPP
#define DOWNLOAD_SCAN_HPP

#include "infrastructure/CurlHttpClient.hpp"
#include "infrastructure/LelScansProvider.hpp"

#include <filesystem>
#include <string>

/**
 * @brief Adaptateur historique de téléchargement d'une page de scan.
 *
 * Objectif projet :
 * Préserver l'interface publique utilisée par l'ancien code tout en supprimant les appels
 * shell et en centralisant les accès réseau dans `CurlHttpClient`.
 */
class Download_Scan {
public:
    Download_Scan();
    explicit Download_Scan(std::string website);
    virtual ~Download_Scan();

    void set_website(std::string website);
    [[nodiscard]] std::string get_website() const;
    [[nodiscard]] std::string get_website_content(const std::string& site) const;

    [[nodiscard]] std::string next_page();
    [[nodiscard]] std::string previous_page();

    void download_picture_page(const std::string& site, const std::filesystem::path& file_name) const;
    [[nodiscard]] std::string get_next_page_url(const std::string& url) const;

private:
    std::string website_;
    CurlHttpClient httpClient_;
    LelScansProvider lelScansProvider_;
};

#endif
