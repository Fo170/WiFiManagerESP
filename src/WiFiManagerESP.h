// ===========================================
// WiFiManagerESP.h - HEADER FILE
// v0.7.8 - Multi-Réseaux + mDNS
// ===========================================

/**
 * @file WiFiManagerESP.h
 * @brief Bibliothèque de gestion WiFi unifiée pour ESP8266 et ESP32
 *        avec support multi-réseaux et basculement automatique
 * @author Fo170
 * @version 0.7.8
 * 
 * Cette bibliothèque fournit une interface simplifiée pour gérer les connexions
 * WiFi sur les plateformes ESP8266 et ESP32, avec :
 * - Support multi-réseaux avec basculement automatique (failover)
 * - Mode point d'accès (AP) simultané
 * - Configuration automatique du hostname
 * - Gestion des événements réseau
 * - Historique des connexions
 * - mDNS (Multicast DNS) avec démarrage automatique
 * - Gestion des services mDNS
 */

#ifndef WIFIMANAGER_ESP_H
#define WIFIMANAGER_ESP_H

#include <Arduino.h>
#include "Delay.h"

// ===========================================
// DÉFINITIONS DE MACROS POUR LES PLATEFORMES
// ===========================================

/** @brief Macro d'accès à la bibliothèque WiFi native */
#define WIFI_LIB WiFi

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
  #define WIFI_MODE_APSTA WIFI_AP_STA
  #define MDNS_UPDATE_REQUIRED
  #define WIFI_EVENT_STA_GOT_IP WiFiEventStationModeGotIP
  #define WIFI_EVENT_STA_DISCONNECTED WiFiEventStationModeDisconnected
  #define WIFI_EVENT_AP_STA_CONNECTED WiFiEventSoftAPModeStationConnected
  #define WIFI_EVENT_AP_STA_DISCONNECTED WiFiEventSoftAPModeStationDisconnected
#endif

#if defined(ESP32)
  #include <WiFi.h>
  #include <ESPmDNS.h>
  #define WIFI_MODE_APSTA WIFI_MODE_APSTA
#endif

#if !defined(ESP8266) && !defined(ESP32)
  #error "Plateforme non supportée ! Seuls ESP8266 et ESP32 sont supportés."
#endif

// ===========================================
// CONSTANTES
// ===========================================

/** @brief Nombre maximum de réseaux WiFi configurables */
#define WIFIMANAGER_MAX_NETWORKS 10

/** @brief Nombre maximum d'entrées dans l'historique des connexions */
#define WIFIMANAGER_MAX_HISTORY 20

/** @brief Nombre maximum de services mDNS annoncés */
#define WIFIMANAGER_MAX_MDNS_SERVICES 5

// ===========================================
// STRUCTURES DE DONNÉES
// ===========================================

/**
 * @brief Structure représentant un réseau WiFi configuré
 */
struct WiFiNetwork {
    const char* ssid;           ///< Nom du réseau
    const char* password;       ///< Mot de passe
    int priority;               ///< Priorité (0 = plus haute)
    unsigned int failCount;     ///< Nombre d'échecs consécutifs
    unsigned long lastFail;     ///< Timestamp du dernier échec (ms)
    unsigned long lastAttempt;  ///< Timestamp de la dernière tentative (ms)
    bool configured;            ///< true si ce slot est utilisé
};

/**
 * @brief Structure représentant une entrée dans l'historique des connexions
 */
struct ConnectionHistoryEntry {
    char timestamp[20];         ///< Heure de l'événement
    char ssid[33];              ///< Nom du réseau
    char status[32];            ///< Statut (Connecté, Échec, Déconnecté...)
    char ip[16];                ///< Adresse IP
    int rssi;                   ///< Force du signal en dBm
    bool used;                  ///< true si cette entrée contient des données
};
/**
 * @brief Structure représentant un service mDNS annoncé
 */
struct MDNSService {
    char name[32];              ///< Nom du service (ex: "http")
    char protocol[8];           ///< Protocole (ex: "tcp")
    uint16_t port;              ///< Port du service
    bool configured;            ///< true si ce slot est utilisé
};

// ===========================================
// CLASSE PRINCIPALE
// ===========================================

