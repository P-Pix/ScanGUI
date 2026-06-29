/**
 * @file ScanUpdater.cpp
 * @brief Implémente la mise à jour d'un scan à partir de ses métadonnées de téléchargement.
 *
 * Le service télécharge les pages manquantes, avance la progression de téléchargement et
 * persiste l'état après chaque page afin de pouvoir reprendre proprement.
 */

#include "application/ScanUpdater.hpp"

#include <filesystem>
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <utility>

ScanUpdater::ScanUpdater(JsonScanRepository repository, CurlHttpClient& httpClient, LelScansProvider provider)
    : repository_(std::move(repository)), httpClient_(httpClient), provider_(std::move(provider)) {
}

/**
 * @brief Télécharge les pages manquantes d'un scan à partir de `data.json`.
 *
 * Objectif projet :
 * Reprendre la mise à jour depuis la progression de téléchargement persistée, éviter de
 * retélécharger les images déjà présentes et arrêter le flux en cas de fin, boucle ou limite
 * de sécurité.
 *
 * @param scanFolder Dossier racine du scan à mettre à jour.
 * @param maxPages Garde-fou contre les boucles de téléchargement.
 * @return Rapport synthétique de mise à jour.
 */
ScanUpdateReport ScanUpdater::update(const std::filesystem::path& scanFolder, int maxPages) const {
    auto metadata = repository_.load(scanFolder);
    if (!metadata) {
        throw std::runtime_error("Aucun fichier data.json trouve pour: " + scanFolder.string());
    }
    if (metadata->downloadUrl.empty()) {
        throw std::runtime_error("Le fichier data.json ne contient pas d'URL de telechargement.");
    }
    if (!provider_.supports(metadata->downloadUrl)) {
        throw std::runtime_error("Source non supportee: " + metadata->downloadUrl);
    }

    ScanUpdateReport report;
    for (int guard = 0; guard < maxPages; ++guard) {
        const std::string pageUrl = provider_.page_url(*metadata);
        const auto chapterFolder = scanFolder / std::to_string(metadata->downloadProgress.chapter);
        std::filesystem::create_directories(chapterFolder);

        const std::string html = httpClient_.get_text(pageUrl);
        const std::string imageUrl = provider_.extract_image_url(html);
        if (imageUrl.empty()) {
            report.message = "Fin ou page sans image: " + pageUrl;
            break;
        }

        std::string extension = std::filesystem::path(imageUrl.substr(0, imageUrl.find('?'))).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (extension != ".jpg" && extension != ".jpeg" && extension != ".png" && extension != ".webp") {
            extension = ".jpg";
        }
        const auto typedOutputPath = chapterFolder / (std::to_string(metadata->downloadProgress.page) + extension);

        if (std::filesystem::exists(typedOutputPath)) {
            ++report.skippedExistingPages;
        } else {
            httpClient_.download_file(imageUrl, typedOutputPath);
            ++report.downloadedPages;
        }

        const std::string nextUrl = provider_.extract_next_url(html);
        const auto nextProgress = provider_.parse_next_progress(metadata->downloadUrl, nextUrl);
        if (!nextProgress) {
            report.message = "Mise a jour terminee.";
            break;
        }

        if (nextProgress->chapter == metadata->downloadProgress.chapter && nextProgress->page == metadata->downloadProgress.page) {
            report.message = "Arret de securite: la page suivante est identique a la page courante.";
            break;
        }

        metadata->downloadProgress = *nextProgress;
        repository_.save(scanFolder, *metadata);
    }

    repository_.save(scanFolder, *metadata);
    if (report.message.empty()) {
        report.message = "Arret de securite: limite de pages atteinte.";
    }
    return report;
}
