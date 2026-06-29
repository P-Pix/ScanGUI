/**
 * @file ScanUpdater.hpp
 * @brief Déclare le service applicatif chargé de mettre à jour un scan depuis sa source web.
 *
 * Le service orchestre la lecture des métadonnées, le téléchargement des images et la
 * progression de téléchargement. Il isole ce flux de l'interface GTK afin de limiter le
 * couplage entre l'IHM et les traitements réseau/fichier.
 */

#ifndef SCANGUI_APPLICATION_SCAN_UPDATER_HPP
#define SCANGUI_APPLICATION_SCAN_UPDATER_HPP

#include "infrastructure/CurlHttpClient.hpp"
#include "infrastructure/JsonScanRepository.hpp"
#include "infrastructure/LelScansProvider.hpp"

#include <filesystem>
#include <string>

/**
 * @brief Résultat synthétique d'une mise à jour de scan.
 *
 * Le rapport permet à l'interface de distinguer les pages réellement téléchargées, les pages
 * déjà présentes et le message de fin ou d'arrêt de sécurité.
 */
struct ScanUpdateReport {
    int downloadedPages{0};
    int skippedExistingPages{0};
    std::string message;
};

/**
 * @brief Service applicatif de téléchargement incrémental d'un scan.
 *
 * Objectif projet :
 * Isoler le flux réseau/fichier de mise à jour afin que la fenêtre GTK ne porte plus la
 * logique de téléchargement, parsing HTML et persistance de progression.
 *
 * Interagit avec :
 * - le repository JSON contenant les métadonnées de téléchargement ;
 * - le client HTTP libcurl ;
 * - le provider spécifique à la source web.
 */
class ScanUpdater {
public:
    ScanUpdater(JsonScanRepository repository, CurlHttpClient& httpClient, LelScansProvider provider);

    [[nodiscard]] ScanUpdateReport update(const std::filesystem::path& scanFolder, int maxPages = 1000) const;

private:
    JsonScanRepository repository_;
    CurlHttpClient& httpClient_;
    LelScansProvider provider_;
};

#endif
