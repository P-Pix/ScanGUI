/**
 * @file OfflineLibrarySync.hpp
 * @brief Déclare le service de synchronisation offline depuis une source de scans.
 *
 * Ce service copie les images exposées par une `ScanDataSource` vers une arborescence locale
 * compatible avec le mode fichier historique. Il sert de pont entre le mode serveur et une
 * lecture hors ligne sans PostgreSQL ni réseau.
 */
#ifndef SCANGUI_APPLICATION_OFFLINE_LIBRARY_SYNC_HPP
#define SCANGUI_APPLICATION_OFFLINE_LIBRARY_SYNC_HPP

#include "application/ScanDataSource.hpp"

#include <filesystem>
#include <functional>
#include <string>

/**
 * @brief État incrémental publié pendant une synchronisation offline.
 *
 * Les compteurs sont cumulatifs pour un scan et permettent à une interface graphique ou à une
 * tâche de fond d'afficher une progression sans connaître le détail de copie des fichiers.
 */
struct OfflineSyncProgress {
    std::string scanId;
    int chapter{0};
    int page{0};
    int downloadedPages{0};
    int skippedPages{0};
    int failedPages{0};
};

/**
 * @brief Rapport final d'une synchronisation offline.
 *
 * Il synthétise le nombre de scans, chapitres et pages réellement copiés, ignorés ou en échec.
 */
struct OfflineSyncReport {
    int scans{0};
    int chapters{0};
    int downloadedPages{0};
    int skippedPages{0};
    int failedPages{0};
};

/**
 * @brief Copie une bibliothèque de scans vers un dossier local lisible sans réseau.
 *
 * Objectif projet :
 * Permettre à un utilisateur connecté au serveur local de rapatrier les pages sur son poste pour
 * continuer la lecture en mode offline. Le service dépend seulement du contrat `ScanDataSource`,
 * ce qui le rend utilisable avec une API ou une source fichier.
 *
 * Interagit avec :
 * - la source de données active ;
 * - le dossier local `scan` ou un cache choisi ;
 * - `JsonScanRepository` pour conserver la progression de lecture.
 */
class OfflineLibrarySync {
public:
    using ProgressCallback = std::function<void(const OfflineSyncProgress&)>;

    OfflineLibrarySync(const ScanDataSource& source, std::filesystem::path outputRoot = "scan");

    /**
     * @brief Synchronise tous les scans visibles pour un profil.
     *
     * @param profile Profil utilisé pour récupérer la bibliothèque et la progression.
     * @param callback Callback optionnel appelé après chaque page traitée.
     * @return Rapport cumulé de synchronisation.
     */
    [[nodiscard]] OfflineSyncReport sync_all(const std::string& profile = "default", ProgressCallback callback = {}) const;

    /**
     * @brief Synchronise un scan précis vers le dossier offline.
     *
     * @param scanId Identifiant stable du scan à copier.
     * @param profile Profil utilisé pour sauvegarder la progression locale.
     * @param callback Callback optionnel appelé après chaque page traitée.
     * @return Rapport de synchronisation du scan.
     */
    [[nodiscard]] OfflineSyncReport sync_scan(const std::string& scanId, const std::string& profile = "default", ProgressCallback callback = {}) const;

private:
    const ScanDataSource& source_;
    std::filesystem::path outputRoot_;

    /**
     * @brief Détermine l'extension d'image à conserver dans le cache offline.
     *
     * @param path Chemin source matérialisé par la source de données.
     * @return Extension reconnue ou `.jpg` par défaut.
     */
    [[nodiscard]] static std::string extension_from_path(const std::filesystem::path& path);
};

#endif
