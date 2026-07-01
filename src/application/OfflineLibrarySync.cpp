/**
 * @file OfflineLibrarySync.cpp
 * @brief Implémente la copie d'une bibliothèque de scans vers un cache local hors ligne.
 *
 * Le service consomme une `ScanDataSource`, matérialise les pages puis les range dans
 * l'arborescence compatible avec le mode fichier local.
 */
#include "application/OfflineLibrarySync.hpp"
#include "infrastructure/JsonScanRepository.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

OfflineLibrarySync::OfflineLibrarySync(const ScanDataSource& source, std::filesystem::path outputRoot)
    : source_(source), outputRoot_(std::move(outputRoot)) {}

/**
 * @brief Synchronise tous les scans exposés par la source vers `outputRoot_`.
 *
 * Les échecs par page sont comptabilisés dans le rapport pour poursuivre la copie des autres
 * scans quand une image isolée est indisponible.
 */
OfflineSyncReport OfflineLibrarySync::sync_all(const std::string& profile, ProgressCallback callback) const {
    OfflineSyncReport total;
    for (const auto& scan : source_.list_scans(profile)) {
        auto report = sync_scan(scan.id, profile, callback);
        total.scans += report.scans;
        total.chapters += report.chapters;
        total.downloadedPages += report.downloadedPages;
        total.skippedPages += report.skippedPages;
        total.failedPages += report.failedPages;
    }
    return total;
}

/**
 * @brief Synchronise un scan précis vers l'arborescence offline.
 *
 * Les pages déjà présentes avec la même taille sont ignorées ; les échecs page par page sont
 * comptabilisés sans arrêter toute la synchronisation.
 */
OfflineSyncReport OfflineLibrarySync::sync_scan(
    const std::string& scanId,
    const std::string& profile,
    ProgressCallback callback) const {
    OfflineSyncReport report;
    OfflineSyncProgress progress;
    progress.scanId = scanId;

    const auto chapters = source_.list_chapters(scanId);
    report.scans = 1;
    report.chapters = static_cast<int>(chapters.size());

    for (int chapter : chapters) {
        for (const auto& page : source_.list_pages(scanId, chapter)) {
            progress.chapter = page.chapter;
            progress.page = page.page;

            try {
                const auto sourcePath = source_.materialize_page(scanId, ScanProgress{page.chapter, page.page});
                const auto destination = outputRoot_ / scanId / std::to_string(page.chapter) /
                    (std::to_string(page.page) + extension_from_path(sourcePath));

                std::filesystem::create_directories(destination.parent_path());
                if (std::filesystem::exists(destination) &&
                    std::filesystem::file_size(destination) == std::filesystem::file_size(sourcePath)) {
                    ++report.skippedPages;
                    ++progress.skippedPages;
                } else {
                    std::filesystem::copy_file(sourcePath, destination, std::filesystem::copy_options::overwrite_existing);
                    ++report.downloadedPages;
                    ++progress.downloadedPages;
                }
            } catch (...) {
                ++report.failedPages;
                ++progress.failedPages;
            }

            if (callback) callback(progress);
        }
    }

    try {
        JsonScanRepository repository;
        repository.save_progress(outputRoot_ / scanId, source_.load_progress(scanId, profile));
    } catch (...) {
        // La copie des images est prioritaire. Une progression illisible ne doit pas annuler la synchro.
    }

    return report;
}

/**
 * @brief Déduit l'extension image à utiliser dans le cache offline.
 */
std::string OfflineLibrarySync::extension_from_path(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".webp") {
        return extension;
    }
    return ".jpg";
}
