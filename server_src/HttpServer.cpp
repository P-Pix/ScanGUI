/**
 * @file HttpServer.cpp
 * @brief Implémente le serveur HTTP local basé sur les sockets POSIX.
 *
 * Ce fichier contient la boucle d'écoute, la lecture des requêtes, le parsing HTTP minimal
 * et l'envoi des réponses sérialisées. Le serveur reste volontairement limité à un usage
 * local pour exposer la bibliothèque de scans au client desktop.
 */

#include "server/HttpServer.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {

constexpr int backlog = 32;
constexpr std::size_t max_header_bytes = 64 * 1024;
constexpr std::size_t max_body_bytes = 2 * 1024 * 1024;

/**
 * @brief Nettoie les espaces de début et de fin d'une valeur HTTP.
 *
 * Ce helper évite de conserver des caractères parasites dans les en-têtes lus depuis la
 * socket, sans modifier le contenu interne de la valeur.
 *
 * @param value Valeur brute issue de la requête.
 * @return Valeur sans espaces périphériques.
 */
std::string trim(std::string value) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

/**
 * @brief Sépare le chemin HTTP et les paramètres de requête.
 *
 * Objectif projet :
 * Fournir au contrôleur API un chemin déjà isolé des paramètres `?key=value`, notamment pour
 * le profil de progression, sans introduire de dépendance à une bibliothèque HTTP complète.
 *
 * @param target Cible brute de la ligne de requête.
 * @param path Chemin décodé sans chaîne de requête.
 * @return Paramètres de requête décodés.
 */
std::map<std::string, std::string> parse_query(const std::string& target, std::string& path) {
    std::map<std::string, std::string> query;
    const auto queryPos = target.find('?');
    path = queryPos == std::string::npos ? target : target.substr(0, queryPos);
    if (queryPos == std::string::npos) {
        return query;
    }

    std::string remaining = target.substr(queryPos + 1);
    while (!remaining.empty()) {
        const auto amp = remaining.find('&');
        const std::string pair = amp == std::string::npos ? remaining : remaining.substr(0, amp);
        const auto eq = pair.find('=');
        if (eq == std::string::npos) {
            query[url_decode(pair)] = "";
        } else {
            query[url_decode(pair.substr(0, eq))] = url_decode(pair.substr(eq + 1));
        }
        if (amp == std::string::npos) {
            break;
        }
        remaining = remaining.substr(amp + 1);
    }
    return query;
}

/**
 * @brief Écrit toute la réponse HTTP sur la socket cliente.
 *
 * Le serveur utilise ce helper pour gérer les écritures partielles possibles avec `send`.
 * Une erreur d'écriture interrompt simplement l'envoi car le client local a probablement
 * fermé la connexion.
 *
 * @param socketFd Descripteur de socket client.
 * @param data Réponse HTTP sérialisée.
 */
void send_all(int socketFd, const std::string& data) {
    const char* buffer = data.data();
    std::size_t remaining = data.size();
    while (remaining > 0) {
        const ssize_t written = ::send(socketFd, buffer, remaining, 0);
        if (written <= 0) {
            return;
        }
        buffer += written;
        remaining -= static_cast<std::size_t>(written);
    }
}

/**
 * @brief Diffuse un fichier en blocs après avoir sérialisé les en-têtes HTTP.
 *
 * Objectif projet :
 * Servir les images sans charger tout le fichier en mémoire, tout en laissant le contrôleur
 * valider au préalable que le chemin demandé est autorisé.
 */
void send_streamed_file(int socketFd, const HttpResponse& response) {
    namespace fs = std::filesystem;
    const fs::path filePath = response.streamFilePath;
    std::error_code ec;
    const auto size = fs::file_size(filePath, ec);
    if (ec) {
        send_all(socketFd, HttpResponse::not_found("Streamed file not found").serialize());
        return;
    }

    send_all(socketFd, response.serialize_headers(static_cast<std::size_t>(size)));
    std::ifstream input(filePath, std::ios::binary);
    char chunk[64 * 1024];
    while (input) {
        input.read(chunk, sizeof(chunk));
        const auto read = input.gcount();
        if (read <= 0) {
            break;
        }
        const char* cursor = chunk;
        std::size_t remaining = static_cast<std::size_t>(read);
        while (remaining > 0) {
            const ssize_t written = ::send(socketFd, cursor, remaining, 0);
            if (written <= 0) {
                return;
            }
            cursor += written;
            remaining -= static_cast<std::size_t>(written);
        }
    }
}

void log_request(const HttpRequest& request, const HttpResponse& response) {
    std::cout << "{\"event\":\"http_request\",\"method\":\"" << json_escape(request.method)
              << "\",\"path\":\"" << json_escape(request.path)
              << "\",\"status\":" << response.status
              << ",\"streamed\":" << (response.streamFile ? "true" : "false") << "}" << std::endl;
}

} // namespace

