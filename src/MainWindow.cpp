#include "MainWindow.hpp"

MainWindow::MainWindow() : 
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
        m_ScrollImage() {
    set_title("Scan Manager");
    set_default_size(this->WIDTH, this->HEIGHT);
    set_position(Gtk::WIN_POS_CENTER);

    add_events(Gdk::KEY_PRESS_MASK);
    signal_key_press_event().connect(sigc::mem_fun(*this, &MainWindow::on_key_press));

    this->m_Scan.set_width(this->WIDTH);
    this->m_Scan.set_height(this->HEIGHT);

    this->m_EventBox.add(this->m_Scan);
    this->m_EventBox.signal_button_press_event().connect(sigc::mem_fun(*this, &MainWindow::on_image_clic));
    this->m_ScrollImage.add(this->m_EventBox);

    this->liste_manga();

    // faire une colone de petite taille pour box manga et mettre le reste pour eventbox
    this->m_Grid.attach(this->m_ScrollListeManga, 0, 0, 1, 1);
    this->m_Grid.attach(this->m_ScrollImage, 1, 0, 1, 1);
    this->m_Grid.set_column_homogeneous(false);

    this->m_EventBox.set_hexpand(true);
    this->m_EventBox.set_vexpand(true);

    m_MenuBar.append(m_File);
    m_FileMenu.append(m_FileOpen);
    m_FileOpen.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_file_open));
    m_FileMenu.append(m_FileSave);
    m_FileSave.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_file_save));
    m_FileMenu.append(m_FileQuit);
    m_FileQuit.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_file_quit));
    m_File.set_submenu(m_FileMenu);

    m_MenuBar.append(m_Download);
    m_DownloadMenu.append(m_NewScan);
    m_NewScan.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_download_new));
    m_DownloadMenu.append(m_UpdateScan);
    m_UpdateScan.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_download_update));
    m_Download.set_submenu(m_DownloadMenu);

    m_VBox.pack_start(m_MenuBar, Gtk::PACK_SHRINK);
    m_VBox.pack_start(m_Grid, Gtk::PACK_EXPAND_WIDGET);
    
    add(m_VBox);
    show_all_children();
}

MainWindow::~MainWindow() {
    for (auto button : m_ListButtonManga) {
        delete button;
    }
    std::cout << "MainWindow deleted" << std::endl;
}

void MainWindow::liste_manga() {
    for (const auto& entry : std::filesystem::directory_iterator("./scan")) {
        if (entry.is_directory()) {
            std::cout << entry.path().filename() << std::endl;
            std::string name = entry.path().filename().string();
            // si le nom est trop long on le met sur plusieurs ligne
            if (name.size() > 15) {
                for (size_t i = 15; i < name.size(); i += 15) {
                    // on cherche s'il y a un espace pour ne pas couper un mot
                    int j = i;
                    while (name[j] != ' ' && j > 0) {
                        j--;
                    }
                    if (j == 0) {
                        name.insert(i, "\n");
                    } else {
                        name.insert(j, "\n");
                    }
                }
            }
            // transformer les "-"" en espace et mettre la première lettre de chaque mot en majuscule
            name[0] = std::toupper(name[0]);
            for (size_t i = 0; i < name.size(); i++) {
                if (name[i] == '-') {
                    name[i] = ' ';
                    name[i + 1] = std::toupper(name[i + 1]);
                }
            }
            Gtk::Button* button = Gtk::manage(new Gtk::Button(name.c_str()));
            button->signal_clicked().connect([this, entry]() {
                this->selected_folder = "./scan/" + entry.path().filename().string();
                this->open_Scan(this->selected_folder);
            });
            this->m_ListButtonManga.push_back(button);
            this->m_GridListeManga.attach(*button, 0, this->m_ListButtonManga.size(), 1, 1);
        }
    }
    this->m_ScrollListeManga.set_size_request(150, this->HEIGHT);
    this->m_ScrollListeManga.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    this->m_ScrollListeManga.add(this->m_GridListeManga);
}

