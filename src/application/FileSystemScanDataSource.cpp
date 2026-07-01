/**
 * @file FileSystemScanDataSource.cpp
 * @brief Source de donnees locale et cache offline basee sur l'arborescence ./scan.
 */
#include "application/FileSystemScanDataSource.hpp"
#include "infrastructure/SimpleJson.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {

bool is_positive_int(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); });
}

int parse_positive_int(const std::string& value) {
    if (!is_positive_int(value)) return 0;
    try { const int parsed = std::stoi(value); return parsed > 0 ? parsed : 0; } catch (...) { return 0; }
}

bool is_image_file(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".webp";
}

std::string title_from_id(std::string id) {
    std::replace(id.begin(), id.end(), '-', ' ');
    std::replace(id.begin(), id.end(), '_', ' ');
    bool upperNext = true;
    for (char& c : id) {
        if (std::isspace(static_cast<unsigned char>(c))) upperNext = true;
        else if (upperNext) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); upperNext = false; }
    }
    return id.empty() ? "Sans nom" : id;
}

/**
 * @brief Produit un nom de fichier sûr pour les données locales par profil.
 *
 * Les caractères non alphanumériques sont remplacés afin d'éviter de créer des chemins
 * inattendus depuis un identifiant de profil saisi par l'utilisateur.
 */
std::string safe_token(std::string value) {
    if (value.empty()) return "default";
    for (char& c : value) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc) && c != '-' && c != '_') c = '_';
    }
    return value;
}

std::vector<std::string> read_lines(const std::filesystem::path& file) {
    std::vector<std::string> lines;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

void write_lines(const std::filesystem::path& file, const std::vector<std::string>& lines) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::trunc);
    for (const auto& line : lines) out << line << '\n';
}

std::vector<std::string> split_pipe(const std::string& line) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : line) {
        if (c == '|') { parts.push_back(current); current.clear(); }
        else current.push_back(c);
    }
    parts.push_back(current);
    return parts;
}

} // namespace

FileSystemScanDataSource::FileSystemScanDataSource(std::filesystem::path scanRoot)
    : scanRoot_(std::move(scanRoot)) {}

std::filesystem::path FileSystemScanDataSource::profile_root(const std::string& profile) const {
    return std::filesystem::path("cache") / "profiles" / safe_token(profile);
}

std::filesystem::path FileSystemScanDataSource::profile_file(const std::string& profile, const std::string& name) const {
    return profile_root(profile) / name;
}

/**
 * @brief Parcourt le dossier local `scan` et construit la bibliothèque visible.
 *
 * Le flux reste tolérant : un dossier mal formé est simplement peu renseigné plutôt que de
 * bloquer toute la liste.
 */
std::vector<ScanSummary> FileSystemScanDataSource::list_scans(const std::string& profile) const {
    std::vector<ScanSummary> scans;
    std::set<std::string> favorites;
    for (const auto& line : read_lines(profile_file(profile, "favorites.txt"))) favorites.insert(line);

    if (!std::filesystem::exists(scanRoot_)) return scans;
    for (const auto& scanEntry : std::filesystem::directory_iterator(scanRoot_)) {
        if (!scanEntry.is_directory()) continue;
        const std::string id = scanEntry.path().filename().string();
        int chapterCount = 0;
        int pageCount = 0;
        std::string cover;
        for (const auto& chapterEntry : std::filesystem::directory_iterator(scanEntry.path())) {
            if (!chapterEntry.is_directory() || parse_positive_int(chapterEntry.path().filename().string()) == 0) continue;
            ++chapterCount;
            for (const auto& pageEntry : std::filesystem::directory_iterator(chapterEntry.path())) {
                if (pageEntry.is_regular_file() && is_image_file(pageEntry.path())) {
                    ++pageCount;
                    if (cover.empty()) cover = pageEntry.path().string();
                }
            }
        }
        ScanSummary summary{id, title_from_id(id), chapterCount, pageCount, ScanProgress{1, 1}, false, "", ""};
        summary.progress = load_progress(id, profile);
        summary.favorite = favorites.count(id) > 0;
        summary.coverImageUrl = cover;
        scans.push_back(summary);
    }
    std::sort(scans.begin(), scans.end(), [](const auto& left, const auto& right) { return left.title < right.title; });
    return scans;
}

