#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "time.h"
#include <WiFiManager.h> // Der Webinterface-WLAN-Manager

// 1. HARDWARE-PINS FÜR DEINEN SATELLITEN
#define OLED_SDA 21
#define OLED_SCL 20

#define LED_RED    2
#define LED_YELLOW 3
#define LED_BLUE   8 // Dies ist auch die Fee-Alarm LED

// 64x32 Pixel OLED Initialisierung (0x3c Adresse)
SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL, GEOMETRY_64_32);

// Variablen
float priceEur = 0, oldPriceEur = 0, priceUsd = 0, percentChange = 0;
int fastestFee = 0, halfHourFee = 0, hourFee = 0, blockHeight = 0;
unsigned long lastUpdate = 0;
int displayMode = 0; // 0=EUR, 1=USD, 2=Block, 3=Mempool, 4=CHART

// Chart-Speicher (64 Pixel maximale Breite auf diesem Display!)
const int CHART_POINTS = 32; // Alle 2 Pixel ein Punkt = 64 Pixel Breite
float priceHistory[CHART_POINTS];
int historyCount = 0;

// Zeit-Einstellungen (Zentral-Europa)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; 
const int   daylightOffset_sec = 3600;

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

// Optimierte Chart-Funktion für das winzige 64x32 Pixel Display
void drawChart() {
  if (historyCount < 2) {
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 11, "Sammle...");
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

  // Chart nutzt exakt Y=12 bis Y=31 (Höhe 19 Pixel)
  int chartHeight = 19;
  int chartOffsetY = 31;

  for (int i = 0; i < historyCount - 1; i++) {
    int x1 = i * 2;
    int x2 = (i + 1) * 2;

    int y1 = chartOffsetY - (int)((priceHistory[i] - minPrice) / priceDelta * chartHeight);
    int y2 = chartOffsetY - (int)((priceHistory[i + 1] - minPrice) / priceDelta * chartHeight);

    if (y1 < 12) y1 = 12;
    if (y2 < 12) y2 = 12;

    display.drawLine(x1, y1, x2, y2);
  }
}

void blinkLEDsTest() {
  digitalWrite(LED_RED, HIGH); delay(200); digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, HIGH); delay(200); digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_BLUE, HIGH); delay(200); digitalWrite(LED_BLUE, LOW);
}

