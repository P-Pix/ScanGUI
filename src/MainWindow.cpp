#include "Main_Window.hpp"

Main_Window::Main_Window() : 
        m_VBox(Gtk::ORIENTATION_VERTICAL),
        m_MenuBar(),
        m_File("File"),
        m_FileMenu(),
        m_FileOpen("Open"),
        m_FileSave("Save"),
        m_FileQuit("Quit"),
        m_Download("Download"),
        m_DownloadMenu(),
        m_UpdateScan("Update"),
        m_NewScan("New"),
        m_Scan(), 
        m_EventBox(),
        m_ScrollListeManga(),
        m_ListButtonManga(),
        m_Grid(),
        m_GridListeManga(), 
        m_ScrollImage(),
        m_go_to(),
        m_Entry_page(),
        m_Entry_chapter(),
        m_Button_go_to(),
        m_GridImage_go_to(),
        m_UpdateAllScan("Update all") {
    set_title("Scan Manager");
    set_default_size(this->WIDTH_WINDOW, this->HEIGHT_WINDOW);
    set_position(Gtk::WIN_POS_CENTER);

    add_events(Gdk::KEY_PRESS_MASK);
    signal_key_press_event().connect(sigc::mem_fun(*this, &Main_Window::on_key_press));

    this->m_Scan.set_width(this->WIDTH);
    this->m_Scan.set_height(this->HEIGHT);

    this->m_EventBox.add(this->m_Scan);
    this->m_EventBox.signal_button_press_event().connect(sigc::mem_fun(*this, &Main_Window::on_image_clic));
    this->m_ScrollImage.add(this->m_EventBox);

    // this->m_Entry_page set text in background
    this->m_Entry_page.set_placeholder_text("Page, actuelle : " + std::to_string(this->m_Scan.get_page_number()) + " / " + std::to_string(this->m_Scan.get_max_page()));
    this->m_go_to.attach(this->m_Entry_page, 0, 0, 1, 1);
    this->m_Entry_chapter.set_placeholder_text("Chapter, actuelle : " + std::to_string(this->m_Scan.get_chapitre()) + " / " + std::to_string(this->m_Scan.get_max_chapter()));
    this->m_go_to.attach(this->m_Entry_chapter, 1, 0, 1, 1);
    this->m_go_to.attach(this->m_Button_go_to, 2, 0, 1, 1);
    this->m_Button_go_to.set_label("Go to");
    this->m_Button_go_to.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::go_to));
    this->m_go_to.set_column_homogeneous(true);

    this->m_GridImage_go_to.attach(this->m_go_to, 0, 1, 1, 1);
    this->m_GridImage_go_to.attach(this->m_ScrollImage, 0, 0, 1, 1);

    this->liste_manga();

    // faire une colone de petite taille pour box manga et mettre le reste pour eventbox
    this->m_Grid.attach(this->m_ScrollListeManga, 0, 0, 1, 1);
    this->m_Grid.attach(this->m_GridImage_go_to, 1, 0, 1, 1);
    this->m_Grid.set_column_homogeneous(false);

    this->m_EventBox.set_hexpand(true);
    this->m_EventBox.set_vexpand(true);

    m_MenuBar.append(m_File);
    m_FileMenu.append(m_FileOpen);
    m_FileOpen.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::on_menu_file_open));
    m_FileMenu.append(m_FileSave);
    m_FileSave.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::on_menu_file_save));
    m_FileMenu.append(m_FileQuit);
    m_FileQuit.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::on_menu_file_quit));
    m_File.set_submenu(m_FileMenu);

    m_MenuBar.append(m_Download);
    m_DownloadMenu.append(m_NewScan);
    m_NewScan.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::on_menu_download_new));
    m_DownloadMenu.append(m_UpdateScan);
    m_UpdateScan.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::on_menu_download_update));
    m_DownloadMenu.append(m_UpdateAllScan);
    m_UpdateAllScan.signal_activate().connect(sigc::mem_fun(*this, &Main_Window::update_all_scan));
    m_Download.set_submenu(m_DownloadMenu);

    m_VBox.pack_start(m_MenuBar, Gtk::PACK_SHRINK);
    m_VBox.pack_start(m_Grid, Gtk::PACK_EXPAND_WIDGET);
    
    add(m_VBox);
    show_all_children();
}

Main_Window::~Main_Window() {
    for (auto button : m_ListButtonManga) {
        delete button;
    }
    std::cout << "Main_Window deleted" << std::endl;
}

void Main_Window::go_to(void) {
    int page = 0;
    std::string page_text = this->m_Entry_page.get_text();
    if (page_text.empty()) {
        page = this->m_Scan.get_page_number();
    } else {
        page = std::stoi(page_text);
    }
    int chapter = 0;
    std::string chapter_text = this->m_Entry_chapter.get_text();
    if (chapter_text.empty()) {
        chapter = this->m_Scan.get_chapitre();
    } else {
        chapter = std::stoi(chapter_text);
    }
    std::cout << "Go to: " << chapter << " " << page << std::endl;
    if (page < 1) {
        page = 1;
    }
    if (chapter < 1) {
        chapter = 1;
    }
    this->m_Scan.set_page(this->selected_folder, chapter, page);
}