/**
 * @class WiFiManagerESP
 * @brief Gestionnaire WiFi unifié pour ESP8266/ESP32 avec multi-réseaux
 * 
 * Cette classe encapsule toute la logique de gestion WiFi :
 * - Connexion/déconnexion au réseau
 * - Multi-réseaux avec basculement automatique (failover)
 * - Création d'un point d'accès (AP)
 * - Configuration du hostname
 * - Surveillance de l'état de la connexion
 * - Gestion automatique des événements réseau
 * - Historique des connexions
 * - mDNS (Multicast DNS) avec démarrage automatique
 * - Gestion des services mDNS
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
     * @brief Démarre la connexion WiFi avec les réseaux configurés
     * @param enableAP Active le mode Point d'Accès simultané (AP+STA)
     * @param timeout Temps d'attente maximum par tentative (ms)
     * 
     * @note Tente de se connecter au réseau avec la meilleure priorité
     * @note Bloquante (compatibilité) : attend la fin de la première tentative
     *       (succès ou échec) ou le timeout. Pour du non-bloquant, voir beginAsync().
     * @warning Nécessite d'avoir appelé addNetwork() au préalable
     */
    void begin(bool enableAP = false, uint32_t timeout = 10000);

    /**
     * @brief Démarre la connexion WiFi avec un réseau spécifique (mode legacy)
     * @param ssid Nom du réseau WiFi
     * @param password Mot de passe du réseau WiFi
     * @param enableAP Active le mode Point d'Accès simultané (AP+STA)
     * @param timeout Temps d'attente maximum pour la connexion (ms)
     * 
     * @note Cette méthode ajoute automatiquement le réseau et appelle begin()
     */
    void begin(const char* ssid, const char* password, bool enableAP = false, uint32_t timeout = 10000);

    /**
     * @brief Démarre la connexion WiFi sans bloquer (machine à états)
     * @param enableAP Active le mode Point d'Accès simultané (AP+STA)
     * @param timeout Temps d'attente maximum par tentative (ms)
     * @return true si une tentative de connexion a été lancée
     * 
     * @note La connexion progresse dans update() (à appeler dans loop()).
     * @note Vérifier l'état avec isConnected() ; attendre avec waitForConnection().
     */
    bool beginAsync(bool enableAP = false, uint32_t timeout = 10000);

    /**
     * @brief Attend (sans bloquer le WiFi) jusqu'à connexion ou timeout
     * @param timeoutMs Durée maximum d'attente (ms)
     * @return true si connecté, false si timeout expiré
     * 
     * @note Boucle sur update() avec délai non bloquant.
     */
    bool waitForConnection(uint32_t timeoutMs);

    /**
     * @brief Met à jour la gestion WiFi - à appeler dans loop()
     * 
     * @note Gère le basculement automatique entre réseaux en cas d'échec
     * @note Détecte les changements d'état et met à jour l'historique
     */
    void update();

    // ===========================================
    // GESTION DES RÉSEAUX (MULTI-RÉSEAUX)
    // ===========================================

    /**
     * @brief Ajoute un réseau WiFi à la liste
     * @param ssid Nom du réseau WiFi
     * @param password Mot de passe du réseau WiFi
     * @param priority Priorité du réseau (0 = plus haute, défaut: 0)
     * @return true si le réseau a été ajouté, false si la liste est pleine
     * 
     * @note Les réseaux sont tentés par ordre de priorité croissante
     */
    bool addNetwork(const char* ssid, const char* password, int priority = 0);

    /**
     * @brief Supprime tous les réseaux configurés
     */
    void clearNetworks();

    /**
     * @brief Retourne le nombre de réseaux configurés
     */
    int getNetworkCount() const;

    /**
     * @brief Retourne l'index du réseau actuellement connecté
     * @return Index du réseau, ou -1 si non connecté
     */
    int getCurrentNetworkIndex() const;

    /**
     * @brief Retourne le SSID d'un réseau configuré
     * @param index Index du réseau
     * @return SSID du réseau, ou chaîne vide si index invalide
     */
    String getNetworkSSID(int index) const;

    /**
     * @brief Retourne le nombre d'échecs d'un réseau
     * @param index Index du réseau
     * @return Nombre d'échecs, ou -1 si index invalide
     */
    int getNetworkFailCount(int index) const;

    /**
     * @brief Bascule manuellement vers le réseau suivant
     * @return true si le basculement a été initié
     */
    bool switchToNextNetwork();

    /**
     * @brief Bascule manuellement vers un réseau spécifique
     * @param index Index du réseau cible
     * @return true si le basculement a été initié
     */
    bool switchToNetwork(int index);

    /**
     * @brief Configure le basculement automatique entre réseaux
     * @param enable true pour activer le basculement auto (défaut: true)
     */
    void setAutoSwitch(bool enable);

    /**
     * @brief Configure la reconnexion automatique native du stack WiFi
     * @param enable true pour activer (défaut: true)
     * 
     * @note Le stack WiFi (ESP32/ESP8266) retente lui-même le réseau courant
     *       lors des coupures brèves. La bibliothèque ne prend le relais
     *       (failover) que si la coupure persiste.
     */
    void setAutoReconnect(bool enable);

    /**
     * @brief Configure le nombre maximum d'échecs avant basculement
     * @param retries Nombre d'échecs (défaut: 3)
     */
    void setMaxRetries(int retries);

    /**
     * @brief Configure le délai entre les tentatives de connexion
     * @param ms Délai en millisecondes (défaut: 10000)
     */
    void setRetryDelay(unsigned long ms);

    /**
     * @brief Configure le délai de "cooldown" après échec avant réessai
     * @param ms Délai en millisecondes (défaut: 60000)
     */
    void setFailCooldown(unsigned long ms);

    // ===========================================
    // GESTION DES CREDENTIALS (MODE LEGACY)
    // ===========================================

    /**
     * @brief Configure les credentials du réseau WiFi (mode legacy mono-réseau)
     * @param ssid Nom du réseau WiFi
     * @param password Mot de passe du réseau WiFi
     * 
     * @note Équivalent à addNetwork() avec priorité 0
     * @deprecated Utilisez addNetwork() pour le multi-réseaux
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
    // mDNS (MULTICAST DNS)
    // ===========================================

    /**
     * @brief Active ou désactive le mDNS automatique après connexion
     * @param enable true pour activer le mDNS auto (défaut: true)
     */
    void setAutoMDNS(bool enable);

    /**
     * @brief Démarre manuellement le responder mDNS
     * @param hostname Nom mDNS souhaité (ex: "mon-esp" -> mon-esp.local)
     * @return true si le mDNS a démarré avec succès
     */
    bool startMDNS(const char* hostname = nullptr);

    /** @brief Arrête le responder mDNS */
    void stopMDNS();

    /** @brief Vérifie si le mDNS est actuellement actif */
    bool isMDNSRunning() const;

    /** @brief Retourne le nom mDNS actuellement utilisé */
    String getMDNSHostname() const;

    /**
     * @brief Ajoute un service à annoncer via mDNS
     * @param name Nom du service (ex: "http", "https", "ftp")
     * @param protocol Protocole (ex: "tcp", "udp")
     * @param port Port du service
     * @return true si le service a été ajouté
     */
    bool addMDNSService(const char* name, const char* protocol, uint16_t port);

    /** @brief Supprime tous les services mDNS configurés */
    void clearMDNSServices();

    /** @brief Retourne le nombre de services mDNS configurés */
    int getMDNSServiceCount() const;

    /**
     * @brief Ajoute un enregistrement TXT à un service mDNS existant
     * @param serviceName Nom du service
     * @param protocol Protocole
     * @param key Clé TXT
     * @param value Valeur TXT
     * @return true si l'enregistrement a été ajouté
     */
    bool addMDNSTxtRecord(const char* serviceName, const char* protocol, const char* key, const char* value);

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

    /** @brief Force une reconnexion au réseau WiFi actuel */
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

    // ===========================================
    // HISTORIQUE DES CONNEXIONS
    // ===========================================

    /**
     * @brief Retourne le nombre d'entrées dans l'historique
     */
    int getHistoryCount() const;

    /**
     * @brief Retourne une entrée de l'historique
     * @param index Index de l'entrée (0 = plus récente)
     * @return Pointeur vers l'entrée, ou nullptr si index invalide
     */
    const ConnectionHistoryEntry* getHistoryEntry(int index) const;

    /**
     * @brief Affiche l'historique des connexions sur Serial
     */
    void printHistory() const;

    /**
     * @brief Efface l'historique des connexions
     */
    void clearHistory();

