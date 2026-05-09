// ===========================================
// WiFiManagerESP.h - HEADER FILE
// v0.6.0
// ===========================================

/**
 * @file WiFiManagerESP.h
 * @brief Bibliothèque de gestion WiFi unifiée pour ESP8266 et ESP32
 * @author Fo170
 * @version 0.6.0
 * 
 * Cette bibliothèque fournit une interface simplifiée pour gérer les connexions
 * WiFi sur les plateformes ESP8266 et ESP32, avec support du mode AP+STA,
 * configuration automatique du hostname et gestion des événements réseau.
 */

#ifndef WIFIMANAGER_ESP_H
#define WIFIMANAGER_ESP_H

#include <Arduino.h>

// ===========================================
// DÉFINITIONS DE MACROS POUR LES PLATEFORMES
// ===========================================

/** @brief Macro d'accès à la bibliothèque WiFi native */
#define WIFI_LIB WiFi

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #define WIFI_MODE_APSTA WIFI_AP_STA
  #define WIFI_EVENT_STA_GOT_IP WiFiEventStationModeGotIP
  #define WIFI_EVENT_STA_DISCONNECTED WiFiEventStationModeDisconnected
  #define WIFI_EVENT_AP_STA_CONNECTED WiFiEventSoftAPModeStationConnected
  #define WIFI_EVENT_AP_STA_DISCONNECTED WiFiEventSoftAPModeStationDisconnected
#endif

#if defined(ESP32)
  #include <WiFi.h>
  #define WIFI_MODE_APSTA WIFI_MODE_APSTA
#endif

#if   !defined(ESP8266) && !defined(ESP32)
  #error "Plateforme non supportée ! Seuls ESP8266 et ESP32 sont supportés."
#endif

// ===========================================
// CLASSE PRINCIPALE
// ===========================================

/**
 * @class WiFiManagerESP
 * @brief Gestionnaire WiFi unifié pour ESP8266/ESP32
 * 
 * Cette classe encapsule toute la logique de gestion WiFi :
 * - Connexion/déconnexion au réseau
 * - Création d'un point d'accès (AP)
 * - Configuration du hostname
 * - Surveillance de l'état de la connexion
 * - Gestion automatique des événements réseau (reconnexion, etc.)
 */
class WiFiManagerESP {
public:
    // ===========================================
    // CONSTRUCTEUR
    // ===========================================

    /** @brief Constructeur par défaut */
    WiFiManagerESP();

    // ===========================================
    // MÉTHODES PRINCIPALES
    // ===========================================

    /**
     * @brief Démarre la connexion WiFi avec les credentials préconfigurés
     * @param enableAP Active le mode Point d'Accès simultané (AP+STA)
     * @param timeout Temps d'attente maximum pour la connexion (ms)
     * 
     * @note Nécessite d'avoir appelé setCredentials() au préalable
     * @warning Si aucun SSID n'est configuré, affiche un message d'erreur sur Serial
     */
    void begin(bool enableAP = false, uint32_t timeout = 10000);

    /**
     * @brief Démarre la connexion WiFi avec des credentials spécifiques
     * @param ssid Nom du réseau WiFi
     * @param password Mot de passe du réseau WiFi
     * @param enableAP Active le mode Point d'Accès simultané (AP+STA)
     * @param timeout Temps d'attente maximum pour la connexion (ms)
     * 
     * @note Cette méthode enregistre automatiquement les credentials via setCredentials()
     */
    void begin(const char* ssid, const char* password, bool enableAP = false, uint32_t timeout = 10000);

    // ===========================================
    // GESTION DES CREDENTIALS
    // ===========================================

    /**
     * @brief Configure les credentials du réseau WiFi à rejoindre
     * @param ssid Nom du réseau WiFi
     * @param password Mot de passe du réseau WiFi
     * 
     * @note Les pointeurs sont stockés directement, assurez-vous que les chaînes
     *       restent valides pendant toute la durée de vie de l'objet
     */
    void setCredentials(const char* ssid, const char* password);

