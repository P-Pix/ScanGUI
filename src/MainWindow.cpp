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
        m_NewScan("New") {
    set_title("Scan Manager");
    set_default_size(this->WIDTH, this->HEIGHT);
    set_position(Gtk::WIN_POS_CENTER);

    this->m_Scan.set_width(this->WIDTH);
    this->m_Scan.set_height(this->HEIGHT - 100);

    this->m_EventBox.add(this->m_Scan);
    this->m_EventBox.signal_button_press_event().connect(sigc::mem_fun(*this, &MainWindow::on_image_clic));

    m_MenuBar.append(m_File);
    m_FileMenu.append(m_FileOpen);
    m_FileOpen.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_file_open));
    m_FileMenu.append(m_FileSave);
    m_FileSave.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_file_save));
    m_FileMenu.append(m_FileQuit);
    m_FileQuit.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_file_quit));
    m_File.set_submenu(m_FileMenu);

    m_MenuBar.append(m_Download);
    m_DownloadMenu.append(m_UpdateScan);
    m_UpdateScan.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_download_update));
    m_DownloadMenu.append(m_NewScan);
    m_NewScan.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_download_new));
    m_Download.set_submenu(m_DownloadMenu);

    m_VBox.pack_start(m_MenuBar, Gtk::PACK_SHRINK);
    m_VBox.pack_start(m_EventBox, Gtk::PACK_EXPAND_WIDGET);
    
    add(m_VBox);
    show_all_children();
}

MainWindow::~MainWindow() {
}

void MainWindow::on_menu_file_open() {
    ScanSelect scan_select;
    scan_select.show();
    while (scan_select.is_visible()) {
        Glib::MainContext::get_default()->iteration(true);
    }
    selected_folder = "./scan/" + scan_select.get_selected_folder();
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
        if (event->button == 1) {
            m_Scan.next_page();
        } else if (event->button == 3) {
            m_Scan.previous_page();
        }
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
        std::string cut = chap.replace(0, json["download"]["url"].asString().size(), "");
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