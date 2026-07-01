/**
 * @file CurlHttpClient.cpp
 * @brief Implémente les opérations HTTP sortantes avec libcurl.
 *
 * Le client gère les lectures texte, téléchargements atomiques et requêtes JSON utilisées
 * par le téléchargement historique et par le client d'API locale.
 */

#include "infrastructure/CurlHttpClient.hpp"

#include <curl/curl.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

size_t write_to_string(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* output = static_cast<std::string*>(userdata);
    const size_t total = size * nmemb;
    output->append(ptr, total);
    return total;
}

size_t write_to_file(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* output = static_cast<std::ofstream*>(userdata);
    const size_t total = size * nmemb;
    output->write(ptr, static_cast<std::streamsize>(total));
    return output->good() ? total : 0;
}

/**
 * @brief Applique les options libcurl communes aux appels HTTP.
 *
 * Objectif projet :
 * Centraliser les timeouts, le suivi des redirections, l'agent utilisateur et le comportement
 * d'erreur afin que les appels réseau restent cohérents.
 */
void configure_common(CURL* curl, const std::string& url, long timeoutSeconds) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ScanGUI/2.0 refactored");
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
}

struct CurlGlobalState {
    CurlGlobalState() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~CurlGlobalState() {
        curl_global_cleanup();
    }
};

void ensure_curl_global_state() {
    static CurlGlobalState state;
    (void)state;
}

/**
 * @brief Transforme un code libcurl en exception détaillée.
 *
 * @param code Résultat retourné par libcurl.
 * @param curl Handle utilisé pour récupérer le statut HTTP.
 * @param url URL appelée, ajoutée au message d'erreur.
 */
void throw_on_curl_error(CURLcode code, CURL* curl, const std::string& url) {
    if (code == CURLE_OK) {
        return;
    }

    long status = 0;
    if (curl != nullptr) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    }

    throw std::runtime_error(
        "Erreur HTTP/CURL sur " + url + " - code=" + std::to_string(code) +
        ", status=" + std::to_string(status) + ", message=" + curl_easy_strerror(code)
    );
}

} // namespace

CurlHttpClient::CurlHttpClient() {
    ensure_curl_global_state();
}

CurlHttpClient::~CurlHttpClient() = default;

/**
 * @brief Récupère le contenu texte d'une URL.
 *
 * @param url URL HTTP ou HTTPS à lire.
 * @return Corps de réponse sous forme de chaîne.
 */
std::string CurlHttpClient::get_text(const std::string& url) const {
    ensure_curl_global_state();
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        throw std::runtime_error("Impossible d'initialiser libcurl");
    }

    std::string response;
    configure_common(curl, url, timeoutSeconds_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode result = curl_easy_perform(curl);
    try {
        throw_on_curl_error(result, curl, url);
    } catch (...) {
        curl_easy_cleanup(curl);
        throw;
    }
    curl_easy_cleanup(curl);
    return response;
}

/**
 * @brief Télécharge une URL vers un fichier local de manière atomique.
 *
 * Objectif projet :
 * Écrire d'abord dans un fichier `.part` pour éviter de laisser une image finale incomplète
 * si le téléchargement échoue.
 */
void CurlHttpClient::download_file(const std::string& url, const std::filesystem::path& outputPath) const {
    ensure_curl_global_state();
    std::filesystem::create_directories(outputPath.parent_path());

    const auto temporaryPath = outputPath.string() + ".part";
    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Impossible d'ouvrir le fichier en ecriture: " + temporaryPath);
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        throw std::runtime_error("Impossible d'initialiser libcurl");
    }

    configure_common(curl, url, timeoutSeconds_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);

    const CURLcode result = curl_easy_perform(curl);
    output.close();

    try {
        throw_on_curl_error(result, curl, url);
    } catch (...) {
        curl_easy_cleanup(curl);
        std::filesystem::remove(temporaryPath);
        throw;
    }

    curl_easy_cleanup(curl);
    std::filesystem::rename(temporaryPath, outputPath);
}


/**
 * @brief Envoie un payload JSON en POST et retourne la réponse texte.
 */
std::string CurlHttpClient::post_json(const std::string& url, const std::string& jsonBody) const {
    ensure_curl_global_state();
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        throw std::runtime_error("Impossible d'initialiser libcurl");
    }

    std::string response;
    configure_common(curl, url, timeoutSeconds_);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(jsonBody.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    const CURLcode result = curl_easy_perform(curl);
    try {
        throw_on_curl_error(result, curl, url);
    } catch (...) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        throw;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}
