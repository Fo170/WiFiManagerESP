# WiFiManagerESP

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Arduino Badge](https://img.shields.io/badge/framework-arduino-brightgreen?logo=arduino.svg)](https://www.arduino.cc/)
[![PlatformIO](https://img.shields.io/badge/platformio-ESP8266,ESP32-violet?logo=platformio)](https://platformio.org/)
[![Arduino Library Badge](https://www.ardu-badge.com/badge/WiFiManagerESP.svg)](https://github.com/Fo170?tab=repositories)

Bibliothèque Arduino/PlatformIO pour la gestion simplifiée des connexions WiFi sur ESP8266 et ESP32, avec support du mode point d'accès (AP) et **gestion multi-réseaux avec basculement automatique**.

Version 0.7.1

## ✨ Fonctionnalités

- ✅ Support ESP8266 et ESP32
- ✅ **Connexion WiFi multi-réseaux avec basculement automatique (failover)**
- ✅ **Historique des connexions avec timestamps**
- ✅ **Priorisation des réseaux (ordre de tentative configurable)**
- ✅ Reconnexion automatique intelligente (pas de conflit avec multi-réseaux)
- ✅ Mode point d'accès (AP) simultané
- ✅ Gestion des événements WiFi
- ✅ Hostname personnalisable
- ✅ Affichage détaillé des informations réseau
- ✅ Callbacks pour les événements WiFi
- ✅ Timeout configurable
- ✅ Compteur d'échecs par réseau avec cooldown

## 📦 Installation

### PlatformIO

Ajoutez la dépendance dans votre fichier `platformio.ini` :

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    https://github.com/Fo170/WiFiManagerESP@^0.7.1
```

## 📚 API Principale

### Configuration multi-réseaux (nouveau)

```cpp
bool addNetwork(const char* ssid, const char* password, int priority = 0)
void clearNetworks()
int getNetworkCount()
int getCurrentNetworkIndex()
String getNetworkSSID(int index)
int getNetworkFailCount(int index)
bool switchToNextNetwork()
bool switchToNetwork(int index)
```

### Configuration du basculement automatique

```cpp
void setAutoSwitch(bool enable)
void setMaxRetries(int retries)
void setRetryDelay(unsigned long ms)
void setFailCooldown(unsigned long ms)
```

### Configuration (legacy mono-réseau)

```cpp
void begin(bool enableAP = false, uint32_t timeout = 10000)
void begin(const char* ssid, const char* password, bool enableAP = false, uint32_t timeout = 10000)
void setCredentials(const char* ssid, const char* password)
void setAPCredentials(const char* ap_ssid, const char* ap_password = "password123")
void setHostname(const char* hostname)
void setHostnamePrefix(const char* prefix)
```

### Informations réseau

```cpp
bool isConnected()
String getLocalIP()
String getGatewayIP()
String getDnsIP()
String getMacAddress()
String getSSID()
int8_t getRSSI()
String getHostname()
String getStatusText()
wl_status_t getStatus()
```

### Gestion de la connexion

```cpp
void reconnect()
void disconnect()
int updateStatus()
void printStatus(bool detailed = false)
void update()  // À appeler dans loop() pour le failover auto
```

### Historique des connexions (nouveau)

```cpp
int getHistoryCount()
const ConnectionHistoryEntry* getHistoryEntry(int index)
void printHistory()
void clearHistory()
```

## 🎯 Exemples

### Exemple multi-réseaux (nouveau)

```cpp
#include <WiFiManagerESP.h>

WiFiManagerESP wifi;

void setup() {
    Serial.begin(115200);

    // Ajouter plusieurs réseaux avec priorité
    wifi.addNetwork("Votre_SSID", "Votre_Mot_De_Passe", 0);
    wifi.addNetwork("Votre_SSID", "Votre_Mot_De_Passe", 1);
    wifi.addNetwork("Votre_SSID", "Votre_Mot_De_Passe", 2);
    wifi.addNetwork("Votre_SSID", "Votre_Mot_De_Passe", 3);

    wifi.setHostname("MonESP");
    wifi.setAutoSwitch(true);      // Basculement auto activé
    wifi.setMaxRetries(3);         // 3 échecs avant basculement
    wifi.setRetryDelay(10000);     // 10s entre tentatives
    wifi.setFailCooldown(60000);   // 60s de cooldown après échec

    wifi.begin(false, 15000);      // Démarrer sans AP, timeout 15s
}

void loop() {
    wifi.update();  // Gère le failover automatiquement

    // Afficher le statut toutes les 10s
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 10000) {
        lastPrint = millis();
        wifi.printStatus(true);
        wifi.printHistory();
    }

    delay(100);
}
```

### Exemple mono-réseau (legacy)

```cpp
#include <WiFiManagerESP.h>

WiFiManagerESP wifi;

void setup() {
    Serial.begin(115200);

    wifi.setHostname("mon-esp32");
    wifi.begin("mon_wifi", "mon_mdp", false, 10000);
}

void loop() {
    wifi.update();
    delay(100);
}
```

## 🛠️ Configuration

### Mode multi-réseaux (failover)

```cpp
wifi.addNetwork("Reseau_Principal", "mdp1", 0);   // Priorité 0 = tenté en premier
wifi.addNetwork("Reseau_Secours", "mdp2", 1);      // Priorité 1 = tenté si le 0 échoue
wifi.addNetwork("Reseau_Tertiaire", "mdp3", 2);    // Priorité 2 = dernier recours
wifi.begin(false, 15000);
```

### Mode point d'accès (AP)

Activez le mode AP en passant `true` à la méthode `begin()` :

```cpp
wifiManager.begin("mon_wifi", "mon_mdp", true);
```

Ou configurez les credentials AP séparément :

```cpp
wifiManager.setAPCredentials("Mon_AP_ESP", "motdepasse123");
wifiManager.begin(true);
```

### Hostname personnalisé

```cpp
wifiManager.setHostname("mon-esp32-personnalise");
```

Ou utilisez un préfixe avec ID unique :

```cpp
wifiManager.setHostnamePrefix("Capteur_");
// Résultat: Capteur_ABC123
```

## 🔧 Compatibilité

| Plateforme | Supporté | Notes |
|-----------|----------|-------|
| ESP32 | ✅ | Pleinement supporté |
| ESP8266 | ✅ | Pleinement supporté |
| Autres | ❌ | Non supporté |

## 📄 Licence

License: GPL v3 - voir [LICENSE](LICENSE)

## 👤 Auteur

Olivier FOURNET

Email: olivier.fournet@free.fr

GitHub: @Fo170

## 🤝 Contribution

Les contributions sont les bienvenues ! N'hésitez pas à ouvrir une issue ou une pull request.

## 📞 Support

- Vérifiez la documentation et les exemples
- Recherchez les issues existantes
- Ouvrez une nouvelle issue si nécessaire

## 📈 Versions

| Version | Date | Changements |
|---------|------|-------------|
| **v0.7.1** | 2026-05 | Ajout du support multi-réseaux avec basculement automatique, historique des connexions, priorisation des réseaux, cooldown après échec |
| v0.6.0 | - | Ajout de la documentation Doxygen, support ESP8266/ESP32, mode AP+STA simultané, gestion des événements WiFi, reconnexion automatique |
| v0.5.0 | - | Première version publique |
