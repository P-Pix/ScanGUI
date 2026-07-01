/**
 * @file Main_Window.cpp
 * @brief Fenetre GTK v6: mode local/serveur/offline, bibliotheque visuelle et lecteur avance.
 */
#include "Main_Window.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace {

/** @brief Transforme un identifiant de dossier en libellé lisible dans la bibliothèque. */
std::string readable_scan_name(std::string name) {
    if (name.empty()) return "Sans nom";
    std::replace(name.begin(), name.end(), '-', ' ');
    std::replace(name.begin(), name.end(), '_', ' ');
    bool upperNext = true;
    for (char& c : name) {
        if (std::isspace(static_cast<unsigned char>(c))) upperNext = true;
        else if (upperNext) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); upperNext = false; }
    }
    return name;
}

/** @brief Normalise une chaîne ASCII pour recherche et tri insensibles à la casse. */
std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

/** @brief Valide un champ GTK censé contenir un entier positif. */
std::optional<int> parse_positive_int(const Glib::ustring& value) {
    const std::string text = value.raw();
    if (text.empty()) return std::nullopt;
    if (!std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); })) return std::nullopt;
    try { const int parsed = std::stoi(text); return parsed > 0 ? std::optional<int>(parsed) : std::nullopt; } catch (...) { return std::nullopt; }
}

/** @brief Liste les dossiers locaux de scans pour l'ancien dialogue d'ouverture. */
std::vector<std::string> scan_folders() {
    std::vector<std::string> folders;
    if (!std::filesystem::exists("scan")) std::filesystem::create_directories("scan");
    for (const auto& entry : std::filesystem::directory_iterator("scan")) {
        if (entry.is_directory()) folders.push_back(entry.path().filename().string());
    }
    std::sort(folders.begin(), folders.end());
    return folders;
}

/** @brief Formate le pourcentage de lecture affiché sur les cartes de bibliothèque. */
std::string percent_label(const ScanSummary& scan) {
    const int percent = static_cast<int>(std::round(scan.completion_percent() * 100.0));
    return std::to_string(std::max(0, std::min(100, percent))) + " %";
}

} // namespace

/**
 * @brief Construit l'interface principale et connecte les actions utilisateur.
 *
 * Le constructeur prépare la bibliothèque, le lecteur, les menus, la file de téléchargement et
 * choisit le mode local comme source de données initiale.
 */
