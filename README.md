# 🚀 ESP32 Bitcoin Live Ticker - Multifunktions-Edition

Dieses Open-Source-Projekt zeigt Bitcoin-Echtzeitdaten auf verschiedenen OLED- und IPS-Displays an. Die Ticker bieten Live-Kurse, prozentuale Tendenzen, die aktuelle Blockzeit sowie detaillierte Mempool-Gebührenwächter.

## 📂 Repository Struktur

Das Repository ist in vier spezialisierte Hardware-Versionen unterteilt:

### 1. [ESP32-Wroom Ticker (WiFi-Manager Edition)](./ESP32-WROOM-Oled-Bitcoin-Ticker-SD-Card)
Für Standard ESP32-Boards mit I2C-OLED (SDA: 21, SCL: 22).
*   **Besonderheit:** Komplett auf **WiFiManager** umgestellt! Keine festen WLAN-Daten im Code. Bietet einen Hardware-Reset via BOOT-Button (GPIO 0). Zeichnet einen fortlaufenden Autoscale-Live-Chart der letzten 32 Minuten (64 Punkte).

![Vorschau des Bitcoin Tickers](ESP32-Bitcoin-Ticker.png)


### 2. [ESP32-CAM (Diebstahlsichere Edition)](./ESP32-CAM-Bitcoin-Ticker-SD-Card)
Optimiert für das ESP32-CAM Board mit integriertem MicroSD-Slot (SDA: GPIO 13, SCL: GPIO 12).
*   **Besonderheit:** **Integrierter Diebstahlschutz!** Importiert beim Booten die `wifi.txt` von der SD-Karte flüchtig in den internen CPU-Flash (Preferences) und **löscht die Datei sofort unwiderruflich von der Karte**. Zeigt nach 32 Minuten den vollautomatisch skalierten Live-Chart.

![Vorschau des Bitcoin Tickers](./ESP32-CAM-Bitcoin-Ticker-SD-Card/ESP32-CAM-Bitcoin-Ticker-SD-Card_EUR.png)


### 3. [ESP32-C3-Version (Diebstahlsicherer für den zukünftigen Messing-Satellit)](./ESP32-C3-Ext-Oled-Ext-SD-Cardreader-Bitcoin-Ticker)
Optimiert für das stromsparende ESP32-C3-Board mit externem SD-Kartenleser (SDA: GPIO 10, SCL: GPIO 21).
*   **Besonderheit:** Nutzt dasselbe **Diebstahlschutz-Löschverfahren** via NVS-Speicher über die sichtbare `wifi.txt`. Bietet Hardware-SPI-Bus-Trennung zur Vermeidung von Signalstörungen.

![Vorschau des Bitcoin Tickers](./ESP32-C3-Ext-Oled-Ext-SD-Cardreader-Bitcoin-Ticker/ESP32-C3-Ext-Oled-Ext-SD-Cardreader-Bitcoin-Ticker-EUR.png)


### 4. [ESP32-C3 Mini-Version (WiFi-Manager Edition)](./ESP32-C3-OLED-72x40)
Ultraminimale Standalone-Version ohne SD-Kartenleser für den Schreibtisch (SDA: GPIO 5, SCL: GPIO 6).
*   **Besonderheit:** Komplett auf **WiFiManager** umgestellt! Bietet einen physischen Werksreset über den BOOT-Button (GPIO 9) und zeigt bei fehlender Verbindung eine bequeme Handy-Anleitung auf dem 72x40 Mini-Display. Inklusive 18-Minuten-Trend-Chart.

![Vorschau des Bitcoin Tickers](./ESP32-C3-OLED-72x40/ESP32-C3-OLED-72x40-USD.png)


### 5. 🛰️ ESP32-C3 Bitcoin Brass Satellite Ticker

Ein ultrakompakter, autonomer Bitcoin- und Mempool-Ticker, verpackt in einem kunstvoll handgelöteten Messing-Drahtskelett. Das Projekt basiert auf dem **ESP32-C3 SuperMini** und visualisiert Live-Blockchain-Daten auf einem winzigen 0,49" OLED-Display, begleitet von drei unabhängig voneinander blinkenden Status-LEDs.

![Vorschau des Bitcoin Tickers](./ESP32-C3-Satellite/ESP32-C3-Satellite-Block.png)


## ✨ Features

- **Ultrakompaktes UI:** Pixelgenaue Anpassung an ein Hailege 0,49" OLED-Display (64x32 Pixel) unter Verwendung der ressourcenschonenden `SSD1306Wire`-Bibliothek.
- **Multitasking LED-Muster:** Komplett unblockierte Steuerung über Zeitstempel (`millis()`) – keine Verzögerungen bei der Datenanzeige.
- **Live Blockchain-Metriken:** Automatische Rotation zwischen Bitcoin-Preis (EUR/USD), prozentualer 24h-Änderung, aktueller Blockzeit, Live-Mempool-Gebühren und einem dynamisch gezeichneten 16-Minuten-Preistrend (Line-Chart).
- **WiFiManager mit RF-Schutz:** Komfortables Einrichten des WLANs über ein Web-Portal. Die Sendeleistung der internen Antenne ist softwareseitig aggressiv gedrosselt (`8.5dBm`), um elektromagnetische Interferenzen (RF-Noise) mit den dichten Messing-Drahtbahnen zu verhindern.
- **Hardware-WLAN-Reset:** Durch Halten des integrierten `BOOT`-Buttons (GPIO 9) für 3 Sekunden beim Starten wird der interne NVS-WLAN-Speicher gelöscht, um den Satelliten mobil in anderen Netzwerken anzumelden.

