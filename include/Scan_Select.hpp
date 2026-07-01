/**
 * @file Scan_Select.hpp
 * @brief Déclare la boîte de dialogue GTK de sélection d'une lecture existante.
 *
 * La fenêtre liste les dossiers présents dans `./scan` afin de conserver le mode d'ouverture
 * direct de la version desktop initiale.
 */

#ifndef SCAN_SELECT_HPP
#define SCAN_SELECT_HPP

#include <gtkmm.h>

#include <filesystem>
#include <string>

/**
 * @brief Dialogue GTK de choix d'un dossier de scan existant.
 *
 * Ce composant conserve le parcours utilisateur historique d'ouverture depuis `./scan`.
 */
class Scan_Select : public Gtk::Dialog {
public:
    explicit Scan_Select(Gtk::Window& parent);
    virtual ~Scan_Select();

    [[nodiscard]] std::string get_selected_folder() const;

private:
    Gtk::ComboBoxText combo_folders_;
};

#endif