    /**
     * @brief Configure les credentials du point d'accès (AP)
     * @param ap_ssid Nom du réseau AP à créer
     * @param ap_password Mot de passe du réseau AP (min. 8 caractères, défaut: "password123")
     * 
     * @warning Un mot de passe de moins de 8 caractères entraînera la création d'un AP ouvert
     */
    void setAPCredentials(const char* ap_ssid, const char* ap_password = "password123");

    // ===========================================
    // CONFIGURATION HOSTNAME
    // ===========================================

    /**
     * @brief Définit un hostname fixe pour l'appareil
     * @param hostname Nom d'hôte souhaité
     * 
     * @note Si appelé après begin(), le hostname est appliqué immédiatement
     * @note Prend priorité sur setHostnamePrefix()
     */
    void setHostname(const char* hostname);

    /**
     * @brief Définit un préfixe pour le hostname automatique
     * @param prefix Préfixe du hostname (ex: "ESP_Device_")
     * 
     * @brief Le hostname final sera : prefix + ID unique de la puce
     * @note Utilisé uniquement si setHostname() n'a pas été appelé
     */
    void setHostnamePrefix(const char* prefix);

    // ===========================================
    // ÉTATS ET INFORMATIONS
    // ===========================================

    /** @brief Vérifie si l'appareil est connecté au réseau WiFi */
    bool isConnected();

    /**
     * @brief Met à jour et retourne l'état actuel de la connexion WiFi
     * @return Code d'état WiFi (voir wl_status_t)
     * 
     * @note Détecte automatiquement les changements d'état et les affiche sur Serial
     */
    int updateStatus();

    /**
     * @brief Affiche l'état complet de la connexion WiFi sur Serial
     * @param detailed Si true, affiche toutes les informations réseau (IP, MAC, RSSI, etc.)
     */
    void printStatus(bool detailed = false);

    // ===========================================
    // GESTION DE LA CONNEXION
    // ===========================================

    /** @brief Force une reconnexion au réseau WiFi configuré */
    void reconnect();

    /** @brief Déconnecte l'appareil du réseau WiFi */
    void disconnect();

    // ===========================================
    // GETTERS RÉSEAU
    // ===========================================

    /** @return Adresse IP locale en mode client (STA) */
    String getLocalIP();

    /** @return Adresse IP du point d'accès (AP) */
    String getAPIP();

    /** @return SSID du réseau auquel l'appareil est connecté */
    String getSSID();

    /** @return Force du signal WiFi en dBm (négatif, plus proche de 0 = meilleur) */
    int8_t getRSSI();

    /** @return Description textuelle de l'état WiFi actuel */
    String getStatusText();

    /** @return État brut de la connexion WiFi (wl_status_t) */
    wl_status_t getStatus();

    /** @return Adresse IP de la passerelle (gateway) */
    String getGatewayIP();

    /** @return Adresse IP du serveur DNS */
    String getDnsIP();

    /** @return Adresse MAC de l'interface WiFi au format XX:XX:XX:XX:XX:XX */
    String getMacAddress();

    /** @return Hostname actuellement configuré */
    String getHostname();

private:
    // ===========================================
    // VARIABLES MEMBRES
    // ===========================================
    
    const char* _ssid = nullptr;           ///< SSID du réseau WiFi cible
    const char* _password = nullptr;       ///< Mot de passe du réseau WiFi cible
    const char* _ap_ssid = "ESP_WiFi_AP";  ///< SSID du point d'accès créé
    const char* _ap_password = "password123"; ///< Mot de passe du point d'accès
    const char* _hostname = nullptr;       ///< Hostname fixe (prioritaire)
    const char* _hostname_prefix = "ESP_Device_"; ///< Préfixe pour hostname auto

