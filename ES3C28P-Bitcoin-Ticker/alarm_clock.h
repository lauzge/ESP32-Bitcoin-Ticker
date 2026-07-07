#pragma once
#include <time.h>
#include <lvgl.h>
#include "config.h"

unsigned long snoozeTargetTime = 0;
bool isSnoozed = false;
lv_obj_t * alarm_win = NULL;

// Callback: Wird aufgerufen, wenn man auf "AUS" drueckt
static void alarm_off_clicked_cb(lv_event_t * e) {
    sysConfig.isAlarming = false;
    sysConfig.alarmActive = false; // Wecker fuer heute ausschalten
    isSnoozed = false;
    
    if(alarm_win) {
        lv_obj_delete(alarm_win); // Schliesst das Fenster sofort
        alarm_win = NULL;
    }
    sysConfig.screenNeedsClear = true; // Hintergrund sauber reinigen
    Serial.println("Wecker via Touch erfolgreich AUS.");
}

// Callback: Wird aufgerufen, wenn man auf "PAUSE" drueckt
static void alarm_snooze_clicked_cb(lv_event_t * e) {
    sysConfig.isAlarming = false;
    isSnoozed = true;
    snoozeTargetTime = millis() + (5 * 60 * 1000); // 5 Minuten Snooze
    
    if(alarm_win) {
        lv_obj_delete(alarm_win);
        alarm_win = NULL;
    }
    sysConfig.screenNeedsClear = true;
    Serial.println("Snooze via Touch erfolgreich AKTIV.");
}

// Baut das Terminal-Warnfenster mitten auf dem Bildschirm auf
void create_alarm_popup() {
    if(alarm_win != NULL) return; 

    alarm_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(alarm_win, 240, 160);
    lv_obj_center(alarm_win);
    lv_obj_set_style_bg_color(alarm_win, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_color(alarm_win, lv_color_hex(0xd9534f), 0);
    lv_obj_set_style_border_width(alarm_win, 2, 0);
    lv_obj_set_style_radius(alarm_win, 8, 0);
    lv_obj_remove_flag(alarm_win, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title = lv_label_create(alarm_win);
    lv_label_set_text(title, "WECKER ALARM!");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_color(title, lv_color_hex(0xd9534f), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    lv_obj_t * text = lv_label_create(alarm_win);
    lv_label_set_text(text, "Bitcoin Weckruf aktiv!");
    lv_obj_align(text, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_text_color(text, lv_color_hex(0xD3D3D3), 0);

    // Große, absolut klicksichere Buttons, da der I2C-Bus voellig frei ist
    lv_obj_t * btn_off = lv_button_create(alarm_win);
    lv_obj_set_size(btn_off, 90, 36);
    lv_obj_align(btn_off, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_set_style_bg_color(btn_off, lv_color_hex(0xd9534f), 0); 
    lv_obj_add_event_cb(btn_off, alarm_off_clicked_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl_off = lv_label_create(btn_off);
    lv_label_set_text(lbl_off, "AUS");
    lv_obj_align(lbl_off, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * btn_snooze = lv_button_create(alarm_win);
    lv_obj_set_size(btn_snooze, 90, 36);
    lv_obj_align(btn_snooze, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(btn_snooze, lv_color_hex(0xF2A900), 0); 
    lv_obj_add_event_cb(btn_snooze, alarm_snooze_clicked_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl_snooze = lv_label_create(btn_snooze);
    lv_label_set_text(lbl_snooze, "PAUSE");
    lv_obj_align(lbl_snooze, LV_ALIGN_CENTER, 0, 0);
}

void checkAlarm() {
    if (isSnoozed && millis() >= snoozeTargetTime) {
        isSnoozed = false;
        sysConfig.isAlarming = true;
    }

    if (!sysConfig.alarmActive && !sysConfig.isAlarming) {
        return;
    }

    time_t now; struct tm *timeinfo; time(&now); timeinfo = localtime(&now);

    // Ausloesung zur exakten Sekunde 0 der Weckzeit
    if (sysConfig.alarmActive && !isSnoozed &&
        timeinfo->tm_hour == sysConfig.alarmHour && 
        timeinfo->tm_min == sysConfig.alarmMinute && 
        timeinfo->tm_sec == 0) {
        sysConfig.isAlarming = true;
    }

    if (sysConfig.isAlarming) {
        create_alarm_popup(); // PopUp oeffnen, Blink-Takt wird von updateGUI erledigt
    }
}
