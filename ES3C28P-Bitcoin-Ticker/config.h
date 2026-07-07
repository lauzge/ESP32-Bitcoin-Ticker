#pragma once
#include <Arduino.h>

struct BitcoinData {
    long priceUSD = 0;
    long priceEUR = 0;
    float changeUSD = 0.0;
    float changeEUR = 0.0;
    
    long blockHeight = 0;
    int lowFee = 0;
    int medFee = 0;
    int highFee = 0;
    
    long moscowTimeEUR = 0; 
    long difficultyBlocks = 0;
    float difficultyChange = 0.0;
    
    // REPARIERT: Feste Array-Zuweisung mit Speicherplaetzen
    long history24h[24]; int count24h = 0;
    long history3d[24];  int count3d = 0;
    long history5d[24];  int count5d = 0;
    
    String timeStr = "00:00";
    String dateStr = "01.01.2026";
    long calcSats = 100000; 
};

extern BitcoinData btc;

struct Config {
    bool autoRotate = false;
    bool nightMode = false;
    int currentSide = 0; 
    long priceAlarmUSD = 0; 
    int chartInterval = 0;  
    int chartCurrency = 0;  
    bool screenNeedsClear = false; 

    int calcSelectedRow = 0;       
    double calcValueSats = 100000000.0; 
    double calcValueEUR = 0.0;
    double calcValueUSD = 0.0;
    String calcInputBuffer = "";   
};

extern Config sysConfig;
