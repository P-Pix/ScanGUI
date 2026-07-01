/**
 * @file run_unit_tests.cpp
 * @brief Tests unitaires autonomes des briques non-GUI du projet.
 *
 * Ces tests vérifient les règles de navigation, de parsing et d'accès fichier sans dépendre de GTK
 * ni d'une base PostgreSQL réelle.
 */

#include "application/DownloadQueue.hpp"
#include "application/FileSystemScanDataSource.hpp"
#include "application/OfflineLibrarySync.hpp"
#include "domain/ScanSession.hpp"
#include "infrastructure/JsonScanRepository.hpp"
#include "infrastructure/SimpleJson.hpp"
#include "server/HttpTypes.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

static void touch(const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << "x";
}

static fs::path temp_root(const std::string& suffix = "") {
    auto root = fs::temp_directory_path() / ("scangui-tests-" + std::to_string(::getpid()) + suffix);
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

int main() {
    const std::string json = R"({"title":"Dr \"Stone\"","chapter":12,"favorite":true,"items":[{"id":"a"},{"id":"b"}]})";
    assert(scangui::json::string_field(json, "title") == "Dr \"Stone\"");
    assert(scangui::json::int_field(json, "chapter") == 12);
    assert(scangui::json::extract_bool(json, "favorite").value_or(false));
    assert(scangui::json::extract_object_array(json).size() == 2);

    auto parts = split_path("/api/scans/one%20piece/chapters");
    assert(parts.size() == 4 && parts[2] == "one piece");
    assert(guess_mime_type("1.PNG") == "image/png");

    auto root = temp_root();
    touch(root / "scan-a" / "1" / "1.jpg");
    touch(root / "scan-a" / "1" / "2.png");
    touch(root / "scan-a" / "2" / "1.webp");

    ScanSession session(root / "scan-a");
    assert(session.current_page().has_value());
    assert(session.next_page()->progress.page == 2);
    assert(session.next_page()->progress.chapter == 2);

    JsonScanRepository repo;
    repo.save_progress(root / "scan-a", ScanProgress{2, 1});
    auto loaded = repo.load(root / "scan-a");
    assert(loaded.has_value() && loaded->saveProgress.chapter == 2);

    FileSystemScanDataSource source(root);
    auto scans = source.list_scans();
    assert(scans.size() == 1 && scans[0].pageCount == 3);
    source.set_favorite("scan-a", true, "tester");
    assert(source.list_scans("tester")[0].favorite);
    source.add_bookmark("scan-a", ScanProgress{1, 2}, "note", "tester");
    assert(source.list_bookmarks("scan-a", "tester").size() == 1);
    touch(root / "scan-a" / "ocr" / "1" / "2.txt");
    { std::ofstream txt(root / "scan-a" / "ocr" / "1" / "2.txt"); txt << "recherche speciale dans une bulle"; }
    assert(!source.search_text("speciale", 5).empty());

    auto offlineRoot = temp_root("-offline");
    OfflineLibrarySync offline(source, offlineRoot);
    auto report = offline.sync_all("tester");
    assert(report.scans == 1);
    assert(fs::exists(offlineRoot / "scan-a" / "1" / "1.jpg"));

    std::atomic<int> completed{0};
    std::atomic<int> progressEvents{0};
    DownloadQueue queue([&](const DownloadJobReport& jobReport) {
        assert(jobReport.success);
        ++completed;
    }, [&](const std::string&, int, int, const std::string&) { ++progressEvents; });
    queue.start();
    queue.pause();
    queue.push(DownloadJob{"paused", [&](DownloadJobContext& ctx) { assert(!ctx.cancelled()); ctx.report(1, 1, "paused job"); touch(root / "queue" / "paused.txt"); }, 0});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    assert(queue.paused());
    assert(queue.pending_jobs() == 1);
    assert(!fs::exists(root / "queue" / "paused.txt"));
    queue.resume();
    queue.push(DownloadJob{"unit", [&](DownloadJobContext& ctx) { assert(!ctx.cancelled()); ctx.report(1, 1, "unit job"); touch(root / "queue" / "done.txt"); }, 0});
    for (int i = 0; i < 100 && completed.load() < 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    queue.stop();
    assert(completed.load() == 2);
    assert(progressEvents.load() >= 2);
    assert(fs::exists(root / "queue" / "paused.txt"));
    assert(fs::exists(root / "queue" / "done.txt"));

    fs::remove_all(root);
    fs::remove_all(offlineRoot);
    fs::remove_all("cache/profiles/tester");
    std::cout << "All ScanGUI unit tests passed" << std::endl;
    return 0;
}
