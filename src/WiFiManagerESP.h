// ===========================================
// WiFiManagerESP.h - HEADER FILE
// v0.10.0 - Multi-Réseaux + mDNS + Classement dynamique
// ===========================================


/**
 * @file WiFiManagerESP.h
 * @brief Bibliothèque de gestion WiFi unifiée pour ESP8266 et ESP32
 *        avec support multi-réseaux et basculement automatique
 * @author Fo170
 * @version 0.10.0
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


// Version de la librairie (source UNIQUE — agrégée par liste_lib.h)
#define __WIFIMANAGERESP_H_VERSION__ "0.10.0"
#include <Arduino.h>
#include <functional>
#include <EEPROM.h>
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
  #include <esp_wifi.h>
  #include <esp_system.h>
  #define WIFI_MODE_APSTA WIFI_MODE_APSTA
#endif

#if !defined(ESP8266) && !defined(ESP32)
  #error "Plateforme non supportée ! Seuls ESP8266 et ESP32 sont supportés."
#endif

// ===========================================
// CONSTANTES
// ===========================================

/** @brief Nombre maximum de réseaux WiFi configurables */
#ifndef WIFIMANAGER_MAX_NETWORKS
#define WIFIMANAGER_MAX_NETWORKS 3
#endif

/** @brief Nombre maximum d'entrées dans l'historique des connexions */
#ifndef WIFIMANAGER_MAX_HISTORY
#define WIFIMANAGER_MAX_HISTORY 8
#endif

/** @brief Nombre maximum de services mDNS annoncés */
#ifndef WIFIMANAGER_MAX_MDNS_SERVICES
#define WIFIMANAGER_MAX_MDNS_SERVICES 2
#endif

/** @brief Nombre maximum de résultats conservés par un scan (scanNetworksAsync) */
#ifndef WIFIMANAGER_MAX_SCAN_RESULTS
#define WIFIMANAGER_MAX_SCAN_RESULTS 12
#endif

/** @brief Taille de l'EEPROM allouée à la persistance des réseaux (octets) */
#ifndef WIFIMANAGER_EEPROM_SIZE
#define WIFIMANAGER_EEPROM_SIZE 512
#endif

/** @brief Version du format de persistance EEPROM */
#define WIFIMANAGER_STORAGE_VERSION 1

// ===========================================
// STRUCTURES DE DONNÉES
// ===========================================

/**
 * @brief Structure représentant un réseau WiFi configuré
 */
struct WiFiNetwork {
    char ssid[33];              ///< Nom du réseau (copie possédée)
    char password[64];          ///< Mot de passe (copie possédée)
    int priority;               ///< Priorité (0 = plus haute)
    unsigned int failCount;     ///< Nombre d'échecs consécutifs
    unsigned long lastFail;     ///< Timestamp du dernier échec (ms)
    unsigned long lastAttempt;  ///< Timestamp de la dernière tentative (ms)
    bool configured;            ///< true si ce slot est utilisé

    // --- classement dynamique (local) : score composite 0..1 ---
    float         connEff;      ///< Déconnexions avec oubli exponentiel (N_eff)
    unsigned long lastDecay;    ///< Timestamp du dernier oubli de connEff (ms)
    int8_t        rssiHist[5];  ///< Historique RSSI (scan) pour l'écart-type
    uint8_t       histCount;    ///< Nb d'échantillons RSSI valides
    uint8_t       histIdx;      ///< Index d'écriture dans rssiHist
    bool          linked;       ///< Connecté sur ce réseau
    unsigned long connSince;    ///< Timestamp de début de connexion (uptime, ms)
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
    uint8_t authMode;           ///< Mode d'authentification (wl_enc_type / wifi_auth_mode_t)
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

/**
 * @brief Structure représentant un résultat de scan WiFi (cache scanNetworksAsync)
 */
struct ScanResult {
    char ssid[33];              ///< Nom du réseau
    int8_t rssi;                ///< Force du signal en dBm
    uint8_t encryption;         ///< Type de chiffrement (wl_enc_type / wifi_auth_mode_t)
    int8_t channel;             ///< Canal radio
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
     * @brief Ajoute un réseau et lance immédiatement sa connexion (non-bloquant)
     * @param ssid Nom du réseau WiFi
     * @param password Mot de passe du réseau WiFi (défaut: vide)
     * @param priority Priorité du réseau (0 = plus haute, défaut: 0)
     * @return true si le réseau a été ajouté et la tentative lancée, false sinon
     * 
     * @note Provisionnement à chaud : utilisable au démarrage sans réseau configuré.
     *       La connexion progresse dans update() (à appeler dans loop()).
     *       Échoue (retourne false) si la liste est pleine ou le SSID invalide.
     */
    bool connectToNetwork(const char* ssid, const char* password = "", int priority = 0);

    /**
     * @brief Supprime tous les réseaux configurés
     */
    void clearNetworks();

    /**
     * @brief Supprime un réseau configuré par son index
     * @param index Index du réseau à supprimer
     * @return true si supprimé, false si index invalide
     */
    bool removeNetwork(int index);

    /**
     * @brief Vérifie si un réseau (par SSID) est déjà configuré
     * @param ssid Nom du réseau recherché
     * @return true si présent dans la liste
     */
    bool hasNetwork(const char* ssid) const;

    /**
     * @brief Retourne l'index d'un réseau configuré par son SSID
     * @param ssid Nom du réseau recherché
     * @return Index du réseau, ou -1 si absent
     */
    int indexOfNetwork(const char* ssid) const;

    /**
     * @brief Met à jour un réseau configuré existant
     * @param index Index du réseau à modifier
     * @param ssid Nouveau nom du réseau
     * @param password Nouveau mot de passe
     * @param priority Nouvelle priorité
     * @return true si modifié, false si index/SSID invalide
     * 
     * @note Re-trie la liste par priorité ; si le réseau modifié était le réseau
     *       courant, l'index courant est ajusté.
     */
    bool updateNetwork(int index, const char* ssid, const char* password, int priority = 0);

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

    /**
     * @brief Configure le timeout d'une tentative de connexion
     * @param ms Délai maximum par tentative (défaut: 15000)
     */
    void setConnectTimeout(uint32_t ms);

    /**
     * @brief Active le basculement automatique sur signal trop faible
     * @param dbm Seuil de RSSI (dBm, valeur négative). 0 = désactivé (défaut).
     * 
     * @note Si le RSSI descend sous ce seuil, le réseau courant est considéré
     *       comme défaillant et le failover est déclenché. Hystérésis de +5 dBm
     *       pour éviter les bascules répétées.
     */
    void setMinRSSI(int8_t dbm);

    // ===========================================
    // CLASSEMENT DYNAMIQUE DES RÉSEAUX (score composite 0..1)
    // ===========================================

    /**
     * @brief Active un classement dynamique basé sur un score composite pondéré.
     *
     * Score (0..1, 1 = idéal) = w_sig·c_sig + w_con·c_con + w_stab·c_stab + w_up·c_up
     *   c_sig  = (RSSI - rssiMin)/(rssiMax - rssiMin)            (borné 0..1)
     *   c_con  = 1/(1 + N_eff)   N_eff = déconnexions à oubli exponentiel (tau)
     *   c_stab = 1/(1 + ecartType(RSSI)/sigma0)                  (sur N derniers scans)
     *   c_up   = 0.5 si non connecté, sinon min(uptime/tauUp, 1)
     *
     * La sélection choisit le score maximal ; la bascule réelle n'a lieu que si
     * score(candidat) >= score(courant) + margin (anti-rebond). Un scan RSSI
     * asynchrone est lancé toutes les scanIntervalMs ; le failover est inhibé
     * pendant le scan (coupure radio ~2 s). Les scans non bloquants sont
     * silencieux côté serveur (résultat journalisé sur le port série).
     *
     * @param enable           active/désactive (défaut false)
     * @param rssiMin          RSSI pour c_sig=0 (dBm, ex. -85)
     * @param rssiMax          RSSI pour c_sig=1 (dBm, ex. -30)
     * @param forgetTauS       constante de temps d'oubli des déconnexions (s)
     * @param uptimeTauS       constante de temps d'ancienneté (s)
     * @param wSig/wCon/wStab/wUp poids des critères (somme = 1)
     * @param margin           marge de bascule (0..1, ex. 0.08)
     * @param scanIntervalMs   période de scan RSSI (ms)
     * @param stabSamples      nb d'échantillons RSSI pour l'écart-type (<=5)
     * @param sigma0           écart-type de référence (dB)
     */
    void setDynamicRanking(bool enable,
                           int8_t rssiMin = -85, int8_t rssiMax = -30,
                           uint32_t forgetTauS = 3600, uint32_t uptimeTauS = 3600,
                           float wSig = 0.50f, float wCon = 0.25f,
                           float wStab = 0.15f, float wUp = 0.10f,
                           float margin = 0.08f, uint32_t scanIntervalMs = 120000,
                           uint8_t stabSamples = 5, float sigma0 = 3.0f);

    /** @return Score composite (0..100) du réseau index, ou -1 si invalide. */
    int getNetworkScore(int index) const;

    /** @brief Callback d'autorisation du scan : le scan est reporté si la callback
     *  renvoie false (ex. une requête MCP est en cours → éviter de couper le WiFi). */
    void setScanGate(bool (*cb)());

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
    // CONFIGURATION IP / RADIO
    // ===========================================

    /**
     * @brief Configure une adresse IP statique (à appeler AVANT begin/beginAsync)
     * @param ip Adresse IP locale
     * @param gateway Adresse de la passerelle
     * @param subnet Masque de sous-réseau
     * @param dns1 DNS primaire (optionnel)
     * @param dns2 DNS secondaire (optionnel)
     * 
     * @note La configuration est réappliquée à chaque reset WiFi interne.
     */
    void setStaticIP(IPAddress ip, IPAddress gateway, IPAddress subnet,
                     IPAddress dns1 = (uint32_t)0, IPAddress dns2 = (uint32_t)0);

    /** @brief Repasse en mode DHCP (annule setStaticIP) */
    void useDHCP();

