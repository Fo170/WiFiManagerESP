// ===========================================
// Exemple d'utilisation rapide - main.cpp
// ===========================================
#include <Arduino.h>

#include "WiFiManagerESP.h"

WiFiManagerESP wifiManager;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Démarrage du système...");

  // Méthode 1: Configuration puis initialisation (legacy mono-réseau)
  wifiManager.setCredentials("Votre_SSID", "Votre_Mot_De_Passe");
  wifiManager.setAPCredentials("ESP_AP", "password123");
  wifiManager.begin(true, 10000); // true = activer AP, 10s timeout

  // Méthode 2: Tout en un (legacy mono-réseau)
  // wifiManager.begin("Votre_SSID", "Votre_Mot_De_Passe", true, 10000);

  // Méthode 3: Multi-réseaux avec failover (nouveau)
  // wifiManager.addNetwork("Reseau_1", "mdp1", 0);
  // wifiManager.addNetwork("Reseau_2", "mdp2", 1);
  // wifiManager.setAutoSwitch(true);
  // wifiManager.begin(false, 15000);

  wifiManager.printStatus(true);
}

void loop() {
  // update() gère automatiquement:
  // - La reconnexion si déconnexion
  // - Le basculement entre réseaux (si multi-réseaux configuré)
  // - L'historique des connexions
  wifiManager.update();

  // Afficher le statut toutes les 5 secondes
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    wifiManager.printStatus(false);
  }

  // Votre code applicatif ici...
  delay(100);
}
