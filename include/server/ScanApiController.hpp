/**
 * @file ScanApiController.hpp
 * @brief Déclare le contrôleur REST de la bibliothèque de scans.
 *
 * Le contrôleur route les endpoints `/api` vers PostgreSQL, l'indexeur et le service de
 * fichiers images. Il concentre aussi les validations de paramètres et de chemins avant
 * d'exposer les pages au client.
 */

#ifndef SCANGUI_SERVER_SCAN_API_CONTROLLER_HPP
#define SCANGUI_SERVER_SCAN_API_CONTROLLER_HPP

#include "server/HttpTypes.hpp"
#include "server/PostgresScanDatabase.hpp"
#include "server/ScanLibraryIndexer.hpp"

#include <filesystem>
#include <string>

/**
 * @brief Contrôleur applicatif des routes REST de ScanGUI.
 *
 * Objectif projet :
 * Traduire les requêtes HTTP en opérations métier sur la bibliothèque, la progression et les
 * images, tout en validant les paramètres et les chemins avant de servir des fichiers.
 */
class ScanApiController {
public:
    ScanApiController(std::filesystem::path scanRoot, PostgresScanDatabase& database);
    [[nodiscard]] HttpResponse handle(const HttpRequest& request);

private:
    std::filesystem::path scanRoot_;
    PostgresScanDatabase& database_;

    [[nodiscard]] HttpResponse health() const;
    [[nodiscard]] HttpResponse sync_library();
    [[nodiscard]] HttpResponse list_scans() const;
    [[nodiscard]] HttpResponse get_scan(const std::string& scanId) const;
    [[nodiscard]] HttpResponse list_chapters(const std::string& scanId) const;
    [[nodiscard]] HttpResponse list_pages(const std::string& scanId, int chapter) const;
    [[nodiscard]] HttpResponse get_image(const std::string& scanId, int chapter, int page) const;
    [[nodiscard]] HttpResponse get_progress(const std::string& scanId, const std::string& profile) const;
    [[nodiscard]] HttpResponse save_progress(const std::string& scanId, const std::string& profile, const std::string& body);

    /**
     * @brief Vérifie qu'une image servie par l'API reste dans le dossier racine autorisé.
     *
     * Objectif projet :
     * Empêcher une entrée PostgreSQL ou une requête détournée d'exposer un fichier arbitraire
     * du poste via l'endpoint d'image.
     *
     * @param path Chemin de page issu de la base.
     * @return true si le chemin canonique reste sous `scanRoot_`, false sinon.
     */
    [[nodiscard]] bool is_safe_page_path(const std::filesystem::path& path) const;
};

#endif
