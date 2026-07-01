/**
 * @file ScanApiController.hpp
 * @brief Déclare le contrôleur REST et front web local de la bibliothèque ScanGUI.
 *
 * Le contrôleur est le point d’entrée applicatif du serveur HTTP local. Il transforme les
 * routes en opérations PostgreSQL, indexation, streaming d’images ou service du client web.
 */
#ifndef SCANGUI_SERVER_SCAN_API_CONTROLLER_HPP
#define SCANGUI_SERVER_SCAN_API_CONTROLLER_HPP

#include "server/HttpTypes.hpp"
#include "server/PostgresScanDatabase.hpp"
#include "server/ScanLibraryIndexer.hpp"

#include <filesystem>
#include <mutex>
#include <string>

/**
 * @brief Routeur applicatif de l’API locale et du front web statique.
 *
 * Objectif projet :
 * Traduire les requêtes HTTP minimales en opérations métier sur la bibliothèque : scans,
 * pages, profils, progression, favoris, marque-pages, historique, OCR et synchronisation.
 * Le contrôleur garde aussi les validations de sécurité qui protègent l’exposition des images.
 *
 * Interagit avec :
 * - `HttpServer` pour recevoir les requêtes décodées ;
 * - `PostgresScanDatabase` pour toutes les données persistées ;
 * - `ScanLibraryIndexer` et `OcrIndexer` pour les endpoints d’administration.
 */
class ScanApiController {
public:
    ScanApiController(
        std::filesystem::path scanRoot,
        PostgresScanDatabase& database,
        std::string bindHost = "127.0.0.1",
        std::string adminToken = "",
        std::filesystem::path webRoot = "web"
    );

    /**
     * @brief Route une requête HTTP vers le handler API ou le front web.
     *
     * @param request Requête déjà parsée par le serveur socket.
     * @return Réponse HTTP prête à sérialiser.
     */
    [[nodiscard]] HttpResponse handle(const HttpRequest& request);

private:
    std::filesystem::path scanRoot_;
    std::filesystem::path webRoot_;
    PostgresScanDatabase& database_;
    std::string bindHost_;
    std::string adminToken_;
    mutable std::mutex requestMutex_;

    [[nodiscard]] HttpResponse health() const;
    [[nodiscard]] HttpResponse version() const;
    [[nodiscard]] HttpResponse stats() const;
    [[nodiscard]] HttpResponse sync_library(const HttpRequest& request);
    [[nodiscard]] HttpResponse ocr_library(const HttpRequest& request);
    [[nodiscard]] HttpResponse offline_manifest(const HttpRequest& request) const;
    [[nodiscard]] HttpResponse list_scans(const HttpRequest& request) const;
    [[nodiscard]] HttpResponse get_scan(const std::string& scanId, const HttpRequest& request) const;
    [[nodiscard]] HttpResponse list_chapters(const std::string& scanId) const;
    [[nodiscard]] HttpResponse list_pages(const std::string& scanId, int chapter, const HttpRequest& request) const;

    /**
     * @brief Retourne une image de page après validation de chemin.
     *
     * @param scanId Identifiant du scan.
     * @param chapter Chapitre demandé.
     * @param page Page demandée.
     * @return Réponse fichier streamée ou erreur HTTP.
     */
    [[nodiscard]] HttpResponse get_image(const std::string& scanId, int chapter, int page) const;

    [[nodiscard]] HttpResponse get_progress(const std::string& scanId, const std::string& profile) const;

    /**
     * @brief Persiste la progression envoyée par le client.
     *
     * @param scanId Identifiant du scan.
     * @param profile Profil utilisateur.
     * @param body Payload JSON contenant chapitre et page.
     * @return Réponse JSON de confirmation ou erreur de validation.
     */
    [[nodiscard]] HttpResponse save_progress(const std::string& scanId, const std::string& profile, const std::string& body);

    [[nodiscard]] HttpResponse list_profiles() const;
    [[nodiscard]] HttpResponse create_profile(const std::string& body);
    [[nodiscard]] HttpResponse list_favorites(const std::string& profile) const;
    [[nodiscard]] HttpResponse set_favorite(const std::string& scanId, const std::string& profile, const std::string& body);
    [[nodiscard]] HttpResponse list_bookmarks(const std::string& scanId, const std::string& profile) const;
    [[nodiscard]] HttpResponse add_bookmark(const std::string& scanId, const std::string& profile, const std::string& body);
    [[nodiscard]] HttpResponse list_history(const std::string& profile, const HttpRequest& request) const;
    [[nodiscard]] HttpResponse search(const HttpRequest& request) const;
    [[nodiscard]] HttpResponse chapter_summary(const std::string& scanId, int chapter) const;

    /**
     * @brief Sert les fichiers statiques du client web local.
     *
     * Le chemin demandé est normalisé pour éviter de sortir du dossier web configuré.
     *
     * @param request Requête HTTP du navigateur.
     * @return Fichier statique ou 404.
     */
    [[nodiscard]] HttpResponse serve_web(const HttpRequest& request) const;

    /**
     * @brief Vérifie qu’une image reste contenue dans la racine des scans.
     *
     * Cette protection empêche une route d’image de servir un fichier arbitraire du poste
     * via un chemin compromis en base.
     *
     * @param path Chemin image résolu depuis PostgreSQL.
     * @return true si le chemin canonique reste sous `scanRoot_`.
     */
    [[nodiscard]] bool is_safe_page_path(const std::filesystem::path& path) const;

    /**
     * @brief Valide les restrictions des endpoints d’administration.
     *
     * Les actions d’indexation et OCR sont limitées à localhost et peuvent exiger un token
     * administrateur via l’en-tête HTTP configuré.
     *
     * @param request Requête admin à valider.
     * @return true si l’appel est autorisé.
     */
    [[nodiscard]] bool admin_request_allowed(const HttpRequest& request) const;
};

#endif
