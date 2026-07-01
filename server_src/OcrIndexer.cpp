/**
 * @file OcrIndexer.cpp
 * @brief Implémente l'indexation texte/OCR des pages stockées en base PostgreSQL.
 *
 * L'indexeur privilégie les fichiers sidecar `.txt` et peut appeler une commande externe
 * configurée pour alimenter la table `page_texts` utilisée par la recherche.
 */
#include "server/OcrIndexer.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

std::string env_or_empty(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

std::string shell_quote(const std::filesystem::path& path) {
    std::string input = path.string();
    std::string output = "'";
    for (char c : input) {
        if (c == '\'') output += "'\\''";
        else output.push_back(c);
    }
    output.push_back('\'');
    return output;
}

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace

OcrIndexer::OcrIndexer(PostgresScanDatabase& database, std::string commandTemplate)
    : database_(database), commandTemplate_(std::move(commandTemplate)) {
    if (commandTemplate_.empty()) commandTemplate_ = env_or_empty("SCANGUI_TESSERACT_CMD");
    if (commandTemplate_.empty()) commandTemplate_ = env_or_empty("SCANGUI_OCR_COMMAND");
}

/**
 * @brief Indexe les pages de tous les scans présents en base.
 */
OcrIndexReport OcrIndexer::index_all() {
    OcrIndexReport total;
    for (const auto& scan : database_.list_scans("default")) {
        auto report = index_scan(scan.id);
        total.pagesVisited += report.pagesVisited;
        total.indexedPages += report.indexedPages;
        total.skippedPages += report.skippedPages;
        total.failedPages += report.failedPages;
    }
    return total;
}

/**
 * @brief Indexe les textes d'un scan précis.
 *
 * Les pages sans texte utile sont comptabilisées comme ignorées plutôt que considérées comme
 * erreurs, car l'OCR est optionnel dans le projet.
 */
OcrIndexReport OcrIndexer::index_scan(const std::string& scanId) {
    OcrIndexReport report;
    for (const auto& chapter : database_.list_chapters(scanId)) {
        for (const auto& page : database_.list_pages(scanId, chapter.number)) {
            ++report.pagesVisited;
            try {
                const std::string text = extract_text(page);
                if (!is_text_useful(text)) {
                    ++report.skippedPages;
                    continue;
                }
                database_.upsert_page_text(scanId, page.chapter, page.page, text);
                ++report.indexedPages;
            } catch (...) {
                ++report.failedPages;
            }
        }
    }
    return report;
}

/**
 * @brief Extrait le texte disponible pour une page via sidecar ou commande OCR.
 */
std::string OcrIndexer::extract_text(const PageRecord& page) const {
    std::string text = read_sidecar_text(page.filePath);
    if (is_text_useful(text)) return text;
    if (!commandTemplate_.empty()) return run_ocr_command(page.filePath);
    return {};
}

std::string OcrIndexer::read_sidecar_text(const std::filesystem::path& imagePath) const {
    std::filesystem::path txt = imagePath;
    txt.replace_extension(".txt");
    return read_file(txt);
}

std::string OcrIndexer::run_ocr_command(const std::filesystem::path& imagePath) const {
    const auto output = std::filesystem::temp_directory_path() / ("scangui-ocr-" + std::to_string(std::rand()) + ".txt");
    std::string command = commandTemplate_;
    if (command.find("{input}") != std::string::npos || command.find("{output}") != std::string::npos) {
        command = replace_all(command, "{input}", shell_quote(imagePath));
        command = replace_all(command, "{output}", shell_quote(output));
    } else {
        command += " " + shell_quote(imagePath) + " " + shell_quote(output);
    }
    const int exitCode = std::system(command.c_str());
    if (exitCode != 0) return {};
    std::string text = read_file(output);
    std::filesystem::remove(output);
    return text;
}

bool OcrIndexer::is_text_useful(const std::string& text) {
    for (unsigned char c : text) {
        if (!std::isspace(c)) return true;
    }
    return false;
}
