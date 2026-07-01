/**
 * @file JsonScanRepository.cpp
 * @brief Implémente la persistance JSON historique des métadonnées de scan.
 *
 * Ce repository lit et écrit le fichier `data.json` placé dans chaque dossier de scan afin
 * de conserver l'URL source, la progression de téléchargement et la progression de lecture.
 */

#include "infrastructure/JsonScanRepository.hpp"
#include "infrastructure/SimpleJson.hpp"

#include <cmath>
#include <fstream>
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

std::optional<scangui::json::Value> object_field(const scangui::json::Value& root, const std::string& key) {
    const auto* field = scangui::json::find_field(root, key);
    if (field == nullptr || !field->is_object()) {
        return std::nullopt;
    }
    return *field;
}

std::optional<int> int_field(const scangui::json::Value& object, const std::string& key) {
    const auto* field = scangui::json::find_field(object, key);
    if (field == nullptr || !field->is_number()) {
        return std::nullopt;
    }
    return static_cast<int>(std::llround(field->as_number()));
}

std::optional<std::string> string_field(const scangui::json::Value& object, const std::string& key) {
    const auto* field = scangui::json::find_field(object, key);
    if (field == nullptr || !field->is_string()) {
        return std::nullopt;
    }
    return field->as_string();
}

ScanProgress read_progress(const scangui::json::Value& section) {
    ScanProgress progress;
    progress.chapter = int_field(section, "chapter").value_or(1);
    progress.page = int_field(section, "page").value_or(1);
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
    const auto root = scangui::json::parse(content);
    if (!root || !root->is_object()) {
        return std::nullopt;
    }

    if (auto download = object_field(*root, "download")) {
        metadata.downloadUrl = string_field(*download, "url").value_or("");
        metadata.downloadProgress = read_progress(*download);
    }

    if (auto save = object_field(*root, "save")) {
        metadata.saveProgress = read_progress(*save);
    } else {
        metadata.saveProgress = metadata.downloadProgress;
    }

    metadata.normalize();
    return metadata;
}

/**
 * @brief Écrit les métadonnées complètes d'un scan dans `data.json`.
 *
 * @throws std::runtime_error si le fichier ne peut pas être ouvert en écriture.
 */
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
           << "    \"url\": \"" << scangui::json::escape(normalized.downloadUrl) << "\",\n"
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
