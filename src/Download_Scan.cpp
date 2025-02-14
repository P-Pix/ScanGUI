#include "Download_Scan.hpp"

Download_Scan::Download_Scan() : website(""), folder(""), page(0), chapitre(0) {
}

Download_Scan::Download_Scan(std::string website) : website(website), folder("One_Piece"), page(0), chapitre(0) {
    std::cout << "Download_Scan: " << website << std::endl;
}

Download_Scan::~Download_Scan() {
}

void Download_Scan::set_website(std::string website) {
    this->website = website;
}

size_t Download_Scan::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t totalSize = size * nmemb;
    s->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string Download_Scan::get_website() {
    return this->website;
}

std::string Download_Scan::get_website_content(std::string site) {
    std::string command = "wget -q -O - " + site;
    std::array<char, 128> buffer;
    std::string result;

    // Ouvre un flux de lecture avec popen pour exécuter la commande
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() a échoué!");
    }

    // Lire la sortie de la commande par morceaux de 128 octets
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

std::string Download_Scan::download_picture_page_lelscan(std::string site) {
    std::string result = this->get_website_content(site);
    // recupere le body
    size_t pos = result.find("<body>");
    if (pos != std::string::npos) {
        result = result.substr(pos);
    }
    pos = result.find("</body>");
    if (pos != std::string::npos) {
        result = result.substr(0, pos);
    }
    // recupere le center
    pos = result.find("<center>");
    if (pos != std::string::npos) {
        result = result.substr(pos);
    }
    pos = result.find("</center>");
    if (pos != std::string::npos) {
        result = result.substr(0, pos);
    }
    // find '<img src="/mangas'
    pos = result.find("<img src=\"/mangas");
    if (pos != std::string::npos) {
        result = result.substr(pos);
    }
    // find 'jpg'
    pos = result.find("jpg");
    if (pos != std::string::npos) {
        result = result.substr(0, pos + 3);
    }
    // soustraire "<img src=\"/"
    result = result.substr(11);
    return "https://lelscans.net/" + result;
}

std::string Download_Scan::url_next_page_lelscan(std::string url) {
    std::string result = this->get_website_content(url);
    // recupere le body
    // recupere à partir de "<div id="image">"
    int pos = result.find("<div id=\"image\">");
    if (pos != std::string::npos) {
        result = result.substr(pos);
    }
    // recupere à partir de "<a href="
    pos = result.find("<a href=");
    if (pos != std::string::npos) {
        result = result.substr(pos);
    }
    // recupere jusqu'à ">"
    pos = result.find(">");
    if (pos != std::string::npos) {
        result = result.substr(0, pos);
    }
    // soustraire "<a href="
    result = result.substr(8);
    //recupere jusqu'à "title"
    pos = result.find("title");
    if (pos != std::string::npos) {
        result = result.substr(0, pos);
    }
    return result;
}

void Download_Scan::download_picture_page(std::string site, std::string file_name) {
    std::string link = "";
    if (site.find("lelscans") != std::string::npos) {
        link = this->download_picture_page_lelscan(site);
        std::cout << "Download_Scan: " << link << std::endl;
    }
    if (link != "") {
        std::string command = "wget -q -O " + file_name + " " + link;
        std::cout << "Download_Scan: " << command << std::endl;
        std::system(command.c_str());
    }
}

std::string Download_Scan::next_page() {
    return "";
}

std::string Download_Scan::previous_page() {
    return "";
}

std::string Download_Scan::get_next_page_url(std::string url) {
    if (url.find("lelscans") != std::string::npos) {
        return this->url_next_page_lelscan(url);
    }
    return "";
}