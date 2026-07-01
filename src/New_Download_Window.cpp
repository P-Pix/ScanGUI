/**
 * @file New_Download_Window.cpp
 * @brief Implémente la création d'une nouvelle lecture depuis une URL.
 *
 * La boîte de dialogue prépare le dossier local du scan et initialise les métadonnées JSON
 * nécessaires au flux de téléchargement.
 */

#include "New_Download_Window.hpp"

#include "infrastructure/JsonScanRepository.hpp"
#include "infrastructure/LelScansProvider.hpp"

#include <filesystem>

New_Download_Window::New_Download_Window(Gtk::Window& parent)
    : Gtk::Dialog("Nouveau telechargement", parent, true) {
    set_default_size(520, 130);
    add_button("Annuler", Gtk::RESPONSE_CANCEL);
    add_button("Creer", Gtk::RESPONSE_OK);

    auto* content = get_content_area();
    content->set_spacing(8);
    content->set_margin_top(12);
    content->set_margin_bottom(12);
    content->set_margin_start(12);
    content->set_margin_end(12);

    auto* label = Gtk::make_managed<Gtk::Label>("URL de depart lelscans.net :");
    label->set_halign(Gtk::ALIGN_START);
    content->pack_start(*label, Gtk::PACK_SHRINK);

    entry_.set_placeholder_text("https://lelscans.net/scan-nom-du-scan/1/1");
    content->pack_start(entry_, Gtk::PACK_SHRINK);

    show_all_children();
}

New_Download_Window::~New_Download_Window() = default;

std::string New_Download_Window::get_website() const {
    return entry_.get_text();
}

std::filesystem::path New_Download_Window::get_folder() const {
    return folder_;
}

/**
 * @brief Crée le dossier et les métadonnées initiales d'un nouveau scan.
 *
 * Objectif projet :
 * Transformer l'URL collée par l'utilisateur en structure locale compatible avec le service
 * de mise à jour, sans lancer immédiatement tout le téléchargement.
 */
bool New_Download_Window::create_scan_from_url(const std::string& url) {
    LelScansProvider provider;
    const auto parsed = provider.parse_reader_url(url);
    if (!parsed) {
        return false;
    }

    folder_ = std::filesystem::path("scan") / parsed->folderName;
    std::filesystem::create_directories(folder_ / std::to_string(parsed->progress.chapter));

    ScanMetadata metadata;
    metadata.downloadUrl = parsed->baseUrl;
    metadata.downloadProgress = parsed->progress;
    metadata.saveProgress = parsed->progress;

    JsonScanRepository repository;
    repository.save(folder_, metadata);
    return true;
}
