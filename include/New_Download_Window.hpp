#ifndef NEW_DOWNLOAD_WINDOW_HPP
#define NEW_DOWNLOAD_WINDOW_HPP

#include <gtkmm.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>

class New_Download_Window : public Gtk::Window {
public:
    New_Download_Window();
    virtual ~New_Download_Window();

    std::string get_website() const { return m_Entry.get_text(); }

    std::string get_folder() const { return folder; }

protected:
    void on_button_clicked();

private:
    Gtk::Box m_VBox;
    Gtk::Entry m_Entry;
    Gtk::Button m_Button;

    std::string folder;
};


#endif // !NEW_DOWNLOAD_WINDOW_HPP