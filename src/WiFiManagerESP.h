// ===========================================
// WiFiManagerESP.h - HEADER FILE
// v0.5.0
// ===========================================
#ifndef WIFIMANAGER_ESP_H
#define WIFIMANAGER_ESP_H

#include <Arduino.h>

// Définitions de macros pour les plateformes
#define WIFI_LIB WiFi // WiFi pour ESP8266 & ESP32

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

class WiFiManagerESP {
public:
  // Constructeur
  WiFiManagerESP();
  
  // Méthodes principales
  void begin(bool enableAP = false, uint32_t timeout = 10000);
  void begin(const char* ssid, const char* password, bool enableAP = false, uint32_t timeout = 10000);
  
  // Gestion des credentials
  void setCredentials(const char* ssid, const char* password);
  void setAPCredentials(const char* ap_ssid, const char* ap_password = "password123");
  
  // Configuration hostname
  void setHostname(const char* hostname);
  void setHostnamePrefix(const char* prefix);
  
  // États et informations
  bool isConnected();
  int updateStatus();
  void printStatus(bool detailed = false);
  
  // Gestion de la connexion
  void reconnect();
  void disconnect();
  
  // Getters réseau
  String getLocalIP();
  String getAPIP();
  String getSSID();
  int8_t getRSSI();
  String getStatusText();
  wl_status_t getStatus();
  
  // Nouveaux getters ajoutés
  String getGatewayIP();
  String getDnsIP();
  String getMacAddress();
  String getHostname();
  
private:
  // Variables membres
  const char* _ssid = nullptr;
  const char* _password = nullptr;
  const char* _ap_ssid = "ESP_WiFi_AP";
  const char* _ap_password = "password123";
  const char* _hostname = nullptr;
  const char* _hostname_prefix = "ESP_Device_";
  
  wl_status_t _currentStatus = WL_DISCONNECTED;
  String _currentStatusText = "";
  unsigned long _lastWifiEvent = 0;
  bool _wifiInitialized = false;
  bool _apEnabled = false;
  
  // Méthodes privées
  void _initWiFi(bool enableAP, uint32_t timeout);
  void _setupCallbacks();
  void _configureHostname();
  String _getStatusText(wl_status_t status);
  String _getModeText();
  String _macToString(const uint8_t* mac);
  
  // Callbacks (différents selon la plateforme)
#if defined(ESP8266)
  void _setupCallbacksESP8266();
#elif defined(ESP32)
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