void Main_Window::liste_manga() {
    std::filesystem::directory_iterator("./scan");
    // trier les dossiers par ordre alphabétique
    std::vector<std::string> folders;
    for (const auto& entry : std::filesystem::directory_iterator("./scan")) {
        if (entry.is_directory()) {
            folders.push_back(entry.path().filename().string());
        }
    }
    std::sort(folders.begin(), folders.end());
    for (std::string entry : folders) {
        std::string name = entry;
        // si le nom est trop long on le met sur plusieurs ligne
        if (name.size() > 15) {
            // trouver le dernier " " avant pour tous les 15 caractères
            for (size_t i = 15; i < name.size(); i += 15) {
                size_t pos = name.rfind("-", i);
                if (pos != std::string::npos) {
                    name[pos] = '\n';
                }
            }
        }
        // transformer les "-"" en espace et mettre la première lettre de chaque mot en majuscule
        name[0] = std::toupper(name[0]);
        for (size_t i = 0; i < name.size(); i++) {
            if (name[i] == '-') {
                name[i] = ' ';
                name[i + 1] = std::toupper(name[i + 1]);
            } else if (name[i - 1] == '\n') {
                name[i] = std::toupper(name[i]);
            }
        }
        Gtk::Button* button = Gtk::manage(new Gtk::Button(name.c_str()));
        button->signal_clicked().connect([this, entry]() {
            this->selected_folder = "./scan/" + entry;
            this->open_Scan(this->selected_folder);
        });
        this->m_ListButtonManga.push_back(button);
        this->m_GridListeManga.attach(*button, 0, this->m_ListButtonManga.size(), 1, 1);
    }
    this->m_ScrollListeManga.set_size_request(150, this->HEIGHT);
    this->m_ScrollListeManga.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    this->m_ScrollListeManga.add(this->m_GridListeManga);
}

void Main_Window::update_all_scan() {
    std::filesystem::directory_iterator("./scan");
    // trier les dossiers par ordre alphabétique
    std::vector<std::string> folders;
    for (const auto& entry : std::filesystem::directory_iterator("./scan")) {
        if (entry.is_directory()) {
            folders.push_back(entry.path().filename().string());
        }
    }
    std::sort(folders.begin(), folders.end());
    for (std::string entry : folders) {
        std::string folder = "./scan/" + entry;
        this->update_scan(folder);
    }
}

bool Main_Window::on_key_press(GdkEventKey* event) {
    if (event->state & GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_o) {
            this->on_menu_file_open();
        } else if (event->keyval == GDK_KEY_s) {
            this->on_menu_file_save();
        } else if (event->keyval == GDK_KEY_q) {
            this->on_menu_file_quit();
        } else if (event->keyval == GDK_KEY_d) {
            this->on_menu_download_update();
        } else if (event->keyval == GDK_KEY_n) {
            this->on_menu_download_new();
        }
    }
    if (event->keyval == GDK_KEY_Right) {
        m_Scan.next_page();
        this->m_Entry_page.set_placeholder_text("Page, actuelle : " + std::to_string(this->m_Scan.get_page_number()) + " / " + std::to_string(this->m_Scan.get_max_page()));
        this->m_Entry_chapter.set_placeholder_text("Chapter, actuelle : " + std::to_string(this->m_Scan.get_chapitre()) + " / " + std::to_string(this->m_Scan.get_max_chapter()));
    } else if (event->keyval == GDK_KEY_Left) {
        m_Scan.previous_page();
        this->m_Entry_page.set_placeholder_text("Page, actuelle : " + std::to_string(this->m_Scan.get_page_number()) + " / " + std::to_string(this->m_Scan.get_max_page()));
        this->m_Entry_chapter.set_placeholder_text("Chapter, actuelle : " + std::to_string(this->m_Scan.get_chapitre()) + " / " + std::to_string(this->m_Scan.get_max_chapter()));
    } else if (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_KP_Add) {
        m_Scan.zoom_in();
    } else if (event->keyval == GDK_KEY_minus || event->keyval == GDK_KEY_KP_Subtract) {
        m_Scan.zoom_out();
    }
    return true;

}

void Main_Window::on_menu_file_open() {
    Scan_Select scan_select;
    scan_select.show();
    while (scan_select.is_visible()) {
        Glib::MainContext::get_default()->iteration(true);
    }
    selected_folder = "./scan/" + scan_select.get_selected_folder();
    this->open_Scan(selected_folder);
}

