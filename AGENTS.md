# AGENTS.md - WiFiManagerESP

## Règles générales

- **Langue**: Utiliser le français pour toutes les réponses
- **Fichiers sources**: Un seul fichier `.h` (avec implémentation inline)
- **Version**: v0.5.0

## Auteur

- **Nom**: FOURNET
- **Prénom**: Olivier
- **Email**: olivier.fournet@free.fr
- **Compte GitHub**: https://github.com/Fo170/

## Structure du projet

- **Fichier principal**: `src/WiFiManagerESP.h`
- **Dossier examples**: `examples/Basic/`, `examples/UtilisationRapide/`
- **Fichiers PlatformIO**: `library.json`, `library.properties`

## Plateformes supportées

- ESP8266
- ESP32

## Dépendances

- ArduinoJson (`bblanchon/ArduinoJson@^6.21.0`)

## Installation

Via PlatformIO dans `platformio.ini`:
```ini
lib_deps = https://github.com/Fo170/WiFiManagerESP@^0.5.0
```

Utilisation:
```cpp
#include <WiFiManagerESP.h>
WiFiManagerESP wifiManager;
```

## Documentation

- Le fichier `README.md` doit contenir toutes les explications nécessaires
- Utiliser des images explicatives (gif, jpg, png) si disponibles
- Pour les schémas, générer des fichiers SVG séparés et les inclure avec `![description](schema.svg)`
- Inclure les liens internet sources

## License

- **GNU General Public License v3.0** (GPL-3.0-only)

## GitHub

- **NE JAMAIS** publier `AGENTS.md` sur GitHub
- **NE JAMAIS** publier `CLAUDE.md` sur GitHub
- **NE JAMAIS** publier `copilot-instructions.md` sur GitHub

## Synchronisation

- Ces fichiers **doivent jamais** disparaître localement lors des synchronisations GitHub
- Créer un fichier `.gitignore` à la racine pour les exclure

## Commandes

Aucun script de build/test. Tester en compilant un projet PlatformIO qui utilise la librairie.