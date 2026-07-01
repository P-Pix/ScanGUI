/**
 * @file ScanSession.cpp
 * @brief Implémente le moteur de navigation dans les pages locales.
 *
 * Ce fichier contient la logique métier indépendante de GTK pour résoudre les chapitres,
 * pages et images disponibles dans l'arborescence d'un scan.
 */

#include "domain/ScanSession.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace {

/**
 * @brief Vérifie qu'un fichier porte une extension image supportée.
 */
bool is_image_extension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp";
}

/**
 * @brief Parse un nom de chapitre ou de page numérique.
 *
 * Les dossiers ou fichiers annexes sont ignorés s'ils ne respectent pas le format attendu.
 */
std::optional<int> parse_positive_int(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }
    if (!std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); })) {
        return std::nullopt;
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

} // namespace

ScanSession::ScanSession(std::filesystem::path root, ScanProgress initial) {
    open(std::move(root), initial);
}

/**
 * @brief Ouvre une racine de scan et normalise la progression initiale.
 */
void ScanSession::open(std::filesystem::path root, ScanProgress initial) {
    root_ = std::move(root);
    progress_ = initial;
    progress_.normalize();
}

bool ScanSession::is_open() const {
    return !root_.empty() && std::filesystem::exists(root_) && std::filesystem::is_directory(root_);
}

const std::filesystem::path& ScanSession::root() const {
    return root_;
}

ScanProgress ScanSession::progress() const {
    return progress_;
}

std::filesystem::path ScanSession::current_page_path() const {
    auto path = page_path(progress_.chapter, progress_.page);
    return path.value_or(root_ / std::to_string(progress_.chapter) / (std::to_string(progress_.page) + ".jpg"));
}

std::optional<ScanPage> ScanSession::current_page() const {
    auto path = page_path(progress_.chapter, progress_.page);
    if (!path) {
        return std::nullopt;
    }
    return ScanPage{*path, progress_};
}

/**
 * @brief Avance vers la page suivante disponible.
 *
 * Objectif projet :
 * Conserver une navigation fluide même lorsque la dernière page d'un chapitre est atteinte :
 * la session bascule alors vers le premier fichier image du prochain chapitre disponible.
 *
 * @return Page suivante résolue ou page courante si aucune suite n'existe.
 */
std::optional<ScanPage> ScanSession::next_page() {
    if (!is_open()) {
        return std::nullopt;
    }

    if (auto next = page_path(progress_.chapter, progress_.page + 1)) {
        ++progress_.page;
        return ScanPage{*next, progress_};
    }

    const auto available_chapters = chapters();
    auto it = std::upper_bound(available_chapters.begin(), available_chapters.end(), progress_.chapter);
    while (it != available_chapters.end()) {
        auto pages = pages_for_chapter(*it);
        if (!pages.empty()) {
            progress_.chapter = *it;
            progress_.page = pages.front();
            return current_page();
        }
        ++it;
    }

    return current_page();
}

/**
 * @brief Recule vers la page précédente disponible.
 *
 * Objectif projet :
 * Permettre une navigation inverse entre chapitres en revenant à la dernière page du chapitre
 * précédent lorsque la page courante est la première du chapitre.
 */
std::optional<ScanPage> ScanSession::previous_page() {
    if (!is_open()) {
        return std::nullopt;
    }

    if (progress_.page > 1) {
        if (auto previous = page_path(progress_.chapter, progress_.page - 1)) {
            --progress_.page;
            return ScanPage{*previous, progress_};
        }
    }

    const auto available_chapters = chapters();
    auto it = std::lower_bound(available_chapters.begin(), available_chapters.end(), progress_.chapter);
    while (it != available_chapters.begin()) {
        --it;
        auto pages = pages_for_chapter(*it);
        if (!pages.empty()) {
            progress_.chapter = *it;
            progress_.page = pages.back();
            return current_page();
        }
    }

    return current_page();
}

/**
 * @brief Tente de déplacer la session vers une position explicite.
 *
 * La progression n'est modifiée que si la page demandée existe réellement.
 */
std::optional<ScanPage> ScanSession::go_to(ScanProgress requested) {
    requested.normalize();
    if (auto path = page_path(requested.chapter, requested.page)) {
        progress_ = requested;
        return ScanPage{*path, progress_};
    }
    return std::nullopt;
}

int ScanSession::max_chapter() const {
    const auto available_chapters = chapters();
    if (available_chapters.empty()) {
        return 0;
    }
    return available_chapters.back();
}

int ScanSession::max_page_in_current_chapter() const {
    const auto pages = pages_for_chapter(progress_.chapter);
    if (pages.empty()) {
        return 0;
    }
    return pages.back();
}

/**
 * @brief Résout le chemin fichier correspondant à un couple chapitre/page.
 *
 * La méthode teste les extensions image supportées et ne retourne un chemin que si le fichier
 * existe réellement, ce qui évite à l'UI de charger une image absente.
 */
std::optional<std::filesystem::path> ScanSession::page_path(int chapter, int page) const {
    if (root_.empty() || chapter < 1 || page < 1) {
        return std::nullopt;
    }

    const auto chapter_dir = root_ / std::to_string(chapter);
    if (!std::filesystem::exists(chapter_dir) || !std::filesystem::is_directory(chapter_dir)) {
        return std::nullopt;
    }

    static const char* extensions[] = {".jpg", ".jpeg", ".png", ".webp"};
    for (const auto* extension : extensions) {
        auto candidate = chapter_dir / (std::to_string(page) + extension);
        if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

/**
 * @brief Liste les chapitres numériques présents dans le dossier du scan.
 *
 * Les dossiers non numériques sont ignorés pour tolérer des fichiers ou dossiers annexes dans
 * la bibliothèque locale.
 */
std::vector<int> ScanSession::chapters() const {
    std::vector<int> result;
    if (root_.empty() || !std::filesystem::exists(root_) || !std::filesystem::is_directory(root_)) {
        return result;
    }

    for (const auto& entry : std::filesystem::directory_iterator(root_)) {
        if (!entry.is_directory()) {
            continue;
        }
        auto parsed = parse_positive_int(entry.path().filename().string());
        if (parsed) {
            result.push_back(*parsed);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<int> ScanSession::pages_for_chapter(int chapter) const {
    std::vector<int> result;
    if (root_.empty() || chapter < 1) {
        return result;
    }

    const auto chapter_dir = root_ / std::to_string(chapter);
    if (!std::filesystem::exists(chapter_dir) || !std::filesystem::is_directory(chapter_dir)) {
        return result;
    }

    for (const auto& entry : std::filesystem::directory_iterator(chapter_dir)) {
        if (!entry.is_regular_file() || !is_image_extension(entry.path())) {
            continue;
        }
        auto parsed = parse_positive_int(entry.path().stem().string());
        if (parsed) {
            result.push_back(*parsed);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