bool MainWindow::on_key_press(GdkEventKey* event) {
    std::cout << "Key: " << event->keyval << std::endl;
    if (event->state & GDK_CONTROL_MASK) {
        std::cout << "Ctrl" << std::endl;
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
    } else if (event->keyval == GDK_KEY_Left) {
        m_Scan.previous_page();
    } else if (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_KP_Add) {
        m_Scan.zoom_in();
    } else if (event->keyval == GDK_KEY_minus || event->keyval == GDK_KEY_KP_Subtract) {
        m_Scan.zoom_out();
    }
    return true;

}

void MainWindow::on_menu_file_open() {
    ScanSelect scan_select;
    scan_select.show();
    while (scan_select.is_visible()) {
        Glib::MainContext::get_default()->iteration(true);
    }
    selected_folder = "./scan/" + scan_select.get_selected_folder();
    this->open_Scan(selected_folder);
}

void MainWindow::open_Scan(std::string folder) {
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
}

void MainWindow::on_menu_file_save() {
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

void MainWindow::on_menu_file_quit() {
    hide();
}

bool MainWindow::on_image_clic(GdkEventButton* event) {
    if (event->type == GDK_BUTTON_PRESS) {
        std::cout << "Button: " << event->button << std::endl;
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
        return true;
    }
    return false;
}

void MainWindow::on_menu_download_update() {
    ScanSelect scan_select;
    scan_select.show();
    while (scan_select.is_visible()) {
        Glib::MainContext::get_default()->iteration(true);
    }
    std::string folder = "./scan/" + scan_select.get_selected_folder();
    this->update_scan(folder);
}

void MainWindow::on_menu_download_new() {
    NewDownloadWindow new_download_window;
    new_download_window.show();
    while (new_download_window.is_visible()) {
        Glib::MainContext::get_default()->iteration(true);
    }
    this->update_scan(new_download_window.get_folder());
}

void MainWindow::update_scan(std::string folder) {
    std::string save_file = folder + "/data.json";
    std::ifstream file(folder + "/data.json", std::ifstream::binary); // json file
    Json::Value json;
    file >> json;
    int page = json["download"]["page"].asInt();
    int chapitre = json["download"]["chapter"].asInt();
    
    DownloadScan download_scan;
    std::string folder_current = folder + "/" + std::to_string(chapitre);
    std::string command_mkdir = "mkdir -p " + folder_current;
    std::system(command_mkdir.c_str());

    std::string url_picture = download_scan.get_picture_page_lelscan(json["download"]["url"].asString() + "/" + std::to_string(chapitre) + "/" + std::to_string(page));
    std::string file_name = folder_current + "/" + std::to_string(page) + ".jpg";
    std::string command_picture = "wget -q -O " + file_name + " " + url_picture;
    std::system(command_picture.c_str());

    std::string next_page = download_scan.url_next_page(json["download"]["url"].asString() + "/" + std::to_string(chapitre) + "/" + std::to_string(page));

    while (next_page != "") {
        std::string chap = next_page;
        std::string cut = chap.replace(0, json["download"]["url"].asString().size() + 1, "");
        if (cut != std::to_string(chapitre) && cut.find("/") == std::string::npos) {
            chapitre++;
            folder_current = folder + "/" + std::to_string(chapitre);
            std::cout << "Folder: " << folder_current << std::endl;
            command_mkdir = "mkdir -p " + folder_current;
            std::system(command_mkdir.c_str());
            next_page = next_page + "/1";
            page = 1;
            json["download"]["page"] = page;
            json["download"]["chapter"] = chapitre;
            std::ofstream save(save_file, std::ofstream::binary);
            save << json;
            save.close();
        } else {
            page++;
        }
        url_picture = download_scan.get_picture_page_lelscan(next_page);
        std::cout << "Page: " << page << " | url: " << url_picture << std::endl;
        file_name = folder_current + "/" + std::to_string(page) + ".jpg";
        command_picture = "wget -q -O " + file_name + " " + url_picture;
        std::system(command_picture.c_str());
        next_page = download_scan.url_next_page(next_page);
    }
    json["download"]["page"] = page;
    json["download"]["chapter"] = chapitre;
    std::ofstream save(save_file, std::ofstream::binary);
    save << json;
    save.close();
}

void MainWindow::download_picture(std::string folder, std::string url) {
}