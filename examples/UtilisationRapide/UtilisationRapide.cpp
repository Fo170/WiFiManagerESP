// ===========================================
// Exemple d'utilisation rapide - main.cpp
// Multi-Réseaux + mDNS + Scan + Diagnostics + Persistance (v0.9.0)
// ===========================================
#include <Arduino.h>

#include "WiFiManagerESP.h"

WiFiManagerESP wifiManager;

void setup() {
  Serial.begin(115200);
  NON_BLOCKING_DELAY(1000);

  Serial.println("Démarrage du système...");

  // --- Méthode 1: Configuration puis initialisation (legacy mono-réseau) ---
  wifiManager.setCredentials("Votre_SSID", "Votre_Mot_De_Passe");
  wifiManager.setAPCredentials("ESP_AP", "password123");
  wifiManager.setHostname("mon-esp");  // Hostname WiFi + nom mDNS
  // Reconnexion native du stack WiFi (défaut activé): absorbe les coupures brèves
  wifiManager.setAutoReconnect(true);
  wifiManager.begin(true, 10000);      // true = activer AP, 10s timeout

  // --- Méthode 2: Tout en un (legacy mono-réseau) ---
  // wifiManager.setHostname("mon-esp");
  // wifiManager.begin("Votre_SSID", "Votre_Mot_De_Passe", true, 10000);

  // --- Méthode 3: Multi-réseaux avec failover (v0.7.1) ---
  // wifiManager.addNetwork("Reseau_1", "mdp1", 0);
  // wifiManager.addNetwork("Reseau_2", "mdp2", 1);
  // wifiManager.setAutoSwitch(true);
  // wifiManager.setHostname("mon-esp");
  // wifiManager.begin(false, 15000);

  // --- Méthode 4: Multi-réseaux + mDNS + services (v0.7.2) ---
  // wifiManager.addNetwork("Reseau_1", "mdp1", 0);
  // wifiManager.addNetwork("Reseau_2", "mdp2", 1);
  // wifiManager.setAutoSwitch(true);
  // wifiManager.setHostname("mon-esp");
  // wifiManager.addMDNSService("http", "tcp", 80);   // Annonce service web
  // wifiManager.addMDNSService("telnet", "tcp", 23); // Annonce service telnet
  // wifiManager.begin(false, 15000);

  // --- Méthode 5: Non-bloquant, machine à états (v0.7.8, recommandé) ---
  // wifiManager.addNetwork("Reseau_1", "mdp1", 0);
  // wifiManager.addNetwork("Reseau_2", "mdp2", 1);
  // wifiManager.setHostname("mon-esp");
  // wifiManager.beginAsync(false, 15000);  // Lance la tentative et rend la main
  // if (wifiManager.waitForConnection(20000)) {
  //   Serial.println("Connecté !");
  // } else {
  //   Serial.println("Timeout : le failover continue dans loop()");
  // }

  // --- Méthode 6: Provisionnement à chaud, sans réseau configuré (v0.9.0) ---
  // wifiManager.scanNetworks();  // (bloquant) liste les réseaux disponibles
  // wifiManager.connectToNetwork("Reseau_Trouve", "motdepasse", 0);  // Ajout + connexion ciblée
  // wifiManager.removeNetwork(0);       // Retire un réseau de la liste
  // wifiManager.updateNetwork(0, "Nouveau_SSID", "nouveau_mdp", 0);  // Modifie un réseau

  // --- Méthode 7: Scan asynchrone + résultats (v0.9.0) ---
  // wifiManager.scanNetworksAsync();    // Non-bloquant, piloté par update()
  // ... puis dans loop() une fois isScanRunning() == false :
  // for (int i = 0; i < wifiManager.getScanResultCount(); i++) {
  //   Serial.printf("%s (%d dBm) canal %d\n", wifiManager.getScanResultSSID(i).c_str(),
  //                 wifiManager.getScanResultRSSI(i), wifiManager.getScanResultChannel(i));
  // }
  // int8_t rssi; uint8_t enc;
  // int idx = wifiManager.findNetworkInScan("Reseau_1", &rssi, &enc);

  // --- Méthode 8: Diagnostics (v0.9.0) ---
  // Serial.println(wifiManager.getBSSID());                 // MAC de l'AP connecté
  // Serial.println(wifiManager.getChannel());               // Canal radio
  // Serial.println(wifiManager.getSubnetMask());            // Masque de sous-réseau
  // Serial.println(wifiManager.getSignalQuality());         // 0-100 %
  // Serial.println(wifiManager.getLastDisconnectReasonText()); // Cause de la dernière déco
  // Serial.println(wifiManager.getAPClientCount());         // Clients sur l'AP

  // --- Méthode 9: IP statique + puissance TX (v0.9.0) ---
  // wifiManager.setStaticIP(IPAddress(192,168,1,50), IPAddress(192,168,1,1),
  //                         IPAddress(255,255,255,0));
  // wifiManager.useDHCP();                 // Revenir au DHCP
  // wifiManager.setTxPower(17.0f);         // Puissance d'émission en dBm

  // --- Méthode 10: Bascule sur signal faible (v0.9.0) ---
  // wifiManager.setMinRSSI(-80);           // Bascule si signal < -80 dBm (hystérésis +5)
  // wifiManager.setConnectTimeout(12000);  // Timeout par tentative (ms)

  // --- Méthode 11: Persistance EEPROM chiffrée (v0.9.0) ---
  // wifiManager.setStorageKey("ma_cle_secrete");  // Clé utilisateur (dérivée avec le chip)
  // wifiManager.saveNetworks();                   // Sauvegarde chiffrée (AES-128-CBC)
  // wifiManager.loadNetworks();                   // Recharge au démarrage
  // wifiManager.factoryReset();                   // Efface réseaux + EEPROM

  wifiManager.printStatus(true);
}

void loop() {
  // update() gère automatiquement:
  // - La reconnexion si déconnexion
  // - Le basculement entre réseaux (si multi-réseaux configuré)
  // - L'historique des connexions
  // - MDNS.update() sur ESP8266 (nécessaire pour le responder mDNS)
  wifiManager.update();

  // Afficher le statut toutes les 5 secondes
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    wifiManager.printStatus(false);
  }

  // Votre code applicatif ici...
  // Accès mDNS: http://mon-esp.local (si mDNS actif)
  NON_BLOCKING_DELAY(100);
}
