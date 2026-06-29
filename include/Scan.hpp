/**
 * @file Scan.hpp
 * @brief Déclare le widget GTK chargé d'afficher et de naviguer dans une session de scan.
 *
 * Le widget conserve l'interface attendue par l'ancienne fenêtre principale tout en délégant
 * la navigation entre pages et chapitres au moteur métier `ScanSession`.
 */

#ifndef SCAN_HPP
#define SCAN_HPP

#include "domain/ScanProgress.hpp"
#include "domain/ScanSession.hpp"

#include <gtkmm.h>

#include <filesystem>
#include <string>

/**
 * @brief Widget d'image spécialisé pour l'affichage d'une session de scan.
 *
 * Objectif projet :
 * Conserver une intégration simple avec GTK tout en déléguant la navigation réelle à
 * `ScanSession`, ce qui réduit le couplage entre rendu graphique et logique métier.
 */
class Scan : public Gtk::Image {
public:
    Scan();
    Scan(int width, int height);
    Scan(std::string folder, int width, int height);
    Scan(std::string folder, int page, int chapitre, int width, int height);
    virtual ~Scan();

    void set_folder(const std::string& folder);
    [[nodiscard]] std::string get_folder() const;

    [[nodiscard]] bool next_page();
    [[nodiscard]] bool previous_page();

    [[nodiscard]] std::string get_page() const;
    [[nodiscard]] bool set_page(const std::string& folder, int chapitre, int page_number);
    [[nodiscard]] bool set_page(const std::filesystem::path& page_path);

    void zoom_in();
    void zoom_out();

    [[nodiscard]] int get_chapitre() const;
    [[nodiscard]] int get_page_number() const;
    [[nodiscard]] ScanProgress get_progress() const;

    [[nodiscard]] int get_max_chapter() const;
    [[nodiscard]] int get_max_page() const;

    void set_width(int width);
    void set_height(int height);

private:
    ScanSession session_;
    int width_{0};
    int height_{0};

    [[nodiscard]] bool display_current_page();
};

#endif
