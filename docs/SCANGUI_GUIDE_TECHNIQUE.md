# ScanGUI v6 - Guide utilisateur et presentation technique complete
Ce guide documente la version evoluee du projet ScanGUI. Il est ecrit pour un developpeur qui n'a jamais vu le code et doit comprendre l'architecture, les flux d'execution, les classes importantes, les routes API et les limites restantes.
## 1. Resume executif
ScanGUI est maintenant organise comme une application multi-sources : une interface GTK, un serveur HTTP local C++, une base PostgreSQL, une option web locale et une synchronisation offline. Le coeur de l'evolution consiste a faire passer l'application par une abstraction `ScanDataSource`, ce qui permet de lire exactement les memes scans depuis le disque local, depuis le serveur API ou depuis une copie offline.
```text
GUI GTK / Client web / futur CLI
        |
        v
ScanDataSource ou API REST
        |
        +-- FileSystemScanDataSource -> scan/ + cache/profiles
        +-- ApiScanDataSource        -> ScanGUIServer -> PostgreSQL + scan/
        +-- OfflineLibrarySync       -> copie locale lisible hors connexion
```
## 2. Etat des evolutions demandees
| Evolution | Etat dans cette version | Commentaire |
|---|---|---|
| Mode client / serveur local | Mis en place | La GUI propose Local direct, Serveur local et Offline. `ApiScanDataSource` consomme l API, met les pages en cache et sauvegarde la progression. |
| Bibliotheque visuelle | Mis en place | Cartes de scans, recherche, tri, favoris, profils, avancement et bouton reprendre. |
| Experience de lecture | Mis en place | Plein ecran, double page, webtoon, zoom molette, deplacement souris, barre de progression, slider, miniatures, raccourcis, marque-pages, historique. |
| Telechargement en tache de fond | Mis en place | `DownloadQueue` gere worker thread, progression, retry, pause, resume et annulation cooperative. |
| Serveur local securise | Mis en place | Mutex autour du controleur, token admin optionnel, admin localhost uniquement, CORS configurable, endpoint stats/version, controle de chemin image. |
| JSON | Ameliore | `SimpleJson` centralise le parsing sans ajouter de dependance. Une migration vers `nlohmann/json` reste possible mais n est pas imposee. |
| Tests unitaires | Mis en place | `make tests` et `ctest` passent. La suite couvre session, JSON, source locale, offline, queue, favoris, bookmarks et recherche sidecar. |
| Profils | Mis en place | Profils, progression separee, favoris, marque-pages et historique existent cote API/BDD et sont visibles dans GTK. |
| OCR et recherche intelligente | Mis en place version pragmatique | Indexation de fichiers texte sidecar et Tesseract optionnel. Recherche et resume simple. Pas de segmentation IA avancee des bulles. |
| Version web locale | Mis en place | Client HTML/CSS/JS servi par le serveur C++ avec bibliotheque, lecteur, favoris, bookmarks et recherche OCR. |
| Option offline | Mis en place | Synchronisation des scans du serveur vers `scan/` via `OfflineLibrarySync`, puis lecture en mode Offline. |

## 3. Utilisation
### 3.1 Mode local direct
Place les images dans la structure `scan/<nom_du_scan>/<numero_chapitre>/<numero_page>.jpg`. Lance ensuite l'application GTK sur une machine disposant de `gtkmm-3.0` et choisis **Local direct**. La progression, les favoris et les marque-pages locaux sont stockes dans `cache/profiles/<profil>`.
```bash
make
./bin/ScanGUI
```
### 3.2 Mode serveur local
Le serveur lit le dossier `scan/`, indexe la bibliotheque en PostgreSQL et expose une API REST.
```bash
docker compose -f docker-compose.postgres.yml up -d
export SCANGUI_HOST=127.0.0.1
export SCANGUI_PORT=8787
export SCANGUI_SCAN_ROOT=scan
export SCANGUI_ADMIN_TOKEN=dev-token
make server
./bin/ScanGUIServer
```
Puis, depuis un autre terminal, reindexe la bibliotheque :
```bash
curl -X POST -H "x-scangui-admin-token: dev-token" http://127.0.0.1:8787/api/admin/sync
```
Dans GTK, choisis **Serveur local**, renseigne `http://127.0.0.1:8787`, puis clique sur **Actualiser**.
### 3.3 Client web local
Le serveur expose aussi un client web minimal : ouvre `http://127.0.0.1:8787/` dans un navigateur. Le web consomme les memes routes que `ApiScanDataSource`.
### 3.4 Mode offline
En mode **Serveur local**, clique sur **Sync offline**. Le job telecharge/materialise toutes les pages dans `scan/`. Ensuite, bascule la GUI sur **Offline** pour lire sans serveur.
### 3.5 OCR et recherche texte
Le mode OCR le plus fiable consiste a placer un fichier texte a cote de chaque image, par exemple `scan/Serie/1/1.txt` pour `1.jpg`. Lance ensuite :
```bash
curl -X POST -H "x-scangui-admin-token: dev-token" http://127.0.0.1:8787/api/admin/ocr
curl "http://127.0.0.1:8787/api/search?q=mot"
```
Si `SCANGUI_TESSERACT_CMD` est defini, l'indexeur peut aussi appeler un moteur externe. Exemple : `export SCANGUI_TESSERACT_CMD="tesseract {image} stdout -l fra"`.
## 4. Architecture de code
Le projet est separe en couches :

- **Domaine** : `ScanSession`, `ScanProgress`, `ScanMetadata` ;
- **Application** : `ScanDataSource`, sources locale/API, offline sync, queue de jobs ;
- **Interface GTK** : `Main_Window` ;
- **Serveur** : `HttpServer`, `ScanApiController`, `PostgresScanDatabase`, `ScanLibraryIndexer`, `OcrIndexer` ;
- **Web** : client statique `web/` ;
- **Tests** : executable autonome `ScanGUITests`.
## 5. Extraits de code commentes
### 5.1 Contrat commun de donnees : ScanDataSource
Ce contrat est le point d'entree de toute la bascule local / serveur / offline. La fenetre GTK n'a plus besoin de savoir si les pages viennent du disque ou de l'API.

**Fichier : `include/application/ScanDataSource.hpp`, lignes 15-106**

```cpp
0015: #include <filesystem>
0016: #include <string>
0017: #include <vector>
0018: 
0019: struct ScanSummary {
0020:     std::string id;
0021:     std::string title;
0022:     int chapterCount{0};
0023:     int pageCount{0};
0024:     ScanProgress progress{1, 1};
0025:     bool favorite{false};
0026:     std::string lastReadAt;
0027:     std::string coverImageUrl;
0028: 
0029:     [[nodiscard]] double completion_percent() const {
0030:         if (pageCount <= 0 || chapterCount <= 0) return 0.0;
0031:         const int chapter = progress.chapter < 1 ? 1 : progress.chapter;
0032:         const int page = progress.page < 1 ? 1 : progress.page;
0033:         const double averagePagesPerChapter = static_cast<double>(pageCount) / static_cast<double>(chapterCount);
0034:         double readPosition = (static_cast<double>(chapter - 1) * averagePagesPerChapter) + static_cast<double>(page);
0035:         if (readPosition <= 1.0) return 0.0;
0036:         double percent = readPosition / static_cast<double>(pageCount);
0037:         if (percent < 0.0) return 0.0;
0038:         if (percent > 1.0) return 1.0;
0039:         return percent;
0040:     }
0041: };
0042: 
0043: struct ScanPageInfo {
0044:     int chapter{0};
0045:     int page{0};
0046:     std::string mimeType;
0047:     long long sizeBytes{0};
0048:     std::string imageUrl;
0049: };
0050: 
0051: struct ProfileSummary {
0052:     std::string id;
0053:     std::string displayName;
0054:     std::string avatarColor{"#3b82f6"};
0055: };
0056: 
0057: struct BookmarkSummary {
0058:     long long id{0};
0059:     std::string scanId;
0060:     int chapter{1};
0061:     int page{1};
0062:     std::string note;
0063:     std::string createdAt;
0064: };
0065: 
0066: struct HistoryEntry {
0067:     std::string scanId;
0068:     std::string title;
0069:     int chapter{1};
0070:     int page{1};
0071:     std::string readAt;
0072: };
0073: 
0074: struct SearchResultSummary {
0075:     std::string scanId;
0076:     std::string title;
0077:     int chapter{1};
0078:     int page{1};
0079:     std::string snippet;
0080:     std::string source;
0081: };
0082: 
0083: class ScanDataSource {
0084: public:
0085:     virtual ~ScanDataSource() = default;
0086: 
0087:     [[nodiscard]] virtual std::vector<ScanSummary> list_scans(const std::string& profile = "default") const = 0;
0088:     [[nodiscard]] virtual std::vector<int> list_chapters(const std::string& scanId) const = 0;
0089:     [[nodiscard]] virtual std::vector<ScanPageInfo> list_pages(const std::string& scanId, int chapter) const = 0;
0090:     [[nodiscard]] virtual std::filesystem::path materialize_page(const std::string& scanId, ScanProgress progress) const = 0;
0091:     [[nodiscard]] virtual ScanProgress load_progress(const std::string& scanId, const std::string& profile = "default") const = 0;
0092:     virtual void save_progress(const std::string& scanId, ScanProgress progress, const std::string& profile = "default") const = 0;
0093: 
0094:     [[nodiscard]] virtual std::vector<ProfileSummary> list_profiles() const { return {ProfileSummary{"default", "Default", "#3b82f6"}}; }
0095:     virtual void save_profile(const ProfileSummary&) const {}
0096:     [[nodiscard]] virtual std::vector<std::string> list_favorites(const std::string&) const { return {}; }
0097:     virtual void set_favorite(const std::string&, bool, const std::string& = "default") const {}
0098:     [[nodiscard]] virtual std::vector<BookmarkSummary> list_bookmarks(const std::string&, const std::string& = "default") const { return {}; }
0099:     virtual BookmarkSummary add_bookmark(const std::string& scanId, ScanProgress progress, const std::string& note, const std::string& profile = "default") const {
0100:         (void)profile;
0101:         return BookmarkSummary{0, scanId, progress.chapter, progress.page, note, ""};
0102:     }
0103:     [[nodiscard]] virtual std::vector<HistoryEntry> list_history(const std::string& = "default", int = 20) const { return {}; }
0104:     [[nodiscard]] virtual std::vector<SearchResultSummary> search_text(const std::string&, int = 20) const { return {}; }
0105: };
0106: 
```
### 5.2 Etat et widgets principaux de la fenetre GTK
La fenetre conserve la source active, les scans filtres, les options de lecture et les widgets de bibliotheque.

**Fichier : `include/Main_Window.hpp`, lignes 72-199**

