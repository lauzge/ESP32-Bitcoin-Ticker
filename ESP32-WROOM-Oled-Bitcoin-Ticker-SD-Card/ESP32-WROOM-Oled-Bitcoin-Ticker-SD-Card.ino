#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "time.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Netzwerk-Daten (werden von SD geladen)
String ssid = "";
String password = "";
String apiKey = ""; // NEU: Wird jetzt fehlerfrei befüllt!

SSD1306Wire display(0x3c, SDA, SCL);

// Variablen
float priceEur = 0, oldPriceEur = 0, priceUsd = 0, percentChange = 0;
int fastestFee = 0, halfHourFee = 0, hourFee = 0, blockHeight = 0;
unsigned long lastUpdate = 0;
int displayMode = 0; // 0=EUR, 1=USD, 2=Block, 3=Mempool

// Zeit-Einstellungen (Zentral-Europa)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; 
const int   daylightOffset_sec = 3600;

bool loadWiFiConfig() {
  if (!SD.begin(5)) { // 5 ist dein CS-Pin
    Serial.println("SD-Karte konnte nicht geladen werden!");
    return false;
  }

  // FIX: Pfad wieder auf die unsichtbare Datei inklusive Punkt geändert!
  File file = SD.open("/.wifi.txt");
  if (!file) {
    Serial.println(".wifi.txt nicht gefunden!");
    return false;
  }

  // Zeilenweise einlesen und von Windows-Steuerzeichen befreien
  if (file.available()) {
    ssid = file.readStringUntil('\n');
    ssid.replace("\r", "");
    ssid.replace("\n", "");
  }
  
  if (file.available()) {
    password = file.readStringUntil('\n');
    password.replace("\r", "");
    password.replace("\n", "");
  }

  if (file.available()) { 
    apiKey = file.readStringUntil('\n'); 
    apiKey.replace("\r", "");
    apiKey.replace("\n", "");
  }

  file.close();
  return (ssid.length() > 0 && password.length() > 0 && apiKey.length() > 0);
}

void setup() {
  Serial.begin(115200);

  pinMode(2, OUTPUT); 
  digitalWrite(2, HIGH); 
  delay(1000); 
  digitalWrite(2, LOW);

  display.init();
  display.flipScreenVertically();

  display.clear();
  display.drawString(0, 0, "Lese SD-Karte...");
  display.display();

  if (loadWiFiConfig()) {
    display.drawString(0, 15, "WiFi Daten geladen");
    display.display();
    
    // Saubere Übergabe für SSIDs mit Leerzeichen
    const char* clean_ssid = ssid.c_str();
    const char* clean_password = password.c_str();
    WiFi.begin(clean_ssid, clean_password);
  } else {
    display.drawString(0, 15, "SD/Datei Fehler!");
    display.display();
  }
  
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  display.clear();
  display.drawString(0, 0, "Verbunden!");
  display.display();
  delay(1000);
}

String getLocalTimeStr() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "00:00";
  
  char timeStringBuff[10]; 
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

void updateData() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  int httpCode;

  // 1. Preise abrufen mit geladenem API-Key
  String url = "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=USD,EUR&api_key=" + apiKey;
  if (http.begin(client, url)) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    httpCode = http.GET(); // FIX: Nur einmal aufrufen, kein doppeltes int deklarieren!
    
    if (httpCode == 200) { // FIX: Nutzt das Ergebnis der obigen Variable
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
  delay(300); 

  // 2. Mempool Gebühren
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

  // 3. Blockhöhe
  if (http.begin(client, "https://mempool.space/api/blocks/tip/height")) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    httpCode = http.GET(); 
    if (httpCode == 200) {
      String payload = http.getString();
      payload.trim(); 
      if (payload.length() > 0) {
        blockHeight = payload.toInt();
      }
    }
    http.end();
  }
  
  // LED Alarm Logik (GPIO 2 Active High für Wroom)
  if (fastestFee > 0 && fastestFee <= 5) {
    digitalWrite(2, HIGH); // LED AN
    Serial.println("LED AN: Gebühren sind niedrig.");
  } else {
    digitalWrite(2, LOW);  // LED AUS
  }
  
  lastUpdate = millis();
}

void loop() {
  if (millis() - lastUpdate > 30000 || lastUpdate == 0) { updateData(); }

  display.clear();
  String currentTime = getLocalTimeStr();

  if (displayMode == 0 || displayMode == 1) { 
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
  else if (displayMode == 2) { 
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Current block:");
    display.setFont(ArialMT_Plain_24);
    display.drawString(0, 18, "#" + String(blockHeight));
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 52, "Update: " + currentTime);
  }
  else { 
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Mempool Fees:");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 18, "Fast: " + String(fastestFee) + " sat/vB");
    display.drawString(0, 29, "Med:  " + String(halfHourFee) + " sat/vB");
    display.drawString(0, 40, "Slow: " + String(hourFee) + " sat/vB");
    display.drawString(0, 51, "Time: " + currentTime);
  }

  display.display();
  displayMode = (displayMode + 1) % 4; 
  delay(5000);
}
