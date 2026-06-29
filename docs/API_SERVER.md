# ScanGUIServer - API locale C++ + PostgreSQL

## Objectif

`ScanGUIServer` ajoute une couche client/serveur locale au projet. Le dossier `scan/` n'est plus seulement une structure lue directement par l'interface : il peut etre indexe dans PostgreSQL puis expose par une API HTTP locale ecrite en C++ sans framework web.

L'objectif est volontairement pedagogique et technique : comprendre comment fonctionne un serveur HTTP minimal, une API REST, une couche de persistance PostgreSQL et une abstraction client, sans cacher toute la logique derriere FastAPI, Express ou NestJS.

## Architecture ajoutee

```txt
server_src/
  main_server.cpp             # Point d'entree du serveur local
  HttpServer.cpp              # Serveur HTTP minimal sur sockets POSIX
  HttpTypes.cpp               # Requetes/reponses HTTP + helpers JSON/MIME
  PostgresScanDatabase.cpp    # Acces PostgreSQL via libpq
  ScanLibraryIndexer.cpp      # Indexation du dossier scan/ vers la BDD
  ScanApiController.cpp       # Routes REST de l'API

include/server/
  HttpServer.hpp
  HttpTypes.hpp
  PostgresScanDatabase.hpp
  ScanLibraryIndexer.hpp
  ScanApiController.hpp

include/application/
  ScanDataSource.hpp          # Abstraction de source de scans
  FileSystemScanDataSource.hpp# Source directe fichiers
  ApiScanDataSource.hpp       # Source distante via API locale
```

## Pourquoi c'est interessant techniquement

- Serveur HTTP local implemente en C++ avec sockets POSIX.
- Routage REST minimal fait a la main.
- Reponses JSON et images binaires gerees sans framework web.
- PostgreSQL utilise via `libpq`, donc pas de BDD from scratch.
- Protection anti path traversal avant de servir une image.
- API testable avec `curl`.
- Abstraction `ScanDataSource` pour separer l'interface de la source reelle des scans.

## Dependances

```bash
sudo apt install g++ make cmake pkg-config libpq-dev libcurl4-openssl-dev
```

Pour le client GTK :

```bash
sudo apt install libgtkmm-3.0-dev
```

## Lancer PostgreSQL rapidement

```bash
docker compose -f docker-compose.postgres.yml up -d
```

La configuration par defaut est :

```txt
base     : scangui
user     : scangui
password : scangui
host     : 127.0.0.1
port     : 5432
```

## Compiler le serveur

Avec Makefile :

```bash
make server
```

Avec CMake :

```bash
cmake -S . -B build-cmake
cmake --build build-cmake --target ScanGUIServer
```

## Lancer le serveur

```bash
cp config/scangui.env.example .env
export SCANGUI_DATABASE_URL="postgresql://scangui:scangui@127.0.0.1:5432/scangui"
export SCANGUI_SCAN_ROOT="scan"
export SCANGUI_PORT=8787
./bin/ScanGUIServer
```

Par defaut, le serveur ecoute sur :

```txt
http://127.0.0.1:8787
```

Au demarrage, il initialise le schema SQL puis synchronise automatiquement le dossier `scan/` si `SCANGUI_SYNC_ON_START=1`.

## Endpoints principaux

### Sante du serveur

```bash
curl http://127.0.0.1:8787/api/health
```

Reponse :

```json
{
  "status": "ok",
  "service": "ScanGUIServer"
}
```

### Synchroniser le dossier scan vers PostgreSQL

```bash
curl -X POST http://127.0.0.1:8787/api/admin/sync
```

### Lister les scans

```bash
curl http://127.0.0.1:8787/api/scans
```

### Recuperer un scan

```bash
curl http://127.0.0.1:8787/api/scans/one-piece
```

### Lister les chapitres

```bash
curl http://127.0.0.1:8787/api/scans/one-piece/chapters
```

### Lister les pages d'un chapitre

```bash
curl http://127.0.0.1:8787/api/scans/one-piece/chapters/1/pages
```

### Recuperer une image

```bash
curl -o page.jpg http://127.0.0.1:8787/api/scans/one-piece/chapters/1/pages/1/image
```

### Lire la progression

```bash
curl "http://127.0.0.1:8787/api/scans/one-piece/progress?profile=default"
```

### Sauvegarder la progression

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"chapter":1,"page":12}' \
  "http://127.0.0.1:8787/api/scans/one-piece/progress?profile=default"
```

## Schema PostgreSQL

Le schema est dans :

```txt
migrations/001_init.sql
```

Tables principales :

- `scans` : bibliotheque des lectures.
- `chapters` : chapitres indexes.
- `pages` : pages/images exposees par l'API.
- `reading_progress` : progression par profil.

## Limites assumees

Ce serveur est volontairement minimal :

- HTTP/1.1 simple, une connexion par requete.
- Pas de TLS, car serveur local uniquement.
- Pas d'authentification, car usage personnel local.
- Pas fait pour Internet ou production.
- Pas de framework web moderne, volontairement, pour montrer la comprehension technique.

Pour un vrai produit distribue, il faudrait ajouter authentification, TLS, logs structures, pagination, tests d'integration, pool de connexions PostgreSQL, observabilite et limites de debit.