std::vector<int> FileSystemScanDataSource::list_chapters(const std::string& scanId) const {
    std::vector<int> chapters;
    const auto scanPath = scanRoot_ / scanId;
    if (!std::filesystem::exists(scanPath)) return chapters;
    for (const auto& entry : std::filesystem::directory_iterator(scanPath)) {
        if (entry.is_directory()) {
            const int chapter = parse_positive_int(entry.path().filename().string());
            if (chapter > 0) chapters.push_back(chapter);
        }
    }
    std::sort(chapters.begin(), chapters.end());
    return chapters;
}

std::vector<ScanPageInfo> FileSystemScanDataSource::list_pages(const std::string& scanId, int chapter) const {
    std::vector<ScanPageInfo> pages;
    const auto chapterPath = scanRoot_ / scanId / std::to_string(chapter);
    if (!std::filesystem::exists(chapterPath)) return pages;
    for (const auto& entry : std::filesystem::directory_iterator(chapterPath)) {
        if (!entry.is_regular_file() || !is_image_file(entry.path())) continue;
        const int page = parse_positive_int(entry.path().stem().string());
        if (page > 0) pages.push_back({chapter, page, "", static_cast<long long>(std::filesystem::file_size(entry.path())), entry.path().string()});
    }
    std::sort(pages.begin(), pages.end(), [](const auto& left, const auto& right) { return left.page < right.page; });
    return pages;
}

/**
 * @brief Résout le chemin local d'une page demandée.
 *
 * @throws std::runtime_error si la page demandée n'existe pas dans l'arborescence locale.
 */
std::filesystem::path FileSystemScanDataSource::materialize_page(const std::string& scanId, ScanProgress progress) const {
    progress.normalize();
    const auto chapterPath = scanRoot_ / scanId / std::to_string(progress.chapter);
    static const char* extensions[] = {".jpg", ".jpeg", ".png", ".webp"};
    for (const auto* extension : extensions) {
        auto candidate = chapterPath / (std::to_string(progress.page) + extension);
        if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) return candidate;
    }
    throw std::runtime_error("Page introuvable dans la source fichier: " + scanId);
}

ScanProgress FileSystemScanDataSource::load_progress(const std::string& scanId, const std::string& profile) const {
    if (profile != "default") {
        const auto file = profile_root(profile) / "progress" / (safe_token(scanId) + ".json");
        std::ifstream in(file);
        if (in) {
            std::ostringstream buffer; buffer << in.rdbuf();
            ScanProgress progress{scangui::json::int_field(buffer.str(), "chapter", 1), scangui::json::int_field(buffer.str(), "page", 1)};
            progress.normalize();
            return progress;
        }
    }
    if (auto metadata = repository_.load(scanRoot_ / scanId)) return metadata->saveProgress;
    return {1, 1};
}

/**
 * @brief Sauvegarde la progression dans `data.json` et alimente l'historique local.
 */
void FileSystemScanDataSource::save_progress(const std::string& scanId, ScanProgress progress, const std::string& profile) const {
    progress.normalize();
    if (profile == "default") {
        repository_.save_progress(scanRoot_ / scanId, progress);
    }
    const auto file = profile_root(profile) / "progress" / (safe_token(scanId) + ".json");
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::trunc);
    out << "{\"scanId\":\"" << scangui::json::escape(scanId) << "\",\"chapter\":" << progress.chapter << ",\"page\":" << progress.page << "}\n";

    auto history = read_lines(profile_file(profile, "history.txt"));
    history.insert(history.begin(), scanId + "|" + std::to_string(progress.chapter) + "|" + std::to_string(progress.page) + "|local");
    if (history.size() > 100) history.resize(100);
    write_lines(profile_file(profile, "history.txt"), history);
}

std::vector<ProfileSummary> FileSystemScanDataSource::list_profiles() const {
    std::vector<ProfileSummary> profiles{{"default", "Default", "#3b82f6"}};
    const auto root = std::filesystem::path("cache") / "profiles";
    if (!std::filesystem::exists(root)) return profiles;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_directory()) continue;
        const std::string id = entry.path().filename().string();
        if (id != "default") profiles.push_back({id, title_from_id(id), "#64748b"});
    }
    return profiles;
}

