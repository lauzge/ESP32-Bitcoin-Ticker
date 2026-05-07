# 🚀 ESP32 Bitcoin Live Ticker v1.1

Ein multifunktionaler Bitcoin-Ticker für den ESP32 mit integriertem OLED-Display (SSD1306). Das Gerät wechselt automatisch zwischen Preis-Informationen, Netzwerk-Statistiken und einer großen Blockzeit-Anzeige.

## ✨ Neue Features in v1.1
- 🕒 **NTP Uhrzeit**: Automatische Synchronisierung der Echtzeit (HH:MM).
- ⛓️ **Große Blockzeit**: Exklusive Ansicht der aktuellen Blockhöhe.
- 🚦 **Mempool-Alarm**: Die onboard LED leuchtet dauerhaft, wenn die Gebühren niedrig sind (<= 5 sat/vB).
- 📊 **Detaillierter Mempool**: Anzeige der empfohlenen Gebühren (Fast, Medium, Slow) in sat/vB.
- 📉 **Trend & Prozent**: Anzeige der Preisveränderung seit dem letzten Abruf.

## 🛠 Hardware
- **Board**: ESP32 Wroom (z.B. DevKit V1)
- **Display**: Integriertes 0.96" OLED (SSD1306)
- **Pins**: SDA (Pin 21), SCL (Pin 22), Onboard LED (Pin 2)

![Vorschau des Bitcoin Tickers](ESP32-Bitcoin-Ticker.png)

## 📡 APIs & Bibliotheken
- **Preise**: [CryptoCompare](https://cryptocompare.com)
- **Blockchain-Daten**: [mempool.space](https://mempool.space)
- **Bibliotheken**: 
  - `SSD1306Wire` (ThingPulse)
  - `ArduinoJson`
  - `HTTPClient` & `WiFiClientSecure`

## 🚀 Installation & Setup
1. Repository klonen.
2. WLAN-Zugangsdaten in der `.ino` Datei anpassen.
3. In der Arduino IDE das passende Board wählen und hochladen.

## 🖥️ Display-Rotation
Das Display wechselt alle 5 Sekunden zwischen:
1. **BTC/EUR** (inkl. Uhrzeit & %-Änderung)
2. **BTC/USD** (inkl. Uhrzeit & %-Änderung)
3. **Blockzeit** (Großansicht der Blockhöhe)
4. **Mempool Fees** (sat/vB für verschiedene Prioritäten)

---
Erstellt mit ❤️ für die Bitcoin-Community.
