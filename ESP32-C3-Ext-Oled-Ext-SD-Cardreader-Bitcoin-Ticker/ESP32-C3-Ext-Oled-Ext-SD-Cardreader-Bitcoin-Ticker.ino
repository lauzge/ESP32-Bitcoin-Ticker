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

#define ONBOARD_LED 8

// Globale Strings
String ssid;
String password;

// Display an C3 Pins (SDA=10, SCL=21)
SSD1306Wire display(0x3c, 10, 21);

// Variablen für Preise und Mempool
float priceEur = 0, oldPriceEur = 0, priceUsd = 0, percentChange = 0;
int fastestFee = 0, halfHourFee = 0, hourFee = 0, blockHeight = 0;
unsigned long lastUpdate = 0;
int displayMode = 0; 

// Zeit-Einstellungen (Zentral-Europa)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; 
const int   daylightOffset_sec = 3600;

bool loadWiFiConfig() {
  // SPI Hardware-Pins für C3 initialisieren (SCK=4, MISO=2, MOSI=3, SS=5)
  SPI.begin(4, 2, 3, 5); 
  
  if (!SD.begin(5)) {
    Serial.println("SD-Karte konnte nicht geladen werden!");
    return false;
  }

  File file = SD.open("/.wifi.txt");
  if (!file) {
    Serial.println(".wifi.txt nicht gefunden!");
    return false;
  }

  // Zeilenweise einlesen und von Windows-Steuerzeichen befreien
  if (file.available()) { ssid = file.readStringUntil('\n'); ssid.replace("\r", ""); ssid.replace("\n", ""); }
  if (file.available()) { password = file.readStringUntil('\n'); password.replace("\r", ""); password.replace("\n", ""); }

  file.close();
  // Validierung: apiKey muss nicht zwingend befüllt sein, da mempool.space keinen Key braucht!
  return (ssid.length() > 0 && password.length() > 0);
}

void setup() {
  delay(1000); 
  Serial.begin(115200);
  Serial.println("\n--- C3-Satellit Bootvorgang gestartet ---");

  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, HIGH); // Erstmal AUS (Active Low)

  Wire.begin(10, 21); 
  display.init();
  display.flipScreenVertically();
  display.setContrast(255);
  display.setBrightness(255);

  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Lese SD-Karte...");
  display.display();

  if (loadWiFiConfig()) {
    display.drawString(0, 15, "WiFi Daten geladen");
    display.display();
    
    Serial.println("WLAN-Daten erfolgreich gelesen.");
    
    // Saubere Zeiger-Übergabe für SSIDs mit Leerzeichen
    const char* clean_ssid = ssid.c_str();
    const char* clean_password = password.c_str();
    WiFi.begin(clean_ssid, clean_password);
  } else {
    display.drawString(0, 15, "SD Fehler!");
    display.display();
    Serial.println("Fehler beim Laden der Konfiguration von SD!");
  }
  
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
  }
  Serial.println("\nWLAN erfolgreich verbunden!");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  display.clear();
  display.drawString(0, 0, "Verbunden!");
  display.display();
  delay(1000);
}

void updateData() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  int httpCode;
 
  // 1. NEU: Bitcoin-Preise direkt von mempool.space abrufen (Kein API-Key nötig!)
  if (http.begin(client, "https://mempool.space(api/v1/prices")) {
    http.addHeader("User-Agent", "ESP32-C3-Ticker");
    httpCode = http.GET(); 
    if (httpCode == 200) {
      JsonDocument doc; // V7-Standard
      deserializeJson(doc, http.getString());
      
      // Werte zuweisen (Mempool liefert Ganzzahlen)
      priceEur = doc["EUR"];
      priceUsd = doc["USD"];
      
      // Trendberechnung stabil im Code selbst ausführen
      if (oldPriceEur > 0) {
        percentChange = ((priceEur - oldPriceEur) / oldPriceEur) * 100.0;
      }
      oldPriceEur = priceEur; // Sichern für den nächsten Intervall
    }
    http.end();
  }
  delay(300);

  // 2. Mempool Gebühren
  if (http.begin(client, "https://mempool.space/api/v1/fees/recommended")) {
    http.addHeader("User-Agent", "ESP32-C3-Ticker");
    httpCode = http.GET(); 
    if (httpCode == 200) {
      JsonDocument doc;
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
    http.addHeader("User-Agent", "ESP32-C3-Ticker");
    httpCode = http.GET(); 
    if (httpCode == 200) {
      String payload = http.getString();
      payload.trim();
      if (payload.length() > 0) blockHeight = payload.toInt();
    }
    http.end();
  }

  // Fee Alarm LED Logik (Active Low)
  if (fastestFee > 0 && fastestFee <= 5) {
    digitalWrite(ONBOARD_LED, LOW);   // LOW = LED AN
  } else {
    digitalWrite(ONBOARD_LED, HIGH);  // HIGH = LED AUS
  }
  
  lastUpdate = millis();
}

String getLocalTimeStr() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "00:00";
  char timeStringBuff[10]; 
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

void loop() {
  if (millis() - lastUpdate > 30000 || lastUpdate == 0) { updateData(); }

  display.clear();
  String currentTime = getLocalTimeStr();

  if (displayMode == 0 || displayMode == 1) { // 1. & 2. PREISE (EUR / USD)
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Bitcoin Live " + currentTime);
    display.setFont(ArialMT_Plain_24);
    if (displayMode == 0) {
      display.drawString(0, 18, String(priceEur, 0) + " EUR"); 
    } else {
      display.drawString(0, 18, "$ " + String(priceUsd, 2)); 
    }
    display.setFont(ArialMT_Plain_10);
    String trend = (percentChange >= 0) ? "+ " : "";
    trend += String(percentChange, 4) + "% " + (percentChange >= 0 ? "^" : "v");
    display.drawString(0, 48, "Chg: " + trend);
  } 
  else if (displayMode == 2) { // 3. BLOCKZEIT GROSS
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Current block:");
    display.setFont(ArialMT_Plain_24);
    display.drawString(0, 18, "#" + String(blockHeight));
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 52, "Update: " + currentTime);
  }
  else { // 4. MEMPOOL FEES
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
