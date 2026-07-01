/**
 * @file ScanLibraryIndexer.cpp
 * @brief Implémente la synchronisation du dossier local de scans vers PostgreSQL.
 *
 * L'indexeur reconstruit le contenu des scans à partir de l'arborescence disque et écrit une
 * vision structurée dans la base pour l'API locale.
 */

#include "server/ScanLibraryIndexer.hpp"

#include "infrastructure/JsonScanRepository.hpp"
#include "server/HttpTypes.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <vector>

ScanLibraryIndexer::ScanLibraryIndexer(std::filesystem::path scanRoot, PostgresScanDatabase& database)
    : scanRoot_(std::move(scanRoot)), database_(database) {
}

/**
 * @brief Reconstruit l'index PostgreSQL à partir de l'arborescence `scan`.
 *
 * Objectif projet :
 * Parcourir les dossiers de scans, détecter chapitres et images valides, puis persister une
 * représentation stable pour l'API. L'opération est transactionnelle pour éviter un index
 * partiellement mis à jour.
 *
 * @return Compteurs de scans, chapitres et pages indexés.
 */
IndexReport ScanLibraryIndexer::sync() {
    std::filesystem::create_directories(scanRoot_);
    IndexReport report;
    JsonScanRepository repository;

    database_.begin();
    try {
        for (const auto& scanEntry : std::filesystem::directory_iterator(scanRoot_)) {
            if (!scanEntry.is_directory()) {
                continue;
            }

            const std::string scanId = scanEntry.path().filename().string();
            ScanSummaryRecord scan;
            scan.id = scanId;
            scan.folderName = scanId;
            scan.title = readable_title(scanId);
            scan.rootPath = std::filesystem::canonical(scanEntry.path()).string();
            if (auto metadata = repository.load(scanEntry.path())) {
                scan.downloadUrl = metadata->downloadUrl;
            }

            database_.upsert_scan(scan);
            database_.delete_scan_content(scanId);
            ++report.scans;

            std::vector<std::filesystem::directory_entry> chapterEntries;
            for (const auto& chapterEntry : std::filesystem::directory_iterator(scanEntry.path())) {
                if (chapterEntry.is_directory() && parse_positive_int(chapterEntry.path().filename().string()) > 0) {
                    chapterEntries.push_back(chapterEntry);
                }
            }
            std::sort(chapterEntries.begin(), chapterEntries.end(), [](const auto& left, const auto& right) {
                return parse_positive_int(left.path().filename().string()) < parse_positive_int(right.path().filename().string());
            });

            for (const auto& chapterEntry : chapterEntries) {
                const int chapterNumber = parse_positive_int(chapterEntry.path().filename().string());
                database_.insert_chapter(scanId, chapterNumber, std::filesystem::canonical(chapterEntry.path()));
                ++report.chapters;

                std::vector<std::filesystem::directory_entry> pageEntries;
                for (const auto& pageEntry : std::filesystem::directory_iterator(chapterEntry.path())) {
                    if (pageEntry.is_regular_file() && is_image_file(pageEntry.path()) && parse_positive_int(pageEntry.path().stem().string()) > 0) {
                        pageEntries.push_back(pageEntry);
                    }
                }
                std::sort(pageEntries.begin(), pageEntries.end(), [](const auto& left, const auto& right) {
                    return parse_positive_int(left.path().stem().string()) < parse_positive_int(right.path().stem().string());
                });

                for (const auto& pageEntry : pageEntries) {
                    PageRecord page;
                    page.chapter = chapterNumber;
                    page.page = parse_positive_int(pageEntry.path().stem().string());
                    page.filePath = std::filesystem::canonical(pageEntry.path());
                    page.mimeType = guess_mime_type(pageEntry.path().string());
                    page.sizeBytes = static_cast<long long>(std::filesystem::file_size(pageEntry.path()));
                    database_.insert_page(scanId, page);
                    ++report.pages;
                }
            }
        }
        database_.commit();
    } catch (...) {
        database_.rollback_noexcept();
        throw;
    }

    return report;
}

/**
 * @brief Indique si un fichier possède une extension image supportée.
 *
 * @param path Chemin du fichier à tester.
 * @return true pour jpg, jpeg, png ou webp.
 */
bool ScanLibraryIndexer::is_image_file(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".webp";
}

/**
 * @brief Convertit un nom de dossier ou fichier en entier strictement positif.
 *
 * Objectif projet :
 * Ignorer les dossiers non numériques et fichiers annexes sans interrompre la synchronisation.
 *
 * @param value Nom à interpréter.
 * @return Entier positif ou 0 si la valeur n'est pas exploitable.
 */
int ScanLibraryIndexer::parse_positive_int(const std::string& value) {
    if (value.empty()) {
        return 0;
    }
    if (!std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); })) {
        return 0;
    }
    try {
        const int parsed = std::stoi(value);
        return parsed > 0 ? parsed : 0;
    } catch (...) {
        return 0;
    }
}

/**
 * @brief Transforme un nom de dossier technique en titre lisible.
 *
 * @param folderName Identifiant de dossier du scan.
 * @return Titre destiné à l'affichage API ou client.
 */
std::string ScanLibraryIndexer::readable_title(std::string folderName) {
    std::replace(folderName.begin(), folderName.end(), '-', ' ');
    bool upperNext = true;
    for (char& c : folderName) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            upperNext = true;
        } else if (upperNext) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            upperNext = false;
        }
    }
    return folderName.empty() ? "Sans nom" : folderName;
}
