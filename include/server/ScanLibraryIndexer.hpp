/**
 * @file ScanLibraryIndexer.hpp
 * @brief Déclare l'indexeur qui synchronise le dossier `scan` vers PostgreSQL.
 *
 * L'indexeur parcourt les scans, chapitres et images présents sur disque afin de produire
 * une base consultable rapidement par l'API locale.
 */

#ifndef SCANGUI_SERVER_SCAN_LIBRARY_INDEXER_HPP
#define SCANGUI_SERVER_SCAN_LIBRARY_INDEXER_HPP

#include "server/PostgresScanDatabase.hpp"

#include <filesystem>

/**
 * @brief Compteurs produits par une synchronisation de bibliothèque.
 *
 * Le rapport alimente la réponse de l'endpoint d'administration et les logs de démarrage.
 */
struct IndexReport {
    int scans{0};
    int chapters{0};
    int pages{0};
};

/**
 * @brief Indexe l'arborescence locale des scans dans PostgreSQL.
 *
 * Objectif projet :
 * Construire une représentation structurée et requêtable de `./scan` pour éviter à l'API de
 * reparcourir le disque à chaque appel.
 */
class ScanLibraryIndexer {
public:
    ScanLibraryIndexer(std::filesystem::path scanRoot, PostgresScanDatabase& database);
    [[nodiscard]] IndexReport sync();

private:
    std::filesystem::path scanRoot_;
    PostgresScanDatabase& database_;

    [[nodiscard]] static bool is_image_file(const std::filesystem::path& path);
    [[nodiscard]] static int parse_positive_int(const std::string& value);
    [[nodiscard]] static std::string readable_title(std::string folderName);
};

#endif