```cpp
0072:     Gtk::Box m_VBox;
0073:     Gtk::MenuBar m_MenuBar;
0074: 
0075:     Gtk::Grid m_Grid;
0076:     Gtk::Box m_LeftPanel;
0077:     Gtk::Box m_LibraryToolbar;
0078:     Gtk::ComboBoxText m_ModeCombo;
0079:     Gtk::ComboBoxText m_ProfileCombo;
0080:     Gtk::Entry m_ServerUrlEntry;
0081:     Gtk::SearchEntry m_SearchEntry;
0082:     Gtk::ComboBoxText m_SortCombo;
0083:     Gtk::CheckButton m_FavoritesOnly;
0084:     Gtk::Button m_ButtonRefresh;
0085:     Gtk::Button m_ButtonResume;
0086:     Gtk::Button m_ButtonOfflineSync;
0087:     Gtk::Button m_ButtonCreateProfile;
0088:     Gtk::Button m_ButtonHistory;
0089:     Gtk::Button m_ButtonSearchText;
0090:     Gtk::Button m_ButtonBookmarks;
0091:     Gtk::ProgressBar m_DownloadProgress;
0092:     Gtk::Label m_DownloadStatus;
0093: 
0094:     std::vector<Gtk::Button*> m_ListButtonManga;
0095:     Gtk::Grid m_GridListeManga;
0096:     Gtk::ScrolledWindow m_ScrollListeManga;
0097: 
0098:     Gtk::Grid m_GridImage_go_to;
0099:     Gtk::ScrolledWindow m_ScrollImage;
0100:     Gtk::Stack m_ReaderStack;
0101:     Gtk::Box m_PageHBox;
0102:     Gtk::Box m_WebtoonBox;
0103:     std::vector<Gtk::Widget*> m_WebtoonImages;
0104: 
0105:     Gtk::Grid m_go_to;
0106:     Gtk::Entry m_Entry_page;
0107:     Gtk::Entry m_Entry_chapter;
0108:     Gtk::Button m_Button_go_to;
0109:     Gtk::Button m_ButtonFavorite;
0110:     Gtk::Button m_ButtonBookmark;
0111:     Gtk::Button m_ButtonShortcuts;
0112:     Gtk::ProgressBar m_ChapterProgress;
0113:     Gtk::Scale m_PageSlider;
0114:     Gtk::ScrolledWindow m_ThumbScroll;
0115:     Gtk::Box m_ThumbBox;
0116:     std::vector<Gtk::Widget*> m_ThumbButtons;
0117: 
0118:     Gtk::MenuItem m_File;
0119:     Gtk::Menu m_FileMenu;
0120:     Gtk::MenuItem m_FileOpen;
0121:     Gtk::MenuItem m_FileSave;
0122:     Gtk::MenuItem m_FileQuit;
0123: 
0124:     Gtk::MenuItem m_Download;
0125:     Gtk::Menu m_DownloadMenu;
0126:     Gtk::MenuItem m_UpdateScan;
0127:     Gtk::MenuItem m_UpdateAllScan;
0128:     Gtk::MenuItem m_NewScan;
0129:     Gtk::MenuItem m_OfflineSyncMenu;
0130:     Gtk::MenuItem m_PauseJobsMenu;
0131:     Gtk::MenuItem m_ResumeJobsMenu;
0132:     Gtk::MenuItem m_CancelJobsMenu;
0133: 
0134:     Gtk::MenuItem m_View;
0135:     Gtk::Menu m_ViewMenu;
0136:     Gtk::CheckMenuItem m_DoublePageMenu;
0137:     Gtk::CheckMenuItem m_WebtoonMenu;
0138:     Gtk::MenuItem m_FullscreenMenu;
0139: 
0140:     Gtk::MenuItem m_Help;
0141:     Gtk::Menu m_HelpMenu;
0142:     Gtk::MenuItem m_ShortcutsMenu;
0143: 
0144:     Scan m_Scan;
0145:     Scan m_SecondScan;
0146:     Gtk::EventBox m_EventBox;
0147:     Gtk::EventBox m_SecondEventBox;
0148: 
0149:     JsonScanRepository repository_;
0150:     CurlHttpClient httpClient_;
0151:     LelScansProvider provider_;
0152: 
0153:     void on_menu_file_open();
0154:     void on_menu_file_save();
0155:     void on_menu_file_quit();
0156:     void on_menu_download_update();
0157:     void on_menu_download_new();
0158:     void update_all_scan();
0159:     void update_scan(const std::filesystem::path& folder);
0160:     void pause_download_queue();
0161:     void resume_download_queue();
0162:     void cancel_download_queue();
0163: 
0164:     void configure_data_source();
0165:     [[nodiscard]] ScanDataSource& active_source();
0166:     [[nodiscard]] const ScanDataSource& active_source() const;
0167:     void on_mode_changed();
0168:     void on_profile_changed();
0169:     void create_profile_dialog();
0170:     void refresh_profile_list();
0171:     void refresh_scan_list();
0172:     [[nodiscard]] std::vector<ScanSummary> filtered_sorted_scans() const;
0173:     void open_scan(const std::filesystem::path& folder);
0174:     void open_scan_by_id(const std::string& scanId, ScanProgress progress = {1, 1});
0175:     void go_to();
0176: 
0177:     void navigate_next();
0178:     void navigate_previous();
0179:     [[nodiscard]] std::optional<ScanProgress> next_progress() const;
0180:     [[nodiscard]] std::optional<ScanProgress> previous_progress() const;
0181:     [[nodiscard]] int current_page_index() const;
0182:     [[nodiscard]] int current_chapter_index() const;
0183:     void save_current_progress();
0184:     void render_current_page();
0185:     void render_webtoon_chapter();
0186:     void rebuild_thumbnails();
0187:     void refresh_progress_placeholders();
0188:     void on_page_slider_changed();
0189:     void toggle_favorite();
0190:     void add_bookmark();
0191:     void show_bookmarks();
0192:     void show_history();
0193:     void show_text_search();
0194:     void resume_last_reading();
0195:     void synchronize_offline();
0196:     void on_download_progress(const std::string& jobId, int current, int total, const std::string& message);
0197:     void show_shortcuts();
0198:     void set_fullscreen_mode();
0199:     void set_double_page_mode(bool enabled);
```
### 5.3 Initialisation des callbacks de telechargement
La file de telechargement publie son etat vers la GUI sans bloquer le thread graphique.

**Fichier : `src/Main_Window.cpp`, lignes 108-126**

```cpp
0108:         Glib::signal_idle().connect([this, report]() {
0109:             if (report.success) {
0110:                 m_DownloadProgress.set_fraction(1.0);
0111:                 m_DownloadStatus.set_text("Termine : " + report.id);
0112:                 refresh_scan_list();
0113:                 show_info("Tache terminee", "La tache " + report.id + " est terminee apres " + std::to_string(report.attempts) + " tentative(s).");
0114:             } else if (report.cancelled) {
0115:                 m_DownloadStatus.set_text("Annule : " + report.id);
0116:                 show_info("Tache annulee", "La tache " + report.id + " a ete annulee proprement.");
0117:             } else {
0118:                 m_DownloadStatus.set_text("Erreur : " + report.id);
0119:                 show_error("Tache en echec", "La tache " + report.id + " a echoue : " + report.error);
0120:             }
0121:             return false;
0122:         });
0123:     }, [this](const std::string& jobId, int current, int total, const std::string& message) {
0124:         on_download_progress(jobId, current, total, message);
0125:     });
0126:     downloadQueue_->start();
```
### 5.4 Barre superieure : mode local, serveur, offline
Ces widgets donnent enfin un acces visible au mode serveur et au mode offline.

**Fichier : `src/Main_Window.cpp`, lignes 190-236**

```cpp
0190:     m_ModeCombo.append_text("Serveur local");
0191:     m_ModeCombo.append_text("Offline");
0192:     m_ModeCombo.set_active(0);
0193:     m_ModeCombo.signal_changed().connect(sigc::mem_fun(*this, &Main_Window::on_mode_changed));
0194: 
0195:     m_ServerUrlEntry.set_text("http://127.0.0.1:8787");
0196:     m_ServerUrlEntry.set_placeholder_text("URL serveur");
0197:     m_SearchEntry.set_placeholder_text("Rechercher un scan");
0198:     m_SearchEntry.signal_search_changed().connect(sigc::mem_fun(*this, &Main_Window::refresh_scan_list));
0199:     m_SortCombo.append_text("Titre");
0200:     m_SortCombo.append_text("Derniere lecture");
0201:     m_SortCombo.append_text("Avancement");
0202:     m_SortCombo.append_text("Chapitres");
0203:     m_SortCombo.append_text("Pages");
0204:     m_SortCombo.set_active(0);
0205:     m_SortCombo.signal_changed().connect(sigc::mem_fun(*this, &Main_Window::refresh_scan_list));
0206:     m_ProfileCombo.signal_changed().connect(sigc::mem_fun(*this, &Main_Window::on_profile_changed));
0207:     m_FavoritesOnly.signal_toggled().connect(sigc::mem_fun(*this, &Main_Window::refresh_scan_list));
0208:     m_ButtonRefresh.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::refresh_scan_list));
0209:     m_ButtonResume.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::resume_last_reading));
0210:     m_ButtonOfflineSync.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::synchronize_offline));
0211:     m_ButtonCreateProfile.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::create_profile_dialog));
0212:     m_ButtonHistory.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::show_history));
0213:     m_ButtonSearchText.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::show_text_search));
0214:     m_ButtonBookmarks.signal_clicked().connect(sigc::mem_fun(*this, &Main_Window::show_bookmarks));
0215:     m_DownloadProgress.set_show_text(true);
0216:     m_DownloadProgress.set_text("0 %");
0217: 
0218:     m_LibraryToolbar.set_spacing(6);
0219:     m_LibraryToolbar.pack_start(m_ModeCombo, Gtk::PACK_SHRINK);
0220:     m_LibraryToolbar.pack_start(m_ServerUrlEntry, Gtk::PACK_SHRINK);
0221:     m_LibraryToolbar.pack_start(m_ProfileCombo, Gtk::PACK_SHRINK);
0222:     m_LibraryToolbar.pack_start(m_SearchEntry, Gtk::PACK_SHRINK);
0223:     m_LibraryToolbar.pack_start(m_SortCombo, Gtk::PACK_SHRINK);
0224:     m_LibraryToolbar.pack_start(m_FavoritesOnly, Gtk::PACK_SHRINK);
0225:     m_LibraryToolbar.pack_start(m_ButtonRefresh, Gtk::PACK_SHRINK);
0226:     m_LibraryToolbar.pack_start(m_ButtonResume, Gtk::PACK_SHRINK);
0227:     m_LibraryToolbar.pack_start(m_ButtonOfflineSync, Gtk::PACK_SHRINK);
0228:     m_LibraryToolbar.pack_start(m_ButtonCreateProfile, Gtk::PACK_SHRINK);
0229:     m_LibraryToolbar.pack_start(m_ButtonHistory, Gtk::PACK_SHRINK);
0230:     m_LibraryToolbar.pack_start(m_ButtonSearchText, Gtk::PACK_SHRINK);
0231:     m_LibraryToolbar.pack_start(m_ButtonBookmarks, Gtk::PACK_SHRINK);
0232:     m_LibraryToolbar.pack_start(m_DownloadProgress, Gtk::PACK_SHRINK);
0233:     m_LibraryToolbar.pack_start(m_DownloadStatus, Gtk::PACK_SHRINK);
0234: 
0235:     m_ScrollListeManga.set_size_request(360, HEIGHT);
0236:     m_ScrollListeManga.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
```
### 5.5 Choix dynamique de la source active
La GUI instancie FileSystemScanDataSource ou ApiScanDataSource selon le choix utilisateur.

**Fichier : `src/Main_Window.cpp`, lignes 301-315**

```cpp
0301:     const Glib::ustring modeText = m_ModeCombo.get_active_text();
0302:     if (modeText == "Serveur local") {
0303:         mode_ = DataMode::Server;
0304:         dataSource_ = std::make_unique<ApiScanDataSource>(m_ServerUrlEntry.get_text().raw(), "cache/api");
0305:     } else if (modeText == "Offline") {
0306:         mode_ = DataMode::Offline;
0307:         dataSource_ = std::make_unique<FileSystemScanDataSource>("scan");
0308:     } else {
0309:         mode_ = DataMode::Local;
0310:         dataSource_ = std::make_unique<FileSystemScanDataSource>("scan");
0311:     }
0312:     refresh_profile_list();
0313:     refresh_scan_list();
0314: }
0315: 
```
### 5.6 Bibliotheque : recherche, tri, favoris, cartes
La bibliotheque reconstruit des cartes de scan avec avancement, favoris et bouton reprendre.

**Fichier : `src/Main_Window.cpp`, lignes 383-461**

