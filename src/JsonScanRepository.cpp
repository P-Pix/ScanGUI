/**
 * @file JsonScanRepository.cpp
 * @brief Implémente la persistance JSON historique des métadonnées de scan.
 *
 * Ce repository lit et écrit le fichier `data.json` placé dans chaque dossier de scan afin
 * de conserver l'URL source, la progression de téléchargement et la progression de lecture.
 */

#include "infrastructure/JsonScanRepository.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Impossible de lire le fichier: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string escape_json_string(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

std::optional<std::string> extract_section(const std::string& content, const std::string& sectionName) {
    const std::regex sectionRegex("\\\"" + sectionName + "\\\"\\s*:\\s*\\{([^}]*)\\}");
    std::smatch match;
    if (std::regex_search(content, match, sectionRegex) && match.size() > 1) {
        return match[1].str();
    }
    return std::nullopt;
}

std::optional<int> extract_int(const std::string& content, const std::string& key) {
    const std::regex valueRegex("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (std::regex_search(content, match, valueRegex) && match.size() > 1) {
        try {
            return std::stoi(match[1].str());
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<std::string> extract_string(const std::string& content, const std::string& key) {
    const std::regex valueRegex("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (std::regex_search(content, match, valueRegex) && match.size() > 1) {
        return match[1].str();
    }
    return std::nullopt;
}

ScanProgress read_progress(const std::string& section) {
    ScanProgress progress;
    progress.chapter = extract_int(section, "chapter").value_or(1);
    progress.page = extract_int(section, "page").value_or(1);
    progress.normalize();
    return progress;
}

} // namespace

/**
 * @brief Charge les métadonnées `data.json` d'un dossier de scan.
 *
 * Objectif projet :
 * Absorber l'absence du fichier comme un cas normal afin de permettre l'ouverture ou
 * l'indexation de dossiers partiellement préparés.
 *
 * @param scanFolder Dossier racine du scan.
 * @return Métadonnées lues ou absence contrôlée si le fichier n'existe pas.
 */
std::optional<ScanMetadata> JsonScanRepository::load(const std::filesystem::path& scanFolder) const {
    const auto path = metadata_path(scanFolder);
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    ScanMetadata metadata;
    const std::string content = read_file(path);

    if (auto download = extract_section(content, "download")) {
        metadata.downloadUrl = extract_string(*download, "url").value_or("");
        metadata.downloadProgress = read_progress(*download);
    }

    if (auto save = extract_section(content, "save")) {
        metadata.saveProgress = read_progress(*save);
    } else {
        metadata.saveProgress = metadata.downloadProgress;
    }

    metadata.normalize();
    return metadata;
}

void JsonScanRepository::save(const std::filesystem::path& scanFolder, const ScanMetadata& metadata) const {
    std::filesystem::create_directories(scanFolder);

    ScanMetadata normalized = metadata;
    normalized.normalize();

    std::ofstream output(metadata_path(scanFolder), std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Impossible d'ecrire le fichier data.json dans: " + scanFolder.string());
    }

    output << "{\n"
           << "  \"download\": {\n"
           << "    \"url\": \"" << escape_json_string(normalized.downloadUrl) << "\",\n"
           << "    \"chapter\": " << normalized.downloadProgress.chapter << ",\n"
           << "    \"page\": " << normalized.downloadProgress.page << "\n"
           << "  },\n"
           << "  \"save\": {\n"
           << "    \"chapter\": " << normalized.saveProgress.chapter << ",\n"
           << "    \"page\": " << normalized.saveProgress.page << "\n"
           << "  }\n"
           << "}\n";
}

/**
 * @brief Met à jour uniquement la progression de lecture d'un scan.
 *
 * Objectif projet :
 * Préserver les métadonnées de téléchargement existantes tout en enregistrant la position
 * courante après navigation utilisateur.
 */
void JsonScanRepository::save_progress(const std::filesystem::path& scanFolder, ScanProgress progress) const {
    progress.normalize();
    ScanMetadata metadata = load(scanFolder).value_or(ScanMetadata{});
    metadata.saveProgress = progress;
    save(scanFolder, metadata);
}

std::filesystem::path JsonScanRepository::metadata_path(const std::filesystem::path& scanFolder) const {
    return scanFolder / "data.json";
}
