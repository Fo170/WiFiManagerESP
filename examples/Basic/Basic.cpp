#include <Arduino.h>
#include "WiFiManagerESP.h"

WiFiManagerESP wifiManager;

void setup() {
  Serial.begin(115200);

  // Mode legacy: un seul réseau
  wifiManager.begin("Votre_SSID", "Votre_Mot_De_Passe");

  if (wifiManager.isConnected()) {
    Serial.println("Connecté avec succès !");
    wifiManager.printStatus(true);
  }
}

void loop() {
  // update() remplace updateStatus() + reconnexion manuelle
  // Gère automatiquement la reconnexion si déconnexion
  wifiManager.update();
  delay(100);
}
