// ===========================================
// Exemple d'utilisation rapide - main.cpp
// v0.7.2 - Multi-Réseaux + mDNS
// ===========================================
#include <Arduino.h>

#include "WiFiManagerESP.h"

WiFiManagerESP wifiManager;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Démarrage du système...");

  // --- Méthode 1: Configuration puis initialisation (legacy mono-réseau) ---
  wifiManager.setCredentials("Votre_SSID", "Votre_Mot_De_Passe");
  wifiManager.setAPCredentials("ESP_AP", "password123");
  wifiManager.setHostname("mon-esp");  // Hostname WiFi + nom mDNS
  wifiManager.begin(true, 10000);      // true = activer AP, 10s timeout

  // --- Méthode 2: Tout en un (legacy mono-réseau) ---
  // wifiManager.setHostname("mon-esp");
  // wifiManager.begin("Votre_SSID", "Votre_Mot_De_Passe", true, 10000);

  // --- Méthode 3: Multi-réseaux avec failover (nouveau v0.7.1) ---
  // wifiManager.addNetwork("Reseau_1", "mdp1", 0);
  // wifiManager.addNetwork("Reseau_2", "mdp2", 1);
  // wifiManager.setAutoSwitch(true);
  // wifiManager.setHostname("mon-esp");
  // wifiManager.begin(false, 15000);

  // --- Méthode 4: Multi-réseaux + mDNS + services (nouveau v0.7.2) ---
  // wifiManager.addNetwork("Reseau_1", "mdp1", 0);
  // wifiManager.addNetwork("Reseau_2", "mdp2", 1);
  // wifiManager.setAutoSwitch(true);
  // wifiManager.setHostname("mon-esp");
  // wifiManager.addMDNSService("http", "tcp", 80);   // Annonce service web
  // wifiManager.addMDNSService("telnet", "tcp", 23); // Annonce service telnet
  // wifiManager.begin(false, 15000);

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
  delay(100);
}
