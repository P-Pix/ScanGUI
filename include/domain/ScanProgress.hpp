/**
 * @file ScanProgress.hpp
 * @brief Définit la position de lecture ou de téléchargement dans un scan.
 *
 * Cette structure métier représente le couple chapitre/page utilisé par le moteur de
 * navigation, la sauvegarde locale, l'API et la base PostgreSQL.
 */

#ifndef SCANGUI_DOMAIN_SCAN_PROGRESS_HPP
#define SCANGUI_DOMAIN_SCAN_PROGRESS_HPP

/**
 * @brief Position chapitre/page utilisée par le lecteur et le téléchargement.
 *
 * Les valeurs sont volontairement simples pour rester compatibles avec le fichier JSON
 * historique, les routes API et les colonnes PostgreSQL de progression.
 */
struct ScanProgress {
    int chapter{1};
    int page{1};

    /**
     * @brief Ramène les valeurs invalides vers une position minimale valide.
     *
     * Une progression absente ou corrompue ne doit pas bloquer l'ouverture d'une lecture :
     * elle est ramenée au chapitre 1, page 1.
     */
    void normalize() {
        if (chapter < 1) {
            chapter = 1;
        }
        if (page < 1) {
            page = 1;
        }
    }
};

#endif