Main_Window::Main_Window()
    : m_VBox(Gtk::ORIENTATION_VERTICAL),
      m_LeftPanel(Gtk::ORIENTATION_VERTICAL),
      m_LibraryToolbar(Gtk::ORIENTATION_VERTICAL),
      m_FavoritesOnly("Favoris"),
      m_ButtonRefresh("Actualiser"),
      m_ButtonResume("Reprendre"),
      m_ButtonOfflineSync("Sync offline"),
      m_ButtonCreateProfile("Nouveau profil"),
      m_ButtonHistory("Historique"),
      m_ButtonSearchText("Recherche OCR"),
      m_ButtonBookmarks("Voir marque-pages"),
      m_DownloadStatus("File inactive"),
      m_PageHBox(Gtk::ORIENTATION_HORIZONTAL),
      m_WebtoonBox(Gtk::ORIENTATION_VERTICAL),
      m_Button_go_to("Go to"),
      m_ButtonFavorite("Favori"),
      m_ButtonBookmark("Marque-page"),
      m_ButtonShortcuts("Raccourcis"),
      m_PageSlider(Gtk::ORIENTATION_HORIZONTAL),
      m_ThumbBox(Gtk::ORIENTATION_HORIZONTAL),
      m_File("Fichier"),
      m_FileOpen("Ouvrir"),
      m_FileSave("Sauvegarder"),
      m_FileQuit("Quitter"),
      m_Download("Telechargement"),
      m_UpdateScan("Mettre a jour"),
      m_UpdateAllScan("Tout mettre a jour"),
      m_NewScan("Nouveau"),
      m_OfflineSyncMenu("Telecharger le serveur en offline"),
      m_PauseJobsMenu("Pause file"),
      m_ResumeJobsMenu("Reprendre file"),
      m_CancelJobsMenu("Annuler file"),
      m_View("Affichage"),
      m_DoublePageMenu("Double page"),
      m_WebtoonMenu("Mode webtoon"),
      m_FullscreenMenu("Plein ecran"),
      m_Help("Aide"),
      m_ShortcutsMenu("Raccourcis clavier") {
    std::filesystem::create_directories("scan");
    std::filesystem::create_directories("cache/api");

    set_title("ScanGUI v6 - Local / Serveur / Offline");
    set_default_size(WIDTH_WINDOW, HEIGHT_WINDOW);
    set_position(Gtk::WIN_POS_CENTER);

    downloadQueue_ = std::make_unique<DownloadQueue>([this](const DownloadJobReport& report) {
        Glib::signal_idle().connect([this, report]() {
            if (report.success) {
                m_DownloadProgress.set_fraction(1.0);
                m_DownloadStatus.set_text("Termine : " + report.id);
                refresh_scan_list();
                show_info("Tache terminee", "La tache " + report.id + " est terminee apres " + std::to_string(report.attempts) + " tentative(s).");
            } else if (report.cancelled) {
                m_DownloadStatus.set_text("Annule : " + report.id);
                show_info("Tache annulee", "La tache " + report.id + " a ete annulee proprement.");
            } else {
                m_DownloadStatus.set_text("Erreur : " + report.id);
                show_error("Tache en echec", "La tache " + report.id + " a echoue : " + report.error);
            }
            return false;
        });
    }, [this](const std::string& jobId, int current, int total, const std::string& message) {
        on_download_progress(jobId, current, total, message);
    });
    downloadQueue_->start();

    add_events(Gdk::KEY_PRESS_MASK);
    signal_key_press_event().connect(sigc::mem_fun(*this, &Main_Window::on_key_press));

    m_Scan.set_width(WIDTH);
    m_Scan.set_height(HEIGHT);
    m_SecondScan.set_width(WIDTH / 2);
    m_SecondScan.set_height(HEIGHT);

    m_EventBox.add(m_Scan);
    m_EventBox.add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK | Gdk::SCROLL_MASK);
    m_EventBox.signal_button_press_event().connect(sigc::mem_fun(*this, &Main_Window::on_image_clic));
    m_EventBox.signal_button_release_event().connect(sigc::mem_fun(*this, &Main_Window::on_image_release));
    m_EventBox.signal_motion_notify_event().connect(sigc::mem_fun(*this, &Main_Window::on_image_motion));
    m_EventBox.signal_scroll_event().connect(sigc::mem_fun(*this, &Main_Window::on_image_scroll));
    m_EventBox.set_hexpand(true);
    m_EventBox.set_vexpand(true);

    m_SecondEventBox.add(m_SecondScan);
    m_SecondEventBox.set_no_show_all(true);
    m_SecondEventBox.hide();

    m_PageHBox.pack_start(m_EventBox, Gtk::PACK_EXPAND_WIDGET);
    m_PageHBox.pack_start(m_SecondEventBox, Gtk::PACK_EXPAND_WIDGET);
    m_WebtoonBox.set_spacing(12);
    m_ReaderStack.add(m_PageHBox, "paged");
    m_ReaderStack.add(m_WebtoonBox, "webtoon");
    m_ReaderStack.set_visible_child("paged");

    m_ScrollImage.add(m_ReaderStack);
    m_ScrollImage.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

    m_Entry_page.set_placeholder_text("Page");
    m_Entry_chapter.set_placeholder_text("Chapitre");
    m_Button_go_to.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::go_to));
    m_ButtonFavorite.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::toggle_favorite));
    m_ButtonBookmark.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::add_bookmark));
    m_ButtonShortcuts.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::show_shortcuts));

    m_ChapterProgress.set_show_text(true);
    m_PageSlider.set_range(1, 1);
    m_PageSlider.set_increments(1, 1);
    m_PageSlider.set_digits(0);
    m_PageSlider.signal_value_changed().connect(sigc::mem_fun(*this, &Main_Window::on_page_slider_changed));
    m_ThumbScroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_NEVER);
    m_ThumbScroll.set_size_request(-1, 86);
    m_ThumbScroll.add(m_ThumbBox);

    m_go_to.attach(m_Entry_chapter, 0, 0, 1, 1);
    m_go_to.attach(m_Entry_page, 1, 0, 1, 1);
    m_go_to.attach(m_Button_go_to, 2, 0, 1, 1);
    m_go_to.attach(m_ButtonFavorite, 3, 0, 1, 1);
    m_go_to.attach(m_ButtonBookmark, 4, 0, 1, 1);
    m_go_to.attach(m_ButtonShortcuts, 5, 0, 1, 1);
    m_go_to.attach(m_ChapterProgress, 0, 1, 3, 1);
    m_go_to.attach(m_PageSlider, 3, 1, 3, 1);
    m_go_to.set_column_homogeneous(false);

    m_GridImage_go_to.attach(m_ScrollImage, 0, 0, 1, 1);
    m_GridImage_go_to.attach(m_ThumbScroll, 0, 1, 1, 1);
    m_GridImage_go_to.attach(m_go_to, 0, 2, 1, 1);

    m_ModeCombo.append_text("Local direct");
    m_ModeCombo.append_text("Serveur local");
    m_ModeCombo.append_text("Offline");
    m_ModeCombo.set_active(0);
    m_ModeCombo.signal_changed().connect(sigc::mem_fun(*this, &Main_Window::on_mode_changed));

    m_ServerUrlEntry.set_text("http://127.0.0.1:8787");
    m_ServerUrlEntry.set_placeholder_text("URL serveur");
    m_SearchEntry.set_placeholder_text("Rechercher un scan");
    m_SearchEntry.signal_search_changed().connect(sigc::mem_fun(*this, &Main_Window::refresh_scan_list));
    m_SortCombo.append_text("Titre");
    m_SortCombo.append_text("Derniere lecture");
    m_SortCombo.append_text("Avancement");
    m_SortCombo.append_text("Chapitres");
    m_SortCombo.append_text("Pages");
    m_SortCombo.set_active(0);
    m_SortCombo.signal_changed().connect(sigc::mem_fun(*this, &Main_Window::refresh_scan_list));
    m_ProfileCombo.signal_changed().connect(sigc::mem_fun(*this, &Main_Window::on_profile_changed));
    m_FavoritesOnly.signal_toggled().connect(sigc::mem_fun(*this, &Main_Window::refresh_scan_list));
    m_ButtonRefresh.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::refresh_scan_list));
    m_ButtonResume.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::resume_last_reading));
    m_ButtonOfflineSync.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::synchronize_offline));
    m_ButtonCreateProfile.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::create_profile_dialog));
    m_ButtonHistory.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::show_history));
    m_ButtonSearchText.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::show_text_search));
    m_ButtonBookmarks.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::show_bookmarks));
    m_DownloadProgress.set_show_text(true);
    m_DownloadProgress.set_text("0 %");

    m_LibraryToolbar.set_spacing(6);
    m_LibraryToolbar.pack_start(m_ModeCombo, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_ServerUrlEntry, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_ProfileCombo, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_SearchEntry, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_SortCombo, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_FavoritesOnly, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_ButtonRefresh, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_ButtonResume, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_ButtonOfflineSync, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_ButtonCreateProfile, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_ButtonHistory, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_ButtonSearchText, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_ButtonBookmarks, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_DownloadProgress, Gtk::PACK_SHRINK);
    m_LibraryToolbar.pack_start(m_DownloadStatus, Gtk::PACK_SHRINK);

    m_ScrollListeManga.set_size_request(360, HEIGHT);
    m_ScrollListeManga.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    m_ScrollListeManga.add(m_GridListeManga);
    m_LeftPanel.pack_start(m_LibraryToolbar, Gtk::PACK_SHRINK);
    m_LeftPanel.pack_start(m_ScrollListeManga, Gtk::PACK_EXPAND_WIDGET);

    m_Grid.attach(m_LeftPanel, 0, 0, 1, 1);
    m_Grid.attach(m_GridImage_go_to, 1, 0, 1, 1);
    m_GridImage_go_to.set_hexpand(true);
    m_GridImage_go_to.set_vexpand(true);

    m_MenuBar.append(m_File);
    m_FileMenu.append(m_FileOpen);
    m_FileOpen.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::on_menu_file_open));
    m_FileMenu.append(m_FileSave);
    m_FileSave.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::on_menu_file_save));
    m_FileMenu.append(m_FileQuit);
    m_FileQuit.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::on_menu_file_quit));
    m_File.set_submenu(m_FileMenu);

    m_MenuBar.append(m_Download);
    m_DownloadMenu.append(m_NewScan);
    m_NewScan.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::on_menu_download_new));
    m_DownloadMenu.append(m_UpdateScan);
    m_UpdateScan.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::on_menu_download_update));
    m_DownloadMenu.append(m_UpdateAllScan);
    m_UpdateAllScan.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::update_all_scan));
    m_DownloadMenu.append(m_OfflineSyncMenu);
    m_OfflineSyncMenu.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::synchronize_offline));
    m_DownloadMenu.append(m_PauseJobsMenu);
    m_PauseJobsMenu.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::pause_download_queue));
    m_DownloadMenu.append(m_ResumeJobsMenu);
    m_ResumeJobsMenu.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::resume_download_queue));
    m_DownloadMenu.append(m_CancelJobsMenu);
    m_CancelJobsMenu.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::cancel_download_queue));
    m_Download.set_submenu(m_DownloadMenu);

    m_MenuBar.append(m_View);
    m_ViewMenu.append(m_DoublePageMenu);
    m_DoublePageMenu.signal_toggled().connect([this]() { set_double_page_mode(m_DoublePageMenu.get_active()); });
    m_ViewMenu.append(m_WebtoonMenu);
    m_WebtoonMenu.signal_toggled().connect([this]() { set_webtoon_mode(m_WebtoonMenu.get_active()); });
    m_ViewMenu.append(m_FullscreenMenu);
    m_FullscreenMenu.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::set_fullscreen_mode));
    m_View.set_submenu(m_ViewMenu);

    m_MenuBar.append(m_Help);
    m_HelpMenu.append(m_ShortcutsMenu);
    m_ShortcutsMenu.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::show_shortcuts));
    m_Help.set_submenu(m_HelpMenu);

    m_VBox.pack_start(m_MenuBar, Gtk::PACK_SHRINK);
    m_VBox.pack_start(m_Grid, Gtk::PACK_EXPAND_WIDGET);
    add(m_VBox);

    configure_data_source();
    refresh_progress_placeholders();
    show_all_children();
    m_SecondEventBox.hide();
}

