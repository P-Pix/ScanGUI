#include "DownloadScan.hpp"

DownloadScan::DownloadScan() : website(""), folder(""), page(0), chapitre(0) {
}

DownloadScan::DownloadScan(std::string website) : website(website), folder("One_Piece"), page(0), chapitre(0) {
    std::cout << "DownloadScan: " << website << std::endl;
}

DownloadScan::~DownloadScan() {
}

void DownloadScan::set_website(std::string website) {
    this->website = website;
}

size_t DownloadScan::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t totalSize = size * nmemb;
    s->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string DownloadScan::get_website() {
    return this->website;
}

std::string DownloadScan::get_website_content(std::string site) {
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

std::string DownloadScan::get_picture_page_lelscan(std::string site) {
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

std::string DownloadScan::url_next_page(std::string url) {
    std::string result = this->get_website_content(url);
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
    // get "<a href="https://lelscans.net/scan-dr-stone/1/2" title="Suivant" style="float:right;" >"
    pos = result.find("Suivant");
    if (pos != std::string::npos) {
        result = result.substr(0, pos);
    }
    pos = result.find("href=\"");
    if (pos != std::string::npos) {
        result = result.substr(pos + 6);
    }
    // recupere à partir de "<div id="image">"
    pos = result.find("<div id=\"image\">");
    if (pos != std::string::npos) {
        result = result.substr(pos);
    }
    // recupere le href
    pos = result.find("href=\"");
    if (pos != std::string::npos) {
        result = result.substr(pos + 6);
    }
    // recupere le href
    pos = result.find("\"");
    if (pos != std::string::npos) {
        result = result.substr(0, pos);
    }
    return result;
}

std::string DownloadScan::next_page() {
    return "";
}

std::string DownloadScan::previous_page() {
    return "";
}