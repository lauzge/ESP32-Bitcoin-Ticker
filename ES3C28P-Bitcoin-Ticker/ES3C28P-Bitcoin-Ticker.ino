#include <lvgl.h>
#include <WiFiManager.h> 
#include "config.h"
#include "gui_lvgl.h" 
#include "wifi_manager.h"
#include "api_mempool.h"
#include "web_server.h"

BitcoinData btc;
Config sysConfig;
LGFX_ES3C28P lcd;

AsyncWebServer server(80);
DNSServer dnsServer;
Preferences preferences;

unsigned long lastUpdate = -45000; 
unsigned long lastSideSwitch = 0;
const long interval = 45000; 

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- Blocktrainer Terminal startet ---");
    const int BUTTON_PIN = 0; pinMode(BUTTON_PIN, INPUT_PULLUP);

    initLVGL();

    if (digitalRead(BUTTON_PIN) == LOW) {
        lv_label_set_text(main_price_label, "RESET MODUS");
        lv_label_set_text(currency_symbol_label, "Knopf 3 Sek halten...");
        lv_timer_handler(); delay(3000); 
        if (digitalRead(BUTTON_PIN) == LOW) {
            WiFiManager wm; wm.resetSettings(); ESP.restart();
        }
    }

    initWiFi();
    triggerScreenClear(); lv_obj_invalidate(lv_screen_active()); lv_timer_handler(); 

    initWebServer(); server.begin();
    initTime();
    updateMempoolData(); updateTimeStrings();
    
    triggerScreenClear(); lv_obj_invalidate(lv_screen_active());
}

void loop() {
    updateTimeStrings();
    if (sysConfig.screenNeedsClear) {
        triggerScreenClear(); lv_obj_invalidate(lv_screen_active());
        sysConfig.screenNeedsClear = false;
    }
    updateGUI();

    if (sysConfig.currentSide != 5 && sysConfig.autoRotate && (millis() - lastSideSwitch > 10000)) {
        sysConfig.currentSide++;
        if(sysConfig.currentSide > 4) sysConfig.currentSide = 0; 
        triggerScreenClear(); lv_obj_invalidate(lv_screen_active()); 
        lastSideSwitch = millis();
    }

    if (millis() - lastUpdate > interval) {
        updateMempoolData(); lastUpdate = millis();
    }
    delay(5);
}
