#pragma once

#include <iostream>
#include <string>

class Profil {
    private:
        std::string name;

    public:
        Profil(void);
        
        Profil(std::string name);

        ~Profil();

        std::string get_name();

        void set_name(std::string name);
}