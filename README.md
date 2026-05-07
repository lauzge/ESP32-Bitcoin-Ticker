# 🚀 ESP32 Bitcoin Live Ticker

Ein kompakter Bitcoin-Preis-Ticker für den ESP32 mit integriertem OLED-Display (SSD1306). Die Daten werden in Echtzeit über die CryptoCompare API abgerufen und wechseln zwischen EUR und USD, inklusive einer prozentualen Tendenzanzeige.

## ✨ Features
- 💰 **Live-Preise**: Anzeige in Euro (€) und US-Dollar ($).
- 📈 **Tendenzanzeige**: Berechnet die prozentuale Veränderung seit dem letzten Update.
- 🔄 **Automatischer Wechsel**: Sanfter Wechsel der Ansichten alle 5 Sekunden.
- ⚡ **Optimiert**: Nutzt HTTPS mit effizientem Speichermanagement (keine Memory Leaks).
- 🛠️ **Einfach**: Kein API-Key für Basisabfragen erforderlich.

## 🛠 Hardware
- **Mikrocontroller**: ESP32 Wroom (z.B. DevKit V1)
- **Display**: Integriertes 0.96" OLED (SSD1306)
- **Pins**: SDA an Pin 21, SCL an Pin 22 (Standard-I2C)

## 📚 Benötigte Bibliotheken
Folgende Bibliotheken müssen in der Arduino IDE installiert sein:
1. `ESP8266 and ESP32 OLED driver for SSD1306 displays` (by ThingPulse)
2. `ArduinoJson` (by Benoit Blanchon)
3. `HTTPClient` & `WiFiClientSecure` (Standard ESP32 Core)

## 🚀 Installation
1. Klone dieses Repository:
   ```bash
   git clone https://github.com/lauzge/ESP32-Bitcoin-Ticker.git
   ```
2. Öffne die Datei `BitcoinTicker.ino` in der Arduino IDE.
3. Trage deine WLAN-Daten ein:
   ```cpp
   const char* ssid = "DEIN_WLAN";
   const char* password = "DEIN_PASSWORT";
   ```
4. Wähle dein Board aus (**DOIT ESP32 DEVKIT V1**) und klicke auf **Upload**.

## 🖥️ API-Referenz
Dieses Projekt nutzt die kostenlose Schnittstelle von [CryptoCompare](https://cryptocompare.com). 
Abfrage-Endpunkt: `/data/price?fsym=BTC&tsyms=EUR,USD`

---
Erstellt mit ❤️ für die Bitcoin-Community.
