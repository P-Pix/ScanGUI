/**
 * @file ScanApiController.cpp
 * @brief Implémente le routage REST de l'API locale de scans.
 *
 * Le contrôleur expose les endpoints de santé, synchronisation, liste des scans, chapitres,
 * pages, images et progression. Il vérifie les paramètres reçus avant tout accès fichier ou
 * base de données.
 */

#include "server/ScanApiController.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace {

/**
 * @brief Convertit un scan indexé en fragment JSON de réponse API.
 *
 * Les chaînes sont échappées car les noms de dossiers et titres proviennent du système de
 * fichiers et ne doivent pas casser le JSON renvoyé au client.
 */
std::string scan_to_json(const ScanSummaryRecord& scan) {
    return "{"
        "\"id\":\"" + json_escape(scan.id) + "\"," 
        "\"title\":\"" + json_escape(scan.title) + "\"," 
        "\"folderName\":\"" + json_escape(scan.folderName) + "\"," 
        "\"downloadUrl\":\"" + json_escape(scan.downloadUrl) + "\"," 
        "\"chapterCount\":" + std::to_string(scan.chapterCount) + ","
        "\"pageCount\":" + std::to_string(scan.pageCount) +
        "}";
}

/**
 * @brief Valide et convertit un identifiant numérique positif reçu dans l'URL.
 *
 * Objectif projet :
 * Refuser explicitement les chapitres et pages invalides avant toute requête PostgreSQL ou
 * résolution de fichier.
 */
std::optional<int> parse_int(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }
    for (unsigned char c : value) {
        if (!std::isdigit(c)) {
            return std::nullopt;
        }
    }
    try {
        int parsed = std::stoi(value);
        if (parsed < 1) {
            return std::nullopt;
        }
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * @brief Extrait un entier positif d'un corps JSON simple.
 *
 * Le serveur maison accepte uniquement le payload attendu pour la progression. Une valeur
 * absente ou invalide est rejetée par le handler appelant.
 */
std::optional<int> extract_json_int(const std::string& body, const std::string& key) {
    const std::regex re("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(body, match, re) || match.size() < 2) {
        return std::nullopt;
    }
    return parse_int(match[1].str());
}

/**
 * @brief Charge une image en mémoire pour la réponse HTTP binaire.
 *
 * @param path Chemin validé avant lecture par le contrôleur.
 * @return Contenu binaire du fichier.
 */
std::string read_file_binary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Impossible de lire l'image: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

} // namespace

ScanApiController::ScanApiController(std::filesystem::path scanRoot, PostgresScanDatabase& database)
    : scanRoot_(std::filesystem::absolute(std::move(scanRoot))), database_(database) {
}

/**
 * @brief Route une requête HTTP vers le handler API correspondant.
 *
 * Objectif projet :
 * Centraliser les routes exposées par le serveur local et convertir les paramètres d'URL en
 * valeurs métier validées avant appel aux méthodes privées.
 *
 * @param request Requête décodée par `HttpServer`.
 * @return Réponse HTTP applicative prête à être sérialisée.
 */
HttpResponse ScanApiController::handle(const HttpRequest& request) {
    try {
        const auto parts = split_path(request.path);
        if (request.method == "GET" && parts == std::vector<std::string>{"api", "health"}) {
            return health();
        }
        if (request.method == "POST" && parts == std::vector<std::string>{"api", "admin", "sync"}) {
            return sync_library();
        }
        if (request.method == "GET" && parts == std::vector<std::string>{"api", "scans"}) {
            return list_scans();
        }
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "scans") {
            const std::string& scanId = parts[2];
            if (request.method == "GET" && parts.size() == 3) {
                return get_scan(scanId);
            }
            if (request.method == "GET" && parts.size() == 4 && parts[3] == "chapters") {
                return list_chapters(scanId);
            }
            if (request.method == "GET" && parts.size() == 6 && parts[3] == "chapters" && parts[5] == "pages") {
                const auto chapter = parse_int(parts[4]);
                if (!chapter) return HttpResponse::bad_request("Invalid chapter number");
                return list_pages(scanId, *chapter);
            }
            if (request.method == "GET" && parts.size() == 8 && parts[3] == "chapters" && parts[5] == "pages" && parts[7] == "image") {
                const auto chapter = parse_int(parts[4]);
                const auto page = parse_int(parts[6]);
                if (!chapter || !page) return HttpResponse::bad_request("Invalid chapter or page number");
                return get_image(scanId, *chapter, *page);
            }
            if ((request.method == "GET" || request.method == "POST" || request.method == "PUT") && parts.size() == 4 && parts[3] == "progress") {
                std::string profile = "default";
                if (auto it = request.query.find("profile"); it != request.query.end() && !it->second.empty()) {
                    profile = it->second;
                }
                if (request.method == "GET") {
                    return get_progress(scanId, profile);
                }
                return save_progress(scanId, profile, request.body);
            }
        }
        return HttpResponse::not_found();
    } catch (const std::exception& error) {
        return HttpResponse::server_error(error.what());
    }
}

HttpResponse ScanApiController::health() const {
    return HttpResponse::json(200, "{\"status\":\"ok\",\"service\":\"ScanGUIServer\"}");
}

