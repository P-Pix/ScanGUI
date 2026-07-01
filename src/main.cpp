/**
 * @file main.cpp
 * @brief Point d'entrée de l'application GTK ScanGUI.
 *
 * Ce fichier initialise l'application graphique, crée la fenêtre principale et charge
 * l'icône si elle est disponible sans bloquer le démarrage en cas d'absence.
 */

#include "Main_Window.hpp"

#include <gtkmm/application.h>

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        auto app = Gtk::Application::create(argc, argv, "fr.scangui.reader");
        Main_Window window;

        try {
            auto icon = Gdk::Pixbuf::create_from_file("./asset/icon2.png");
            window.set_icon(icon);
        } catch (const Glib::Error& error) {
            std::cerr << "Icone non chargee: " << error.what() << std::endl;
        }

        return app->run(window);
    } catch (const std::exception& error) {
        std::cerr << "Erreur fatale: " << error.what() << std::endl;
        return 1;
    }
}
