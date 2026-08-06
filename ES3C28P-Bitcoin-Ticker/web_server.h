#pragma once
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <WiFi.h> // WICHTIG: Fuer den direkten Zugriff auf den Hardware-WLAN-Speicher
#include "config.h"
#include "api_calculator.h"

extern AsyncWebServer server;
extern Preferences preferences;

void initWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("/dashboard");
    });

    server.on("/dashboard", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<html><head><title>Bitcoin Ticker Dashboard</title><style>";
        html += "body{font-family:Arial; background:#121212; color:#fff; text-align:center; padding-top:20px;}";
        html += ".card{background:#1e1e1e; padding:15px; margin:10px; display:inline-block; border-radius:10px; width:220px; box-shadow:0 4px 8px rgba(0,0,0,0.5); text-align:left;}";
        html += "input[type=submit], select, input[type=number]{padding:8px; border-radius:5px; border:none; margin:5px; font-size:14px; background:#2A2A2A; color:#fff;}";
        html += "input[type=submit]{background:#f2a900; color:#fff; font-weight:bold; cursor:pointer;}";
        html += ".btn-danger{background:#d9534f !important;}";
        html += "form{background:#1e1e1e; max-width:400px; margin:15px auto; padding:15px; border-radius:10px; text-align:left;}";
        html += "</style></head><body><h1>Bitcoin Ticker Control Center</h1>";
        html += "<div class='card'><h3>Live Preise</h3><p>USD: " + String(btc.priceUSD) + " $</p><p>EUR: " + String(btc.priceEUR) + " &euro;</p></div>";
        html += "<div class='card'><h3>Blockchain</h3><p>Block: " + String(btc.blockHeight) + "</p><p>Moscow: " + String(btc.moscowTimeEUR) + " Sat</p></div>";
        
        CalcResults c = calculateSats();
        html += "<div class='card'><h3>Sats Rechner</h3><p>Sats: " + String(btc.calcSats) + "</p><p>EUR: " + String(c.eur, 2) + " &euro;</p></div>";
        html += "<form action='/settings' method='POST'><h3>System-Einstellungen</h3>";
        html += "Seitenwechsel: <select name='rotate'><option value='1'" + String(sysConfig.autoRotate ? " selected" : "") + ">Ein (10s)</option><option value='0'" + String(!sysConfig.autoRotate ? " selected" : "") + ">Aus</option></select><br>";
        html += "Chart Fenster: <select name='interval'><option value='0'" + String(sysConfig.chartInterval == 0 ? " selected" : "") + ">24h</option><option value='1'" + String(sysConfig.chartInterval == 1 ? " selected" : "") + ">3d</option><option value='2'" + String(sysConfig.chartInterval == 2 ? " selected" : "") + ">5d</option></select><br>";
        html += "Chart Waehrung: <select name='currency'><option value='0'" + String(sysConfig.chartCurrency == 0 ? " selected" : "") + ">EUR</option><option value='1'" + String(sysConfig.chartCurrency == 1 ? " selected" : "") + ">USD</option></select><br><br><input type='submit' value='System Speichern'></form>";
        html += "<br><form action='/reset_wifi' method='POST'><input type='submit' class='btn-danger' value='WLAN loeschen & neu starten'></form></body></html>";
        request->send(200, "text/html", html);
    });

    server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
        if(request->hasArg("rotate")) sysConfig.autoRotate = (request->arg("rotate") == "1");
        if(request->hasArg("interval")) sysConfig.chartInterval = request->arg("interval").toInt();
        if(request->hasArg("currency")) sysConfig.chartCurrency = request->arg("currency").toInt();
        sysConfig.screenNeedsClear = true; 
        request->redirect("/dashboard");
    });

    server.on("/reset_wifi", HTTP_POST, [](AsyncWebServerRequest *request){
        // KORREKTUR: Loescht erst unsere Preferences und radiert danach die echten Hardware-WLAN-Daten aus dem NVS-Flash!
        preferences.begin("wifi_cfg", false); preferences.clear(); preferences.end();
        
        // Parameter 1: Verbindung kappen | Parameter 2: Gespeicherte Router-Daten aus dem Flash restlos entfernen
        WiFi.disconnect(true, true); 
        
        request->send(200, "text/html", "<h3>WLAN restlos geloescht! ESP32-S3 startet neu im AP-Modus...</h3>");
        delay(2000); 
        ESP.restart();
    });
}