```cpp
0383:     auto scans = active_source().list_scans(currentProfile_);
0384:     const std::string query = lower_ascii(m_SearchEntry.get_text().raw());
0385:     const bool favoritesOnly = m_FavoritesOnly.get_active();
0386:     scans.erase(std::remove_if(scans.begin(), scans.end(), [&](const ScanSummary& scan) {
0387:         if (favoritesOnly && !scan.favorite) return true;
0388:         if (!query.empty() && lower_ascii(scan.title).find(query) == std::string::npos && lower_ascii(scan.id).find(query) == std::string::npos) return true;
0389:         return false;
0390:     }), scans.end());
0391: 
0392:     const std::string sort = m_SortCombo.get_active_text().raw();
0393:     if (sort == "Derniere lecture") {
0394:         std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.lastReadAt, a.title) > std::tie(b.lastReadAt, b.title); });
0395:     } else if (sort == "Avancement") {
0396:         std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.progress.chapter, a.progress.page, a.title) > std::tie(b.progress.chapter, b.progress.page, b.title); });
0397:     } else if (sort == "Chapitres") {
0398:         std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.chapterCount, a.title) > std::tie(b.chapterCount, b.title); });
0399:     } else if (sort == "Pages") {
0400:         std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.pageCount, a.title) > std::tie(b.pageCount, b.title); });
0401:     } else {
0402:         std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return a.title < b.title; });
0403:     }
0404:     return scans;
0405: }
0406: 
0407: void Main_Window::refresh_scan_list() {
0408:     for (auto* button : m_ListButtonManga) m_GridListeManga.remove(*button);
0409:     m_ListButtonManga.clear();
0410: 
0411:     std::vector<ScanSummary> scans;
0412:     try { scans = filtered_sorted_scans(); } catch (const std::exception& error) { show_error("Bibliotheque indisponible", error.what()); return; }
0413: 
0414:     int row = 0;
0415:     int col = 0;
0416:     constexpr int columns = 2;
0417:     for (const auto& scan : scans) {
0418:         auto* button = Gtk::make_managed<Gtk::Button>();
0419:         auto* card = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
0420:         card->set_spacing(4);
0421:         card->set_margin_top(6);
0422:         card->set_margin_bottom(6);
0423:         card->set_margin_left(6);
0424:         card->set_margin_right(6);
0425: 
0426:         try {
0427:             auto chapters = active_source().list_chapters(scan.id);
0428:             if (!chapters.empty()) {
0429:                 auto pages = active_source().list_pages(scan.id, chapters.front());
0430:                 if (!pages.empty()) {
0431:                     const auto coverPath = active_source().materialize_page(scan.id, ScanProgress{pages.front().chapter, pages.front().page});
0432:                     auto pixbuf = Gdk::Pixbuf::create_from_file(coverPath.string(), 120, 160, true);
0433:                     auto* image = Gtk::make_managed<Gtk::Image>(pixbuf);
0434:                     card->pack_start(*image, Gtk::PACK_SHRINK);
0435:                 }
0436:             }
0437:         } catch (...) {
0438:             auto* noCover = Gtk::make_managed<Gtk::Label>("Pas de couverture");
0439:             card->pack_start(*noCover, Gtk::PACK_SHRINK);
0440:         }
0441: 
0442:         std::ostringstream label;
0443:         label << (scan.favorite ? "★ " : "") << scan.title << "\n"
0444:               << scan.chapterCount << " chap. - " << scan.pageCount << " pages\n"
0445:               << "Progression : ch. " << scan.progress.chapter << ", p. " << scan.progress.page << "\n"
0446:               << percent_label(scan);
0447:         if (!scan.lastReadAt.empty()) label << "\nDerniere lecture : " << scan.lastReadAt;
0448:         label << "\nCliquer pour reprendre";
0449: 
0450:         auto* text = Gtk::make_managed<Gtk::Label>(label.str());
0451:         text->set_line_wrap(true);
0452:         text->set_justify(Gtk::JUSTIFY_CENTER);
0453:         card->pack_start(*text, Gtk::PACK_SHRINK);
0454: 
0455:         auto* progress = Gtk::make_managed<Gtk::ProgressBar>();
0456:         progress->set_show_text(true);
0457:         const double completion = scan.completion_percent();
0458:         progress->set_fraction(std::clamp(completion, 0.0, 1.0));
0459:         progress->set_text(completion <= 0.0 ? "0 %" : "en cours");
0460:         card->pack_start(*progress, Gtk::PACK_SHRINK);
0461: 
```
### 5.7 Lecture : page simple, double page, webtoon, zoom
Le lecteur gere plusieurs modes d'affichage tout en gardant une source unique de pages.

**Fichier : `src/Main_Window.cpp`, lignes 540-609**

```cpp
0540:     if (currentScanId_.empty()) return;
0541:     currentPages_ = active_source().list_pages(currentScanId_, currentProgress_.chapter);
0542:     if (currentPages_.empty()) throw std::runtime_error("Chapitre vide ou introuvable.");
0543:     if (std::none_of(currentPages_.begin(), currentPages_.end(), [&](const auto& p) { return p.page == currentProgress_.page; })) currentProgress_.page = currentPages_.front().page;
0544: 
0545:     if (webtoonMode_) {
0546:         render_webtoon_chapter();
0547:     } else {
0548:         m_ReaderStack.set_visible_child("paged");
0549:         const auto pagePath = active_source().materialize_page(currentScanId_, currentProgress_);
0550:         m_Scan.set_page(pagePath);
0551:         if (doublePageMode_) {
0552:             if (auto second = next_progress()) {
0553:                 try { m_SecondScan.set_page(active_source().materialize_page(currentScanId_, *second)); m_SecondEventBox.show(); }
0554:                 catch (...) { m_SecondEventBox.hide(); }
0555:             } else {
0556:                 m_SecondEventBox.hide();
0557:             }
0558:         } else {
0559:             m_SecondEventBox.hide();
0560:         }
0561:     }
0562: 
0563:     save_current_progress();
0564:     rebuild_thumbnails();
0565:     refresh_progress_placeholders();
0566: }
0567: 
0568: void Main_Window::render_webtoon_chapter() {
0569:     for (auto* widget : m_WebtoonImages) m_WebtoonBox.remove(*widget);
0570:     m_WebtoonImages.clear();
0571:     for (const auto& page : currentPages_) {
0572:         try {
0573:             auto path = active_source().materialize_page(currentScanId_, ScanProgress{page.chapter, page.page});
0574:             auto* image = Gtk::make_managed<Gtk::Image>(path.string());
0575:             image->set_margin_bottom(12);
0576:             m_WebtoonBox.pack_start(*image, Gtk::PACK_SHRINK);
0577:             m_WebtoonImages.push_back(image);
0578:         } catch (...) {}
0579:     }
0580:     m_ReaderStack.set_visible_child("webtoon");
0581:     m_WebtoonBox.show_all();
0582: }
0583: 
0584: void Main_Window::rebuild_thumbnails() {
0585:     for (auto* widget : m_ThumbButtons) m_ThumbBox.remove(*widget);
0586:     m_ThumbButtons.clear();
0587:     for (const auto& page : currentPages_) {
0588:         auto* button = Gtk::make_managed<Gtk::Button>();
0589:         auto* box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
0590:         try {
0591:             const auto path = active_source().materialize_page(currentScanId_, ScanProgress{page.chapter, page.page});
0592:             auto pixbuf = Gdk::Pixbuf::create_from_file(path.string(), 56, 74, true);
0593:             auto* image = Gtk::make_managed<Gtk::Image>(pixbuf);
0594:             box->pack_start(*image, Gtk::PACK_SHRINK);
0595:         } catch (...) {}
0596:         auto* label = Gtk::make_managed<Gtk::Label>(std::to_string(page.page));
0597:         box->pack_start(*label, Gtk::PACK_SHRINK);
0598:         button->add(*box);
0599:         button->signal_clicked().connect([this, page]() { currentProgress_ = {page.chapter, page.page}; render_current_page(); });
0600:         m_ThumbBox.pack_start(*button, Gtk::PACK_SHRINK);
0601:         m_ThumbButtons.push_back(button);
0602:     }
0603:     m_ThumbBox.show_all();
0604: }
0605: 
0606: void Main_Window::save_current_progress() {
0607:     if (currentScanId_.empty()) return;
0608:     try { active_source().save_progress(currentScanId_, currentProgress_, currentProfile_); } catch (const std::exception& error) { std::cerr << "Sauvegarde impossible: " << error.what() << std::endl; }
0609: }
```
### 5.8 Marque-pages, historique et recherche OCR dans GTK
Ces dialogues consomment les methodes ajoutees au contrat ScanDataSource.

**Fichier : `src/Main_Window.cpp`, lignes 649-738**

```cpp
0649:     if (currentScanId_.empty()) { show_error("Aucun scan ouvert", "Ouvre un scan avant de consulter les marque-pages."); return; }
0650:     try {
0651:         const auto bookmarks = active_source().list_bookmarks(currentScanId_, currentProfile_);
0652:         Gtk::Dialog dialog("Marque-pages", *this, true);
0653:         dialog.add_button("Fermer", Gtk::RESPONSE_CLOSE);
0654:         auto* area = dialog.get_content_area();
0655:         if (bookmarks.empty()) {
0656:             auto* label = Gtk::make_managed<Gtk::Label>("Aucun marque-page pour ce scan.");
0657:             area->pack_start(*label, Gtk::PACK_SHRINK);
0658:         }
0659:         for (const auto& bookmark : bookmarks) {
0660:             std::ostringstream text;
0661:             text << "Chapitre " << bookmark.chapter << ", page " << bookmark.page;
0662:             if (!bookmark.note.empty()) text << " - " << bookmark.note;
0663:             auto* button = Gtk::make_managed<Gtk::Button>(text.str());
0664:             button->signal_clicked().connect([this, &dialog, bookmark]() {
0665:                 currentProgress_ = ScanProgress{bookmark.chapter, bookmark.page};
0666:                 dialog.response(Gtk::RESPONSE_OK);
0667:             });
0668:             area->pack_start(*button, Gtk::PACK_SHRINK);
0669:         }
0670:         dialog.show_all_children();
0671:         if (dialog.run() == Gtk::RESPONSE_OK) render_current_page();
0672:     } catch (const std::exception& error) { show_error("Marque-pages indisponibles", error.what()); }
0673: }
0674: 
0675: void Main_Window::show_history() {
0676:     try {
0677:         const auto history = active_source().list_history(currentProfile_, 20);
0678:         Gtk::Dialog dialog("Historique de lecture", *this, true);
0679:         dialog.add_button("Fermer", Gtk::RESPONSE_CLOSE);
0680:         auto* area = dialog.get_content_area();
0681:         if (history.empty()) {
0682:             auto* label = Gtk::make_managed<Gtk::Label>("Aucun historique pour ce profil.");
0683:             area->pack_start(*label, Gtk::PACK_SHRINK);
0684:         }
0685:         for (const auto& item : history) {
0686:             std::ostringstream text;
0687:             text << item.title << " - ch. " << item.chapter << ", p. " << item.page;
0688:             if (!item.readAt.empty()) text << " - " << item.readAt;
0689:             auto* button = Gtk::make_managed<Gtk::Button>(text.str());
0690:             button->signal_clicked().connect([this, &dialog, item]() {
0691:                 open_scan_by_id(item.scanId, ScanProgress{item.chapter, item.page});
0692:                 dialog.response(Gtk::RESPONSE_OK);
0693:             });
0694:             area->pack_start(*button, Gtk::PACK_SHRINK);
0695:         }
0696:         dialog.show_all_children();
0697:         (void)dialog.run();
0698:     } catch (const std::exception& error) { show_error("Historique indisponible", error.what()); }
0699: }
0700: 
0701: void Main_Window::show_text_search() {
0702:     Gtk::Dialog input("Recherche OCR / texte", *this, true);
0703:     input.add_button("Annuler", Gtk::RESPONSE_CANCEL);
0704:     input.add_button("Rechercher", Gtk::RESPONSE_OK);
0705:     Gtk::Entry queryEntry;
0706:     queryEntry.set_placeholder_text("Mot ou expression a chercher dans les pages OCR");
0707:     queryEntry.set_text(m_SearchEntry.get_text());
0708:     input.get_content_area()->pack_start(queryEntry);
0709:     input.show_all_children();
0710:     if (input.run() != Gtk::RESPONSE_OK) return;
0711:     const std::string query = queryEntry.get_text().raw();
0712:     if (query.empty()) return;
0713: 
0714:     try {
0715:         const auto results = active_source().search_text(query, 30);
0716:         Gtk::Dialog dialog("Resultats OCR", *this, true);
0717:         dialog.add_button("Fermer", Gtk::RESPONSE_CLOSE);
0718:         auto* area = dialog.get_content_area();
0719:         if (results.empty()) {
0720:             auto* label = Gtk::make_managed<Gtk::Label>("Aucun resultat indexe. Lance /api/admin/ocr ou ajoute des fichiers .txt sidecar.");
0721:             label->set_line_wrap(true);
0722:             area->pack_start(*label, Gtk::PACK_SHRINK);
0723:         }
0724:         for (const auto& result : results) {
0725:             std::ostringstream text;
0726:             text << result.title << " - ch. " << result.chapter << ", p. " << result.page << "\n" << result.snippet;
0727:             auto* button = Gtk::make_managed<Gtk::Button>(text.str());
0728:             button->signal_clicked().connect([this, &dialog, result]() {
0729:                 open_scan_by_id(result.scanId, ScanProgress{result.chapter, result.page});
0730:                 dialog.response(Gtk::RESPONSE_OK);
0731:             });
0732:             area->pack_start(*button, Gtk::PACK_SHRINK);
0733:         }
0734:         dialog.show_all_children();
0735:         (void)dialog.run();
0736:     } catch (const std::exception& error) { show_error("Recherche impossible", error.what()); }
0737: }
0738: 
```
### 5.9 Synchronisation offline depuis le serveur
Le bouton offline lance un job de synchronisation via DownloadQueue et OfflineLibrarySync.

**Fichier : `src/Main_Window.cpp`, lignes 740-778**

