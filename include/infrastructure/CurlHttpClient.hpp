/**
 * @file CurlHttpClient.hpp
 * @brief Déclare le client HTTP basé sur libcurl utilisé par l'application.
 *
 * Cette classe remplace les appels shell historiques par une dépendance réseau contrôlée,
 * avec téléchargement de fichier, lecture texte et envoi JSON pour l'API locale.
 */

#ifndef SCANGUI_INFRASTRUCTURE_CURL_HTTP_CLIENT_HPP
#define SCANGUI_INFRASTRUCTURE_CURL_HTTP_CLIENT_HPP

#include <filesystem>
#include <string>

/**
 * @brief Client HTTP minimal basé sur libcurl.
 *
 * Objectif projet :
 * Remplacer les appels shell `wget` par une API C++ contrôlée, portable et testable pour les
 * téléchargements, lectures texte et appels JSON.
 */
class CurlHttpClient {
public:
    CurlHttpClient();
    ~CurlHttpClient();

    CurlHttpClient(const CurlHttpClient&) = delete;
    CurlHttpClient& operator=(const CurlHttpClient&) = delete;

    /**
     * @brief Récupère le contenu texte d'une URL.
     *
     * @throws std::runtime_error si libcurl signale une erreur réseau ou HTTP.
     */
    [[nodiscard]] std::string get_text(const std::string& url) const;
    /**
     * @brief Télécharge une ressource vers un fichier local.
     *
     * @throws std::runtime_error si le téléchargement ou l'ouverture du fichier échoue.
     */
    void download_file(const std::string& url, const std::filesystem::path& outputPath) const;
    /** @brief Envoie un payload JSON en POST et retourne la réponse texte. */
    [[nodiscard]] std::string post_json(const std::string& url, const std::string& jsonBody) const;

private:
    long timeoutSeconds_{30};
};

#endif
