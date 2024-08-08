#include "ScanSelect.hpp"

ScanSelect::ScanSelect()
    : button_validate("Valider") {
    set_title("Sélectionnez un dossier");
    set_default_size(300, 100);

    // Charger les dossiers dans la liste déroulante
    for (const auto& entry : fs::directory_iterator("scan")) {
        if (entry.is_directory()) {
            combo_folders.append(entry.path().filename().string());
        }
    }

    combo_folders.signal_changed().connect(sigc::mem_fun(*this, &ScanSelect::on_folder_selected));
    button_validate.signal_clicked().connect(sigc::mem_fun(*this, &ScanSelect::on_button_clicked));

    // Layout
    Gtk::Box *vbox = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL, 5);
    vbox->set_margin_top(10);
    vbox->set_margin_bottom(10);
    vbox->set_margin_start(10);
    vbox->set_margin_end(10);

    vbox->pack_start(combo_folders);
    vbox->pack_start(button_validate, Gtk::PACK_SHRINK);

    add(*vbox);
    show_all_children();
}

ScanSelect::~ScanSelect() {
}

void ScanSelect::on_folder_selected() {
    selected_folder = combo_folders.get_active_text();
}

void ScanSelect::on_button_clicked() {
    if (!selected_folder.empty()) {
        std::cout << "Dossier sélectionné : " << selected_folder << std::endl;
        hide(); // Ferme la fenêtre après validation
    }
}