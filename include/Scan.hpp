#ifndef SCAN_HPP
#define SCAN_HPP

#include <string>
#include <fstream>
#include <gtkmm.h>
#include <iostream>
#include <filesystem>
#include <vector>

/**
 * @brief La class Scan est une class hérité de Gtk::Image.
 * @brief Elle sert a représenter la page actuelle du scan
 */
class Scan : public Gtk::Image {
public:

    /**
     * @brief Création d'un Scan vide
     * @return Scan("", 1, 1, 0, 0)
     */
    Scan();

    /**
     * @brief Création d'un Scan avec la taille de la fenetre
     * @param width largeur de l'image
     * @param height hauteur de l'image
     * @return Scan("", 1, 1, width, height)
     */
    Scan(int width, int height);

    /**
     * @brief Création d'un Scan avec la taille de la fenetre et le nom du manga selectionné
     * @param folder nom du manga selectionné
     * @param width largeur de l'image
     * @param height hauteur de l'image
     * @return Scan(folder, 1, 1, width, height)
     */
    Scan(std::string folder, int width, int height);

    /**
     * @brief Création d'un Scan avec la taille de la fenetre et le nom du manga selectionné ainsi qui le chapitre et la page voulu
     * @param folder nom du manga selectionné
     * @param page page selectionné
     * @param chapitre chapitre selectionné
     * @param width largeur de l'image
     * @param height hauteur de l'image
     * @return Scan(folder, 1, 1, width, height)
     */
    Scan(std::string folder, int page, int chapitre, int width, int height);

    /**
     * @brief Destructeur de base de GTK::Image aucun ajout de ma part
     */
    virtual ~Scan();

    void set_folder(std::string folder);
    std::string get_folder();

    void next_page();
    void previous_page();

    std::string get_page();
    void set_page(std::string folder, int chapitre, int page_number);
    void set_page(std::string page_path);

    void zoom_in();
    void zoom_out();

    int get_chapitre();
    int get_page_number();

    int get_max_chapter();
    int get_max_page();

    void set_width(int width);
    void set_height(int height);

protected:

private:
    std::string folder;
    int page;
    int chapitre;

    int width;
    int height;
};

#endif // !SCAN_HPP