# ScanGUI

**ScanGUI** est un lecteur desktop C++/GTK pour séries d’images organisées en chapitres et pages.

Le projet vient d’un ancien logiciel personnel de lecture de scans/mangas développé pendant mes études, puis repris comme base d’entraînement pour travailler une architecture C++ plus propre : séparation progressive du domaine, interface GTK, serveur local C++, API HTTP et indexation PostgreSQL.

L’objectif du projet est double :

1. disposer d’un lecteur local simple pour lire des images séquentielles hors ligne ;
2. expérimenter une architecture plus technique avec un serveur local C++ écrit sans framework web moderne.

> Ce projet est un projet personnel et pédagogique. Le serveur HTTP est volontairement minimal et local. Il n’est pas conçu pour être exposé sur Internet.

---

## Sommaire

- [Fonctionnalités](#fonctionnalités)
- [Architecture générale](#architecture-générale)
- [Structure du projet](#structure-du-projet)
- [Prérequis](#prérequis)
- [Installation rapide](#installation-rapide)
- [Lancer l’application desktop GTK](#lancer-lapplication-desktop-gtk)
- [Lancer PostgreSQL](#lancer-postgresql)
- [Lancer le serveur local C++](#lancer-le-serveur-local-c)
- [Tester l’API locale](#tester-lapi-locale)
- [Utilisation du logiciel](#utilisation-du-logiciel)
- [Format attendu des scans](#format-attendu-des-scans)
- [Variables d’environnement](#variables-denvironnement)
- [Commandes utiles](#commandes-utiles)
- [Limites connues](#limites-connues)
- [Pistes d’évolution](#pistes-dévolution)

---

## Evolutions de cette version

Cette version correspond a la V6 finale demandee. Elle transforme ScanGUI en lecteur local/API/offline plus complet :

- interface GTK avec selecteur **Local direct / Serveur local / Offline** ;
- bibliotheque visuelle avec cartes, couvertures, recherche, tris, favoris, progression et bouton de reprise ;
- lecteur enrichi : page simple, double page, mode webtoon vertical, plein ecran, zoom, deplacement a la souris, slider, miniatures et raccourcis ;
- source de donnees unifiee `ScanDataSource` avec `FileSystemScanDataSource` et `ApiScanDataSource` ;
- cache API local dans `cache/api` ;
- synchronisation offline du serveur vers `scan/` via `OfflineLibrarySync` ;
- file de taches de fond `DownloadQueue` avec progression, pause/reprise cooperative, annulation et retry ;
- profils locaux et serveur, favoris, marque-pages et historique ;
- serveur local securise : mutex de controleur, controle anti path traversal, CORS configurable, token admin optionnel, refus admin si non local ;
- endpoints `/api/version`, `/api/stats`, `/api/offline/manifest`, `/api/search` et front web local ;
- pipeline OCR/recherche par sidecar `.txt` ou commande externe `SCANGUI_TESSERACT_CMD` ;
- helper JSON centralise `SimpleJson` ;
- tests unitaires executables avec `make tests` et CTest.

La documentation detaillee finale se trouve dans :

- `docs/API_SERVER.md` ;
- `docs/EVOLUTIONS.md` ;
- `docs/SCANGUI_GUIDE_TECHNIQUE.md` ;
- `docs/SCANGUI_GUIDE_TECHNIQUE.tex` ;
- `docs/SCANGUI_GUIDE_TECHNIQUE.pdf`.


## Fonctionnalités

### Application desktop

- Lecture d’images locales organisées par scans, chapitres et pages.
- Interface graphique native en C++ avec GTKmm.
- Navigation rapide au clavier et à la souris.
- Reprise automatique de lecture via `data.json`, API ou profil local.
- Sauvegarde de la dernière page consultée par profil.
- Bibliothèque visuelle avec recherche, tri, favoris, couvertures et reprise.
- Lecteur enrichi avec plein écran, double page, webtoon, zoom, miniatures, slider, marque-pages et historique.
- Synchronisation offline depuis le serveur vers le dossier local `scan/`.

### Serveur local C++

- Serveur HTTP local écrit en C++ sans framework web.
- Utilisation de sockets POSIX.
- Routage REST minimal fait à la main.
- Exposition de la bibliothèque locale via API HTTP.
- Indexation des scans dans PostgreSQL.
- Service des images par endpoint HTTP avec controle de chemin avant lecture.
- Sauvegarde et lecture de la progression via API.
- Profils, favoris, marque-pages, historique, recherche OCR et manifeste offline.
- Protection de base contre le path traversal avant de servir les fichiers.
- Token admin optionnel et refus des routes admin si le serveur n’est pas local.

### Base de données PostgreSQL

- Indexation des scans disponibles.
- Indexation des chapitres.
- Indexation des pages.
- Sauvegarde de la progression de lecture par profil.
- Favoris, marque-pages, historique et textes OCR indexés.

---

## Architecture générale

Le projet peut être utilisé de deux façons.

### Mode simple : application GTK directe

```txt
Utilisateur
   |
   v
Application GTK C++
   |
   v
Dossier local scan/
```

Dans ce mode, l’application lit directement les dossiers locaux.

C’est le mode le plus simple pour utiliser le logiciel rapidement.

### Mode API locale : serveur C++ + PostgreSQL

```txt
Utilisateur
   |
   v
Application desktop / client API
   |
   v
API HTTP locale ScanGUIServer
   |
   +--> PostgreSQL
   |
   +--> Dossier local scan/
```

Dans ce mode, le serveur local indexe le dossier `scan/` dans PostgreSQL puis expose les scans via une API REST.

Cette version contient déjà l’abstraction `ScanDataSource`, avec :

- `FileSystemScanDataSource` pour une lecture directe depuis les fichiers ;
- `ApiScanDataSource` pour une lecture via le serveur local.

Dans cette version, l’interface GTK passe par `ScanDataSource` et peut basculer entre **Local direct**, **Serveur local** et **Offline** depuis le sélecteur de mode.

---

## Structure du projet

```txt
ScanGUI_refactored/
├── asset/                         # Icônes et ressources graphiques
├── config/
│   └── scangui.env.example         # Exemple de configuration serveur
├── docs/
│   ├── API_SERVER.md               # Documentation détaillée de l’API locale
│   └── REFACTORING.md              # Notes de refactorisation
├── include/
│   ├── application/                # Abstractions applicatives
│   ├── domain/                     # Objets métier et navigation
│   ├── infrastructure/             # HTTP, fichiers, parsing externe
│   ├── server/                     # Headers du serveur HTTP local
│   └── *.hpp                       # Widgets GTK historiques
├── migrations/
│   └── 001_init.sql                # Schéma PostgreSQL
├── server_src/                     # Implémentation du serveur HTTP C++
├── src/                            # Application GTK et services partagés
├── docker-compose.postgres.yml     # PostgreSQL local via Docker
├── CMakeLists.txt                  # Build CMake
├── Makefile                        # Build simple Make
└── README.md
```

---

## Prérequis

Le projet est prévu principalement pour Linux ou WSL.

Le serveur local utilise des sockets POSIX. Pour Windows natif, une adaptation réseau serait nécessaire.

### Dépendances système Debian / Ubuntu

```bash
sudo apt update
sudo apt install -y \
  g++ \
  make \
  cmake \
  pkg-config \
  libgtkmm-3.0-dev \
  libcurl4-openssl-dev \
  libpq-dev
```

### Dépendance Docker optionnelle

Docker est seulement nécessaire pour lancer PostgreSQL rapidement :

```bash
docker --version
docker compose version
```

---

## Installation rapide

Créer les dossiers nécessaires :

```bash
make init
```

Compiler l’application desktop :

```bash
make
```

Lancer l’application :

```bash
make run
```

---

## Lancer l’application desktop GTK

### Avec Makefile

```bash
make
./bin/ScanGUI
```

ou directement :

```bash
make run
```

### Avec CMake

```bash
cmake -S . -B build-cmake
cmake --build build-cmake --target ScanGUI
./build-cmake/ScanGUI
```

### Compiler uniquement l’interface GTK

```bash
cmake -S . -B build-gui \
  -DSCANGUI_BUILD_GUI=ON \
  -DSCANGUI_BUILD_SERVER=OFF

cmake --build build-gui --target ScanGUI
./build-gui/ScanGUI
```

---

## Lancer PostgreSQL

Le projet fournit un `docker-compose.postgres.yml` pour démarrer une base locale.

```bash
docker compose -f docker-compose.postgres.yml up -d
```

Configuration par défaut :

```txt
Base     : scangui
User     : scangui
Password : scangui
Host     : 127.0.0.1
Port     : 5432
```

Vérifier que le conteneur tourne :

```bash
docker ps
```

Arrêter PostgreSQL :

```bash
docker compose -f docker-compose.postgres.yml down
```

Supprimer aussi les données PostgreSQL locales :

```bash
docker compose -f docker-compose.postgres.yml down -v
```

---

## Lancer le serveur local C++

Le serveur local se nomme `ScanGUIServer`.

Il expose le dossier `scan/` sous forme d’API HTTP et utilise PostgreSQL pour indexer les scans, chapitres, pages et progressions.

### 1. Démarrer PostgreSQL

```bash
docker compose -f docker-compose.postgres.yml up -d
```

### 2. Charger la configuration d’environnement

Le fichier d’exemple est disponible ici :

```txt
config/scangui.env.example
```

Pour l’utiliser dans le shell courant :

```bash
set -a
source config/scangui.env.example
set +a
```

Les variables peuvent aussi être exportées manuellement :

```bash
export SCANGUI_DATABASE_URL="postgresql://scangui:scangui@127.0.0.1:5432/scangui"
export SCANGUI_SCAN_ROOT="scan"
export SCANGUI_HOST="127.0.0.1"
export SCANGUI_PORT="8787"
export SCANGUI_SYNC_ON_START="1"
```

### 3. Compiler le serveur

Avec Makefile :

```bash
make server
```

Avec CMake :

```bash
cmake -S . -B build-server \
  -DSCANGUI_BUILD_GUI=OFF \
  -DSCANGUI_BUILD_SERVER=ON

cmake --build build-server --target ScanGUIServer
```

### 4. Démarrer le serveur

Avec Makefile :

```bash
make run-server
```

Ou directement :

```bash
./bin/ScanGUIServer
```

Avec CMake :

```bash
./build-server/ScanGUIServer
```

Par défaut, le serveur écoute ici :

```txt
http://127.0.0.1:8787
```

Au démarrage, si `SCANGUI_SYNC_ON_START=1`, le serveur synchronise automatiquement le dossier `scan/` vers PostgreSQL.

---

## Tester l’API locale

### Vérifier la santé du serveur

```bash
curl http://127.0.0.1:8787/api/health
```

Réponse attendue :

```json
{
  "status": "ok",
  "service": "ScanGUIServer"
}
```

### Synchroniser manuellement la bibliothèque

À utiliser après avoir ajouté ou supprimé des dossiers dans `scan/` :

```bash
curl -X POST http://127.0.0.1:8787/api/admin/sync
```

### Lister les scans

```bash
curl http://127.0.0.1:8787/api/scans
```

### Lire le détail d’un scan

```bash
curl http://127.0.0.1:8787/api/scans/<scan_id>
```

Exemple :

```bash
curl http://127.0.0.1:8787/api/scans/one-piece
```

### Lister les chapitres d’un scan

```bash
curl http://127.0.0.1:8787/api/scans/<scan_id>/chapters
```

Exemple :

```bash
curl http://127.0.0.1:8787/api/scans/one-piece/chapters
```

### Lister les pages d’un chapitre

```bash
curl http://127.0.0.1:8787/api/scans/<scan_id>/chapters/<chapitre>/pages
```

Exemple :

```bash
curl http://127.0.0.1:8787/api/scans/one-piece/chapters/1/pages
```

### Télécharger une image via l’API

```bash
curl -o page.jpg \
  http://127.0.0.1:8787/api/scans/<scan_id>/chapters/<chapitre>/pages/<page>/image
```

Exemple :

```bash
curl -o page.jpg \
  http://127.0.0.1:8787/api/scans/one-piece/chapters/1/pages/1/image
```

### Lire la progression d’un profil

```bash
curl "http://127.0.0.1:8787/api/scans/<scan_id>/progress?profile=default"
```

Exemple :

```bash
curl "http://127.0.0.1:8787/api/scans/one-piece/progress?profile=default"
```

### Sauvegarder la progression

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"chapter":1,"page":12}' \
  "http://127.0.0.1:8787/api/scans/<scan_id>/progress?profile=default"
```

Exemple :

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"chapter":1,"page":12}' \
  "http://127.0.0.1:8787/api/scans/one-piece/progress?profile=default"
```

---

## Utilisation du logiciel

### 1. Préparer une bibliothèque locale

Créer un dossier de scan dans `scan/` :

```bash
mkdir -p scan/exemple/1
```

Ajouter des images dans le chapitre :

```txt
scan/exemple/1/1.jpg
scan/exemple/1/2.jpg
scan/exemple/1/3.jpg
```

La structure complète ressemble à ceci :

```txt
scan/
└── exemple/
    ├── data.json
    ├── 1/
    │   ├── 1.jpg
    │   ├── 2.jpg
    │   └── 3.jpg
    └── 2/
        ├── 1.jpg
        └── 2.jpg
```

Le fichier `data.json` peut être créé par l’application. Il sert à conserver la progression et les informations de téléchargement.

### 2. Lancer l’application GTK

```bash
make run
```

### 3. Ouvrir une lecture

Dans l’interface :

1. ouvrir le menu `File` ;
2. choisir `Open` ;
3. sélectionner le scan voulu ;
4. naviguer dans les pages.

### 4. Navigation

| Action        | Effet                      |
| ------------- | -------------------------- |
| Flèche droite | Page suivante              |
| Flèche gauche | Page précédente            |
| Clic gauche   | Page suivante              |
| Clic droit    | Page précédente            |
| `+`           | Zoom avant                 |
| `-`           | Zoom arrière               |
| `File > Save` | Sauvegarder la progression |
| `File > Quit` | Quitter l’application      |

### 5. Mettre à jour l’index API après ajout de fichiers

Si le serveur est utilisé, synchroniser PostgreSQL après ajout de nouveaux scans :

```bash
curl -X POST http://127.0.0.1:8787/api/admin/sync
```

---

## Format attendu des scans

Le format attendu est volontairement simple :

```txt
scan/<nom_du_scan>/<numero_chapitre>/<numero_page>.<extension>
```

Exemple :

```txt
scan/one-piece/1/1.jpg
scan/one-piece/1/2.jpg
scan/one-piece/2/1.jpg
```

Extensions prises en charge par l’indexation :

- `.jpg`
- `.jpeg`
- `.png`
- `.webp`

Le rendu réel dépend aussi des formats supportés par GDK/GTK sur la machine.

---

## Variables d’environnement

Le serveur lit sa configuration depuis les variables suivantes :

| Variable                | Valeur par défaut                                     | Rôle                                                         |
| ----------------------- | ----------------------------------------------------- | ------------------------------------------------------------ |
| `SCANGUI_DATABASE_URL`  | `postgresql://scangui:scangui@127.0.0.1:5432/scangui` | URL de connexion PostgreSQL                                  |
| `SCANGUI_SCAN_ROOT`     | `scan`                                                | Dossier racine des scans                                     |
| `SCANGUI_HOST`          | `127.0.0.1`                                           | Adresse d’écoute du serveur                                  |
| `SCANGUI_PORT`          | `8787`                                                | Port HTTP local                                              |
| `SCANGUI_SYNC_ON_START` | `1`                                                   | Synchronise automatiquement au démarrage si différent de `0` |

Charger le fichier d’exemple :

```bash
set -a
source config/scangui.env.example
set +a
```

---

## Commandes utiles

### Initialiser les dossiers

```bash
make init
```

### Compiler l’application GTK

```bash
make
```

### Lancer l’application GTK

```bash
make run
```

### Compiler le serveur C++

```bash
make server
```

### Lancer le serveur C++

```bash
make run-server
```

### Lancer PostgreSQL

```bash
docker compose -f docker-compose.postgres.yml up -d
```

### Arrêter PostgreSQL

```bash
docker compose -f docker-compose.postgres.yml down
```

### Nettoyer les fichiers de build

```bash
make clean
```

### Nettoyer build, binaires, cache et dossier scan

Attention : cette commande supprime aussi le dossier `scan/`.

```bash
make mrproper
```

---

## Exemple de scénario complet

### 1. Démarrer PostgreSQL

```bash
docker compose -f docker-compose.postgres.yml up -d
```

### 2. Préparer les variables serveur

```bash
set -a
source config/scangui.env.example
set +a
```

### 3. Compiler le serveur

```bash
make server
```

### 4. Lancer le serveur

```bash
make run-server
```

### 5. Tester l’API dans un autre terminal

```bash
curl http://127.0.0.1:8787/api/health
curl -X POST http://127.0.0.1:8787/api/admin/sync
curl http://127.0.0.1:8787/api/scans
```

### 6. Lancer l’application desktop

```bash
make run
```

---

## Limites connues

- Le serveur HTTP est volontairement minimal et local.
- Pas de TLS.
- Pas d’authentification.
- Pas de pagination API.
- Pas de pool de connexions PostgreSQL.
- Le parsing JSON côté client API reste volontairement simple.
- L’interface GTK actuelle reste principalement branchée sur la lecture directe du dossier `scan/`.
- L’API locale et `ApiScanDataSource` servent de base pour une évolution client/serveur plus complète.
- Le projet n’est pas destiné à être distribué comme produit fini.

---

## Licence

Projet personnel d’apprentissage.
