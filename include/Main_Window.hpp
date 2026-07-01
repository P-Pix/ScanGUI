/**
 * @file Main_Window.hpp
 * @brief Déclare la fenêtre principale GTK : bibliothèque, lecteur, serveur local et offline.
 *
 * La fenêtre orchestre le choix de source de données, la bibliothèque visuelle, le lecteur, les profils,
 * les favoris, les marque-pages et les opérations longues planifiées en arrière-plan.
 */
#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include "Scan.hpp"
#include "New_Download_Window.hpp"
#include "Scan_Select.hpp"
#include "application/ApiScanDataSource.hpp"
#include "application/DownloadQueue.hpp"
#include "application/FileSystemScanDataSource.hpp"
#include "application/OfflineLibrarySync.hpp"
#include "application/ScanUpdater.hpp"
#include "infrastructure/CurlHttpClient.hpp"
#include "infrastructure/JsonScanRepository.hpp"
#include "infrastructure/LelScansProvider.hpp"

#include <gtkmm.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Mode de source de données sélectionné par l’utilisateur dans l’interface.
 *
 * `Local` lit directement `./scan`, `Server` consomme l’API HTTP locale et `Offline` relit
 * les copies synchronisées localement pour une utilisation sans réseau.
 */
enum class DataMode {
    Local,
    Server,
    Offline
};

/**
 * @brief Fenêtre principale GTK du lecteur ScanGUI.
 *
 * Objectif projet :
 * Regrouper la bibliothèque visuelle, la sélection du mode de données, le lecteur d’images,
 * la progression, les favoris, marque-pages, historique et synchronisations offline.
 *
 * Interagit avec :
 * - `ScanDataSource` pour la lecture locale/API/offline ;
 * - `DownloadQueue` pour les traitements longs ;
 * - `Scan` et `ScanSession` pour le rendu et la navigation ;
 * - les services d’infrastructure HTTP et JSON historiques.
 */
class Main_Window : public Gtk::Window {
public:
    Main_Window();
    virtual ~Main_Window();

protected:
    /**
     * @brief Démarre la navigation souris ou le déplacement de l’image zoomée.
     *
     * @param event Événement GTK bouton souris.
     * @return true si l’événement est consommé.
     */
    bool on_image_clic(GdkEventButton* event);
    bool on_image_release(GdkEventButton* event);
    bool on_image_motion(GdkEventMotion* event);
    /**
     * @brief Convertit la molette en zoom ou navigation selon le contexte lecteur.
     *
     * @param event Événement GTK de défilement.
     * @return true si l’événement est consommé.
     */
    bool on_image_scroll(GdkEventScroll* event);
    /**
     * @brief Traite les raccourcis clavier globaux de lecture.
     *
     * @param event Événement clavier GTK.
     * @return true si un raccourci a été appliqué.
     */
    bool on_key_press(GdkEventKey* event);

private:
    const int WIDTH = Gdk::screen_width();
    const int HEIGHT = Gdk::screen_height() - 125;
    const int WIDTH_WINDOW = Gdk::screen_width();
    const int HEIGHT_WINDOW = Gdk::screen_height() - 100;

    DataMode mode_{DataMode::Local};
    std::unique_ptr<ScanDataSource> dataSource_;
    std::unique_ptr<DownloadQueue> downloadQueue_;

    std::string currentProfile_{"default"};
    std::string currentScanId_;
    std::string currentScanTitle_;
    ScanProgress currentProgress_{1, 1};
    std::vector<ScanPageInfo> currentPages_;
    std::vector<int> currentChapters_;
    bool updatingSlider_{false};
    bool doublePageMode_{false};
    bool webtoonMode_{false};
    bool fullscreenMode_{false};
    bool dragging_{false};
    double dragStartX_{0};
    double dragStartY_{0};
    double hadjStart_{0};
    double vadjStart_{0};

    Gtk::Box m_VBox;
    Gtk::MenuBar m_MenuBar;

    Gtk::Grid m_Grid;
    Gtk::Box m_LeftPanel;
    Gtk::Box m_LibraryToolbar;
    Gtk::ComboBoxText m_ModeCombo;
    Gtk::ComboBoxText m_ProfileCombo;
    Gtk::Entry m_ServerUrlEntry;
    Gtk::SearchEntry m_SearchEntry;
    Gtk::ComboBoxText m_SortCombo;
    Gtk::CheckButton m_FavoritesOnly;
    Gtk::Button m_ButtonRefresh;
    Gtk::Button m_ButtonResume;
    Gtk::Button m_ButtonOfflineSync;
    Gtk::Button m_ButtonCreateProfile;
    Gtk::Button m_ButtonHistory;
    Gtk::Button m_ButtonSearchText;
    Gtk::Button m_ButtonBookmarks;
    Gtk::ProgressBar m_DownloadProgress;
    Gtk::Label m_DownloadStatus;

    std::vector<Gtk::Button*> m_ListButtonManga;
    Gtk::Grid m_GridListeManga;
    Gtk::ScrolledWindow m_ScrollListeManga;

    Gtk::Grid m_GridImage_go_to;
    Gtk::ScrolledWindow m_ScrollImage;
    Gtk::Stack m_ReaderStack;
    Gtk::Box m_PageHBox;
    Gtk::Box m_WebtoonBox;
    std::vector<Gtk::Widget*> m_WebtoonImages;

