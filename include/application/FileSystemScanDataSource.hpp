/**
 * @file FileSystemScanDataSource.hpp
 * @brief Déclare la source de données historique basée sur le dossier local des scans.
 *
 * Cette classe conserve le mode de lecture direct sur `./scan` pour garder un comportement
 * compatible avec la version desktop initiale, tout en respectant le même contrat que la
 * source API.
 */

#ifndef SCANGUI_APPLICATION_FILESYSTEM_SCAN_DATA_SOURCE_HPP
#define SCANGUI_APPLICATION_FILESYSTEM_SCAN_DATA_SOURCE_HPP

#include "application/ScanDataSource.hpp"
#include "infrastructure/JsonScanRepository.hpp"

/**
 * @brief Source de données basée sur l'arborescence locale `scan`.
 *
 * Objectif projet :
 * Conserver le mode de lecture historique sans serveur tout en respectant le contrat commun
 * de l'application. Cette classe sert aussi de repli simple pour tester la bibliothèque sans
 * PostgreSQL.
 */
class FileSystemScanDataSource final : public ScanDataSource {
public:
    explicit FileSystemScanDataSource(std::filesystem::path scanRoot = "scan");

    [[nodiscard]] std::vector<ScanSummary> list_scans() const override;
    [[nodiscard]] std::vector<int> list_chapters(const std::string& scanId) const override;
    [[nodiscard]] std::vector<ScanPageInfo> list_pages(const std::string& scanId, int chapter) const override;
    [[nodiscard]] std::filesystem::path materialize_page(const std::string& scanId, ScanProgress progress) const override;
    [[nodiscard]] ScanProgress load_progress(const std::string& scanId, const std::string& profile = "default") const override;
    void save_progress(const std::string& scanId, ScanProgress progress, const std::string& profile = "default") const override;

private:
    std::filesystem::path scanRoot_;
    JsonScanRepository repository_;
};

#endif
