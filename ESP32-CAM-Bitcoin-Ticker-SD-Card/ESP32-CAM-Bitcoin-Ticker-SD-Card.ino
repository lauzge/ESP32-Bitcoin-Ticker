#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "time.h" 
#include "FS.h"
#include "SD_MMC.h"
#include "SPI.h"
#include <Preferences.h> // Bibliothek für nicht-flüchtigen internen Flash-Speicher

#define ONBOARD_LED 33

// Globale Variablen für Netzwerkdaten
String ssid = "";
String password = "";

SSD1306Wire display(0x3c, 13, 12); // SDA=13, SCL=12

// Preferences Instanz deklarieren
Preferences preferences;

// Variablen für Preise und Mempool
float priceEur = 0, oldPriceEur = 0, priceUsd = 0, percentChange = 0;
int fastestFee = 0, halfHourFee = 0, hourFee = 0, blockHeight = 0;
unsigned long lastUpdate = 0;
int displayMode = 0; // 0=EUR, 1=USD, 2=Block, 3=Mempool, 4=CHART

// Chart-Speicher (128 Pixel Breite, alle 2 Pixel ein Wert = 64 Datenpunkte)
const int CHART_POINTS = 64;
float priceHistory[CHART_POINTS];
int historyCount = 0;

// Zeit-Einstellungen (Zentral-Europa)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; 
const int   daylightOffset_sec = 3600;