private:
    // ===========================================
    // VARIABLES MEMBRES - RÉSEAUX
    // ===========================================

    WiFiNetwork _networks[WIFIMANAGER_MAX_NETWORKS];  ///< Tableau des réseaux configurés
    int _networkCount = 0;                            ///< Nombre de réseaux configurés
    int _currentNetwork = -1;                         ///< Index du réseau actuel (-1 = aucun)
    int _lastConnectedNetwork = -1;                   ///< Dernier réseau connecté avec succès
    int _lastTriedNetwork = -1;                       ///< Dernier réseau essayé (pour éviter réessai immédiat)

    // ===========================================
    // VARIABLES MEMBRES - CONFIGURATION
    // ===========================================

    bool _autoSwitch = true;            ///< Basculement automatique activé
    bool _autoReconnect = true;         ///< Reconnexion automatique native du stack WiFi
    int _maxRetries = 3;                ///< Échecs avant basculement
    unsigned long _retryDelay = 10000;  ///< Délai entre tentatives (ms)
    unsigned long _failCooldown = 60000; ///< Délai avant réessai après échec (ms)

    // ===========================================
    // VARIABLES MEMBRES - AP
    // ===========================================

    const char* _ap_ssid = "ESP_WiFi_AP";   ///< SSID du point d'accès créé
    const char* _ap_password = "password123"; ///< Mot de passe du point d'accès
    bool _apEnabled = false;                ///< Indique si le mode AP est actif

    // ===========================================
    // VARIABLES MEMBRES - HOSTNAME
    // ===========================================

    const char* _hostname = nullptr;        ///< Hostname fixe (prioritaire)
    const char* _hostname_prefix = "ESP_Device_"; ///< Préfixe pour hostname auto

    // ===========================================
    // VARIABLES MEMBRES - mDNS
    // ===========================================

    bool _autoMDNS = true;                  ///< mDNS automatique activé
    bool _mdnsRunning = false;              ///< État du responder mDNS
    char _mdnsHostname[64];                 ///< Nom mDNS actuellement utilisé
    MDNSService _mdnsServices[WIFIMANAGER_MAX_MDNS_SERVICES]; ///< Services mDNS configurés
    int _mdnsServiceCount = 0;              ///< Nombre de services configurés
    bool _mdnsPendingStart = false;         ///< Démarrage mDNS en attente de connexion

    // ===========================================
    // VARIABLES MEMBRES - ÉTAT
    // ===========================================

    /**
     * @brief Machine à états de la connexion (état toujours défini)
     *
     * CONN_IDLE       : aucune tentative en cours (repos)
     * CONN_CONNECTING : WIFI_LIB.begin() lancé, attente de WL_CONNECTED
     * CONN_CONNECTED  : connecté (état stable)
     * CONN_FAILED     : tentative terminée en échec (retour IDLE au prochain update)
     */
    enum class ConnectionState : uint8_t {
        CONN_IDLE,
        CONN_CONNECTING,
        CONN_CONNECTED,
        CONN_FAILED
    };

    ConnectionState _connState = ConnectionState::CONN_IDLE; ///< État de la machine à états
    int _connTargetIndex = -1;        ///< Index du réseau de la tentative en cours
    uint32_t _connTimeout = 10000;    ///< Timeout de la tentative en cours (ms)

    wl_status_t _currentStatus = WL_DISCONNECTED;   ///< État WiFi en cache
    String _currentStatusText = "";                 ///< Description textuelle de l'état
    unsigned long _lastWifiEvent = 0;               ///< Timestamp du dernier changement d'état
    bool _wifiInitialized = false;                  ///< WiFi initialisé
    unsigned long _lastConnectionAttempt = 0;       ///< Timestamp dernière tentative
    unsigned long _connectionStartTime = 0;         ///< Timestamp début connexion actuelle

    // ===========================================
    // VARIABLES MEMBRES - HISTORIQUE
    // ===========================================

    ConnectionHistoryEntry _history[WIFIMANAGER_MAX_HISTORY]; ///< Historique des connexions
    int _historyIndex = 0;                                      ///< Index circulaire
    int _historyCount = 0;                                      ///< Nombre d'entrées valides

    // ===========================================
    // MÉTHODES PRIVÉES - MULTI-RÉSEAUX
    // ===========================================

    /**
     * @brief Trie les réseaux par priorité (ordre croissant)
     */
    void _sortNetworksByPriority();

    /**
     * @brief Lance une tentative de connexion asynchrone vers un réseau
     * @param index Index du réseau dans le tableau
     * @param timeout Délai maximum de la tentative (ms)
     * @return true si la tentative a été lancée, false si index invalide ou déjà en cours
     * 
     * @note Non bloquant : la progression se fait dans update() via _handleConnection().
     */
    bool _startConnection(int index, uint32_t timeout);

    /**
     * @brief Pilote la machine à états de connexion (appelée par update())
     */
    void _handleConnection();

    /**
     * @brief Finalise une connexion réussie (reset failCount, historique, mDNS)
     */
    void _onConnectSuccess();

    /**
     * @brief Finalise une tentative en échec (failCount++, historique, failover)
     */
    void _onConnectFail();

    /**
     * @brief Trouve le meilleur réseau à tenter (priorité + cooldown)
     * @return Index du réseau, ou -1 si aucun disponible
     */
    int _findBestNetwork();

    /**
     * @brief Réinitialise le WiFi proprement (déconnexion + mode OFF + mode STA)
     */
    void _resetWiFi();

    /**
     * @brief Ajoute une entrée dans l'historique
     */
    void _addToHistory(const char* ssid, const char* status, const char* ip, int rssi);

    /**
     * @brief Formate un temps en millisecondes en chaîne lisible
     */
    String _formatTime(unsigned long ms) const;

    // ===========================================
    // MÉTHODES PRIVÉES - mDNS
    // ===========================================

    /** @brief Démarre le mDNS avec le hostname configuré */
    bool _startMDNSInternal();

    /** @brief Annonce tous les services mDNS configurés */
    void _announceMDNSServices();

    /** @brief Trouve l'index d'un service mDNS par nom et protocole */
    int _findMDNSService(const char* name, const char* protocol) const;

    // ===========================================
    // MÉTHODES PRIVÉES - INITIALISATION
    // ===========================================

    /**
     * @brief Initialise le WiFi (AP + hostname) et lance la première tentative
     * @param enableAP Active le mode AP+STA
     * @param timeout Délai maximum de la tentative de connexion
     * @return true si une tentative a été lancée
     */
    bool _beginInternal(bool enableAP, uint32_t timeout);

    /** @brief Configure les callbacks d'événements WiFi selon la plateforme */
    void _setupCallbacks();

    /** @brief Applique la configuration du hostname à la bibliothèque WiFi native */
    void _configureHostname();

    // ===========================================
    // MÉTHODES PRIVÉES - UTILITAIRES
    // ===========================================

    String _getStatusText(wl_status_t status);
    String _getModeText();
    String _macToString(const uint8_t* mac);

    // ===========================================
    // CALLBACKS (spécifiques par plateforme)
    // ===========================================

#if defined(ESP8266)
    void _setupCallbacksESP8266();
#elif defined(ESP32)
    void _WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
#endif
};

// ===========================================
// IMPLÉMENTATION - CONSTRUCTEUR
// ===========================================

WiFiManagerESP::WiFiManagerESP() {
    // Initialiser le tableau des réseaux
    for (int i = 0; i < WIFIMANAGER_MAX_NETWORKS; i++) {
        _networks[i].ssid = nullptr;
        _networks[i].password = nullptr;
        _networks[i].priority = 99;
        _networks[i].failCount = 0;
        _networks[i].lastFail = 0;
        _networks[i].lastAttempt = 0;
        _networks[i].configured = false;
        _lastTriedNetwork = -1;
    }
    // Initialiser l'historique
    for (int i = 0; i < WIFIMANAGER_MAX_HISTORY; i++) {
        memset(&_history[i], 0, sizeof(ConnectionHistoryEntry));
        _history[i].used = false;
    }
    // Initialiser les services mDNS
    for (int i = 0; i < WIFIMANAGER_MAX_MDNS_SERVICES; i++) {
        memset(&_mdnsServices[i], 0, sizeof(MDNSService));
        _mdnsServices[i].configured = false;
    }
    _mdnsHostname[0] = '\0';
}

// ===========================================
// IMPLÉMENTATION - GESTION DES RÉSEAUX
// ===========================================

bool WiFiManagerESP::addNetwork(const char* ssid, const char* password, int priority) {
    if (_networkCount >= WIFIMANAGER_MAX_NETWORKS) {
        Serial.println("[WiFiManagerESP] ERREUR: Nombre maximum de réseaux atteint");
        return false;
    }
    if (ssid == nullptr || strlen(ssid) == 0) {
        Serial.println("[WiFiManagerESP] ERREUR: SSID invalide");
        return false;
    }

    _networks[_networkCount] = {
        ssid,
        password ? password : "",
        priority,
        0,      // failCount
        0,      // lastFail
        0,      // lastAttempt
        true    // configured
    };
    _networkCount++;

    // Trier par priorité
    _sortNetworksByPriority();

    Serial.printf("[WiFiManagerESP] Réseau ajouté: %s (priorité=%d)\n", ssid, priority);
    return true;
}