/**
 * @brief Lance une synchronisation du dossier local vers PostgreSQL.
 *
 * Objectif projet :
 * Mettre à jour l'index de l'API après ajout ou téléchargement de scans sans redémarrer le
 * serveur.
 *
 * @return Rapport JSON de synchronisation.
 */
HttpResponse ScanApiController::sync_library() {
    ScanLibraryIndexer indexer(scanRoot_, database_);
    const auto report = indexer.sync();
    return HttpResponse::json(
        200,
        "{\"status\":\"synced\",\"scans\":" + std::to_string(report.scans) +
        ",\"chapters\":" + std::to_string(report.chapters) +
        ",\"pages\":" + std::to_string(report.pages) + "}"
    );
}

HttpResponse ScanApiController::list_scans() const {
    const auto scans = database_.list_scans();
    std::string json = "{\"items\":[";
    for (std::size_t i = 0; i < scans.size(); ++i) {
        if (i > 0) json += ',';
        json += scan_to_json(scans[i]);
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

HttpResponse ScanApiController::get_scan(const std::string& scanId) const {
    const auto scan = database_.get_scan(scanId);
    if (!scan) {
        return HttpResponse::not_found("Scan not found");
    }
    return HttpResponse::json(200, scan_to_json(*scan));
}

HttpResponse ScanApiController::list_chapters(const std::string& scanId) const {
    if (!database_.get_scan(scanId)) {
        return HttpResponse::not_found("Scan not found");
    }
    const auto chapters = database_.list_chapters(scanId);
    std::string json = "{\"items\":[";
    for (std::size_t i = 0; i < chapters.size(); ++i) {
        if (i > 0) json += ',';
        json += "{\"chapter\":" + std::to_string(chapters[i].number) + ",\"pageCount\":" + std::to_string(chapters[i].pageCount) + "}";
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

HttpResponse ScanApiController::list_pages(const std::string& scanId, int chapter) const {
    if (!database_.get_scan(scanId)) {
        return HttpResponse::not_found("Scan not found");
    }
    const auto pages = database_.list_pages(scanId, chapter);
    std::string json = "{\"items\":[";
    for (std::size_t i = 0; i < pages.size(); ++i) {
        if (i > 0) json += ',';
        json += "{\"chapter\":" + std::to_string(pages[i].chapter) +
                ",\"page\":" + std::to_string(pages[i].page) +
                ",\"mimeType\":\"" + json_escape(pages[i].mimeType) + "\"" +
                ",\"sizeBytes\":" + std::to_string(pages[i].sizeBytes) +
                ",\"imageUrl\":\"/api/scans/" + json_escape(scanId) + "/chapters/" + std::to_string(pages[i].chapter) + "/pages/" + std::to_string(pages[i].page) + "/image\"}";
    }
    json += "]}";
    return HttpResponse::json(200, json);
}

/**
 * @brief Sert le fichier image associé à une page indexée.
 *
 * Objectif projet :
 * Exposer les pages au client desktop via HTTP tout en vérifiant que le chemin stocké en base
 * reste dans le dossier racine autorisé.
 */
HttpResponse ScanApiController::get_image(const std::string& scanId, int chapter, int page) const {
    const auto record = database_.get_page(scanId, chapter, page);
    if (!record) {
        return HttpResponse::not_found("Page not found");
    }
    if (!is_safe_page_path(record->filePath)) {
        return HttpResponse::bad_request("Unsafe image path rejected");
    }
    return HttpResponse::binary(200, record->mimeType, read_file_binary(record->filePath));
}

HttpResponse ScanApiController::get_progress(const std::string& scanId, const std::string& profile) const {
    if (!database_.get_scan(scanId)) {
        return HttpResponse::not_found("Scan not found");
    }
    const auto progress = database_.get_progress(scanId, profile);
    return HttpResponse::json(
        200,
        "{\"scanId\":\"" + json_escape(scanId) + "\",\"profile\":\"" + json_escape(profile) +
        "\",\"chapter\":" + std::to_string(progress.chapter) +
        ",\"page\":" + std::to_string(progress.page) + "}"
    );
}

HttpResponse ScanApiController::save_progress(const std::string& scanId, const std::string& profile, const std::string& body) {
    if (!database_.get_scan(scanId)) {
        return HttpResponse::not_found("Scan not found");
    }
    const auto chapter = extract_json_int(body, "chapter");
    const auto page = extract_json_int(body, "page");
    if (!chapter || !page) {
        return HttpResponse::bad_request("Expected JSON body: {\"chapter\": 1, \"page\": 1}");
    }
    ScanProgress progress{*chapter, *page};
    progress.normalize();
    database_.save_progress(scanId, profile, progress);
    return get_progress(scanId, profile);
}

/**
 * @brief Vérifie qu'un fichier image reste confiné dans le dossier racine des scans.
 *
 * Cette protection limite les risques de path traversal ou d'entrée base corrompue avant de
 * renvoyer un fichier binaire au client HTTP.
 */
bool ScanApiController::is_safe_page_path(const std::filesystem::path& path) const {
    if (!std::filesystem::exists(path)) {
        return false;
    }
    const auto root = std::filesystem::weakly_canonical(scanRoot_);
    const auto file = std::filesystem::weakly_canonical(path);
    const auto rootString = root.string();
    const auto fileString = file.string();
    return fileString == rootString || (fileString.rfind(rootString + std::string(1, std::filesystem::path::preferred_separator), 0) == 0);
}
