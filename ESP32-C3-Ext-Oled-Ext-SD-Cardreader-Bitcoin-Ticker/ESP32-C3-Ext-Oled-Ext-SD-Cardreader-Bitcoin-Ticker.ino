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

// Globale Strings als einfache Arrays/Objekte deklarieren, um Boot-Abstürze zu verhindern
String ssid;
String password;
String apiKey;

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
  if (file.available()) { apiKey = file.readStringUntil('\n'); apiKey.replace("\r", ""); apiKey.replace("\n", ""); }

  file.close();
  return (ssid.length() > 0 && password.length() > 0 && apiKey.length() > 0);
}

void setup() {
  // Absolut erste Aktion: Dem USB-CDC Controller Zeit geben, stabil zu werden
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
    WiFi.begin(ssid.c_str(), password.c_str());
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
 
  // 1. Preise abrufen
  String priceUrl = "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=USD,EUR&api_key=" + apiKey;
  if (http.begin(client, priceUrl)) {
    http.addHeader("User-Agent", "ESP32-C3-Ticker");
    httpCode = http.GET(); 
    if (httpCode == 200) {
      JsonDocument doc; // Universeller V7-Standard (Verhindert Abstürze)
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

  // 2. Mempool
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

  // Fee Alarm LED Logik
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

  if (displayMode == 0 || displayMode == 1) {
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