HttpServer::HttpServer(std::string host, int port, Handler handler)
    : host_(std::move(host)), port_(port), handler_(std::move(handler)) {
}

/**
 * @brief Démarre la boucle d'écoute du serveur HTTP local.
 *
 * Objectif projet :
 * Accepter les connexions entrantes sur l'adresse configurée et créer un thread court par
 * client afin de garder l'API réactive pour les usages locaux.
 */
void HttpServer::start() {
    const int serverSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        throw std::runtime_error("socket() failed: " + std::string(std::strerror(errno)));
    }

    int enabled = 1;
    ::setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port_));
    if (::inet_pton(AF_INET, host_.c_str(), &address.sin_addr) != 1) {
        ::close(serverSocket);
        throw std::runtime_error("Invalid bind address: " + host_);
    }

    if (::bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(serverSocket);
        throw std::runtime_error("bind() failed: " + std::string(std::strerror(errno)));
    }

    if (::listen(serverSocket, backlog) < 0) {
        ::close(serverSocket);
        throw std::runtime_error("listen() failed: " + std::string(std::strerror(errno)));
    }

    std::cout << "ScanGUIServer listening on http://" << host_ << ':' << port_ << std::endl;
    while (true) {
        const int clientSocket = ::accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            std::cerr << "accept() failed: " << std::strerror(errno) << std::endl;
            continue;
        }
        std::thread([this, clientSocket]() {
            handle_client(clientSocket);
        }).detach();
    }
}

/**
 * @brief Lit, parse et traite une connexion HTTP cliente.
 *
 * Objectif projet :
 * Encadrer les limites de taille des en-têtes et du corps avant de déléguer la requête au
 * handler applicatif. Les erreurs sont converties en réponse HTTP plutôt qu'en arrêt serveur.
 *
 * @param clientSocket Socket client acceptée par la boucle principale.
 */
void HttpServer::handle_client(int clientSocket) const {
    try {
        std::string raw;
        char buffer[8192];
        std::size_t headerEnd = std::string::npos;
        while (raw.size() < max_header_bytes) {
            const ssize_t received = ::recv(clientSocket, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                ::close(clientSocket);
                return;
            }
            raw.append(buffer, static_cast<std::size_t>(received));
            headerEnd = raw.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                break;
            }
        }

        if (headerEnd == std::string::npos) {
            send_all(clientSocket, HttpResponse::bad_request("HTTP headers are too large or incomplete").serialize());
            ::close(clientSocket);
            return;
        }

        HttpRequest request = parse_request(raw.substr(0, headerEnd + 4));
        std::size_t contentLength = 0;
        if (auto it = request.headers.find("content-length"); it != request.headers.end()) {
            contentLength = static_cast<std::size_t>(std::stoul(it->second));
        }
        if (contentLength > max_body_bytes) {
            send_all(clientSocket, HttpResponse::bad_request("HTTP body is too large").serialize());
            ::close(clientSocket);
            return;
        }

        request.body = raw.substr(headerEnd + 4);
        while (request.body.size() < contentLength) {
            const ssize_t received = ::recv(clientSocket, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                break;
            }
            request.body.append(buffer, static_cast<std::size_t>(received));
        }
        if (request.body.size() > contentLength) {
            request.body.resize(contentLength);
        }

        HttpResponse response;
        if (request.method == "OPTIONS") {
            response = HttpResponse::text(204, "");
        } else {
            response = handler_(request);
        }
        log_request(request, response);
        if (response.streamFile) {
            send_streamed_file(clientSocket, response);
        } else {
            send_all(clientSocket, response.serialize());
        }
    } catch (const std::exception& error) {
        send_all(clientSocket, HttpResponse::server_error(error.what()).serialize());
    }
    ::close(clientSocket);
}

/**
 * @brief Convertit les en-têtes HTTP bruts en structure `HttpRequest`.
 *
 * Le parseur reste volontairement minimal : il extrait la ligne de requête, les en-têtes et
 * la query string nécessaires au serveur local, sans chercher à couvrir tout HTTP.
 *
 * @param raw Bloc d'en-têtes terminé par la ligne vide HTTP.
 * @return Requête applicative utilisable par le routeur.
 */
HttpRequest HttpServer::parse_request(const std::string& raw) const {
    std::istringstream input(raw);
    std::string requestLine;
    std::getline(input, requestLine);
    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }

    std::istringstream firstLine(requestLine);
    HttpRequest request;
    std::string version;
    firstLine >> request.method >> request.target >> version;
    if (request.method.empty() || request.target.empty()) {
        throw std::runtime_error("Invalid HTTP request line");
    }
    request.query = parse_query(request.target, request.path);

    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string key = trim(line.substr(0, colon));
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        request.headers[key] = trim(line.substr(colon + 1));
    }
    return request;
}