void WiFiManagerESP::clearNetworks() {
    for (int i = 0; i < WIFIMANAGER_MAX_NETWORKS; i++) {
        _networks[i].ssid = nullptr;
        _networks[i].password = nullptr;
        _networks[i].priority = 99;
        _networks[i].failCount = 0;
        _networks[i].lastFail = 0;
        _networks[i].lastAttempt = 0;
        _networks[i].configured = false;
    }
    _networkCount = 0;
    _currentNetwork = -1;
    _lastConnectedNetwork = -1;
    Serial.println("[WiFiManagerESP] Tous les réseaux ont été supprimés");
}

int WiFiManagerESP::getNetworkCount() const {
    return _networkCount;
}

int WiFiManagerESP::getCurrentNetworkIndex() const {
    return _currentNetwork;
}

String WiFiManagerESP::getNetworkSSID(int index) const {
    if (index < 0 || index >= _networkCount || !_networks[index].configured) {
        return String("");
    }
    return String(_networks[index].ssid);
}

int WiFiManagerESP::getNetworkFailCount(int index) const {
    if (index < 0 || index >= _networkCount || !_networks[index].configured) {
        return -1;
    }
    return _networks[index].failCount;
}

bool WiFiManagerESP::switchToNextNetwork() {
    if (_networkCount <= 1) {
        Serial.println("[WiFiManagerESP] Pas assez de réseaux pour basculer");
        return false;
    }

    Serial.println("[WiFiManagerESP] Basculement vers le réseau suivant...");

    // Marquer le réseau actuel comme ayant échoué pour forcer le changement
    if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
        _networks[_currentNetwork].failCount = _maxRetries;
        _networks[_currentNetwork].lastFail = millis();
    }

    // Trouver et connecter au meilleur réseau suivant
    int nextNetwork = _findBestNetwork();
    if (nextNetwork >= 0) {
        return _startConnection(nextNetwork, 15000);
    }

    // Si aucun réseau n'est disponible, réessayer le premier
    if (_networkCount > 0) {
        Serial.println("[WiFiManagerESP] Tous les réseaux en cooldown, réessai du premier");
        _networks[0].failCount = 0; // Reset le compteur
        return _startConnection(0, 15000);
    }

    return false;
}

bool WiFiManagerESP::switchToNetwork(int index) {
    if (index < 0 || index >= _networkCount || !_networks[index].configured) {
        Serial.printf("[WiFiManagerESP] Index réseau invalide: %d\n", index);
        return false;
    }

    Serial.printf("[WiFiManagerESP] Basculement vers le réseau %d: %s\n", 
                  index, _networks[index].ssid);
    return _startConnection(index, 15000);
}

void WiFiManagerESP::setAutoSwitch(bool enable) {
    _autoSwitch = enable;
    Serial.printf("[WiFiManagerESP] Basculement auto: %s\n", enable ? "ACTIVÉ" : "DÉSACTIVÉ");
}

void WiFiManagerESP::setAutoReconnect(bool enable) {
    _autoReconnect = enable;
    WIFI_LIB.setAutoReconnect(_autoReconnect);
    Serial.printf("[WiFiManagerESP] Reconnexion auto WiFi: %s\n", enable ? "ACTIVÉE" : "DÉSACTIVÉE");
}

void WiFiManagerESP::setMaxRetries(int retries) {
    _maxRetries = max(1, retries);
    Serial.printf("[WiFiManagerESP] Max retries: %d\n", _maxRetries);
}

void WiFiManagerESP::setRetryDelay(unsigned long ms) {
    _retryDelay = ms;
    Serial.printf("[WiFiManagerESP] Retry delay: %lu ms\n", _retryDelay);
}

void WiFiManagerESP::setFailCooldown(unsigned long ms) {
    _failCooldown = ms;
    Serial.printf("[WiFiManagerESP] Fail cooldown: %lu ms\n", _failCooldown);
}

// ===========================================
// IMPLÉMENTATION - MÉTHODES PRIVÉES MULTI-RÉSEAUX
// ===========================================

void WiFiManagerESP::_sortNetworksByPriority() {
    // Tri par insertion simple (par priorité croissante)
    for (int i = 1; i < _networkCount; i++) {
        WiFiNetwork key = _networks[i];
        int j = i - 1;
        while (j >= 0 && _networks[j].priority > key.priority) {
            _networks[j + 1] = _networks[j];
            j--;
        }
        _networks[j + 1] = key;
    }
}

bool WiFiManagerESP::_startConnection(int index, uint32_t timeout) {
    if (_connState == ConnectionState::CONN_CONNECTING) {
        Serial.println("[WiFiManagerESP] Connexion déjà en cours, tentative ignorée");
        return false;
    }
    if (index < 0 || index >= _networkCount || !_networks[index].configured) {
        Serial.printf("[WiFiManagerESP] ERREUR: Réseau %d invalide\n", index);
        return false;
    }

    _currentNetwork = index;
    _connTargetIndex = index;
    _connTimeout = timeout;
    _connState = ConnectionState::CONN_CONNECTING;
    _connectionStartTime = millis();
    _lastConnectionAttempt = millis();
    _networks[index].lastAttempt = millis();

    Serial.printf("\n[WiFiManagerESP] 📡 Connexion à [%d] %s (priorité=%d)\n", 
                  index, _networks[index].ssid, _networks[index].priority);

    // Reset complet du WiFi (courte séquence bloquante ~600ms)
    _resetWiFi();

    // Démarrer la connexion (fire-and-forget : la progression est suivie par update())
    WIFI_LIB.begin(_networks[index].ssid, _networks[index].password);

    Serial.println("[WiFiManagerESP] ⏳ Connexion lancée (suivie par update())");

    return true;
}

void WiFiManagerESP::_handleConnection() {
    if (_connState != ConnectionState::CONN_CONNECTING) return;

    wl_status_t status = WIFI_LIB.status();

    if (status == WL_CONNECTED) {
        _onConnectSuccess();
    } else if (status == WL_CONNECT_FAILED) {
        // Échec définitif (ex: mot de passe erroné) : basculer sans attendre le timeout
        _onConnectFail();
    } else if (millis() - _connectionStartTime >= _connTimeout) {
        _onConnectFail();
    }
}

void WiFiManagerESP::_onConnectSuccess() {
    int index = _connTargetIndex;
    _connState = ConnectionState::CONN_CONNECTED;

    Serial.printf("[WiFiManagerESP] ✅ CONNECTÉ! IP=%s RSSI=%d dBm\n",
        WIFI_LIB.localIP().toString().c_str(), WIFI_LIB.RSSI());

    _networks[index].failCount = 0; // ← Reset SEULEMENT au succès
    _lastConnectedNetwork = index;
    _wifiInitialized = true;

    _addToHistory(_networks[index].ssid, "✅ Connecté", 
                 WIFI_LIB.localIP().toString().c_str(), WIFI_LIB.RSSI());

    // Démarrer mDNS automatiquement si activé
    if (_autoMDNS) {
        _startMDNSInternal();
    }

    updateStatus();
}

void WiFiManagerESP::_onConnectFail() {
    int index = _connTargetIndex;
    wl_status_t finalStatus = WIFI_LIB.status();

    Serial.printf("[WiFiManagerESP] ❌ Échec (status=%d: %s)\n", 
                 (int)finalStatus, _getStatusText(finalStatus).c_str());

    _networks[index].failCount++;  // Incrémenté en cas d'échec
    _networks[index].lastFail = millis();

    _addToHistory(_networks[index].ssid, "❌ Échec", "", 0);

    updateStatus();

    // Laisse update() déclencher le failover au prochain passage (état IDLE)
    _connState = ConnectionState::CONN_FAILED;
}

