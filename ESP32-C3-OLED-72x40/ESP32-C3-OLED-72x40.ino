#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "time.h" 
#include <WiFiManager.h> // Der Webinterface-WLAN-Manager

// Onboard-LED für den Fee-Alarm beim C3 SuperMini
#define ONBOARD_LED 8

// Ihr funktionierender Display-Konstruktor (SDA=5, SCL=6)
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 6, /* data=*/ 5);

// API-Endpunkte
const char* feeEndpoint   = "https://mempool.space/api/v1/fees/recommended";
const char* blockEndpoint = "https://mempool.space/api/blocks/tip/height";
const char* priceEndpoint = "https://mempool.space/api/v1/prices"; 

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

// Chart-Speicher für das kleine 72x40 Display (36 Punkte * 2 Pixel = 72 Pixel Breite)
const int CHART_POINTS = 36;
long priceHistory[CHART_POINTS];
int historyCount = 0;

unsigned long lastApiCheck = 0;
const unsigned long apiInterval = 30000; 
int displayMode = 0; // 0=EUR, 1=USD, 2=Chg, 3=Block, 4=Mempool, 5=Chart

void addPriceToHistory(long newPrice) {
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
  if (historyCount < 2) return;

  long minPrice = priceHistory[0];
  long maxPrice = priceHistory[0];
  for (int i = 1; i < historyCount; i++) {
    if (priceHistory[i] < minPrice) minPrice = priceHistory[i];
    if (priceHistory[i] > maxPrice) maxPrice = priceHistory[i];
  }

  long priceDelta = maxPrice - minPrice;
  if (priceDelta == 0) priceDelta = 2; 

  maxPrice += priceDelta * 0.05;
  minPrice -= priceDelta * 0.05;
  
  float floatDelta = (float)(maxPrice - minPrice);
  int chartHeight = 25;
  int chartOffsetY = 39;

  for (int i = 0; i < historyCount - 1; i++) {
    int x1 = i * 2;
    int x2 = (i + 1) * 2;

    int y1 = chartOffsetY - (int)((float)(priceHistory[i] - minPrice) / floatDelta * chartHeight);
    int y2 = chartOffsetY - (int)((float)(priceHistory[i + 1] - minPrice) / floatDelta * chartHeight);

    if (y1 < 13) y1 = 13;
    if (y2 < 13) y2 = 13;

    u8g2.drawLine(x1, y1, x2, y2);
  }
}

void updateAllData() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.addHeader("User-Agent", "ESP32-C3-MiniTicker");

  if (http.begin(client, priceEndpoint)) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      JsonDocument doc; 
      deserializeJson(doc, http.getString());
      
      long currentEur = doc["EUR"].as<long>();
      priceUSD = doc["USD"].as<long>();
      
      if (currentEur > 0) {
        if (priceEUR > 0) {
          if (currentEur != priceEUR) { 
            oldPriceEUR = priceEUR;
            percentChange = ((float)currentEur - (float)oldPriceEUR) / (float)oldPriceEUR * 100.0;
          }
        } else {
          percentChange = 0.00;
        }
        priceEUR = currentEur; 
        addPriceToHistory(priceEUR); 
      }
    }
    http.end();
  }
  delay(300);

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

  if (http.begin(client, blockEndpoint)) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      payload.trim();
      if (payload.length() > 0) blockHeight = payload.toInt();
    }
    http.end();
  }

  if (fastestFee > 0 && fastestFee <= 5) {
    digitalWrite(ONBOARD_LED, LOW);   
  } else {
    digitalWrite(ONBOARD_LED, HIGH);  
  }
}

String getLocalTimeStr() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "00:00";
  char timeStringBuff[10];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Dem USB-CDC beim C3 Zeit geben, stabil zu werden
  Serial.println("\n--- WiFi-Manager C3 Ticker startet ---");

  // 1. HARDWARE-BUTTON RESET LOGIK (GPIO 9 ist der BOOT-Button beim C3 SuperMini)
  const int BUTTON_PIN = 9; 
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, HIGH); // Erstmal aus (Active Low)
  
  for (int i = 0; i < CHART_POINTS; i++) {
    priceHistory[i] = 0;
  }

  // Display initialisieren für sofortiges Feedback beim Booten
  Wire.begin(5, 6); 
  u8g2.begin();

  // Prüfen, ob der BOOT-Knopf direkt beim Starten gedrückt gehalten wird (LOW = gedrückt)
  if (digitalRead(BUTTON_PIN) == LOW) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 10, "RESET-MODUS!");
    u8g2.drawStr(0, 22, "Knopf halten...");
    u8g2.sendBuffer();
    
    delay(3000); // 3 Sekunden Sicherheits-Wartezeit gegen versehentliches Drücken

    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("!!! C3 WLAN-Schnittstelle manuell zurueckgesetzt !!!");
      u8g2.clearBuffer();
      u8g2.drawStr(0, 15, "WLAN ");
      u8g2.drawStr(0, 28, "GELÖSCHT!");
      u8g2.sendBuffer();
      delay(2000);
      
      WiFiManager wm;
      wm.resetSettings(); // Löscht die gespeicherten Router-Daten im NVS-Flash
    }
  }

  // 2. NORMALER STARTVORGANG
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 15, "WLAN...");
  u8g2.sendBuffer();

  // WiFi Vorbereitung
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Sendeleistung drosseln gegen BOD

  WiFiManager wm;
  
  // Falls kein bekanntes WLAN gefunden wird, schaltet das kleine Display um:
  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_4x6_tf); // Ultrakompakte Schrift für das 72x40 Display
    u8g2.drawStr(0, 6, "KEIN WLAN GEFUNDEN!");
    u8g2.drawStr(0, 16, "Handy verbinden mit:");
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 26, "C3-Ticker-AP");
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(0, 36, "Browser: 192.168.4.1");
    u8g2.sendBuffer();
    Serial.println("Portal-Modus aktiv.");
  });

  // Startet das Portal "C3-Ticker-AP", falls kein WLAN erreichbar ist
  if (!wm.autoConnect("C3-Ticker-AP")) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 20, "WM Fehler!");
    u8g2.sendBuffer();
    delay(3000);
    ESP.restart();
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 15, "WLAN OK!");
  u8g2.sendBuffer();
  delay(1500);

  // Uhrzeit via NTP holen und ersten API-Abruf starten
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

    if (displayMode == 0 || displayMode == 1 || displayMode == 2 || displayMode == 5) {
      u8g2.setFont(u8g2_font_5x7_tf);
      if (displayMode == 0) u8g2.drawStr(0, 8, "BTC-EUR");
      else if (displayMode == 1) u8g2.drawStr(0, 8, "BTC-USD");
      else if (displayMode == 2) u8g2.drawStr(0, 8, "Change");
      else u8g2.drawStr(0, 8, "Trend 18m"); 
      
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
      else if (displayMode == 5) {
        drawChart(); 
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
    
    if (historyCount >= CHART_POINTS) {
      displayMode = (displayMode + 1) % 6; 
    } else {
      displayMode = (displayMode + 1) % 5; 
    }
    
    delay(5000); 
    
  } else {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 20, "Lade Daten...");
    u8g2.sendBuffer();
    delay(1000);
  }
}
