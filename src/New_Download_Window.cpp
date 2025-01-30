#include "New_Download_Window.hpp"

New_Download_Window::New_Download_Window() : m_VBox(Gtk::ORIENTATION_VERTICAL), m_Entry(), m_Button("Download") {
    set_title("New Download");
    set_border_width(10);
    set_default_size(200, 100);

    m_Button.signal_clicked().connect(sigc::mem_fun(*this, &New_Download_Window::on_button_clicked));

    m_VBox.pack_start(m_Entry);
    m_VBox.pack_start(m_Button);

    add(m_VBox);

    show_all_children();
}

New_Download_Window::~New_Download_Window() {
}

void New_Download_Window::on_button_clicked() {
    std::string url = m_Entry.get_text();
    if (url.find("https://lelscans.net/") != std::string::npos) {
        url.replace(0, 26, "").replace(url.begin() + url.find("/"), url.end(), "");
        this->folder = url;
        std::string command = "mkdir -p ./scan/" + url;
        std::system(command.c_str());
        std::string number = m_Entry.get_text();
        number.replace(0, 26, "").replace(0, number.find("/") + 1, "");
        std::string chapter = number.substr(0, number.find("/"));
        std::string page = number.substr(number.find("/") + 1, number.size());
        std::string json = "{\"download\" : {\"chapter\" : " + chapter + ",\"page\" : " + page + ",\"url\" : \"https://lelscans.net/scan-" + url + "/\"},\"save\" : {\"chapter\" : " + chapter + ",\"page\" : " + page +"}}";
        std::ofstream out("./scan/" + url + "/data.json", std::ios::out);
        out << json;
        out.close();
    }
    hide();
}