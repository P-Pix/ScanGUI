/**
 * @file Profil.hpp
 * @brief Déclare le modèle minimal de profil utilisateur historique.
 *
 * Cette classe conserve une structure simple utilisée par l'ancienne fenêtre de profil et
 * pourra servir de point d'extension pour une gestion multi-profils plus complète.
 */

#pragma once

#include <iostream>
#include <string>

/**
 * @brief Modèle minimal de profil utilisateur.
 *
 * Cette classe reste volontairement simple et sert de support à l'écran de profil historique.
 */
class Profil {
    private:
        std::string name;

    public:
        Profil(void);
        
        Profil(std::string name);

        ~Profil();

        std::string get_name();

        void set_name(std::string name);
};