/**
 * @file ScanMetadata.hpp
 * @brief Regroupe les métadonnées persistées pour un scan local.
 *
 * Ces informations conservent à la fois l'URL de téléchargement, la dernière position de
 * téléchargement et la dernière position de lecture afin de préserver le format historique
 * `data.json`.
 */

#ifndef SCANGUI_DOMAIN_SCAN_METADATA_HPP
#define SCANGUI_DOMAIN_SCAN_METADATA_HPP

#include "domain/ScanProgress.hpp"

#include <string>

/**
 * @brief Métadonnées locales associées à un dossier de scan.
 *
 * Objectif projet :
 * Conserver dans `data.json` l'état utile aux deux flux historiques : le téléchargement
 * incrémental depuis une URL source et la reprise de lecture utilisateur.
 */
struct ScanMetadata {
    std::string downloadUrl;
    ScanProgress downloadProgress{};
    ScanProgress saveProgress{};

    void normalize() {
        downloadProgress.normalize();
        saveProgress.normalize();
    }
};

#endif