// ===========================================
// IMPLÉMENTATION - _findBestNetwork()
// ===========================================

int WiFiManagerESP::_findBestNetwork() {
    if (_networkCount == 0) return -1;

    int bestNetwork = -1;
    int bestPriority = 999;
    unsigned long now = millis();

    // Première passe : chercher le réseau avec la meilleure priorité
    // qui n'est PAS le réseau actuel et qui n'est PAS en cooldown
    for (int i = 0; i < _networkCount; i++) {
        // Sauter le réseau actuel
        if (i == _currentNetwork) continue;

        // Réinitialiser le compteur si le cooldown est passé
        if (_networks[i].failCount > 0 && 
            now - _networks[i].lastFail >= _failCooldown) {
            _networks[i].failCount = 0;
        }

        // Vérifier si le réseau est en cooldown
        if (_networks[i].failCount > 0 && 
            now - _networks[i].lastFail < _failCooldown) {
            continue; // Réseau en cooldown
        }

        // Chercher le réseau avec la meilleure priorité (la plus petite valeur)
        if (_networks[i].priority < bestPriority) {
            bestPriority = _networks[i].priority;
            bestNetwork = i;
        }
    }

    // Si on a trouvé un réseau, le retourner
    if (bestNetwork >= 0) {
        return bestNetwork;
    }

    // Deuxième passe : tous les autres réseaux sont en cooldown
    // Réinitialiser le réseau actuel et le réessayer
    if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
        Serial.println("[WiFiManagerESP] Tous les réseaux en cooldown, reset du réseau actuel");
        _networks[_currentNetwork].failCount = 0;
        return _currentNetwork;
    }

    // Dernière chance : réinitialiser TOUS les réseaux et reprendre au début
    Serial.println("[WiFiManagerESP] Reset de tous les réseaux");
    for (int i = 0; i < _networkCount; i++) {
        _networks[i].failCount = 0;
    }
    return 0;
}

void WiFiManagerESP::_resetWiFi() {
    WIFI_LIB.disconnect(true);
    NON_BLOCKING_DELAY(300);
    WIFI_LIB.mode(WIFI_OFF);
    NON_BLOCKING_DELAY(200);
    _configureHostname();
    WIFI_LIB.mode(WIFI_STA);
    NON_BLOCKING_DELAY(100);
}

void WiFiManagerESP::_addToHistory(const char* ssid, const char* status, const char* ip, int rssi) {
    unsigned long now = millis();
    unsigned long h = now / 3600000;
    unsigned long m = (now % 3600000) / 60000;
    unsigned long s = (now % 60000) / 1000;

    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu", h, m, s);

    memset(&_history[_historyIndex], 0, sizeof(ConnectionHistoryEntry));
    _history[_historyIndex].used = true;
    strncpy(_history[_historyIndex].timestamp, timeStr, sizeof(_history[_historyIndex].timestamp) - 1);
    strncpy(_history[_historyIndex].ssid, ssid, sizeof(_history[_historyIndex].ssid) - 1);
    strncpy(_history[_historyIndex].status, status, sizeof(_history[_historyIndex].status) - 1);
    strncpy(_history[_historyIndex].ip, ip, sizeof(_history[_historyIndex].ip) - 1);
    _history[_historyIndex].rssi = rssi;
    _history[_historyIndex].used = true;

    _historyIndex = (_historyIndex + 1) % WIFIMANAGER_MAX_HISTORY;
    if (_historyCount < WIFIMANAGER_MAX_HISTORY) _historyCount++;
}

String WiFiManagerESP::_formatTime(unsigned long ms) const {
    unsigned long sec = ms / 1000;
    unsigned long min = sec / 60;
    unsigned long hr = min / 60;
    sec %= 60; min %= 60;
    char buf[20];
    if (hr > 0) sprintf(buf, "%02luh%02lum", hr, min);
    else if (min > 0) sprintf(buf, "%02lum%02lus", min, sec);
    else sprintf(buf, "%02lus", sec);
    return String(buf);
}

// ===========================================
// IMPLÉMENTATION - mDNS
// ===========================================

void WiFiManagerESP::setAutoMDNS(bool enable) {
    _autoMDNS = enable;
    Serial.printf("[WiFiManagerESP] mDNS auto: %s\n", enable ? "ACTIVÉ" : "DÉSACTIVÉ");
}

bool WiFiManagerESP::startMDNS(const char* hostname) {
    if (hostname != nullptr && strlen(hostname) > 0) {
        strncpy(_mdnsHostname, hostname, sizeof(_mdnsHostname) - 1);
        _mdnsHostname[sizeof(_mdnsHostname) - 1] = '\0';
    } else if (_hostname != nullptr && strlen(_hostname) > 0) {
        strncpy(_mdnsHostname, _hostname, sizeof(_mdnsHostname) - 1);
        _mdnsHostname[sizeof(_mdnsHostname) - 1] = '\0';
    } else if (_hostname_prefix != nullptr) {
        String autoHostname;
#if defined(ESP8266)
        autoHostname = String(_hostname_prefix) + String(ESP.getChipId());
#elif defined(ESP32)
        autoHostname = String(_hostname_prefix) + String((uint32_t)(ESP.getEfuseMac() >> 32));
#endif
        strncpy(_mdnsHostname, autoHostname.c_str(), sizeof(_mdnsHostname) - 1);
        _mdnsHostname[sizeof(_mdnsHostname) - 1] = '\0';
    } else {
        Serial.println("[WiFiManagerESP] ERREUR mDNS: Aucun hostname configuré");
        return false;
    }
    return _startMDNSInternal();
}

void WiFiManagerESP::stopMDNS() {
    if (_mdnsRunning) {
#if defined(ESP8266)
        MDNS.close();
#elif defined(ESP32)
        mdns_free();
#endif
        _mdnsRunning = false;
        Serial.println("[WiFiManagerESP] mDNS arrêté");
    }
}

bool WiFiManagerESP::isMDNSRunning() const {
    return _mdnsRunning;
}

String WiFiManagerESP::getMDNSHostname() const {
    if (_mdnsRunning && _mdnsHostname[0] != '\0') {
        return String(_mdnsHostname);
    }
    return String("");
}

bool WiFiManagerESP::addMDNSService(const char* name, const char* protocol, uint16_t port) {
    if (_mdnsServiceCount >= WIFIMANAGER_MAX_MDNS_SERVICES) {
        Serial.println("[WiFiManagerESP] ERREUR: Nombre maximum de services mDNS atteint");
        return false;
    }
    if (name == nullptr || strlen(name) == 0 || protocol == nullptr || strlen(protocol) == 0) {
        Serial.println("[WiFiManagerESP] ERREUR: Nom ou protocole de service invalide");
        return false;
    }
    if (port == 0) {
        Serial.println("[WiFiManagerESP] ERREUR: Port de service invalide");
        return false;
    }
    if (_findMDNSService(name, protocol) >= 0) {
        Serial.printf("[WiFiManagerESP] Service mDNS déjà existant: %s.%s\n", name, protocol);
        return false;
    }
    strncpy(_mdnsServices[_mdnsServiceCount].name, name, sizeof(_mdnsServices[_mdnsServiceCount].name) - 1);
    strncpy(_mdnsServices[_mdnsServiceCount].protocol, protocol, sizeof(_mdnsServices[_mdnsServiceCount].protocol) - 1);
    _mdnsServices[_mdnsServiceCount].port = port;
    _mdnsServices[_mdnsServiceCount].configured = true;
    _mdnsServiceCount++;
    Serial.printf("[WiFiManagerESP] Service mDNS ajouté: %s.%s port=%d\n", name, protocol, port);
    if (_mdnsRunning) {
#if defined(ESP8266)
        MDNS.addService(name, protocol, port);
#elif defined(ESP32)
        mdns_service_add(NULL, name, protocol, port, NULL, 0);
#endif
    }
    return true;
}