Main_Window::~Main_Window() {
    if (downloadQueue_) downloadQueue_->stop();
}

/**
 * @brief Instancie la source de données correspondant au mode sélectionné.
 *
 * Les modes `Local` et `Offline` utilisent une source fichier avec une racine différente,
 * tandis que le mode `Server` construit un client API à partir de l'URL saisie.
 */
void Main_Window::configure_data_source() {
    const Glib::ustring modeText = m_ModeCombo.get_active_text();
    if (modeText == "Serveur local") {
        mode_ = DataMode::Server;
        dataSource_ = std::make_unique<ApiScanDataSource>(m_ServerUrlEntry.get_text().raw(), "cache/api");
    } else if (modeText == "Offline") {
        mode_ = DataMode::Offline;
        dataSource_ = std::make_unique<FileSystemScanDataSource>("scan");
    } else {
        mode_ = DataMode::Local;
        dataSource_ = std::make_unique<FileSystemScanDataSource>("scan");
    }
    refresh_profile_list();
    refresh_scan_list();
}

ScanDataSource& Main_Window::active_source() { return *dataSource_; }
const ScanDataSource& Main_Window::active_source() const { return *dataSource_; }

/** @brief Recharge profils et bibliothèque après changement de mode de données. */
void Main_Window::on_mode_changed() {
    try { configure_data_source(); } catch (const std::exception& error) { show_error("Source indisponible", error.what()); }
}

/** @brief Applique le profil choisi et recharge les indicateurs dépendants de ce profil. */
void Main_Window::on_profile_changed() {
    const auto text = m_ProfileCombo.get_active_text();
    if (!text.empty()) currentProfile_ = text.raw();
    refresh_scan_list();
}

/**
 * @brief Demande un nom de profil, le normalise et le sauvegarde via la source active.
 */
void Main_Window::create_profile_dialog() {
    Gtk::Dialog dialog("Nouveau profil", *this, true);
    dialog.add_button("Annuler", Gtk::RESPONSE_CANCEL);
    dialog.add_button("Enregistrer", Gtk::RESPONSE_OK);

    Gtk::Grid grid;
    grid.set_row_spacing(8);
    grid.set_column_spacing(8);
    Gtk::Label idLabel("Identifiant");
    Gtk::Entry idEntry;
    idEntry.set_placeholder_text("ex: lecteur2");
    Gtk::Label nameLabel("Nom affiche");
    Gtk::Entry nameEntry;
    nameEntry.set_placeholder_text("ex: Lecteur 2");
    Gtk::Label colorLabel("Couleur");
    Gtk::Entry colorEntry;
    colorEntry.set_text("#3b82f6");
    grid.attach(idLabel, 0, 0, 1, 1);
    grid.attach(idEntry, 1, 0, 1, 1);
    grid.attach(nameLabel, 0, 1, 1, 1);
    grid.attach(nameEntry, 1, 1, 1, 1);
    grid.attach(colorLabel, 0, 2, 1, 1);
    grid.attach(colorEntry, 1, 2, 1, 1);
    dialog.get_content_area()->pack_start(grid);
    dialog.show_all_children();

    if (dialog.run() != Gtk::RESPONSE_OK) return;
    const std::string id = idEntry.get_text().raw();
    if (id.empty()) { show_error("Profil invalide", "L'identifiant du profil est obligatoire."); return; }
    const std::string display = nameEntry.get_text().empty() ? id : nameEntry.get_text().raw();
    try {
        active_source().save_profile(ProfileSummary{id, display, colorEntry.get_text().raw()});
        currentProfile_ = id;
        refresh_profile_list();
        refresh_scan_list();
    } catch (const std::exception& error) {
        show_error("Creation du profil impossible", error.what());
    }
}

/** @brief Reconstruit la combo des profils sans supposer le backend utilisé. */
void Main_Window::refresh_profile_list() {
    m_ProfileCombo.remove_all();
    std::vector<ProfileSummary> profiles;
    try { profiles = active_source().list_profiles(); } catch (...) { profiles = {ProfileSummary{"default", "Default", "#3b82f6"}}; }
    int activeIndex = 0;
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        m_ProfileCombo.append_text(profiles[i].id);
        if (profiles[i].id == currentProfile_) activeIndex = static_cast<int>(i);
    }
    if (profiles.empty()) m_ProfileCombo.append_text("default");
    m_ProfileCombo.set_active(activeIndex);
}

