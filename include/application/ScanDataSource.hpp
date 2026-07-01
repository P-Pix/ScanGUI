/**
 * @file ScanDataSource.hpp
 * @brief Définit l’abstraction d’accès aux scans utilisée par toute l’application.
 *
 * Cette interface découple l’interface GTK du support réel des données. Les scans peuvent
 * provenir du système de fichiers local, du serveur HTTP local adossé à PostgreSQL ou d’un
 * cache offline, sans modifier le code de la bibliothèque visuelle ni du lecteur.
 */
#ifndef SCANGUI_APPLICATION_SCAN_DATA_SOURCE_HPP
#define SCANGUI_APPLICATION_SCAN_DATA_SOURCE_HPP

#include "domain/ScanProgress.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief Vue synthétique d’un scan affichée dans la bibliothèque.
 *
 * Objectif projet :
 * Normaliser les informations nécessaires à l’interface quel que soit le support réel
 * des données : dossier local, API serveur ou cache offline. La structure agrège aussi
 * la progression du profil courant pour calculer l’avancement visible dans la liste.
 */
struct ScanSummary {
    std::string id;
    std::string title;
    int chapterCount{0};
    int pageCount{0};
    ScanProgress progress{1, 1};
    bool favorite{false};
    std::string lastReadAt;
    std::string coverImageUrl;

    /**
     * @brief Calcule un pourcentage d’avancement approximatif pour l’affichage.
     *
     * Le calcul ne dépend que du nombre global de pages et de chapitres afin de rester
     * disponible même lorsque toutes les pages ne sont pas chargées en mémoire.
     *
     * @return Ratio compris entre 0.0 et 1.0.
     */
    [[nodiscard]] double completion_percent() const {
        if (pageCount <= 0 || chapterCount <= 0) return 0.0;
        const int chapter = progress.chapter < 1 ? 1 : progress.chapter;
        const int page = progress.page < 1 ? 1 : progress.page;
        const double averagePagesPerChapter = static_cast<double>(pageCount) / static_cast<double>(chapterCount);
        double readPosition = (static_cast<double>(chapter - 1) * averagePagesPerChapter) + static_cast<double>(page);
        if (readPosition <= 1.0) return 0.0;
        double percent = readPosition / static_cast<double>(pageCount);
        if (percent < 0.0) return 0.0;
        if (percent > 1.0) return 1.0;
        return percent;
    }
};

/**
 * @brief Description minimale d’une page exposée par une source de données.
 *
 * Elle permet au lecteur de résoudre une image sans connaître l’arborescence physique
 * locale ni la route HTTP utilisée côté serveur.
 */
struct ScanPageInfo {
    int chapter{0};
    int page{0};
    std::string mimeType;
    long long sizeBytes{0};
    std::string imageUrl;
};

/**
 * @brief Profil utilisateur disponible dans la source active.
 *
 * Le profil sépare progression, favoris, historique et marque-pages afin que plusieurs
 * lecteurs puissent partager la même bibliothèque sans écraser leur état de lecture.
 */
struct ProfileSummary {
    std::string id;
    std::string displayName;
    std::string avatarColor{"#3b82f6"};
};

/**
 * @brief Marque-page attaché à une position de lecture précise.
 *
 * La structure est utilisée par l’interface pour lister et créer des points de reprise
 * annotés sans exposer la représentation PostgreSQL ou fichier.
 */
struct BookmarkSummary {
    long long id{0};
    std::string scanId;
    int chapter{1};
    int page{1};
    std::string note;
    std::string createdAt;
};

/**
 * @brief Entrée d’historique de lecture d’un profil.
 *
 * Elle alimente le bouton de reprise et les écrans d’historique sans imposer à la GUI
 * de connaître le support de persistance utilisé.
 */
struct HistoryEntry {
    std::string scanId;
    std::string title;
    int chapter{1};
    int page{1};
    std::string readAt;
};

/**
 * @brief Résultat de recherche texte ou OCR dans les pages indexées.
 *
 * Le résultat donne uniquement les éléments utiles à la navigation : scan, chapitre, page
 * et extrait textuel. La source peut provenir de PostgreSQL, d’un sidecar local ou d’une API.
 */
struct SearchResultSummary {
    std::string scanId;
    std::string title;
    int chapter{1};
    int page{1};
    std::string snippet;
    std::string source;
};

/**
 * @brief Contrat applicatif unique pour accéder à une bibliothèque de scans.
 *
 * Objectif projet :
 * Découpler totalement la fenêtre GTK du stockage réel. Le même écran peut ainsi lire
 * les scans depuis le dossier local historique, depuis le serveur HTTP local ou depuis
 * un cache offline sans dupliquer la logique de navigation.
 *
 * Interagit avec :
 * - la bibliothèque visuelle et le lecteur GTK ;
 * - les implémentations fichier et API ;
 * - les services de synchronisation offline et de recherche.
 */
class ScanDataSource {
public:
    virtual ~ScanDataSource() = default;

