#ifndef DOWNLOADSCAN_HPP
#define DOWNLOADSCAN_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

class DownloadScan {
public:
    DownloadScan();
    DownloadScan(std::string website);
    virtual ~DownloadScan();

    void set_website(std::string website);
    std::string get_website();
    std::string get_website_content(std::string site);

    std::string next_page();
    std::string previous_page();

    void download_picture_page(std::string site, std::string file_name);

    std::string get_next_page_url(std::string url);

protected:

private:
    std::string website;
    std::string folder;
    int page;
    int chapitre;

    std::string url_next_page_lelscan(std::string url);
    std::string download_picture_page_lelscan(std::string site);
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s);
};

#endif // !DOWNLOADSCAN_HPP