    /**
     * @brief Règle la puissance d'émission WiFi
     * @param dBm Puissance en dBm (ESP8266: valeur continue ; ESP32: palier le plus proche)
     * @return true si appliqué
     */
    bool setTxPower(float dBm);

    // ===========================================
    // PERSISTANCE (EEPROM CHIFFRÉE AES-128)
    // ===========================================

    /**
     * @brief Définit la clé utilisateur de chiffrement de la persistance
     * @param key Clé secrète (chaîne). La clé finale = SHA256(salt + chipId + key).
     * 
     * @note Sans clé utilisateur, la clé est dérivée du chip uniquement (plus faible).
     * @warning La clé est stockée dans le firmware : protection « au repos » uniquement.
     */
    void setStorageKey(const char* key);

    /**
     * @brief Sauvegarde les réseaux configurés dans l'EEPROM (chiffrés)
     * @return true si la sauvegarde a réussi
     */
    bool saveNetworks();

    /**
     * @brief Charge les réseaux depuis l'EEPROM (chiffrés) vers la liste
     * @return true si des réseaux ont été chargés
     * 
     * @note Ne remplace PAS la liste existante : ajoute les réseaux chargés.
     *       Utilisez clearNetworks() avant si nécessaire.
     */
    bool loadNetworks();

    /**
     * @brief Efface les réseaux persistés et réinitialise l'EEPROM
     * @return true si l'opération a réussi
     */
    bool factoryReset();

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

    /**
     * @brief Supprime un service mDNS annoncé
     * @param name Nom du service (ex: "http")
     * @param protocol Protocole (ex: "tcp")
     * @return true si le service a été trouvé et supprimé
     */
    bool removeMDNSService(const char* name, const char* protocol);

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

    /** @brief Résume l'état WiFi (connecté + IP, Gateway, DNS, MAC, hostname, SSID, RSSI) dans une String */
    String Etat_Wifi();

    /**
     * @brief Scanne les réseaux WiFi disponibles et retourne la liste formatée
     * @return String contenant la liste des réseaux (numéro, SSID, RSSI, chiffrement)
     * 
     * Effectue un scan bloquant, affiche les résultats sur Serial, et met à jour
     * le chiffrement du réseau actuellement connecté si celui-ci est trouvé.
     */
    String scanNetworks();

    /**
     * @brief Lance un scan asynchrone (non-bloquant)
     * @return true si le scan a été lancé
     * 
     * @note Résultats disponibles via getScanResultCount()/getScanResultSSID()...
     *       une fois le scan terminé (surveillé automatiquement par update()).
     *       Le nombre de résultats conservés est borné par WIFIMANAGER_MAX_SCAN_RESULTS.
     */
    bool scanNetworksAsync();

    /** @return true si un scan asynchrone est en cours */
    bool isScanRunning() const;

    /** @return Nombre de résultats disponibles dans le cache du dernier scan */
    int getScanResultCount() const;

    /** @return SSID du résultat de scan à l'index donné, ou chaîne vide si invalide */
    String getScanResultSSID(int index) const;

    /** @return RSSI (dBm) du résultat de scan à l'index donné */
    int8_t getScanResultRSSI(int index) const;

    /** @return Type de chiffrement du résultat de scan à l'index donné */
    uint8_t getScanResultEncryption(int index) const;

    /** @return Canal radio du résultat de scan à l'index donné */
    int8_t getScanResultChannel(int index) const;

    /**
     * @brief Recherche un SSID dans le cache du dernier scan
     * @param ssid SSID recherché
     * @param rssi (sortie, optionnel) RSSI trouvé
     * @param encryption (sortie, optionnel) chiffrement trouvé
     * @return Index du résultat, ou -1 si absent
     */
    int findNetworkInScan(const char* ssid, int8_t* rssi = nullptr, uint8_t* encryption = nullptr) const;

    // ===========================================
    // GESTION DE LA CONNEXION
    // ===========================================

    /** @brief Force une reconnexion au réseau WiFi actuel */
    void reconnect();

    /** @brief Déconnecte l'appareil du réseau WiFi */
    void disconnect();

    // ===========================================
    // HOOKS APPLICATION (v0.7.9)
    // ===========================================

    /**
     * @brief Callback appelé à CHAQUE connexion WiFi réussie (_onConnectSuccess).
     *
     * Utile pour ré-armer un stack dépendant du WiFi (ex. ESP-NOW : esv.rearm()
     * + esv.rearmPeers() après un reset WiFi qui a effacé le stack).
     */
    void onWiFiConnected(std::function<void()> cb) { _onConnectedCb = cb; }

    /** @brief Callback appelé à chaque échec de connexion (_onConnectFail). */
    void onWiFiDisconnected(std::function<void()> cb) { _onDisconnectedCb = cb; }

    /**
     * @brief Désactive (défaut true) le power-save WiFi après connexion.
     *
     * ESP32 : setSleep(false) (WIFI_PS_NONE) ; ESP8266 : WIFI_NONE_SLEEP.
     * Le power-save (modem sleep) fait dormir la radio → des trames ESP-NOW
     * entrantes sont manquées (~25-30 % de perte mesurée). À couper quand
     * ESP-NOW est utilisé.
     */
    void setPowerSaveOff(bool enable) { _powerSaveOff = enable; }

    /** @return Nombre de réinitialisations WiFi effectuées (_resetWiFi). */
    uint32_t getResetCount() const { return _resetCount; }

    /** @return Timestamp (ms) de la dernière réinitialisation WiFi. */
    unsigned long getLastResetTime() const { return _lastResetTime; }

    // ===========================================
    // GETTERS RÉSEAU
    // ===========================================

    /** @return Adresse IP locale en mode client (STA) */
    String getLocalIP();

    /** @return Adresse IP du point d'accès (AP) */
    String getAPIP();

    /** @return SSID du réseau auquel l'appareil est connecté */
    String getSSID();

    /** @return Force du signal WiFi en dBm (négatif, plus proche de 0 = meilleur), ou -127 si non connecté */
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

    /** @return Adresse MAC (BSSID) du point d'accès connecté, ou chaîne vide si non connecté */
    String getBSSID();

    /** @return Canal radio du réseau connecté, ou 0 si non connecté */
    int32_t getChannel();

    /** @return Masque de sous-réseau (ex: 255.255.255.0) */
    String getSubnetMask();

    /** @return Qualité du signal 0-100 % (basée sur le RSSI), ou -1 si non connecté */
    int getSignalQuality();

    /** @return Code de raison de la dernière déconnexion (voir wl_status_t / wifi_err_reason_t) */
    int getLastDisconnectReason();

    /** @return Description textuelle de la dernière raison de déconnexion */
    String getLastDisconnectReasonText();

    /** @return Nombre de clients actuellement connectés au point d'accès (0 si AP inactif) */
    int getAPClientCount();

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
    uint32_t _connectTimeout = 15000;   ///< Timeout d'une tentative de connexion (ms)
    int8_t _minRSSI = 0;                ///< Seuil de basculement sur signal faible (0 = désactivé)
    bool _rssiWeak = false;             ///< État d'hystérésis pour setMinRSSI

    // --- classement dynamique (local) ---
    bool     _dynRank = false;          ///< Active le score composite
    int8_t   _rankRssiMin = -85;        ///< RSSI -> c_sig=0
    int8_t   _rankRssiMax = -30;        ///< RSSI -> c_sig=1
    uint32_t _rankForgetTauS = 3600;    ///< Oubli des déconnexions (s)
    uint32_t _rankUptimeTauS = 3600;    ///< Ancienneté lien (s)
    float    _rankWSig = 0.50f, _rankWCon = 0.25f, _rankWStab = 0.15f, _rankWUp = 0.10f;
    float    _rankMargin = 0.08f;       ///< Marge anti-rebond (0..1)
    uint32_t _rankScanIntervalMs = 120000;
    uint8_t  _rankStabSamples = 5;      ///< Échantillons RSSI pour l'écart-type (<=5)
    float    _rankSigma0 = 3.0f;        ///< Écart-type de référence (dB)
    unsigned long _lastRankScan = 0;    ///< Dernier scan RSSI (ms)
    bool     _scanInhibitFailover = false; ///< Failover inhibé pendant un scan
    bool     _rankEvalPending = false;  ///< Évaluation du classement en attente (scan en cours)
    bool (*_scanGate)() = nullptr;      ///< Autorisation externe du scan (nullptr = toujours ok)

    // ===========================================
    // VARIABLES MEMBRES - IP STATIQUE
    // ===========================================

    bool _useStaticIP = false;          ///< Utilise une IP statique si vrai
    IPAddress _staticIP;                ///< IP statique
    IPAddress _staticGateway;           ///< Passerelle statique
    IPAddress _staticSubnet;            ///< Masque statique
    IPAddress _staticDns1;              ///< DNS primaire statique
    IPAddress _staticDns2;              ///< DNS secondaire statique

    // ===========================================
    // VARIABLES MEMBRES - SCAN
    // ===========================================

    ScanResult _scanResults[WIFIMANAGER_MAX_SCAN_RESULTS]; ///< Cache des résultats de scan
    int _scanResultCount = 0;           ///< Nombre de résultats valides
    bool _scanAsyncRunning = false;     ///< Scan asynchrone en cours

    // ===========================================
    // VARIABLES MEMBRES - PERSISTANCE
    // ===========================================

    char _storageKey[33] = {0};         ///< Clé utilisateur de chiffrement EEPROM

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
    uint8_t _currentAuthMode = 0xFF;                ///< Mode d'auth du réseau connecté (0xFF = inconnu)
    int _lastDisconnectReason = 0;                  ///< Code de la dernière déconnexion (callbacks)

    // ===========================================
    // v0.7.9 — hooks + power-save + compteur de resets
    // ===========================================
    std::function<void()> _onConnectedCb;           ///< Callback connexion réussie
    std::function<void()> _onDisconnectedCb;        ///< Callback échec connexion
    bool _powerSaveOff = true;                      ///< Coupe le power-save après connexion (ESP-NOW)
    uint32_t _resetCount = 0;                       ///< Nb de _resetWiFi() (diagnostic ESP-NOW)
    unsigned long _lastResetTime = 0;               ///< Timestamp du dernier _resetWiFi()

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