bool loadWiFiConfig() {
  // 1. Versuchen, die SD-Karte im sicheren 1-Bit-Modus zu starten
  bool sdAvailable = SD_MMC.begin("/sdcard", true);
  
  if (sdAvailable && SD_MMC.exists("/wifi.txt")) {
    Serial.println("Neue wifi.txt auf SD-Karte gefunden! Verarbeite...");
    File file = SD_MMC.open("/wifi.txt", FILE_READ);
    if (file) {
      if (file.available()) { ssid = file.readStringUntil('\n'); ssid.replace("\r", ""); ssid.replace("\n", ""); }
      if (file.available()) { password = file.readStringUntil('\n'); password.replace("\r", ""); password.replace("\n", ""); }
      file.close();

      if (ssid.length() > 0 && password.length() > 0) {
        // Daten intern im NVS sichern
        preferences.begin("wifi-store", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
        Serial.println("WLAN-Daten erfolgreich im internen Speicher gesichert.");

        // DIEBSTAHLSCHUTZ: Datei unwiderruflich von der SD-Karte löschen!
        if (SD_MMC.remove("/wifi.txt")) {
          Serial.println("Sicherheitsloeschung erfolgreich: wifi.txt von SD entfernt!");
        } else {
          Serial.println("WARNUNG: wifi.txt konnte nicht geloescht werden!");
        }
        return true;
      }
    }
  }

  // 2. Wenn keine Datei da ist, Daten aus dem internen NVS-Speicher laden
  Serial.println("Lade WLAN-Daten aus dem internen NVS-Speicher...");
  preferences.begin("wifi-store", true); // Schreibgeschützt öffnen
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();

  return (ssid.length() > 0 && password.length() > 0);
}

void addPriceToHistory(float newPrice) {
  if (historyCount < CHART_POINTS) {
    priceHistory[historyCount] = newPrice;
    historyCount++;
  } else {
    // Array nach links verschieben (ältesten Wert löschen)
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

  // Min- und Max-Werte im Speicher finden
  float minPrice = priceHistory[0];
  float maxPrice = priceHistory[0];
  for (int i = 1; i < historyCount; i++) {
    if (priceHistory[i] < minPrice) minPrice = priceHistory[i];
    if (priceHistory[i] > maxPrice) maxPrice = priceHistory[i];
  }

  // Dynamisches Sicherheits-Polster (Padding) berechnen
  float priceDelta = maxPrice - minPrice;
  if (priceDelta == 0) priceDelta = 2.0;

  // Wir fügen oben und unten jeweils 5% Puffer hinzu, damit die Kurve niemals anstößt
  maxPrice += priceDelta * 0.05;
  minPrice -= priceDelta * 0.05;
  priceDelta = maxPrice - minPrice;

  // Chart-Bereich auf dem OLED festlegen (Y von 16 bis 52 -> Höhe 36 Pixel)
  int chartHeight = 36;
  int chartOffsetIndexY = 52;

  // Linien zeichnen
  for (int i = 0; i < historyCount - 1; i++) {
    int x1 = i * 2;
    int x2 = (i + 1) * 2;

    int y1 = chartOffsetIndexY - (int)((priceHistory[i] - minPrice) / priceDelta * chartHeight);
    int y2 = chartOffsetIndexY - (int)((priceHistory[i + 1] - minPrice) / priceDelta * chartHeight);

    if (y1 < 16) y1 = 16;
    if (y2 < 16) y2 = 16;

    display.drawLine(x1, y1, x2, y2);
  }
  
  // Skalenwerte anzeigen
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 54, String((int)minPrice) + " EUR");
  display.drawString(80, 54, String((int)maxPrice) + " MAX");
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
  delay(1500); // Dem CAM-Board Zeit geben zum Einschwingen
  Serial.println("\n--- ESP32-CAM Diebstahlschutz-Boot ---");
  
  pinMode(ONBOARD_LED, OUTPUT); 
  digitalWrite(ONBOARD_LED, LOW); // LED Test AN
  delay(500); 
  digitalWrite(ONBOARD_LED, HIGH); // LED Test AUS
 
  for (int i = 0; i < CHART_POINTS; i++) {
    priceHistory[i] = 0.0;
  }

  display.init();
  display.flipScreenVertically();
  
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Initialisiere...");
  display.display();

  // Konfiguration laden (Prüft SD, beschreibt NVS, löscht Datei oder liest Intern)
  if (loadWiFiConfig()) {
    display.clear();
    display.drawString(0, 0, "WLAN-Daten aktiv.");
    display.drawString(0, 15, "Verbinde...");
    display.display();
    
    const char* clean_ssid = ssid.c_str();
    const char* clean_password = password.c_str();
    WiFi.begin(clean_ssid, clean_password);
  } else {
    display.clear();
    display.drawString(0, 0, "KEINE DATEN!");
    display.drawString(0, 15, "wifi.txt auf SD einlegen");
    display.display();
    Serial.println("Fehler: Keine Daten gefunden!");
    while(true) { delay(1000); } 
  }
  
  // Warten auf WiFi
  int timeoutCounter = 0;
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
    timeoutCounter++;
    if(timeoutCounter > 60) { 
      WiFi.reconnect();
      timeoutCounter = 0;
    }
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  display.clear();
  display.drawString(0, 0, "Verbunden!");
  display.display();
  delay(1000);

  updateData(); // Erste Daten direkt laden
}

void updateData() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  int httpCode;

  // 1. Bitcoin-Preise schlüssellos direkt von mempool.space holen
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
        addPriceToHistory(priceEur); // Preis in die Historie schieben
      }
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
      if (payload.length() > 0) blockHeight = payload.toInt();
    }
    http.end();
  }

  // LED Alarm Logik (GPIO 33 Active Low für ESP32-CAM)
  if (fastestFee > 0 && fastestFee <= 5) {
    digitalWrite(ONBOARD_LED, LOW); 
  } else {
    digitalWrite(ONBOARD_LED, HIGH);  
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
  else if (displayMode == 3) { 
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Mempool Fees:");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 18, "Fast: " + String(fastestFee) + " sat/vB");
    display.drawString(0, 29, "Med:  " + String(halfHourFee) + " sat/vB");
    display.drawString(0, 40, "Slow: " + String(hourFee) + " sat/vB");
    display.drawString(0, 51, "Time: " + currentTime);
  }
  else if (displayMode == 4) { 
    // 5. SEITE: CHART
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "BTC Trend 30m");
    drawChart();
    }
    display.display();
    // Intelligente Rotation: Schaltet die Chart-Seite (Index 4) erst nach 32 Minuten (64 Punkten) frei
    if (historyCount >= CHART_POINTS) {
      displayMode = (displayMode + 1) % 5;
    }
     else {
      displayMode = (displayMode + 1) % 4;
    }
    delay(5000);
}