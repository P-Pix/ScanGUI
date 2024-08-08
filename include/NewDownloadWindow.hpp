#ifndef NewDownloadWindowWINDOW_HPP
#define NewDownloadWindowWINDOW_HPP

#include <gtkmm.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>

class NewDownloadWindow : public Gtk::Window {
public:
    NewDownloadWindow();
    virtual ~NewDownloadWindow();

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


#endif // !NewDownloadWindowWINDOW_HPP