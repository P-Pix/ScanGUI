/**
 * @file Main_Window.hpp
 * @brief Déclare la fenêtre principale GTK du lecteur de scans.
 *
 * Cette fenêtre porte les interactions utilisateur historiques : sélection d'une lecture,
 * affichage de page, navigation clavier/souris, sauvegarde de progression et lancement des
 * actions de téléchargement.
 */

#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include "Scan.hpp"
#include "New_Download_Window.hpp"
#include "Scan_Select.hpp"
#include "application/ScanUpdater.hpp"
#include "infrastructure/CurlHttpClient.hpp"
#include "infrastructure/JsonScanRepository.hpp"
#include "infrastructure/LelScansProvider.hpp"

#include <gtkmm.h>

#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief Fenêtre principale du lecteur desktop GTK.
 *
 * Objectif projet :
 * Regrouper les interactions de lecture : ouverture d'un scan, navigation, affichage de la
 * page courante, sauvegarde de progression et déclenchement des mises à jour.
 *
 * Interagit avec :
 * - le widget `Scan` ;
 * - les dialogues GTK d'ouverture et de téléchargement ;
 * - les services de persistance et de téléchargement.
 */
class Main_Window : public Gtk::Window {
public:
    Main_Window();
    virtual ~Main_Window();

protected:
    bool on_image_clic(GdkEventButton* event);
    bool on_key_press(GdkEventKey* event);

private:
    const int WIDTH = Gdk::screen_width();
    const int HEIGHT = Gdk::screen_height() - 125;
    const int WIDTH_WINDOW = Gdk::screen_width();
    const int HEIGHT_WINDOW = Gdk::screen_height() - 100;

    std::filesystem::path selected_folder_;

    Gtk::Box m_VBox;
    Gtk::MenuBar m_MenuBar;

    Gtk::Grid m_Grid;
    std::vector<Gtk::Button*> m_ListButtonManga;
    Gtk::Grid m_GridListeManga;
    Gtk::ScrolledWindow m_ScrollListeManga;
    Gtk::Grid m_GridImage_go_to;
    Gtk::ScrolledWindow m_ScrollImage;

    Gtk::Grid m_go_to;
    Gtk::Entry m_Entry_page;
    Gtk::Entry m_Entry_chapter;
    Gtk::Button m_Button_go_to;

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

    Scan m_Scan;
    Gtk::EventBox m_EventBox;

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

    void refresh_scan_list();
    void open_scan(const std::filesystem::path& folder);
    void go_to();

    void navigate_next();
    void navigate_previous();
    void save_current_progress();
    void refresh_progress_placeholders();
    void show_error(const std::string& title, const std::string& message);
    void show_info(const std::string& title, const std::string& message);
};

#endif
