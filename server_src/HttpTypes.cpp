/**
 * @file HttpTypes.cpp
 * @brief Implémente les helpers HTTP et JSON utilisés par le serveur local.
 *
 * Les fonctions de ce fichier construisent les réponses standards, sérialisent le protocole
 * HTTP minimal et normalisent les chemins, URLs et contenus JSON exposés par l'API.
 */

#include "server/HttpTypes.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace {

/**
 * @brief Associe un code HTTP au libellé court envoyé au client.
 *
 * @param status Code de statut HTTP.
 * @return Raison textuelle utilisée dans la première ligne de réponse.
 */
std::string reason_for_status(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

/**
 * @brief Construit un payload JSON d'erreur échappé.
 *
 * Objectif projet :
 * Uniformiser les réponses d'erreur de l'API et éviter qu'un message d'exception injecte du
 * JSON invalide dans le corps de réponse.
 *
 * @param message Message d'erreur à exposer.
 * @return Objet JSON minimal contenant le champ `error`.
 */
std::string error_json(const std::string& message) {
    return std::string("{\"error\":\"") + json_escape(message) + "\"}";
}

} // namespace

HttpResponse HttpResponse::text(int statusCode, std::string responseBody) {
    HttpResponse response;
    response.status = statusCode;
    response.reason = reason_for_status(statusCode);
    response.headers["Content-Type"] = "text/plain; charset=utf-8";
    response.body = std::move(responseBody);
    return response;
}

HttpResponse HttpResponse::json(int statusCode, std::string responseBody) {
    HttpResponse response;
    response.status = statusCode;
    response.reason = reason_for_status(statusCode);
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.body = std::move(responseBody);
    return response;
}

HttpResponse HttpResponse::binary(int statusCode, std::string contentType, std::string responseBody) {
    HttpResponse response;
    response.status = statusCode;
    response.reason = reason_for_status(statusCode);
    response.headers["Content-Type"] = std::move(contentType);
    response.headers["Cache-Control"] = "private, max-age=3600";
    response.body = std::move(responseBody);
    return response;
}

HttpResponse HttpResponse::not_found(std::string message) {
    return json(404, error_json(message));
}

HttpResponse HttpResponse::bad_request(std::string message) {
    return json(400, error_json(message));
}

HttpResponse HttpResponse::server_error(std::string message) {
    return json(500, error_json(message));
}

/**
 * @brief Sérialise la réponse applicative au format HTTP/1.1.
 *
 * Objectif projet :
 * Transformer les réponses produites par les handlers en texte directement transmissible sur
 * la socket, avec longueur de contenu, fermeture de connexion et en-têtes CORS locaux.
 *
 * @return Réponse complète prête à être envoyée au client.
 */
std::string HttpResponse::serialize() const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << ' ' << reason << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n";
    out << "Access-Control-Allow-Origin: http://127.0.0.1:8787\r\n";
    out << "Access-Control-Allow-Headers: Content-Type\r\n";
    out << "Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS\r\n";
    for (const auto& [key, value] : headers) {
        out << key << ": " << value << "\r\n";
    }
    out << "\r\n";
    out << body;
    return out.str();
}

/**
 * @brief Découpe un chemin d'URL en segments décodés.
 *
 * Objectif projet :
 * Simplifier le routage REST du contrôleur en comparant des segments plutôt que des chaînes
 * complètes fragiles.
 *
 * @param path Chemin URL reçu dans la requête.
 * @return Segments non vides du chemin.
 */
std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : path) {
        if (c == '/') {
            if (!current.empty()) {
                parts.push_back(url_decode(current));
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        parts.push_back(url_decode(current));
    }
    return parts;
}

/**
 * @brief Décode les séquences URL simples utilisées par les routes et query strings.
 *
 * @param value Valeur encodée issue de l'URL.
 * @return Valeur décodée pour le routeur ou les paramètres applicatifs.
 */
std::string url_decode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const std::string hex = value.substr(i + 1, 2);
            char* end = nullptr;
            const long decoded = std::strtol(hex.c_str(), &end, 16);
            if (end != nullptr && *end == '\0') {
                result.push_back(static_cast<char>(decoded));
                i += 2;
                continue;
            }
        }
        if (value[i] == '+') {
            result.push_back(' ');
        } else {
            result.push_back(value[i]);
        }
    }
    return result;
}

/**
 * @brief Échappe une chaîne destinée à être insérée dans du JSON construit manuellement.
 *
 * Objectif projet :
 * Sécuriser les réponses JSON produites sans bibliothèque dédiée, notamment pour les noms de
 * scans, profils et messages d'erreur issus de fichiers ou requêtes.
 *
 * @param value Chaîne brute.
 * @return Chaîne échappée compatible JSON.
 */
std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (unsigned char c : value) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (c < 0x20) {
                    std::ostringstream code;
                    code << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    escaped += code.str();
                } else {
                    escaped.push_back(static_cast<char>(c));
                }
        }
    }
    return escaped;
}

/**
 * @brief Déduit le type MIME d'une image à partir de son extension.
 *
 * @param path Chemin ou nom de fichier à analyser.
 * @return Type MIME reconnu ou `application/octet-stream` en repli.
 */
std::string guess_mime_type(const std::string& path) {
    auto extension = path.substr(path.find_last_of('.') == std::string::npos ? path.size() : path.find_last_of('.'));
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".png") return "image/png";
    if (extension == ".webp") return "image/webp";
    return "application/octet-stream";
}