/**
 * @brief Applique recherche, filtre favoris et tri sur la bibliothèque.
 *
 * Les exceptions de la source sont laissées au caller pour affichage centralisé via dialogue.
 */
std::vector<ScanSummary> Main_Window::filtered_sorted_scans() const {
    auto scans = active_source().list_scans(currentProfile_);
    const std::string query = lower_ascii(m_SearchEntry.get_text().raw());
    const bool favoritesOnly = m_FavoritesOnly.get_active();
    scans.erase(std::remove_if(scans.begin(), scans.end(), [&](const ScanSummary& scan) {
        if (favoritesOnly && !scan.favorite) return true;
        if (!query.empty() && lower_ascii(scan.title).find(query) == std::string::npos && lower_ascii(scan.id).find(query) == std::string::npos) return true;
        return false;
    }), scans.end());

    const std::string sort = m_SortCombo.get_active_text().raw();
    if (sort == "Derniere lecture") {
        std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.lastReadAt, a.title) > std::tie(b.lastReadAt, b.title); });
    } else if (sort == "Avancement") {
        std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.progress.chapter, a.progress.page, a.title) > std::tie(b.progress.chapter, b.progress.page, b.title); });
    } else if (sort == "Chapitres") {
        std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.chapterCount, a.title) > std::tie(b.chapterCount, b.title); });
    } else if (sort == "Pages") {
        std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.pageCount, a.title) > std::tie(b.pageCount, b.title); });
    } else {
        std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return a.title < b.title; });
    }
    return scans;
}

/**
 * @brief Reconstruit les cartes de scans affichées dans le panneau gauche.
 *
 * Chaque carte embarque les métadonnées utiles : titre, volume, progression, favori et bouton
 * de reprise à la dernière progression connue.
 */
void Main_Window::refresh_scan_list() {
    for (auto* button : m_ListButtonManga) m_GridListeManga.remove(*button);
    m_ListButtonManga.clear();

    std::vector<ScanSummary> scans;
    try { scans = filtered_sorted_scans(); } catch (const std::exception& error) { show_error("Bibliotheque indisponible", error.what()); return; }

    int row = 0;
    int col = 0;
    constexpr int columns = 2;
    for (const auto& scan : scans) {
        auto* button = Gtk::make_managed<Gtk::Button>();
        auto* card = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
        card->set_spacing(4);
        card->set_margin_top(6);
        card->set_margin_bottom(6);
        card->set_margin_left(6);
        card->set_margin_right(6);

        try {
            auto chapters = active_source().list_chapters(scan.id);
            if (!chapters.empty()) {
                auto pages = active_source().list_pages(scan.id, chapters.front());
                if (!pages.empty()) {
                    const auto coverPath = active_source().materialize_page(scan.id, ScanProgress{pages.front().chapter, pages.front().page});
                    auto pixbuf = Gdk::Pixbuf::create_from_file(coverPath.string(), 120, 160, true);
                    auto* image = Gtk::make_managed<Gtk::Image>(pixbuf);
                    card->pack_start(*image, Gtk::PACK_SHRINK);
                }
            }
        } catch (...) {
            auto* noCover = Gtk::make_managed<Gtk::Label>("Pas de couverture");
            card->pack_start(*noCover, Gtk::PACK_SHRINK);
        }

        std::ostringstream label;
        label << (scan.favorite ? "★ " : "") << scan.title << "\n"
              << scan.chapterCount << " chap. - " << scan.pageCount << " pages\n"
              << "Progression : ch. " << scan.progress.chapter << ", p. " << scan.progress.page << "\n"
              << percent_label(scan);
        if (!scan.lastReadAt.empty()) label << "\nDerniere lecture : " << scan.lastReadAt;
        label << "\nCliquer pour reprendre";

        auto* text = Gtk::make_managed<Gtk::Label>(label.str());
        text->set_line_wrap(true);
        text->set_justify(Gtk::JUSTIFY_CENTER);
        card->pack_start(*text, Gtk::PACK_SHRINK);

        auto* progress = Gtk::make_managed<Gtk::ProgressBar>();
        progress->set_show_text(true);
        const double completion = scan.completion_percent();
        progress->set_fraction(std::clamp(completion, 0.0, 1.0));
        progress->set_text(completion <= 0.0 ? "0 %" : "en cours");
        card->pack_start(*progress, Gtk::PACK_SHRINK);

        button->add(*card);
        button->set_hexpand(true);
        button->set_vexpand(false);
        button->signal_clicked().connect([this, scan]() { open_scan_by_id(scan.id, scan.progress); });
        m_ListButtonManga.push_back(button);
        m_GridListeManga.attach(*button, col, row, 1, 1);
        if (++col >= columns) { col = 0; ++row; }
    }
    m_GridListeManga.show_all();
}

void Main_Window::open_scan(const std::filesystem::path& folder) {
    open_scan_by_id(folder.filename().string(), active_source().load_progress(folder.filename().string(), currentProfile_));
}

/**
 * @brief Ouvre un scan via la source active et prépare le lecteur.
 *
 * La méthode charge chapitres/pages depuis la source, matérialise la page courante puis met à
 * jour miniatures, slider, favoris et progression.
 */
void Main_Window::open_scan_by_id(const std::string& scanId, ScanProgress progress) {
    if (scanId.empty()) return;
    currentScanId_ = scanId;
    currentScanTitle_ = readable_scan_name(scanId);
    currentProgress_ = progress;
    currentProgress_.normalize();
    currentChapters_ = active_source().list_chapters(scanId);
    if (currentChapters_.empty()) throw std::runtime_error("Ce scan ne contient aucun chapitre indexe.");
    if (std::find(currentChapters_.begin(), currentChapters_.end(), currentProgress_.chapter) == currentChapters_.end()) currentProgress_.chapter = currentChapters_.front();
    currentPages_ = active_source().list_pages(scanId, currentProgress_.chapter);
    if (!currentPages_.empty() && std::none_of(currentPages_.begin(), currentPages_.end(), [&](const auto& p) { return p.page == currentProgress_.page; })) currentProgress_.page = currentPages_.front().page;
    render_current_page();
}

