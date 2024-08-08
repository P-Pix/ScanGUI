#ifndef SCANSELECT_HPP
#define SCANSELECT_HPP

#include <gtkmm.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

namespace fs = std::filesystem;

/**
 * @brief Classe permettant d'ouvrir une nouvelle fenetre avec la liste des dossier dans le dossier scan
 * La fenetre est composer d'une liste déroulante avec la possibilité de selectionner un dossier et d'un bouton pour valider la selection
 * Une fois fait la fenetre se ferme et le scan s'ouvre dans la MainWindow
 */
class ScanSelect : public Gtk::Window {
public:
    ScanSelect();
    virtual ~ScanSelect();

    std::string get_selected_folder() const { return selected_folder; }
    
protected:
    // Signal handlers:
    void on_folder_selected();
    void on_button_clicked();

    // Member widgets:
    Gtk::ComboBoxText combo_folders;
    Gtk::Button button_validate;

    // Variables:
    std::string selected_folder;
};

#endif // !SCANSELECT_HPP