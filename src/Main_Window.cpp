/**
 * @file Main_Window.cpp
 * @brief Implémente l'écran principal GTK du lecteur.
 *
 * Cette fenêtre coordonne les widgets, les callbacks utilisateur, la navigation de lecture,
 * la sauvegarde de progression et les actions de mise à jour de scans.
 */

#include "Main_Window.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

/**
 * @brief Convertit un nom de dossier de scan en libellé lisible pour la liste GTK.
 *
 * Objectif projet :
 * Rendre la bibliothèque plus agréable sans modifier les identifiants de dossiers utilisés
 * par le stockage local.
 */
std::string readable_scan_name(std::string name) {
    if (name.empty()) {
        return "Sans nom";
    }

    std::replace(name.begin(), name.end(), '-', ' ');
    bool upperNext = true;
    for (char& c : name) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            upperNext = true;
            continue;
        }
        if (upperNext) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            upperNext = false;
        }
    }

    constexpr std::size_t lineLength = 18;
    for (std::size_t pos = lineLength; pos < name.size(); pos += lineLength) {
        const auto split = name.rfind(' ', pos);
        if (split != std::string::npos && name[split] == ' ') {
            name[split] = '\n';
        }
    }
    return name;
}

/**
 * @brief Valide une saisie utilisateur attendue comme entier positif.
 *
 * Objectif projet :
 * Éviter qu'un champ page ou chapitre invalide provoque une exception lors de la navigation
 * directe.
 */
std::optional<int> parse_positive_int(const Glib::ustring& value) {
    const std::string text = value.raw();
    if (text.empty()) {
        return std::nullopt;
    }
    if (!std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); })) {
        return std::nullopt;
    }
    try {
        int parsed = std::stoi(text);
        if (parsed < 1) {
            return std::nullopt;
        }
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * @brief Liste les dossiers de scans disponibles pour l'interface.
 *
 * La création automatique de `scan` permet au logiciel de démarrer proprement même sur une
 * première installation sans bibliothèque existante.
 */
std::vector<std::string> scan_folders() {
    std::vector<std::string> folders;
    if (!std::filesystem::exists("scan")) {
        std::filesystem::create_directories("scan");
    }
    for (const auto& entry : std::filesystem::directory_iterator("scan")) {
        if (entry.is_directory()) {
            folders.push_back(entry.path().filename().string());
        }
    }
    std::sort(folders.begin(), folders.end());
    return folders;
}

} // namespace

Main_Window::Main_Window()
    : m_VBox(Gtk::ORIENTATION_VERTICAL),
      m_File("File"),
      m_FileOpen("Open"),
      m_FileSave("Save"),
      m_FileQuit("Quit"),
      m_Download("Download"),
      m_UpdateScan("Update"),
      m_UpdateAllScan("Update all"),
      m_NewScan("New") {
    std::filesystem::create_directories("scan");

    set_title("ScanGUI - Offline Reader");
    set_default_size(WIDTH_WINDOW, HEIGHT_WINDOW);
    set_position(Gtk::WIN_POS_CENTER);

    add_events(Gdk::KEY_PRESS_MASK);
    signal_key_press_event().connect(sigc::mem_fun(*this, &Main_Window::on_key_press));

    m_Scan.set_width(WIDTH);
    m_Scan.set_height(HEIGHT);

    m_EventBox.add(m_Scan);
    m_EventBox.signal_button_press_event().connect(sigc::mem_fun(*this, &Main_Window::on_image_clic));
    m_EventBox.set_hexpand(true);
    m_EventBox.set_vexpand(true);

    m_ScrollImage.add(m_EventBox);
    m_ScrollImage.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

    m_Entry_page.set_placeholder_text("Page");
    m_Entry_chapter.set_placeholder_text("Chapitre");
    m_Button_go_to.set_label("Go to");
    m_Button_go_to.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::go_to));

    m_go_to.attach(m_Entry_page, 0, 0, 1, 1);
    m_go_to.attach(m_Entry_chapter, 1, 0, 1, 1);
    m_go_to.attach(m_Button_go_to, 2, 0, 1, 1);
    m_go_to.set_column_homogeneous(true);

    m_GridImage_go_to.attach(m_ScrollImage, 0, 0, 1, 1);
    m_GridImage_go_to.attach(m_go_to, 0, 1, 1, 1);

    refresh_scan_list();

    m_Grid.attach(m_ScrollListeManga, 0, 0, 1, 1);
    m_Grid.attach(m_GridImage_go_to, 1, 0, 1, 1);
    m_Grid.set_column_homogeneous(false);
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
    m_Download.set_submenu(m_DownloadMenu);

    m_VBox.pack_start(m_MenuBar, Gtk::PACK_SHRINK);
    m_VBox.pack_start(m_Grid, Gtk::PACK_EXPAND_WIDGET);
    add(m_VBox);

    refresh_progress_placeholders();
    show_all_children();
}