void updateData() {
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  int httpCode;
  digitalWrite(LED_YELLOW, HIGH);

  if (http.begin(client, "https://mempool.space/api/v1/prices")) {
    http.addHeader("User-Agent", "ESP32-C3-Satellit");
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
            digitalWrite(LED_RED, HIGH); delay(100); digitalWrite(LED_RED, LOW);
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
  delay(200);

  if (http.begin(client, "https://mempool.space/api/v1/fees/recommended")) {
    http.addHeader("User-Agent", "ESP32-C3-Satellit");
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
  delay(200); 

  if (http.begin(client, "https://mempool.space/api/blocks/tip/height")) {
    http.addHeader("User-Agent", "ESP32-C3-Satellit");
    httpCode = http.GET(); 
    if (httpCode == 200) {
      String payload = http.getString();
      payload.trim(); 
      if (payload.length() > 0) blockHeight = payload.toInt();
    }
    http.end();
  }
  
  if (fastestFee > 0 && fastestFee <= 5) {
    digitalWrite(LED_BLUE, LOW); 
  } else {
    digitalWrite(LED_BLUE, HIGH); 
  }
  
  digitalWrite(LED_YELLOW, LOW); 
  lastUpdate = millis();
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
  Serial.println("\n--- WiFi-Manager C3 Satellit startet ---");

  // 1. HARDWARE-BUTTON RESET LOGIK (GPIO 9 ist der BOOT-Button beim C3 SuperMini)
  const int BUTTON_PIN = 9; 
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // LED Pins konfigurieren
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  
  // Alle LEDs initial ausschalten
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_BLUE, HIGH); // Active Low Aus-Zustand

  blinkLEDsTest(); // Kurzer Funktionstest aller LEDs beim Einschalten

  for (int i = 0; i < CHART_POINTS; i++) {
    priceHistory[i] = 0.0;
  }

  // I2C-Bus explizit auf deinen Wunsch-Pins 21 und 20 starten
  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.flipScreenVertically();

  // Prüfen, ob der BOOT-Knopf DIREKT beim Starten gedrückt gehalten wird (LOW = gedrückt)
  if (digitalRead(BUTTON_PIN) == LOW) {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "RESET-MODUS");
    display.drawString(0, 14, "Knopf 3 Sek");
    display.drawString(0, 24, "halten...");
    display.display();
    
    // Gelbe LED leuchtet als Warnung während des Countdowns
    digitalWrite(LED_YELLOW, HIGH);
    delay(3000); // 3 Sekunden Sicherheits-Wartezeit gegen versehentliches Drücken

    // Nach 3 Sekunden prüfen, ob der Knopf immer noch gehalten wird
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("!!! C3 WLAN-Schnittstelle manuell zurückgesetzt !!!");
      display.clear();
      display.drawString(0, 5, "WLAN ");
      display.drawString(0, 18, "GELÖSCHT!");
      display.display();
      
      // Rote LED blinkt schnell zur Bestätigung
      for(int i=0; i<6; i++) {
        digitalWrite(LED_RED, !digitalRead(LED_RED));
        delay(150);
      }
      digitalWrite(LED_RED, LOW);

      WiFiManager wm;
      wm.resetSettings(); // Löscht die gespeicherten Router-Daten im NVS-Flash
    }
    digitalWrite(LED_YELLOW, LOW);
  }

  // 2. NORMALER STARTVORGANG
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 10, "WLAN...");
  display.display();

  // WiFi Vorbereitung aus dem funktionierenden Mini-Stack
  WiFi.disconnect(true); 
  delay(500);
  WiFi.mode(WIFI_STA);   
  WiFi.setSleep(false);  
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Sendeleistung drosseln gegen BOD/Interferenzen

  WiFiManager wm;

  // AP-Anzeige auf dem 64x32 Display platzsparend ausgeben
  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "CONNECT:");
    display.drawString(0, 11, "Satellit-AP");
    display.drawString(0, 22, "192.168.4.1");
    display.display();
    
    // Gelbe LED leuchtet im Portal-Modus dauerhaft
    digitalWrite(LED_YELLOW, HIGH);
  });

  // Startet das Portal "Satellit-Ticker-AP", falls kein WLAN erreichbar ist
  if (!wm.autoConnect("Satellit-Ticker-AP")) {
    display.clear();
    display.drawString(0, 10, "WM Fehler!");
    display.display();
    delay(3000);
    ESP.restart();
  }

  display.clear();
  display.drawString(0, 10, "WLAN OK!");
  display.display();
  delay(1000);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  updateData();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 11, "Offline...");
    display.display();
    WiFi.reconnect();
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    delay(2000);
    return;
  }

  if (millis() - lastUpdate > 30000 || lastUpdate == 0) { 
    updateData(); 
  }

  display.clear();
  String currentTime = getLocalTimeStr();

  // FIX: Alle Seiten nutzen jetzt die kompakte Arial_10 Schriftart
  display.setFont(ArialMT_Plain_10);

  if (displayMode == 0 || displayMode == 1) { // 1. & 2. PREISE (EUR / USD)
    // Zeile 1: Ticker-Kopf
    if (displayMode == 0) display.drawString(0, 0, "EUR " + currentTime);
    else display.drawString(0, 0, "USD " + currentTime);
    
    // Zeile 2: Preis kompakt formatiert (z. B. "54817 EUR" passt jetzt locker nebeneinander)
    if (displayMode == 0) display.drawString(0, 11, String(priceEur, 0) + " EUR");
    else display.drawString(0, 11, "$ " + String(priceUsd, 0));
    
    // Zeile 3: Trend
    String sign = (percentChange >= 0) ? "+ " : "";
    display.drawString(0, 22, "Chg: " + sign + String(percentChange, 2) + "%");
  } 
  else if (displayMode == 2) { // 3. BLOCKZEIT (In zwei kompakte Zeilen aufgeteilt)
    display.drawString(0, 0, "Blockheight:");
    display.drawString(0, 11, "#" + String(blockHeight));
    display.drawString(0, 22, "Zeit: " + currentTime);
  }
  else if (displayMode == 3) { // 4. MEMPOOL (Kompakte 3er Einteilung, Zeilenabstand exakt 11 Pixel)
    display.drawString(0, 0, "FEE (sat/vB)");
    display.drawString(0, 11, "F:" + String(fastestFee) + "  M:" + String(halfHourFee));
    display.drawString(0, 22, "Slow: " + String(hourFee));
  }
  else if (displayMode == 4) { // 5. SEITE: LIVE CHART
    display.drawString(0, 0, "Trend " + currentTime);
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
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    delay(10); 
  }
}
