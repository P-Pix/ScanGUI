#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <gtkmm.h>
#include <iostream>
#include <string>
#include "ScanSelect.hpp"

class MainWindow : public Gtk::Window {
public:
    MainWindow();
    virtual ~MainWindow();

protected:

private:
    std::string selected_folder;

    Gtk::Box m_VBox;
    Gtk::MenuBar m_MenuBar;

    Gtk::MenuItem m_File;
    Gtk::Menu m_FileMenu;
    Gtk::MenuItem m_FileOpen; // Ouvre une nouvelle fenetre avec juste la liste des scan, avec une différenciation si le scan est déjà download ou pas
    Gtk::MenuItem m_FileSave; // Sauvegarde la page actuelle du scan
    Gtk::MenuItem m_FileQuit;

    void on_menu_file_open();
    void on_menu_file_save();
    void on_menu_file_quit();
};

#endif // !MAINWINDOW_HPP
