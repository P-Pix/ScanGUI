#include "Scan.hpp"

Scan::Scan() : folder(""), page(0), chapitre(0), width(0), height(0) {
}

Scan::Scan(int width, int height) : folder(""), page(0), chapitre(0), width(width), height(height) {
}

Scan::Scan(std::string folder, int width, int height) : folder(folder), page(0), chapitre(0), width(width), height(height) {
}

Scan::Scan(std::string folder, int page, int chapitre, int width, int height) : folder(folder), page(page), chapitre(chapitre), width(width), height(height) {
}

Scan::~Scan() {
}

void Scan::set_folder(std::string folder) {
    this->folder = folder;
}

std::string Scan::get_folder() {
    return folder;
}

void Scan::next_page() {
    std::cout << "next_page" << std::endl;
    std::string page_path = folder + "/" + std::to_string(chapitre) + "/" + std::to_string(page + 1) + ".jpg";
    if (std::ifstream(page_path)) {
        page++;
    } else {
        std::string chapitre_path = folder + "/" + std::to_string(chapitre + 1) + "/" + std::to_string(1) + ".jpg";
        if (std::ifstream(chapitre_path)) {
            chapitre++;
            page = 1;
        }
    }
    std::cout << this->get_page() << std::endl;
    this->set_page(this->get_page());
}

void Scan::previous_page() {
    std::cout << "previous_page" << std::endl;
    std::string page_path = folder + "/" + std::to_string(chapitre) + "/" + std::to_string(page - 1) + ".jpg";
    if (page > 1 && std::ifstream(page_path)) {
        page--;
    } else if (chapitre > 1 && std::ifstream(folder + "/" + std::to_string(chapitre - 1) + "/" + std::to_string(1) + ".jpg")) {
        chapitre--;
        page = 1;
    }
    std::cout << this->get_page() << std::endl;
    this->set_page(this->get_page());
}

std::string Scan::get_page() {
    return folder + "/" + std::to_string(chapitre) + "/" + std::to_string(page) + ".jpg";
}

int Scan::get_chapitre() {
    return chapitre;
}

int Scan::get_page_number() {
    return page;
}

void Scan::set_page(std::string page_path) {
    std::cout << "set_page" << std::endl;
    Glib::RefPtr<Gdk::Pixbuf> original_pixbuf = Gdk::Pixbuf::create_from_file(page_path);
    int width = original_pixbuf->get_width();
    int height = original_pixbuf->get_height();
    if (width > this->width) {
        height = height * this->width / width;
        width = this->width;
    } else if (height > this->height) {
        width = width * this->height / height;
        height = this->height;
    }
    Glib::RefPtr<Gdk::Pixbuf> resized_pixbuf = original_pixbuf->scale_simple(width, height, Gdk::INTERP_BILINEAR);
    this->set(resized_pixbuf);
}

void Scan::set_page(std::string folder, int chapitre, int page_number) {
    this->folder = folder;
    this->chapitre = chapitre;
    this->page = page_number;
    this->set_page(this->get_page());
}

void Scan::set_width(int width) {
    this->width = width;
}

void Scan::set_height(int height) {
    this->height = height;
}