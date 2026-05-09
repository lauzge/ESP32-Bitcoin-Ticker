#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "time.h" // Für die Uhrzeit
#include "FS.h"
#include "SD_MMC.h"
#include "SPI.h"

// Netzwerk-Daten
// Diese Variablen bleiben leer, da sie von der SD gefüllt werden
String ssid = "";
String password = "";

//SSD1306Wire display(0x3c, SDA, SCL);
SSD1306Wire display(0x3c, 13, 12); // An ESP-CAM muss das Display auf Pin 12 und Pin 13 gesetzt werden

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
  // Für ESP32-CAM: Kein CS-Pin nötig, aber 1-Bit-Modus erzwingen (true)
  if (!SD_MMC.begin("/sdcard", true)) { 
    Serial.println("SD_MMC Fehler!");
    return false;
  }

  // WICHTIG: Hier muss auch SD_MMC stehen, nicht SD!
  File file = SD_MMC.open("/wifi.txt");
  if (!file) {
    Serial.println("Datei nicht gefunden!");
    return false;
  }

  if (file.available()) {
    ssid = file.readStringUntil('\n');
    ssid.trim();
  }
  
  if (file.available()) {
    password = file.readStringUntil('\n');
    password.trim();
  }

  file.close();
  return (ssid.length() > 0 && password.length() > 0);
}


void setup() {
  Serial.begin(115200);
  delay(1000); 
  
  // I2C Pins 13 (SDA) und 12 (SCL)
//  if (!Wire.begin(13, 12)) {
//    Serial.println("I2C Bus Fehler!");
//  }

  display.init();
  display.flipScreenVertically();
//  display.setContrast(255);      
//  display.setBrightness(255);    
  
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Initialisiere...");
  display.display();

  // SD_MMC einmalig im 1-Bit-Modus starten
  Serial.println("Lese SD-Karte...");
  if (SD_MMC.begin("/sdcard", true)) { 
      // Jetzt die WiFi-Daten auslesen (Funktion sollte nur noch file.open machen)
      if (loadWiFiConfig()) {
        display.drawString(0, 15, "WiFi Daten geladen");
        Serial.println("WiFi Daten geladen");
        display.display();
        WiFi.begin(ssid.c_str(), password.c_str());
      } else {
        display.drawString(0, 15, "Datei-Fehler!");
        display.display();
      }
  } else {
      Serial.println("SD-Hardware Fehler!");
      display.drawString(0, 15, "SD fehlt!");
      display.display();
  }
  
  // Warten auf WiFi
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  pinMode(33, OUTPUT);
  digitalWrite(33, HIGH); // Onboard-LED AUS
  
  display.clear();
  display.drawString(0, 0, "Verbunden!");
  display.display();
  delay(1000);
}

String getLocalTimeStr() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "00:00";
  
  char timeStringBuff[10]; // Wichtig: [10] reserviert den Platz für "HH:MM\0"
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

void updateData() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  int httpCode; // Hier deklarieren wir die Variable für die ganze Funktion!

  // 1. Preise
  if (http.begin(client, "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=USD,EUR")) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    httpCode = http.GET(); // Hier wird sie nur noch benutzt (ohne "int" davor)
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
  delay(300); // Kurze Pause für den Speicher

  // 2. Mempool Gebühren
  if (http.begin(client, "https://mempool.space/api/v1/fees/recommended")) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    int httpCode = http.GET(); // Hier wird die Variable jetzt korrekt erstellt
    if (httpCode == 200) {
      StaticJsonDocument<512> doc;
      deserializeJson(doc, http.getString());
      fastestFee = doc["fastestFee"];
      halfHourFee = doc["halfHourFee"];
      hourFee = doc["hourFee"];
    }
    http.end();
  }
  delay(300); // Nochmals kurz warten

  // 3. Blockhöhe (Erhöhte Robustheit)
  if (http.begin(client, "https://mempool.space/api/blocks/tip/height")) {
    http.addHeader("User-Agent", "ESP32-Ticker");
    httpCode = http.GET(); // Wiederverwendung
    if (httpCode == 200) {
      String payload = http.getString();
      payload.trim(); // Entfernt unsichtbare Leerzeichen/Zeilenumbrüche
      if (payload.length() > 0) {
        blockHeight = payload.toInt();
      }
    }
    http.end();
  }
  // LED Alarm Logik: Dauerlicht bei niedrigen Gebühren
  if (fastestFee > 0 && fastestFee <= 5) {
    digitalWrite(2, HIGH); // LED leuchtet dauerhaft
    Serial.println("LED AN: Gebühren sind niedrig.");
  } else {
    digitalWrite(2, LOW);  // LED aus
  }
  
  lastUpdate = millis();
}

void loop() {
  if (millis() - lastUpdate > 30000 || lastUpdate == 0) { updateData(); }

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
  else { // MEMPOOL
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Mempool Fees:");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 18, "Fast: " + String(fastestFee) + " sat/vB");
    display.drawString(0, 29, "Med:  " + String(halfHourFee) + " sat/vB");
    display.drawString(0, 40, "Slow: " + String(hourFee) + " sat/vB");
    display.drawString(0, 51, "Time: " + currentTime);
  }

  display.display();
 
  displayMode = (displayMode + 1) % 4; // Rotiert durch 4 Ansichten
  delay(5000);
}