/** @brief Navigue vers la position saisie dans les champs chapitre/page. */
void Main_Window::go_to() {
    if (currentScanId_.empty()) { show_error("Aucune lecture ouverte", "Ouvre d'abord une lecture."); return; }
    const int chapter = parse_positive_int(m_Entry_chapter.get_text()).value_or(currentProgress_.chapter);
    const int page = parse_positive_int(m_Entry_page.get_text()).value_or(currentProgress_.page);
    currentProgress_ = {chapter, page};
    render_current_page();
}

int Main_Window::current_page_index() const {
    for (std::size_t i = 0; i < currentPages_.size(); ++i) if (currentPages_[i].page == currentProgress_.page) return static_cast<int>(i);
    return -1;
}

int Main_Window::current_chapter_index() const {
    auto it = std::find(currentChapters_.begin(), currentChapters_.end(), currentProgress_.chapter);
    return it == currentChapters_.end() ? -1 : static_cast<int>(std::distance(currentChapters_.begin(), it));
}

/** @brief Calcule la page suivante en tenant compte du passage de chapitre. */
std::optional<ScanProgress> Main_Window::next_progress() const {
    const int pageIndex = current_page_index();
    if (pageIndex >= 0 && pageIndex + 1 < static_cast<int>(currentPages_.size())) return ScanProgress{currentProgress_.chapter, currentPages_[pageIndex + 1].page};
    const int chapterIndex = current_chapter_index();
    if (chapterIndex >= 0 && chapterIndex + 1 < static_cast<int>(currentChapters_.size())) {
        auto pages = active_source().list_pages(currentScanId_, currentChapters_[chapterIndex + 1]);
        if (!pages.empty()) return ScanProgress{currentChapters_[chapterIndex + 1], pages.front().page};
    }
    return std::nullopt;
}

/** @brief Calcule la page précédente en tenant compte du retour au chapitre précédent. */
std::optional<ScanProgress> Main_Window::previous_progress() const {
    const int pageIndex = current_page_index();
    if (pageIndex > 0) return ScanProgress{currentProgress_.chapter, currentPages_[pageIndex - 1].page};
    const int chapterIndex = current_chapter_index();
    if (chapterIndex > 0) {
        auto pages = active_source().list_pages(currentScanId_, currentChapters_[chapterIndex - 1]);
        if (!pages.empty()) return ScanProgress{currentChapters_[chapterIndex - 1], pages.back().page};
    }
    return std::nullopt;
}

/** @brief Applique la progression suivante et redessine le lecteur. */
void Main_Window::navigate_next() {
    if (auto next = next_progress()) { currentProgress_ = *next; render_current_page(); }
}

/** @brief Applique la progression précédente et redessine le lecteur. */
void Main_Window::navigate_previous() {
    if (auto previous = previous_progress()) { currentProgress_ = *previous; render_current_page(); }
}

/**
 * @brief Affiche la page courante dans le mode de lecture actif.
 *
 * En mode double page, la méthode matérialise aussi la page suivante si elle existe. En mode
 * webtoon, elle délègue au rendu vertical continu.
 */
void Main_Window::render_current_page() {
    if (currentScanId_.empty()) return;
    currentPages_ = active_source().list_pages(currentScanId_, currentProgress_.chapter);
    if (currentPages_.empty()) throw std::runtime_error("Chapitre vide ou introuvable.");
    if (std::none_of(currentPages_.begin(), currentPages_.end(), [&](const auto& p) { return p.page == currentProgress_.page; })) currentProgress_.page = currentPages_.front().page;

    if (webtoonMode_) {
        render_webtoon_chapter();
    } else {
        m_ReaderStack.set_visible_child("paged");
        const auto pagePath = active_source().materialize_page(currentScanId_, currentProgress_);
        m_Scan.set_page(pagePath);
        if (doublePageMode_) {
            if (auto second = next_progress()) {
                try { m_SecondScan.set_page(active_source().materialize_page(currentScanId_, *second)); m_SecondEventBox.show(); }
                catch (...) { m_SecondEventBox.hide(); }
            } else {
                m_SecondEventBox.hide();
            }
        } else {
            m_SecondEventBox.hide();
        }
    }

    save_current_progress();
    rebuild_thumbnails();
    refresh_progress_placeholders();
}

/** @brief Matérialise toutes les pages du chapitre et les place dans une colonne scrollable. */
void Main_Window::render_webtoon_chapter() {
    for (auto* widget : m_WebtoonImages) m_WebtoonBox.remove(*widget);
    m_WebtoonImages.clear();
    for (const auto& page : currentPages_) {
        try {
            auto path = active_source().materialize_page(currentScanId_, ScanProgress{page.chapter, page.page});
            auto* image = Gtk::make_managed<Gtk::Image>(path.string());
            image->set_margin_bottom(12);
            m_WebtoonBox.pack_start(*image, Gtk::PACK_SHRINK);
            m_WebtoonImages.push_back(image);
        } catch (...) {}
    }
    m_ReaderStack.set_visible_child("webtoon");
    m_WebtoonBox.show_all();
}

/** @brief Reconstruit les boutons miniatures permettant l'accès rapide aux pages. */
void Main_Window::rebuild_thumbnails() {
    for (auto* widget : m_ThumbButtons) m_ThumbBox.remove(*widget);
    m_ThumbButtons.clear();
    for (const auto& page : currentPages_) {
        auto* button = Gtk::make_managed<Gtk::Button>();
        auto* box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
        try {
            const auto path = active_source().materialize_page(currentScanId_, ScanProgress{page.chapter, page.page});
            auto pixbuf = Gdk::Pixbuf::create_from_file(path.string(), 56, 74, true);
            auto* image = Gtk::make_managed<Gtk::Image>(pixbuf);
            box->pack_start(*image, Gtk::PACK_SHRINK);
        } catch (...) {}
        auto* label = Gtk::make_managed<Gtk::Label>(std::to_string(page.page));
        box->pack_start(*label, Gtk::PACK_SHRINK);
        button->add(*box);
        button->signal_clicked().connect([this, page]() { currentProgress_ = {page.chapter, page.page}; render_current_page(); });
        m_ThumbBox.pack_start(*button, Gtk::PACK_SHRINK);
        m_ThumbButtons.push_back(button);
    }
    m_ThumbBox.show_all();
}

/** @brief Sauvegarde la position courante et rafraîchit la bibliothèque après navigation. */
void Main_Window::save_current_progress() {
    if (currentScanId_.empty()) return;
    try { active_source().save_progress(currentScanId_, currentProgress_, currentProfile_); } catch (const std::exception& error) { std::cerr << "Sauvegarde impossible: " << error.what() << std::endl; }
}

