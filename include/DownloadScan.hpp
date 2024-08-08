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

/*
 * https://lelscans.net/scan-dr-stone/195/4
 * body/center/div id="image"/table/tbody/tr/td/a/img src="/mangas/dr-stone/195/03.jpg?v=fr1620975509"
 * https://lelscans.net/mangas/dr-stone/195/03.jpg
 * https://lelscans.net/mangas/dr-stone/195/04.jpg
*/

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

    std::string get_picture_page_lelscan(std::string site);
    std::string url_next_page(std::string url);

protected:

private:
    std::string website;
    std::string folder;
    int page;
    int chapitre;

    size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s);
};

#endif // !DOWNLOADSCAN_HPP