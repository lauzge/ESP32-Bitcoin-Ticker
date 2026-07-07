#pragma once
#include "config.h"
#include <lvgl.h>

extern lv_obj_t * calc_lbl_sats;
extern lv_obj_t * calc_lbl_eur;
extern lv_obj_t * calc_lbl_usd;

struct CalcResults {
    double btc = 0.0;
    double usd = 0.0;
    double eur = 0.0;
};

CalcResults calculateSats() {
    CalcResults res;
    if (btc.priceUSD > 0 && btc.priceEUR > 0) {
        res.btc = (double)btc.calcSats / 100000000.0;
        res.usd = res.btc * (double)btc.priceUSD;
        res.eur = res.btc * (double)btc.priceEUR;
    }
    return res;
}

// Setzt die echten Live-Marktpreise als Startwerte in den Rechner
void loadCalcInitialValues() {
    static bool initialValuesLoaded = false;
    if (!initialValuesLoaded && btc.priceEUR > 0 && btc.priceUSD > 0) {
        sysConfig.calcValueSats = 100000000.0; // Fixer Wunsch-Startwert: 100 Mio.
        sysConfig.calcValueEUR = (double)btc.priceEUR; // Echter aktueller Euro-Kurs!
        sysConfig.calcValueUSD = (double)btc.priceUSD; // Echter aktueller Dollar-Kurs!
        initialValuesLoaded = true;
        Serial.println("Rechner-Startwerte erfolgreich geladen!");
    }
}

void updateCalcFieldsOnScreen() {
    if (calc_lbl_sats == NULL) return;
    
    // REPARIERT: Nutzt nun reine String-Konvertierung anstelle von %.2f, was das "f" eliminiert!
    if (sysConfig.calcSelectedRow == 0 && sysConfig.calcInputBuffer.length() > 0) {
        String displayStr = sysConfig.calcInputBuffer;
        displayStr.replace(".", ",");
        lv_label_set_text(calc_lbl_sats, displayStr.c_str());
    } else {
        lv_label_set_text(calc_lbl_sats, String(sysConfig.calcValueSats, 0).c_str());
    }

    if (sysConfig.calcSelectedRow == 1 && sysConfig.calcInputBuffer.length() > 0) {
        String displayStr = sysConfig.calcInputBuffer;
        displayStr.replace(".", ",");
        lv_label_set_text(calc_lbl_eur, displayStr.c_str());
    } else {
        lv_label_set_text(calc_lbl_eur, String(sysConfig.calcValueEUR, 2).c_str());
    }

    if (sysConfig.calcSelectedRow == 2 && sysConfig.calcInputBuffer.length() > 0) {
        String displayStr = sysConfig.calcInputBuffer;
        displayStr.replace(".", ",");
        lv_label_set_text(calc_lbl_usd, displayStr.c_str());
    } else {
        lv_label_set_text(calc_lbl_usd, String(sysConfig.calcValueUSD, 2).c_str());
    }
}

void executeCalculation() {
    if (btc.priceUSD <= 0 || btc.priceEUR <= 0) return;

    if (sysConfig.calcInputBuffer.length() > 0) {
        sysConfig.calcInputBuffer.replace(",", ".");
        double typedValue = sysConfig.calcInputBuffer.toDouble();
        
        if (sysConfig.calcSelectedRow == 0) sysConfig.calcValueSats = typedValue;
        else if (sysConfig.calcSelectedRow == 1) sysConfig.calcValueEUR = typedValue;
        else if (sysConfig.calcSelectedRow == 2) sysConfig.calcValueUSD = typedValue;
        
        sysConfig.calcInputBuffer = ""; 
    }

    // Exakte Dreisatz-Berechnung
    if (sysConfig.calcSelectedRow == 0) { 
        double btcVal = sysConfig.calcValueSats / 100000000.0;
        sysConfig.calcValueEUR = btcVal * (double)btc.priceEUR;
        sysConfig.calcValueUSD = btcVal * (double)btc.priceUSD;
    } 
    else if (sysConfig.calcSelectedRow == 1) { 
        double btcVal = sysConfig.calcValueEUR / (double)btc.priceEUR;
        sysConfig.calcValueSats = btcVal * 100000000.0;
        sysConfig.calcValueUSD = btcVal * (double)btc.priceUSD;
    } 
    else if (sysConfig.calcSelectedRow == 2) { 
        double btcVal = sysConfig.calcValueUSD / (double)btc.priceUSD;
        sysConfig.calcValueSats = btcVal * 100000000.0;
        sysConfig.calcValueEUR = btcVal * (double)btc.priceEUR;
    }
    
    updateCalcFieldsOnScreen();
}

void handleCalcKeyPress(const char* btnText) {
    if (strcmp(btnText, "NEXT") == 0) {
        executeCalculation(); 
        sysConfig.calcSelectedRow++;
        if (sysConfig.calcSelectedRow > 2) sysConfig.calcSelectedRow = 0;
        sysConfig.calcInputBuffer = "";
    } 
    else if (strcmp(btnText, "C") == 0) {
        sysConfig.calcInputBuffer = "";
    } 
    else {
        if (strcmp(btnText, ",") == 0) {
            if (sysConfig.calcInputBuffer.indexOf('.') == -1) sysConfig.calcInputBuffer += ".";
        } else {
            sysConfig.calcInputBuffer += String(btnText);
        }
    }
    updateCalcFieldsOnScreen();
}
