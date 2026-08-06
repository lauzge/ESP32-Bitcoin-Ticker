#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "config.h"

void handleCalcKeyPress(const char* btnText);
void executeCalculation();
void updateCalcFieldsOnScreen();
void loadCalcInitialValues();

class LGFX_ES3C28P : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Light_PWM     _light_instance;
  lgfx::Touch_FT5x06  _touch_instance; 
public:
  LGFX_ES3C28P() {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST; cfg.spi_mode = 0; cfg.freq_write = 40000000;
      cfg.pin_sclk = 12; cfg.pin_mosi = 11; cfg.pin_miso = 13; cfg.pin_dc = 46;
      _bus_instance.config(cfg); _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 10; cfg.pin_rst = -1;
      cfg.memory_width = 240; cfg.memory_height = 320;
      cfg.panel_width = 240; cfg.panel_height = 320;
      cfg.offset_x = 0; cfg.offset_y = 0;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 45; cfg.freq = 44100; cfg.pwm_channel = 1;
      _light_instance.config(cfg); _panel_instance.setLight(&_light_instance);
    }
    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 0; cfg.x_max = 240; cfg.y_min = 0; cfg.y_max = 320;
      cfg.pin_sda = 16; cfg.pin_scl = 15; cfg.i2c_port = 1; cfg.freq = 400000;
      _touch_instance.config(cfg); _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

extern LGFX_ES3C28P lcd;

// Globale UI-Komponenten
lv_obj_t * main_price_label;
lv_obj_t * currency_symbol_label;
lv_obj_t * btc_logo_label;
lv_obj_t * percentage_label;
lv_obj_t * block_title_label;
lv_obj_t * bottom_time_label;
lv_obj_t * bottom_date_label;
lv_obj_t * terminal_line; 

lv_obj_t * clock_hour_label = NULL;
lv_obj_t * clock_colon_label = NULL;
lv_obj_t * clock_min_label = NULL;

lv_obj_t * chart_time_box;
lv_obj_t * chart_time_label;
lv_obj_t * chart_top_time_label;
lv_obj_t * chart_top_date_label;
lv_obj_t * chart_top_price_label;

lv_obj_t * calc_container = NULL;   
lv_obj_t * calc_btn_sats;
lv_obj_t * calc_btn_eur;
lv_obj_t * calc_btn_usd;

lv_obj_t * calc_lbl_sats = NULL;
lv_obj_t * calc_lbl_eur = NULL;
lv_obj_t * calc_lbl_usd = NULL;

// REPARIERT: Echtes LVGL 9 Chart-Objekt mitsamt Datenreihe deklariert
lv_obj_t * chart_obj = NULL;
lv_chart_series_t * chart_series = NULL;
lv_obj_t * chart_max_price_lbl = NULL;
lv_obj_t * chart_min_price_lbl = NULL;

void my_disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.writePixels((uint16_t *)px_map, w * h, true);
    lcd.endWrite();
    lv_display_flush_ready(disp);
}