    // --- classement dynamique (local) ---
    float _scoreNetwork(int i) const;                 ///< score composite 0..1
    void  _decayConnEff(WiFiNetwork& net, unsigned long now); ///< oubli N_eff
    int   _bestByScore(bool excludeCurrent);          ///< argmax score

    /**
     * @brief Réinitialise le WiFi proprement (déconnexion + mode OFF + mode STA)
     */
    void _resetWiFi();

    /**
     * @brief Ajoute une entrée dans l'historique
     */
    void _addToHistory(const char* ssid, const char* status, const char* ip, int rssi, uint8_t authMode);

    /**
     * @brief Formate un temps en millisecondes en chaîne lisible
     */
    String _formatTime(unsigned long ms) const;

    /**
     * @brief Convertit un code de chiffrement (mode d'auth) en texte lisible
     * @param mode Code brut (wl_enc_type ESP8266 / wifi_auth_mode_t ESP32)
     * @return Texte descriptif (ex: "WPA2", "WPA3", "Ouvert", "Inconnu")
     */
    String _getEncryptionText(uint8_t mode) const;

    /**
     * @brief Rafraîchit le chiffrement du réseau actuellement connecté
     * ESP32: via esp_wifi_sta_get_ap_info() ; ESP8266: laissé inchangé (mis à jour par scanNetworks)
     */
    void _refreshCurrentAuthMode();

    // ===========================================
    // MÉTHODES PRIVÉES - SCAN / IP / CRYPTO
    // ===========================================

    /** @brief Copie les n premiers résultats du scan natif dans le cache interne */
    void _cacheScanResults(int n);

    /** @brief Applique la configuration IP (statique ou DHCP) au stack WiFi */
    void _applyStaticIP();

    /** @brief Dérive la clé AES 16 octets depuis salt + chipId + clé utilisateur */
    void _deriveStorageKey(uint8_t out[16]) const;

    /** @brief Retourne un entier aléatoire 32 bits (plateforme) */
    static uint32_t _random32();

    /** @brief Renvoie l'identifiant unique du chip (32 bits) */
    static uint32_t _chipId();

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

    /**
     * @brief Initialise le stack WiFi (callbacks, hostname, mode STA/AP)
     * @param enableAP Active le mode AP+STA
     * @note Idempotent : peut être appelé par begin()/beginAsync() ou connectToNetwork()
     */
    void _initWiFi(bool enableAP);

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
        _networks[i].ssid[0] = '\0';
        _networks[i].password[0] = '\0';
        _networks[i].priority = 99;
        _networks[i].failCount = 0;
        _networks[i].lastFail = 0;
        _networks[i].lastAttempt = 0;
        _networks[i].configured = false;
        _networks[i].connEff = 0.0f;
        _networks[i].lastDecay = 0;
        memset(_networks[i].rssiHist, 0, sizeof(_networks[i].rssiHist));
        _networks[i].histCount = 0;
        _networks[i].histIdx = 0;
        _networks[i].linked = false;
        _networks[i].connSince = 0;
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

    WiFiNetwork& net = _networks[_networkCount];
    strncpy(net.ssid, ssid, sizeof(net.ssid) - 1);
    net.ssid[sizeof(net.ssid) - 1] = '\0';
    const char* pass = password ? password : "";
    strncpy(net.password, pass, sizeof(net.password) - 1);
    net.password[sizeof(net.password) - 1] = '\0';
    net.priority = priority;
    net.failCount = 0;      // failCount
    net.lastFail = 0;       // lastFail
    net.lastAttempt = 0;    // lastAttempt
    net.configured = true;  // configured
    net.connEff = 0.0f;
    net.lastDecay = 0;
    memset(net.rssiHist, 0, sizeof(net.rssiHist));
    net.histCount = 0;
    net.histIdx = 0;
    net.linked = false;
    net.connSince = 0;
    _networkCount++;

    // Trier par priorité
    _sortNetworksByPriority();

