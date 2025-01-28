#pragma once

#include <iostream>
#include <string>
#include <gtkmm.h>

#include "Profil.hpp"

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