// REPARIERT: Invertiert und spiegelt die Achsen, damit der Touch perfekt zum Bild passt!
void my_touch_read(lv_indev_t * indev, lv_indev_data_t * data) {
    uint16_t touchX, touchY;
    if (lcd.getTouch(&touchX, &touchY)) {
        data->state = LV_INDEV_STATE_PRESSED;
        
        // MATHEMATISCHE ACHSEN-KORREKTUR FUER DAS QUERFORMAT DES ES3C28P
        // Dreht die x-Achse um und spiegelt die y-Achse von unten nach oben
        data->point.x = 320 - touchX;
        data->point.y = 240 - touchY;
        
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Wischgesten-Callback fuer alle verbleibenden Seiten
static void gesture_event_cb(lv_event_t * e) {
    if(calc_container != NULL && !lv_obj_has_flag(calc_container, LV_OBJ_FLAG_HIDDEN)) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if(dir == LV_DIR_LEFT) {
        sysConfig.currentSide++;
        if(sysConfig.currentSide > 5) sysConfig.currentSide = 0; 
        lcd.fillScreen(0x000000); 
        lv_obj_invalidate(lv_screen_active());
        Serial.println("Geste: Nach links gewischt");
    }
    else if(dir == LV_DIR_RIGHT) {
        sysConfig.currentSide--;
        if(sysConfig.currentSide < 0) sysConfig.currentSide = 5;
        lcd.fillScreen(0x000000); 
        lv_obj_invalidate(lv_screen_active());
        Serial.println("Geste: Nach rechts gewischt");
    }
}

void initLVGL() {
    lcd.init();
    lcd.setRotation(1); 
    lcd.invertDisplay(true); 
    lcd.fillScreen(0x000000);

    lv_init();

    static uint8_t buf1[320 * 240 / 10 * 2];
    lv_display_t * disp = lv_display_create(320, 240);
    lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_disp_flush);

    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touch_read);

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); 
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(scr, gesture_event_cb, LV_EVENT_GESTURE, NULL);

    main_price_label = lv_label_create(scr);
    lv_label_set_text(main_price_label, "");
    lv_obj_align(main_price_label, LV_ALIGN_CENTER, 0, -25);
    lv_obj_set_style_text_font(main_price_label, &lv_font_montserrat_40, 0);   

    static lv_point_precise_t line_points[] = { {15, 130}, {305, 130} };
    terminal_line = lv_line_create(scr);
    lv_line_set_points(terminal_line, line_points, 2);
    lv_obj_set_style_line_color(terminal_line, lv_color_hex(0x222222), 0); 
    lv_obj_set_style_line_width(terminal_line, 2, 0);

    currency_symbol_label = lv_label_create(scr);
    lv_label_set_text(currency_symbol_label, "");
    lv_obj_align(currency_symbol_label, LV_ALIGN_CENTER, 0, 30); 
    lv_obj_set_style_text_font(currency_symbol_label, &lv_font_montserrat_14, 0); 

    percentage_label = lv_label_create(scr);
    lv_label_set_text(percentage_label, "");
    lv_obj_align(percentage_label, LV_ALIGN_CENTER, 55, 30); 
    lv_obj_set_style_text_font(percentage_label, &lv_font_montserrat_14, 0);

    block_title_label = lv_label_create(scr);
    lv_label_set_text(block_title_label, "");
    lv_obj_set_style_text_color(block_title_label, lv_color_hex(0x888888), 0); 
    lv_obj_set_style_text_font(block_title_label, &lv_font_montserrat_14, 0);

    bottom_time_label = lv_label_create(scr);
    lv_label_set_text(bottom_time_label, "");
    lv_obj_align(bottom_time_label, LV_ALIGN_BOTTOM_RIGHT, -10, -18);
    lv_obj_set_style_text_color(bottom_time_label, lv_color_hex(0x444444), 0); 
    lv_obj_set_style_text_font(bottom_time_label, &lv_font_montserrat_10, 0); 

    bottom_date_label = lv_label_create(scr);
    lv_label_set_text(bottom_date_label, "");
    lv_obj_align(bottom_date_label, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
    lv_obj_set_style_text_color(bottom_date_label, lv_color_hex(0x444444), 0);
    lv_obj_set_style_text_font(bottom_date_label, &lv_font_montserrat_10, 0);

    chart_time_box = lv_obj_create(scr);
    lv_obj_set_size(chart_time_box, 55, 24);
    lv_obj_align(chart_time_box, LV_ALIGN_TOP_LEFT, 10, 8);
    lv_obj_set_style_bg_color(chart_time_box, lv_color_hex(0x222222), 0); 
    lv_obj_set_style_border_width(chart_time_box, 0, 0);
    lv_obj_set_style_radius(chart_time_box, 6, 0); 
    lv_obj_remove_flag(chart_time_box, LV_OBJ_FLAG_SCROLLABLE);

    chart_time_label = lv_label_create(chart_time_box);
    lv_label_set_text(chart_time_label, "");
    lv_obj_align(chart_time_label, LV_ALIGN_CENTER, 0, 0);
    lcd.setTextColor(TFT_DARKGRAY); 
    lv_obj_set_style_text_color(chart_time_label, lv_color_hex(0xD3D3D3), 0);
    lv_obj_set_style_text_font(chart_time_label, &lv_font_montserrat_12, 0);

    chart_top_time_label = lv_label_create(scr);
    lv_label_set_text(chart_top_time_label, "");
    lv_obj_align(chart_top_time_label, LV_ALIGN_TOP_LEFT, 75, 6);
    lv_obj_set_style_text_color(chart_top_time_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(chart_top_time_label, &lv_font_montserrat_12, 0);

    chart_top_date_label = lv_label_create(scr);
    lv_label_set_text(chart_top_date_label, "");
    lv_obj_align(chart_top_date_label, LV_ALIGN_TOP_LEFT, 75, 20);
    lv_obj_set_style_text_color(chart_top_date_label, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(chart_top_date_label, &lv_font_montserrat_10, 0);

    chart_top_price_label = lv_label_create(scr);
    lv_label_set_text(chart_top_price_label, "");
    lv_obj_align(chart_top_price_label, LV_ALIGN_TOP_RIGHT, -10, 6);
    lv_obj_set_style_text_color(chart_top_price_label, lv_color_hex(0xF2A900), 0); 
    lv_obj_set_style_text_font(chart_top_price_label, &lv_font_montserrat_20, 0); 

    lv_obj_add_flag(chart_time_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(chart_top_time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(chart_top_date_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(chart_top_price_label, LV_OBJ_FLAG_HIDDEN);
}

void updateGUI() {
    lv_tick_inc(5); 

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);

    // Alle Standard-Sichtbarkeiten pro Durchlauf ordnungsgemaess zuruecksetzen
    lv_obj_remove_flag(main_price_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(terminal_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(bottom_time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(bottom_date_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(block_title_label, ""); 
    
    if (clock_hour_label != NULL) lv_obj_add_flag(clock_hour_label, LV_OBJ_FLAG_HIDDEN);
    if (clock_colon_label != NULL) lv_obj_add_flag(clock_colon_label, LV_OBJ_FLAG_HIDDEN);
    if (clock_min_label != NULL) lv_obj_add_flag(clock_min_label, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_add_flag(chart_time_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(chart_top_time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(chart_top_date_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(chart_top_price_label, LV_OBJ_FLAG_HIDDEN);
    
    if(btc_logo_label == NULL) {
        btc_logo_label = lv_label_create(lv_screen_active());
        lv_obj_set_style_text_font(btc_logo_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(btc_logo_label, lv_color_hex(0xF2A900), 0); 
    }
    lv_obj_add_flag(btc_logo_label, LV_OBJ_FLAG_HIDDEN);

    if(calc_container != NULL && sysConfig.currentSide != 5) {
        lv_obj_add_flag(calc_container, LV_OBJ_FLAG_HIDDEN);
    }

    if(sysConfig.currentSide == 0) { // 1. USD SEITE
        lv_obj_align(main_price_label, LV_ALIGN_CENTER, 0, -25);
        lv_obj_set_style_text_color(main_price_label, lv_color_hex(0xF2A900), 0); 
        lv_label_set_text(main_price_label, String(btc.priceUSD).c_str());
        
        lv_obj_align(currency_symbol_label, LV_ALIGN_CENTER, -42, 30);
        lv_obj_set_style_text_color(currency_symbol_label, lv_color_hex(0xD3D3D3), 0);
        lv_label_set_text(currency_symbol_label, " / USD"); 
        
        lv_obj_remove_flag(btc_logo_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(btc_logo_label, "B");
        lv_obj_align(btc_logo_label, LV_ALIGN_CENTER, -72, 30);
        
        lv_obj_align(percentage_label, LV_ALIGN_CENTER, 55, 30); 
        String pctStr = (btc.changeUSD >= 0 ? "+" : "") + String(btc.changeUSD, 2) + "%";
        lv_label_set_text(percentage_label, pctStr.c_str());
        lv_obj_set_style_text_color(percentage_label, lv_color_hex(btc.changeUSD >= 0 ? 0x00FF00 : 0xFF0000), 0);
    } 
    else if(sysConfig.currentSide == 1) { // 2. EUR SEITE
        lv_obj_align(main_price_label, LV_ALIGN_CENTER, 0, -25);
        lv_obj_set_style_text_color(main_price_label, lv_color_hex(0xF2A900), 0); 
        lv_label_set_text(main_price_label, String(btc.priceEUR).c_str());
        
        lv_obj_align(currency_symbol_label, LV_ALIGN_CENTER, -42, 30);
        lv_obj_set_style_text_color(currency_symbol_label, lv_color_hex(0xD3D3D3), 0);
        lv_label_set_text(currency_symbol_label, " / EUR"); 
        
        lv_obj_remove_flag(btc_logo_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(btc_logo_label, "B");
        lv_obj_align(btc_logo_label, LV_ALIGN_CENTER, -72, 30);
        
        lv_obj_align(percentage_label, LV_ALIGN_CENTER, 55, 30); 
        String pctStr = (btc.changeEUR >= 0 ? "+" : "") + String(btc.changeEUR, 2) + "%";
        lv_label_set_text(percentage_label, pctStr.c_str());
        lv_obj_set_style_text_color(percentage_label, lv_color_hex(btc.changeEUR >= 0 ? 0x00FF00 : 0xFF0000), 0);
    }
    else if(sysConfig.currentSide == 2) { // 3. GROSSE TERMINAL UHRZEIT
        lv_obj_add_flag(main_price_label, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(terminal_line, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(bottom_time_label, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(bottom_date_label, LV_OBJ_FLAG_HIDDEN);
        
        if (clock_hour_label == NULL) {
            lv_obj_t * scr = lv_screen_active();
            clock_hour_label = lv_label_create(scr);
            lv_obj_align(clock_hour_label, LV_ALIGN_CENTER, -35, -20); 
            lv_obj_set_style_text_font(clock_hour_label, &lv_font_montserrat_40, 0);
            lv_obj_set_style_text_color(clock_hour_label, lv_color_hex(0xD3D3D3), 0);
            
            clock_colon_label = lv_label_create(scr);
            lv_obj_align(clock_colon_label, LV_ALIGN_CENTER, 0, -22); 
            lv_obj_set_style_text_font(clock_colon_label, &lv_font_montserrat_40, 0);
            lv_label_set_text(clock_colon_label, ":");
            
            clock_min_label = lv_label_create(scr);
            lv_obj_align(clock_min_label, LV_ALIGN_CENTER, 35, -20); 
            lv_obj_set_style_text_font(clock_min_label, &lv_font_montserrat_40, 0);
            lv_obj_set_style_text_color(clock_min_label, lv_color_hex(0xD3D3D3), 0);
        }
        lv_obj_remove_flag(clock_hour_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(clock_colon_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(clock_min_label, LV_OBJ_FLAG_HIDDEN);

        String stunde = btc.timeStr.substring(0, 2);
        String minute = btc.timeStr.substring(3, 5);
        lv_label_set_text(clock_hour_label, stunde.c_str());
        lv_label_set_text(clock_min_label, minute.c_str());

        time_t raw_sec; struct tm *t_info; time(&raw_sec); t_info = localtime(&raw_sec);
        if (t_info->tm_sec % 2 == 0) lv_obj_set_style_text_color(clock_colon_label, lv_color_hex(0xD3D3D3), 0); 
        else lv_obj_set_style_text_color(clock_colon_label, lv_color_hex(0x000000), 0); 
        
        lv_obj_align(currency_symbol_label, LV_ALIGN_CENTER, 0, 35);
        lv_obj_set_style_text_font(currency_symbol_label, &lv_font_montserrat_14, 0); 
        lv_obj_set_style_text_color(currency_symbol_label, lv_color_hex(0x666666), 0); 
        lv_label_set_text(currency_symbol_label, btc.dateStr.c_str());
        lv_label_set_text(percentage_label, ""); 
    }
    else if(sysConfig.currentSide == 3) { // 4. BLOCKCHAIN SEITE
        lv_obj_align(main_price_label, LV_ALIGN_CENTER, 0, -15);
        lv_obj_set_style_text_color(main_price_label, lv_color_hex(0xFFFFFF), 0); 
        lv_label_set_text(main_price_label, String(btc.blockHeight).c_str());
        lv_obj_align(block_title_label, LV_ALIGN_CENTER, -45, -48);
        lv_label_set_text(block_title_label, "BLOCK");
        
        lv_obj_align(currency_symbol_label, LV_ALIGN_CENTER, 0, 30);
        String infoStr = "FEES: " + String(btc.medFee) + " sat  |  DIFF " + ((btc.difficultyChange >= 0) ? "+" : "") + String(btc.difficultyChange, 1) + "%";
        lv_label_set_text(currency_symbol_label, infoStr.c_str());
        lv_obj_set_style_text_color(currency_symbol_label, lv_color_hex(0xF2A900), 0); 
        lv_label_set_text(percentage_label, "");
    }
    else if(sysConfig.currentSide == 4) { // 5. CHART SEITE
        lv_obj_add_flag(main_price_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(terminal_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bottom_time_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bottom_date_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(currency_symbol_label, ""); lv_label_set_text(percentage_label, "");
        
        lv_obj_remove_flag(chart_time_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(chart_top_time_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(chart_top_date_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(chart_top_price_label, LV_OBJ_FLAG_HIDDEN);
        
        lv_label_set_text(chart_top_time_label, btc.timeStr.c_str());
        String shortDate = btc.dateStr.substring(btc.dateStr.indexOf(",") + 2);
        lv_label_set_text(chart_top_date_label, shortDate.c_str());
        
        if (sysConfig.chartInterval == 0) lv_label_set_text(chart_time_label, "24h");
        else if (sysConfig.chartInterval == 1) lv_label_set_text(chart_time_label, "3d");
        else if (sysConfig.chartInterval == 2) lv_label_set_text(chart_time_label, "5d");

        if (sysConfig.chartCurrency == 0) lv_label_set_text(chart_top_price_label, (String(btc.priceEUR) + " EUR").c_str());
        else lv_label_set_text(chart_top_price_label, (String(btc.priceUSD) + " USD").c_str());
    }
    else if(sysConfig.currentSide == 5) { // 6. INTERAKTIVER SATOSHI-RECHNER SEITE
        lv_obj_add_flag(main_price_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(terminal_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bottom_time_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bottom_date_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(currency_symbol_label, ""); lv_label_set_text(percentage_label, "");

        // Container-Fenster erzeugen, falls es noch nicht existiert
        if(calc_container == NULL) {
            lv_obj_t * scr = lv_screen_active();
            calc_container = lv_obj_create(scr);
            lv_obj_set_size(calc_container, 320, 240);
            lv_obj_align(calc_container, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_style_bg_color(calc_container, lv_color_hex(0x000000), 0);
            lv_obj_set_style_border_width(calc_container, 0, 0);
            lv_obj_set_style_radius(calc_container, 0, 0);
            lv_obj_set_style_pad_all(calc_container, 0, 0);
            lv_obj_remove_flag(calc_container, LV_OBJ_FLAG_SCROLLABLE);

            // 1. FENSTERBALKEN (Ganz oben)
            lv_obj_t * title_bar = lv_obj_create(calc_container);
            lv_obj_set_size(title_bar, 320, 30);
            lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
            lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x111111), 0);
            lv_obj_set_style_border_width(title_bar, 0, 0);
            lv_obj_set_style_pad_all(title_bar, 0, 0);
            lv_obj_remove_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t * title_lbl = lv_label_create(title_bar);
            lv_label_set_text(title_lbl, "Rechner");
            lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 10, 0);
            lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xFFFFFF), 0);

            // Schliessknopf (X) ganz rechts oben
            lv_obj_t * close_btn = lv_button_create(title_bar);
            lv_obj_set_size(close_btn, 30, 24);
            lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -5, 0);
            lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xd9534f), 0);
            lv_obj_t * close_lbl = lv_label_create(close_btn);
            lv_label_set_text(close_lbl, "X");
            lv_obj_align(close_lbl, LV_ALIGN_CENTER, 0, 0);
            
            // Event: Schliesst den Rechner und wirft den User auf Seite 0 (USD) zurueck
            lv_obj_add_event_cb(close_btn, [](lv_event_t * e){
                sysConfig.currentSide = 0;
                lcd.fillScreen(0x000000);
                lv_obj_invalidate(lv_screen_active());
            }, LV_EVENT_CLICKED, NULL);

            // 2. DURCHGEHENDER ORANGER STRICH UNTER DER LEISTE
            static lv_point_precise_t top_line_pts[] = { {0, 30}, {320, 30} };
            lv_obj_t * top_orange_line = lv_line_create(calc_container);
            lv_line_set_points(top_orange_line, top_line_pts, 2);
            lv_obj_set_style_line_color(top_orange_line, lv_color_hex(0xF2A900), 0);
            lv_obj_set_style_line_width(top_orange_line, 2, 0);

            // 3. RECHTE HAELFTE: TASCHENRECHNERTASEN (Zahlenfeld Matrix via LVGL)
            static const char * btnm_map[] = {
                "7", "8", "9", "\n",
                "4", "5", "6", "\n",
                "1", "2", "3", "\n",
                ",", "0", "C", "NEXT", ""
            };

            lv_obj_t * btnm = lv_buttonmatrix_create(calc_container);
            lv_obj_set_size(btnm, 140, 195);
            lv_obj_align(btnm, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
            lv_buttonmatrix_set_map(btnm, btnm_map);
            lv_obj_set_style_bg_color(btnm, lv_color_hex(0x151515), 0);
            lv_obj_set_style_border_width(btnm, 0, 0);
            lv_obj_set_style_pad_all(btnm, 2, 0);
            
            // Event-Handler fuer das Keypad
            lv_obj_add_event_cb(btnm, [](lv_event_t * e){
                lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e);
                uint32_t id = lv_buttonmatrix_get_selected_button(obj); 
                if(id == LV_BUTTONMATRIX_BUTTON_NONE) return;
                const char * txt = lv_buttonmatrix_get_button_text(obj, id);
                handleCalcKeyPress(txt);
                lv_obj_invalidate(lv_screen_active());
            }, LV_EVENT_VALUE_CHANGED, NULL);
            // 4. LINKE HAELFTE: NATIVE LVGL 9 ZEILEN UND INTERAKTIVE BUTTONS
            int startY = 35;
            int rowHeight = 31;

            lv_obj_t * fix_row_lbl = lv_label_create(calc_container);
            lv_label_set_text(fix_row_lbl, "Sat.  100000000");
            lv_obj_set_style_text_color(fix_row_lbl, lv_color_hex(0xF2A900), 0); 
            lv_obj_set_style_text_font(fix_row_lbl, &lv_font_montserrat_12, 0); 
            lv_obj_align(fix_row_lbl, LV_ALIGN_TOP_LEFT, 10, startY + 6);

            calc_btn_sats = lv_button_create(calc_container);
            lv_obj_set_style_radius(calc_btn_sats, 4, 0);
            lv_obj_set_size(calc_btn_sats, 160, rowHeight - 2);
            lv_obj_align(calc_btn_sats, LV_ALIGN_TOP_LEFT, 5, startY + rowHeight);
            lv_obj_set_style_bg_color(calc_btn_sats, lv_color_hex(0x1a1a1a), 0);
            lv_obj_set_style_pad_all(calc_btn_sats, 0, 0);
            lv_obj_set_style_border_width(calc_btn_sats, 0, 0);
            
            lv_obj_t * lbl_sats_tag = lv_label_create(calc_btn_sats);
            lv_label_set_text(lbl_sats_tag, "Sat.");
            lv_obj_set_style_text_color(lbl_sats_tag, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(lbl_sats_tag, &lv_font_montserrat_12, 0);
            lv_obj_align(lbl_sats_tag, LV_ALIGN_LEFT_MID, 6, 0);

            calc_lbl_sats = lv_label_create(calc_btn_sats);
            lv_obj_set_style_text_color(calc_lbl_sats, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(calc_lbl_sats, &lv_font_montserrat_12, 0);
            lv_obj_align(calc_lbl_sats, LV_ALIGN_LEFT_MID, 40, 0);

            lv_obj_add_event_cb(calc_btn_sats, [](lv_event_t * e){
                sysConfig.calcSelectedRow = 0;
                sysConfig.calcInputBuffer = "";
            }, LV_EVENT_CLICKED, NULL);

            calc_btn_eur = lv_button_create(calc_container);
            lv_obj_set_style_radius(calc_btn_eur, 4, 0);
            lv_obj_set_size(calc_btn_eur, 160, rowHeight - 2);
            lv_obj_align(calc_btn_eur, LV_ALIGN_TOP_LEFT, 5, startY + (rowHeight * 2));
            lv_obj_set_style_bg_color(calc_btn_eur, lv_color_hex(0x000000), 0);
            lv_obj_set_style_pad_all(calc_btn_eur, 0, 0);
            lv_obj_set_style_border_width(calc_btn_eur, 0, 0);

            lv_obj_t * lbl_eur_tag = lv_label_create(calc_btn_eur);
            lv_label_set_text(lbl_eur_tag, "EUR");
            lv_obj_set_style_text_color(lbl_eur_tag, lv_color_hex(0xD3D3D3), 0);
            lv_obj_set_style_text_font(lbl_eur_tag, &lv_font_montserrat_12, 0);
            lv_obj_align(lbl_eur_tag, LV_ALIGN_LEFT_MID, 6, 0);

            calc_lbl_eur = lv_label_create(calc_btn_eur);
            lv_obj_set_style_text_color(calc_lbl_eur, lv_color_hex(0xD3D3D3), 0);
            lv_obj_set_style_text_font(calc_lbl_eur, &lv_font_montserrat_12, 0);
            lv_obj_align(calc_lbl_eur, LV_ALIGN_LEFT_MID, 40, 0);

            lv_obj_add_event_cb(calc_btn_eur, [](lv_event_t * e){
                sysConfig.calcSelectedRow = 1;
                sysConfig.calcInputBuffer = "";
            }, LV_EVENT_CLICKED, NULL);

            calc_btn_usd = lv_button_create(calc_container);
            lv_obj_set_style_radius(calc_btn_usd, 4, 0);
            lv_obj_set_size(calc_btn_usd, 160, rowHeight - 2);
            lv_obj_align(calc_btn_usd, LV_ALIGN_TOP_LEFT, 5, startY + (rowHeight * 3));
            lv_obj_set_style_bg_color(calc_btn_usd, lv_color_hex(0x000000), 0);
            lv_obj_set_style_pad_all(calc_btn_usd, 0, 0);
            lv_obj_set_style_border_width(calc_btn_usd, 0, 0);

            lv_obj_t * lbl_usd_tag = lv_label_create(calc_btn_usd);
            lv_label_set_text(lbl_usd_tag, "USD");
            lv_obj_set_style_text_color(lbl_usd_tag, lv_color_hex(0xD3D3D3), 0);
            lv_obj_set_style_text_font(lbl_usd_tag, &lv_font_montserrat_12, 0);
            lv_obj_align(lbl_usd_tag, LV_ALIGN_LEFT_MID, 6, 0);

            calc_lbl_usd = lv_label_create(calc_btn_usd);
            lv_obj_set_style_text_color(calc_lbl_usd, lv_color_hex(0xD3D3D3), 0);
            lv_obj_set_style_text_font(calc_lbl_usd, &lv_font_montserrat_12, 0);
            lv_obj_align(calc_lbl_usd, LV_ALIGN_LEFT_MID, 40, 0);

            lv_obj_add_event_cb(calc_btn_usd, [](lv_event_t * e){
                sysConfig.calcSelectedRow = 2;
                sysConfig.calcInputBuffer = "";
            }, LV_EVENT_CLICKED, NULL);

            lv_obj_t * calc_go_btn = lv_button_create(calc_container);
            lv_obj_set_size(calc_go_btn, 150, 36);
            lv_obj_align(calc_go_btn, LV_ALIGN_TOP_LEFT, 10, startY + (rowHeight * 4) + 6);
            lv_obj_set_style_bg_color(calc_go_btn, lv_color_hex(0xF2A900), 0); 
            lv_obj_set_style_radius(calc_go_btn, 6, 0);
            
            lv_obj_t * calc_go_lbl = lv_label_create(calc_go_btn);
            lv_label_set_text(calc_go_lbl, "RECHNEN");
            lv_obj_set_style_text_font(calc_go_lbl, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(calc_go_lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_align(calc_go_lbl, LV_ALIGN_CENTER, 0, 0);

            lv_obj_add_event_cb(calc_go_btn, [](lv_event_t * e){
                executeCalculation(); 
            }, LV_EVENT_CLICKED, NULL);

            loadCalcInitialValues();
            updateCalcFieldsOnScreen();
        }

        lv_obj_set_style_bg_color(calc_btn_sats, lv_color_hex(sysConfig.calcSelectedRow == 0 ? 0x222222 : 0x000000), 0);
        lv_obj_set_style_bg_color(calc_btn_eur, lv_color_hex(sysConfig.calcSelectedRow == 1 ? 0x222222 : 0x000000), 0);
        lv_obj_set_style_bg_color(calc_btn_usd, lv_color_hex(sysConfig.calcSelectedRow == 2 ? 0x222222 : 0x000000), 0);

        lv_obj_remove_flag(calc_container, LV_OBJ_FLAG_HIDDEN);
    }

    if(sysConfig.currentSide != 2 && sysConfig.currentSide != 4 && sysConfig.currentSide != 5) {
        String shortDate = btc.dateStr.substring(btc.dateStr.indexOf(",") + 2);
        lv_label_set_text(bottom_time_label, btc.timeStr.c_str());
        lv_label_set_text(bottom_date_label, shortDate.c_str());
    }

    // Wenn wir die Chartseite verlassen, verstecken wir das native LVGL-Chart komplett
    if(sysConfig.currentSide != 4 && chart_obj != NULL) {
        lv_obj_add_flag(chart_obj, LV_OBJ_FLAG_HIDDEN);
        if(chart_max_price_lbl) lv_obj_add_flag(chart_max_price_lbl, LV_OBJ_FLAG_HIDDEN);
        if(chart_min_price_lbl) lv_obj_add_flag(chart_min_price_lbl, LV_OBJ_FLAG_HIDDEN);
    }

    lv_timer_handler();
    
    // REPARIERT: Live-Chart mitsamt Raster komplett auf flackerfreie LVGL 9 Engine migriert
    if(sysConfig.currentSide == 4) {
        static unsigned long lastChartRender = 0;
        
        if (millis() - lastChartRender > 2000 || chart_obj == NULL) {
            lastChartRender = millis();
            
            long* activeHistory = btc.history24h; int activeCount = btc.count24h; 
            if (sysConfig.chartInterval == 1) { activeHistory = btc.history3d; activeCount = btc.count3d; }
            else if (sysConfig.chartInterval == 2) { activeHistory = btc.history5d; activeCount = btc.count5d; }

            if (activeCount > 1) {
                long maxPrice = activeHistory[0]; long minPrice = activeHistory[0];
                for(int i = 0; i < activeCount; i++) {
                    if(activeHistory[i] > maxPrice) maxPrice = activeHistory[i];
                    if(activeHistory[i] < minPrice) minPrice = activeHistory[i];
                }
                if(maxPrice == minPrice) { maxPrice += 10; minPrice -= 10; }

                int cMinY = 55, cMaxY = 210;

                // Initialisiert das native Chart-Widget beim allerersten Aufruf
                if(chart_obj == NULL) {
                    lv_obj_t * scr = lv_screen_active();
                    
                    chart_obj = lv_chart_create(scr);
                    lv_obj_set_size(chart_obj, 240, 155);
                    lv_obj_align(chart_obj, LV_ALIGN_TOP_LEFT, 15, cMinY);
                    lv_chart_set_type(chart_obj, LV_CHART_TYPE_LINE);
                    
                    // Sauberes, dezentes Hintergrund-Gitter (Raster) definieren
                    lv_chart_set_div_line_count(chart_obj, 5, 6); // 5 horizontale, 6 vertikale Linien
                    lv_obj_set_style_line_color(chart_obj, lv_color_hex(0x1a1a1a), LV_PART_ITEMS); // Dunkelgraues Raster
                    lv_obj_set_style_bg_color(chart_obj, lv_color_hex(0x000000), 0); // Schwarzer Chart-Hintergrund
                    lv_obj_set_style_border_width(chart_obj, 0, 0);
                    
                    // Datenreihe anlegen (Hellgraue Kurslinie)
                    chart_series = lv_chart_add_series(chart_obj, lv_color_hex(0xD3D3D3), LV_CHART_AXIS_PRIMARY_Y);
                    lv_chart_set_point_count(chart_obj, 24);

                    // Min/Max Preislabels rechts platzieren
                    chart_max_price_lbl = lv_label_create(scr);
                    lv_obj_set_style_text_color(chart_max_price_lbl, lv_color_hex(0x555555), 0);
                    lv_obj_set_style_text_font(chart_max_price_lbl, &lv_font_montserrat_10, 0);
                    lv_obj_align(chart_max_price_lbl, LV_ALIGN_TOP_RIGHT, -5, cMinY);

                    chart_min_price_lbl = lv_label_create(scr);
                    lv_obj_set_style_text_color(chart_min_price_lbl, lv_color_hex(0x555555), 0);
                    lv_obj_set_style_text_font(chart_min_price_lbl, &lv_font_montserrat_10, 0);
                    lv_obj_align(chart_min_price_lbl, LV_ALIGN_TOP_RIGHT, -5, cMaxY - 10);
                }

                // Sichtbarkeit einschalten
                lv_obj_remove_flag(chart_obj, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(chart_max_price_lbl, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(chart_min_price_lbl, LV_OBJ_FLAG_HIDDEN);

                // Dynamischen Y-Achsenbereich an die aktuellen Min/Max Kurse anpassen
                lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, minPrice, maxPrice);
                // Array-Werte sequentiell in das native LVGL-Chart schieben
                for(int i = 0; i < 24; i++) {
                    if (i < activeCount) {
                        lv_chart_set_next_value(chart_obj, chart_series, activeHistory[i]);
                    } else {
                        lv_chart_set_next_value(chart_obj, chart_series, activeHistory[activeCount - 1]);
                    }
                }
                lv_label_set_text(chart_max_price_lbl, String(maxPrice).c_str());
                lv_label_set_text(chart_min_price_lbl, String(minPrice).c_str());
                lv_chart_refresh(chart_obj); // Chart-Zeichnung flackerfrei aktualisieren
            }
        }
    }
}

void triggerScreenClear() {
    lcd.fillScreen(0x000000);
}
