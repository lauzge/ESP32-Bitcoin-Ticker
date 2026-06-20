# 🚀 ESP32 Bitcoin Live Ticker - Multifunktions-Edition

Dieses Open-Source-Projekt zeigt Bitcoin-Echtzeitdaten auf verschiedenen OLED- und IPS-Displays an. Die Ticker bieten Live-Kurse, prozentuale Tendenzen, die aktuelle Blockzeit sowie detaillierte Mempool-Gebührenwächter.

## 📂 Repository Struktur

Das Repository ist in vier spezialisierte Hardware-Versionen unterteilt:

### 1. [ESP32-Wroom Ticker (WiFi-Manager Edition)](./ESP32-WROOM-Oled-Bitcoin-Ticker-SD-Card)
Für Standard ESP32-Boards mit I2C-OLED (SDA: 21, SCL: 22). 
*   **Besonderheit:** Komplett auf **WiFiManager** umgestellt! Es sind keine festen WLAN-Daten im Code nötig. Der Ticker holt die Preise schlüssellos direkt von `mempool.space` und zeichnet einen fortlaufenden Autoscale-Live-Chart der letzten 32 Minuten.

### 2. [ESP32-CAM-Version](./ESP32-CAM-Bitcoin-Ticker-SD-Card)
Optimiert für das ultrakompakte ESP32-CAM Board mit integriertem MicroSD-Slot.
*   **Besonderheit:** Nutzt den SD_MMC Bus im 1-Bit Modus und die versteckte Datei `/.wifi.txt` zur Konfiguration. Preise werden schlüssellos von `mempool.space` abgerufen.
*   **Display-Pins:** SDA: GPIO 13, SCL: GPIO 12

### 3. [ESP32-C3-Version (Messing-Satellit / SPI-SD)](./ESP32-C3-Ext-Oled-Ext-SD-Cardreader-Bitcoin-Ticker)
Optimiert für das stromsparende ESP32-C3-Board mit externem SD-Kartenleser.
*   **Besonderheit:** Hardware-SPI-Bus-Trennung zur Vermeidung von Datenkonflikten. Nutzt ebenfalls die verschlüsselte/versteckte `/.wifi.txt`.
*   **Display-Pins:** SDA: GPIO 10, SCL: GPIO 21

### 4. [ESP32-C3 Mini-Version (WiFi-Manager Edition)](./ESP32-C3-OLED-72x40)
Ultraminimale Standalone-Version ohne SD-Kartenleser für den Schreibtisch. Nutzt ein winziges 72x40 Pixel OLED-Display mit maximaler Informationsdichte.
*   **Besonderheit:** Komplett auf **WiFiManager** umgestellt! Zeigt bei fehlender Verbindung eine bequeme Handy-Anleitung im Webinterface-Modus an. Bietet eine integrierte NTP-Uhrzeit sowie einen pixelgenauen Autoscale-Trend-Chart der letzten 18 Minuten direkt von `mempool.space`.
*   **Display-Pins:** SDA: GPIO 5, SCL: GPIO 6

---

## ⚙️ Netzwerk-Konfiguration (WiFiManager vs. SD-Karte)

### Für die WiFiManager-Editionen (Wroom & C3-Mini):
Sollte der Ticker kein bekanntes Netzwerk finden, öffnet er automatisch einen eigenen Access Point (z. B. `C3-Ticker-AP` oder `Wroom-Ticker-AP`). 
1. Verbinde dein Smartphone mit diesem unverschlüsselten WLAN.
2. Öffne im Browser die IP-Adresse `192.168.4.1`.
3. Wähle dein Heim-WLAN aus, tippe das Passwort ein und speichere. Der ESP32 sichert die Daten dauerhaft im internen NVS-Speicher.

### Für die klassischen Versionen mit SD-Karte (CAM & C3-Satellit):
Erstelle eine versteckte Datei namens `.wifi.txt` im Hauptverzeichnis deiner MicroSD-Karte (FAT32 formatiert).
```text
DEINE_WLAN_SSID
DEIN_WLAN_PASSWORT
```

---

## ✨ Features
- 💰 **Preise:** Live-Kurse in EUR und USD schlüssellos direkt von der Open-Source-Plattform `mempool.space`.
- 📉 **Trend-Charts:** Fortlaufend gezeichnete Live-Kurven auf dem OLED mit intelligentem 5%-Polster (Autoscale), damit die Kurve niemals oben oder unten flachdrückt.
- ⛓️ **Blockchain:** Große, unübersehbare Anzeige der aktuellen Blockhöhe.
- 🚦 **Mempool:** Aktuelle, empfohlene On-Chain-Gebühren (Fast/Med/Slow).
- 🕒 **NTP:** Automatische Uhrzeitsynchronisation im Hintergrund.
- 💡 **Low-Fee-Alert:** Onboard LED leuchtet als Indikator, sobald die Gebühren unter <= 5 sat/vB fallen.

## 📚 Benötigte Bibliotheken
Folgende Libraries müssen im Bibliotheksverwalter der Arduino IDE installiert sein:
- `ESP8266 and ESP32 OLED driver for SSD1306` (Für Wroom, CAM und C3-Satellit)
- `U8g2` (Spezifisch für die 72x40 C3 Mini-Version)
- `WiFiManager` by tzapu (Für die schlüssellosen Web-Setup-Versionen)
- `ArduinoJson` (Benoit Blanchon, V7-Standard empfohlen)

---
Erstellt mit ❤️ für die Bitcoin- und Bastler-Community.
