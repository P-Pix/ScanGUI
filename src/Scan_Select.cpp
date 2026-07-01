/**
 * @file Scan_Select.cpp
 * @brief Implémente la sélection d'un scan local existant.
 *
 * La boîte de dialogue liste les dossiers de `./scan` et renvoie le choix utilisateur à la
 * fenêtre principale.
 */

#include "Scan_Select.hpp"

#include <algorithm>
#include <vector>

Scan_Select::Scan_Select(Gtk::Window& parent)
    : Gtk::Dialog("Selectionner une lecture", parent, true) {
    set_default_size(360, 120);
    add_button("Annuler", Gtk::RESPONSE_CANCEL);
    add_button("Ouvrir", Gtk::RESPONSE_OK);

    auto* content = get_content_area();
    content->set_spacing(8);
    content->set_margin_top(12);
    content->set_margin_bottom(12);
    content->set_margin_start(12);
    content->set_margin_end(12);

    auto* label = Gtk::make_managed<Gtk::Label>("Choisir un dossier dans ./scan :");
    label->set_halign(Gtk::ALIGN_START);
    content->pack_start(*label, Gtk::PACK_SHRINK);

    std::vector<std::string> folders;
    if (std::filesystem::exists("scan") && std::filesystem::is_directory("scan")) {
        for (const auto& entry : std::filesystem::directory_iterator("scan")) {
            if (entry.is_directory()) {
                folders.push_back(entry.path().filename().string());
            }
        }
    }
    std::sort(folders.begin(), folders.end());

    for (const auto& folder : folders) {
        combo_folders_.append(folder);
    }
    if (!folders.empty()) {
        combo_folders_.set_active(0);
    }

    content->pack_start(combo_folders_, Gtk::PACK_SHRINK);
    show_all_children();
}

Scan_Select::~Scan_Select() = default;

std::string Scan_Select::get_selected_folder() const {
    return combo_folders_.get_active_text();
}
