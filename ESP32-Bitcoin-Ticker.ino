#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SSD1306Wire.h"

// Netzwerk-Daten
const char* ssid = "MEIN_WLAN";
const char* password = "MEIN_PASSWORT";

SSD1306Wire display(0x3c, SDA, SCL);

// Variablen für Preise und Mempool
float priceEur = 0, oldPriceEur = 0, priceUsd = 0, percentChange = 0;
int fastestFee = 0, halfHourFee = 0, hourFee = 0, blockHeight = 0;
unsigned long lastUpdate = 0;
int displayMode = 0; // 0=EUR, 1=USD, 2=Mempool


void setup() {
  Serial.begin(115200);
  display.init();
  display.flipScreenVertically();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
}

void updateData() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
   
  // 1. Preise abrufen (CryptoCompare)
  if (http.begin(client, "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=USD,EUR")) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    if (http.GET() == 200) {
      StaticJsonDocument<512> doc;
      deserializeJson(doc, http.getString());
      if (priceEur > 0) {
        oldPriceEur = priceEur;
        priceEur = doc["EUR"];
        percentChange = ((priceEur - oldPriceEur) / oldPriceEur) * 100;
      } else { priceEur = doc["EUR"]; }
      priceUsd = doc["USD"];
    }
    http.end();
  }

  delay(500); // Kurze Pause für den Speicher

  // 2. Mempool Gebühren
  if (http.begin(client, "https://mempool.space/api/v1/fees/recommended")) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    if (http.GET() == 200) {
      StaticJsonDocument<512> doc;
      deserializeJson(doc, http.getString());
      fastestFee = doc["fastestFee"];
      halfHourFee = doc["halfHourFee"];
      hourFee = doc["hourFee"];
    }
    http.end();
  }

  delay(500); // Nochmals kurz warten

  // 3. Blockhöhe (Erhöhte Robustheit)
  if (http.begin(client, "https://mempool.space/api/blocks/tip/height")) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      payload.trim(); // Entfernt unsichtbare Leerzeichen/Zeilenumbrüche
      if (payload.length() > 0) {
        blockHeight = payload.toInt();
      }
    }
    http.end();
  }
  lastUpdate = millis();
}

void loop() {
  if (millis() - lastUpdate > 30000 || lastUpdate == 0) { updateData(); }

  display.clear();
  
  if (displayMode == 0 || displayMode == 1) {
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Bitcoin Live:");
    display.setFont(ArialMT_Plain_24);
    if (displayMode == 0) {
      // Preis ohne Nachkommastellen für mehr Platz
      display.drawString(0, 18, String(priceEur, 0) + " EUR");
    } else {
      display.drawString(0, 18, "$ " + String(priceUsd, 2));
    }
    display.setFont(ArialMT_Plain_10);
    String trend = (percentChange >= 0) ? "+ " : "";
    trend += String(percentChange, 4) + "% " + (percentChange >= 0 ? "^" : "v");
    display.drawString(0, 48, "Chg: " + trend);
  } 
  else {
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Mempool/Block:");
    
    display.setFont(ArialMT_Plain_10);
    // Wir rücken die Zeilen enger zusammen (17, 28, 39, 50)
    display.drawString(0, 17, "Block: " + (blockHeight > 0 ? String(blockHeight) : "Lade..."));
    display.drawString(0, 28, "Fast: " + String(fastestFee) + " sat/vB");
    display.drawString(0, 39, "Med:  " + String(halfHourFee) + " sat/vB");
    display.drawString(0, 50, "Slow: " + String(hourFee) + " sat/vB");
  }

  display.display();
  displayMode = (displayMode + 1) % 3;
  delay(5000); 
}