    wl_status_t _currentStatus = WL_DISCONNECTED; ///< État WiFi en cache
    String _currentStatusText = "";       ///< Description textuelle de l'état en cache
    unsigned long _lastWifiEvent = 0;       ///< Timestamp du dernier changement d'état
    bool _wifiInitialized = false;          ///< Indique si le WiFi a été initialisé
    bool _apEnabled = false;                ///< Indique si le mode AP est actif

    // ===========================================
    // MÉTHODES PRIVÉES
    // ===========================================

    /**
     * @brief Initialise le WiFi avec les paramètres configurés
     * @param enableAP Active le mode AP+STA
     * @param timeout Délai maximum d'attente de connexion
     * 
     * @note Configure les callbacks, le hostname, le mode WiFi et tente la connexion
     */
    void _initWiFi(bool enableAP, uint32_t timeout);

    /** @brief Configure les callbacks d'événements WiFi selon la plateforme */
    void _setupCallbacks();

    /** @brief Applique la configuration du hostname à la bibliothèque WiFi native */
    void _configureHostname();

    /**
     * @brief Convertit un code d'état WiFi en texte lisible
     * @param status Code d'état wl_status_t
     * @return Chaîne descriptive en français
     */
    String _getStatusText(wl_status_t status);

    /**
     * @brief Retourne le mode WiFi actuel en texte lisible
     * @return Description du mode (Client, AP, Client+AP, etc.)
     */
    String _getModeText();

    /**
     * @brief Convertit une adresse MAC en chaîne formatée
     * @param mac Tableau de 6 octets représentant l'adresse MAC
     * @return Adresse MAC au format XX:XX:XX:XX:XX:XX
     */
    String _macToString(const uint8_t* mac);

    // ===========================================
    // CALLBACKS (spécifiques par plateforme)
    // ===========================================

#if defined(ESP8266)
    /** @brief Configure les callbacks WiFi spécifiques à l'ESP8266 */
    void _setupCallbacksESP8266();
#elif defined(ESP32)
    /**
     * @brief Gestionnaire d'événements WiFi pour ESP32
     * @param event Type d'événement WiFi
     * @param info Informations supplémentaires sur l'événement
     * 
     * @note Gère : connexion, déconnexion, attribution IP, clients AP
     */
    void _WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
#endif
};

// ===========================================

WiFiManagerESP::WiFiManagerESP() {
  // Constructeur par défaut
}

void WiFiManagerESP::begin(bool enableAP, uint32_t timeout) {
  if (_ssid == nullptr) {
    Serial.println("[WiFiManagerESP] Aucun SSID configuré. Utilisez setCredentials() d'abord.");
    return;
  }
  begin(_ssid, _password, enableAP, timeout);
}

void WiFiManagerESP::begin(const char* ssid, const char* password, bool enableAP, uint32_t timeout) {
  setCredentials(ssid, password);
  _initWiFi(enableAP, timeout);
}

void WiFiManagerESP::setCredentials(const char* ssid, const char* password) {
  _ssid = ssid;
  _password = password;
}

void WiFiManagerESP::setAPCredentials(const char* ap_ssid, const char* ap_password) {
  _ap_ssid = ap_ssid;
  _ap_password = ap_password;
}

void WiFiManagerESP::setHostname(const char* hostname) {
  _hostname = hostname;
  if (_wifiInitialized) {
    _configureHostname();
  }
}

void WiFiManagerESP::setHostnamePrefix(const char* prefix) {
  _hostname_prefix = prefix;
  if (_wifiInitialized) {
    _configureHostname();
  }
}

