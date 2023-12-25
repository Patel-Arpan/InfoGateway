/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <math.h>
#include "main.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include <esp_sleep.h>
#include "esp_log.h"
#include <driver/gpio.h>


static const char *TAG = "AppLVGL";

//timer to print real world time every second in log, for test purpose.
//later to be shown on the display.
void current_time_handler(char *buf)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64] = {0};

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);    
    memcpy(buf, strftime_buf, 64);    
}

static lv_obj_t *current_time_label;
static char new_string[72];

void current_time_timer(lv_timer_t *timer)
{
  /*Use the user_data*/
  uint32_t *l_data = (uint32_t *)timer->user_data;
  ESP_LOGI(TAG, "current_time_timer called with user data: %lu\n", *l_data);

  *l_data = *l_data - 1;
  timer->user_data = (uint32_t *)l_data;

  char strftime_buf[64] = {0};
  current_time_handler(strftime_buf); //passing NULL, timer instance not available, as we are using lvgl timer

  /*Do something with LVGL*/
  if(*l_data > 0) {
    //sprintf(new_string, "%s %s", "Time:", strftime_buf);    
    ESP_LOGI(TAG, "Time: %s", strftime_buf);
    lv_label_set_text(current_time_label, strftime_buf);
  }

  if (*l_data < 1) {
    lv_timer_del(timer);

    if (current_time_label!= NULL) {
        lv_obj_del(current_time_label);
    }

    /*  //LVGL at present does not support, turn off display, remove display, stop display.
    //ToDo: Need to implement above API for better powering off.
    //put the device into deep sleep and turn off the display. */
    /*  bsp_display_backlight_off();
    esp_deep_sleep_start(); //put the device in deep sleep (lowest power mode)  */
  }
}

void show_current_time_lvgl()
{
    //text style part
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, &lv_font_montserrat_18); //lv_font_unscii_8

    lv_obj_t *l_scr = lv_disp_get_scr_act(NULL);

    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(l_scr, lv_color_black(), LV_PART_MAIN);

    lv_disp_set_bg_color(NULL, lv_color_black());

    /*Create a white label/text, set its text and align it to the center*/
    current_time_label = lv_label_create(l_scr);
    lv_label_set_text(current_time_label, "Time:");
    lv_obj_set_style_text_color(l_scr, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_style(current_time_label, &style, 0);
    
    //align part
    lv_obj_align(current_time_label, LV_ALIGN_DEFAULT, 0, 0);

    //updating values based on timer event of label.
    
    static uint32_t cnt = 5 * 60;
    lv_timer_create(current_time_timer, 1000,  &cnt);    
}