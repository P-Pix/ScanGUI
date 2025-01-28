# ScanGUI

ScanGUI est une application en C++ ayant pour intéret de télécharger des mangas et des les rendres accéssible hors ligne et d'avoir une interface facile d'utilisation pour la lecture de scan.

## Fonctions disponibles

Les différentes fonctions disponibles actuellement sont :
- Le téléchargement de scan sur www.lelscans.com
- La sauvegarde automatique de l'avancé dans la lecture sans l'intervention du lecteur
- La reprise automatique de la lecture sans intervention de l'utilisateur
- Le déplacement instantané vers une page précise en l'indiquant dans le champs réservé

## Objectifs actuels

### Profil
Ajouter une fenetre à l'ouverture du programme qui demande le profil de l'utilisateur pour pouvoir avoir plusieurs sauvegarde de l'avancé d'une lecture.

Avoir un bouton sur cette fenetre pour ajouter un profil

Si le profil n'éxiste pas dans `data.json` lors de la selection d'une lecture alors le créer et le mettre à chapitre 1, page 0

### Download Scan

Augmenter le panel des site de scans à télécharger

### Améliorer le code

Réparer les doawnload sur www.lelscans.com

Refaire le code pour pouvoir avoir une lecture plus clair du code et pouvoir faire des ajours sur le long terme