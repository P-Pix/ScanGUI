/**
 * @file ApiScanDataSource.hpp
 * @brief Déclare la source de données qui consomme le serveur HTTP local.
 *
 * Cette implémentation matérialise les pages reçues par l'API dans un cache local afin que
 * l'interface GTK puisse continuer à afficher des fichiers images, même lorsque la source
 * réelle des données est le serveur C++ adossé à PostgreSQL.
 */

#ifndef SCANGUI_APPLICATION_API_SCAN_DATA_SOURCE_HPP
#define SCANGUI_APPLICATION_API_SCAN_DATA_SOURCE_HPP

#include "application/ScanDataSource.hpp"
#include "infrastructure/CurlHttpClient.hpp"

/**
 * @brief Source de données reposant sur le serveur HTTP local ScanGUI.
 *
 * Objectif projet :
 * Permettre à l'application desktop de consommer les scans via une API locale plutôt que de
 * lire directement les dossiers. Les images sont matérialisées dans un cache pour conserver
 * l'affichage GTK basé sur des fichiers.
 *
 * Interagit avec :
 * - `ScanGUIServer` et ses endpoints `/api` ;
 * - le client HTTP libcurl ;
 * - le cache local des pages téléchargées.
 */
class ApiScanDataSource final : public ScanDataSource {
public:
    ApiScanDataSource(std::string baseUrl = "http://127.0.0.1:8787", std::filesystem::path cacheRoot = "cache/api");

    /** @brief Charge la bibliothèque depuis `/api/scans`. */
    [[nodiscard]] std::vector<ScanSummary> list_scans(const std::string& profile = "default") const override;
    [[nodiscard]] std::vector<int> list_chapters(const std::string& scanId) const override;
    [[nodiscard]] std::vector<ScanPageInfo> list_pages(const std::string& scanId, int chapter) const override;
    /** @brief Télécharge si nécessaire l'image API dans le cache local. */
    [[nodiscard]] std::filesystem::path materialize_page(const std::string& scanId, ScanProgress progress) const override;
    [[nodiscard]] ScanProgress load_progress(const std::string& scanId, const std::string& profile = "default") const override;
    /** @brief Envoie la progression courante à l'endpoint `/progress`. */
    void save_progress(const std::string& scanId, ScanProgress progress, const std::string& profile = "default") const override;

    [[nodiscard]] std::vector<ProfileSummary> list_profiles() const override;
    void save_profile(const ProfileSummary& profile) const override;
    [[nodiscard]] std::vector<std::string> list_favorites(const std::string& profile) const override;
    void set_favorite(const std::string& scanId, bool favorite, const std::string& profile = "default") const override;
    [[nodiscard]] std::vector<BookmarkSummary> list_bookmarks(const std::string& scanId, const std::string& profile = "default") const override;
    BookmarkSummary add_bookmark(const std::string& scanId, ScanProgress progress, const std::string& note, const std::string& profile = "default") const override;
    [[nodiscard]] std::vector<HistoryEntry> list_history(const std::string& profile = "default", int limit = 20) const override;
    [[nodiscard]] std::vector<SearchResultSummary> search_text(const std::string& query, int limit = 20) const override;

private:
    std::string baseUrl_;
    std::filesystem::path cacheRoot_;
    CurlHttpClient http_;

    [[nodiscard]] std::string endpoint(const std::string& path) const;
    [[nodiscard]] static std::string url_encode(const std::string& value);
};

#endif
