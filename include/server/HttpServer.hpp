/**
 * @file HttpServer.hpp
 * @brief Déclare le serveur HTTP local construit directement sur les sockets POSIX.
 *
 * Le serveur accepte des connexions TCP locales, décode une requête HTTP minimale et délègue
 * le traitement applicatif à un handler injecté. Il est pensé pour un usage local et
 * pédagogique, pas comme serveur web généraliste.
 */

#ifndef SCANGUI_SERVER_HTTP_SERVER_HPP
#define SCANGUI_SERVER_HTTP_SERVER_HPP

#include "server/HttpTypes.hpp"

#include <functional>
#include <string>

/**
 * @brief Serveur HTTP local minimal basé sur des sockets POSIX.
 *
 * Objectif projet :
 * Exposer une API locale au client desktop sans framework web, dans un but d'apprentissage et
 * d'abstraction des accès aux scans. Le serveur reste volontairement limité à l'usage local.
 */
class HttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    HttpServer(std::string host, int port, Handler handler);
    /**
     * @brief Lance la boucle d'écoute bloquante du serveur local.
     *
     * Le serveur accepte les connexions entrantes et délègue chaque requête au handler injecté.
     */
    void start();

private:
    std::string host_;
    int port_;
    Handler handler_;

    void handle_client(int clientSocket) const;
    [[nodiscard]] HttpRequest parse_request(const std::string& raw) const;
};

#endif
