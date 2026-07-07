#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "time.h"
#include <WiFiManager.h> // NEU: WiFiManager für das Webinterface-Setup

SSD1306Wire display(0x3c, SDA, SCL);

// Variablen
float priceEur = 0, oldPriceEur = 0, priceUsd = 0, percentChange = 0;
int fastestFee = 0, halfHourFee = 0, hourFee = 0, blockHeight = 0;
unsigned long lastUpdate = 0;
int displayMode = 0; // 0=EUR, 1=USD, 2=Block, 3=Mempool, 4=CHART

// Chart-Speicher (128 Pixel Breite, wir nutzen alle 2 Pixel einen Wert = 64 Datenpunkte)
const int CHART_POINTS = 64;
float priceHistory[CHART_POINTS];
int historyCount = 0;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; 
const int   daylightOffset_sec = 3600;

void setup() {
  Serial.begin(115200);
  delay(1000); // Dem USB-Serial-Wandler Zeit zum Einschwingen geben
  Serial.println("\n--- WiFi-Manager Wroom Ticker startet ---");

  // 1. HARDWARE-BUTTON RESET LOGIK (GPIO 0 ist der BOOT-Button beim Wroom)
  const int BUTTON_PIN = 0; 
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Status-LED konfigurieren
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW); // Status-LED aus

  // Display initialisieren für sofortiges Feedback beim Booten
  display.init();
  display.flipScreenVertically();

  // Prüfen, ob der BOOT-Knopf direkt beim Starten gedrückt gehalten wird (LOW = gedrückt)
  if (digitalRead(BUTTON_PIN) == LOW) {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "RESET-MODUS AKTIV!");
    display.drawString(0, 16, "Knopf 3 Sek halten...");
    display.display();
    
    delay(3000); // 3 Sekunden Sicherheits-Wartezeit gegen versehentliches Drücken

    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("!!! WLAN-Schnittstelle manuell zurueckgesetzt !!!");
      display.drawString(0, 34, "WLAN geloescht!");
      display.drawString(0, 48, "AP startet neu...");
      display.display();
      delay(2000);
      
      WiFiManager wm;
      wm.resetSettings(); // Löscht die gespeicherten Router-Daten im NVS-Flash
    }
  }

  // 2. NORMALER STARTVORGANG
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Starte WiFi-Manager...");
  display.drawString(0, 15, "Pruefe gespeichertes WLAN");
  display.display();

  // Historie mit 0 initialisieren
  for (int i = 0; i < CHART_POINTS; i++) {
    priceHistory[i] = 0.0;
  }

  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Sendeleistung drosseln gegen Störungen/Brownouts

  WiFiManager wm;

  // Callback-Funktion: Zeigt Anleitung auf dem OLED, wenn kein WLAN bekannt ist
  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    Serial.println("Kein bekanntes WLAN! Geoeffneter AP aktiv.");
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "KEIN WLAN GEFUNDEN!");
    display.drawString(0, 16, "Handy-WLAN verbinden mit:");
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 28, "Wroom-BTC-Ticker-AP");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 48, "Browser: 192.168.4.1");
    display.display();
  });

  // Startet das Portal "Wroom-BTC-Ticker-AP", falls kein WLAN erreichbar ist
  if (!wm.autoConnect("Wroom-BTC-Ticker-AP")) {
    Serial.println("Verbindung fehlgeschlagen.");
    display.clear();
    display.drawString(0, 20, "Fehler! Neustart...");
    display.display();
    delay(3000);
    ESP.restart();
  }

  // Erfolgreich im Netzwerk angemeldet
  Serial.println("WLAN erfolgreich verbunden!");
  display.clear();
  display.drawString(0, 0, "WLAN OK!");
  display.display();
  delay(1000);

  // Uhrzeit via NTP holen und ersten API-Abruf starten
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  updateData();
}

String getLocalTimeStr() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "00:00";
  char timeStringBuff[10];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

void addPriceToHistory(float newPrice) {
  if (historyCount < CHART_POINTS) {
    priceHistory[historyCount] = newPrice;
    historyCount++;
  } else {
    for (int i = 0; i < CHART_POINTS - 1; i++) {
      priceHistory[i] = priceHistory[i + 1];
    }
    priceHistory[CHART_POINTS - 1] = newPrice;
  }
}

