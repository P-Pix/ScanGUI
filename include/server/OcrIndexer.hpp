/**
 * @file OcrIndexer.hpp
 * @brief Déclare le service d’indexation OCR/texte des pages.
 *
 * L’indexation permet à l’API locale d’exposer une recherche texte sur les pages déjà
 * synchronisées en base. Le service reste optionnel : il peut lire des fichiers sidecar `.txt`
 * ou lancer une commande OCR externe configurée par l’environnement.
 */
#ifndef SCANGUI_SERVER_OCR_INDEXER_HPP
#define SCANGUI_SERVER_OCR_INDEXER_HPP

#include "server/PostgresScanDatabase.hpp"

#include <filesystem>
#include <string>

/**
 * @brief Compteurs produits par une indexation OCR ou sidecar.
 *
 * Ils permettent à l’endpoint d’administration de rendre compte des pages parcourues, indexées,
 * ignorées ou en échec sans exposer les exceptions internes.
 */
struct OcrIndexReport {
    int pagesVisited{0};
    int indexedPages{0};
    int skippedPages{0};
    int failedPages{0};
};

/**
 * @brief Service d’indexation texte des pages de scan.
 *
 * Objectif projet :
 * Alimenter la recherche intelligente en stockant dans PostgreSQL le texte issu de fichiers
 * sidecar `.txt` ou d’une commande OCR externe configurable. Le service reste optionnel afin
 * de ne pas imposer Tesseract ou un autre moteur à l’installation de base.
 *
 * Interagit avec :
 * - les pages indexées dans PostgreSQL ;
 * - les fichiers sidecar présents près des images ;
 * - l’endpoint d’administration `/api/admin/ocr`.
 */
class OcrIndexer {
public:
    OcrIndexer(PostgresScanDatabase& database, std::string commandTemplate = {});

    /**
     * @brief Indexe le texte de tous les scans connus en base.
     *
     * @return Rapport cumulé d’indexation.
     */
    [[nodiscard]] OcrIndexReport index_all();

    /**
     * @brief Indexe le texte des pages d’un scan précis.
     *
     * @param scanId Identifiant du scan à traiter.
     * @return Rapport d’indexation du scan.
     */
    [[nodiscard]] OcrIndexReport index_scan(const std::string& scanId);

private:
    PostgresScanDatabase& database_;
    std::string commandTemplate_;

    /**
     * @brief Extrait le texte exploitable d’une page.
     *
     * La méthode privilégie un sidecar local avant de lancer la commande OCR externe.
     *
     * @param page Page indexée en base.
     * @return Texte extrait ou chaîne vide.
     */
    [[nodiscard]] std::string extract_text(const PageRecord& page) const;

    /**
     * @brief Lit un fichier `.txt` associé à l’image si présent.
     *
     * @param imagePath Chemin de l’image de référence.
     * @return Texte du sidecar ou chaîne vide.
     */
    [[nodiscard]] std::string read_sidecar_text(const std::filesystem::path& imagePath) const;

    /**
     * @brief Lance la commande OCR externe configurée.
     *
     * @param imagePath Image à traiter.
     * @return Texte produit dans le fichier temporaire attendu.
     */
    [[nodiscard]] std::string run_ocr_command(const std::filesystem::path& imagePath) const;

    /**
     * @brief Détermine si un texte mérite d’être indexé.
     *
     * @param text Texte extrait.
     * @return true si le contenu contient au moins quelques caractères alphanumériques.
     */
    [[nodiscard]] static bool is_text_useful(const std::string& text);
};

#endif
