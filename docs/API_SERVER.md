# API serveur - ScanGUI v6

Base locale conseillee : `http://127.0.0.1:8787`.

## Routes publiques

| Route | Methode | Role |
|---|---:|---|
| `/` | GET | sert le client web local |
| `/web/index.html`, `/web/app.js`, `/web/style.css` | GET | fichiers statiques du client web |
| `/api/health` | GET | etat minimal du serveur |
| `/api/version` | GET | version applicative et schema API |
| `/api/stats` | GET | compte scans, chapitres, pages, profils, favoris, marque-pages, textes OCR |
| `/api/scans?profile=default&q=&favorites=false&sort=name&limit=50&offset=0` | GET | bibliotheque filtree, triee et paginee |
| `/api/scans/{scanId}` | GET | details d'un scan |
| `/api/scans/{scanId}/chapters` | GET | chapitres d'un scan |
| `/api/scans/{scanId}/chapters/{chapter}/pages` | GET | pages d'un chapitre |
| `/api/scans/{scanId}/chapters/{chapter}/pages/{page}/image` | GET | image de page avec controle anti path traversal |
| `/api/scans/{scanId}/progress?profile=default` | GET, POST, PUT | progression de lecture par profil |
| `/api/profiles` | GET, POST | liste et creation/mise a jour des profils |
| `/api/profiles/{profile}/favorites` | GET | favoris d'un profil |
| `/api/scans/{scanId}/favorite?profile=default` | POST, DELETE | ajout/retrait favori |
| `/api/scans/{scanId}/bookmarks?profile=default` | GET, POST | marque-pages |
| `/api/profiles/{profile}/history` | GET | historique de lecture |
| `/api/offline/manifest` | GET | manifeste pour synchronisation offline |
| `/api/search?q=texte&profile=default&limit=20` | GET | recherche dans textes OCR indexes |
| `/api/scans/{scanId}/chapters/{chapter}/summary` | GET | extrait/resume simple depuis OCR |

## Routes admin

| Route | Methode | Protection | Role |
|---|---:|---|---|
| `/api/admin/sync` | POST | localhost + token optionnel | reindexation de la bibliotheque disque vers PostgreSQL |
| `/api/admin/ocr` | POST | localhost + token optionnel | indexation OCR/sidecar vers `page_texts` |

Si `SCANGUI_ADMIN_TOKEN` est defini, envoyer l'en-tete `x-scangui-admin-token`.
Les routes admin sont refusees quand le serveur n'est pas lie a `127.0.0.1` ou `localhost`.

## Variables d'environnement utiles

| Variable | Defaut | Role |
|---|---|---|
| `SCANGUI_HOST` | `127.0.0.1` | adresse d'ecoute |
| `SCANGUI_PORT` | `8787` | port HTTP |
| `SCANGUI_SCAN_ROOT` | `scan` | dossier local des scans |
| `SCANGUI_DATABASE_URL` | selon configuration | connexion PostgreSQL |
| `SCANGUI_CORS_ORIGIN` | `*` | origine CORS |
| `SCANGUI_ADMIN_TOKEN` | vide | protection admin optionnelle |
| `SCANGUI_TESSERACT_CMD` | vide | commande OCR externe, avec `{image}` remplacable |