```cpp
0740:     Glib::signal_idle().connect([this, jobId, current, total, message]() {
0741:         if (total > 0) {
0742:             const double fraction = std::clamp(static_cast<double>(current) / static_cast<double>(total), 0.0, 1.0);
0743:             m_DownloadProgress.set_fraction(fraction);
0744:             m_DownloadProgress.set_text(std::to_string(static_cast<int>(std::round(fraction * 100.0))) + " %");
0745:         } else {
0746:             m_DownloadProgress.pulse();
0747:             m_DownloadProgress.set_text("en cours");
0748:         }
0749:         m_DownloadStatus.set_text(jobId + " : " + message);
0750:         return false;
0751:     });
0752: }
0753: 
0754: void Main_Window::resume_last_reading() {
0755:     try {
0756:         const auto history = active_source().list_history(currentProfile_, 1);
0757:         if (!history.empty()) { open_scan_by_id(history.front().scanId, ScanProgress{history.front().chapter, history.front().page}); return; }
0758:         const auto scans = active_source().list_scans(currentProfile_);
0759:         if (!scans.empty()) open_scan_by_id(scans.front().id, scans.front().progress);
0760:     } catch (const std::exception& error) { show_error("Reprise impossible", error.what()); }
0761: }
0762: 
0763: void Main_Window::synchronize_offline() {
0764:     const std::string baseUrl = m_ServerUrlEntry.get_text().raw();
0765:     const std::string profile = currentProfile_;
0766:     downloadQueue_->push(DownloadJob{"offline-sync", [baseUrl, profile](DownloadJobContext& ctx) {
0767:         ApiScanDataSource api(baseUrl, "cache/api");
0768:         OfflineLibrarySync sync(api, "scan");
0769:         auto report = sync.sync_all(profile, [&](const OfflineSyncProgress& progress) {
0770:             ctx.report(progress.downloadedPages + progress.skippedPages + progress.failedPages, 0,
0771:                        "scan=" + progress.scanId + " ch=" + std::to_string(progress.chapter) + " p=" + std::to_string(progress.page));
0772:             ctx.wait_if_paused();
0773:             if (ctx.cancelled()) throw std::runtime_error("offline sync cancelled");
0774:         });
0775:         ctx.report(report.downloadedPages, report.downloadedPages + report.skippedPages + report.failedPages, "synchronisation offline terminee");
0776:     }, 1});
0777:     show_info("Synchronisation lancee", "Le telechargement offline est place dans la file de taches. Les fichiers seront copies dans ./scan.");
0778: }
```
### 5.10 File de jobs asynchrones
DownloadQueue expose pause, reprise, annulation, progression et retry sans dependance GTK.

**Fichier : `include/application/DownloadQueue.hpp`, lignes 13-93**

```cpp
0013: 
0014: /**
0015:  * @brief Contexte fourni a une tache de telechargement.
0016:  *
0017:  * La tache peut tester `cancelled()` entre deux operations longues et publier une progression.
0018:  * L'annulation reste cooperative : la file ne tue jamais brutalement un thread en cours.
0019:  */
0020: struct DownloadJobContext {
0021:     std::atomic<bool>& cancelRequested;
0022:     std::function<bool()> pauseRequested;
0023:     std::function<void()> waitWhilePaused;
0024:     std::function<void(int current, int total, const std::string& message)> progress;
0025: 
0026:     [[nodiscard]] bool cancelled() const { return cancelRequested.load(); }
0027:     [[nodiscard]] bool paused() const { return pauseRequested ? pauseRequested() : false; }
0028:     void wait_if_paused() const {
0029:         if (waitWhilePaused) waitWhilePaused();
0030:     }
0031:     void report(int current, int total, const std::string& message) const {
0032:         if (progress) progress(current, total, message);
0033:     }
0034: };
0035: 
0036: struct DownloadJob {
0037:     std::string id;
0038:     std::function<void(DownloadJobContext&)> work;
0039:     int maxRetries{2};
0040: };
0041: 
0042: struct DownloadJobReport {
0043:     std::string id;
0044:     bool success{false};
0045:     bool cancelled{false};
0046:     int attempts{0};
0047:     std::string error;
0048: };
0049: 
0050: /**
0051:  * @brief Petite file de travaux en arriere-plan pour synchronisations, mises a jour et OCR.
0052:  *
0053:  * La file ne connait pas le contenu des travaux : elle execute des callbacks. Cette separation
0054:  * permet de brancher la synchronisation offline, la mise a jour de scans ou une indexation OCR
0055:  * sans coupler la GUI aux details reseau ou disque.
0056:  */
0057: class DownloadQueue {
0058: public:
0059:     using CompletionCallback = std::function<void(const DownloadJobReport&)>;
0060:     using ProgressCallback = std::function<void(const std::string& jobId, int current, int total, const std::string& message)>;
0061: 
0062:     explicit DownloadQueue(CompletionCallback onComplete = {}, ProgressCallback onProgress = {});
0063:     ~DownloadQueue();
0064: 
0065:     void start();
0066:     void stop();
0067:     void cancel();
0068:     void pause();
0069:     void resume();
0070:     void clear();
0071:     void push(DownloadJob job);
0072: 
0073:     [[nodiscard]] bool running() const;
0074:     [[nodiscard]] bool paused() const;
0075:     [[nodiscard]] bool cancellation_requested() const;
0076:     [[nodiscard]] std::size_t pending_jobs() const;
0077: 
0078: private:
0079:     CompletionCallback onComplete_;
0080:     ProgressCallback onProgress_;
0081:     std::queue<DownloadJob> jobs_;
0082:     mutable std::mutex mutex_;
0083:     std::condition_variable condition_;
0084:     std::thread worker_;
0085:     std::atomic<bool> running_{false};
0086:     std::atomic<bool> stopRequested_{false};
0087:     std::atomic<bool> cancelRequested_{false};
0088:     std::atomic<bool> pauseRequested_{false};
0089: 
0090:     void loop();
0091: };
0092: 
0093: #endif
```
### 5.11 Execution thread-safe des jobs
La boucle worker attend la reprise, respecte l'annulation et remonte un rapport de fin.

**Fichier : `src/application/DownloadQueue.cpp`, lignes 36-115**

```cpp
0036: void DownloadQueue::resume() {
0037:     pauseRequested_ = false;
0038:     condition_.notify_all();
0039: }
0040: 
0041: void DownloadQueue::clear() {
0042:     std::lock_guard<std::mutex> lock(mutex_);
0043:     std::queue<DownloadJob> empty;
0044:     std::swap(jobs_, empty);
0045: }
0046: 
0047: void DownloadQueue::push(DownloadJob job) {
0048:     {
0049:         std::lock_guard<std::mutex> lock(mutex_);
0050:         jobs_.push(std::move(job));
0051:     }
0052:     cancelRequested_ = false;
0053:     condition_.notify_one();
0054: }
0055: 
0056: bool DownloadQueue::running() const { return running_; }
0057: bool DownloadQueue::paused() const { return pauseRequested_; }
0058: bool DownloadQueue::cancellation_requested() const { return cancelRequested_; }
0059: 
0060: std::size_t DownloadQueue::pending_jobs() const {
0061:     std::lock_guard<std::mutex> lock(mutex_);
0062:     return jobs_.size();
0063: }
0064: 
0065: void DownloadQueue::loop() {
0066:     while (!stopRequested_) {
0067:         DownloadJob job;
0068:         {
0069:             std::unique_lock<std::mutex> lock(mutex_);
0070:             condition_.wait(lock, [this] {
0071:                 return stopRequested_ || (!pauseRequested_ && !jobs_.empty());
0072:             });
0073:             if (stopRequested_) break;
0074:             if (jobs_.empty()) continue;
0075:             job = std::move(jobs_.front());
0076:             jobs_.pop();
0077:         }
0078: 
0079:         DownloadJobReport report;
0080:         report.id = job.id;
0081:         for (int attempt = 1; attempt <= job.maxRetries + 1; ++attempt) {
0082:             report.attempts = attempt;
0083:             report.cancelled = false;
0084:             try {
0085:                 if (!job.work) throw std::runtime_error("empty job");
0086:                 DownloadJobContext context{
0087:                     cancelRequested_,
0088:                     [this]() { return pauseRequested_.load(); },
0089:                     [this]() {
0090:                         std::unique_lock<std::mutex> lock(mutex_);
0091:                         condition_.wait(lock, [this] {
0092:                             return stopRequested_.load() || cancelRequested_.load() || !pauseRequested_.load();
0093:                         });
0094:                     },
0095:                     [this, jobId = job.id](int current, int total, const std::string& message) {
0096:                         if (onProgress_) onProgress_(jobId, current, total, message);
0097:                     }
0098:                 };
0099:                 if (context.cancelled()) {
0100:                     report.cancelled = true;
0101:                     report.error = "cancelled before start";
0102:                     break;
0103:                 }
0104:                 context.report(0, 0, "demarrage");
0105:                 job.work(context);
0106:                 if (context.cancelled()) {
0107:                     report.cancelled = true;
0108:                     report.error = "cancelled";
0109:                     break;
0110:                 }
0111:                 report.success = true;
0112:                 report.error.clear();
0113:                 break;
0114:             } catch (const std::exception& error) {
0115:                 report.error = error.what();
```
### 5.12 Copie offline complete d'une bibliotheque
OfflineLibrarySync parcourt les scans, chapitres et pages d'une source, puis les materialise sur le disque.

**Fichier : `src/application/OfflineLibrarySync.cpp`, lignes 1-86**

```cpp
0001: #include "application/OfflineLibrarySync.hpp"
0002: #include "infrastructure/JsonScanRepository.hpp"
0003: 
0004: #include <algorithm>
0005: #include <cctype>
0006: #include <filesystem>
0007: #include <utility>
0008: 
0009: OfflineLibrarySync::OfflineLibrarySync(const ScanDataSource& source, std::filesystem::path outputRoot)
0010:     : source_(source), outputRoot_(std::move(outputRoot)) {}
0011: 
0012: OfflineSyncReport OfflineLibrarySync::sync_all(const std::string& profile, ProgressCallback callback) const {
0013:     OfflineSyncReport total;
0014:     for (const auto& scan : source_.list_scans(profile)) {
0015:         auto report = sync_scan(scan.id, profile, callback);
0016:         total.scans += report.scans;
0017:         total.chapters += report.chapters;
0018:         total.downloadedPages += report.downloadedPages;
0019:         total.skippedPages += report.skippedPages;
0020:         total.failedPages += report.failedPages;
0021:     }
0022:     return total;
0023: }
0024: 
0025: OfflineSyncReport OfflineLibrarySync::sync_scan(
0026:     const std::string& scanId,
0027:     const std::string& profile,
0028:     ProgressCallback callback) const {
0029:     OfflineSyncReport report;
0030:     OfflineSyncProgress progress;
0031:     progress.scanId = scanId;
0032: 
0033:     const auto chapters = source_.list_chapters(scanId);
0034:     report.scans = 1;
0035:     report.chapters = static_cast<int>(chapters.size());
0036: 
0037:     for (int chapter : chapters) {
0038:         for (const auto& page : source_.list_pages(scanId, chapter)) {
0039:             progress.chapter = page.chapter;
0040:             progress.page = page.page;
0041: 
0042:             try {
0043:                 const auto sourcePath = source_.materialize_page(scanId, ScanProgress{page.chapter, page.page});
0044:                 const auto destination = outputRoot_ / scanId / std::to_string(page.chapter) /
0045:                     (std::to_string(page.page) + extension_from_path(sourcePath));
0046: 
0047:                 std::filesystem::create_directories(destination.parent_path());
0048:                 if (std::filesystem::exists(destination) &&
0049:                     std::filesystem::file_size(destination) == std::filesystem::file_size(sourcePath)) {
0050:                     ++report.skippedPages;
0051:                     ++progress.skippedPages;
0052:                 } else {
0053:                     std::filesystem::copy_file(sourcePath, destination, std::filesystem::copy_options::overwrite_existing);
0054:                     ++report.downloadedPages;
0055:                     ++progress.downloadedPages;
0056:                 }
0057:             } catch (...) {
0058:                 ++report.failedPages;
0059:                 ++progress.failedPages;
0060:             }
0061: 
0062:             if (callback) callback(progress);
0063:         }
0064:     }
0065: 
0066:     try {
0067:         JsonScanRepository repository;
0068:         repository.save_progress(outputRoot_ / scanId, source_.load_progress(scanId, profile));
0069:     } catch (...) {
0070:         // La copie des images est prioritaire. Une progression illisible ne doit pas annuler la synchro.
0071:     }
0072: 
0073:     return report;
0074: }
0075: 
0076: std::string OfflineLibrarySync::extension_from_path(const std::filesystem::path& path) {
0077:     std::string extension = path.extension().string();
0078:     std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
0079:         return static_cast<char>(std::tolower(c));
0080:     });
0081: 
0082:     if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".webp") {
0083:         return extension;
0084:     }
0085:     return ".jpg";
0086: }
```
### 5.13 Source locale : bibliotheque, profils et recherche texte
La source locale lit les dossiers scan/, stocke les donnees de profil dans cache/profiles et cherche dans les fichiers .txt adjacents.

**Fichier : `src/application/FileSystemScanDataSource.cpp`, lignes 92-288**