void FileSystemScanDataSource::save_profile(const ProfileSummary& profile) const {
    std::filesystem::create_directories(profile_root(profile.id));
    std::ofstream out(profile_file(profile.id, "profile.json"), std::ios::trunc);
    out << "{\"id\":\"" << scangui::json::escape(profile.id) << "\",\"displayName\":\"" << scangui::json::escape(profile.displayName) << "\",\"avatarColor\":\"" << scangui::json::escape(profile.avatarColor) << "\"}\n";
}

std::vector<std::string> FileSystemScanDataSource::list_favorites(const std::string& profile) const {
    return read_lines(profile_file(profile, "favorites.txt"));
}

void FileSystemScanDataSource::set_favorite(const std::string& scanId, bool favorite, const std::string& profile) const {
    auto lines = read_lines(profile_file(profile, "favorites.txt"));
    lines.erase(std::remove(lines.begin(), lines.end(), scanId), lines.end());
    if (favorite) lines.push_back(scanId);
    write_lines(profile_file(profile, "favorites.txt"), lines);
}

std::vector<BookmarkSummary> FileSystemScanDataSource::list_bookmarks(const std::string& scanId, const std::string& profile) const {
    std::vector<BookmarkSummary> bookmarks;
    long long id = 1;
    for (const auto& line : read_lines(profile_file(profile, "bookmarks.txt"))) {
        auto parts = split_pipe(line);
        if (parts.size() < 4 || parts[0] != scanId) continue;
        bookmarks.push_back({id++, parts[0], parse_positive_int(parts[1]), parse_positive_int(parts[2]), parts[3], "local"});
    }
    return bookmarks;
}

BookmarkSummary FileSystemScanDataSource::add_bookmark(const std::string& scanId, ScanProgress progress, const std::string& note, const std::string& profile) const {
    progress.normalize();
    auto lines = read_lines(profile_file(profile, "bookmarks.txt"));
    lines.push_back(scanId + "|" + std::to_string(progress.chapter) + "|" + std::to_string(progress.page) + "|" + note);
    write_lines(profile_file(profile, "bookmarks.txt"), lines);
    return {static_cast<long long>(lines.size()), scanId, progress.chapter, progress.page, note, "local"};
}

std::vector<HistoryEntry> FileSystemScanDataSource::list_history(const std::string& profile, int limit) const {
    std::vector<HistoryEntry> history;
    for (const auto& line : read_lines(profile_file(profile, "history.txt"))) {
        auto parts = split_pipe(line);
        if (parts.size() < 4) continue;
        history.push_back({parts[0], title_from_id(parts[0]), parse_positive_int(parts[1]), parse_positive_int(parts[2]), parts[3]});
        if (static_cast<int>(history.size()) >= limit) break;
    }
    return history;
}

/**
 * @brief Recherche une chaîne dans les sidecars `.txt` proches des images locales.
 *
 * Cette recherche fournit une alternative offline simple à l'index PostgreSQL/OCR.
 */
std::vector<SearchResultSummary> FileSystemScanDataSource::search_text(const std::string& query, int limit) const {
    std::vector<SearchResultSummary> results;
    if (query.empty() || limit <= 0 || !std::filesystem::exists(scanRoot_)) return results;
    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& scanEntry : std::filesystem::directory_iterator(scanRoot_)) {
        if (!scanEntry.is_directory()) continue;
        const std::string scanId = scanEntry.path().filename().string();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(scanEntry.path())) {
            if (!entry.is_regular_file() || entry.path().extension() != ".txt") continue;
            std::ifstream in(entry.path());
            std::ostringstream buffer;
            buffer << in.rdbuf();
            std::string text = buffer.str();
            std::string lower = text;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            auto pos = lower.find(q);
            if (pos == std::string::npos) continue;

            int chapter = 1;
            int page = 1;
            auto parentName = entry.path().parent_path().filename().string();
            auto stemName = entry.path().stem().string();
            chapter = parse_positive_int(parentName);
            page = parse_positive_int(stemName);
            if (chapter <= 0) chapter = 1;
            if (page <= 0) page = 1;

            const std::size_t begin = pos > 80 ? pos - 80 : 0;
            std::string snippet = text.substr(begin, 220);
            results.push_back({scanId, title_from_id(scanId), chapter, page, snippet, "local"});
            if (static_cast<int>(results.size()) >= limit) return results;
        }
    }
    return results;
}