    Gtk::Grid m_go_to;
    Gtk::Entry m_Entry_page;
    Gtk::Entry m_Entry_chapter;
    Gtk::Button m_Button_go_to;
    Gtk::Button m_ButtonFavorite;
    Gtk::Button m_ButtonBookmark;
    Gtk::Button m_ButtonShortcuts;
    Gtk::ProgressBar m_ChapterProgress;
    Gtk::Scale m_PageSlider;
    Gtk::ScrolledWindow m_ThumbScroll;
    Gtk::Box m_ThumbBox;
    std::vector<Gtk::Widget*> m_ThumbButtons;

    Gtk::MenuItem m_File;
    Gtk::Menu m_FileMenu;
    Gtk::MenuItem m_FileOpen;
    Gtk::MenuItem m_FileSave;
    Gtk::MenuItem m_FileQuit;

    Gtk::MenuItem m_Download;
    Gtk::Menu m_DownloadMenu;
    Gtk::MenuItem m_UpdateScan;
    Gtk::MenuItem m_UpdateAllScan;
    Gtk::MenuItem m_NewScan;
    Gtk::MenuItem m_OfflineSyncMenu;
    Gtk::MenuItem m_PauseJobsMenu;
    Gtk::MenuItem m_ResumeJobsMenu;
    Gtk::MenuItem m_CancelJobsMenu;

    Gtk::MenuItem m_View;
    Gtk::Menu m_ViewMenu;
    Gtk::CheckMenuItem m_DoublePageMenu;
    Gtk::CheckMenuItem m_WebtoonMenu;
    Gtk::MenuItem m_FullscreenMenu;

    Gtk::MenuItem m_Help;
    Gtk::Menu m_HelpMenu;
    Gtk::MenuItem m_ShortcutsMenu;

    Scan m_Scan;
    Scan m_SecondScan;
    Gtk::EventBox m_EventBox;
    Gtk::EventBox m_SecondEventBox;

    JsonScanRepository repository_;
    CurlHttpClient httpClient_;
    LelScansProvider provider_;

    void on_menu_file_open();
    void on_menu_file_save();
    void on_menu_file_quit();
    void on_menu_download_update();
    void on_menu_download_new();
    void update_all_scan();
    void update_scan(const std::filesystem::path& folder);
    void pause_download_queue();
    void resume_download_queue();
    void cancel_download_queue();

    /**
     * @brief Reconstruit la source de données selon le mode sélectionné.
     *
     * Cette méthode est le point de bascule entre lecture directe, API locale et cache offline.
     */
    void configure_data_source();
    [[nodiscard]] ScanDataSource& active_source();
    [[nodiscard]] const ScanDataSource& active_source() const;
    void on_mode_changed();
    void on_profile_changed();
    void create_profile_dialog();
    void refresh_profile_list();
    /**
     * @brief Recharge et redessine la bibliothèque visuelle.
     *
     * Elle applique recherche, tri, favoris et progression du profil courant.
     */
    void refresh_scan_list();
    /**
     * @brief Applique les filtres et tris utilisateur sur la bibliothèque.
     *
     * @return Scans prêts à afficher dans la grille GTK.
     */
    [[nodiscard]] std::vector<ScanSummary> filtered_sorted_scans() const;
    void open_scan(const std::filesystem::path& folder);
    /**
     * @brief Ouvre un scan depuis la source active à une position donnée.
     *
     * @param scanId Identifiant du scan.
     * @param progress Position initiale de lecture.
     */
    void open_scan_by_id(const std::string& scanId, ScanProgress progress = {1, 1});
    void go_to();

    void navigate_next();
    void navigate_previous();
    [[nodiscard]] std::optional<ScanProgress> next_progress() const;
    [[nodiscard]] std::optional<ScanProgress> previous_progress() const;
    [[nodiscard]] int current_page_index() const;
    [[nodiscard]] int current_chapter_index() const;
    /**
     * @brief Persiste la position courante dans la source active.
     *
     * Effet de bord : met à jour le fichier local ou l’API serveur selon le mode.
     */
    void save_current_progress();
    /**
     * @brief Matérialise puis affiche la page courante.
     *
     * Cette méthode centralise la mise à jour de l’image principale et de la deuxième page
     * lorsque le mode double page est actif.
     */
    void render_current_page();
    /** @brief Reconstruit l’affichage vertical continu du chapitre courant. */
    void render_webtoon_chapter();
    /** @brief Reconstruit la bande de miniatures du chapitre courant. */
    void rebuild_thumbnails();
    void refresh_progress_placeholders();
    void on_page_slider_changed();
    /** @brief Bascule le favori du scan courant pour le profil actif. */
    void toggle_favorite();
    /** @brief Crée un marque-page à la position courante. */
    void add_bookmark();
    void show_bookmarks();
    /** @brief Affiche l’historique de lecture du profil courant. */
    void show_history();
    /** @brief Lance une recherche texte/OCR via la source active. */
    void show_text_search();
    void resume_last_reading();
    /** @brief Programme la synchronisation offline dans la file de téléchargement. */
    void synchronize_offline();
    /**
     * @brief Reçoit la progression d’un job de fond et met à jour les widgets GTK.
     *
     * @param jobId Identifiant du job.
     * @param current Étape courante.
     * @param total Nombre total d’étapes si connu.
     * @param message Message utilisateur court.
     */
    void on_download_progress(const std::string& jobId, int current, int total, const std::string& message);
    void show_shortcuts();
    void set_fullscreen_mode();
    void set_double_page_mode(bool enabled);
    void set_webtoon_mode(bool enabled);
    void zoom_in();
    void zoom_out();

    void show_error(const std::string& title, const std::string& message);
    void show_info(const std::string& title, const std::string& message);
};

#endif