```cpp
0092: std::vector<ScanSummary> FileSystemScanDataSource::list_scans(const std::string& profile) const {
0093:     std::vector<ScanSummary> scans;
0094:     std::set<std::string> favorites;
0095:     for (const auto& line : read_lines(profile_file(profile, "favorites.txt"))) favorites.insert(line);
0096: 
0097:     if (!std::filesystem::exists(scanRoot_)) return scans;
0098:     for (const auto& scanEntry : std::filesystem::directory_iterator(scanRoot_)) {
0099:         if (!scanEntry.is_directory()) continue;
0100:         const std::string id = scanEntry.path().filename().string();
0101:         int chapterCount = 0;
0102:         int pageCount = 0;
0103:         std::string cover;
0104:         for (const auto& chapterEntry : std::filesystem::directory_iterator(scanEntry.path())) {
0105:             if (!chapterEntry.is_directory() || parse_positive_int(chapterEntry.path().filename().string()) == 0) continue;
0106:             ++chapterCount;
0107:             for (const auto& pageEntry : std::filesystem::directory_iterator(chapterEntry.path())) {
0108:                 if (pageEntry.is_regular_file() && is_image_file(pageEntry.path())) {
0109:                     ++pageCount;
0110:                     if (cover.empty()) cover = pageEntry.path().string();
0111:                 }
0112:             }
0113:         }
0114:         ScanSummary summary{id, title_from_id(id), chapterCount, pageCount, ScanProgress{1, 1}, false, "", ""};
0115:         summary.progress = load_progress(id, profile);
0116:         summary.favorite = favorites.count(id) > 0;
0117:         summary.coverImageUrl = cover;
0118:         scans.push_back(summary);
0119:     }
0120:     std::sort(scans.begin(), scans.end(), [](const auto& left, const auto& right) { return left.title < right.title; });
0121:     return scans;
0122: }
0123: 
0124: std::vector<int> FileSystemScanDataSource::list_chapters(const std::string& scanId) const {
0125:     std::vector<int> chapters;
0126:     const auto scanPath = scanRoot_ / scanId;
0127:     if (!std::filesystem::exists(scanPath)) return chapters;
0128:     for (const auto& entry : std::filesystem::directory_iterator(scanPath)) {
0129:         if (entry.is_directory()) {
0130:             const int chapter = parse_positive_int(entry.path().filename().string());
0131:             if (chapter > 0) chapters.push_back(chapter);
0132:         }
0133:     }
0134:     std::sort(chapters.begin(), chapters.end());
0135:     return chapters;
0136: }
0137: 
0138: std::vector<ScanPageInfo> FileSystemScanDataSource::list_pages(const std::string& scanId, int chapter) const {
0139:     std::vector<ScanPageInfo> pages;
0140:     const auto chapterPath = scanRoot_ / scanId / std::to_string(chapter);
0141:     if (!std::filesystem::exists(chapterPath)) return pages;
0142:     for (const auto& entry : std::filesystem::directory_iterator(chapterPath)) {
0143:         if (!entry.is_regular_file() || !is_image_file(entry.path())) continue;
0144:         const int page = parse_positive_int(entry.path().stem().string());
0145:         if (page > 0) pages.push_back({chapter, page, "", static_cast<long long>(std::filesystem::file_size(entry.path())), entry.path().string()});
0146:     }
0147:     std::sort(pages.begin(), pages.end(), [](const auto& left, const auto& right) { return left.page < right.page; });
0148:     return pages;
0149: }
0150: 
0151: std::filesystem::path FileSystemScanDataSource::materialize_page(const std::string& scanId, ScanProgress progress) const {
0152:     progress.normalize();
0153:     const auto chapterPath = scanRoot_ / scanId / std::to_string(progress.chapter);
0154:     static const char* extensions[] = {".jpg", ".jpeg", ".png", ".webp"};
0155:     for (const auto* extension : extensions) {
0156:         auto candidate = chapterPath / (std::to_string(progress.page) + extension);
0157:         if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) return candidate;
0158:     }
0159:     throw std::runtime_error("Page introuvable dans la source fichier: " + scanId);
0160: }
0161: 
0162: ScanProgress FileSystemScanDataSource::load_progress(const std::string& scanId, const std::string& profile) const {
0163:     if (profile != "default") {
0164:         const auto file = profile_root(profile) / "progress" / (safe_token(scanId) + ".json");
0165:         std::ifstream in(file);
0166:         if (in) {
0167:             std::ostringstream buffer; buffer << in.rdbuf();
0168:             ScanProgress progress{scangui::json::int_field(buffer.str(), "chapter", 1), scangui::json::int_field(buffer.str(), "page", 1)};
0169:             progress.normalize();
0170:             return progress;
0171:         }
0172:     }
0173:     if (auto metadata = repository_.load(scanRoot_ / scanId)) return metadata->saveProgress;
0174:     return {1, 1};
0175: }
0176: 
0177: void FileSystemScanDataSource::save_progress(const std::string& scanId, ScanProgress progress, const std::string& profile) const {
0178:     progress.normalize();
0179:     if (profile == "default") {
0180:         repository_.save_progress(scanRoot_ / scanId, progress);
0181:     }
0182:     const auto file = profile_root(profile) / "progress" / (safe_token(scanId) + ".json");
0183:     std::filesystem::create_directories(file.parent_path());
0184:     std::ofstream out(file, std::ios::trunc);
0185:     out << "{\"scanId\":\"" << scangui::json::escape(scanId) << "\",\"chapter\":" << progress.chapter << ",\"page\":" << progress.page << "}\n";
0186: 
0187:     auto history = read_lines(profile_file(profile, "history.txt"));
0188:     history.insert(history.begin(), scanId + "|" + std::to_string(progress.chapter) + "|" + std::to_string(progress.page) + "|local");
0189:     if (history.size() > 100) history.resize(100);
0190:     write_lines(profile_file(profile, "history.txt"), history);
0191: }
0192: 
0193: std::vector<ProfileSummary> FileSystemScanDataSource::list_profiles() const {
0194:     std::vector<ProfileSummary> profiles{{"default", "Default", "#3b82f6"}};
0195:     const auto root = std::filesystem::path("cache") / "profiles";
0196:     if (!std::filesystem::exists(root)) return profiles;
0197:     for (const auto& entry : std::filesystem::directory_iterator(root)) {
0198:         if (!entry.is_directory()) continue;
0199:         const std::string id = entry.path().filename().string();
0200:         if (id != "default") profiles.push_back({id, title_from_id(id), "#64748b"});
0201:     }
0202:     return profiles;
0203: }
0204: 
0205: void FileSystemScanDataSource::save_profile(const ProfileSummary& profile) const {
0206:     std::filesystem::create_directories(profile_root(profile.id));
0207:     std::ofstream out(profile_file(profile.id, "profile.json"), std::ios::trunc);
0208:     out << "{\"id\":\"" << scangui::json::escape(profile.id) << "\",\"displayName\":\"" << scangui::json::escape(profile.displayName) << "\",\"avatarColor\":\"" << scangui::json::escape(profile.avatarColor) << "\"}\n";
0209: }
0210: 
0211: std::vector<std::string> FileSystemScanDataSource::list_favorites(const std::string& profile) const {
0212:     return read_lines(profile_file(profile, "favorites.txt"));
0213: }
0214: 
0215: void FileSystemScanDataSource::set_favorite(const std::string& scanId, bool favorite, const std::string& profile) const {
0216:     auto lines = read_lines(profile_file(profile, "favorites.txt"));
0217:     lines.erase(std::remove(lines.begin(), lines.end(), scanId), lines.end());
0218:     if (favorite) lines.push_back(scanId);
0219:     write_lines(profile_file(profile, "favorites.txt"), lines);
0220: }
0221: 
0222: std::vector<BookmarkSummary> FileSystemScanDataSource::list_bookmarks(const std::string& scanId, const std::string& profile) const {
0223:     std::vector<BookmarkSummary> bookmarks;
0224:     long long id = 1;
0225:     for (const auto& line : read_lines(profile_file(profile, "bookmarks.txt"))) {
0226:         auto parts = split_pipe(line);
0227:         if (parts.size() < 4 || parts[0] != scanId) continue;
0228:         bookmarks.push_back({id++, parts[0], parse_positive_int(parts[1]), parse_positive_int(parts[2]), parts[3], "local"});
0229:     }
0230:     return bookmarks;
0231: }
0232: 
0233: BookmarkSummary FileSystemScanDataSource::add_bookmark(const std::string& scanId, ScanProgress progress, const std::string& note, const std::string& profile) const {
0234:     progress.normalize();
0235:     auto lines = read_lines(profile_file(profile, "bookmarks.txt"));
0236:     lines.push_back(scanId + "|" + std::to_string(progress.chapter) + "|" + std::to_string(progress.page) + "|" + note);
0237:     write_lines(profile_file(profile, "bookmarks.txt"), lines);
0238:     return {static_cast<long long>(lines.size()), scanId, progress.chapter, progress.page, note, "local"};
0239: }
0240: 
0241: std::vector<HistoryEntry> FileSystemScanDataSource::list_history(const std::string& profile, int limit) const {
0242:     std::vector<HistoryEntry> history;
0243:     for (const auto& line : read_lines(profile_file(profile, "history.txt"))) {
0244:         auto parts = split_pipe(line);
0245:         if (parts.size() < 4) continue;
0246:         history.push_back({parts[0], title_from_id(parts[0]), parse_positive_int(parts[1]), parse_positive_int(parts[2]), parts[3]});
0247:         if (static_cast<int>(history.size()) >= limit) break;
0248:     }
0249:     return history;
0250: }
0251: 
0252: std::vector<SearchResultSummary> FileSystemScanDataSource::search_text(const std::string& query, int limit) const {
0253:     std::vector<SearchResultSummary> results;
0254:     if (query.empty() || limit <= 0 || !std::filesystem::exists(scanRoot_)) return results;
0255:     std::string q = query;
0256:     std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
0257: 
0258:     for (const auto& scanEntry : std::filesystem::directory_iterator(scanRoot_)) {
0259:         if (!scanEntry.is_directory()) continue;
0260:         const std::string scanId = scanEntry.path().filename().string();
0261:         for (const auto& entry : std::filesystem::recursive_directory_iterator(scanEntry.path())) {
0262:             if (!entry.is_regular_file() || entry.path().extension() != ".txt") continue;
0263:             std::ifstream in(entry.path());
0264:             std::ostringstream buffer;
0265:             buffer << in.rdbuf();
0266:             std::string text = buffer.str();
0267:             std::string lower = text;
0268:             std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
0269:             auto pos = lower.find(q);
0270:             if (pos == std::string::npos) continue;
0271: 
0272:             int chapter = 1;
0273:             int page = 1;
0274:             auto parentName = entry.path().parent_path().filename().string();
0275:             auto stemName = entry.path().stem().string();
0276:             chapter = parse_positive_int(parentName);
0277:             page = parse_positive_int(stemName);
0278:             if (chapter <= 0) chapter = 1;
0279:             if (page <= 0) page = 1;
0280: 
0281:             const std::size_t begin = pos > 80 ? pos - 80 : 0;
0282:             std::string snippet = text.substr(begin, 220);
0283:             results.push_back({scanId, title_from_id(scanId), chapter, page, snippet, "local"});
0284:             if (static_cast<int>(results.size()) >= limit) return results;
0285:         }
0286:     }
0287:     return results;
0288: }
```
### 5.14 Source API : scans, images, progression, OCR
La source API convertit les appels HTTP du serveur en objets metier et met les images en cache local.

**Fichier : `src/application/ApiScanDataSource.cpp`, lignes 49-181**

