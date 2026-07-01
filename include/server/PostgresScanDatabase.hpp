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
    int progressChapter{1};
    int progressPage{1};
    bool favorite{false};
    std::string lastReadAt;
    std::string coverImageUrl;
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


/** @brief Profil utilisateur expose par l'API locale. */
struct ProfileRecord {
    std::string id;
    std::string displayName;
    std::string avatarColor;
    std::string createdAt;
};

/** @brief Marque-page utilisateur. */
struct BookmarkRecord {
    long long id{0};
    std::string scanId;
    int chapter{1};
    int page{1};
    std::string note;
    std::string createdAt;
};

/** @brief Ligne d'historique de lecture. */
struct HistoryRecord {
    std::string scanId;
    std::string title;
    int chapter{1};
    int page{1};
    std::string readAt;
};


/** @brief Resultat de recherche plein texte sur les pages OCR. */
struct SearchResultRecord {
    std::string scanId;
    std::string title;
    int chapter{1};
    int page{1};
    std::string snippet;
};

/** @brief Texte OCR ou sidecar indexe pour une page. */
struct PageTextRecord {
    std::string scanId;
    int chapter{1};
    int page{1};
    std::string text;
};

/** @brief Compteurs globaux de bibliotheque. */
struct LibraryStatsRecord {
    int scans{0};
    int chapters{0};
    int pages{0};
    int profiles{0};
    int favorites{0};
    int bookmarks{0};
    int pageTexts{0};
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

    /**
     * @brief Crée ou met à jour le schéma SQL attendu par le serveur.
     *
     * La méthode initialise aussi le profil `default` pour garantir une progression utilisable
     * dès le premier démarrage.
     */
    void initialize_schema();
    /** @brief Ouvre une transaction PostgreSQL explicite. */
    void begin();
    /** @brief Valide la transaction courante. */
    void commit();
    /** @brief Annule la transaction courante sans propager d’exception. */
    void rollback_noexcept();

    /**
     * @brief Insère ou met à jour les métadonnées principales d’un scan.
     *
     * @param scan Ligne synthétique issue de l’indexation disque.
     */
    void upsert_scan(const ScanSummaryRecord& scan);
    /**
     * @brief Supprime chapitres, pages et textes associés avant réindexation.
     *
     * @param scanId Identifiant du scan à nettoyer.
     */
    void delete_scan_content(const std::string& scanId);
    void insert_chapter(const std::string& scanId, int chapterNumber, const std::filesystem::path& path);
    void insert_page(const std::string& scanId, const PageRecord& page);

    /**
     * @brief Liste les scans enrichis de la progression et des favoris d’un profil.
     *
     * @param profile Profil utilisé pour joindre l’état utilisateur.
     * @return Scans triés pour l’API.
     */
    [[nodiscard]] std::vector<ScanSummaryRecord> list_scans(const std::string& profile = "default") const;
    [[nodiscard]] std::optional<ScanSummaryRecord> get_scan(const std::string& scanId, const std::string& profile = "default") const;
    [[nodiscard]] std::vector<ChapterRecord> list_chapters(const std::string& scanId) const;
    [[nodiscard]] std::vector<PageRecord> list_pages(const std::string& scanId, int chapterNumber) const;
    /**
     * @brief Résout une page précise en base.
     *
     * @param scanId Identifiant du scan.
     * @param chapterNumber Numéro du chapitre.
     * @param pageNumber Numéro de page.
     * @return Page trouvée ou absence contrôlée.
     */
    [[nodiscard]] std::optional<PageRecord> get_page(const std::string& scanId, int chapterNumber, int pageNumber) const;

    [[nodiscard]] ScanProgress get_progress(const std::string& scanId, const std::string& profile) const;
    /**
     * @brief Enregistre la progression de lecture et alimente l’historique.
     *
     * Effet de bord : met à jour `reading_progress` et `reading_history`.
     */
    void save_progress(const std::string& scanId, const std::string& profile, ScanProgress progress);

    [[nodiscard]] std::vector<ProfileRecord> list_profiles() const;
    void upsert_profile(const ProfileRecord& profile);
    [[nodiscard]] std::vector<std::string> list_favorites(const std::string& profile) const;
    [[nodiscard]] bool is_favorite(const std::string& scanId, const std::string& profile) const;
    [[nodiscard]] std::string last_read_at(const std::string& scanId, const std::string& profile) const;
    /** @brief Met à jour l’état favori d’un scan pour un profil. */
    void set_favorite(const std::string& scanId, const std::string& profile, bool favorite);
    [[nodiscard]] std::vector<BookmarkRecord> list_bookmarks(const std::string& scanId, const std::string& profile) const;
    /**
     * @brief Crée un marque-page persistant pour une position de lecture.
     *
     * @return Ligne créée avec identifiant PostgreSQL.
     */
    [[nodiscard]] BookmarkRecord add_bookmark(const std::string& scanId, const std::string& profile, ScanProgress progress, const std::string& note);
    [[nodiscard]] std::vector<HistoryRecord> list_history(const std::string& profile, int limit = 20) const;

    /** @brief Insère ou remplace le texte OCR/sidecar associé à une page. */
    void upsert_page_text(const std::string& scanId, int chapter, int page, const std::string& text);
    /**
     * @brief Recherche dans les textes indexés avec une correspondance insensible à la casse.
     *
     * @param query Terme utilisateur recherché.
     * @param limit Nombre maximal de résultats.
     * @return Résultats prêts pour l’API.
     */
    [[nodiscard]] std::vector<SearchResultRecord> search_page_text(const std::string& query, int limit = 50) const;
    [[nodiscard]] std::vector<PageTextRecord> chapter_text(const std::string& scanId, int chapter) const;

    /**
     * @brief Calcule les compteurs globaux de la bibliothèque.
     *
     * @return Statistiques synthétiques pour `/api/stats`.
     */
    [[nodiscard]] LibraryStatsRecord stats() const;

private:
    PGconn* connection_{nullptr};

    void exec_command(const std::string& sql) const;
    /**
     * @brief Exécute une requête paramétrée libpq.
     *
     * Objectif sécurité : éviter la concaténation SQL pour les valeurs issues des routes API.
     *
     * @param sql Requête contenant les placeholders PostgreSQL.
     * @param params Valeurs à binder.
     * @return Résultat PostgreSQL à libérer avec `clear_result`.
     */
    [[nodiscard]] PGresult* exec_params(const std::string& sql, const std::vector<std::string>& params) const;
    static void clear_result(PGresult* result);
};

#endif
