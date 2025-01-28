#include "../include/Profil.hpp"

Profil::Profil() : name("") {
}

Profil::Profil(std::string name) : name(name) {
}

Profil::~Profil() {
}

std::string Profil::get_name() {
    return this->name;
}

void Profil::set_name(std::string name) {
    this->name = name;
}