    Serial.printf("[WiFiManagerESP] Réseau ajouté: %s (priorité=%d)\n", ssid, priority);
    return true;
}

bool WiFiManagerESP::connectToNetwork(const char* ssid, const char* password, int priority) {
    // Ajout du réseau (échoue si liste pleine ou SSID invalide)
    if (!addNetwork(ssid, password, priority)) {
        return false;
    }

    // Renseigner la pile WiFi si aucun begin()/beginAsync() n'a encore été appelé
    if (!_wifiInitialized) {
        _initWiFi(false);
    }

    // Retrouver l'index du réseau ajouté (le tri par priorité a pu le déplacer)
    int index = -1;
    for (int i = 0; i < _networkCount; i++) {
        if (_networks[i].configured && strcmp(_networks[i].ssid, ssid) == 0) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        Serial.println("[WiFiManagerESP] ERREUR: réseau ajouté introuvable");
        return false;
    }

    Serial.printf("[WiFiManagerESP] Provisionnement: connexion ciblée sur [%d] %s\n", index, ssid);
    return switchToNetwork(index);
}

void WiFiManagerESP::clearNetworks() {
    for (int i = 0; i < WIFIMANAGER_MAX_NETWORKS; i++) {
        _networks[i].ssid[0] = '\0';
        _networks[i].password[0] = '\0';
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

bool WiFiManagerESP::removeNetwork(int index) {
    if (index < 0 || index >= _networkCount || !_networks[index].configured) {
        Serial.printf("[WiFiManagerESP] ERREUR: index réseau invalide: %d\n", index);
        return false;
    }

    Serial.printf("[WiFiManagerESP] Suppression du réseau [%d] %s\n", index, _networks[index].ssid);

    // Décaler les entrées suivantes
    for (int i = index; i < _networkCount - 1; i++) {
        _networks[i] = _networks[i + 1];
    }

    // Vider le dernier slot
    int last = _networkCount - 1;
    _networks[last].ssid[0] = '\0';
    _networks[last].password[0] = '\0';
    _networks[last].priority = 99;
    _networks[last].failCount = 0;
    _networks[last].lastFail = 0;
    _networks[last].lastAttempt = 0;
    _networks[last].configured = false;
    _networkCount--;

    // Ajuster les index de référence
    if (_currentNetwork == index) {
        _currentNetwork = -1;
    } else if (_currentNetwork > index) {
        _currentNetwork--;
    }
    if (_lastConnectedNetwork == index) {
        _lastConnectedNetwork = -1;
    } else if (_lastConnectedNetwork > index) {
        _lastConnectedNetwork--;
    }
    if (_lastTriedNetwork == index) {
        _lastTriedNetwork = -1;
    } else if (_lastTriedNetwork > index) {
        _lastTriedNetwork--;
    }
    if (_connTargetIndex == index) {
        _connTargetIndex = -1;
    } else if (_connTargetIndex > index) {
        _connTargetIndex--;
    }

    return true;
}

bool WiFiManagerESP::hasNetwork(const char* ssid) const {
    return indexOfNetwork(ssid) >= 0;
}

int WiFiManagerESP::indexOfNetwork(const char* ssid) const {
    if (ssid == nullptr) return -1;
    for (int i = 0; i < _networkCount; i++) {
        if (_networks[i].configured && strcmp(_networks[i].ssid, ssid) == 0) {
            return i;
        }
    }
    return -1;
}

bool WiFiManagerESP::updateNetwork(int index, const char* ssid, const char* password, int priority) {
    if (index < 0 || index >= _networkCount || !_networks[index].configured) {
        Serial.printf("[WiFiManagerESP] ERREUR: index réseau invalide: %d\n", index);
        return false;
    }
    if (ssid == nullptr || strlen(ssid) == 0) {
        Serial.println("[WiFiManagerESP] ERREUR: SSID invalide");
        return false;
    }

    // Mémoriser les SSID référencés AVANT le tri (pour recalculer les index)
    char curSsid[33] = {0};
    bool hasCur = (_currentNetwork >= 0 && _currentNetwork < _networkCount);
    if (hasCur) strncpy(curSsid, _networks[_currentNetwork].ssid, sizeof(curSsid) - 1);
    char lastSsid[33] = {0};
    bool hasLast = (_lastConnectedNetwork >= 0 && _lastConnectedNetwork < _networkCount);
    if (hasLast) strncpy(lastSsid, _networks[_lastConnectedNetwork].ssid, sizeof(lastSsid) - 1);
    char targetSsid[33] = {0};
    bool hasTarget = (_connState == ConnectionState::CONN_CONNECTING &&
                      _connTargetIndex >= 0 && _connTargetIndex < _networkCount);
    if (hasTarget) strncpy(targetSsid, _networks[_connTargetIndex].ssid, sizeof(targetSsid) - 1);

    WiFiNetwork& net = _networks[index];
    strncpy(net.ssid, ssid, sizeof(net.ssid) - 1);
    net.ssid[sizeof(net.ssid) - 1] = '\0';
    const char* pass = password ? password : "";
    strncpy(net.password, pass, sizeof(net.password) - 1);
    net.password[sizeof(net.password) - 1] = '\0';
    net.priority = priority;

    _sortNetworksByPriority();

    // Recalculer les index (le tri a pu changer les positions)
    _currentNetwork = hasCur ? indexOfNetwork(curSsid) : -1;
    _lastConnectedNetwork = hasLast ? indexOfNetwork(lastSsid) : -1;
    if (hasTarget) _connTargetIndex = indexOfNetwork(targetSsid);

    Serial.printf("[WiFiManagerESP] Réseau mis à jour: %s (priorité=%d)\n", ssid, priority);
    return true;
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
        return _startConnection(nextNetwork, _connectTimeout);
    }

    // Si aucun réseau n'est disponible, réessayer le premier
    if (_networkCount > 0) {
        Serial.println("[WiFiManagerESP] Tous les réseaux en cooldown, réessai du premier");
        _networks[0].failCount = 0; // Reset le compteur
        return _startConnection(0, _connectTimeout);
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
    return _startConnection(index, _connectTimeout);
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

void WiFiManagerESP::setConnectTimeout(uint32_t ms) {
    _connectTimeout = ms;
    Serial.printf("[WiFiManagerESP] Timeout de connexion: %lu ms\n", (unsigned long)_connectTimeout);
}

void WiFiManagerESP::setMinRSSI(int8_t dbm) {
    _minRSSI = dbm;
    _rssiWeak = false;
    if (dbm == 0) {
        Serial.println("[WiFiManagerESP] Basculement sur signal faible: DÉSACTIVÉ");
    } else {
        Serial.printf("[WiFiManagerESP] Basculement si RSSI < %d dBm\n", dbm);
    }
}

// ===========================================
// IMPLÉMENTATION - MÉTHODES PRIVÉES MULTI-RÉSEAUX
// ===========================================

// ---------------------------------------------------------------------------
// CLASSEMENT DYNAMIQUE (score composite 0..1) — local
// ---------------------------------------------------------------------------
void WiFiManagerESP::setDynamicRanking(bool enable,
                                       int8_t rssiMin, int8_t rssiMax,
                                       uint32_t forgetTauS, uint32_t uptimeTauS,
                                       float wSig, float wCon, float wStab, float wUp,
                                       float margin, uint32_t scanIntervalMs,
                                       uint8_t stabSamples, float sigma0) {
    _dynRank = enable;
    _rankRssiMin = rssiMin;
    _rankRssiMax = rssiMax;
    _rankForgetTauS = forgetTauS ? forgetTauS : 3600;
    _rankUptimeTauS = uptimeTauS ? uptimeTauS : 3600;
    _rankWSig = wSig; _rankWCon = wCon; _rankWStab = wStab; _rankWUp = wUp;
    _rankMargin = margin;
    _rankScanIntervalMs = scanIntervalMs ? scanIntervalMs : 120000;
    _rankStabSamples = (stabSamples < 1) ? 1 : (stabSamples > 5 ? 5 : stabSamples);
    _rankSigma0 = (sigma0 > 0.0f) ? sigma0 : 3.0f;
    Serial.printf("[WiFiManagerESP] Classement dynamique: %s (sig[%d..%d dBm] "
                  "w=%.2f/%.2f/%.2f/%.2f marge=%.2f scan=%lus)\n",
                  enable ? "ACTIVÉ" : "désactivé", (int)rssiMin, (int)rssiMax,
                  wSig, wCon, wStab, wUp, margin, (unsigned long)(_rankScanIntervalMs / 1000));
}

// Oubli exponentiel du compteur de déconnexions.
void WiFiManagerESP::_decayConnEff(WiFiNetwork& net, unsigned long now) {
    if (net.lastDecay == 0) { net.lastDecay = now; return; }
    unsigned long dt = now - net.lastDecay;
    net.lastDecay = now;
    if (dt == 0 || net.connEff <= 0.0f) return;
    float k = expf(-(float)dt / (float)(_rankForgetTauS * 1000UL));
    net.connEff *= k;
    if (net.connEff < 0.001f) net.connEff = 0.0f;
}

float WiFiManagerESP::_scoreNetwork(int i) const {
    if (i < 0 || i >= _networkCount || !_networks[i].configured) return -1.0f;
    const WiFiNetwork& net = _networks[i];

    // RSSI : scan si vu, sinon RSSI réel si connecté, sinon plancher.
    int rssi = _rankRssiMin - 15; // plancher (ex. -100)
    bool found = false;
    int8_t scanRssi = 0;
    if (findNetworkInScan(net.ssid, &scanRssi, nullptr) >= 0) { rssi = scanRssi; found = true; }
    else if (net.linked) { rssi = WIFI_LIB.RSSI(); found = true; }

    // c_sig (linéaire borné)
    float cSig = ((float)(rssi - _rankRssiMin)) / (float)(_rankRssiMax - _rankRssiMin);
    if (cSig < 0.0f) cSig = 0.0f;
    if (cSig > 1.0f) cSig = 1.0f;
    if (!found) cSig = 0.0f;   // réseau non vu -> signal inexploitable

    // c_con = 1/(1+N_eff)
    float cCon = 1.0f / (1.0f + net.connEff);

    // c_stab = 1/(1 + sigma/sigma0)
    float cStab = 1.0f;
    if (net.histCount >= 2) {
        float mean = 0.0f;
        for (uint8_t k = 0; k < net.histCount; k++) mean += net.rssiHist[k];
        mean /= net.histCount;
        float var = 0.0f;
        for (uint8_t k = 0; k < net.histCount; k++) {
            float d = net.rssiHist[k] - mean; var += d * d;
        }
        var /= net.histCount;
        cStab = 1.0f / (1.0f + sqrtf(var) / _rankSigma0);
    }

    // c_up = 0.5 si non connecté, sinon min(uptime/tau,1)
    float cUp = 0.5f;
    if (net.linked) {
        unsigned long up = (millis() - net.connSince) / 1000UL;
        cUp = (float)up / (float)_rankUptimeTauS;
        if (cUp > 1.0f) cUp = 1.0f;
    }

    return _rankWSig * cSig + _rankWCon * cCon + _rankWStab * cStab + _rankWUp * cUp;
}

int WiFiManagerESP::getNetworkScore(int index) const {
    float s = _scoreNetwork(index);
    return (s < 0.0f) ? -1 : (int)lroundf(s * 100.0f);
}

void WiFiManagerESP::setScanGate(bool (*cb)()) { _scanGate = cb; }

int WiFiManagerESP::_bestByScore(bool excludeCurrent) {
    int best = -1;
    float bestScore = -1.0f;
    for (int i = 0; i < _networkCount; i++) {
        if (!_networks[i].configured) continue;
        if (excludeCurrent && i == _currentNetwork) continue;
        float s = _scoreNetwork(i);
        // départage : à score égal, la plus petite priorité gagne
        if (s > bestScore + 1e-4f ||
            (fabsf(s - bestScore) <= 1e-4f && best >= 0 &&
             _networks[i].priority < _networks[best].priority)) {
            bestScore = s;
            best = i;
        }
    }
    return best;
}

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
    if (index < 0 || index >= _networkCount) index = _currentNetwork;
    if (index < 0 || index >= _networkCount) {
        // Aucun réseau valide associé (ex. liste modifiée pendant la tentative)
        _connState = ConnectionState::CONN_CONNECTED;
        _wifiInitialized = true;
        _refreshCurrentAuthMode();
        updateStatus();
        if (_onConnectedCb) _onConnectedCb();
        return;
    }
    _connState = ConnectionState::CONN_CONNECTED;

    Serial.printf("[WiFiManagerESP] ✅ CONNECTÉ! IP=%s RSSI=%d dBm\n",
        WIFI_LIB.localIP().toString().c_str(), WIFI_LIB.RSSI());

    // v0.7.9 : couper le power-save WiFi (la radio dort → trames ESP-NOW manquées)
    if (_powerSaveOff) {
#if defined(ESP8266)
        WIFI_LIB.setSleepMode(WIFI_NONE_SLEEP);
#else
        WIFI_LIB.setSleep(false);
#endif
    }

    _networks[index].failCount = 0; // ← Reset SEULEMENT au succès
    _networks[index].linked = true;
    _networks[index].connSince = millis();
    if (_dynRank) _lastRankScan = 0;   // planifier un scan/évaluation après (re)connexion
    _lastConnectedNetwork = index;
    _wifiInitialized = true;

    // Rafraîchir le chiffrement du réseau connecté
    _refreshCurrentAuthMode();

    _addToHistory(_networks[index].ssid, "✅ Connecté", 
                  WIFI_LIB.localIP().toString().c_str(), WIFI_LIB.RSSI(), _currentAuthMode);

    // Démarrer mDNS automatiquement si activé
    if (_autoMDNS) {
        _startMDNSInternal();
    }

    updateStatus();

    // v0.7.9 : hook application (ex. rearm ESP-NOW) à CHAQUE connexion réussie
    if (_onConnectedCb) _onConnectedCb();
}

void WiFiManagerESP::_onConnectFail() {
    int index = _connTargetIndex;
    if (index < 0 || index >= _networkCount) index = _currentNetwork;
    wl_status_t finalStatus = WIFI_LIB.status();

    Serial.printf("[WiFiManagerESP] ❌ Échec (status=%d: %s)\n", 
                 (int)finalStatus, _getStatusText(finalStatus).c_str());

    if (index >= 0 && index < _networkCount) {
        _networks[index].failCount++;  // Incrémenté en cas d'échec
        _networks[index].lastFail = millis();
        if (_dynRank) { _decayConnEff(_networks[index], millis()); _networks[index].connEff += 1.0f; }
        _networks[index].linked = false;
        _addToHistory(_networks[index].ssid, "❌ Échec", "", 0, 0xFF);
    } else {
        _addToHistory("?", "❌ Échec", "", 0, 0xFF);
    }

    updateStatus();

    // Laisse update() déclencher le failover au prochain passage (état IDLE)
    _connState = ConnectionState::CONN_FAILED;

    // v0.7.9 : hook application (ex. couper les envois ESP-NOW)
    if (_onDisconnectedCb) _onDisconnectedCb();
}

// ===========================================
// IMPLÉMENTATION - _findBestNetwork()
// ===========================================

int WiFiManagerESP::_findBestNetwork() {
    if (_networkCount == 0) return -1;

    unsigned long now = millis();

    // Réinitialiser les compteurs dont le cooldown est écoulé
    for (int i = 0; i < _networkCount; i++) {
        if (_networks[i].failCount > 0 &&
            now - _networks[i].lastFail >= _failCooldown) {
            _networks[i].failCount = 0;
        }
    }

    // --- Mode classement dynamique : argmax du score composite ---
    if (_dynRank) {
        int best = -1;
        float bestScore = -2.0f;
        for (int i = 0; i < _networkCount; i++) {
            if (!_networks[i].configured) continue;
            if (i == _currentNetwork) continue;                 // ne pas se ré-élire soi-même
            if (_networks[i].failCount > 0 &&
                now - _networks[i].lastFail < _failCooldown) continue;  // en cooldown
            float s = _scoreNetwork(i);
            if (s > bestScore + 1e-4f) { bestScore = s; best = i; }
        }
        if (best >= 0) return best;
        // Tous hors-jeu : réessayer le réseau courant, sinon reset global
        if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
            _networks[_currentNetwork].failCount = 0;
            return _currentNetwork;
        }
        for (int i = 0; i < _networkCount; i++) _networks[i].failCount = 0;
        return 0;
    }

    // --- Mode classique : meilleure priorité ---
    int bestNetwork = -1;
    int bestPriority = 999;

    for (int i = 0; i < _networkCount; i++) {
        if (i == _currentNetwork) continue;
        if (_networks[i].failCount > 0 &&
            now - _networks[i].lastFail < _failCooldown) {
            continue; // Réseau en cooldown
        }
        if (_networks[i].priority < bestPriority) {
            bestPriority = _networks[i].priority;
            bestNetwork = i;
        }
    }

    if (bestNetwork >= 0) {
        return bestNetwork;
    }

    if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
        Serial.println("[WiFiManagerESP] Tous les réseaux en cooldown, reset du réseau actuel");
        _networks[_currentNetwork].failCount = 0;
        return _currentNetwork;
    }

    Serial.println("[WiFiManagerESP] Reset de tous les réseaux");
    for (int i = 0; i < _networkCount; i++) {
        _networks[i].failCount = 0;
    }
    return 0;
}

void WiFiManagerESP::_resetWiFi() {
    // v0.7.9 : diagnostic — chaque reset efface le stack ESP-NOW (esp_wifi_stop/deinit)
    _resetCount++;
    _lastResetTime = millis();
    Serial.printf("[WiFiManagerESP] 🔄 reset WiFi #%lu (%lu ms)\n", (unsigned long)_resetCount, _lastResetTime);
    WIFI_LIB.disconnect(true);
    NON_BLOCKING_DELAY(300);
    WIFI_LIB.mode(WIFI_OFF);
    NON_BLOCKING_DELAY(200);
    _configureHostname();
    WIFI_LIB.mode(WIFI_STA);
    NON_BLOCKING_DELAY(100);
    _applyStaticIP();
}

void WiFiManagerESP::_addToHistory(const char* ssid, const char* status, const char* ip, int rssi, uint8_t authMode) {
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
    _history[_historyIndex].authMode = authMode;
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

String WiFiManagerESP::_getEncryptionText(uint8_t mode) const {
#if defined(ESP8266)
    // ESP8266 renvoie des valeurs wl_enc_type (ENC_TYPE_*)
    switch (mode) {
        case ENC_TYPE_NONE:  return "Ouvert";
        case ENC_TYPE_WEP:   return "🔒 WEP";
        case ENC_TYPE_TKIP:  return "🔒 WPA (TKIP)";
        case ENC_TYPE_CCMP:  return "🔒 WPA2 (AES)";
        case ENC_TYPE_AUTO:  return "🔒 WPA/WPA2";
        default:             return "❓ Inconnu";
    }
#elif defined(ESP32)
    // ESP32 renvoie des valeurs wifi_auth_mode_t (WIFI_AUTH_*)
    switch (mode) {
        case WIFI_AUTH_OPEN:            return "Ouvert";
        case WIFI_AUTH_WEP:             return "🔒 WEP";
        case WIFI_AUTH_WPA_PSK:         return "🔒 WPA";
        case WIFI_AUTH_WPA2_PSK:        return "🔒 WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "🔒 WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "🔒 WPA2-Entreprise";
        case WIFI_AUTH_WPA3_PSK:        return "🔒 WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "🔒 WPA2/WPA3";
        case WIFI_AUTH_WAPI_PSK:        return "🔒 WAPI";
        case WIFI_AUTH_WPA3_ENT_192:    return "🔒 WPA3-Ent-192";
        default:                        return "❓ Inconnu";
    }
#else
    return "❓ Inconnu";
#endif
}

void WiFiManagerESP::_refreshCurrentAuthMode() {
#if defined(ESP32)
    // ESP32: lecture directe du mode d'auth du réseau connecté (sans scan)
    wifi_ap_record_t apInfo;
    if (esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK) {
        _currentAuthMode = static_cast<uint8_t>(apInfo.authmode);
    }
#elif defined(ESP8266)
    // ESP8266: pas d'API directe sans scan -> remis à "inconnu".
    // La valeur sera renseignée par scanNetworks() si le SSID courant est trouvé.
    _currentAuthMode = 0xFF;
#endif
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

bool WiFiManagerESP::removeMDNSService(const char* name, const char* protocol) {
    int idx = _findMDNSService(name, protocol);
    if (idx < 0) {
        Serial.printf("[WiFiManagerESP] ERREUR: Service mDNS %s.%s non trouvé\n",
                      name ? name : "?", protocol ? protocol : "?");
        return false;
    }

    // Décaler les services suivants et vider le dernier slot
    for (int i = idx; i < _mdnsServiceCount - 1; i++) {
        _mdnsServices[i] = _mdnsServices[i + 1];
    }
    memset(&_mdnsServices[_mdnsServiceCount - 1], 0, sizeof(MDNSService));
    _mdnsServices[_mdnsServiceCount - 1].configured = false;
    _mdnsServiceCount--;

    Serial.printf("[WiFiManagerESP] Service mDNS supprimé: %s.%s\n", name, protocol);

    // Ré-annoncer la liste restante (redémarrage mDNS propre et portable)
    if (_mdnsRunning) {
        _startMDNSInternal();
    }
    return true;
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
        Serial.println("Heure     | SSID                             | Statut         | IP              | RSSI | Chiffr.");
        Serial.println("----------+----------------------------------+----------------+-----------------+------+---------");
        for (int i = 0; i < _historyCount; i++) {
            const ConnectionHistoryEntry* entry = getHistoryEntry(i);
            if (entry) {
                String encText = _getEncryptionText(entry->authMode);
                Serial.printf("%-9s | %-32s | %-16s | %-15s | %4ddB | %s\n",
                    entry->timestamp, entry->ssid, entry->status, 
                    entry->ip, entry->rssi, encText.c_str());
            }
        }
    }
    Serial.println("=================================================================\n");
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

    _initWiFi(enableAP);

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

void WiFiManagerESP::_initWiFi(bool enableAP) {
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

    // Appliquer l'IP statique éventuelle avant la connexion
    _applyStaticIP();

    _wifiInitialized = true;
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
#if defined(ESP32)
    // Finalisation d'un scan asynchrone (polling natif ESP32)
    if (_scanAsyncRunning) {
        int16_t n = WIFI_LIB.scanComplete();
        if (n >= 0) {
            _cacheScanResults(n);
            _scanAsyncRunning = false;
            WIFI_LIB.scanDelete();
            Serial.printf("[WiFiManagerESP] Scan async terminé: %d réseau(x)\n", n);
        }
    }
#endif

    if (!_wifiInitialized || _networkCount == 0) return;

    wl_status_t previousStatus = _currentStatus;
    updateStatus();

    // Détection de perte de lien : soit détectée ici (previousStatus), soit déjà
    // rafraîchie par un callback WiFi (on compare aussi l'état machine CONNECTED).
    bool lostLink = (previousStatus == WL_CONNECTED && _currentStatus != WL_CONNECTED) ||
                    (_connState == ConnectionState::CONN_CONNECTED && _currentStatus != WL_CONNECTED);

    if (lostLink && !_scanInhibitFailover) {
        Serial.println("[WiFiManagerESP] ⚠️ Connexion perdue détectée!");
        if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
            if (_dynRank) { _decayConnEff(_networks[_currentNetwork], millis()); _networks[_currentNetwork].connEff += 1.0f; }
            _networks[_currentNetwork].linked = false;
            _addToHistory(_networks[_currentNetwork].ssid, "❌ Déconnecté", "", 0, 0xFF);
        }
        // Sortir de l'état CONNECTED pour permettre au failover de se déclencher
        if (_connState == ConnectionState::CONN_CONNECTED) {
            _connState = ConnectionState::CONN_IDLE;
        }
    }

    // Basculement sur signal trop faible (setMinRSSI), avec hystérésis +5 dBm
    if (_minRSSI != 0 && _currentStatus == WL_CONNECTED && _networkCount > 1 && !_scanInhibitFailover) {
        int8_t rssi = WIFI_LIB.RSSI();
        if (!_rssiWeak && rssi < _minRSSI) {
            _rssiWeak = true;
            Serial.printf("[WiFiManagerESP] ⚠️ Signal faible (%d dBm < %d) → basculement\n", rssi, _minRSSI);
            if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
                _networks[_currentNetwork].failCount = _maxRetries;
                _networks[_currentNetwork].lastFail = millis();
            }
            _connState = ConnectionState::CONN_IDLE;
            switchToNextNetwork();
        } else if (_rssiWeak && rssi > (int8_t)(_minRSSI + 5)) {
            _rssiWeak = false;  // Signal redevenu bon → réarmer la détection
        }
    }

    // Piloter la tentative de connexion en cours (machine à états)
    _handleConnection();

    // Après un échec, repasser en repos pour laisser le failover décider
    if (_connState == ConnectionState::CONN_FAILED) {
        _connState = ConnectionState::CONN_IDLE;
    }

    // ------------------------------------------------------------------
    // Classement dynamique : scan périodique + roaming proactif
    // ------------------------------------------------------------------
    if (_dynRank) {
        unsigned long now = millis();

        // Sécurité : si le scan async ne rend pas la main (callback perdu), on
        // débloque l'état après 20 s pour ne pas rester sans scan indéfiniment.
        if (_rankEvalPending && !_scanAsyncRunning &&
            (_lastRankScan == 0 || now - _lastRankScan > 20000)) {
            Serial.println("[WiFiManagerESP] Scan async: delai depasse, reinitialisation");
            _rankEvalPending = false;
            _scanInhibitFailover = false;
        }
        if (_scanInhibitFailover && now - _lastRankScan > 20000) {
            _scanInhibitFailover = false;
        }

        // Fin d'un scan asynchrone : évaluer le classement
        if (_rankEvalPending && !_scanAsyncRunning) {
            _rankEvalPending = false;
            _scanInhibitFailover = false;

            // Oubli progressif des déconnexions pour tous les réseaux
            for (int i = 0; i < _networkCount; i++) _decayConnEff(_networks[i], now);

            // Journal série des scores
            Serial.println("[WiFiManagerESP] --- Scores classement dynamique ---");
            for (int i = 0; i < _networkCount; i++)
                Serial.printf("  [%d] %-20s : %3d%%  (echecs %u, connEff %.2f)\n",
                              i, _networks[i].ssid, getNetworkScore(i),
                              _networks[i].failCount, _networks[i].connEff);

            // Roaming proactif : basculer si un candidat dépasse le courant + marge
            if (_currentStatus == WL_CONNECTED && _connState != ConnectionState::CONN_CONNECTING) {
                int cur = _currentNetwork;
                float sCur = (cur >= 0 && cur < _networkCount) ? _scoreNetwork(cur) : -1.0f;
                int best = _bestByScore(true);
                if (best >= 0) {
                    float sBest = _scoreNetwork(best);
                    bool cd = _networks[best].failCount > 0 &&
                              now - _networks[best].lastFail < _failCooldown;
                    if (!cd && sBest >= sCur + _rankMargin) {
                        Serial.printf("[WiFiManagerESP] 🔀 Roaming -> %s (score %d%% vs %d%%)\n",
                                      _networks[best].ssid, (int)(sBest * 100 + 0.5f),
                                      (int)(sCur * 100 + 0.5f));
                        if (cur >= 0 && cur < _networkCount) {
                            _networks[cur].failCount = _maxRetries;
                            _networks[cur].lastFail = now;
                            _networks[cur].linked = false;
                        }
                        _lastTriedNetwork = cur;
                        _startConnection(best, _connectTimeout);
                    }
                }
            }
        }

        // Lancer un scan périodique (si connecté, stable et aucun scan en cours)
        if (_currentStatus == WL_CONNECTED &&
            _connState == ConnectionState::CONN_CONNECTED &&
            !_scanAsyncRunning && !_rankEvalPending &&
            (_scanGate == nullptr || _scanGate()) &&
            (_lastRankScan == 0 || now - _lastRankScan >= _rankScanIntervalMs)) {
            _lastRankScan = now;
            if (scanNetworksAsync()) {
                _rankEvalPending = true;
                _scanInhibitFailover = true;   // ignorer la coupure radio du scan
            }
        }
    }

    if (_autoSwitch && _currentStatus != WL_CONNECTED && _connState == ConnectionState::CONN_IDLE && !_scanInhibitFailover) {
        unsigned long now = millis();

        if (now - _lastConnectionAttempt >= _retryDelay) {
            _lastConnectionAttempt = now;

            // Incrémenter le compteur d'échecs du réseau actuel
            if (_currentNetwork >= 0 && _currentNetwork < _networkCount) {
                _networks[_currentNetwork].failCount++;
                _networks[_currentNetwork].lastFail = now;
            }

            // Chercher le prochain réseau : par meilleur score (dynamique) ou
            // par rotation (mode classique, en évitant le dernier essayé).
            int nextNetwork = -1;
            if (_dynRank) {
                nextNetwork = _findBestNetwork();
            } else {
                for (int offset = 1; offset <= _networkCount; offset++) {
                    int candidate = (_currentNetwork + offset) % _networkCount;
                    if (candidate == _lastTriedNetwork && _networkCount > 1) continue;
                    if (_networks[candidate].failCount == 0 ||
                        now - _networks[candidate].lastFail >= _failCooldown) {
                        nextNetwork = candidate;
                        break;
                    }
                }
            }

            if (nextNetwork >= 0) {
                _lastTriedNetwork = _currentNetwork;  // Mémoriser le réseau qu'on quitte
                _startConnection(nextNetwork, _connectTimeout);
            } else {
                // Tous en cooldown, reset et recommencer
                Serial.println("[WiFiManagerESP] Tous les réseaux en cooldown, reset global");
                for (int i = 0; i < _networkCount; i++) {
                    _networks[i].failCount = 0;
                }
                _lastTriedNetwork = -1;  // Reset le dernier essayé
                Serial.println("[WiFiManagerESP] 🔄 Réessai d'un réseau...");
                _startConnection(0, _connectTimeout);
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
// IMPLÉMENTATION - IP STATIQUE / RADIO
// ===========================================

void WiFiManagerESP::setStaticIP(IPAddress ip, IPAddress gateway, IPAddress subnet,
                                 IPAddress dns1, IPAddress dns2) {
    _staticIP = ip;
    _staticGateway = gateway;
    _staticSubnet = subnet;
    _staticDns1 = dns1;
    _staticDns2 = dns2;
    _useStaticIP = true;
    Serial.printf("[WiFiManagerESP] IP statique: %s\n", ip.toString().c_str());
    if (_wifiInitialized) {
        _applyStaticIP();
    }
}

void WiFiManagerESP::useDHCP() {
    _useStaticIP = false;
    Serial.println("[WiFiManagerESP] Retour en DHCP");
#if defined(ESP8266)
    WIFI_LIB.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
#elif defined(ESP32)
    WIFI_LIB.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
#endif
    if (_wifiInitialized) {
        _applyStaticIP();
    }
}

void WiFiManagerESP::_applyStaticIP() {
    if (_useStaticIP) {
        WIFI_LIB.config(_staticIP, _staticGateway, _staticSubnet, _staticDns1, _staticDns2);
        Serial.printf("[WiFiManagerESP] Configuration IP statique appliquée: %s\n",
                      _staticIP.toString().c_str());
    }
}

bool WiFiManagerESP::setTxPower(float dBm) {
#if defined(ESP8266)
    WIFI_LIB.setOutputPower(dBm);
    Serial.printf("[WiFiManagerESP] Puissance TX: %.2f dBm\n", dBm);
    return true;
#elif defined(ESP32)
    // Choix du palier wifi_power_t le plus proche (table des valeurs supportées)
    static const struct { float dbm; wifi_power_t p; } table[] = {
        { 19.5f, WIFI_POWER_19_5dBm },
        { 19.0f, WIFI_POWER_19dBm },
        { 18.5f, WIFI_POWER_18_5dBm },
        { 17.0f, WIFI_POWER_17dBm },
        { 15.0f, WIFI_POWER_15dBm },
        { 13.0f, WIFI_POWER_13dBm },
        { 11.0f, WIFI_POWER_11dBm },
        {  8.5f, WIFI_POWER_8_5dBm },
        {  7.0f, WIFI_POWER_7dBm },
        {  5.0f, WIFI_POWER_5dBm },
        {  2.0f, WIFI_POWER_2dBm },
        { -1.0f, WIFI_POWER_MINUS_1dBm },
    };
    wifi_power_t chosen = WIFI_POWER_19_5dBm;
    float best = 1e9f;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        float d = fabsf(dBm - table[i].dbm);
        if (d < best) { best = d; chosen = table[i].p; }
    }
    bool ok = WIFI_LIB.setTxPower(chosen);
    Serial.printf("[WiFiManagerESP] Puissance TX: %.1f dBm (palier %d) -> %s\n",
                  dBm, (int)chosen, ok ? "OK" : "échec");
    return ok;
#endif
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

String WiFiManagerESP::scanNetworks() {
    Serial.println("\n=== SCAN RÉSEAUX WiFi ===");

    _scanResultCount = 0;
    int n = WIFI_LIB.scanNetworks();
    String result = "";

    if (n <= 0) {
        Serial.println("Aucun réseau trouvé");
        result = "Aucun réseau trouvé";
    } else {
        _cacheScanResults(n);
        Serial.printf("%d réseau(s) trouvé(s)\n", _scanResultCount);
        for (int i = 0; i < _scanResultCount; ++i) {
            String ssid = String(_scanResults[i].ssid);
            int32_t rssi = _scanResults[i].rssi;
            uint8_t encType = _scanResults[i].encryption;

            String encText = _getEncryptionText(encType);

            // Si ce SSID correspond au réseau actuellement connecté, mettre à jour le cache
            if (isConnected() && ssid.equals(getSSID())) {
                _currentAuthMode = encType;
            }

            String line = String(i + 1) + ": " + ssid + " (" + String(rssi) + " dBm) [" + encText + "]";
            Serial.println(line);
            result += line;
            if (i < _scanResultCount - 1) result += "\n";

            NON_BLOCKING_DELAY(10);
        }
    }

    WIFI_LIB.scanDelete();
    Serial.println("==========================\n");
    return result;
}

void WiFiManagerESP::_cacheScanResults(int n) {
    _scanResultCount = 0;
    if (n <= 0) return;

    int count = (n < WIFIMANAGER_MAX_SCAN_RESULTS) ? n : WIFIMANAGER_MAX_SCAN_RESULTS;
    for (int i = 0; i < count; i++) {
        String s = WIFI_LIB.SSID(i);
        strncpy(_scanResults[i].ssid, s.c_str(), sizeof(_scanResults[i].ssid) - 1);
        _scanResults[i].ssid[sizeof(_scanResults[i].ssid) - 1] = '\0';
        _scanResults[i].rssi = (int8_t)WIFI_LIB.RSSI(i);
        _scanResults[i].encryption = (uint8_t)WIFI_LIB.encryptionType(i);
        _scanResults[i].channel = (int8_t)WIFI_LIB.channel(i);
    }
    _scanResultCount = count;
}

bool WiFiManagerESP::scanNetworksAsync() {
    if (_scanAsyncRunning) {
        Serial.println("[WiFiManagerESP] Scan déjà en cours");
        return false;
    }
    _scanResultCount = 0;

#if defined(ESP8266)
    _scanAsyncRunning = true;
    WIFI_LIB.scanNetworksAsync([this](int n) {
        this->_cacheScanResults(n);
        this->_scanAsyncRunning = false;
        WIFI_LIB.scanDelete();
        Serial.printf("[WiFiManagerESP] Scan async terminé: %d réseau(x)\n", n);
    });
    Serial.println("[WiFiManagerESP] Scan asynchrone lancé");
    return true;
#elif defined(ESP32)
    int16_t r = WIFI_LIB.scanNetworks(true);
    if (r == WIFI_SCAN_FAILED) {
        Serial.println("[WiFiManagerESP] ERREUR: Échec du lancement du scan");
        _scanAsyncRunning = false;
        return false;
    }
    _scanAsyncRunning = true;
    Serial.println("[WiFiManagerESP] Scan asynchrone lancé");
    return true;
#endif
}

bool WiFiManagerESP::isScanRunning() const {
    return _scanAsyncRunning;
}

int WiFiManagerESP::getScanResultCount() const {
    return _scanResultCount;
}

String WiFiManagerESP::getScanResultSSID(int index) const {
    if (index < 0 || index >= _scanResultCount) return String("");
    return String(_scanResults[index].ssid);
}

int8_t WiFiManagerESP::getScanResultRSSI(int index) const {
    if (index < 0 || index >= _scanResultCount) return 0;
    return _scanResults[index].rssi;
}

uint8_t WiFiManagerESP::getScanResultEncryption(int index) const {
    if (index < 0 || index >= _scanResultCount) return 0;
    return _scanResults[index].encryption;
}

int8_t WiFiManagerESP::getScanResultChannel(int index) const {
    if (index < 0 || index >= _scanResultCount) return 0;
    return _scanResults[index].channel;
}

int WiFiManagerESP::findNetworkInScan(const char* ssid, int8_t* rssi, uint8_t* encryption) const {
    if (ssid == nullptr) return -1;
    for (int i = 0; i < _scanResultCount; i++) {
        if (strcmp(_scanResults[i].ssid, ssid) == 0) {
            if (rssi) *rssi = _scanResults[i].rssi;
            if (encryption) *encryption = _scanResults[i].encryption;
            return i;
        }
    }
    return -1;
}

String WiFiManagerESP::Etat_Wifi() {
    String s = "WiFi: ";
    if (isConnected()) {
        s += "✅ connecté";
        s += "\n🌐 IP: " + getLocalIP();
        s += "\n📡 Gateway: " + getGatewayIP();
        s += "\n🖧  DNS: " + getDnsIP();
        s += "\n🆔 MAC: " + getMacAddress();
        s += "\nHostname: " + getHostname();
        s += "\nSSID: " + getSSID();
        s += "\nRSSI: ";
        s += String(getRSSI());
        s += " dBm 📶";
        s += "\nChiffrement: " + _getEncryptionText(_currentAuthMode);
    } else {
        s += "❌ non connecté";
    }
    return s;
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
            Serial.print("🖧  DNS: ");
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
            Serial.print("Chiffrement: ");
            Serial.println(_getEncryptionText(_currentAuthMode));
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
        _startConnection(_currentNetwork, _connectTimeout);
    } else if (_networkCount > 0) {
        Serial.println("[WiFiManagerESP] Reconnexion au premier réseau...");
        _startConnection(0, _connectTimeout);
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
    if (!isConnected()) {
        return -127;
    }
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

String WiFiManagerESP::getBSSID() {
    if (!isConnected()) {
        return String("");
    }
    return WIFI_LIB.BSSIDstr();
}

int32_t WiFiManagerESP::getChannel() {
    if (!isConnected()) {
        return 0;
    }
    return WIFI_LIB.channel();
}

String WiFiManagerESP::getSubnetMask() {
    return WIFI_LIB.subnetMask().toString();
}

int WiFiManagerESP::getSignalQuality() {
    if (!isConnected()) {
        return -1;
    }
    int8_t rssi = WIFI_LIB.RSSI();
    // Mapping simple : -50 dBm ou mieux -> 100 %, -100 dBm ou pire -> 0 %
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    return (int)(2 * (rssi + 100));
}

int WiFiManagerESP::getLastDisconnectReason() {
    return _lastDisconnectReason;
}

String WiFiManagerESP::getLastDisconnectReasonText() {
#if defined(ESP32)
    return String(WIFI_LIB.disconnectReasonName((wifi_err_reason_t)_lastDisconnectReason));
#elif defined(ESP8266)
    switch (_lastDisconnectReason) {
        case 1:   return "UNSPECIFIED";
        case 2:   return "AUTH_EXPIRE";
        case 3:   return "AUTH_LEAVE";
        case 4:   return "ASSOC_EXPIRE";
        case 5:   return "ASSOC_TOOMANY";
        case 6:   return "NOT_AUTHED";
        case 7:   return "NOT_ASSOCED";
        case 8:   return "ASSOC_LEAVE";
        case 9:   return "ASSOC_NOT_AUTHED";
        case 15:  return "4WAY_HANDSHAKE_TIMEOUT";
        case 200: return "BEACON_TIMEOUT";
        case 201: return "NO_AP_FOUND";
        case 202: return "AUTH_FAIL";
        case 203: return "ASSOC_FAIL";
        case 204: return "HANDSHAKE_TIMEOUT";
        default:  return String("Code ") + String(_lastDisconnectReason);
    }
#endif
}

int WiFiManagerESP::getAPClientCount() {
    return WIFI_LIB.softAPgetStationNum();
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
        this->_lastDisconnectReason = (int)evt.reason;
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
            _lastDisconnectReason = (int)info.wifi_sta_disconnected.reason;
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

// ===========================================
// IMPLÉMENTATION - IDENTIFIANTS / ALÉATOIRE
// ===========================================

uint32_t WiFiManagerESP::_chipId() {
#if defined(ESP8266)
    return ESP.getChipId();
#elif defined(ESP32)
    return (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFULL);
#endif
}

uint32_t WiFiManagerESP::_random32() {
#if defined(ESP8266)
    return (uint32_t)RANDOM_REG32;
#elif defined(ESP32)
    return esp_random();
#endif
}

// ===========================================
// CRYPTO EMBARQUÉ (AES-128-CBC + SHA-256)
// ===========================================

static const uint8_t wm_aes_sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint8_t wm_aes_rsbox[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

static uint8_t wm_xtime(uint8_t x) { return (uint8_t)((x << 1) ^ ((x >> 7) * 0x1b)); }

static uint8_t wm_gmul(uint8_t x, uint8_t y) {
    uint8_t r = 0;
    while (y) {
        if (y & 1) r ^= x;
        x = wm_xtime(x);
        y >>= 1;
    }
    return r;
}

static void wm_aes128_expand(const uint8_t* key, uint8_t* rk) {
    static const uint8_t rcon[11] = {0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};
    memcpy(rk, key, 16);
    for (int i = 16; i < 176; i += 4) {
        uint8_t t[4] = { rk[i-4], rk[i-3], rk[i-2], rk[i-1] };
        if (i % 16 == 0) {
            uint8_t tmp = t[0];
            t[0] = wm_aes_sbox[t[1]] ^ rcon[i / 16];
            t[1] = wm_aes_sbox[t[2]];
            t[2] = wm_aes_sbox[t[3]];
            t[3] = wm_aes_sbox[tmp];
        }
        rk[i]   = rk[i-16] ^ t[0];
        rk[i+1] = rk[i-15] ^ t[1];
        rk[i+2] = rk[i-14] ^ t[2];
        rk[i+3] = rk[i-13] ^ t[3];
    }
}

static void wm_aes_add_rk(uint8_t* s, const uint8_t* rk) {
    for (int i = 0; i < 16; i++) s[i] ^= rk[i];
}

static void wm_aes_sub(uint8_t* s) {
    for (int i = 0; i < 16; i++) s[i] = wm_aes_sbox[s[i]];
}

static void wm_aes_inv_sub(uint8_t* s) {
    for (int i = 0; i < 16; i++) s[i] = wm_aes_rsbox[s[i]];
}

static void wm_aes_shift_rows(uint8_t* s) {
    uint8_t t;
    t = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t;
    t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
    t = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t;
}

static void wm_aes_inv_shift_rows(uint8_t* s) {
    uint8_t t;
    t = s[13]; s[13] = s[9]; s[9] = s[5]; s[5] = s[1]; s[1] = t;
    t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
    t = s[3]; s[3] = s[7]; s[7] = s[11]; s[11] = s[15]; s[15] = t;
}

static void wm_aes_mix_columns(uint8_t* s) {
    for (int c = 0; c < 4; c++) {
        uint8_t* col = &s[c * 4];
        uint8_t a = col[0], b = col[1], cc = col[2], d = col[3];
        uint8_t t = a ^ b ^ cc ^ d;
        col[0] ^= t ^ wm_xtime(a ^ b);
        col[1] ^= t ^ wm_xtime(b ^ cc);
        col[2] ^= t ^ wm_xtime(cc ^ d);
        col[3] ^= t ^ wm_xtime(d ^ a);
    }
}

static void wm_aes_inv_mix_columns(uint8_t* s) {
    for (int c = 0; c < 4; c++) {
        uint8_t* col = &s[c * 4];
        uint8_t a = col[0], b = col[1], cc = col[2], d = col[3];
        col[0] = wm_gmul(a,14) ^ wm_gmul(b,11) ^ wm_gmul(cc,13) ^ wm_gmul(d,9);
        col[1] = wm_gmul(a,9)  ^ wm_gmul(b,14) ^ wm_gmul(cc,11) ^ wm_gmul(d,13);
        col[2] = wm_gmul(a,13) ^ wm_gmul(b,9)  ^ wm_gmul(cc,14) ^ wm_gmul(d,11);
        col[3] = wm_gmul(a,11) ^ wm_gmul(b,13) ^ wm_gmul(cc,9)  ^ wm_gmul(d,14);
    }
}

static void wm_aes128_encrypt_block(const uint8_t* rk, const uint8_t* in, uint8_t* out) {
    uint8_t s[16];
    memcpy(s, in, 16);
    wm_aes_add_rk(s, rk);
    for (int r = 1; r < 10; r++) {
        wm_aes_sub(s);
        wm_aes_shift_rows(s);
        wm_aes_mix_columns(s);
        wm_aes_add_rk(s, rk + 16 * r);
    }
    wm_aes_sub(s);
    wm_aes_shift_rows(s);
    wm_aes_add_rk(s, rk + 160);
    memcpy(out, s, 16);
}

static void wm_aes128_decrypt_block(const uint8_t* rk, const uint8_t* in, uint8_t* out) {
    uint8_t s[16];
    memcpy(s, in, 16);
    wm_aes_add_rk(s, rk + 160);
    for (int r = 9; r >= 1; r--) {
        wm_aes_inv_shift_rows(s);
        wm_aes_inv_sub(s);
        wm_aes_add_rk(s, rk + 16 * r);
        wm_aes_inv_mix_columns(s);
    }
    wm_aes_inv_shift_rows(s);
    wm_aes_inv_sub(s);
    wm_aes_add_rk(s, rk);
    memcpy(out, s, 16);
}

static void wm_aes128_cbc_encrypt(const uint8_t* rk, const uint8_t iv_in[16],
                                  const uint8_t* data, size_t len, uint8_t* out) {
    uint8_t iv[16];
    memcpy(iv, iv_in, 16);
    for (size_t o = 0; o < len; o += 16) {
        uint8_t blk[16];
        for (int i = 0; i < 16; i++) blk[i] = data[o + i] ^ iv[i];
        wm_aes128_encrypt_block(rk, blk, out + o);
        memcpy(iv, out + o, 16);
    }
}

static void wm_aes128_cbc_decrypt(const uint8_t* rk, const uint8_t iv_in[16],
                                  const uint8_t* data, size_t len, uint8_t* out) {
    uint8_t iv[16];
    memcpy(iv, iv_in, 16);
    for (size_t o = 0; o < len; o += 16) {
        uint8_t blk[16];
        wm_aes128_decrypt_block(rk, data + o, blk);
        for (int i = 0; i < 16; i++) out[o + i] = blk[i] ^ iv[i];
        memcpy(iv, data + o, 16);
    }
}

static uint32_t wm_rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

static void wm_sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint64_t blen = (uint64_t)len * 8;

    size_t padlen = ((len + 1 + 8 + 63) / 64) * 64;
    uint8_t* msg = (uint8_t*)malloc(padlen);
    if (!msg) return;
    memcpy(msg, data, len);
    msg[len] = 0x80;
    for (size_t i = len + 1; i < padlen - 8; i++) msg[i] = 0;
    for (int i = 0; i < 8; i++) msg[padlen - 1 - i] = (uint8_t)(blen >> (8 * i));

    for (size_t off = 0; off < padlen; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)msg[off + i*4] << 24) | ((uint32_t)msg[off + i*4 + 1] << 16) |
                   ((uint32_t)msg[off + i*4 + 2] << 8) | (uint32_t)msg[off + i*4 + 3];
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = wm_rotr(w[i-15],7) ^ wm_rotr(w[i-15],18) ^ (w[i-15] >> 3);
            uint32_t s1 = wm_rotr(w[i-2],17) ^ wm_rotr(w[i-2],19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = wm_rotr(e,6) ^ wm_rotr(e,11) ^ wm_rotr(e,25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = wm_rotr(a,2) ^ wm_rotr(a,13) ^ wm_rotr(a,22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    free(msg);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(h[i] >> 24);
        out[i*4+1] = (uint8_t)(h[i] >> 16);
        out[i*4+2] = (uint8_t)(h[i] >> 8);
        out[i*4+3] = (uint8_t)(h[i]);
    }
}

// ===========================================
// IMPLÉMENTATION - PERSISTANCE (EEPROM CHIFFRÉE)
// ===========================================

void WiFiManagerESP::_deriveStorageKey(uint8_t out[16]) const {
    const char salt[16] = { 'W','M','E','S','-','p','e','r','s','i','s','t','-','v','1','!' };
    uint8_t buf[64];
    size_t n = 0;
    memcpy(buf, salt, 16); n = 16;
    uint32_t id = _chipId();
    buf[n++] = (uint8_t)(id);
    buf[n++] = (uint8_t)(id >> 8);
    buf[n++] = (uint8_t)(id >> 16);
    buf[n++] = (uint8_t)(id >> 24);
    size_t kl = strlen(_storageKey);
    if (kl > 32) kl = 32;
    memcpy(buf + n, _storageKey, kl); n += kl;

    uint8_t hash[32];
    wm_sha256(buf, n, hash);
    memcpy(out, hash, 16);
}

void WiFiManagerESP::setStorageKey(const char* key) {
    if (key != nullptr) {
        strncpy(_storageKey, key, sizeof(_storageKey) - 1);
        _storageKey[sizeof(_storageKey) - 1] = '\0';
        Serial.println("[WiFiManagerESP] Clé de persistance définie");
    } else {
        _storageKey[0] = '\0';
        Serial.println("[WiFiManagerESP] Clé de persistance effacée");
    }
}

bool WiFiManagerESP::saveNetworks() {
    uint8_t key[16];
    _deriveStorageKey(key);
    uint8_t rk[176];
    wm_aes128_expand(key, rk);

    uint8_t buf[WIFIMANAGER_EEPROM_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'W'; buf[1] = 'M'; buf[2] = 'E'; buf[3] = 'S';
    buf[4] = WIFIMANAGER_STORAGE_VERSION;

    size_t off = 8;
    int stored = 0;
    for (int i = 0; i < _networkCount; i++) {
        uint8_t iv[16];
        for (int k = 0; k < 4; k++) {
            uint32_t r = _random32();
            memcpy(iv + k * 4, &r, 4);
        }
        uint8_t ssidBuf[48];
        memset(ssidBuf, 0, sizeof(ssidBuf));
        strncpy((char*)ssidBuf, _networks[i].ssid, 32);
        uint8_t passBuf[64];
        memset(passBuf, 0, sizeof(passBuf));
        strncpy((char*)passBuf, _networks[i].password, 63);

        uint8_t ssidCt[48];
        uint8_t passCt[64];
        wm_aes128_cbc_encrypt(rk, iv, ssidBuf, 48, ssidCt);
        wm_aes128_cbc_encrypt(rk, iv, passBuf, 64, passCt);

        if (off + 48 + 64 + 16 + 1 > sizeof(buf)) {
            Serial.println("[WiFiManagerESP] ⚠️ EEPROM pleine : réseaux tronqués");
            break;
        }
        memcpy(buf + off, ssidCt, 48); off += 48;
        memcpy(buf + off, passCt, 64); off += 64;
        memcpy(buf + off, iv, 16); off += 16;
        buf[off++] = (uint8_t)(_networks[i].priority & 0xff);
        stored++;
    }
    buf[5] = (uint8_t)stored;

    EEPROM.begin(WIFIMANAGER_EEPROM_SIZE);
    for (size_t i = 0; i < sizeof(buf); i++) EEPROM.write((int)i, buf[i]);
    bool ok = EEPROM.commit();
    EEPROM.end();

    Serial.printf("[WiFiManagerESP] %d réseau(x) sauvegardé(s) en EEPROM (%s)\n",
                  stored, ok ? "OK" : "échec");
    return ok;
}

bool WiFiManagerESP::loadNetworks() {
    EEPROM.begin(WIFIMANAGER_EEPROM_SIZE);
    bool valid = (EEPROM.read(0) == 'W' && EEPROM.read(1) == 'M' &&
                  EEPROM.read(2) == 'E' && EEPROM.read(3) == 'S' &&
                  EEPROM.read(4) == WIFIMANAGER_STORAGE_VERSION);
    uint8_t count = EEPROM.read(5);

    if (!valid) {
        EEPROM.end();
        Serial.println("[WiFiManagerESP] Aucune donnée de persistance valide");
        return false;
    }

    uint8_t key[16];
    _deriveStorageKey(key);
    uint8_t rk[176];
    wm_aes128_expand(key, rk);

    size_t off = 8;
    int loaded = 0;
    int maxLoad = (count < WIFIMANAGER_MAX_NETWORKS) ? count : WIFIMANAGER_MAX_NETWORKS;

    for (int i = 0; i < maxLoad; i++) {
        uint8_t ssidCt[48], passCt[64], iv[16];
        for (int k = 0; k < 48; k++) { ssidCt[k] = EEPROM.read((int)(off + k)); }
        off += 48;
        for (int k = 0; k < 64; k++) { passCt[k] = EEPROM.read((int)(off + k)); }
        off += 64;
        for (int k = 0; k < 16; k++) { iv[k] = EEPROM.read((int)(off + k)); }
        off += 16;
        uint8_t prio = EEPROM.read((int)off);
        off += 1;

        uint8_t ssidBuf[48], passBuf[64];
        wm_aes128_cbc_decrypt(rk, iv, ssidCt, 48, ssidBuf);
        wm_aes128_cbc_decrypt(rk, iv, passCt, 64, passBuf);
        ssidBuf[47] = '\0';
        passBuf[63] = '\0';

        if (addNetwork((const char*)ssidBuf, (const char*)passBuf, (int)prio)) loaded++;
    }

    EEPROM.end();
    Serial.printf("[WiFiManagerESP] %d réseau(x) chargé(s) depuis l'EEPROM\n", loaded);
    return loaded > 0;
}

bool WiFiManagerESP::factoryReset() {
    clearNetworks();
    EEPROM.begin(WIFIMANAGER_EEPROM_SIZE);
    for (size_t i = 0; i < WIFIMANAGER_EEPROM_SIZE; i++) EEPROM.write((int)i, 0);
    bool ok = EEPROM.commit();
    EEPROM.end();
    WIFI_LIB.disconnect(true);
    _connState = ConnectionState::CONN_IDLE;
    _connTargetIndex = -1;
    _currentAuthMode = 0xFF;
    _lastTriedNetwork = -1;
    Serial.println("[WiFiManagerESP] Factory reset: réseaux et EEPROM effacés");
    return ok;
}

#endif // WIFIMANAGER_ESP_H
