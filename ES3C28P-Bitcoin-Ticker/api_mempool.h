#pragma once
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"

long initialPriceUSD = 0;
long initialPriceEUR = 0;

void initTime() {
    configTime(3600, 3600, "pool.ntp.org"); 
}

void updateTimeStrings() {
    time_t now;
    struct tm *timeinfo;
    time(&now);
    timeinfo = localtime(&now);

    int hour = timeinfo->tm_hour;
    int min  = timeinfo->tm_min;
    int day  = timeinfo->tm_mday;
    int month = timeinfo->tm_mon + 1;
    int year  = timeinfo->tm_year + 1900;
    int wday  = timeinfo->tm_wday;

    String hStr = (hour < 10) ? "0" + String(hour) : String(hour);
    String mStr = (min < 10) ? "0" + String(min) : String(min);
    String dStr = (day < 10) ? "0" + String(day) : String(day);
    String moStr = (month < 10) ? "0" + String(month) : String(month);
    
    btc.timeStr = hStr + ":" + mStr;

    String tage[] = {"SONNTAG", "MONTAG", "DIENSTAG", "MITTWOCH", "DONNERSTAG", "FREITAG", "SAMSTAG"};
    btc.dateStr = tage[wday] + ", " + dStr + "." + moStr + "." + String(year);
}

void updateMempoolData() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        JsonDocument doc;
        
        // 1. PREISE ABFRAGEN
        http.begin("https://mempool.space/api/v1/prices");
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            deserializeJson(doc, http.getString());
            btc.priceUSD = doc["USD"];
            btc.priceEUR = doc["EUR"];
            
            if(btc.priceEUR > 0) btc.moscowTimeEUR = 100000000 / btc.priceEUR;
            if (initialPriceUSD == 0) initialPriceUSD = btc.priceUSD;
            if (initialPriceEUR == 0) initialPriceEUR = btc.priceEUR;
            
            if (initialPriceUSD > 0) btc.changeUSD = ((float)(btc.priceUSD - initialPriceUSD) / initialPriceUSD) * 100.0;
            if (initialPriceEUR > 0) btc.changeEUR = ((float)(btc.priceEUR - initialPriceEUR) / initialPriceEUR) * 100.0;
            
            // Waehlt den Zielpreis fuer den Chart basierend auf den Einstellungen aus
            long targetPrice = (sysConfig.chartCurrency == 0) ? btc.priceEUR : btc.priceUSD;
            
            // Befuellt das 24h Chart-Array sauber mit Index
            if (btc.count24h < 24) { 
                btc.history24h[btc.count24h] = targetPrice; 
                btc.count24h++; 
            } else { 
                for(int i = 0; i < 23; i++) btc.history24h[i] = btc.history24h[i + 1]; 
                btc.history24h[23] = targetPrice; 
            }

            // Befuellt das 3-Tage Chart-Array mit Index
            if (btc.count3d < 24) { 
                btc.history3d[btc.count3d] = targetPrice; 
                btc.count3d++; 
            } else { 
                for(int i = 0; i < 23; i++) btc.history3d[i] = btc.history3d[i + 1]; 
                btc.history3d[23] = targetPrice; 
            }

            // Befuellt das 5-Tage Chart-Array mit Index
            if (btc.count5d < 24) { 
                btc.history5d[btc.count5d] = targetPrice; 
                btc.count5d++; 
            } else { 
                for(int i = 0; i < 23; i++) btc.history5d[i] = btc.history5d[i + 1]; 
                btc.history5d[23] = targetPrice; 
            }
        }
        http.end();

        // 2. GEBUEHREN (FEES)
        http.begin("https://mempool.space/api/v1/fees/recommended");
        if (http.GET() == HTTP_CODE_OK) {
            deserializeJson(doc, http.getString());
            btc.lowFee = doc["hourFee"];
            btc.medFee = doc["halfHourFee"];
            btc.highFee = doc["fastestFee"];
        }
        http.end();

        // 3. BLOCKHOEHE
        http.begin("https://mempool.space/api/blocks/tip/height");
        if (http.GET() == HTTP_CODE_OK) btc.blockHeight = http.getString().toInt();
        http.end();

        // 4. DIFFICULTY
        http.begin("https://mempool.space/api/v1/difficulty-adjustment");
        if (http.GET() == HTTP_CODE_OK) {
            deserializeJson(doc, http.getString());
            btc.difficultyBlocks = doc["remainingBlocks"];
            btc.difficultyChange = doc["difficultyChange"];
        }
        http.end();
    }
} // KORREKTUR: Diese schließende Klammer hat gefehlt und alle Fehler ausgelöst!
