#include <Arduino.h>
#include "WiFiManagerESP.h"

WiFiManagerESP wifiManager;

void setup() {
  Serial.begin(115200);

  // Mode legacy: un seul réseau
  // Le mDNS démarre automatiquement après connexion (setAutoMDNS true par défaut)
  wifiManager.setHostname("mon-esp");  // Définit aussi le nom mDNS

  // Reconnexion native du stack WiFi (défaut activé): absorbe les coupures brèves.
  // wifiManager.setAutoReconnect(false);  // à désactiver pour un failover immédiat

  wifiManager.begin("Votre_SSID", "Votre_Mot_De_Passe");

  if (wifiManager.isConnected()) {
    Serial.println("Connecté avec succès !");
    wifiManager.printStatus(true);
  }
}

void loop() {
  // update() gère:
  // - La connexion (machine à états non-bloquante, jamais de freeze)
  // - La reconnexion si déconnexion
  // - MDNS.update() sur ESP8266 (nécessaire pour le responder mDNS)
  wifiManager.update();

  // isConnected() retourne true si le lien WiFi est actif.
  // Ex: digitalWrite(LED, !wifiManager.isConnected()) pour allumer une LED quand connecté.

  NON_BLOCKING_DELAY(100);
}