```cpp
0049: std::vector<ScanSummary> ApiScanDataSource::list_scans(const std::string& profile) const {
0050:     const std::string json = http_.get_text(endpoint("/api/scans?profile=" + url_encode(profile)));
0051:     std::vector<ScanSummary> scans;
0052:     for (const auto& object : extract_items_objects(json)) {
0053:         const std::string id = string_field(object, "id");
0054:         if (id.empty()) continue;
0055:         ScanSummary summary{id, string_field(object, "title"), int_field(object, "chapterCount"), int_field(object, "pageCount")};
0056:         summary.progress = {int_field(object, "progressChapter", 1), int_field(object, "progressPage", 1)};
0057:         summary.progress.normalize();
0058:         summary.favorite = bool_field(object, "favorite");
0059:         summary.lastReadAt = string_field(object, "lastReadAt");
0060:         summary.coverImageUrl = string_field(object, "coverImageUrl");
0061:         scans.push_back(summary);
0062:     }
0063:     return scans;
0064: }
0065: 
0066: std::vector<int> ApiScanDataSource::list_chapters(const std::string& scanId) const {
0067:     const std::string json = http_.get_text(endpoint("/api/scans/" + url_encode(scanId) + "/chapters"));
0068:     std::vector<int> chapters;
0069:     for (const auto& object : extract_items_objects(json)) {
0070:         const int chapter = int_field(object, "chapter");
0071:         if (chapter > 0) chapters.push_back(chapter);
0072:     }
0073:     return chapters;
0074: }
0075: 
0076: std::vector<ScanPageInfo> ApiScanDataSource::list_pages(const std::string& scanId, int chapter) const {
0077:     const std::string json = http_.get_text(endpoint("/api/scans/" + url_encode(scanId) + "/chapters/" + std::to_string(chapter) + "/pages"));
0078:     std::vector<ScanPageInfo> pages;
0079:     for (const auto& object : extract_items_objects(json)) {
0080:         ScanPageInfo page;
0081:         page.chapter = int_field(object, "chapter");
0082:         page.page = int_field(object, "page");
0083:         page.mimeType = string_field(object, "mimeType");
0084:         page.sizeBytes = long_field(object, "sizeBytes");
0085:         page.imageUrl = string_field(object, "imageUrl");
0086:         if (page.chapter > 0 && page.page > 0) pages.push_back(page);
0087:     }
0088:     return pages;
0089: }
0090: 
0091: std::filesystem::path ApiScanDataSource::materialize_page(const std::string& scanId, ScanProgress progress) const {
0092:     progress.normalize();
0093:     const auto pages = list_pages(scanId, progress.chapter);
0094:     auto it = std::find_if(pages.begin(), pages.end(), [&](const ScanPageInfo& page) { return page.page == progress.page; });
0095:     if (it == pages.end()) throw std::runtime_error("Page introuvable via l'API: " + scanId);
0096: 
0097:     const auto output = cacheRoot_ / scanId / std::to_string(progress.chapter) / (std::to_string(progress.page) + extension_for_mime(it->mimeType));
0098:     if (!std::filesystem::exists(output)) http_.download_file(endpoint(it->imageUrl), output);
0099:     return output;
0100: }
0101: 
0102: ScanProgress ApiScanDataSource::load_progress(const std::string& scanId, const std::string& profile) const {
0103:     const std::string json = http_.get_text(endpoint("/api/scans/" + url_encode(scanId) + "/progress?profile=" + url_encode(profile)));
0104:     ScanProgress progress{int_field(json, "chapter", 1), int_field(json, "page", 1)};
0105:     progress.normalize();
0106:     return progress;
0107: }
0108: 
0109: void ApiScanDataSource::save_progress(const std::string& scanId, ScanProgress progress, const std::string& profile) const {
0110:     progress.normalize();
0111:     const std::string body = "{\"chapter\":" + std::to_string(progress.chapter) + ",\"page\":" + std::to_string(progress.page) + "}";
0112:     (void)http_.post_json(endpoint("/api/scans/" + url_encode(scanId) + "/progress?profile=" + url_encode(profile)), body);
0113: }
0114: 
0115: std::vector<ProfileSummary> ApiScanDataSource::list_profiles() const {
0116:     const std::string json = http_.get_text(endpoint("/api/profiles"));
0117:     std::vector<ProfileSummary> profiles;
0118:     for (const auto& object : extract_items_objects(json)) {
0119:         const std::string id = string_field(object, "id");
0120:         if (!id.empty()) profiles.push_back({id, string_field(object, "displayName", id), string_field(object, "avatarColor", "#3b82f6")});
0121:     }
0122:     if (profiles.empty()) profiles.push_back({"default", "Default", "#3b82f6"});
0123:     return profiles;
0124: }
0125: 
0126: void ApiScanDataSource::save_profile(const ProfileSummary& profile) const {
0127:     const std::string id = profile.id.empty() ? "default" : profile.id;
0128:     const std::string body = "{\"id\":\"" + scangui::json::escape(id) + "\",\"displayName\":\"" + scangui::json::escape(profile.displayName.empty() ? id : profile.displayName) + "\",\"avatarColor\":\"" + scangui::json::escape(profile.avatarColor.empty() ? "#3b82f6" : profile.avatarColor) + "\"}";
0129:     (void)http_.post_json(endpoint("/api/profiles"), body);
0130: }
0131: 
0132: std::vector<std::string> ApiScanDataSource::list_favorites(const std::string& profile) const {
0133:     const std::string json = http_.get_text(endpoint("/api/profiles/" + url_encode(profile) + "/favorites"));
0134:     std::vector<std::string> favorites;
0135:     auto root = scangui::json::parse(json);
0136:     if (!root) return favorites;
0137:     const auto* items = scangui::json::find_field(*root, "items");
0138:     if (items == nullptr || !items->is_array()) return favorites;
0139:     for (const auto& item : items->as_array()) if (item.is_string()) favorites.push_back(item.as_string());
0140:     return favorites;
0141: }
0142: 
0143: void ApiScanDataSource::set_favorite(const std::string& scanId, bool favorite, const std::string& profile) const {
0144:     const std::string body = std::string("{\"favorite\":") + (favorite ? "true" : "false") + "}";
0145:     (void)http_.post_json(endpoint("/api/scans/" + url_encode(scanId) + "/favorite?profile=" + url_encode(profile)), body);
0146: }
0147: 
0148: std::vector<BookmarkSummary> ApiScanDataSource::list_bookmarks(const std::string& scanId, const std::string& profile) const {
0149:     const std::string json = http_.get_text(endpoint("/api/scans/" + url_encode(scanId) + "/bookmarks?profile=" + url_encode(profile)));
0150:     std::vector<BookmarkSummary> bookmarks;
0151:     for (const auto& object : extract_items_objects(json)) {
0152:         bookmarks.push_back({long_field(object, "id"), string_field(object, "scanId", scanId), int_field(object, "chapter", 1), int_field(object, "page", 1), string_field(object, "note"), string_field(object, "createdAt")});
0153:     }
0154:     return bookmarks;
0155: }
0156: 
0157: BookmarkSummary ApiScanDataSource::add_bookmark(const std::string& scanId, ScanProgress progress, const std::string& note, const std::string& profile) const {
0158:     progress.normalize();
0159:     const std::string body = "{\"chapter\":" + std::to_string(progress.chapter) + ",\"page\":" + std::to_string(progress.page) + ",\"note\":\"" + scangui::json::escape(note) + "\"}";
0160:     const std::string json = http_.post_json(endpoint("/api/scans/" + url_encode(scanId) + "/bookmarks?profile=" + url_encode(profile)), body);
0161:     return {long_field(json, "id"), string_field(json, "scanId", scanId), int_field(json, "chapter", progress.chapter), int_field(json, "page", progress.page), string_field(json, "note", note), string_field(json, "createdAt")};
0162: }
0163: 
0164: std::vector<HistoryEntry> ApiScanDataSource::list_history(const std::string& profile, int limit) const {
0165:     const std::string json = http_.get_text(endpoint("/api/profiles/" + url_encode(profile) + "/history?limit=" + std::to_string(limit)));
0166:     std::vector<HistoryEntry> history;
0167:     for (const auto& object : extract_items_objects(json)) {
0168:         history.push_back({string_field(object, "scanId"), string_field(object, "title"), int_field(object, "chapter", 1), int_field(object, "page", 1), string_field(object, "readAt")});
0169:     }
0170:     return history;
0171: }
0172: 
0173: std::vector<SearchResultSummary> ApiScanDataSource::search_text(const std::string& query, int limit) const {
0174:     if (query.empty()) return {};
0175:     const std::string json = http_.get_text(endpoint("/api/search?q=" + url_encode(query) + "&limit=" + std::to_string(limit)));
0176:     std::vector<SearchResultSummary> results;
0177:     for (const auto& object : extract_items_objects(json)) {
0178:         results.push_back({string_field(object, "scanId"), string_field(object, "title"), int_field(object, "chapter", 1), int_field(object, "page", 1), string_field(object, "snippet"), "api"});
0179:     }
0180:     return results;
0181: }
```
### 5.15 Routage central de l'API
Le controleur expose les routes serveur, web, progression, profils, favoris, offline, OCR et admin.

**Fichier : `server_src/ScanApiController.cpp`, lignes 128-183**

```cpp
0128: HttpResponse ScanApiController::handle(const HttpRequest& request) {
0129:     std::lock_guard<std::mutex> lock(requestMutex_);
0130:     try {
0131:         const auto parts = split_path(request.path);
0132:         if (request.method == "GET" && (request.path == "/" || (parts.size() >= 1 && parts[0] == "web"))) return serve_web(request);
0133:         if (request.method == "GET" && parts == std::vector<std::string>{"api", "health"}) return health();
0134:         if (request.method == "GET" && parts == std::vector<std::string>{"api", "version"}) return version();
0135:         if (request.method == "GET" && parts == std::vector<std::string>{"api", "stats"}) return stats();
0136:         if (request.method == "GET" && parts == std::vector<std::string>{"api", "search"}) return search(request);
0137:         if (request.method == "GET" && parts == std::vector<std::string>{"api", "offline", "manifest"}) return offline_manifest(request);
0138:         if (request.method == "POST" && parts == std::vector<std::string>{"api", "admin", "sync"}) return sync_library(request);
0139:         if (request.method == "POST" && parts == std::vector<std::string>{"api", "admin", "ocr"}) return ocr_library(request);
0140: 
0141:         if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "profiles") {
0142:             if (request.method == "GET" && parts.size() == 2) return list_profiles();
0143:             if (request.method == "POST" && parts.size() == 2) return create_profile(request.body);
0144:             if (request.method == "GET" && parts.size() == 4 && parts[3] == "favorites") return list_favorites(parts[2]);
0145:             if (request.method == "GET" && parts.size() == 4 && parts[3] == "history") return list_history(parts[2], request);
0146:         }
0147: 
0148:         if (request.method == "GET" && parts == std::vector<std::string>{"api", "scans"}) return list_scans(request);
0149: 
0150:         if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "scans") {
0151:             const auto& scanId = parts[2];
0152:             if (request.method == "GET" && parts.size() == 3) return get_scan(scanId, request);
0153:             if (request.method == "GET" && parts.size() == 4 && parts[3] == "chapters") return list_chapters(scanId);
0154:             if (request.method == "GET" && parts.size() == 6 && parts[3] == "chapters" && parts[5] == "summary") {
0155:                 auto chapter = parse_int(parts[4]);
0156:                 if (!chapter) return HttpResponse::bad_request("Invalid chapter number");
0157:                 return chapter_summary(scanId, *chapter);
0158:             }
0159:             if (request.method == "GET" && parts.size() == 6 && parts[3] == "chapters" && parts[5] == "pages") {
0160:                 auto chapter = parse_int(parts[4]);
0161:                 if (!chapter) return HttpResponse::bad_request("Invalid chapter number");
0162:                 return list_pages(scanId, *chapter, request);
0163:             }
0164:             if (request.method == "GET" && parts.size() == 8 && parts[3] == "chapters" && parts[5] == "pages" && parts[7] == "image") {
0165:                 auto chapter = parse_int(parts[4]);
0166:                 auto page = parse_int(parts[6]);
0167:                 if (!chapter || !page) return HttpResponse::bad_request("Invalid chapter or page number");
0168:                 return get_image(scanId, *chapter, *page);
0169:             }
0170:             if ((request.method == "GET" || request.method == "POST" || request.method == "PUT") && parts.size() == 4 && parts[3] == "progress") {
0171:                 const std::string profile = query_string(request, "profile", "default");
0172:                 return request.method == "GET" ? get_progress(scanId, profile) : save_progress(scanId, profile, request.body);
0173:             }
0174:             if ((request.method == "POST" || request.method == "PUT") && parts.size() == 4 && parts[3] == "favorite") return set_favorite(scanId, query_string(request, "profile", "default"), request.body);
0175:             if (request.method == "GET" && parts.size() == 4 && parts[3] == "bookmarks") return list_bookmarks(scanId, query_string(request, "profile", "default"));
0176:             if (request.method == "POST" && parts.size() == 4 && parts[3] == "bookmarks") return add_bookmark(scanId, query_string(request, "profile", "default"), request.body);
0177:         }
0178: 
0179:         return HttpResponse::not_found();
0180:     } catch (const std::exception& error) {
0181:         return HttpResponse::server_error(error.what());
0182:     }
0183: }
```
### 5.16 API de bibliotheque paginee et filtree
Les parametres q, favorites, sort, limit et offset preparent une vraie bibliotheque client.

**Fichier : `server_src/ScanApiController.cpp`, lignes 231-278**