void WiFiManagerESP::clearMDNSServices() {
    for (int i = 0; i < WIFIMANAGER_MAX_MDNS_SERVICES; i++) {
        memset(&_mdnsServices[i], 0, sizeof(MDNSService));
        _mdnsServices[i].configured = false;
    }
    _mdnsServiceCount = 0;
    Serial.println("[WiFiManagerESP] Tous les services mDNS ont été supprimés");
}

int WiFiManagerESP::getMDNSServiceCount() const {
    return _mdnsServiceCount;
}

bool WiFiManagerESP::addMDNSTxtRecord(const char* serviceName, const char* protocol, const char* key, const char* value) {
    if (!_mdnsRunning) {
        Serial.println("[WiFiManagerESP] ERREUR: mDNS non démarré");
        return false;
    }
    if (serviceName == nullptr || protocol == nullptr || key == nullptr) {
        Serial.println("[WiFiManagerESP] ERREUR: Paramètres TXT invalides");
        return false;
    }
    int svcIdx = _findMDNSService(serviceName, protocol);
    if (svcIdx < 0) {
        Serial.printf("[WiFiManagerESP] ERREUR: Service %s.%s non trouvé\n", serviceName, protocol);
        return false;
    }
#if defined(ESP8266)
    MDNS.addServiceTxt(serviceName, protocol, key, value ? value : "");
#elif defined(ESP32)
    mdns_service_txt_item_set(serviceName, protocol, key, value ? value : "");
#endif
    Serial.printf("[WiFiManagerESP] TXT record ajouté: %s.%s -> %s=%s\n", serviceName, protocol, key, value ? value : "");
    return true;
}

bool WiFiManagerESP::_startMDNSInternal() {
    if (_mdnsHostname[0] == '\0') {
        if (_hostname != nullptr && strlen(_hostname) > 0) {
            strncpy(_mdnsHostname, _hostname, sizeof(_mdnsHostname) - 1);
            _mdnsHostname[sizeof(_mdnsHostname) - 1] = '\0';
        } else if (_hostname_prefix != nullptr) {
            String autoHostname;
#if defined(ESP8266)
            autoHostname = String(_hostname_prefix) + String(ESP.getChipId());
#elif defined(ESP32)
            autoHostname = String(_hostname_prefix) + String((uint32_t)(ESP.getEfuseMac() >> 32));
#endif
            strncpy(_mdnsHostname, autoHostname.c_str(), sizeof(_mdnsHostname) - 1);
            _mdnsHostname[sizeof(_mdnsHostname) - 1] = '\0';
        } else {
            Serial.println("[WiFiManagerESP] ERREUR mDNS: Aucun hostname configuré");
            return false;
        }
    }
    if (_mdnsRunning) {
        stopMDNS();
    }
#if defined(ESP8266)
    if (!MDNS.begin(_mdnsHostname)) {
        Serial.printf("[WiFiManagerESP] ERREUR: Échec du démarrage mDNS (ESP8266)\n");
        _mdnsRunning = false;
        return false;
    }
#elif defined(ESP32)
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        Serial.printf("[WiFiManagerESP] ERREUR: Échec de l'init mDNS (ESP32), err=%d\n", err);
        _mdnsRunning = false;
        return false;
    }
    mdns_hostname_set(_mdnsHostname);
    mdns_instance_name_set(_mdnsHostname);
#endif
    _mdnsRunning = true;
    Serial.printf("[WiFiManagerESP] ✅ mDNS démarré: %s.local\n", _mdnsHostname);
    _announceMDNSServices();
    return true;
}

void WiFiManagerESP::_announceMDNSServices() {
    if (!_mdnsRunning) return;
    for (int i = 0; i < _mdnsServiceCount; i++) {
        if (!_mdnsServices[i].configured) continue;
#if defined(ESP8266)
        MDNS.addService(_mdnsServices[i].name, _mdnsServices[i].protocol, _mdnsServices[i].port);
#elif defined(ESP32)
        mdns_service_add(NULL, _mdnsServices[i].name, _mdnsServices[i].protocol, _mdnsServices[i].port, NULL, 0);
#endif
        Serial.printf("[WiFiManagerESP] mDNS service annoncé: %s.%s port=%d\n",
                      _mdnsServices[i].name, _mdnsServices[i].protocol, _mdnsServices[i].port);
    }
}

int WiFiManagerESP::_findMDNSService(const char* name, const char* protocol) const {
    for (int i = 0; i < _mdnsServiceCount; i++) {
        if (_mdnsServices[i].configured &&
            strcmp(_mdnsServices[i].name, name) == 0 &&
            strcmp(_mdnsServices[i].protocol, protocol) == 0) {
            return i;
        }
    }
    return -1;
}

// ===========================================
// IMPLÉMENTATION - HISTORIQUE
// ===========================================

int WiFiManagerESP::getHistoryCount() const {
    return _historyCount;
}

const ConnectionHistoryEntry* WiFiManagerESP::getHistoryEntry(int index) const {
    if (index < 0 || index >= _historyCount) return nullptr;
    int idx = (_historyIndex - 1 - index + WIFIMANAGER_MAX_HISTORY) % WIFIMANAGER_MAX_HISTORY;
    if (!_history[idx].used) return nullptr;
    return &_history[idx];
}

void WiFiManagerESP::printHistory() const {
    Serial.println("\n=== 📊 HISTORIQUE DES CONNEXIONS ===");
    if (_historyCount == 0) {
        Serial.println("(vide)");
    } else {
        Serial.println("Heure     | SSID                             | Statut         | IP              | RSSI");
        Serial.println("----------+----------------------------------+----------------+-----------------+------");
        for (int i = 0; i < _historyCount; i++) {
            const ConnectionHistoryEntry* entry = getHistoryEntry(i);
            if (entry) {
                Serial.printf("%-9s | %-32s | %-16s | %-15s | %4ddB\n",
                    entry->timestamp, entry->ssid, entry->status, 
                    entry->ip, entry->rssi);
            }
        }
    }
    Serial.println("==================================\n");
}

void WiFiManagerESP::clearHistory() {
    for (int i = 0; i < WIFIMANAGER_MAX_HISTORY; i++) {
        memset(&_history[i], 0, sizeof(ConnectionHistoryEntry));
        _history[i].used = false;
    }
    _historyIndex = 0;
    _historyCount = 0;
    Serial.println("[WiFiManagerESP] Historique effacé");
}

// ===========================================
// IMPLÉMENTATION - MÉTHODES PRINCIPALES
// ===========================================