/** @brief Synchronise champs texte, slider, barre d'avancement et bouton favori. */
void Main_Window::refresh_progress_placeholders() {
    const int pageIndex = std::max(0, current_page_index());
    m_Entry_page.set_placeholder_text("Page actuelle : " + std::to_string(currentProgress_.page) + " / " + std::to_string(static_cast<int>(currentPages_.size())));
    m_Entry_chapter.set_placeholder_text("Chapitre actuel : " + std::to_string(currentProgress_.chapter) + " / " + std::to_string(static_cast<int>(currentChapters_.size())));
    updatingSlider_ = true;
    m_PageSlider.set_range(1, std::max(1, static_cast<int>(currentPages_.size())));
    m_PageSlider.set_value(pageIndex + 1);
    updatingSlider_ = false;
    const double fraction = currentPages_.empty() ? 0.0 : static_cast<double>(pageIndex + 1) / static_cast<double>(currentPages_.size());
    m_ChapterProgress.set_fraction(std::clamp(fraction, 0.0, 1.0));
    m_ChapterProgress.set_text("Chapitre " + std::to_string(currentProgress_.chapter) + " - " + std::to_string(static_cast<int>(std::round(fraction * 100.0))) + " %");
}

/** @brief Convertit le slider en page courante en évitant les boucles de signal GTK. */
void Main_Window::on_page_slider_changed() {
    if (updatingSlider_ || currentPages_.empty()) return;
    int index = static_cast<int>(std::round(m_PageSlider.get_value())) - 1;
    index = std::clamp(index, 0, static_cast<int>(currentPages_.size()) - 1);
    currentProgress_.page = currentPages_[index].page;
    render_current_page();
}

/** @brief Inverse le favori du scan courant et recharge les cartes visibles. */
void Main_Window::toggle_favorite() {
    if (currentScanId_.empty()) return;
    bool currentlyFavorite = false;
    for (const auto& scan : active_source().list_scans(currentProfile_)) if (scan.id == currentScanId_) currentlyFavorite = scan.favorite;
    active_source().set_favorite(currentScanId_, !currentlyFavorite, currentProfile_);
    refresh_scan_list();
}

/** @brief Crée un marque-page sur la page courante avec une note optionnelle. */
void Main_Window::add_bookmark() {
    if (currentScanId_.empty()) return;
    try {
        active_source().add_bookmark(currentScanId_, currentProgress_, "Ajoute depuis GTK", currentProfile_);
        show_info("Marque-page ajoute", "Position : chapitre " + std::to_string(currentProgress_.chapter) + ", page " + std::to_string(currentProgress_.page));
    } catch (const std::exception& error) { show_error("Marque-page impossible", error.what()); }
}

/** @brief Affiche les marque-pages du scan courant et permet de revenir à l'un d'eux. */
void Main_Window::show_bookmarks() {
    if (currentScanId_.empty()) { show_error("Aucun scan ouvert", "Ouvre un scan avant de consulter les marque-pages."); return; }
    try {
        const auto bookmarks = active_source().list_bookmarks(currentScanId_, currentProfile_);
        Gtk::Dialog dialog("Marque-pages", *this, true);
        dialog.add_button("Fermer", Gtk::RESPONSE_CLOSE);
        auto* area = dialog.get_content_area();
        if (bookmarks.empty()) {
            auto* label = Gtk::make_managed<Gtk::Label>("Aucun marque-page pour ce scan.");
            area->pack_start(*label, Gtk::PACK_SHRINK);
        }
        for (const auto& bookmark : bookmarks) {
            std::ostringstream text;
            text << "Chapitre " << bookmark.chapter << ", page " << bookmark.page;
            if (!bookmark.note.empty()) text << " - " << bookmark.note;
            auto* button = Gtk::make_managed<Gtk::Button>(text.str());
            button->signal_clicked().connect([this, &dialog, bookmark]() {
                currentProgress_ = ScanProgress{bookmark.chapter, bookmark.page};
                dialog.response(Gtk::RESPONSE_OK);
            });
            area->pack_start(*button, Gtk::PACK_SHRINK);
        }
        dialog.show_all_children();
        if (dialog.run() == Gtk::RESPONSE_OK) render_current_page();
    } catch (const std::exception& error) { show_error("Marque-pages indisponibles", error.what()); }
}

/** @brief Affiche l'historique du profil actif et ouvre l'entrée choisie. */
void Main_Window::show_history() {
    try {
        const auto history = active_source().list_history(currentProfile_, 20);
        Gtk::Dialog dialog("Historique de lecture", *this, true);
        dialog.add_button("Fermer", Gtk::RESPONSE_CLOSE);
        auto* area = dialog.get_content_area();
        if (history.empty()) {
            auto* label = Gtk::make_managed<Gtk::Label>("Aucun historique pour ce profil.");
            area->pack_start(*label, Gtk::PACK_SHRINK);
        }
        for (const auto& item : history) {
            std::ostringstream text;
            text << item.title << " - ch. " << item.chapter << ", p. " << item.page;
            if (!item.readAt.empty()) text << " - " << item.readAt;
            auto* button = Gtk::make_managed<Gtk::Button>(text.str());
            button->signal_clicked().connect([this, &dialog, item]() {
                open_scan_by_id(item.scanId, ScanProgress{item.chapter, item.page});
                dialog.response(Gtk::RESPONSE_OK);
            });
            area->pack_start(*button, Gtk::PACK_SHRINK);
        }
        dialog.show_all_children();
        (void)dialog.run();
    } catch (const std::exception& error) { show_error("Historique indisponible", error.what()); }
}

