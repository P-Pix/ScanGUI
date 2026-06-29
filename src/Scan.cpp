/**
 * @file Scan.cpp
 * @brief Implémente le widget d'affichage d'une page de scan.
 *
 * Le widget charge les images, applique le redimensionnement et délègue la résolution des
 * pages au moteur `ScanSession`.
 */

#include "Scan.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

Scan::Scan() = default;

Scan::Scan(int width, int height) : width_(width), height_(height) {
}

Scan::Scan(std::string folder, int width, int height) : session_(std::move(folder)), width_(width), height_(height) {
    display_current_page();
}

Scan::Scan(std::string folder, int page, int chapitre, int width, int height)
    : session_(std::move(folder), ScanProgress{chapitre, page}), width_(width), height_(height) {
    display_current_page();
}

Scan::~Scan() = default;

void Scan::set_folder(const std::string& folder) {
    session_.open(folder, session_.progress());
}

std::string Scan::get_folder() const {
    return session_.root().string();
}

bool Scan::next_page() {
    const auto page = session_.next_page();
    if (!page) {
        return false;
    }
    return set_page(page->path);
}

bool Scan::previous_page() {
    const auto page = session_.previous_page();
    if (!page) {
        return false;
    }
    return set_page(page->path);
}

std::string Scan::get_page() const {
    return session_.current_page_path().string();
}

bool Scan::set_page(const std::string& folder, int chapitre, int page_number) {
    ScanProgress progress{chapitre, page_number};
    progress.normalize();
    session_.open(folder, progress);

    if (!session_.current_page()) {
        auto fallback = ScanProgress{1, 1};
        session_.open(folder, fallback);
    }

    return display_current_page();
}

/**
 * @brief Charge et affiche une image depuis un chemin local.
 *
 * Objectif projet :
 * Centraliser le redimensionnement et la gestion d'erreur image pour éviter que la fenêtre
 * principale manipule directement `Gdk::Pixbuf`.
 */
bool Scan::set_page(const std::filesystem::path& page_path) {
    if (page_path.empty() || !std::filesystem::exists(page_path)) {
        clear();
        return false;
    }

    try {
        Glib::RefPtr<Gdk::Pixbuf> original_pixbuf = Gdk::Pixbuf::create_from_file(page_path.string());
        int targetWidth = original_pixbuf->get_width();
        int targetHeight = original_pixbuf->get_height();

        if (width_ > 0 && height_ > 0 && targetWidth > 0 && targetHeight > 0) {
            const double scale = std::min({
                static_cast<double>(width_) / static_cast<double>(targetWidth),
                static_cast<double>(height_) / static_cast<double>(targetHeight),
                1.0
            });
            targetWidth = std::max(1, static_cast<int>(std::lround(targetWidth * scale)));
            targetHeight = std::max(1, static_cast<int>(std::lround(targetHeight * scale)));
        }

        Glib::RefPtr<Gdk::Pixbuf> resized_pixbuf = original_pixbuf->scale_simple(targetWidth, targetHeight, Gdk::INTERP_HYPER);
        set(resized_pixbuf);
        return true;
    } catch (const Glib::Error& error) {
        std::cerr << "Impossible d'afficher l'image " << page_path << ": " << error.what() << std::endl;
        clear();
        return false;
    } catch (const std::exception& error) {
        std::cerr << "Impossible d'afficher l'image " << page_path << ": " << error.what() << std::endl;
        clear();
        return false;
    }
}

void Scan::zoom_in() {
    width_ = std::max(1, static_cast<int>(std::lround(width_ * 1.1)));
    height_ = std::max(1, static_cast<int>(std::lround(height_ * 1.1)));
    display_current_page();
}

void Scan::zoom_out() {
    width_ = std::max(1, static_cast<int>(std::lround(width_ * 0.9)));
    height_ = std::max(1, static_cast<int>(std::lround(height_ * 0.9)));
    display_current_page();
}

int Scan::get_chapitre() const {
    return session_.progress().chapter;
}

int Scan::get_page_number() const {
    return session_.progress().page;
}

ScanProgress Scan::get_progress() const {
    return session_.progress();
}

int Scan::get_max_chapter() const {
    return session_.max_chapter();
}

int Scan::get_max_page() const {
    return session_.max_page_in_current_chapter();
}

void Scan::set_width(int width) {
    width_ = std::max(1, width);
}

void Scan::set_height(int height) {
    height_ = std::max(1, height);
}

/**
 * @brief Rafraîchit l'image affichée à partir de la session courante.
 *
 * @return true si une page a pu être chargée, false si la session ne résout aucune image.
 */
bool Scan::display_current_page() {
    const auto page = session_.current_page();
    if (!page) {
        clear();
        return false;
    }
    return set_page(page->path);
}
