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
    this->set_page(this->get_page());
}

int extractNumber(const std::string& filepath) {
    size_t lastSlash = filepath.find_last_of('/');
    size_t startPos = lastSlash != std::string::npos ? lastSlash + 1 : 0;
    size_t dotPos = filepath.find('.', startPos);
    size_t i = startPos;
    while (i < dotPos && std::isdigit(filepath[i])) {
        ++i;
    }
    return std::stoi(filepath.substr(startPos, i - startPos));
}

bool customCompare(const std::string& a, const std::string& b) {
    int numA = extractNumber(a);
    int numB = extractNumber(b);
    return numA < numB;
}

void Scan::previous_page() {
    std::string page_path = folder + "/" + std::to_string(chapitre) + "/" + std::to_string(page - 1) + ".jpg";
    if (std::ifstream(page_path)) {
        page--;
    } else {
        std::vector<std::string> file_name_v = std::vector<std::string>();
        if (chapitre == 1) {
            return;
        }
        for (const auto & entry : std::filesystem::directory_iterator(folder + "/" + std::to_string(chapitre - 1))) {
            file_name_v.push_back(entry.path());
        }
        std::sort(file_name_v.begin(), file_name_v.end(), customCompare);
        std::string file_name = file_name_v[file_name_v.size() - 1];
        if (std::ifstream(file_name)) {
            chapitre--;
            page = extractNumber(file_name);
        }
    }
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
    Glib::RefPtr<Gdk::Pixbuf> original_pixbuf = Gdk::Pixbuf::create_from_file(page_path);
    int w = original_pixbuf->get_width();
    int h = original_pixbuf->get_height();
    if (w > this->width) {
        h = h * this->width / w;
        w = this->width;
    } else if (h > this->height) {
        w = w * this->height / h;
        h = this->height;
    }
    Glib::RefPtr<Gdk::Pixbuf> resized_pixbuf = original_pixbuf->scale_simple(w, h, Gdk::INTERP_BILINEAR);
    this->set(resized_pixbuf);
}

void Scan::zoom_in() {
    this->set_width(this->width * 1.1);
    this->set_height(this->height * 1.1);
    this->set_page(this->get_page());
}

void Scan::zoom_out() {
    this->set_width(this->width * 0.9);
    this->set_height(this->height * 0.9);
    this->set_page(this->get_page());
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