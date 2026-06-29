/**
 * @file Window_Profil.hpp
 * @brief Déclare la fenêtre GTK historique de saisie de profil.
 *
 * Cette fenêtre représente l'amorce de gestion de profil utilisateur prévue dans l'ancien
 * projet et conserve les widgets nécessaires à la saisie d'un nom.
 */

#pragma once

#include <iostream>
#include <string>
#include <gtkmm.h>

#include "Profil.hpp"

/**
 * @brief Fenêtre GTK historique dédiée à la saisie d'un profil.
 *
 * Elle regroupe les widgets de saisie et le modèle `Profil` sans encore porter de logique de
 * persistance avancée.
 */
class Window_Profil : public Gtk::Window {
    public:
        Window_Profil();
        virtual ~Window_Profil();

    protected:
        bool on_key_press(GdkEventKey* event);

    private:
        Gtk::Box m_VBox;
        Gtk::MenuBar m_MenuBar;

        Gtk::Grid m_Grid;
        Gtk::Label m_Label_name;
        Gtk::Entry m_Entry_name;
        Gtk::Button m_Button_save;

        Profil m_Profil;
};