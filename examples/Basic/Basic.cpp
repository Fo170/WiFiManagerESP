#include <Arduino.h>
#include "WiFiManagerESP.h"

WiFiManagerESP wifiManager;

void setup() {
  Serial.begin(115200);
  
  wifiManager.begin("Votre_SSID", "Votre_Mot_De_Passe");
  
  if (wifiManager.isConnected()) {
    Serial.println("Connecté avec succès !");
    wifiManager.printStatus(true);
  }
}

void loop() {
  wifiManager.updateStatus();
  delay(5000);
  
  if (!wifiManager.isConnected()) {
    Serial.println("Déconnecté, tentative de reconnexion...");
    wifiManager.reconnect();
  }
}
