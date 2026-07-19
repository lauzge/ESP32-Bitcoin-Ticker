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

#define LED_GREEN  2  // Pin 2 blinkt langsam (600ms)
#define LED_YELLOW 3  // Pin 3 blinkt schnell (150ms) / statisch bei API-Abruf
#define LED_RED    8  // Pin 8 ist die Fee-Alarm LED (Active Low)

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

// Globale Variablen für das asynchrone Multitasking-Blinken
unsigned long timerYellow = 0;
unsigned long timerGreen = 0;
bool stateYellow = false;
bool stateGreen = false;

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
  digitalWrite(LED_GREEN, HIGH); delay(200); digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, HIGH); delay(200); digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, HIGH); delay(200); digitalWrite(LED_RED, LOW);
}

void updateData() {
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  int httpCode;
  
  // Gelbe LED geht beim Web-Abruf statisch an
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
      JsonDocument doc;
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
  
  // Blaue LED Fee-Alarm
  if (fastestFee > 0 && fastestFee <= 5) {
    digitalWrite(LED_RED, LOW); 
  } else {
    digitalWrite(LED_RED, HIGH); 
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
  delay(1000); 

  const int BUTTON_PIN = 9; 
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT); // Pin 8 (Deine blaue LED / Fee-Alarm)
  
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, HIGH); // Active Low Aus-Zustand für Pin 8 (Onboard-LED Aus)

  blinkLEDsTest(); 

  for (int i = 0; i < CHART_POINTS; i++) {
    priceHistory[i] = 0.0;
  }

  Wire.begin(OLED_SDA, OLED_SCL);
  display.init();
  display.flipScreenVertically();

  // Prüfen, ob der BOOT-Knopf (GPIO 9) direkt beim Einschalten gehalten wird
  if (digitalRead(BUTTON_PIN) == LOW) {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "RESET-MODUS");
    display.drawString(0, 14, "Knopf 3 Sek");
    display.drawString(0, 24, "halten...");
    display.display();
    
    digitalWrite(LED_YELLOW, HIGH);
    delay(3000); 

    if (digitalRead(BUTTON_PIN) == LOW) {
      display.clear();
      display.drawString(0, 5, "WLAN ");
      display.drawString(0, 18, "GELÖSCHT!");
      display.display();
      
      for(int i=0; i<6; i++) {
        digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
        delay(150);
      }
      digitalWrite(LED_GREEN, LOW);

      WiFiManager wm;
      wm.resetSettings(); 
    }
    digitalWrite(LED_YELLOW, LOW);
  }

  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 10, "WLAN...");
  display.display();

  WiFi.disconnect(true); 
  delay(500);
  WiFi.mode(WIFI_STA);   
  WiFi.setSleep(false);  
  WiFi.setTxPower(WIFI_POWER_8_5dBm); 

  WiFiManager wm;

  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "CONNECT:");
    display.drawString(0, 11, "Satellit-AP");
    display.drawString(0, 22, "192.168.4.1");
    display.display();
    digitalWrite(LED_YELLOW, HIGH);
  });

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

// FIX: Variable umbenannt, um Namenskonflikte mit dem ESP32-Core Core zu verhindern!
unsigned long timerBlueAlarm = 0;
bool stateAlarm = false;

void loop() {
  unsigned long now = millis();

  // 1. GELBE LED: Schnelles Blinken (150 ms)
  if (now - timerYellow >= 150) {
    timerYellow = now;
    stateYellow = !stateYellow;
    if (now - lastUpdate <= 30000) {
      digitalWrite(LED_YELLOW, stateYellow ? HIGH : LOW);
    }
  }

  // 2. GRÜNE LED: Langsames Blinken (600 ms)
  if (now - timerGreen >= 600) {
    timerGreen = now;
    stateGreen = !stateGreen;
    digitalWrite(LED_GREEN, stateGreen ? HIGH : LOW);
  }

  // 3. BLAUE/ROTE LED (PIN 8): Dynamischer Blink-FeeAlarm bei <= 5 sat/vB
  if (fastestFee > 0 && fastestFee <= 5) {
    if (now - timerBlueAlarm >= 500) {
      timerBlueAlarm = now;
      stateAlarm = !stateAlarm;
      digitalWrite(LED_RED, stateAlarm ? HIGH : LOW); 
    }
  } else {
    digitalWrite(LED_RED, HIGH); // Dauer-AUS im Leerlauf (Invertiert für Onboard-LED Schutz)
  }

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

  if (now - lastUpdate > 30000 || lastUpdate == 0) { 
    updateData(); 
  }

  display.clear();
  String currentTime = getLocalTimeStr();
  display.setFont(ArialMT_Plain_10);

  if (displayMode == 0 || displayMode == 1) { 
    if (displayMode == 0) display.drawString(0, 0, "EUR " + currentTime);
    else display.drawString(0, 0, "USD " + currentTime);
    
    if (displayMode == 0) display.drawString(0, 11, String(priceEur, 0) + " EUR");
    else display.drawString(0, 11, "$ " + String(priceUsd, 0));
    
    String sign = (percentChange >= 0) ? "+ " : "";
    display.drawString(0, 22, "Chg: " + sign + String(percentChange, 2) + "%");
  } 
  else if (displayMode == 2) { 
    display.drawString(0, 0, "Blockheight:");
    display.drawString(0, 11, "#" + String(blockHeight));
    display.drawString(0, 22, "Zeit: " + currentTime);
  }
  else if (displayMode == 3) { 
    display.drawString(0, 0, "FEE (sat/vB)");
    display.drawString(0, 11, "F:" + String(fastestFee) + "  M:" + String(halfHourFee));
    display.drawString(0, 22, "Slow: " + String(hourFee));
  }
  else if (displayMode == 4) { 
    display.drawString(0, 0, "Trend " + currentTime);
    drawChart();
  }

  display.display();

  if (historyCount >= CHART_POINTS) {
    displayMode = (displayMode + 1) % 5; 
  } else {
    displayMode = (displayMode + 1) % 4; 
  }
  
  // Nicht-blockierendes Warten für das Display (5 Sekunden)
  unsigned long startDelay = millis();
  while (millis() - startDelay < 5000) {
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    
    unsigned long loopNow = millis();
    
    // LEDs in der Warteschleife weiterblinken lassen
    if (loopNow - timerYellow >= 600) {
      timerYellow = loopNow;
      stateYellow = !stateYellow;
      if (loopNow - lastUpdate <= 30000) {
        digitalWrite(LED_YELLOW, stateYellow ? HIGH : LOW);
      }
    }
    if (loopNow - timerGreen >= 150) {
      timerGreen = loopNow;
      stateGreen = !stateGreen;
      digitalWrite(LED_GREEN, stateGreen ? HIGH : LOW);
    }
    
    // Fee-Alarm Blinken in der Warteschleife halten
    if (fastestFee > 0 && fastestFee <= 5) {
      if (loopNow - timerBlueAlarm >= 500) {
        timerBlueAlarm = loopNow;
        stateAlarm = !stateAlarm;
        digitalWrite(LED_RED, stateAlarm ? HIGH : LOW);
      }
    } else {
      digitalWrite(LED_RED, HIGH);
    }
    
    delay(10); 
  }
}
