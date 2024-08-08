#include "MainWindow.hpp"

MainWindow::MainWindow() : 
        m_VBox(Gtk::ORIENTATION_VERTICAL),
        m_MenuBar(),
        m_File("File"),
        m_FileMenu(),
        m_FileOpen("Open"),
        m_FileSave("Save"),
        m_FileQuit("Quit") {
    set_title("Scan Manager");
    set_default_size(800, 600);
    set_position(Gtk::WIN_POS_CENTER);

    m_MenuBar.append(m_File);

    m_FileMenu.append(m_FileOpen);
    m_FileOpen.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_file_open));
    m_FileMenu.append(m_FileSave);
    m_FileSave.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_file_save));
    m_FileMenu.append(m_FileQuit);
    m_FileQuit.signal_activate().connect(sigc::mem_fun(*this, &MainWindow::on_menu_file_quit));

    m_File.set_submenu(m_FileMenu);

    m_VBox.pack_start(m_MenuBar, Gtk::PACK_SHRINK);

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
    selected_folder = scan_select.get_selected_folder();
    std::cout << "File -> Open" << std::endl;
    std::cout << "Selected folder: " << selected_folder << std::endl;
}

void MainWindow::on_menu_file_save() {
    std::cout << "File -> Save" << std::endl;
}

void MainWindow::on_menu_file_quit() {
    hide();
}