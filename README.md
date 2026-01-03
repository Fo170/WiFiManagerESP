# WiFiManagerESP
Bibliothèque pour la gestion simplifiée des connexions WiFi sur ESP8266 et ESP32 avec support du mode point d'accès

[![PlatformIO Registry](https://badges.registry.platformio.org/packages/olivierfournet/library/WiFiManagerESP.svg)](https://registry.platformio.org/libraries/olivierfournet/WiFiManagerESP)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Bibliothèque Arduino/PlatformIO pour la gestion simplifiée des connexions WiFi sur ESP8266 et ESP32, avec support du mode point d'accès (AP).

## ✨ Fonctionnalités

- ✅ Support ESP8266 et ESP32
- ✅ Connexion WiFi automatique avec reconnexion
- ✅ Mode point d'accès (AP) simultané
- ✅ Gestion des événements WiFi
- ✅ Hostname personnalisable
- ✅ Affichage détaillé des informations réseau
- ✅ Callbacks pour les événements WiFi
- ✅ Timeout configurable

## 📦 Installation

### PlatformIO

Ajoutez la dépendance dans votre fichier `platformio.ini` :

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
lib_deps = 
    https://github.com/Fo170/WiFiManagerESP@^0.5.0

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    https://github.com/Fo170/WiFiManagerESP@^0.5.0
```

## 📚 API Principale
Configuration
void begin(bool enableAP = false, uint32_t timeout = 10000)

void begin(const char* ssid, const char* password, bool enableAP = false, uint32_t timeout = 10000)

void setCredentials(const char* ssid, const char* password)

void setAPCredentials(const char* ap_ssid, const char* ap_password = "password123")

void setHostname(const char* hostname)

void setHostnamePrefix(const char* prefix)

Informations réseau
bool isConnected()

String getLocalIP()

String getGatewayIP()

String getDnsIP()

String getMacAddress()

String getSSID()

int8_t getRSSI()

String getHostname()

String getStatusText()

Gestion de la connexion
void reconnect()

void disconnect()

int updateStatus()

void printStatus(bool detailed = false)

## 🎯 Exemples
Consultez le dossier examples/ pour des exemples complets :

Basic - Connexion WiFi simple

Utilisation Rapide - Mode client + point d'accès

## 🛠️ Configuration
Mode point d'accès (AP)
Activez le mode AP en passant true à la méthode begin() :

cpp
wifiManager.begin("mon_wifi", "mon_mdp", true);
Ou configurez les credentials AP séparément :

cpp
wifiManager.setAPCredentials("Mon_AP_ESP", "motdepasse123");
wifiManager.begin(true);
Hostname personnalisé
cpp
wifiManager.setHostname("mon-esp32-personnalise");
Ou utilisez un préfixe avec ID unique :

cpp
wifiManager.setHostnamePrefix("Capteur_");
// Résultat: Capteur_ABC123

## 🔧 Compatibilité
Plateforme	Supporté	Notes
ESP32	✅	Pleinement supporté
ESP8266	✅	Pleinement supporté
Autres	❌	Non supporté

## 📄 Licence
Ce projet est sous licence GPL-3.0. Voir le fichier LICENSE pour plus de détails.

## 👤 Auteur
Olivier FOURNET

Email: olivier.fournet@free.fr

GitHub: @Fo170

## 🤝 Contribution
Les contributions sont les bienvenues ! N'hésitez pas à ouvrir une issue ou une pull request.

## 📞 Support
Vérifiez la documentation et les exemples

Recherchez les issues existantes

Ouvrez une nouvelle issue si nécessaire

## 📈 Versions
v0.5.0 - Première version publique

Support ESP8266 et ESP32

Mode AP + STA simultané

Gestion des événements WiFi

Reconnexion automatique
