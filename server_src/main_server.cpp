/**
 * @file main_server.cpp
 * @brief Point d'entrée du serveur local ScanGUI.
 *
 * Le main charge la configuration depuis l'environnement, initialise PostgreSQL, synchronise
 * éventuellement la bibliothèque locale et démarre la boucle HTTP.
 */

#include "server/HttpServer.hpp"
#include "server/PostgresScanDatabase.hpp"
#include "server/ScanApiController.hpp"
#include "server/ScanLibraryIndexer.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

/**
 * @brief Lit une variable d'environnement avec valeur de repli.
 *
 * Objectif projet :
 * Permettre une configuration simple du serveur local sans fichier de configuration
 * obligatoire.
 */
std::string env_or(const char* name, std::string fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        return fallback;
    }
    return value;
}

/**
 * @brief Lit une variable d'environnement numérique avec valeur de repli.
 *
 * Une valeur absente ou invalide ne doit pas empêcher le serveur de démarrer en mode local.
 */
int env_int_or(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

} // namespace

/**
 * @brief Initialise et démarre le serveur local ScanGUI.
 *
 * Objectif projet :
 * Préparer PostgreSQL, synchroniser éventuellement la bibliothèque et exposer les routes API
 * consommables par le client desktop.
 */
int main() {
    try {
        const std::string databaseUrl = env_or(
            "SCANGUI_DATABASE_URL",
            "postgresql://scangui:scangui@127.0.0.1:5432/scangui"
        );
        const std::filesystem::path scanRoot = env_or("SCANGUI_SCAN_ROOT", "scan");
        const std::string host = env_or("SCANGUI_HOST", "127.0.0.1");
        const int port = env_int_or("SCANGUI_PORT", 8787);
        const bool syncOnStart = env_or("SCANGUI_SYNC_ON_START", "1") != "0";
        const std::string adminToken = env_or("SCANGUI_ADMIN_TOKEN", "");
        const std::filesystem::path webRoot = env_or("SCANGUI_WEB_ROOT", "web");

        PostgresScanDatabase database(databaseUrl);
        database.initialize_schema();

        if (syncOnStart) {
            ScanLibraryIndexer indexer(scanRoot, database);
            const auto report = indexer.sync();
            std::cout << "Initial sync: " << report.scans << " scan(s), "
                      << report.chapters << " chapter(s), " << report.pages << " page(s)" << std::endl;
        }

        ScanApiController controller(scanRoot, database, host, adminToken, webRoot);
        HttpServer server(host, port, [&controller](const HttpRequest& request) {
            return controller.handle(request);
        });
        server.start();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ScanGUIServer fatal error: " << error.what() << std::endl;
        return 1;
    }
}
