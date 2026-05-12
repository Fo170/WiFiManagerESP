#include <Arduino.h>
#include "WiFiManagerESP.h"

WiFiManagerESP wifiManager;

void setup() {
  Serial.begin(115200);

  // Mode legacy: un seul réseau
  // Le mDNS démarre automatiquement après connexion (setAutoMDNS true par défaut)
  wifiManager.setHostname("mon-esp");  // Définit aussi le nom mDNS
  wifiManager.begin("Votre_SSID", "Votre_Mot_De_Passe");

  if (wifiManager.isConnected()) {
    Serial.println("Connecté avec succès !");
    wifiManager.printStatus(true);
  }
}

void loop() {
  // update() gère:
  // - La reconnexion si déconnexion
  // - MDNS.update() sur ESP8266 (nécessaire pour le responder mDNS)
  wifiManager.update();
  delay(100);
}
