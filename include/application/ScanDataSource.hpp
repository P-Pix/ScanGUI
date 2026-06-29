/**
 * @file ScanDataSource.hpp
 * @brief Définit le contrat d'accès aux scans utilisé par l'application.
 *
 * Ce header découple les écrans et services applicatifs de la source réelle des données.
 * Les implémentations peuvent lire directement le disque ou consommer l'API HTTP locale
 * sans modifier le code qui manipule les scans, chapitres, pages et progressions.
 */

#ifndef SCANGUI_APPLICATION_SCAN_DATA_SOURCE_HPP
#define SCANGUI_APPLICATION_SCAN_DATA_SOURCE_HPP

#include "domain/ScanProgress.hpp"

#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief Résumé affichable d'une lecture disponible.
 *
 * Cette structure transporte les informations suffisantes pour construire une bibliothèque
 * côté interface sans exposer la représentation complète en base ou sur disque.
 */
struct ScanSummary {
    std::string id;
    std::string title;
    int chapterCount{0};
    int pageCount{0};
};

/**
 * @brief Métadonnées d'une page exposée par une source de scans.
 *
 * Les champs permettent d'identifier la page, d'afficher des informations techniques et de
 * récupérer l'image via une URL ou une matérialisation locale selon l'implémentation.
 */
struct ScanPageInfo {
    int chapter{0};
    int page{0};
    std::string mimeType;
    long long sizeBytes{0};
    std::string imageUrl;
};

/**
 * @brief Contrat d'accès aux scans pour les couches applicatives et UI.
 *
 * Objectif projet :
 * Cette interface évite de lier l'interface GTK à une source unique de données. Le même code
 * peut ainsi lire directement `./scan` ou passer par le serveur HTTP local adossé à PostgreSQL.
 *
 * Interagit avec :
 * - les widgets et services qui affichent la bibliothèque ;
 * - les implémentations fichier et API ;
 * - les structures métier de progression et de pages.
 */
class ScanDataSource {
public:
    virtual ~ScanDataSource() = default;

    /**
     * @brief Liste les scans disponibles dans la source active.
     *
     * @return Résumés normalisés utilisables par l'écran de bibliothèque.
     */
    [[nodiscard]] virtual std::vector<ScanSummary> list_scans() const = 0;
    /**
     * @brief Retourne les chapitres disponibles pour un scan.
     *
     * @param scanId Identifiant stable du scan demandé.
     * @return Numéros de chapitres disponibles, généralement triés par ordre croissant.
     */
    [[nodiscard]] virtual std::vector<int> list_chapters(const std::string& scanId) const = 0;
    /**
     * @brief Retourne les pages connues pour un chapitre donné.
     *
     * @param scanId Identifiant stable du scan demandé.
     * @param chapter Numéro du chapitre à inspecter.
     * @return Métadonnées des pages disponibles pour ce chapitre.
     */
    [[nodiscard]] virtual std::vector<ScanPageInfo> list_pages(const std::string& scanId, int chapter) const = 0;
    /**
     * @brief Produit un chemin local affichable pour une page.
     *
     * Objectif projet :
     * L'interface GTK affiche des fichiers images. Cette méthode cache donc la différence entre
     * une page déjà présente sur disque et une page récupérée depuis l'API puis mise en cache.
     *
     * @param scanId Identifiant stable du scan.
     * @param progress Position chapitre/page à matérialiser.
     * @return Chemin local de l'image prête à être affichée.
     */
    [[nodiscard]] virtual std::filesystem::path materialize_page(const std::string& scanId, ScanProgress progress) const = 0;
    /**
     * @brief Charge la dernière progression connue pour un profil.
     *
     * @param scanId Identifiant stable du scan.
     * @param profile Profil de lecture concerné.
     * @return Position de lecture normalisée.
     */
    [[nodiscard]] virtual ScanProgress load_progress(const std::string& scanId, const std::string& profile = "default") const = 0;
    /**
     * @brief Sauvegarde la position de lecture courante.
     *
     * @param scanId Identifiant stable du scan.
     * @param progress Position chapitre/page à persister.
     * @param profile Profil de lecture concerné.
     */
    virtual void save_progress(const std::string& scanId, ScanProgress progress, const std::string& profile = "default") const = 0;
};

#endif
