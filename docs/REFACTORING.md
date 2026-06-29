# Notes de refactorisation

## Objectif

Transformer un prototype C++/GTK fonctionnel en base plus professionnelle sans changer brutalement de technologie.

## Avant

- `Main_Window` contenait l'UI, la navigation, la sauvegarde JSON, la mise a jour et la logique fichier.
- `Scan` etait a la fois widget GTK, modele de lecture et moteur de navigation.
- Les telechargements passaient par `wget`, `system()` et `popen()`.
- Plusieurs cas pouvaient provoquer des crashs : saisie non numerique, dossier vide, page absente, image corrompue.

## Apres

- `ScanSession` gere la navigation sans dependance GTK.
- `JsonScanRepository` isole la persistance `data.json`.
- `CurlHttpClient` gere le reseau sans shell.
- `LelScansProvider` isole le parsing specifique a une source externe.
- `ScanUpdater` contient le cas d'utilisation de mise a jour.
- `Main_Window` orchestre l'interface et appelle les services.

## Limites restantes

- La mise a jour reste synchrone : pendant le telechargement, l'UI peut encore etre bloquee.
- Le parsing HTML reste dependant de la structure du site source.
- Le projet reste lie a GTKmm 3, alors que GTKmm 4 ou Qt 6 seraient plus interessants pour une evolution moderne.

## Prochaine etape prioritaire

Ajouter des tests unitaires sur les classes sans GTK :

- `ScanSession` ;
- `JsonScanRepository` ;
- `LelScansProvider`.

C'est le meilleur moyen de transformer ce vieux projet en demonstration de refactorisation professionnelle.

## Evolution API locale C++ / PostgreSQL

Une nouvelle etape de refactorisation ajoute une architecture client/serveur locale :

- `ScanGUIServer` : serveur HTTP local ecrit en C++ sans framework web.
- `PostgresScanDatabase` : acces PostgreSQL avec `libpq`.
- `ScanLibraryIndexer` : synchronisation du dossier `scan/` vers les tables `scans`, `chapters`, `pages`.
- `ScanApiController` : endpoints REST pour recuperer scans, chapitres, pages, images et progression.
- `ScanDataSource` : abstraction applicative permettant de consommer les scans depuis le filesystem ou depuis l'API.

Cette evolution garde le projet dans une logique d'apprentissage avancee : elle montre la comprehension des couches HTTP, routeur, BDD, JSON et exposition d'images binaires, tout en restant honnete sur le fait qu'un vrai produit public utiliserait plutot un framework serveur mature.
