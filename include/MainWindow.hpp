#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <json/json.h>
#include "ScanSelect.hpp"
#include "Scan.hpp"
#include "NewDownloadWindow.hpp"
#include "DownloadScan.hpp"

class MainWindow : public Gtk::Window {
public:
    MainWindow();
    virtual ~MainWindow();

protected:
    bool on_image_clic(GdkEventButton* event);
    bool on_key_press(GdkEventKey* event);

private:
    const int WIDTH = Gdk::screen_width();
    const int HEIGHT = Gdk::screen_height() - 100;

    std::string selected_folder;

    Gtk::Box m_VBox;
    Gtk::MenuBar m_MenuBar;

    Gtk::Grid m_Grid;
    std::vector<Gtk::Button*> m_ListButtonManga;
    Gtk::Grid m_GridListeManga;
    Gtk::ScrolledWindow m_ScrollListeManga;
    Gtk::ScrolledWindow m_ScrollImage;

    Gtk::MenuItem m_File;
    Gtk::Menu m_FileMenu;
    Gtk::MenuItem m_FileOpen; // Ouvre une nouvelle fenetre avec juste la liste des scan, avec une différenciation si le scan est déjà download ou pas
    Gtk::MenuItem m_FileSave; // Sauvegarde la page actuelle du scan
    Gtk::MenuItem m_FileQuit;

    Gtk::MenuItem m_Download;
    Gtk::Menu m_DownloadMenu;
    Gtk::MenuItem m_UpdateScan;
    Gtk::MenuItem m_NewScan; // ouvre une fenetre avec une demande d'url pour le scan

    Scan m_Scan;

    Gtk::EventBox m_EventBox;

    void on_menu_file_open();
    void on_menu_file_save();
    void on_menu_file_quit();

    void on_menu_download_update();
    void on_menu_download_new();

    void update_scan(std::string folder);
    void download_picture(std::string folder, std::string url);

    void liste_manga();

    void open_Scan(std::string folder);
};

#endif // !MAINWINDOW_HPP