bool WiFiManagerESP::_beginInternal(bool enableAP, uint32_t timeout) {
    if (_networkCount == 0) {
        Serial.println("[WiFiManagerESP] ERREUR: Aucun réseau configuré. Utilisez addNetwork() d'abord.");
        return false;
    }

    _apEnabled = enableAP;
    _setupCallbacks();

    // Reconnexion automatique native du stack WiFi (à régler avant WiFi.begin())
    WIFI_LIB.setAutoReconnect(_autoReconnect);

    // Configurer le hostname
    _configureHostname();

    // Configurer le mode AP si demandé
    if (enableAP) {
#if defined(ESP8266)
        WIFI_LIB.mode(WIFI_AP_STA);
#elif defined(ESP32)
        WIFI_LIB.mode(WIFI_MODE_APSTA);
#endif
        Serial.print("[WiFiManagerESP] Création du point d'accès: ");
        Serial.println(_ap_ssid);

        if (strlen(_ap_password) >= 8) {
            WIFI_LIB.softAP(_ap_ssid, _ap_password);
        } else {
            WIFI_LIB.softAP(_ap_ssid);
        }

        Serial.print("[WiFiManagerESP] AP IP: ");
        Serial.println(getAPIP());
    } else {
#if defined(ESP8266)
        WIFI_LIB.mode(WIFI_STA);
#elif defined(ESP32)
        WIFI_LIB.mode(WIFI_MODE_STA);
#endif
    }

    _wifiInitialized = true;

    // Tenter de se connecter au meilleur réseau (asynchrone)
    int bestNetwork = _findBestNetwork();
    if (bestNetwork < 0) {
        _connState = ConnectionState::CONN_IDLE;
        updateStatus();
        return false;
    }

    updateStatus();
    return _startConnection(bestNetwork, timeout);
}

void WiFiManagerESP::begin(bool enableAP, uint32_t timeout) {
    if (!_beginInternal(enableAP, timeout)) return;

    // Attente bloquante (compatibilité) : pilote la tentative en cours
    // jusqu'à son terme (succès ou échec) ou le timeout, sans failover.
    unsigned long start = millis();
    while (millis() - start < timeout &&
           _connState == ConnectionState::CONN_CONNECTING) {
        _handleConnection();
#if defined(ESP8266)
        if (_mdnsRunning) {
            MDNS.update();
        }
#endif
        NON_BLOCKING_DELAY(50);
    }
}

bool WiFiManagerESP::beginAsync(bool enableAP, uint32_t timeout) {
    return _beginInternal(enableAP, timeout);
}

bool WiFiManagerESP::waitForConnection(uint32_t timeoutMs) {
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        update();
        if (isConnected()) return true;
        NON_BLOCKING_DELAY(50);
    }
    return isConnected();
}

void WiFiManagerESP::begin(const char* ssid, const char* password, bool enableAP, uint32_t timeout) {
    clearNetworks();
    addNetwork(ssid, password, 0);
    begin(enableAP, timeout);
}

// ===========================================
// IMPLÉMENTATION - update()
// ===========================================

void WiFiManagerESP::update() {
    if (!_wifiInitialized || _networkCount == 0) return;

    wl_status_t previousStatus = _currentStatus;
    updateStatus();

    // Détection de perte de lien : soit détectée ici (previousStatus), soit déjà
    // rafraîchie par un callback WiFi (on compare aussi l'état machine CONNECTED).
    bool lostLink = (previousStatus == WL_CONNECTED && _currentStatus != WL_CONNECTED) ||
                    (_connState == ConnectionState::CONN_CONNECTED && _currentStatus != WL_CONNECTED);

    if (lostLink) {
        Serial.println("[WiFiManagerESP] ⚠️ Connexion perdue détectée!");
        if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
            _addToHistory(_networks[_currentNetwork].ssid, "❌ Déconnecté", "", 0);
        }
        // Sortir de l'état CONNECTED pour permettre au failover de se déclencher
        if (_connState == ConnectionState::CONN_CONNECTED) {
            _connState = ConnectionState::CONN_IDLE;
        }
    }

    // Piloter la tentative de connexion en cours (machine à états)
    _handleConnection();

    // Après un échec, repasser en repos pour laisser le failover décider
    if (_connState == ConnectionState::CONN_FAILED) {
        _connState = ConnectionState::CONN_IDLE;
    }

    if (_autoSwitch && _currentStatus != WL_CONNECTED && _connState == ConnectionState::CONN_IDLE) {
        unsigned long now = millis();

        if (now - _lastConnectionAttempt >= _retryDelay) {
            _lastConnectionAttempt = now;

            // Incrémenter le compteur d'échecs du réseau actuel
            if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
                _networks[_currentNetwork].failCount++;
                _networks[_currentNetwork].lastFail = now;
            }

            // Chercher le prochain réseau qui n'est pas en cooldown
            // en évitant de réessayer celui qui vient d'échouer (_lastTriedNetwork)
            int nextNetwork = -1;
            for (int offset = 1; offset <= _networkCount; offset++) {
                int candidate = (_currentNetwork + offset) % _networkCount;
                
                // Éviter de réessayer immédiatement le dernier réseau essayé avec échec
                if (candidate == _lastTriedNetwork && _networkCount > 1) continue;
                
                if (_networks[candidate].failCount == 0 ||
                    now - _networks[candidate].lastFail >= _failCooldown) {
                    nextNetwork = candidate;
                    break;
                }
            }

            if (nextNetwork >= 0) {
                _lastTriedNetwork = _currentNetwork;  // Mémoriser le réseau qu'on quitte
                _startConnection(nextNetwork, 15000);
            } else {
                // Tous en cooldown, reset et recommencer
                Serial.println("[WiFiManagerESP] Tous les réseaux en cooldown, reset global");
                for (int i = 0; i < _networkCount; i++) {
                    _networks[i].failCount = 0;
                }
                _lastTriedNetwork = -1;  // Reset le dernier essayé
                Serial.println("[WiFiManagerESP] 🔄 Réessai d'un réseau...");
                _startConnection(0, 15000);
            }
        }
    }
    
#if defined(ESP8266)
    if (_mdnsRunning) {
        MDNS.update();
    }
#endif
}

// ===========================================
// IMPLÉMENTATION - CREDENTIALS (LEGACY)
// ===========================================

void WiFiManagerESP::setCredentials(const char* ssid, const char* password) {
    clearNetworks();
    addNetwork(ssid, password, 0);
}

void WiFiManagerESP::setAPCredentials(const char* ap_ssid, const char* ap_password) {
    _ap_ssid = ap_ssid;
    _ap_password = ap_password;
}

// ===========================================
// IMPLÉMENTATION - HOSTNAME
// ===========================================

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

// ===========================================
// IMPLÉMENTATION - ÉTATS ET INFORMATIONS
// ===========================================

bool WiFiManagerESP::isConnected() {
    updateStatus();
    return (_currentStatus == WL_CONNECTED);
}

int WiFiManagerESP::updateStatus() {
    static wl_status_t lastStatus = WL_DISCONNECTED;

    wl_status_t newStatus = WIFI_LIB.status();

    if (newStatus != lastStatus) {
        _currentStatus = newStatus;
        _lastWifiEvent = millis();
        lastStatus = newStatus;
        _currentStatusText = _getStatusText(_currentStatus);

        Serial.print("[WiFiManagerESP] Statut changé: ");
        Serial.print((int)_currentStatus);
        Serial.print(" - ");
        Serial.println(_currentStatusText);
    }

    return (int)_currentStatus;
}

