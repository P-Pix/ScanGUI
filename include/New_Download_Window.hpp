/**
 * @file New_Download_Window.hpp
 * @brief Déclare la boîte de dialogue GTK de création d'une nouvelle lecture.
 *
 * Cette fenêtre récupère une URL utilisateur, prépare le dossier local associé et écrit les
 * métadonnées initiales nécessaires aux mises à jour futures.
 */

#ifndef NEW_DOWNLOAD_WINDOW_HPP
#define NEW_DOWNLOAD_WINDOW_HPP

#include <gtkmm.h>

#include <filesystem>
#include <string>

/**
 * @brief Dialogue GTK de création d'un scan depuis une URL source.
 *
 * Objectif projet :
 * Récupérer l'URL saisie, valider la source supportée et préparer les métadonnées locales
 * nécessaires au téléchargement incrémental.
 */
class New_Download_Window : public Gtk::Dialog {
public:
    explicit New_Download_Window(Gtk::Window& parent);
    virtual ~New_Download_Window();

    [[nodiscard]] std::string get_website() const;
    [[nodiscard]] std::filesystem::path get_folder() const;
    [[nodiscard]] bool create_scan_from_url(const std::string& url);

private:
    Gtk::Entry entry_;
    std::filesystem::path folder_;
};

#endif
