#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SSD1306Wire.h"

// Netzwerk-Daten
const char* ssid = "VOSTNET";
const char* password = "B6DeLKdcAWvRmjB";

SSD1306Wire display(0x3c, SDA, SCL);

// Variablen für Preise und Logik
float priceEur = 0;
float oldPriceEur = 0; // Speichert den vorherigen Wert
float priceUsd = 0;
float percentChange = 0;
unsigned long lastUpdate = 0;
bool showEur = true;


void setup() {
  Serial.begin(115200);
  display.init();
  display.flipScreenVertically();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Initialisiere WiFi...");
  display.display();
 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
}

void updatePrices() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  // Fordert EUR und USD gleichzeitig an
  String url = "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=USD,EUR";
  
  if (http.begin(client, url)) {
    http.addHeader("Accept-Encoding", "identity");
    http.addHeader("User-Agent", "ESP32-Bitcoin-Ticker");
    
    if (http.GET() == 200) {
      StaticJsonDocument<256> doc;
      deserializeJson(doc, http.getString());
      
      if (priceEur > 0) {
        oldPriceEur = priceEur;
        priceEur = doc["EUR"];
        // Prozentformel: ((Neu - Alt) / Alt) * 100
        percentChange = ((priceEur - oldPriceEur) / oldPriceEur) * 100;
      } else {
        priceEur = doc["EUR"]; // Erster Lauf
      }
      priceUsd = doc["USD"];
      lastUpdate = millis();
    }
    http.end();
  }
}

void loop() {
  // Update alle 30 Sekunden
  if (millis() - lastUpdate > 30000 || lastUpdate == 0) {
    updatePrices();
  }

  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Bitcoin Live:");
  
  display.setFont(ArialMT_Plain_24);
  if (showEur) {
    display.drawString(0, 18, String(priceEur, 2) + " €");
  } else {
    display.drawString(0, 18, "$ " + String(priceUsd, 2));
  }

  // Tendenz und Prozentanzeige
  display.setFont(ArialMT_Plain_10);
  String trend = (percentChange >= 0) ? "+" : "";
  trend += String(percentChange, 4) + "%"; 
  
  display.drawString(0, 48, "Änderung: " + trend);
  display.display();

  showEur = !showEur;
  delay(5000); 
}
