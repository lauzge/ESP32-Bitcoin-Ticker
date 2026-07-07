#pragma once
#include <WiFi.h>
#include <WiFiManager.h> 
#include <lvgl.h>

extern lv_obj_t * main_price_label;
extern lv_obj_t * currency_symbol_label;

void initWiFi() {
    WiFiManager wm;
    WiFi.setTxPower(WIFI_POWER_8_5dBm); 

    wm.setSaveConfigCallback([](){
        Serial.println("WLAN-Daten empfangen. Starte Verbindung...");
    });

    wm.setAPCallback([](WiFiManager *myWiFiManager) {
        Serial.println("Kein bekanntes WLAN! Geoeffneter AP aktiv.");
        lv_label_set_text(main_price_label, "KEIN WLAN!");
        lv_label_set_text(currency_symbol_label, "Handy verbinden mit:\n'ES3C28P-BTC-Ticker-AP'\n\nBrowser: 192.168.4.1");
        lv_timer_handler();
    });

    if (!wm.autoConnect("ES3C28P-BTC-Ticker-AP")) {
        Serial.println("Verbindung fehlgeschlagen.");
        lv_label_set_text(main_price_label, "FEHLER!");
        lv_label_set_text(currency_symbol_label, "Neustart in 3 Sekunden...");
        lv_timer_handler();
        delay(3000);
        ESP.restart();
    }

    Serial.println("WLAN erfolgreich verbunden!");
    lv_label_set_text(main_price_label, "WLAN OK");
    lv_label_set_text(currency_symbol_label, "Lade Bitcoin Terminal...");
    lv_timer_handler();
    delay(1000);
}
