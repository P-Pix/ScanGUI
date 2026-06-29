/**
 * @file ScanPage.hpp
 * @brief Représente une page de scan résolue par le moteur de navigation.
 *
 * Une page associe le chemin image effectivement affichable et la progression métier
 * correspondante, ce qui évite à l'interface de recalculer l'état courant.
 */

#ifndef SCANGUI_DOMAIN_SCAN_PAGE_HPP
#define SCANGUI_DOMAIN_SCAN_PAGE_HPP

#include "domain/ScanProgress.hpp"

#include <filesystem>

/**
 * @brief Page résolue par une session de lecture.
 *
 * La structure associe le fichier image trouvé sur disque et la progression à sauvegarder
 * après affichage.
 */
struct ScanPage {
    std::filesystem::path path;
    ScanProgress progress{};
};

#endif