    /**
     * @brief Charge la liste normalisée des scans disponibles pour un profil.
     *
     * @param profile Profil dont il faut intégrer la progression et les favoris.
     * @return Scans prêts à être affichés dans la bibliothèque.
     */
    [[nodiscard]] virtual std::vector<ScanSummary> list_scans(const std::string& profile = "default") const = 0;

    /**
     * @brief Liste les chapitres disponibles pour un scan.
     *
     * @param scanId Identifiant stable du scan.
     * @return Numéros de chapitres triés par la source.
     */
    [[nodiscard]] virtual std::vector<int> list_chapters(const std::string& scanId) const = 0;

    /**
     * @brief Liste les pages connues d’un chapitre.
     *
     * @param scanId Identifiant stable du scan.
     * @param chapter Chapitre demandé.
     * @return Pages disponibles avec métadonnées d’image.
     */
    [[nodiscard]] virtual std::vector<ScanPageInfo> list_pages(const std::string& scanId, int chapter) const = 0;

    /**
     * @brief Résout une page en chemin local affichable par GTK.
     *
     * Objectif projet :
     * Masquer la différence entre une image déjà présente sur disque et une image servie
     * par l’API puis mise en cache local.
     *
     * @param scanId Identifiant stable du scan.
     * @param progress Position chapitre/page à matérialiser.
     * @return Chemin local de l’image prête à être chargée.
     */
    [[nodiscard]] virtual std::filesystem::path materialize_page(const std::string& scanId, ScanProgress progress) const = 0;

    /**
     * @brief Charge la progression enregistrée pour un scan et un profil.
     *
     * @param scanId Identifiant stable du scan.
     * @param profile Profil utilisateur concerné.
     * @return Position de reprise normalisée.
     */
    [[nodiscard]] virtual ScanProgress load_progress(const std::string& scanId, const std::string& profile = "default") const = 0;

    /**
     * @brief Enregistre la dernière position connue de lecture.
     *
     * Effet de bord : met à jour le stockage local, l’API ou la base selon l’implémentation.
     *
     * @param scanId Identifiant stable du scan.
     * @param progress Position à persister.
     * @param profile Profil utilisateur concerné.
     */
    virtual void save_progress(const std::string& scanId, ScanProgress progress, const std::string& profile = "default") const = 0;

    /**
     * @brief Liste les profils connus par la source.
     *
     * Les implémentations qui ne gèrent pas les profils renvoient au minimum le profil
     * `default` pour garder l’interface utilisable.
     *
     * @return Profils disponibles.
     */
    [[nodiscard]] virtual std::vector<ProfileSummary> list_profiles() const { return {ProfileSummary{"default", "Default", "#3b82f6"}}; }

    /**
     * @brief Crée ou met à jour un profil utilisateur.
     *
     * L’implémentation par défaut ne fait rien afin de conserver la compatibilité des sources
     * de données minimales.
     *
     * @param profile Profil à persister.
     */
    virtual void save_profile(const ProfileSummary&) const {}

    /**
     * @brief Liste les scans marqués comme favoris pour un profil.
     *
     * @param profile Profil utilisateur concerné.
     * @return Identifiants de scans favoris.
     */
    [[nodiscard]] virtual std::vector<std::string> list_favorites(const std::string&) const { return {}; }

    /**
     * @brief Active ou retire le statut favori d’un scan.
     *
     * @param scanId Identifiant stable du scan.
     * @param favorite true pour marquer comme favori, false pour retirer.
     * @param profile Profil utilisateur concerné.
     */
    virtual void set_favorite(const std::string&, bool, const std::string& = "default") const {}

    /**
     * @brief Liste les marque-pages associés à un scan et un profil.
     *
     * @param scanId Identifiant stable du scan.
     * @param profile Profil utilisateur concerné.
     * @return Marque-pages triés par la source.
     */
    [[nodiscard]] virtual std::vector<BookmarkSummary> list_bookmarks(const std::string&, const std::string& = "default") const { return {}; }

    /**
     * @brief Ajoute un marque-page à la position courante.
     *
     * @param scanId Identifiant stable du scan.
     * @param progress Position chapitre/page du marque-page.
     * @param note Note libre associée.
     * @param profile Profil utilisateur concerné.
     * @return Marque-page créé ou représentation minimale en mode local simplifié.
     */
    virtual BookmarkSummary add_bookmark(const std::string& scanId, ScanProgress progress, const std::string& note, const std::string& profile = "default") const {
        (void)profile;
        return BookmarkSummary{0, scanId, progress.chapter, progress.page, note, ""};
    }

    /**
     * @brief Retourne les dernières positions de lecture d’un profil.
     *
     * @param profile Profil utilisateur concerné.
     * @param limit Nombre maximal d’entrées souhaité.
     * @return Historique exploitable par l’écran de reprise.
     */
    [[nodiscard]] virtual std::vector<HistoryEntry> list_history(const std::string& = "default", int = 20) const { return {}; }

    /**
     * @brief Recherche du texte indexé dans les pages.
     *
     * @param query Terme recherché.
     * @param limit Nombre maximal de résultats.
     * @return Résultats navigables dans le lecteur.
     */
    [[nodiscard]] virtual std::vector<SearchResultSummary> search_text(const std::string&, int = 20) const { return {}; }
};

#endif
