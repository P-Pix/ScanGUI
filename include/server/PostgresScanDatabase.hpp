/**
 * @file PostgresScanDatabase.hpp
 * @brief Déclare l'accès PostgreSQL utilisé par le serveur de scans.
 *
 * Cette classe centralise les requêtes SQL, les transactions et la conversion des résultats
 * PostgreSQL vers les structures métier exposées par l'API locale.
 */

#ifndef SCANGUI_SERVER_POSTGRES_SCAN_DATABASE_HPP
#define SCANGUI_SERVER_POSTGRES_SCAN_DATABASE_HPP

#include "domain/ScanProgress.hpp"

#include <libpq-fe.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Ligne synthétique d'un scan indexé en base.
 *
 * Le record alimente les endpoints de liste et de détail sans exposer directement les lignes
 * SQL au contrôleur.
 */
struct ScanSummaryRecord {
    std::string id;
    std::string title;
    std::string folderName;
    std::string rootPath;
    std::string downloadUrl;
    int chapterCount{0};
    int pageCount{0};
};

/**
 * @brief Chapitre indexé pour un scan donné.
 *
 * Le record fournit le numéro de chapitre et le nombre de pages disponibles pour l'API.
 */
struct ChapterRecord {
    int number{0};
    int pageCount{0};
};

/**
 * @brief Page image indexée en base PostgreSQL.
 *
 * Elle relie la position chapitre/page à un fichier local validable avant exposition par HTTP.
 */
struct PageRecord {
    int chapter{0};
    int page{0};
    std::filesystem::path filePath;
    std::string mimeType;
    long long sizeBytes{0};
};

/**
 * @brief Repository PostgreSQL du serveur de scans.
 *
 * Objectif projet :
 * Centraliser les accès SQL, transactions et conversions de résultats afin que le contrôleur
 * REST et l'indexeur ne manipulent pas directement libpq.
 *
 * Interagit avec :
 * - les tables `scans`, `chapters`, `pages` et `reading_progress` ;
 * - l'indexeur de bibliothèque ;
 * - le contrôleur API.
 */
class PostgresScanDatabase {
public:
    explicit PostgresScanDatabase(std::string connectionString);
    ~PostgresScanDatabase();

    PostgresScanDatabase(const PostgresScanDatabase&) = delete;
    PostgresScanDatabase& operator=(const PostgresScanDatabase&) = delete;

    void initialize_schema();
    void begin();
    void commit();
    void rollback_noexcept();

    void upsert_scan(const ScanSummaryRecord& scan);
    void delete_scan_content(const std::string& scanId);
    void insert_chapter(const std::string& scanId, int chapterNumber, const std::filesystem::path& path);
    void insert_page(const std::string& scanId, const PageRecord& page);

    [[nodiscard]] std::vector<ScanSummaryRecord> list_scans() const;
    [[nodiscard]] std::optional<ScanSummaryRecord> get_scan(const std::string& scanId) const;
    [[nodiscard]] std::vector<ChapterRecord> list_chapters(const std::string& scanId) const;
    [[nodiscard]] std::vector<PageRecord> list_pages(const std::string& scanId, int chapterNumber) const;
    [[nodiscard]] std::optional<PageRecord> get_page(const std::string& scanId, int chapterNumber, int pageNumber) const;

    [[nodiscard]] ScanProgress get_progress(const std::string& scanId, const std::string& profile) const;
    void save_progress(const std::string& scanId, const std::string& profile, ScanProgress progress);

private:
    PGconn* connection_{nullptr};

    void exec_command(const std::string& sql) const;
    [[nodiscard]] PGresult* exec_params(const std::string& sql, const std::vector<std::string>& params) const;
    static void clear_result(PGresult* result);
};

#endif