void WiFiManagerESP::_configureHostname() {
  if (_hostname != nullptr) {
    #if defined(ESP8266)
      WIFI_LIB.hostname(_hostname);
    #elif defined(ESP32)
      WIFI_LIB.setHostname(_hostname);
    #endif
    Serial.print("[WiFiManagerESP] Hostname configuré: ");
    Serial.println(_hostname);
  } else if (_hostname_prefix != nullptr) {
    String hostnameStr;
    #if defined(ESP8266)
      hostnameStr = String(_hostname_prefix) + String(ESP.getChipId());
    #elif defined(ESP32)
      hostnameStr = String(_hostname_prefix) + String((uint32_t)(ESP.getEfuseMac() >> 32));
    #endif
    
    #if defined(ESP8266)
      WIFI_LIB.hostname(hostnameStr.c_str());
    #elif defined(ESP32)
      WIFI_LIB.setHostname(hostnameStr.c_str());
    #endif
    
    Serial.print("[WiFiManagerESP] Hostname configuré: ");
    Serial.println(hostnameStr);
  }
}

String WiFiManagerESP::getGatewayIP() {
  return WIFI_LIB.gatewayIP().toString();
}

String WiFiManagerESP::getDnsIP() {
  return WIFI_LIB.dnsIP().toString();
}

String WiFiManagerESP::getMacAddress() {
  uint8_t mac[6];
  #if defined(ESP8266)
    WIFI_LIB.macAddress(mac);
  #elif defined(ESP32)
    WIFI_LIB.macAddress(mac);
  #endif
  return _macToString(mac);
}

String WiFiManagerESP::getHostname() {
  #if defined(ESP8266)
    return WIFI_LIB.hostname();
  #elif defined(ESP32)
    return WIFI_LIB.getHostname();
  #endif
}

void WiFiManagerESP::_initWiFi(bool enableAP, uint32_t timeout) {
  if (_wifiInitialized) {
    Serial.println("[WiFiManagerESP] WiFi déjà initialisé");
    return;
  }
  
  Serial.println("\n=== INITIALISATION WiFi ===");
  
  _setupCallbacks();
  _apEnabled = enableAP;
  
  // Configurer le hostname avant la connexion
  _configureHostname();
  
  // Déterminer le mode WiFi
  if (enableAP) {
    #if defined(ESP8266)
      WIFI_LIB.mode(WIFI_AP_STA);
    #elif defined(ESP32)
      WIFI_LIB.mode(WIFI_MODE_APSTA);
    #endif
    
    Serial.print("Création du point d'accès: ");
    Serial.println(_ap_ssid);
    
    if (strlen(_ap_password) >= 8) {
      WIFI_LIB.softAP(_ap_ssid, _ap_password);
    } else {
      WIFI_LIB.softAP(_ap_ssid); // AP ouvert
    }
    
    Serial.print("AP IP: ");
    Serial.println(getAPIP());
  } else {
    #if defined(ESP8266)
      WIFI_LIB.mode(WIFI_STA);
    #elif defined(ESP32)
      WIFI_LIB.mode(WIFI_MODE_STA);
    #endif
  }
  
  // Se connecter en tant que client
  if (strlen(_ssid) > 0) {
    Serial.print("Connexion au réseau: ");
    Serial.println(_ssid);
    
    WIFI_LIB.begin(_ssid, _password);
    
    // Attendre la connexion avec timeout
    unsigned long startTime = millis();
    while (WIFI_LIB.status() != WL_CONNECTED && millis() - startTime < timeout) {
      delay(250);
      Serial.print(".");
      updateStatus();
    }
    
    Serial.println();
    
    if (WIFI_LIB.status() == WL_CONNECTED) {
      Serial.print("Connecté! IP: ");
      Serial.println(getLocalIP());
      Serial.print("Gateway: ");
      Serial.println(getGatewayIP());
      Serial.print("DNS: ");
      Serial.println(getDnsIP());
      Serial.print("MAC: ");
      Serial.println(getMacAddress());
    } else {
      Serial.println("Échec de connexion au réseau");
    }
  }
  
  _wifiInitialized = true;
  updateStatus();
  Serial.println("=== WiFi INITIALISÉ ===\n");
}