## 📌 Hardware-Pinout (ESP32-C3 SuperMini)

| Komponente | Pin (GPIO) | Beschreibung / Logik |
| :--- | :---: | :--- |
| **OLED SDA** | `21` | I2C Datenleitung zum 0,49" Display |
| **OLED SCL** | `20` | I2C Taktleitung zum 0,49" Display |
| **LED Gelb** | `3` | System-Aktivität: Blinkt schnell im 600ms-Takt (leuchtet statisch bei API-Abruf) |
| **LED Grün** | `2` | Puls-Anzeige: Blinkt ruhig im langsamen 150ms-Takt |
| **LED Blau** | `8` | **Fee-Alarm:** Blinkt asynchron im 500ms-Takt, sobald die Gebühren `<= 5 sat/vB` fallen (Nutzt Active-Low-Logik parallel zur Onboard-LED) |
| **BOOT-Button**| `9` | Halten beim Einschalten löscht die WLAN-Daten |

## 🛠️ Abhängigkeiten (Arduino IDE Libraries)

Für das Kompilieren werden folgende Bibliotheken benötigt:
- `ESP32-SSD1306-WebThing` (bzw. ThingPulse `ESP8266 and ESP32 OLED driver for SSD1306 displays`)
- `ArduinoJson` (Optimiert für Version 7+)
- `WiFiManager` (von tablatronix)

> ⚠️ **Wichtig für die Arduino IDE:** Setze unter *Werkzeuge -> PSRAM* den Wert zwingend auf **`Disabled`**, um den Netzwerk-Stack des C3-Chips stabil zu halten.


---

## ⚙️ Netzwerk-Konfiguration (WiFiManager vs. Diebstahlschutz)

### 1. Für die WiFiManager-Editionen (Wroom & C3-Mini):
Sollte der Ticker kein Netzwerk finden, öffnet er den Access Point `Wroom-BTC-Ticker-AP` oder `C3-Ticker-AP`. 
1. Verbinde dein Handy mit dem WLAN.
2. Rufe im Browser `192.168.4.1` auf.
3. Tippe dein WLAN-Passwort ein. 
*   **Hardware-Reset:** Halte beim Einschalten den **BOOT-Button** (Wroom: GPIO 0 / C3: GPIO 9) für 3 Sekunden gedrückt, um den Speicher manuell zu löschen.

### 2. Für die Diebstahlsicheren SD-Editionen (CAM & Satellit):
Erstelle eine ganz normale, sichtbare Textdatei namens `wifi.txt` im Hauptverzeichnis deiner MicroSD-Karte (FAT32 formatiert):
```text
DEINE_WLAN_SSID
DEIN_WLAN_PASSWORT
```
*   **Der Sicherheits-Ablauf:** Beim ersten Start liest der Ticker die Karte aus, brennt die Daten fest in den internen, geschützten Speicher (NVS) des ESP32-Chips und **löscht die `wifi.txt` sofort vollständig von der SD-Karte**. 
*   **WLAN wechseln:** Um neue Daten einzuspeisen, erstelle am PC einfach eine neue `wifi.txt` auf der Karte. Der Ticker überschreibt beim nächsten Booten den alten internen Speicher und löscht die Datei wieder.

---

## ✨ Features
- 💰 **Preise:** Live-Kurse in EUR und USD vollkommen schlüssellos direkt von der Open-Source-Plattform `mempool.space`.
- 📉 **Trend-Charts:** Dynamische Kurven-Anzeige mit **Autoscale-Algorithmus (5% Padding)**, damit die Kurve niemals an den oberen oder unteren Displayrand anstößt oder abgeschnitten wird.
- ⛓️ **Blockchain:** Große, unübersehbare Anzeige der aktuellen Blockhöhe ("Bitcoin-Weltzeit").
- 🚦 **Mempool:** Aktuelle, empfohlene On-Chain-Gebühren (Fast/Med/Slow) direkt von `mempool.space`.
- 🕒 **NTP:** Automatische Uhrzeitsynchronisation im Hintergrund.
- 💡 **Low-Fee-Alert:** Onboard LED leuchtet dauerhaft, sobald die Gebühren unter <= 5 sat/vB fallen.

## 📚 Benötigte Bibliotheken
Folgende Libraries müssen im Bibliotheksverwalter der Arduino IDE installiert sein:
- `ESP8266 and ESP32 OLED driver for SSD1306` (Für Wroom, CAM und C3-Satellit)
- `U8g2` (Spezifisch für die 72x40 C3 Mini-Version)
- `WiFiManager` by tzapu (Für die schlüssellosen Web-Setup-Versionen)
- `ArduinoJson` (Benoit Blanchon, V7-Standard empfohlen)
- `Preferences` & `Wire` & `SD_MMC` (Integrierte ESP32 Core-Bibliotheken)

---
Erstellt mit ❤️ für die Bitcoin- und Bastler-Community.