Main_Window::~Main_Window() = default;

/**
 * @brief Reconstruit la liste latérale des lectures disponibles.
 *
 * Objectif projet :
 * Synchroniser l'interface avec le contenu courant du dossier `scan`, notamment après la
 * création ou la mise à jour d'une lecture.
 */
void Main_Window::refresh_scan_list() {
    for (auto* button : m_ListButtonManga) {
        m_GridListeManga.remove(*button);
    }
    m_ListButtonManga.clear();

    int row = 0;
    for (const auto& folder : scan_folders()) {
        auto* button = Gtk::make_managed<Gtk::Button>(readable_scan_name(folder));
        button->set_hexpand(true);
        button->signal_clicked().connect([this, folder]() {
            open_scan(std::filesystem::path("scan") / folder);
        });
        m_ListButtonManga.push_back(button);
        m_GridListeManga.attach(*button, 0, row++, 1, 1);
    }

    m_ScrollListeManga.set_size_request(180, HEIGHT);
    m_ScrollListeManga.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    if (m_ScrollListeManga.get_child() == nullptr) {
        m_ScrollListeManga.add(m_GridListeManga);
    }
    m_GridListeManga.show_all();
}

/**
 * @brief Ouvre directement le chapitre et la page saisis par l'utilisateur.
 *
 * Objectif projet :
 * Fournir un accès rapide à une position précise tout en sauvegardant la progression si la
 * page existe réellement.
 */
void Main_Window::go_to() {
    if (selected_folder_.empty()) {
        show_error("Aucune lecture ouverte", "Ouvre d'abord une lecture avant d'aller a une page precise.");
        return;
    }

    const int page = parse_positive_int(m_Entry_page.get_text()).value_or(m_Scan.get_page_number());
    const int chapter = parse_positive_int(m_Entry_chapter.get_text()).value_or(m_Scan.get_chapitre());

    if (!m_Scan.set_page(selected_folder_.string(), chapter, page)) {
        show_error("Page introuvable", "Impossible d'ouvrir le chapitre " + std::to_string(chapter) + ", page " + std::to_string(page) + ".");
        return;
    }

    save_current_progress();
    refresh_progress_placeholders();
}

void Main_Window::update_all_scan() {
    try {
        for (const auto& folder : scan_folders()) {
            update_scan(std::filesystem::path("scan") / folder);
        }
        refresh_scan_list();
    } catch (const std::exception& error) {
        show_error("Mise a jour impossible", error.what());
    }
}

bool Main_Window::on_key_press(GdkEventKey* event) {
    if (event->state & GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_o) {
            on_menu_file_open();
        } else if (event->keyval == GDK_KEY_s) {
            on_menu_file_save();
        } else if (event->keyval == GDK_KEY_q) {
            on_menu_file_quit();
        } else if (event->keyval == GDK_KEY_d) {
            on_menu_download_update();
        } else if (event->keyval == GDK_KEY_n) {
            on_menu_download_new();
        }
        return true;
    }

    if (event->keyval == GDK_KEY_Right) {
        navigate_next();
    } else if (event->keyval == GDK_KEY_Left) {
        navigate_previous();
    } else if (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_KP_Add) {
        m_Scan.zoom_in();
    } else if (event->keyval == GDK_KEY_minus || event->keyval == GDK_KEY_KP_Subtract) {
        m_Scan.zoom_out();
    }
    return true;
}

void Main_Window::on_menu_file_open() {
    Scan_Select scan_select(*this);
    if (scan_select.run() != Gtk::RESPONSE_OK) {
        return;
    }

    const auto folder = scan_select.get_selected_folder();
    if (folder.empty()) {
        return;
    }
    open_scan(std::filesystem::path("scan") / folder);
}