void WiFiManagerESP::printStatus(bool detailed) {
    updateStatus();

    Serial.println("\n=== ÉTAT WiFi MANAGER ===");
    Serial.print("Statut: ");
    Serial.print((int)_currentStatus);
    Serial.print(" - ");
    Serial.println(_currentStatusText);

    Serial.printf("Réseaux configurés: %d\n", _networkCount);
    if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
        Serial.printf("Réseau actuel: [%d] %s (échecs=%d)\n",
            _currentNetwork, _networks[_currentNetwork].ssid,
            _networks[_currentNetwork].failCount);
    } else {
        Serial.println("Réseau actuel: aucun");
    }

    if (detailed) {
        Serial.print("Mode WiFi: ");
        Serial.println(_getModeText());

        Serial.println("\n--- 📡 RÉSEAUX CONFIGURÉS ---");
        for (int i = 0; i < _networkCount; i++) {
            Serial.printf("%s [%d] %s | prio=%d | échecs=%d | dernier=%s\n",
                (i == _currentNetwork) ? "▶" : " ",
                i, _networks[i].ssid, _networks[i].priority,
                _networks[i].failCount,
                _formatTime(millis() - _networks[i].lastFail).c_str());
        }

        Serial.println("\n--- MODE CLIENT (STA) ---");
        Serial.print("Connexion: ");
        Serial.println(_currentStatus == WL_CONNECTED ? "OUI ✅" : "NON ❌");

        if (_currentStatus == WL_CONNECTED) {
            Serial.print("🌐 IP: ");
            Serial.println(getLocalIP());
            Serial.print("📡 Gateway: ");
            Serial.println(getGatewayIP());
            Serial.print("🖧 DNS: ");
            Serial.println(getDnsIP());
            Serial.print("🆔 MAC: ");
            Serial.println(getMacAddress());
            Serial.print("Hostname: ");
            Serial.println(getHostname());
            Serial.print("SSID: ");
            Serial.println(getSSID());
			Serial.print("RSSI: ");
            Serial.print(getRSSI());
            Serial.println(" dBm 📶");
        }

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

        // Section mDNS
        Serial.println("\n--- mDNS ---");
        Serial.print("mDNS actif: ");
        Serial.println(_mdnsRunning ? "OUI ✅" : "NON ❌");
        if (_mdnsRunning) {
            Serial.print("Nom mDNS: ");
            Serial.print(getMDNSHostname());
            Serial.println(".local");
            Serial.printf("Services annoncés: %d\n", _mdnsServiceCount);
            for (int i = 0; i < _mdnsServiceCount; i++) {
                if (_mdnsServices[i].configured) {
                    Serial.printf("  - %s.%s port=%d\n",
                        _mdnsServices[i].name, _mdnsServices[i].protocol, _mdnsServices[i].port);
                }
            }
        }
        Serial.print("mDNS auto: ");
        Serial.println(_autoMDNS ? "OUI ✅" : "NON ❌");

        Serial.println("\n--- INFORMATIONS GÉNÉRALES ---");
        Serial.print("📊 Dernier événement: ");
        Serial.print((millis() - _lastWifiEvent) / 1000);
        Serial.println(" secondes");
        Serial.print("Initialisé: ");
        Serial.println(_wifiInitialized ? "OUI ✅" : "NON ❌");
        Serial.print("Basculement auto: ");
        Serial.println(_autoSwitch ? "OUI ✅" : "NON ❌");
        Serial.print("Max retries: ");
        Serial.println(_maxRetries);
    }

    Serial.println("=========================\n");
}

// ===========================================
// IMPLÉMENTATION - GESTION CONNEXION
// ===========================================

void WiFiManagerESP::reconnect() {
    if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
        Serial.println("[WiFiManagerESP] Reconnexion au réseau actuel...");
        _startConnection(_currentNetwork, 15000);
    } else if (_networkCount > 0) {
        Serial.println("[WiFiManagerESP] Reconnexion au premier réseau...");
        _startConnection(0, 15000);
    } else {
        Serial.println("[WiFiManagerESP] ERREUR: Aucun réseau configuré");
    }
}

void WiFiManagerESP::disconnect() {
    WIFI_LIB.disconnect();
    _currentNetwork = -1;
    _connState = ConnectionState::CONN_IDLE;
    updateStatus();
}

// ===========================================
// IMPLÉMENTATION - GETTERS RÉSEAU
// ===========================================

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

String WiFiManagerESP::getStatusText() {
    updateStatus();
    return _currentStatusText;
}

wl_status_t WiFiManagerESP::getStatus() {
    updateStatus();
    return _currentStatus;
}

String WiFiManagerESP::getGatewayIP() {
    return WIFI_LIB.gatewayIP().toString();
}

String WiFiManagerESP::getDnsIP() {
    return WIFI_LIB.dnsIP().toString();
}

String WiFiManagerESP::getMacAddress() {
    uint8_t mac[6];
    WIFI_LIB.macAddress(mac);
    return _macToString(mac);
}

String WiFiManagerESP::getHostname() {
#if defined(ESP8266)
    return WIFI_LIB.hostname();
#elif defined(ESP32)
    return WIFI_LIB.getHostname();
#endif
}

// ===========================================
// IMPLÉMENTATION - MÉTHODES PRIVÉES UTILITAIRES
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
// IMPLÉMENTATION - CALLBACKS PAR PLATEFORME
// ===========================================

#if defined(ESP8266)
void WiFiManagerESP::_setupCallbacksESP8266() {
    // Callback pour IP attribuée
    WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP& evt) {
        Serial.print("[WiFiManagerESP] IP attribuée: ");
        Serial.println(evt.ip);
        this->updateStatus();
    });

    // Callback pour déconnexion - NE PAS reconnecter auto si multi-réseaux
    WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected& evt) {
        Serial.print("[WiFiManagerESP] Déconnecté! Raison: ");
        Serial.println(evt.reason);
        this->updateStatus();
        // La reconnexion est gérée par update() si autoSwitch est activé
    });

    // Callbacks AP
    WiFi.onSoftAPModeStationConnected([this](const WiFiEventSoftAPModeStationConnected& evt) {
        Serial.print("[WiFiManagerESP] [AP] Nouveau client: ");
        Serial.println(this->_macToString(evt.mac));
        this->updateStatus();
    });

    WiFi.onSoftAPModeStationDisconnected([this](const WiFiEventSoftAPModeStationDisconnected& evt) {
        Serial.print("[WiFiManagerESP] [AP] Client déconnecté: ");
        Serial.println(this->_macToString(evt.mac));
        this->updateStatus();
    });
}
#endif

#if defined(ESP32)
void WiFiManagerESP::_WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch(event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("[WiFiManagerESP] IP attribuée: ");
            Serial.println(WiFi.localIP());
            updateStatus();
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.print("[WiFiManagerESP] Déconnecté! Raison: ");
            Serial.println(info.wifi_sta_disconnected.reason);
            updateStatus();
            // La reconnexion est gérée par update() si autoSwitch est activé
            break;

        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            Serial.print("[WiFiManagerESP] [AP] Nouveau client. MAC: ");
            Serial.println(_macToString(info.wifi_ap_staconnected.mac));
            updateStatus();
            break;

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.print("[WiFiManagerESP] [AP] Client déconnecté. MAC: ");
            Serial.println(_macToString(info.wifi_ap_stadisconnected.mac));
            updateStatus();
            break;

        default:
            break;
    }
}
#endif

#endif // WIFIMANAGER_ESP_H
