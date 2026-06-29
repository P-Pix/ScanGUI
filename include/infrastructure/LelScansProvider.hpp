/**
 * @file LelScansProvider.hpp
 * @brief Déclare l'adaptateur de parsing pour les URLs et pages lelscans.
 *
 * Le provider regroupe les règles spécifiques au site source afin que le téléchargement ne
 * dépende pas de chaînes de parsing dispersées dans l'interface ou les services.
 */

#ifndef SCANGUI_INFRASTRUCTURE_LELSCANS_PROVIDER_HPP
#define SCANGUI_INFRASTRUCTURE_LELSCANS_PROVIDER_HPP

#include "domain/ScanMetadata.hpp"
#include "domain/ScanProgress.hpp"

#include <optional>
#include <string>

/**
 * @brief Provider de parsing dédié à la source lelscans.
 *
 * Objectif projet :
 * Encapsuler les règles de reconnaissance d'URL, d'extraction d'image et de progression pour
 * que le service de téléchargement reste indépendant de la structure HTML externe.
 */
class LelScansProvider {
public:
    struct ParsedUrl {
        std::string folderName;
        std::string baseUrl;
        ScanProgress progress{};
    };

    [[nodiscard]] bool supports(const std::string& url) const;
    [[nodiscard]] std::optional<ParsedUrl> parse_reader_url(const std::string& url) const;
    [[nodiscard]] std::string page_url(const ScanMetadata& metadata) const;
    [[nodiscard]] std::optional<ScanProgress> parse_next_progress(const std::string& baseUrl, const std::string& nextUrl) const;
    [[nodiscard]] std::string extract_image_url(const std::string& html) const;
    [[nodiscard]] std::string extract_next_url(const std::string& html) const;

private:
    [[nodiscard]] std::string absolute_url(const std::string& url) const;
};

#endif