void drawChart() {
  if (historyCount < 2) {
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 25, "Sammle Chart-Daten...");
    return;
  }

  float minPrice = priceHistory[0];
  float maxPrice = priceHistory[0];
  for (int i = 1; i < historyCount; i++) {
    if (priceHistory[i] < minPrice) minPrice = priceHistory[i];
    if (priceHistory[i] > maxPrice) maxPrice = priceHistory[i];
  }

  float priceDelta = maxPrice - minPrice;
  if (priceDelta == 0) priceDelta = 2.0;

  maxPrice += priceDelta * 0.05;
  minPrice -= priceDelta * 0.05;
  priceDelta = maxPrice - minPrice;

  int chartHeight = 36;
  int chartOffsetIndexY = 52;

  for (int i = 0; i < historyCount - 1; i++) {
    int x1 = i * 2;
    int x2 = (i + 1) * 2;

    int y1 = chartOffsetIndexY - (int)((priceHistory[i] - minPrice) / priceDelta * chartHeight);
    int y2 = chartOffsetIndexY - (int)((priceHistory[i + 1] - minPrice) / priceDelta * chartHeight);

    if (y1 < 16) y1 = 16;
    if (y2 < 16) y2 = 16;

    display.drawLine(x1, y1, x2, y2);
  }
  
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 54, String((int)minPrice) + " EUR");
  display.drawString(80, 54, String((int)maxPrice) + " MAX");
}

void updateData() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  int httpCode;

  if (http.begin(client, "https://mempool.space/api/v1/prices")) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    httpCode = http.GET(); 
    
    if (httpCode == 200) { 
      JsonDocument doc; 
      deserializeJson(doc, http.getString());
      
      float currentEur = doc["EUR"].as<float>();
      priceUsd = doc["USD"].as<float>(); 
      
      if (currentEur > 0) {
        if (priceEur > 0) {
          if (currentEur != priceEur) { 
            oldPriceEur = priceEur;
            percentChange = ((currentEur - oldPriceEur) / oldPriceEur) * 100.0;
          }
        } else {
          percentChange = 0.00;
        }
        priceEur = currentEur; 
        addPriceToHistory(priceEur); 
      }
    }
    http.end();
  }
  delay(300);

  if (http.begin(client, "https://mempool.space/api/v1/fees/recommended")) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    httpCode = http.GET(); 
    if (httpCode == 200) {
      StaticJsonDocument<512> doc;
      deserializeJson(doc, http.getString());
      fastestFee = doc["fastestFee"];
      halfHourFee = doc["halfHourFee"];
      hourFee = doc["hourFee"];
    }
    http.end();
  }
  delay(300); 

  if (http.begin(client, "https://mempool.space/api/blocks/tip/height")) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    httpCode = http.GET(); 
    if (httpCode == 200) {
      String payload = http.getString();
      payload.trim(); 
      if (payload.length() > 0) blockHeight = payload.toInt();
    }
    http.end();
  }
  
  if (fastestFee > 0 && fastestFee <= 5) {
    digitalWrite(2, HIGH); 
  } else {
    digitalWrite(2, LOW); 
  }
  
  lastUpdate = millis();
}

void loop() {
  if (millis() - lastUpdate > 30000 || lastUpdate == 0) { 
    updateData(); 
  }

  display.clear();
  String currentTime = getLocalTimeStr();

  if (displayMode == 0 || displayMode == 1) { // PREISE
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Bitcoin Live " + currentTime);
    display.setFont(ArialMT_Plain_24);
    if (displayMode == 0) display.drawString(0, 18, String(priceEur, 0) + " EUR");
    else display.drawString(0, 18, "$ " + String(priceUsd, 2));
    display.setFont(ArialMT_Plain_10);
    String trend = (percentChange >= 0) ? "+ " : "";
    trend += String(percentChange, 4) + "% " + (percentChange >= 0 ? "^" : "v");
    display.drawString(0, 48, "Chg: " + trend);
  } 
  else if (displayMode == 2) { // BLOCKZEIT GROSS
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Current block:");
    display.setFont(ArialMT_Plain_24);
    display.drawString(0, 18, "#" + String(blockHeight));
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 52, "Update: " + currentTime);
  }
  else if (displayMode == 3) { // MEMPOOL
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Mempool Fees:");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 18, "Fast: " + String(fastestFee) + " sat/vB");
    display.drawString(0, 29, "Med:  " + String(halfHourFee) + " sat/vB");
    display.drawString(0, 40, "Slow: " + String(hourFee) + " sat/vB");
    display.drawString(0, 51, "Time: " + currentTime);
  }
  else if (displayMode == 4) { // LIVE CHART
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "BTC Trend 30m");
    drawChart(); 
  }

  display.display();
  
  if (historyCount >= CHART_POINTS) {
    displayMode = (displayMode + 1) % 5; 
  } else {
    displayMode = (displayMode + 1) % 4; 
  }
  
  unsigned long startDelay = millis();
  while (millis() - startDelay < 5000) {
    delay(10); 
  }
}