/** @brief Demande une recherche texte et ouvre le résultat choisi dans le lecteur. */
void Main_Window::show_text_search() {
    Gtk::Dialog input("Recherche OCR / texte", *this, true);
    input.add_button("Annuler", Gtk::RESPONSE_CANCEL);
    input.add_button("Rechercher", Gtk::RESPONSE_OK);
    Gtk::Entry queryEntry;
    queryEntry.set_placeholder_text("Mot ou expression a chercher dans les pages OCR");
    queryEntry.set_text(m_SearchEntry.get_text());
    input.get_content_area()->pack_start(queryEntry);
    input.show_all_children();
    if (input.run() != Gtk::RESPONSE_OK) return;
    const std::string query = queryEntry.get_text().raw();
    if (query.empty()) return;

    try {
        const auto results = active_source().search_text(query, 30);
        Gtk::Dialog dialog("Resultats OCR", *this, true);
        dialog.add_button("Fermer", Gtk::RESPONSE_CLOSE);
        auto* area = dialog.get_content_area();
        if (results.empty()) {
            auto* label = Gtk::make_managed<Gtk::Label>("Aucun resultat indexe. Lance /api/admin/ocr ou ajoute des fichiers .txt sidecar.");
            label->set_line_wrap(true);
            area->pack_start(*label, Gtk::PACK_SHRINK);
        }
        for (const auto& result : results) {
            std::ostringstream text;
            text << result.title << " - ch. " << result.chapter << ", p. " << result.page << "\n" << result.snippet;
            auto* button = Gtk::make_managed<Gtk::Button>(text.str());
            button->signal_clicked().connect([this, &dialog, result]() {
                open_scan_by_id(result.scanId, ScanProgress{result.chapter, result.page});
                dialog.response(Gtk::RESPONSE_OK);
            });
            area->pack_start(*button, Gtk::PACK_SHRINK);
        }
        dialog.show_all_children();
        (void)dialog.run();
    } catch (const std::exception& error) { show_error("Recherche impossible", error.what()); }
}

/** @brief Met à jour les indicateurs GTK à partir des callbacks `DownloadQueue`. */
void Main_Window::on_download_progress(const std::string& jobId, int current, int total, const std::string& message) {
    Glib::signal_idle().connect([this, jobId, current, total, message]() {
        if (total > 0) {
            const double fraction = std::clamp(static_cast<double>(current) / static_cast<double>(total), 0.0, 1.0);
            m_DownloadProgress.set_fraction(fraction);
            m_DownloadProgress.set_text(std::to_string(static_cast<int>(std::round(fraction * 100.0))) + " %");
        } else {
            m_DownloadProgress.pulse();
            m_DownloadProgress.set_text("en cours");
        }
        m_DownloadStatus.set_text(jobId + " : " + message);
        return false;
    });
}

/** @brief Rouvre la première entrée d'historique, si disponible. */
void Main_Window::resume_last_reading() {
    try {
        const auto history = active_source().list_history(currentProfile_, 1);
        if (!history.empty()) { open_scan_by_id(history.front().scanId, ScanProgress{history.front().chapter, history.front().page}); return; }
        const auto scans = active_source().list_scans(currentProfile_);
        if (!scans.empty()) open_scan_by_id(scans.front().id, scans.front().progress);
    } catch (const std::exception& error) { show_error("Reprise impossible", error.what()); }
}

/**
 * @brief Ajoute la synchronisation offline complète à la file de travaux.
 *
 * Le travail exécute `OfflineLibrarySync` hors de la boucle GTK pour éviter de figer l'interface
 * pendant le téléchargement ou la copie des pages.
 */
void Main_Window::synchronize_offline() {
    const std::string baseUrl = m_ServerUrlEntry.get_text().raw();
    const std::string profile = currentProfile_;
    downloadQueue_->push(DownloadJob{"offline-sync", [baseUrl, profile](DownloadJobContext& ctx) {
        ApiScanDataSource api(baseUrl, "cache/api");
        OfflineLibrarySync sync(api, "scan");
        auto report = sync.sync_all(profile, [&](const OfflineSyncProgress& progress) {
            ctx.report(progress.downloadedPages + progress.skippedPages + progress.failedPages, 0,
                       "scan=" + progress.scanId + " ch=" + std::to_string(progress.chapter) + " p=" + std::to_string(progress.page));
            ctx.wait_if_paused();
            if (ctx.cancelled()) throw std::runtime_error("offline sync cancelled");
        });
        ctx.report(report.downloadedPages, report.downloadedPages + report.skippedPages + report.failedPages, "synchronisation offline terminee");
    }, 1});
    show_info("Synchronisation lancee", "Le telechargement offline est place dans la file de taches. Les fichiers seront copies dans ./scan.");
}

/** @brief Mappe les raccourcis clavier vers navigation, affichage et actions de lecture. */
bool Main_Window::on_key_press(GdkEventKey* event) {
    if (event->state & GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_o) on_menu_file_open();
        else if (event->keyval == GDK_KEY_s) on_menu_file_save();
        else if (event->keyval == GDK_KEY_q) on_menu_file_quit();
        else if (event->keyval == GDK_KEY_d) on_menu_download_update();
        else if (event->keyval == GDK_KEY_n) on_menu_download_new();
        else if (event->keyval == GDK_KEY_f) toggle_favorite();
        else if (event->keyval == GDK_KEY_b) add_bookmark();
        return true;
    }
    if (event->keyval == GDK_KEY_Right || event->keyval == GDK_KEY_space) navigate_next();
    else if (event->keyval == GDK_KEY_Left) navigate_previous();
    else if (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_KP_Add) zoom_in();
    else if (event->keyval == GDK_KEY_minus || event->keyval == GDK_KEY_KP_Subtract) zoom_out();
    else if (event->keyval == GDK_KEY_F11) set_fullscreen_mode();
    else if (event->keyval == GDK_KEY_F1) show_shortcuts();
    return true;
}

/** @brief Démarre le drag de l'image zoomée ou déclenche la navigation au clic. */
bool Main_Window::on_image_clic(GdkEventButton* event) {
    if (event->type != GDK_BUTTON_PRESS) return false;
    if (event->button == 2) {
        dragging_ = true;
        dragStartX_ = event->x_root;
        dragStartY_ = event->y_root;
        hadjStart_ = m_ScrollImage.get_hadjustment()->get_value();
        vadjStart_ = m_ScrollImage.get_vadjustment()->get_value();
        return true;
    }
    if (event->button == 1) navigate_next();
    else if (event->button == 3) navigate_previous();
    return true;
}

bool Main_Window::on_image_release(GdkEventButton* event) {
    if (event->button == 2) dragging_ = false;
    return false;
}

/** @brief Déplace les ajustements du scroll pendant un drag souris. */
bool Main_Window::on_image_motion(GdkEventMotion* event) {
    if (!dragging_) return false;
    auto hadj = m_ScrollImage.get_hadjustment();
    auto vadj = m_ScrollImage.get_vadjustment();
    hadj->set_value(std::clamp(hadjStart_ - (event->x_root - dragStartX_), hadj->get_lower(), hadj->get_upper()));
    vadj->set_value(std::clamp(vadjStart_ - (event->y_root - dragStartY_), vadj->get_lower(), vadj->get_upper()));
    return true;
}