int WiFiManagerESP::updateStatus() {
  static wl_status_t lastStatus = WL_DISCONNECTED;
  
  wl_status_t newStatus = WIFI_LIB.status();
  
  if (newStatus != lastStatus) {
    _currentStatus = newStatus;
    _lastWifiEvent = millis();
    lastStatus = newStatus;
    _currentStatusText = _getStatusText(_currentStatus);
    
    Serial.print("[WiFi] Statut changé: ");
    Serial.print((int)_currentStatus);
    Serial.print(" - ");
    Serial.println(_currentStatusText);
  }
  
  return (int)_currentStatus;
}

bool WiFiManagerESP::isConnected() {
  updateStatus();
  return (_currentStatus == WL_CONNECTED);
}

wl_status_t WiFiManagerESP::getStatus() {
  updateStatus();
  return _currentStatus;
}

String WiFiManagerESP::getStatusText() {
  updateStatus();
  return _currentStatusText;
}

String WiFiManagerESP::getLocalIP() {
  return WIFI_LIB.localIP().toString();
}

String WiFiManagerESP::getAPIP() {
  return WIFI_LIB.softAPIP().toString();
}

String WiFiManagerESP::getSSID() {
  return WIFI_LIB.SSID();
}

int8_t WiFiManagerESP::getRSSI() {
  return WIFI_LIB.RSSI();
}

void WiFiManagerESP::printStatus(bool detailed) {
  updateStatus();
  
  Serial.println("\n=== ÉTAT WiFi ACTUEL ===");
  Serial.print("Statut: ");
  Serial.print((int)_currentStatus);
  Serial.print(" - ");
  Serial.println(_currentStatusText);
  
  if (detailed) {
    Serial.print("Mode WiFi: ");
    Serial.println(_getModeText());
    
    // Informations STA
    Serial.println("\n--- MODE CLIENT (STA) ---");
    Serial.print("Connexion: ");
    Serial.println(_currentStatus == WL_CONNECTED ? "OUI" : "NON");
    
    if (_currentStatus == WL_CONNECTED) {
      Serial.print("IP: ");
      Serial.println(getLocalIP());
      Serial.print("Gateway: ");
      Serial.println(getGatewayIP());
      Serial.print("DNS: ");
      Serial.println(getDnsIP());
      Serial.print("MAC: ");
      Serial.println(getMacAddress());
      Serial.print("Hostname: ");
      Serial.println(getHostname());
      Serial.print("RSSI: ");
      Serial.print(getRSSI());
      Serial.println(" dBm");
      Serial.print("SSID: ");
      Serial.println(getSSID());
    }
    
    // Informations AP
    if (_apEnabled) {
      Serial.println("\n--- MODE POINT D'ACCÈS (AP) ---");
      Serial.print("SSID AP: ");
      Serial.println(_ap_ssid);
      Serial.print("IP AP: ");
      Serial.println(getAPIP());
      #if defined(ESP8266)
        Serial.print("Clients connectés: ");
        Serial.println(WIFI_LIB.softAPgetStationNum());
      #elif defined(ESP32)
        Serial.print("Clients connectés: ");
        Serial.println(WIFI_LIB.softAPgetStationNum());
      #endif
    }
    
    // Informations générales
    Serial.println("\n--- INFORMATIONS GÉNÉRALES ---");
    Serial.print("Dernier événement: ");
    Serial.print((millis() - _lastWifiEvent) / 1000);
    Serial.println(" secondes");
    Serial.print("Initialisé: ");
    Serial.println(_wifiInitialized ? "OUI" : "NON");
  }
  
  Serial.println("=========================\n");
}

void WiFiManagerESP::reconnect() {
  Serial.println("[WiFiManagerESP] Tentative de reconnexion...");
  WIFI_LIB.disconnect();
  delay(100);
  WIFI_LIB.begin(_ssid, _password);
  updateStatus();
}

void WiFiManagerESP::disconnect() {
  WIFI_LIB.disconnect();
  updateStatus();
}

// ===========================================
// MÉTHODES PRIVÉES
// ===========================================

