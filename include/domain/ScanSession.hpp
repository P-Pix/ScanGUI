/**
 * @file ScanSession.hpp
 * @brief Déclare le moteur de navigation indépendant de l'interface graphique.
 *
 * Cette classe centralise la logique de passage entre pages et chapitres dans une
 * arborescence locale de scans. Elle peut être testée ou réutilisée sans dépendre de GTK.
 */

#ifndef SCANGUI_DOMAIN_SCAN_SESSION_HPP
#define SCANGUI_DOMAIN_SCAN_SESSION_HPP

#include "domain/ScanPage.hpp"
#include "domain/ScanProgress.hpp"

#include <filesystem>
#include <optional>
#include <vector>

/**
 * @brief Moteur de navigation dans une arborescence locale de pages.
 *
 * Objectif projet :
 * Séparer la logique chapitre/page de l'interface graphique afin que les règles de navigation
 * soient testables, réutilisables et indépendantes de GTK.
 *
 * Interagit avec :
 * - le dossier racine d'un scan ;
 * - les objets `ScanProgress` et `ScanPage` ;
 * - le widget `Scan` qui affiche le résultat.
 */
class ScanSession {
public:
    ScanSession() = default;
    explicit ScanSession(std::filesystem::path root, ScanProgress initial = {});

    void open(std::filesystem::path root, ScanProgress initial = {});
    [[nodiscard]] bool is_open() const;

    [[nodiscard]] const std::filesystem::path& root() const;
    [[nodiscard]] ScanProgress progress() const;
    [[nodiscard]] std::filesystem::path current_page_path() const;

    [[nodiscard]] std::optional<ScanPage> current_page() const;
    [[nodiscard]] std::optional<ScanPage> next_page();
    [[nodiscard]] std::optional<ScanPage> previous_page();
    [[nodiscard]] std::optional<ScanPage> go_to(ScanProgress requested);

    [[nodiscard]] int max_chapter() const;
    [[nodiscard]] int max_page_in_current_chapter() const;

private:
    std::filesystem::path root_{};
    ScanProgress progress_{};

    [[nodiscard]] std::optional<std::filesystem::path> page_path(int chapter, int page) const;
    [[nodiscard]] std::vector<int> chapters() const;
    [[nodiscard]] std::vector<int> pages_for_chapter(int chapter) const;
};

#endif
