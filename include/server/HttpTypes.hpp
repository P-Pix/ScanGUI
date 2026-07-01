/**
 * @file HttpTypes.hpp
 * @brief Définit les structures et helpers HTTP minimaux du serveur local.
 *
 * Ce fichier contient les types de requête/réponse, la sérialisation HTTP et les utilitaires
 * de chemin, d'échappement JSON et de type MIME nécessaires à l'API sans framework web.
 */

#ifndef SCANGUI_SERVER_HTTP_TYPES_HPP
#define SCANGUI_SERVER_HTTP_TYPES_HPP

#include <cstddef>
#include <map>
#include <string>
#include <vector>

/**
 * @brief Requête HTTP décodée par le serveur local.
 *
 * Cette structure contient uniquement les éléments utiles au routeur applicatif : méthode,
 * cible, chemin, paramètres, en-têtes et corps.
 */
struct HttpRequest {
    std::string method;
    std::string target;
    std::string path;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;
    std::string body;
};

/**
 * @brief Réponse HTTP construite par les handlers de l'API locale.
 *
 * La structure centralise le statut, les en-têtes et le corps avant sérialisation vers la
 * socket cliente.
 */
struct HttpResponse {
    int status{200};
    std::string reason{"OK"};
    std::map<std::string, std::string> headers;
    std::string body;
    std::string streamFilePath;
    bool streamFile{false};

    /** @brief Construit une réponse texte avec en-têtes cohérents. */
    static HttpResponse text(int status, std::string body);
    static HttpResponse json(int status, std::string body);
    static HttpResponse binary(int status, std::string contentType, std::string body);
    /**
     * @brief Construit une réponse de streaming fichier.
     */
    static HttpResponse file(int status, std::string contentType, std::string path);
    static HttpResponse not_found(std::string message = "Route not found");
    static HttpResponse bad_request(std::string message);
    static HttpResponse server_error(std::string message);

    [[nodiscard]] std::string serialize() const;
    [[nodiscard]] std::string serialize_headers(std::size_t contentLength) const;
};

/** @brief Découpe un chemin URL en segments déjà décodés. */
[[nodiscard]] std::vector<std::string> split_path(const std::string& path);
/**
 * @brief Décode une portion d'URL sans lever en cas de séquence invalide.
 */
[[nodiscard]] std::string url_decode(const std::string& value);
/**
 * @brief Échappe une chaîne avant insertion dans du JSON construit manuellement.
 */
[[nodiscard]] std::string json_escape(const std::string& value);
[[nodiscard]] std::string guess_mime_type(const std::string& path);

#endif