String WiFiManagerESP::_getStatusText(wl_status_t status) {
  switch(status) {
    case WL_IDLE_STATUS: return "En attente";
    case WL_NO_SSID_AVAIL: return "SSID non disponible";
    case WL_SCAN_COMPLETED: return "Scan terminé";
    case WL_CONNECTED: return "Connecté";
    case WL_CONNECT_FAILED: return "Échec connexion";
    case WL_CONNECTION_LOST: return "Connexion perdue";
    case WL_DISCONNECTED: return "Déconnecté";
    default: return "Inconnu";
  }
}

String WiFiManagerESP::_getModeText() {
  #if defined(ESP8266)
    WiFiMode_t mode = WIFI_LIB.getMode();
    switch(mode) {
      case WIFI_OFF: return "Éteint";
      case WIFI_STA: return "Client";
      case WIFI_AP: return "Point d'accès";
      case WIFI_AP_STA: return "Client + Point d'accès";
      default: return "Inconnu";
    }
  #elif defined(ESP32)
    wifi_mode_t mode = WIFI_LIB.getMode();
    switch(mode) {
      case WIFI_MODE_NULL: return "Éteint";
      case WIFI_MODE_STA: return "Client";
      case WIFI_MODE_AP: return "Point d'accès";
      case WIFI_MODE_APSTA: return "Client + Point d'accès";
      default: return "Inconnu";
    }
  #endif
  return "Inconnu";
}

String WiFiManagerESP::_macToString(const uint8_t* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void WiFiManagerESP::_setupCallbacks() {
  #if defined(ESP8266)
    _setupCallbacksESP8266();
  #elif defined(ESP32)
    WIFI_LIB.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
      this->_WiFiEvent(event, info);
    });
  #endif
}

// ===========================================
// IMPLÉMENTATIONS SPÉCIFIQUES PAR PLATEFORME
// ===========================================

#if defined(ESP8266)
void WiFiManagerESP::_setupCallbacksESP8266() {
  // Callback pour IP attribuée
  WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP& evt) {
    Serial.print("[WiFi] IP attribuée: ");
    Serial.println(evt.ip);
    this->updateStatus();
  });
  
  // Callback pour déconnexion
  WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected& evt) {
    Serial.print("[WiFi] Déconnecté! Raison: ");
    Serial.println(evt.reason);
    
    if (strlen(this->_ssid) > 0) {
      Serial.println("[WiFi] Tentative de reconnexion...");
      WiFi.begin(this->_ssid, this->_password);
    }
    
    this->updateStatus();
  });
  
  // Callbacks AP
  WiFi.onSoftAPModeStationConnected([this](const WiFiEventSoftAPModeStationConnected& evt) {
    Serial.print("[AP] Nouveau client: ");
    Serial.println(this->_macToString(evt.mac));
    this->updateStatus();
  });
  
  WiFi.onSoftAPModeStationDisconnected([this](const WiFiEventSoftAPModeStationDisconnected& evt) {
    Serial.print("[AP] Client déconnecté: ");
    Serial.println(this->_macToString(evt.mac));
    this->updateStatus();
  });
}
#endif

#if defined(ESP32)
void WiFiManagerESP::_WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WiFi] IP attribuée: ");
      Serial.println(WiFi.localIP());
      updateStatus();
      break;
      
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.print("[WiFi] Déconnecté! Raison: ");
      Serial.println(info.wifi_sta_disconnected.reason);
      
      if (strlen(_ssid) > 0) {
        Serial.println("[WiFi] Tentative de reconnexion...");
        WiFi.begin(_ssid, _password);
      }
      
      updateStatus();
      break;
      
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.print("[AP] Nouveau client. MAC: ");
      Serial.println(_macToString(info.wifi_ap_staconnected.mac));
      updateStatus();
      break;
      
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.print("[AP] Client déconnecté. MAC: ");
      Serial.println(_macToString(info.wifi_ap_stadisconnected.mac));
      updateStatus();
      break;
      
    default:
      break;
  }
}
#endif

#endif // WIFIMANAGER_ESP_H
