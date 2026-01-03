// ===========================================
// Exemple d'utilisation - main.cpp
// ===========================================
#include <Arduino.h>

#include "WiFiManagerESP.h"

WiFiManagerESP wifiManager;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Démarrage du système...");
  
  // Méthode 1: Configuration puis initialisation
  wifiManager.setCredentials("Votre_SSID", "Votre_Mot_De_Passe");
  wifiManager.setAPCredentials("ESP_AP", "password123");
  wifiManager.begin(true, 10000); // true = activer AP, 10s timeout
  
  // Méthode 2: Tout en un
  // wifiManager.begin("Votre_SSID", "Votre_Mot_De_Passe", true, 10000);
  
  wifiManager.printStatus(true);
}

void loop() {
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    
    wifiManager.updateStatus();
    wifiManager.printStatus(false);
    
    static unsigned long disconnectTime = 0;
    if (wifiManager.isConnected()) {
      disconnectTime = 0;
    } else if (disconnectTime == 0) {
      disconnectTime = millis();
    } else if (millis() - disconnectTime > 30000) {
      wifiManager.reconnect();
      disconnectTime = millis();
    }
  }
  
  // Votre code applicatif ici...
  delay(100);
}
