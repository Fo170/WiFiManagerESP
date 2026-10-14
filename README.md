# WiFiManagerESP

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Arduino Badge](https://img.shields.io/badge/framework-arduino-brightgreen?logo=arduino.svg)](https://www.arduino.cc/)
[![PlatformIO](https://img.shields.io/badge/platformio-ESP8266,ESP32-violet?logo=platformio)](https://platformio.org/)
[![Arduino Library Badge](https://www.ardu-badge.com/badge/WiFiManagerESP.svg)](https://github.com/Fo170?tab=repositories)

Bibliothèque Arduino/PlatformIO pour la gestion simplifiée des connexions WiFi sur ESP8266 et ESP32 : mode point d'accès (AP), **connexion non-bloquante (machine à états)**, **gestion multi-réseaux avec basculement automatique (failover)** et **classement dynamique des réseaux**, reconnexion automatique native du stack WiFi, historique des connexions et mDNS.

Version 0.10.0

## ✨ Fonctionnalités

- ✅ Support ESP8266 et ESP32
- ✅ **Connexion WiFi multi-réseaux avec basculement automatique (failover)**
- ✅ **Connexion non-bloquante (machine à états)** : `update()` pilote la tentative, le timeout et le failover sans jamais bloquer le `loop()`
- ✅ **Démarrage asynchrone** : `beginAsync()` + `waitForConnection()` pour lancer et attendre sans geler le code applicatif
- ✅ **Provisionnement à chaud** : `connectToNetwork()`, `removeNetwork()`, `updateNetwork()`
- ✅ **Scan WiFi** bloquant (`scanNetworks()`) ou asynchrone (`scanNetworksAsync()`) + cache de résultats
- ✅ **Diagnostics** : BSSID, canal, masque, qualité du signal, raison de déconnexion, clients AP
- ✅ **Bascule sur signal faible** (`setMinRSSI`, hystérésis) et timeout de connexion configurable
- ✅ **Classement dynamique des réseaux** (`setDynamicRanking()`) : score composite (RSSI, déconnexions à oubli exponentiel, stabilité, ancienneté), roaming proactif et scan RSSI périodique
- ✅ **IP statique** (`setStaticIP()` / `useDHCP()`) et puissance TX (`setTxPower()`)
- ✅ **Persistance EEPROM chiffrée AES-128** (`saveNetworks()` / `loadNetworks()`)
- ✅ **Historique des connexions avec timestamps et type de chiffrement**
- ✅ **Priorisation des réseaux (ordre de tentative configurable)**
- ✅ Reconnexion automatique intelligente (pas de conflit avec multi-réseaux)
- ✅ **Reconnexion automatique native du stack WiFi** (`setAutoReconnect`) pour absorber les coupures brèves
- ✅ Mode point d'accès (AP) simultané
- ✅ Gestion des événements WiFi
- ✅ Hostname personnalisable
- ✅ Affichage détaillé des informations réseau
- ✅ Callbacks pour les événements WiFi
- ✅ Timeout configurable
- ✅ Compteur d'échecs par réseau avec cooldown
- ✅ **mDNS (Multicast DNS) avec services annoncés**

## 📦 Installation

### PlatformIO

Ajoutez la dépendance dans votre fichier `platformio.ini` :

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    https://github.com/Fo170/WiFiManagerESP.git@^0.10.0
```

> **Dépendance requise** : la bibliothèque `NON_BLOCKING_DELAY` est utilisée pour les délais non bloquants.
> - **PlatformIO** : la dépendance `https://github.com/Fo170/NON_BLOCKING_DELAY.git@^1.0.0` est résolue automatiquement via `library.json`.
> - **Arduino IDE** : installez la bibliothèque manuellement (pas dans le registre Arduino) en la clonant :
>   `git clone https://github.com/Fo170/NON_BLOCKING_DELAY.git` dans votre dossier `libraries/`.

## 📚 API Principale

### Configuration multi-réseaux

```cpp
bool addNetwork(const char* ssid, const char* password, int priority = 0)
bool connectToNetwork(const char* ssid, const char* password = "", int priority = 0)  // Ajout + connexion non-bloquante
bool removeNetwork(int index)
bool updateNetwork(int index, const char* ssid, const char* password, int priority = 0)
bool hasNetwork(const char* ssid)
int indexOfNetwork(const char* ssid)
void clearNetworks()
int getNetworkCount()
int getCurrentNetworkIndex()
String getNetworkSSID(int index)
int getNetworkFailCount(int index)
bool switchToNextNetwork()
bool switchToNetwork(int index)
```

> **Provisionnement à chaud** : `connectToNetwork()` ajoute le réseau puis lance immédiatement
> sa connexion (non-bloquante, pilotée par `update()`). Utilisable au démarrage avec **zéro réseau
> configuré** — il initialise la pile WiFi si nécessaire. Échoue si la liste est pleine
> (`WIFIMANAGER_MAX_NETWORKS`). Voir `scanNetworks()` pour lister les réseaux disponibles.

### Configuration du basculement automatique

```cpp
void setAutoSwitch(bool enable)
void setAutoReconnect(bool enable)   // Reconnexion native du stack WiFi (défaut: true)
void setMaxRetries(int retries)
void setRetryDelay(unsigned long ms)
void setFailCooldown(unsigned long ms)
void setConnectTimeout(uint32_t ms)  // Timeout d'une tentative (défaut: 15000)
void setMinRSSI(int8_t dbm)          // Bascule si signal < seuil (0 = désactivé, hystérésis +5 dBm)
```

> **Reconnexion automatique native** : `setAutoReconnect(true)` (défaut) laisse le stack WiFi
> (ESP32/ESP8266) retenter lui-même le réseau courant lors des coupures brèves. La bibliothèque
> ne prend le relais (failover) que si la coupure persiste au-delà de ce que le stack peut résorber.
> Désactivable via `setAutoReconnect(false)` si vous préférez que le failover agisse dès la première
> détection de perte de lien.

### Classement dynamique des réseaux (v0.10.0)

```cpp
void setDynamicRanking(bool enable,
                       int8_t rssiMin = -85, int8_t rssiMax = -30,
                       uint32_t forgetTauS = 3600, uint32_t uptimeTauS = 3600,
                       float wSig = 0.50f, float wCon = 0.25f,
                       float wStab = 0.15f, float wUp = 0.10f,
                       float margin = 0.08f, uint32_t scanIntervalMs = 120000,
                       uint8_t stabSamples = 5, float sigma0 = 3.0f)
int getNetworkScore(int index)   // Score composite 0-100 % (‑1 si invalide)
void setScanGate(bool (*cb)())   // Autorisation externe du scan (retourne false pour reporter)
```

> **Score composite (0..1)** = `w_sig·c_sig + w_con·c_con + w_stab·c_stab + w_up·c_up` :
> `c_sig` = RSSI normalisé, `c_con` = `1/(1+N_eff)` (déconnexions à oubli exponentiel `forgetTauS`),
> `c_stab` = `1/(1+écart‑type(RSSI)/sigma0)` sur les `stabSamples` derniers scans,
> `c_up` = `0.5` si non connecté sinon `min(uptime/uptimeTauS, 1)`.
>
> La sélection retient le meilleur score ; la bascule n'a lieu que si
> `score(candidat) >= score(courant) + margin` (anti‑rebond). Un scan RSSI asynchrone est lancé
> toutes les `scanIntervalMs` ; le failover est inhibé pendant le scan (coupure radio ~2 s).
> `setScanGate()` permet de reporter le scan (ex. requête MCP en cours). Désactivé par défaut.

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
String scanNetworks()   // Scanne les réseaux (SSID, RSSI, chiffrement) — retourne la liste formatée
String Etat_Wifi()      // Résumé de l'état WiFi (dont le type de chiffrement) dans une String
String getBSSID()       // MAC de l'AP connecté
int32_t getChannel()    // Canal radio
String getSubnetMask()
int getSignalQuality()  // Qualité 0-100 % (depuis le RSSI)
int getLastDisconnectReason()
String getLastDisconnectReasonText()
int getAPClientCount()
```

### Scan asynchrone + résultats

```cpp
bool scanNetworksAsync()               // Scan non-bloquant (piloté par update())
bool isScanRunning()
int getScanResultCount()               // WIFIMANAGER_MAX_SCAN_RESULTS max (défaut: 12)
String getScanResultSSID(int index)
int8_t getScanResultRSSI(int index)
uint8_t getScanResultEncryption(int index)
int8_t getScanResultChannel(int index)
int findNetworkInScan(const char* ssid, int8_t* rssi = nullptr, uint8_t* encryption = nullptr)
```

### Adresse IP statique / radio

```cpp
void setStaticIP(IPAddress ip, IPAddress gateway, IPAddress subnet,
                 IPAddress dns1 = (uint32_t)0, IPAddress dns2 = (uint32_t)0)
void useDHCP()
bool setTxPower(float dBm)   // ESP8266 continu ; ESP32 palier le plus proche
```

### Persistance EEPROM (chiffrée AES-128)

```cpp
void setStorageKey(const char* key)   // Clé utilisateur (clé finale = SHA256(salt + chipId + key))
bool saveNetworks()                   // Sauvegarde les réseaux (SSID + mot de passe chiffrés)
bool loadNetworks()                   // Charge les réseaux depuis l'EEPROM
bool factoryReset()                   // Efface réseaux + EEPROM
```

> **Sécurité** : les SSID et mots de passe sont chiffrés en **AES-128-CBC**, la clé étant dérivée
> du chip et d'une clé utilisateur optionnelle (`setStorageKey`). Sans secure element, cela protège
> contre la lecture en clair de l'EEPROM (dump) mais pas contre un attaquant disposant du firmware
> et de la puce.

### Gestion de la connexion

```cpp
bool beginAsync(bool enableAP = false, uint32_t timeout = 10000)  // Non-bloquant : lance la tentative, update() la suit
bool waitForConnection(uint32_t timeoutMs)                        // Attend (sans bloquer le WiFi) la connexion
void reconnect()
void disconnect()
int updateStatus()
void printStatus(bool detailed = false)
void update()  // À appeler dans loop() pour le failover auto
```

> **Connexion non-bloquante (machine à états)** : `begin()` reste bloquant (compatibilité)
> mais s'appuie sur la même machine à états que `beginAsync()` — aucune tentative ne bloque plus
> `update()`. L'état est piloté par `update()` (à appeler dans `loop()`).
> `isConnected()` retourne `true` quand le lien WiFi est actif (pilotage LED, etc.).

> **Sémantique de retour (async)** : `switchToNetwork()`, `switchToNextNetwork()` et `reconnect()`
> retournent `true` si la **tentative a été lancée** (non-bloquant). Le résultat réel s'observe
> via `isConnected()` / `update()`.

### Hooks application + power-save (v0.7.9)

```cpp
// Appelé à CHAQUE connexion réussie — ré-arme un stack dépendant du WiFi (ESP-NOW) :
wifiManager.onWiFiConnected([]() {
  esv.rearm();        // ré-initialise ESP-NOW + callbacks (effacés par un reset WiFi)
  esv.rearmPeers();   // ré-enregistre les peers au driver
});
wifiManager.onWiFiDisconnected([]() {
  // ex. couper proprement les envois ESP-NOW
});

wifiManager.setPowerSaveOff(true);   // défaut : coupe le power-save après connexion (ESP-NOW)
```

> **Pourquoi couper le power-save ?** En mode `WIFI_PS_MIN_MODEM` (défaut ESP32), la radio dort
> périodiquement et **manque des trames ESP-NOW entrantes** (perte mesurée ~25-30 %). `setSleep(false)`
> / `WIFI_NONE_SLEEP` garde la radio éveillée.
>
> **Diagnostic des resets** : chaque `_resetWiFi()` (qui efface le stack ESP-NOW) est compté —
> `getResetCount()` / `getLastResetTime()` — et loggé (`🔄 reset WiFi #n`). Utile pour corréler
> pertes ESP-NOW ↔ flapping WiFi.

### mDNS (Multicast DNS)

```cpp
void setAutoMDNS(bool enable)           // Active/désactive mDNS auto (défaut: true)
bool startMDNS(const char* hostname)    // Démarre mDNS manuellement
void stopMDNS()                         // Arrête mDNS
bool isMDNSRunning()                    // Vérifie si mDNS est actif
String getMDNSHostname()                // Retourne le nom mDNS
bool addMDNSService(const char* name, const char* protocol, uint16_t port)
void clearMDNSServices()
int getMDNSServiceCount()
bool addMDNSTxtRecord(const char* service, const char* protocol, const char* key, const char* value)
```

### Historique des connexions

```cpp
int getHistoryCount()
const ConnectionHistoryEntry* getHistoryEntry(int index)
void printHistory()
void clearHistory()
```

## 🎯 Exemples

### Exemple multi-réseaux

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

    NON_BLOCKING_DELAY(100);
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
    NON_BLOCKING_DELAY(100);
}
```

### Exemple non-bloquant (async, recommandé)

```cpp
#include <WiFiManagerESP.h>

WiFiManagerESP wifi;

void setup() {
    Serial.begin(115200);

    wifi.setHostname("mon-esp");
    wifi.addNetwork("WiFi_Principal", "mdp1", 0);
    wifi.addNetwork("WiFi_Secours", "mdp2", 1);

    // Reconnexion native du stack WiFi (défaut activé) : absorbe les coupures brèves.
    // wifi.setAutoReconnect(false);  // à désactiver pour un failover immédiat

    // Non-bloquant : lance la tentative et rend la main immédiatement.
    // La connexion progresse dans update() (appelé dans loop()).
    wifi.beginAsync(false, 15000);
}

void loop() {
    wifi.update();   // Pilote la connexion + le failover, sans jamais bloquer

    // Pilotage simple d'un LED (allumé si connecté)
    digitalWrite(LED_BUILTIN, !wifi.isConnected());

    NON_BLOCKING_DELAY(100);
}
```

> **Attendre la connexion de façon bloquante (mais sans geler le WiFi)** :
> ```cpp
> if (wifi.waitForConnection(20000)) {
>     // Connecté
> } else {
>     // Timeout : le failover continue dans loop()
> }
> ```

### Exemple provisionnement à chaud + diagnostics + persistance (v0.9.0)

```cpp
#include <WiFiManagerESP.h>

WiFiManagerESP wifi;
bool scanDone = false;

void setup() {
    Serial.begin(115200);

    // Reprise des réseaux sauvegardés (chiffrés) au démarrage
    wifi.setStorageKey("ma_cle_secrete");
    if (wifi.loadNetworks()) {
        wifi.beginAsync(false, 15000);
    } else {
        // Aucun réseau connu : on scanne pour en ajouter un à chaud
        wifi.scanNetworksAsync();
    }

    // Seuils et diagnostics
    wifi.setMinRSSI(-80);          // bascule si signal trop faible (hystérésis +5 dBm)
    wifi.setConnectTimeout(12000);
}

void loop() {
    wifi.update();

    // Exploitation du scan asynchrone
    if (!scanDone && !wifi.isScanRunning()) {
        scanDone = true;
        for (int i = 0; i < wifi.getScanResultCount(); i++) {
            Serial.printf("%s (%d dBm) canal %d\n",
                wifi.getScanResultSSID(i).c_str(),
                wifi.getScanResultRSSI(i),
                wifi.getScanResultChannel(i));
        }
        // Ajouter + connecter un réseau trouvé, puis persister
        wifi.connectToNetwork("Mon_Reseau", "MotDePasse", 0);
        wifi.saveNetworks();
    }

    if (wifi.isConnected()) {
        Serial.printf("Qualité: %d%% | BSSID: %s | Canal: %d\n",
            wifi.getSignalQuality(), wifi.getBSSID().c_str(), wifi.getChannel());
    }

    NON_BLOCKING_DELAY(100);
}
```

## 🎯 Exemple mDNS

```cpp
#include <WiFiManagerESP.h>

WiFiManagerESP wifi;

void setup() {
    Serial.begin(115200);

    wifi.setHostname("mon-esp");
    wifi.addNetwork("WiFi_Principal", "mdp1", 0);
    wifi.addNetwork("WiFi_Secours", "mdp2", 1);

    // Services mDNS à annoncer
    wifi.addMDNSService("http", "tcp", 80);
    wifi.addMDNSService("telnet", "tcp", 23);

    wifi.begin(false, 15000);  // mDNS démarre auto après connexion
}

void loop() {
    wifi.update();  // Gère failover + MDNS.update() sur ESP8266
}
```

Accès via : `http://mon-esp.local`

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

### mDNS (Multicast DNS)

Le mDNS démarre automatiquement après connexion WiFi si `setAutoMDNS(true)` (défaut).

```cpp
wifi.setHostname("mon-esp");           // Définit aussi le nom mDNS
wifi.setAutoMDNS(true);                // Active le mDNS auto (défaut)
wifi.addMDNSService("http", "tcp", 80); // Annonce un service web
wifi.addMDNSService("ssh", "tcp", 22);  // Annonce un service SSH
wifi.begin(false, 15000);
```

- **ESP8266** : Nécessite `wifi.update()` dans `loop()` pour `MDNS.update()`
- **ESP32** : mDNS géré en tâche de fond, pas besoin d'`update()`
- Jusqu'à 5 services mDNS simultanés
- Enregistrements TXT supportés pour enrichir les services

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

| Version |   Date  | Changements |
|---------|---------|-------------|
| **v0.10.0** | 2026-10	| **Motif** : le failover par priorité statique ignore la qualité réelle du lien (un réseau prioritaire mais faible est conservé) — d'où un score composite et un roaming proactif *avant* dégradation. **Classement dynamique des réseaux** : `setDynamicRanking()` (score composite pondéré RSSI / déconnexions à oubli exponentiel / stabilité / ancienneté du lien), `getNetworkScore()` (0-100 %), `setScanGate()` (autorisation externe du scan). **Roaming proactif** : bascule vers un meilleur candidat si `score >= courant + marge` (anti-rebond), scan RSSI asynchrone périodique (`scanIntervalMs`), failover inhibé pendant le scan (coupure radio). Champs `WiFiNetwork` enrichis (`connEff`, `rssiHist`, `linked`, `connSince`…) et `_findBestNetwork()` refactorisé (mode dynamique + classique). |
| **v0.9.0** | 2026-09	| **Motif** : rendre les problèmes réseau diagnosticables, supprimer le blocage du `loop()` pendant le scan (~2 s), et permettre un provisionnement persistant sans identifiants en clair. **Diagnostics** : `getBSSID()`, `getChannel()`, `getSubnetMask()`, `getSignalQuality()`, `getLastDisconnectReason()`/`…Text()`, `getAPClientCount()`. **Robustesse** : `setConnectTimeout()`, `setMinRSSI()` (bascule sur signal faible avec hystérésis +5 dBm), `hasNetwork()`, `indexOfNetwork()`, `updateNetwork()`, `removeMDNSService()`. **Scan non bloquant** : `scanNetworksAsync()` + cache de résultats (12 max) et accesseurs `getScanResult*()` / `findNetworkInScan()`. **IP statique** : `setStaticIP()` / `useDHCP()`. **Radio** : `setTxPower()`. **Persistance EEPROM chiffrée** : AES-128-CBC + SHA-256 embarqués (sans dépendance), clé dérivée du chip + clé utilisateur (`setStorageKey()`), `saveNetworks()` / `loadNetworks()` / `factoryReset()`. |
| **v0.8.0** | 2026-09	| **Optimisation mémoire** : réduction des constantes par défaut pour limiter l'empreinte RAM — `WIFIMANAGER_MAX_HISTORY` 20→8, `WIFIMANAGER_MAX_NETWORKS` 10→3, `WIFIMANAGER_MAX_MDNS_SERVICES` 5→2. **Chiffrement WiFi** : nouvelle méthode `scanNetworks()` (scan bloquant + affichage SSID/RSSI/chiffrement + retour String) ; affichage du type de chiffrement dans `printStatus()`, `Etat_Wifi()` et `printHistory()` (nouvelle colonne) ; mise en cache du chiffrement du réseau connecté (ESP32 via `esp_wifi_sta_get_ap_info()`, ESP8266 renseigné par `scanNetworks()`) ; helper `_getEncryptionText()` multi-plateforme. **Provisionnement à chaud** : `connectToNetwork()` (ajout + connexion ciblée non-bloquante, utilisable sans réseau configuré), `removeNetwork(index)` ; les SSID/mots de passe sont désormais **copiés** dans la classe (ajout dynamique sûr). |
| **v0.7.9** | 2026-08	| **Motif** : un reset WiFi efface le stack ESP-NOW et le modem-sleep fait manquer des trames ESP-NOW entrantes — d'où les hooks de ré-armement et la coupure du power-save. **Intégration ESP-NOW** : hooks applicatifs `onWiFiConnected()` / `onWiFiDisconnected()` (appelés à chaque connexion réussie / échec — pour ré-armer un stack dépendant du WiFi, ex. `esv.rearm()` + `esv.rearmPeers()`) ; **coupe le power-save WiFi après connexion** (`setPowerSaveOff(true)` par défaut → `WiFi.setSleep(false)` ESP32 / `WIFI_NONE_SLEEP` ESP8266) car le modem sleep fait manquer des trames ESP-NOW entrantes (~25-30 % de perte) ; compteur de réinitialisations WiFi `getResetCount()` / `getLastResetTime()` + log `🔄 reset WiFi #n` dans `_resetWiFi()` (chaque reset efface le stack ESP-NOW). |
| **v0.7.8** | 2026-08	| Remplacement des `delay()` bloquants par la librairie non-bloquante `NON_BLOCKING_DELAY` (vTaskDelay sur ESP32, busy-wait + yield sur ESP8266) ; dépendance déclarée dans `library.json` ; bump des exemples. **Connexion refactorisée en machine à états non-bloquante** : `begin()` conservé bloquant (compatibilité), ajout de `beginAsync()` et `waitForConnection()`, `update()` pilote la tentative et le failover sans jamais bloquer (fin du freeze de 10-15 s), `switchToNetwork()`/`switchToNextNetwork()`/`reconnect()` retournent désormais `true` si la tentative a été lancée. **Corrections de bugs** : reconnexion automatique rétablie après perte de lien (l'état restait bloqué sur `CONNECTED` et le failover ne se déclenchait plus) ; failover immédiat sur `WL_CONNECT_FAILED` (ex. mot de passe erroné) sans attendre le timeout complet. Ajout de `setAutoReconnect()` (défaut activé) : le stack WiFi retente lui-même le réseau courant sur coupures brèves, la bibliothèque ne fail-over que si la coupure persiste. |
| **v0.7.7** | 2026-06	| lastTriedNetwork est déclarée en static localement dans update(), mais elle n'est pas un membre de la classe. Si on veut la rendre réellement utilisable et persistante entre les appels, il faut la déclarer comme variable membre privée en _lastTriedNetwork. |
| v0.7.6 | 2026-06	| la variable lastTriedNetwork est déclarée en static mais jamais utilisée. Correction pour qu'elle serve réellement à éviter de réessayer immédiatement le même réseau qui vient d'échouer  |
| v0.7.5 | 2026-06	| Refonte du basculement automatique : parcours circulaire forcé de tous les réseaux configurés pour garantir que chaque réseau est testé ; _connectToNetwork() ne réinitialise plus le failCount au début de la tentative (uniquement sur connexion réussie) ; _findBestNetwork() simplifié avec exploration systématique ; correction du hostname ESP8266 appliqué avant WiFi.mode(WIFI_STA) ; résolution du bug des pointeurs const char* vers tableaux externes |
| v0.7.4 | 2026-05	| Corrections critiques du basculement multi-réseaux : _findBestNetwork() retourne désormais le réseau avec la meilleure priorité (et non plus le premier trouvé) ; ajout d'un fallback quand tous les réseaux sont en cooldown ; _connectToNetwork() réinitialise le failCount au début de chaque tentative pour éviter les boucles infinies ; update() gère correctement le cas où _findBestNetwork() retourne -1 |
| v0.7.2 - v0.7.3 | 2026-05 | Ajout du support mDNS (Multicast DNS) avec services annoncés, enregistrements TXT, démarrage automatique après connexion |
| v0.7.1 | 2026-05 | Ajout du support multi-réseaux avec basculement automatique, historique des connexions, priorisation des réseaux, cooldown après échec |
| v0.6.0 | - | Ajout de la documentation Doxygen, support ESP8266/ESP32, mode AP+STA simultané, gestion des événements WiFi, reconnexion automatique |
| v0.5.0 | - | Première version publique |
