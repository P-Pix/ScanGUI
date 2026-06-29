/**
 * @file FileSystemScanDataSource.cpp
 * @brief Implémente la source de données directe sur le système de fichiers.
 *
 * Ce fichier conserve le fonctionnement historique du lecteur en parcourant `./scan` sans
 * passer par l'API locale ni PostgreSQL.
 */

#include "application/FileSystemScanDataSource.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

bool is_positive_int(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); });
}

int parse_positive_int(const std::string& value) {
    if (!is_positive_int(value)) {
        return 0;
    }
    try {
        const int parsed = std::stoi(value);
        return parsed > 0 ? parsed : 0;
    } catch (...) {
        return 0;
    }
}

bool is_image_file(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".webp";
}

std::string title_from_id(std::string id) {
    std::replace(id.begin(), id.end(), '-', ' ');
    bool upperNext = true;
    for (char& c : id) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            upperNext = true;
        } else if (upperNext) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            upperNext = false;
        }
    }
    return id.empty() ? "Sans nom" : id;
}

} // namespace

FileSystemScanDataSource::FileSystemScanDataSource(std::filesystem::path scanRoot)
    : scanRoot_(std::move(scanRoot)) {
}

/**
 * @brief Parcourt `scanRoot_` pour construire la bibliothèque locale.
 *
 * Objectif projet :
 * Fournir une liste compatible avec l'abstraction `ScanDataSource` sans nécessiter le serveur
 * C++ ni PostgreSQL.
 *
 * @return Scans trouvés avec compteurs de chapitres et pages.
 */
std::vector<ScanSummary> FileSystemScanDataSource::list_scans() const {
    std::vector<ScanSummary> scans;
    if (!std::filesystem::exists(scanRoot_)) {
        return scans;
    }
    for (const auto& scanEntry : std::filesystem::directory_iterator(scanRoot_)) {
        if (!scanEntry.is_directory()) {
            continue;
        }
        const std::string id = scanEntry.path().filename().string();
        int chapterCount = 0;
        int pageCount = 0;
        for (const auto& chapterEntry : std::filesystem::directory_iterator(scanEntry.path())) {
            if (!chapterEntry.is_directory() || parse_positive_int(chapterEntry.path().filename().string()) == 0) {
                continue;
            }
            ++chapterCount;
            for (const auto& pageEntry : std::filesystem::directory_iterator(chapterEntry.path())) {
                if (pageEntry.is_regular_file() && is_image_file(pageEntry.path())) {
                    ++pageCount;
                }
            }
        }
        scans.push_back({id, title_from_id(id), chapterCount, pageCount});
    }
    std::sort(scans.begin(), scans.end(), [](const auto& left, const auto& right) {
        return left.title < right.title;
    });
    return scans;
}

std::vector<int> FileSystemScanDataSource::list_chapters(const std::string& scanId) const {
    std::vector<int> chapters;
    const auto scanPath = scanRoot_ / scanId;
    if (!std::filesystem::exists(scanPath)) {
        return chapters;
    }
    for (const auto& entry : std::filesystem::directory_iterator(scanPath)) {
        if (entry.is_directory()) {
            const int chapter = parse_positive_int(entry.path().filename().string());
            if (chapter > 0) {
                chapters.push_back(chapter);
            }
        }
    }
    std::sort(chapters.begin(), chapters.end());
    return chapters;
}

std::vector<ScanPageInfo> FileSystemScanDataSource::list_pages(const std::string& scanId, int chapter) const {
    std::vector<ScanPageInfo> pages;
    const auto chapterPath = scanRoot_ / scanId / std::to_string(chapter);
    if (!std::filesystem::exists(chapterPath)) {
        return pages;
    }
    for (const auto& entry : std::filesystem::directory_iterator(chapterPath)) {
        if (!entry.is_regular_file() || !is_image_file(entry.path())) {
            continue;
        }
        const int page = parse_positive_int(entry.path().stem().string());
        if (page > 0) {
            pages.push_back({chapter, page, "", static_cast<long long>(std::filesystem::file_size(entry.path())), ""});
        }
    }
    std::sort(pages.begin(), pages.end(), [](const auto& left, const auto& right) {
        return left.page < right.page;
    });
    return pages;
}

/**
 * @brief Résout directement sur disque l'image d'une progression donnée.
 *
 * La méthode teste les extensions supportées pour conserver la compatibilité avec les scans
 * historiques qui n'utilisent pas tous le même format d'image.
 */
std::filesystem::path FileSystemScanDataSource::materialize_page(const std::string& scanId, ScanProgress progress) const {
    progress.normalize();
    const auto chapterPath = scanRoot_ / scanId / std::to_string(progress.chapter);
    static const char* extensions[] = {".jpg", ".jpeg", ".png", ".webp"};
    for (const auto* extension : extensions) {
        auto candidate = chapterPath / (std::to_string(progress.page) + extension);
        if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Page introuvable dans la source fichier: " + scanId);
}

ScanProgress FileSystemScanDataSource::load_progress(const std::string& scanId, const std::string&) const {
    if (auto metadata = repository_.load(scanRoot_ / scanId)) {
        return metadata->saveProgress;
    }
    return {1, 1};
}

void FileSystemScanDataSource::save_progress(const std::string& scanId, ScanProgress progress, const std::string&) const {
    repository_.save_progress(scanRoot_ / scanId, progress);
}
