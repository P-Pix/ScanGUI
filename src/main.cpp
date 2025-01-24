#include "MainWindow.hpp"

#include <gtkmm/application.h>
#include <random>
#include <ctime>
//#include <discord_rpc.h>

std::string randomString(int length) {
    std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

    std::random_device rd;
    std::mt19937 generator(rd());

    std::shuffle(str.begin(), str.end(), generator);

    return str.substr(0, length);    // assumes 32 < number of characters in str
}

int main(int argc, char** argv) {
    std::srand(std::time(nullptr));
    auto app = Gtk::Application::create(argc, argv, "org.gtkmm.example");
    //auto app = Gtk::Application::create(argc, argv, randomString(32));

    MainWindow window;

    auto icon = Gdk::Pixbuf::create_from_file("./asset/icon2.png");

    window.set_icon(icon);

    /*
    // Discord
    DiscordEventHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    Discord_Initialize("898013013013013013", &handlers, 1, nullptr);

    DiscordRichPresence discordPresence;
    memset(&discordPresence, 0, sizeof(discordPresence));
    discordPresence.state = "Lecture de manga";
    discordPresence.details = "Lecture de manga";
    discordPresence.largeImageKey = "icon";
    discordPresence.largeImageText = "Manga Reader";
    Discord_UpdatePresence(&discordPresence);
    */
    
    return app->run(window);
}