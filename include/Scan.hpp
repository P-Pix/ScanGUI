#ifndef SCAN_HPP
#define SCAN_HPP

#include <string>
#include <fstream>
#include <gtkmm.h>
#include <iostream>
#include <filesystem>
#include <vector>

class Scan : public Gtk::Image {
public:
    Scan();
    Scan(int width, int height);
    Scan(std::string folder, int width, int height);
    Scan(std::string folder, int page, int chapitre, int width, int height);
    virtual ~Scan();

    void set_folder(std::string folder);
    std::string get_folder();

    void next_page();
    void previous_page();

    std::string get_page();
    void set_page(std::string folder, int chapitre, int page_number);
    void set_page(std::string page_path);

    void zoom_in();
    void zoom_out();

    int get_chapitre();
    int get_page_number();

    void set_width(int width);
    void set_height(int height);

protected:

private:
    std::string folder;
    int page;
    int chapitre;

    int width;
    int height;
};

#endif // !SCAN_HPP