```cpp
0231: HttpResponse ScanApiController::list_scans(const HttpRequest& request) const {
0232:     const std::string profile = query_string(request, "profile", "default");
0233:     const std::string q = lower_ascii(query_string(request, "q", ""));
0234:     const bool favoritesOnly = query_string(request, "favorites", "0") == "1";
0235:     const std::string sort = query_string(request, "sort", "title");
0236:     int limit = query_int(request, "limit", 0);
0237:     int offset = query_int(request, "offset", 0);
0238: 
0239:     auto scans = database_.list_scans(profile);
0240:     scans.erase(std::remove_if(scans.begin(), scans.end(), [&](const ScanSummaryRecord& scan) {
0241:         if (favoritesOnly && !scan.favorite) return true;
0242:         if (!q.empty() && lower_ascii(scan.title).find(q) == std::string::npos && lower_ascii(scan.id).find(q) == std::string::npos) return true;
0243:         return false;
0244:     }), scans.end());
0245: 
0246:     if (sort == "progress") {
0247:         std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.progressChapter, a.progressPage, a.title) > std::tie(b.progressChapter, b.progressPage, b.title); });
0248:     } else if (sort == "chapters") {
0249:         std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.chapterCount, a.title) > std::tie(b.chapterCount, b.title); });
0250:     } else if (sort == "pages") {
0251:         std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.pageCount, a.title) > std::tie(b.pageCount, b.title); });
0252:     } else if (sort == "lastRead") {
0253:         std::sort(scans.begin(), scans.end(), [](const auto& a, const auto& b) { return std::tie(a.lastReadAt, a.title) > std::tie(b.lastReadAt, b.title); });
0254:     }
0255: 
0256:     const int total = static_cast<int>(scans.size());
0257:     if (offset < 0) offset = 0;
0258:     if (offset > total) offset = total;
0259:     auto begin = scans.begin() + offset;
0260:     auto end = scans.end();
0261:     if (limit > 0 && offset + limit < total) end = scans.begin() + offset + limit;
0262: 
0263:     std::string json = "{\"profile\":\"" + scangui::json::escape(profile) + "\",\"total\":" + std::to_string(total) + ",\"items\":[";
0264:     bool first = true;
0265:     for (auto it = begin; it != end; ++it) {
0266:         if (!first) json += ',';
0267:         first = false;
0268:         json += scan_to_json(*it);
0269:     }
0270:     json += "]}";
0271:     return HttpResponse::json(200, json);
0272: }
0273: 
0274: HttpResponse ScanApiController::get_scan(const std::string& scanId, const HttpRequest& request) const {
0275:     auto scan = database_.get_scan(scanId, query_string(request, "profile", "default"));
0276:     return scan ? HttpResponse::json(200, scan_to_json(*scan)) : HttpResponse::not_found("Scan not found");
0277: }
0278: 
```
### 5.17 Recherche OCR et resume de chapitre
Ces routes interrogent les textes indexes pour retrouver une page ou produire un extrait de chapitre.

**Fichier : `server_src/ScanApiController.cpp`, lignes 417-443**

```cpp
0417: HttpResponse ScanApiController::search(const HttpRequest& request) const {
0418:     const std::string query = query_string(request, "q", "");
0419:     if (query.empty()) return HttpResponse::bad_request("Missing q query parameter");
0420:     const auto rows = database_.search_page_text(query, query_int(request, "limit", 50));
0421:     std::string json = "{\"query\":\"" + scangui::json::escape(query) + "\",\"items\":[";
0422:     for (std::size_t i = 0; i < rows.size(); ++i) {
0423:         if (i) json += ',';
0424:         const auto& row = rows[i];
0425:         json += "{\"scanId\":\"" + scangui::json::escape(row.scanId) +
0426:             "\",\"title\":\"" + scangui::json::escape(row.title) +
0427:             "\",\"chapter\":" + std::to_string(row.chapter) +
0428:             ",\"page\":" + std::to_string(row.page) +
0429:             ",\"snippet\":\"" + scangui::json::escape(row.snippet) + "\"}";
0430:     }
0431:     json += "]}";
0432:     return HttpResponse::json(200, json);
0433: }
0434: 
0435: HttpResponse ScanApiController::chapter_summary(const std::string& scanId, int chapter) const {
0436:     if (!database_.get_scan(scanId)) return HttpResponse::not_found("Scan not found");
0437:     const auto texts = database_.chapter_text(scanId, chapter);
0438:     std::string joined;
0439:     for (const auto& pageText : texts) {
0440:         if (!joined.empty()) joined += "\n";
0441:         joined += pageText.text;
0442:     }
0443:     return HttpResponse::json(200, "{\"scanId\":\"" + scangui::json::escape(scanId) + "\",\"chapter\":" + std::to_string(chapter) + ",\"summary\":\"" + scangui::json::escape(summarize_text(joined)) + "\"}");
```
### 5.18 Securite admin locale et token optionnel
Les operations dangereuses sont refusees hors localhost et peuvent exiger un token.

**Fichier : `server_src/ScanApiController.cpp`, lignes 464-479**

```cpp
0464: bool ScanApiController::is_safe_page_path(const std::filesystem::path& path) const {
0465:     if (!std::filesystem::exists(path)) return false;
0466:     const auto root = std::filesystem::weakly_canonical(scanRoot_);
0467:     const auto file = std::filesystem::weakly_canonical(path);
0468:     const std::string rootText = root.string();
0469:     const std::string fileText = file.string();
0470:     return fileText == rootText || fileText.rfind(rootText + std::string(1, std::filesystem::path::preferred_separator), 0) == 0;
0471: }
0472: 
0473: bool ScanApiController::admin_request_allowed(const HttpRequest& request) const {
0474:     const bool local = bindHost_ == "127.0.0.1" || bindHost_ == "localhost";
0475:     if (!local) return false;
0476:     if (adminToken_.empty()) return true;
0477:     auto it = request.headers.find("x-scangui-admin-token");
0478:     return it != request.headers.end() && it->second == adminToken_;
0479: }
```
### 5.19 Schema PostgreSQL enrichi
Le schema gere profils, progression, favoris, marque-pages, historique et textes OCR.

**Fichier : `migrations/001_init.sql`, lignes 25-87**

```sql
0025:     PRIMARY KEY (scan_id, chapter_number, page_number),
0026:     FOREIGN KEY (scan_id, chapter_number) REFERENCES chapters(scan_id, chapter_number) ON DELETE CASCADE
0027: );
0028: 
0029: CREATE TABLE IF NOT EXISTS profiles (
0030:     id TEXT PRIMARY KEY,
0031:     display_name TEXT NOT NULL,
0032:     avatar_color TEXT NOT NULL DEFAULT '#3b82f6',
0033:     created_at TIMESTAMPTZ NOT NULL DEFAULT now()
0034: );
0035: 
0036: INSERT INTO profiles(id, display_name, avatar_color)
0037: VALUES('default', 'Default', '#3b82f6')
0038: ON CONFLICT(id) DO NOTHING;
0039: 
0040: CREATE TABLE IF NOT EXISTS reading_progress (
0041:     scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
0042:     profile TEXT NOT NULL DEFAULT 'default',
0043:     chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
0044:     page_number INTEGER NOT NULL CHECK (page_number > 0),
0045:     updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
0046:     PRIMARY KEY (scan_id, profile)
0047: );
0048: 
0049: CREATE TABLE IF NOT EXISTS favorites (
0050:     scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
0051:     profile TEXT NOT NULL DEFAULT 'default',
0052:     created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
0053:     PRIMARY KEY (scan_id, profile)
0054: );
0055: 
0056: CREATE TABLE IF NOT EXISTS bookmarks (
0057:     id BIGSERIAL PRIMARY KEY,
0058:     scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
0059:     profile TEXT NOT NULL DEFAULT 'default',
0060:     chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
0061:     page_number INTEGER NOT NULL CHECK (page_number > 0),
0062:     note TEXT NOT NULL DEFAULT '',
0063:     created_at TIMESTAMPTZ NOT NULL DEFAULT now()
0064: );
0065: 
0066: CREATE TABLE IF NOT EXISTS reading_history (
0067:     id BIGSERIAL PRIMARY KEY,
0068:     scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
0069:     profile TEXT NOT NULL DEFAULT 'default',
0070:     chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
0071:     page_number INTEGER NOT NULL CHECK (page_number > 0),
0072:     read_at TIMESTAMPTZ NOT NULL DEFAULT now()
0073: );
0074: 
0075: CREATE TABLE IF NOT EXISTS page_texts (
0076:     scan_id TEXT NOT NULL,
0077:     chapter_number INTEGER NOT NULL,
0078:     page_number INTEGER NOT NULL,
0079:     page_text TEXT NOT NULL DEFAULT '',
0080:     updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
0081:     PRIMARY KEY (scan_id, chapter_number, page_number),
0082:     FOREIGN KEY (scan_id, chapter_number, page_number) REFERENCES pages(scan_id, chapter_number, page_number) ON DELETE CASCADE
0083: );
0084: 
0085: CREATE INDEX IF NOT EXISTS idx_pages_scan_chapter ON pages(scan_id, chapter_number, page_number);
0086: CREATE INDEX IF NOT EXISTS idx_history_profile ON reading_history(profile, read_at DESC);
0087: CREATE INDEX IF NOT EXISTS idx_page_texts_scan_chapter ON page_texts(scan_id, chapter_number, page_number);
```
### 5.20 Indexeur OCR local / sidecar / Tesseract
L'OCR privilegie les fichiers .txt voisins des images et peut appeler un moteur externe configure.

**Fichier : `server_src/OcrIndexer.cpp`, lignes 53-115**

```cpp
0053: 
0054: OcrIndexReport OcrIndexer::index_all() {
0055:     OcrIndexReport total;
0056:     for (const auto& scan : database_.list_scans("default")) {
0057:         auto report = index_scan(scan.id);
0058:         total.pagesVisited += report.pagesVisited;
0059:         total.indexedPages += report.indexedPages;
0060:         total.skippedPages += report.skippedPages;
0061:         total.failedPages += report.failedPages;
0062:     }
0063:     return total;
0064: }
0065: 
0066: OcrIndexReport OcrIndexer::index_scan(const std::string& scanId) {
0067:     OcrIndexReport report;
0068:     for (const auto& chapter : database_.list_chapters(scanId)) {
0069:         for (const auto& page : database_.list_pages(scanId, chapter.number)) {
0070:             ++report.pagesVisited;
0071:             try {
0072:                 const std::string text = extract_text(page);
0073:                 if (!is_text_useful(text)) {
0074:                     ++report.skippedPages;
0075:                     continue;
0076:                 }
0077:                 database_.upsert_page_text(scanId, page.chapter, page.page, text);
0078:                 ++report.indexedPages;
0079:             } catch (...) {
0080:                 ++report.failedPages;
0081:             }
0082:         }
0083:     }
0084:     return report;
0085: }
0086: 
0087: std::string OcrIndexer::extract_text(const PageRecord& page) const {
0088:     std::string text = read_sidecar_text(page.filePath);
0089:     if (is_text_useful(text)) return text;
0090:     if (!commandTemplate_.empty()) return run_ocr_command(page.filePath);
0091:     return {};
0092: }
0093: 
0094: std::string OcrIndexer::read_sidecar_text(const std::filesystem::path& imagePath) const {
0095:     std::filesystem::path txt = imagePath;
0096:     txt.replace_extension(".txt");
0097:     return read_file(txt);
0098: }
0099: 
0100: std::string OcrIndexer::run_ocr_command(const std::filesystem::path& imagePath) const {
0101:     const auto output = std::filesystem::temp_directory_path() / ("scangui-ocr-" + std::to_string(std::rand()) + ".txt");
0102:     std::string command = commandTemplate_;
0103:     if (command.find("{input}") != std::string::npos || command.find("{output}") != std::string::npos) {
0104:         command = replace_all(command, "{input}", shell_quote(imagePath));
0105:         command = replace_all(command, "{output}", shell_quote(output));
0106:     } else {
0107:         command += " " + shell_quote(imagePath) + " " + shell_quote(output);
0108:     }
0109:     const int exitCode = std::system(command.c_str());
0110:     if (exitCode != 0) return {};
0111:     std::string text = read_file(output);
0112:     std::filesystem::remove(output);
0113:     return text;
0114: }
0115: 
```
### 5.21 Client web local minimal
Le client web consomme la meme API que GTK et prouve l'architecture multi-clients.

**Fichier : `web/app.js`, lignes 1-124**