void Main_Window::open_scan(const std::filesystem::path& folder) {
    selected_folder_ = folder;

    ScanProgress progress{1, 1};
    try {
        if (auto metadata = repository_.load(folder)) {
            progress = metadata->saveProgress;
        }
    } catch (const std::exception& error) {
        show_error("Sauvegarde illisible", error.what());
    }

    if (!m_Scan.set_page(folder.string(), progress.chapter, progress.page)) {
        m_Scan.set_page(folder.string(), 1, 1);
    }
    refresh_progress_placeholders();
}

void Main_Window::on_menu_file_save() {
    save_current_progress();
}

void Main_Window::on_menu_file_quit() {
    hide();
}

bool Main_Window::on_image_clic(GdkEventButton* event) {
    if (event->type != GDK_BUTTON_PRESS) {
        return false;
    }

    m_Scan.set_width(WIDTH);
    m_Scan.set_height(HEIGHT);
    if (event->button == 1) {
        navigate_next();
    } else if (event->button == 3) {
        navigate_previous();
    }
    return true;
}

void Main_Window::on_menu_download_update() {
    Scan_Select scan_select(*this);
    if (scan_select.run() != Gtk::RESPONSE_OK) {
        return;
    }

    const auto folder = scan_select.get_selected_folder();
    if (!folder.empty()) {
        update_scan(std::filesystem::path("scan") / folder);
    }
}

void Main_Window::on_menu_download_new() {
    New_Download_Window new_download_window(*this);
    if (new_download_window.run() != Gtk::RESPONSE_OK) {
        return;
    }

    if (!new_download_window.create_scan_from_url(new_download_window.get_website())) {
        show_error("URL invalide", "Format attendu : https://lelscans.net/scan-nom-du-scan/1/1");
        return;
    }

    refresh_scan_list();
    update_scan(new_download_window.get_folder());
}

void Main_Window::update_scan(const std::filesystem::path& folder) {
    try {
        ScanUpdater updater(repository_, httpClient_, provider_);
        const auto report = updater.update(folder);
        show_info(
            "Mise a jour terminee",
            "Pages telechargees : " + std::to_string(report.downloadedPages) +
            "\nPages deja presentes : " + std::to_string(report.skippedExistingPages) +
            "\n" + report.message
        );
    } catch (const std::exception& error) {
        show_error("Mise a jour impossible", error.what());
    }
}

void Main_Window::navigate_next() {
    if (selected_folder_.empty()) {
        return;
    }
    m_Scan.next_page();
    save_current_progress();
    refresh_progress_placeholders();
}

void Main_Window::navigate_previous() {
    if (selected_folder_.empty()) {
        return;
    }
    m_Scan.previous_page();
    save_current_progress();
    refresh_progress_placeholders();
}

/**
 * @brief Sauvegarde la position courante de la lecture ouverte.
 *
 * Objectif projet :
 * Permettre la reprise automatique à la prochaine ouverture du même scan sans action
 * supplémentaire de l'utilisateur.
 */
void Main_Window::save_current_progress() {
    if (selected_folder_.empty()) {
        return;
    }
    try {
        repository_.save_progress(selected_folder_, m_Scan.get_progress());
    } catch (const std::exception& error) {
        show_error("Sauvegarde impossible", error.what());
    }
}

void Main_Window::refresh_progress_placeholders() {
    m_Entry_page.set_placeholder_text(
        "Page actuelle : " + std::to_string(m_Scan.get_page_number()) +
        " / " + std::to_string(m_Scan.get_max_page())
    );
    m_Entry_chapter.set_placeholder_text(
        "Chapitre actuel : " + std::to_string(m_Scan.get_chapitre()) +
        " / " + std::to_string(m_Scan.get_max_chapter())
    );
}

void Main_Window::show_error(const std::string& title, const std::string& message) {
    Gtk::MessageDialog dialog(*this, title, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
    dialog.set_secondary_text(message);
    dialog.run();
}

void Main_Window::show_info(const std::string& title, const std::string& message) {
    Gtk::MessageDialog dialog(*this, title, false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
    dialog.set_secondary_text(message);
    dialog.run();
}