/** @brief Associe la molette souris au zoom du lecteur. */
bool Main_Window::on_image_scroll(GdkEventScroll* event) {
    if (!(event->state & GDK_CONTROL_MASK)) return false;
    if (event->direction == GDK_SCROLL_UP) zoom_in();
    else if (event->direction == GDK_SCROLL_DOWN) zoom_out();
    return true;
}

void Main_Window::zoom_in() { m_Scan.zoom_in(); m_SecondScan.zoom_in(); }
void Main_Window::zoom_out() { m_Scan.zoom_out(); m_SecondScan.zoom_out(); }

/** @brief Bascule l'état plein écran de la fenêtre. */
void Main_Window::set_fullscreen_mode() {
    fullscreenMode_ = !fullscreenMode_;
    if (fullscreenMode_) fullscreen(); else unfullscreen();
}

/** @brief Active ou désactive l'affichage double page. */
void Main_Window::set_double_page_mode(bool enabled) {
    doublePageMode_ = enabled;
    if (doublePageMode_ && webtoonMode_) { webtoonMode_ = false; m_WebtoonMenu.set_active(false); }
    if (!currentScanId_.empty()) render_current_page();
}

/** @brief Active ou désactive le défilement vertical continu. */
void Main_Window::set_webtoon_mode(bool enabled) {
    webtoonMode_ = enabled;
    if (webtoonMode_ && doublePageMode_) { doublePageMode_ = false; m_DoublePageMenu.set_active(false); }
    if (!currentScanId_.empty()) render_current_page();
}

/** @brief Affiche la documentation intégrée des raccourcis de lecture. */
void Main_Window::show_shortcuts() {
    show_info("Raccourcis", "Droite / Espace : page suivante\nGauche : page precedente\nCtrl+F : favori\nCtrl+B : marque-page\nCtrl+molette : zoom\nClic molette glisse : deplacer l'image zoomee\nF11 : plein ecran\nF1 : aide");
}

void Main_Window::on_menu_file_open() {
    if (mode_ == DataMode::Server) {
        refresh_scan_list();
        show_info("Mode serveur", "Choisis un scan depuis la bibliotheque visuelle alimentee par /api/scans.");
        return;
    }
    Scan_Select scan_select(*this);
    if (scan_select.run() != Gtk::RESPONSE_OK) return;
    const auto folder = scan_select.get_selected_folder();
    if (!folder.empty()) open_scan_by_id(folder, active_source().load_progress(folder, currentProfile_));
}

void Main_Window::on_menu_file_save() { save_current_progress(); }
void Main_Window::on_menu_file_quit() { hide(); }

/** @brief Planifie la mise à jour du scan courant en arrière-plan. */
void Main_Window::on_menu_download_update() {
    Scan_Select scan_select(*this);
    if (scan_select.run() != Gtk::RESPONSE_OK) return;
    const auto folder = scan_select.get_selected_folder();
    if (!folder.empty()) update_scan(std::filesystem::path("scan") / folder);
}

/** @brief Ouvre le dialogue de création d'un scan depuis une URL source. */
void Main_Window::on_menu_download_new() {
    New_Download_Window new_download_window(*this);
    if (new_download_window.run() != Gtk::RESPONSE_OK) return;
    if (!new_download_window.create_scan_from_url(new_download_window.get_website())) {
        show_error("URL invalide", "Format attendu : https://lelscans.net/scan-nom-du-scan/1/1");
        return;
    }
    refresh_scan_list();
    update_scan(new_download_window.get_folder());
}

/** @brief Ajoute tous les scans locaux connus à la file de mise à jour. */
void Main_Window::update_all_scan() {
    downloadQueue_->push(DownloadJob{"update-all", [](DownloadJobContext& ctx) {
        const auto folders = scan_folders();
        int index = 0;
        for (const auto& folder : folders) {
            ctx.wait_if_paused();
            if (ctx.cancelled()) throw std::runtime_error("update-all cancelled");
            ctx.report(++index, static_cast<int>(folders.size()), "mise a jour " + folder);
            JsonScanRepository repository;
            CurlHttpClient httpClient;
            LelScansProvider provider;
            ScanUpdater updater(repository, httpClient, provider);
            (void)updater.update(std::filesystem::path("scan") / folder);
        }
    }, 1});
}


/** @brief Suspend les travaux d'arrière-plan compatibles avec la pause coopérative. */
void Main_Window::pause_download_queue() {
    if (!downloadQueue_) return;
    downloadQueue_->pause();
    show_info("File en pause", "Les nouvelles taches restent dans la file et reprendront sur demande.");
}

/** @brief Relance les travaux d'arrière-plan après une pause. */
void Main_Window::resume_download_queue() {
    if (!downloadQueue_) return;
    downloadQueue_->resume();
    show_info("File reprise", "Les telechargements et synchronisations peuvent reprendre.");
}

/** @brief Annule les travaux en attente et signale l'annulation au travail courant. */
void Main_Window::cancel_download_queue() {
    if (!downloadQueue_) return;
    downloadQueue_->cancel();
    show_info("File annulee", "Les taches en attente sont supprimees. La tache en cours peut se terminer selon son callback.");
}

/** @brief Planifie la mise à jour incrémentale d'un dossier de scan local. */
void Main_Window::update_scan(const std::filesystem::path& folder) {
    try {
        ScanUpdater updater(repository_, httpClient_, provider_);
        const auto report = updater.update(folder);
        show_info("Mise a jour terminee", "Pages telechargees : " + std::to_string(report.downloadedPages) + "\nPages deja presentes : " + std::to_string(report.skippedExistingPages) + "\n" + report.message);
        refresh_scan_list();
    } catch (const std::exception& error) { show_error("Mise a jour impossible", error.what()); }
}

/** @brief Affiche une erreur maîtrisée à l'utilisateur sans quitter l'application. */
void Main_Window::show_error(const std::string& title, const std::string& message) {
    Gtk::MessageDialog dialog(*this, title, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
    dialog.set_secondary_text(message);
    dialog.run();
}

/** @brief Affiche une information utilisateur courte dans une boîte GTK. */
void Main_Window::show_info(const std::string& title, const std::string& message) {
    Gtk::MessageDialog dialog(*this, title, false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
    dialog.set_secondary_text(message);
    dialog.run();
}