```js
0001: const state = { scans: [], current: null, chapters: [], pages: [], chapterIndex: 0, pageIndex: 0 };
0002: const $ = (id) => document.getElementById(id);
0003: const profile = () => $('profile').value || 'default';
0004: const api = async (url, options = {}) => {
0005:   const response = await fetch(url, options);
0006:   if (!response.ok) throw new Error(await response.text());
0007:   return response.json();
0008: };
0009: 
0010: async function loadLibrary() {
0011:   const params = new URLSearchParams({ profile: profile(), sort: $('sort').value });
0012:   if ($('search').value) params.set('q', $('search').value);
0013:   if ($('favoritesOnly').checked) params.set('favorites', '1');
0014:   const data = await api(`/api/scans?${params}`);
0015:   state.scans = data.items || [];
0016:   renderLibrary();
0017:   if ($('search').value) searchOcr($('search').value).catch(() => {});
0018: }
0019: 
0020: function renderLibrary() {
0021:   $('library').innerHTML = '';
0022:   state.scans.forEach(scan => {
0023:     const card = document.createElement('div');
0024:     card.className = 'card';
0025:     card.innerHTML = `<div><h3>${escapeHtml(scan.title)}</h3><div class="badge">${scan.favorite ? '★ Favori' : 'Lecture'}</div></div>
0026:       <p>${scan.chapterCount} chap. - ${scan.pageCount} pages<br>Progression: ch. ${scan.progressChapter || 1}, p. ${scan.progressPage || 1}<br>${scan.lastReadAt || ''}</p>`;
0027:     card.onclick = () => openScan(scan, scan.progressChapter || 1, scan.progressPage || 1);
0028:     $('library').appendChild(card);
0029:   });
0030: }
0031: 
0032: async function openScan(scan, chapter = 1, page = 1) {
0033:   state.current = scan;
0034:   $('readerTitle').textContent = scan.title;
0035:   const chaptersData = await api(`/api/scans/${encodeURIComponent(scan.id)}/chapters`);
0036:   state.chapters = (chaptersData.items || []).map(c => c.chapter);
0037:   state.chapterIndex = Math.max(0, state.chapters.indexOf(chapter));
0038:   await loadPages(page);
0039: }
0040: 
0041: async function loadPages(page = 1) {
0042:   if (!state.current || state.chapters.length === 0) return;
0043:   const chapter = state.chapters[state.chapterIndex];
0044:   const pagesData = await api(`/api/scans/${encodeURIComponent(state.current.id)}/chapters/${chapter}/pages`);
0045:   state.pages = pagesData.items || [];
0046:   state.pageIndex = Math.max(0, state.pages.findIndex(p => p.page === page));
0047:   if (state.pageIndex < 0) state.pageIndex = 0;
0048:   renderPage();
0049: }
0050: 
0051: async function renderPage() {
0052:   const page = state.pages[state.pageIndex];
0053:   if (!state.current || !page) return;
0054:   $('page').src = page.imageUrl;
0055:   $('page').classList.remove('hidden');
0056:   $('meta').textContent = `Chapitre ${page.chapter} - page ${page.page} / ${state.pages.length}`;
0057:   await api(`/api/scans/${encodeURIComponent(state.current.id)}/progress?profile=${encodeURIComponent(profile())}`, {
0058:     method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ chapter: page.chapter, page: page.page })
0059:   }).catch(() => {});
0060: }
0061: 
0062: async function nextPage() {
0063:   if (state.pageIndex + 1 < state.pages.length) { state.pageIndex++; return renderPage(); }
0064:   if (state.chapterIndex + 1 < state.chapters.length) { state.chapterIndex++; return loadPages(1); }
0065: }
0066: 
0067: async function prevPage() {
0068:   if (state.pageIndex > 0) { state.pageIndex--; return renderPage(); }
0069:   if (state.chapterIndex > 0) { state.chapterIndex--; await loadPages(999999); state.pageIndex = Math.max(0, state.pages.length - 1); return renderPage(); }
0070: }
0071: 
0072: async function toggleFavorite() {
0073:   if (!state.current) return;
0074:   state.current.favorite = !state.current.favorite;
0075:   await api(`/api/scans/${encodeURIComponent(state.current.id)}/favorite?profile=${encodeURIComponent(profile())}`, {
0076:     method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ favorite: state.current.favorite })
0077:   });
0078:   renderLibrary();
0079: }
0080: 
0081: async function addBookmark() {
0082:   if (!state.current || !state.pages[state.pageIndex]) return;
0083:   const p = state.pages[state.pageIndex];
0084:   const note = prompt('Note du marque-page', '') || '';
0085:   await api(`/api/scans/${encodeURIComponent(state.current.id)}/bookmarks?profile=${encodeURIComponent(profile())}`, {
0086:     method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ chapter: p.chapter, page: p.page, note })
0087:   });
0088: }
0089: 
0090: async function resume() {
0091:   const data = await api(`/api/profiles/${encodeURIComponent(profile())}/history?limit=1`);
0092:   const item = (data.items || [])[0];
0093:   if (!item) return;
0094:   const scan = state.scans.find(s => s.id === item.scanId) || { id: item.scanId, title: item.title };
0095:   openScan(scan, item.chapter, item.page);
0096: }
0097: 
0098: async function searchOcr(query) {
0099:   const data = await api(`/api/search?q=${encodeURIComponent(query)}&limit=8`);
0100:   $('ocrResults').innerHTML = '';
0101:   (data.items || []).forEach(item => {
0102:     const div = document.createElement('div');
0103:     div.className = 'result';
0104:     div.innerHTML = `<strong>${escapeHtml(item.title)}</strong> - ch. ${item.chapter}, p. ${item.page}<br>${escapeHtml(item.snippet)}`;
0105:     div.onclick = () => openScan({ id: item.scanId, title: item.title }, item.chapter, item.page);
0106:     $('ocrResults').appendChild(div);
0107:   });
0108: }
0109: 
0110: function escapeHtml(text) {
0111:   return String(text || '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#039;'}[c]));
0112: }
0113: 
0114: $('refresh').onclick = loadLibrary;
0115: $('resume').onclick = resume;
0116: $('next').onclick = nextPage;
0117: $('prev').onclick = prevPage;
0118: $('favorite').onclick = toggleFavorite;
0119: $('bookmark').onclick = addBookmark;
0120: $('sort').onchange = loadLibrary;
0121: $('favoritesOnly').onchange = loadLibrary;
0122: $('search').addEventListener('keydown', e => { if (e.key === 'Enter') loadLibrary(); });
0123: window.addEventListener('keydown', e => { if (e.key === 'ArrowRight') nextPage(); if (e.key === 'ArrowLeft') prevPage(); });
0124: loadLibrary().catch(err => $('library').innerHTML = `<div class="result">Erreur: ${escapeHtml(err.message)}</div>`);
```
### 5.22 Tests unitaires ajoutes
La suite autonome couvre les briques les plus critiques ajoutees dans cette version.

**Fichier : `tests/run_unit_tests.cpp`, lignes 1-103**

```cpp
0001: #include "application/DownloadQueue.hpp"
0002: #include "application/FileSystemScanDataSource.hpp"
0003: #include "application/OfflineLibrarySync.hpp"
0004: #include "domain/ScanSession.hpp"
0005: #include "infrastructure/JsonScanRepository.hpp"
0006: #include "infrastructure/SimpleJson.hpp"
0007: #include "server/HttpTypes.hpp"
0008: 
0009: #include <atomic>
0010: #include <cassert>
0011: #include <chrono>
0012: #include <filesystem>
0013: #include <fstream>
0014: #include <iostream>
0015: #include <thread>
0016: #include <unistd.h>
0017: 
0018: namespace fs = std::filesystem;
0019: 
0020: static void touch(const fs::path& path) {
0021:     fs::create_directories(path.parent_path());
0022:     std::ofstream out(path, std::ios::binary);
0023:     out << "x";
0024: }
0025: 
0026: static fs::path temp_root(const std::string& suffix = "") {
0027:     auto root = fs::temp_directory_path() / ("scangui-tests-" + std::to_string(::getpid()) + suffix);
0028:     fs::remove_all(root);
0029:     fs::create_directories(root);
0030:     return root;
0031: }
0032: 
0033: int main() {
0034:     const std::string json = R"({"title":"Dr \"Stone\"","chapter":12,"favorite":true,"items":[{"id":"a"},{"id":"b"}]})";
0035:     assert(scangui::json::string_field(json, "title") == "Dr \"Stone\"");
0036:     assert(scangui::json::int_field(json, "chapter") == 12);
0037:     assert(scangui::json::extract_bool(json, "favorite").value_or(false));
0038:     assert(scangui::json::extract_object_array(json).size() == 2);
0039: 
0040:     auto parts = split_path("/api/scans/one%20piece/chapters");
0041:     assert(parts.size() == 4 && parts[2] == "one piece");
0042:     assert(guess_mime_type("1.PNG") == "image/png");
0043: 
0044:     auto root = temp_root();
0045:     touch(root / "scan-a" / "1" / "1.jpg");
0046:     touch(root / "scan-a" / "1" / "2.png");
0047:     touch(root / "scan-a" / "2" / "1.webp");
0048: 
0049:     ScanSession session(root / "scan-a");
0050:     assert(session.current_page().has_value());
0051:     assert(session.next_page()->progress.page == 2);
0052:     assert(session.next_page()->progress.chapter == 2);
0053: 
0054:     JsonScanRepository repo;
0055:     repo.save_progress(root / "scan-a", ScanProgress{2, 1});
0056:     auto loaded = repo.load(root / "scan-a");
0057:     assert(loaded.has_value() && loaded->saveProgress.chapter == 2);
0058: 
0059:     FileSystemScanDataSource source(root);
0060:     auto scans = source.list_scans();
0061:     assert(scans.size() == 1 && scans[0].pageCount == 3);
0062:     source.set_favorite("scan-a", true, "tester");
0063:     assert(source.list_scans("tester")[0].favorite);
0064:     source.add_bookmark("scan-a", ScanProgress{1, 2}, "note", "tester");
0065:     assert(source.list_bookmarks("scan-a", "tester").size() == 1);
0066:     touch(root / "scan-a" / "ocr" / "1" / "2.txt");
0067:     { std::ofstream txt(root / "scan-a" / "ocr" / "1" / "2.txt"); txt << "recherche speciale dans une bulle"; }
0068:     assert(!source.search_text("speciale", 5).empty());
0069: 
0070:     auto offlineRoot = temp_root("-offline");
0071:     OfflineLibrarySync offline(source, offlineRoot);
0072:     auto report = offline.sync_all("tester");
0073:     assert(report.scans == 1);
0074:     assert(fs::exists(offlineRoot / "scan-a" / "1" / "1.jpg"));
0075: 
0076:     std::atomic<int> completed{0};
0077:     std::atomic<int> progressEvents{0};
0078:     DownloadQueue queue([&](const DownloadJobReport& jobReport) {
0079:         assert(jobReport.success);
0080:         ++completed;
0081:     }, [&](const std::string&, int, int, const std::string&) { ++progressEvents; });
0082:     queue.start();
0083:     queue.pause();
0084:     queue.push(DownloadJob{"paused", [&](DownloadJobContext& ctx) { assert(!ctx.cancelled()); ctx.report(1, 1, "paused job"); touch(root / "queue" / "paused.txt"); }, 0});
0085:     std::this_thread::sleep_for(std::chrono::milliseconds(30));
0086:     assert(queue.paused());
0087:     assert(queue.pending_jobs() == 1);
0088:     assert(!fs::exists(root / "queue" / "paused.txt"));
0089:     queue.resume();
0090:     queue.push(DownloadJob{"unit", [&](DownloadJobContext& ctx) { assert(!ctx.cancelled()); ctx.report(1, 1, "unit job"); touch(root / "queue" / "done.txt"); }, 0});
0091:     for (int i = 0; i < 100 && completed.load() < 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
0092:     queue.stop();
0093:     assert(completed.load() == 2);
0094:     assert(progressEvents.load() >= 2);
0095:     assert(fs::exists(root / "queue" / "paused.txt"));
0096:     assert(fs::exists(root / "queue" / "done.txt"));
0097: 
0098:     fs::remove_all(root);
0099:     fs::remove_all(offlineRoot);
0100:     fs::remove_all("cache/profiles/tester");
0101:     std::cout << "All ScanGUI unit tests passed" << std::endl;
0102:     return 0;
0103: }
```
## 6. Verification realisee
Les validations suivantes ont ete executees dans l'environnement de travail :

| Verification | Resultat |
|---|---|
| `make tests` | OK - `All ScanGUI unit tests passed` |
| `make server` | OK - serveur C++ lie avec `libpq` |
| CMake sans GUI avec `clang++`/`lld` | OK - `ctest` passe |
| Compilation GTK | Non executable ici : `gtkmm-3.0` absent du sandbox |
| PDF | Compile et rendu en images pour controle visuel |
Pour compiler la GUI sur une machine Debian/Ubuntu, installe par exemple `libgtkmm-3.0-dev`, puis lance `make`.
## 7. Limites restantes assumées
La version v6 rend toutes les evolutions utilisables ou branchees dans le code, mais certaines restent volontairement simples : l'OCR ne fait pas de segmentation IA des bulles, le JSON reste un parseur simple interne plutot qu'une dependance `nlohmann/json`, et le client web est un client local leger plutot qu'une application React complete. Ces choix gardent le projet compilable et pedagogique tout en montrant clairement ou placer une evolution industrielle.