void Main_Window::open_Scan(std::string folder) {
    std::cout << "Open scan: " << folder << std::endl;
    std::string save_file = selected_folder + "/data.json";
    std::cout << "Save file: " << save_file << std::endl;
    std::ifstream file(save_file, std::ifstream::binary); // json file
    if (!file.is_open()) {
        m_Scan.set_page(selected_folder, 1, 1);
        return;
    }
    Json::Value json;
    file >> json;
    int page = json["save"]["page"].asInt();
    int chapitre = json["save"]["chapter"].asInt();
    m_Scan.set_page(selected_folder, chapitre, page);
    this->m_Entry_page.set_placeholder_text("Page, actuelle : " + std::to_string(this->m_Scan.get_page_number()) + " / " + std::to_string(this->m_Scan.get_max_page()));
    this->m_Entry_chapter.set_placeholder_text("Chapter, actuelle : " + std::to_string(this->m_Scan.get_chapitre()) + " / " + std::to_string(this->m_Scan.get_max_chapter()));
}

void Main_Window::on_menu_file_save() {
    std::string save_file = selected_folder + "/data.json";
    std::ifstream file(selected_folder + "/data.json", std::ifstream::binary); // json file
    Json::Value json;
    file >> json;
    json["save"]["chapter"] = m_Scan.get_chapitre();
    json["save"]["page"] = m_Scan.get_page_number();
    file.close();
    std::ofstream save(save_file, std::ofstream::binary);
    save << json;
    save.close();
}

void Main_Window::on_menu_file_quit() {
    hide();
}

bool Main_Window::on_image_clic(GdkEventButton* event) {
    if (event->type == GDK_BUTTON_PRESS) {
        this->m_Scan.set_width(this->WIDTH);
        this->m_Scan.set_height(this->HEIGHT);
        if (event->button == 1) {
            m_Scan.next_page();
        } else if (event->button == 3) {
            m_Scan.previous_page();
        }
        std::ifstream file(selected_folder + "/data.json", std::ifstream::binary); // json file
        Json::Value json;
        file >> json;
        json["save"]["chapter"] = m_Scan.get_chapitre();
        json["save"]["page"] = m_Scan.get_page_number();
        file.close();
        std::ofstream save(selected_folder + "/data.json", std::ofstream::binary);
        save << json;
        save.close();
        this->m_Entry_page.set_placeholder_text("Page, actuelle : " + std::to_string(this->m_Scan.get_page_number()) + " / " + std::to_string(this->m_Scan.get_max_page()));
        this->m_Entry_chapter.set_placeholder_text("Chapter, actuelle : " + std::to_string(this->m_Scan.get_chapitre()) + " / " + std::to_string(this->m_Scan.get_max_chapter()));
        return true;
    }
    return false;
}

void Main_Window::on_menu_download_update() {
    Scan_Select scan_select;
    scan_select.show();
    while (scan_select.is_visible()) {
        Glib::MainContext::get_default()->iteration(true);
    }
    std::string folder = "./scan/" + scan_select.get_selected_folder();
    this->update_scan(folder);
}

void Main_Window::on_menu_download_new() {
    New_Download_Window new_download_window;
    new_download_window.show();
    while (new_download_window.is_visible()) {
        Glib::MainContext::get_default()->iteration(true);
    }
    this->update_scan(new_download_window.get_folder());
}

void Main_Window::update_scan(std::string folder) {
    std::string data = folder + "/data.json";
    std::ifstream file(data, std::ifstream::binary); // json file
    file >> this->m_current_scan;
    file.close();

    Download_Scan download_scan;
    std::string url = this->m_current_scan["download"]["url"].asString() + this->m_current_scan["download"]["chapter"].asString() + "/" + this->m_current_scan["download"]["page"].asString();

    do {
        std::cout << "URL: " << url << std::endl;

        // télécharge l'image courante
        std::string folder_current = folder + "/" + this->m_current_scan["download"]["chapter"].asString();
        download_scan.download_picture_page(url, folder_current + "/" + this->m_current_scan["download"]["page"].asString() + ".jpg");

        // récupère l'url de la page suivante
        std::string next_page = download_scan.get_next_page_url(url);
        std::cout << "Next page: " << next_page << std::endl;

        // test si on a un nouveau chapitre ou non
        url = this->new_chapter(folder, next_page);
    } while (url != "");

    std::cout << "Download End" << std::endl;

    std::ofstream save(data, std::ofstream::binary);
    save << this->m_current_scan;
    save.close();
}

std::string Main_Window::new_chapter(std::string folder, std::string url) {
    if (url == "/1" || url == "#main_hot" || url == "") {
        return "";
    }

    std::string chap = url;
    std::string cut = chap.replace(0, this->m_current_scan["download"]["url"].asString().size() + 1, "");

    if (cut != this->m_current_scan["download"]["chapter"].asString() &&
        cut.find("/") == std::string::npos) {
        url = url + "/1";

        if (url == "/1" || url == "#main_hot") {
            return "";
        }

        this->m_current_scan["download"]["page"] = 1;
        this->m_current_scan["download"]["chapter"] = this->m_current_scan["download"]["chapter"].asInt() + 1;

        std::filesystem::create_directory(folder + "/" + this->m_current_scan["download"]["chapter"].asString());
        
    } else {
        this->m_current_scan["download"]["page"] = this->m_current_scan["download"]["page"].asInt() + 1;
    }

    std::ofstream save(folder + "/data.json", std::ofstream::binary);
    save << this->m_current_scan;
    save.close();
    return url;
}