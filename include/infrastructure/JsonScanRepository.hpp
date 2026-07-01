/**
 * @file JsonScanRepository.hpp
 * @brief Déclare le repository de métadonnées JSON par dossier de scan.
 *
 * Le repository conserve la compatibilité avec le fichier `data.json` historique tout en
 * isolant la lecture et l'écriture de progression du reste de l'application.
 */

#ifndef SCANGUI_INFRASTRUCTURE_JSON_SCAN_REPOSITORY_HPP
#define SCANGUI_INFRASTRUCTURE_JSON_SCAN_REPOSITORY_HPP

#include "domain/ScanMetadata.hpp"
#include "domain/ScanProgress.hpp"

#include <filesystem>
#include <optional>

/**
 * @brief Repository de lecture/écriture du fichier `data.json` d'un scan.
 *
 * Objectif projet :
 * Centraliser la compatibilité avec le format historique pour éviter de disperser le parsing
 * JSON dans les fenêtres, services ou sources de données.
 */
class JsonScanRepository {
public:
    /**
     * @brief Lit les métadonnées `data.json` d'un scan.
     *
     * @return Métadonnées normalisées, ou `std::nullopt` si le fichier est absent ou inexploitable.
     */
    [[nodiscard]] std::optional<ScanMetadata> load(const std::filesystem::path& scanFolder) const;
    void save(const std::filesystem::path& scanFolder, const ScanMetadata& metadata) const;
    void save_progress(const std::filesystem::path& scanFolder, ScanProgress progress) const;

private:
    [[nodiscard]] std::filesystem::path metadata_path(const std::filesystem::path& scanFolder) const;
};

#endif
