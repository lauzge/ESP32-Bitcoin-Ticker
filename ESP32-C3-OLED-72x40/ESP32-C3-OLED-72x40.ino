#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "time.h" // Für die Uhrzeit

// 1. WLAN-Zugangsdaten und API-Key anpassen
const char* ssid     = "MEIN_WLAN";
const char* password = "MEIN_PASSWORT";
const char* apiKey   = "MEIN_API_KEY"; // NEU: Dein CryptoCompare Key

// Onboard-LED für den Fee-Alarm beim C3 SuperMini
#define ONBOARD_LED 8

// Ihr funktionierender Display-Konstruktor (SDA=5, SCL=6)
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 6, /* data=*/ 5);

// API-Endpunkte (Die Preis-URL bauen wir jetzt dynamisch mit dem Key zusammen)
const char* feeEndpoint   = "https://mempool.space/api/v1/fees/recommended";
const char* blockEndpoint = "https://mempool.space/api/blocks/tip/height";

// Zeit-Einstellungen (Zentral-Europa)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; 
const int   daylightOffset_sec = 3600;

// Variablen
long priceEUR = 0;
long oldPriceEUR = 0;
long priceUSD = 0;
float percentChange = 0;
int fastestFee = 0;
int blockHeight = 0;

unsigned long lastApiCheck = 0;
const unsigned long apiInterval = 30000; 
int displayMode = 0; // 0=EUR, 1=USD, 2=Chg, 3=Block, 4=Mempool

void updateAllData() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.addHeader("User-Agent", "ESP32-C3-MiniTicker");

  // 1. Preise abrufen (Jetzt dynamisch mit API-Key)
  String priceEndpoint = "https://min-api.cryptocompare.com/data/price?fsym=BTC&tsyms=USD,EUR&api_key=" + String(apiKey);

  if (http.begin(client, priceEndpoint)) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      JsonDocument doc;
      deserializeJson(doc, http.getString());
      if (priceEUR > 0) {
        oldPriceEUR = priceEUR;
        priceEUR = doc["EUR"];
        percentChange = ((float)(priceEUR - oldPriceEUR) / oldPriceEUR) * 100.0;
      } else {
        priceEUR = doc["EUR"];
      }
      priceUSD = doc["USD"];
    }
    http.end();
  }
  delay(300);

  // 2. Mempool Fees abrufen
  if (http.begin(client, feeEndpoint)) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      JsonDocument doc;
      deserializeJson(doc, http.getString());
      fastestFee = doc["fastestFee"];
    }
    http.end();
  }
  delay(300);

  // 3. Blockhöhe abrufen
  if (http.begin(client, blockEndpoint)) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      payload.trim();
      if (payload.length() > 0) blockHeight = payload.toInt();
    }
    http.end();
  }

  // BLAUE LED Fee-Alarm Logik (GPIO 8, Active Low beim C3 SuperMini)
  if (fastestFee > 0 && fastestFee <= 5) {
    digitalWrite(ONBOARD_LED, LOW);   // LOW = LED AN
  } else {
    digitalWrite(ONBOARD_LED, HIGH);  // HIGH = LED AUS
  }
}

// Holt die aktuelle Uhrzeit im Format HH:MM
String getLocalTimeStr() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "00:00";
  char timeStringBuff[10];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, HIGH); // Erstmal aus (Active Low)

  Wire.begin(5, 6); 
  u8g2.begin();
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 15, "WLAN...");
  u8g2.sendBuffer();

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); 

  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); 

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    attempts++;
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 12, "Suche WiFi");
    String dots = "Sec: " + String(attempts / 2);
    u8g2.drawStr(0, 28, dots.c_str());
    u8g2.sendBuffer();
    
    if (attempts > 60) {
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      WiFi.setTxPower(WIFI_POWER_8_5dBm);
      attempts = 0;
    }
  }
  
  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, "WLAN OK!");
  u8g2.sendBuffer();
  delay(1500);

  // Zeit-Sync starten
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  updateAllData();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 20, "Offline...");
    u8g2.sendBuffer();
    WiFi.reconnect();
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    delay(2000);
    return;
  }

  if (millis() - lastApiCheck >= apiInterval || lastApiCheck == 0) {
    updateAllData();
    lastApiCheck = millis();
  }

  if (priceEUR > 0 && priceUSD > 0) {
    u8g2.clearBuffer();
    String timeStr = getLocalTimeStr();

    if (displayMode == 0 || displayMode == 1 || displayMode == 2) {
      u8g2.setFont(u8g2_font_5x7_tf);
      if (displayMode == 0) u8g2.drawStr(0, 8, "BTC-EUR");
      else if (displayMode == 1) u8g2.drawStr(0, 8, "BTC-USD");
      else u8g2.drawStr(0, 8, "Change");
      
      u8g2.drawStr(42, 8, timeStr.c_str()); 
      u8g2.drawHLine(0, 10, 72);

      if (displayMode == 0) {
        u8g2.setFont(u8g2_font_logisoso16_tf); 
        u8g2.drawStr(0, 34, String(priceEUR).c_str());
      } 
      else if (displayMode == 1) {
        u8g2.setFont(u8g2_font_logisoso16_tf); 
        u8g2.drawStr(0, 34, String(priceUSD).c_str());
      } 
      else if (displayMode == 2) {
        String trend = "";
        if (percentChange > 0) trend = " +^";
        else if (percentChange < 0) trend = " v";
        else trend = " --";
        
        String chgStr = String(percentChange, 2) + trend;
        u8g2.setFont(u8g2_font_6x12_tf); 
        u8g2.drawStr(0, 28, chgStr.c_str());
      }
    }
    else if (displayMode == 3) { 
      u8g2.setFont(u8g2_font_5x7_tf);
      u8g2.drawStr(0, 8, "BLOCKHEIGHT");
      u8g2.drawHLine(0, 10, 72);
      
      u8g2.setFont(u8g2_font_6x12_tf);
      String blkStr = "#" + String(blockHeight);
      u8g2.drawStr(0, 24, blkStr.c_str()); 
      
      u8g2.setFont(u8g2_font_4x6_tf); 
      u8g2.drawStr(0, 38, ("Zeit: " + timeStr).c_str());
    }
    else if (displayMode == 4) { 
      u8g2.setFont(u8g2_font_5x7_tf);
      u8g2.drawStr(0, 8, "MEMPOOL FEE");
      u8g2.drawHLine(0, 10, 72);
      
      u8g2.setFont(u8g2_font_6x12_tf);
      String feeStr = String(fastestFee) + " sat/vB";
      u8g2.drawStr(0, 24, feeStr.c_str());
      
      u8g2.setFont(u8g2_font_4x6_tf); 
      u8g2.drawStr(0, 38, ("Zeit: " + timeStr).c_str());
    }

    u8g2.sendBuffer();
    displayMode = (displayMode + 1) % 5; 
    delay(5000); 
    
  } else {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 20, "Lade Daten...");
    u8g2.sendBuffer();
    delay(1000);
  }
}
