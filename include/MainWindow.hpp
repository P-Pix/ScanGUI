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
    const int HEIGHT = Gdk::screen_height() - 125;

    const int WIDTH_WINDOW = Gdk::screen_width();
    const int HEIGHT_WINDOW = Gdk::screen_height() - 100;

    std::string selected_folder;

    Gtk::Box m_VBox;
    Gtk::MenuBar m_MenuBar;

    Gtk::Grid m_Grid; // Grid pour la liste des scan
    std::vector<Gtk::Button*> m_ListButtonManga; // Liste des boutons pour les manga
    Gtk::Grid m_GridListeManga; // Grid pour la liste des manga
    Gtk::ScrolledWindow m_ScrollListeManga; // ScrolledWindow pour la liste des manga
    Gtk::Grid m_GridImage_go_to; // Grid pour l'image du manga et le go to
    Gtk::ScrolledWindow m_ScrollImage; // ScrolledWindow pour l'image du manga
    // Texte pour saisir la page et le chapitre si on veut aller directement à une page composé de 2 Gtk::Entry 2 affichage de texte et un bouton pour valider
    Gtk::Grid m_go_to;
    Gtk::Entry m_Entry_page;
    Gtk::Entry m_Entry_chapter;
    Gtk::Button m_Button_go_to;

    Gtk::MenuItem m_File;
    Gtk::Menu m_FileMenu;
    Gtk::MenuItem m_FileOpen; // Ouvre une nouvelle fenetre avec juste la liste des scan, avec une différenciation si le scan est déjà download ou pas
    Gtk::MenuItem m_FileSave; // Sauvegarde la page actuelle du scan
    Gtk::MenuItem m_FileQuit;

    Gtk::MenuItem m_Download;
    Gtk::Menu m_DownloadMenu;
    Gtk::MenuItem m_UpdateScan;
    Gtk::MenuItem m_UpdateAllScan;
    Gtk::MenuItem m_NewScan; // ouvre une fenetre avec une demande d'url pour le scan

    Scan m_Scan;
    Json::Value m_current_scan;

    Gtk::EventBox m_EventBox;

    /**
     * Appuyer sur le bouton open de file dans le menu
     */
    void on_menu_file_open();

    /**
     * Appuyer sur le bouton save de file dans le menu
     */
    void on_menu_file_save();

    /**
     * Appuyer sur le bouton quit de file dans le menu
     */
    void on_menu_file_quit();

    /**
     * Appuyer sur le bouton upadte dans le download
     */
    void on_menu_download_update();

    /**
     * Appuyer sur le bouton update all dans le download
     */
    void update_all_scan();

    /**
     * Appuyer sur le bouton new dans le download
     */
    void on_menu_download_new();

    /**
     * Appuyer sur le bouton upadte dans la fenetre de mise à jour avec le nom d'un manga
     */
    void update_scan(std::string folder);

    /**
     * Fonction pour optimiser l'affichage de la liste des manga dans le menu
     */
    void liste_manga();

    /**
     * Fonction quand on clic sur un nom de manga dans la liste de manga
     */
    void open_Scan(std::string folder);

    std::string new_chapter(std::string folder, std::string url);
};

#endif // !MAINWINDOW_HPP
