/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <math.h>
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include <esp_sleep.h>
#include "esp_log.h"
#include <driver/gpio.h>


// LVGL image declare
//LV_IMG_DECLARE(esp_logo)
//LV_IMG_DECLARE(esp_text)

#if 0
typedef struct {
    lv_obj_t *scr;
    int count_val;
} my_timer_context_t;

static lv_obj_t *arc[3];
static lv_obj_t *img_logo;
static lv_obj_t *img_text;
static lv_color_t arc_color[] = {
    LV_COLOR_MAKE(232, 87, 116),
    LV_COLOR_MAKE(126, 87, 162),
    LV_COLOR_MAKE(90, 202, 228),
};
#endif

#if 0
static void anim_timer_cb(lv_timer_t *timer)
{
    my_timer_context_t *timer_ctx = (my_timer_context_t *) timer->user_data;
    int count = timer_ctx->count_val;
    lv_obj_t *scr = timer_ctx->scr;

    // Play arc animation
    if (count < 90) {
        lv_coord_t arc_start = count > 0 ? (1 - cosf(count / 180.0f * PI)) * 270 : 0;
        lv_coord_t arc_len = (sinf(count / 180.0f * PI) + 1) * 135;

        for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++) {
            lv_arc_set_bg_angles(arc[i], arc_start, arc_len);
            lv_arc_set_rotation(arc[i], (count + 120 * (i + 1)) % 360);
        }
    }

    // Delete arcs when animation finished
    if (count == 90) {
        for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++) {
            lv_obj_del(arc[i]);
        }

        // Create new image and make it transparent
        img_text = lv_img_create(scr);
        lv_img_set_src(img_text, &esp_text);
        lv_obj_set_style_img_opa(img_text, 0, 0);
    }

    // Move images when arc animation finished
    if ((count >= 100) && (count <= 180)) {
        lv_coord_t offset = (sinf((count - 140) * 2.25f / 90.0f) + 1) * 20.0f;
        lv_obj_align(img_logo, LV_ALIGN_CENTER, 0, -offset);
        lv_obj_align(img_text, LV_ALIGN_CENTER, 0, 2 * offset);
        lv_obj_set_style_img_opa(img_text, offset / 40.0f * 255, 0);
    }

    // Delete timer when all animation finished
    if ((count += 5) == 220) {
        lv_timer_del(timer);
    } else {
        timer_ctx->count_val = count;
    }
}
#endif

static lv_obj_t *hello_world_label;
static char new_string[24];

void hello_world_timer(lv_timer_t *timer)
{
  /*Use the user_data*/
  uint32_t *l_data = (uint32_t *)timer->user_data;
  ESP_LOGI("example", "hello_world_timer called with user data: %lu\n", *l_data);

  *l_data = *l_data - 1;
  timer->user_data = (uint32_t *)l_data;

  /*Do something with LVGL*/
  if(*l_data > 0) {
    sprintf(new_string, "%s %lu", "Hello World!", *l_data);
    lv_label_set_text(hello_world_label, new_string);
  }

  if (*l_data < 1) {
    lv_timer_del(timer);

    if (hello_world_label!= NULL) {
        lv_obj_del(hello_world_label);
    }

    //LVGL at present does not support, turn off display, remove display, stop display.
    //ToDo: Need to implement above API for better powering off.
    //put the device into deep sleep and turn off the display.
    bsp_display_backlight_off();
    esp_deep_sleep_start(); //put the device in deep sleep (lowest power mode)
  }

}

void hello_world_label_animation_ready_cb(lv_anim_t *a) {
    // Change the color of the text
    lv_color_t l_color = LV_COLOR_MAKE(rand() % 256, rand() % 256, rand() % 256);
    lv_obj_set_style_text_color(hello_world_label, l_color, 0);
}

void start_hello_world_label_animation(lv_obj_t *label, int repeat_count)
{
    lv_anim_t l_anim;
    lv_anim_init(&l_anim);
    lv_anim_set_var(&l_anim, label);
    lv_anim_set_exec_cb(&l_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&l_anim, LV_HOR_RES + lv_obj_get_width(label), 0);
    lv_anim_set_time(&l_anim, 1000 * strlen(lv_label_get_text(label)));
    lv_anim_set_playback_time(&l_anim, 1000 * strlen(lv_label_get_text(label)));
    lv_anim_set_playback_delay(&l_anim, 1000);
    lv_anim_set_repeat_count(&l_anim, repeat_count);
    lv_anim_set_ready_cb(&l_anim, hello_world_label_animation_ready_cb);
    lv_anim_start(&l_anim);
}

void hello_world_text_lvgl_demo(lv_obj_t *scr)
{
    //text style part
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, &lv_font_montserrat_20);

    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

    /*Create a white label/text, set its text and align it to the center*/
    hello_world_label = lv_label_create(scr);
    lv_label_set_text(hello_world_label, "Hello World!");
    lv_obj_set_style_text_color(scr, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_style(hello_world_label, &style, 0);
    
    //align part
    lv_obj_align(hello_world_label, LV_ALIGN_TOP_MID, 0, 10);

    //updating values based on timer event of label.
    
    static uint32_t hello_world_data = 1;
    lv_timer_create(hello_world_timer, 10000,  &hello_world_data);

    // Start the animation
    //start_hello_world_label_animation(hello_world_label, 10);
}

#if 0
void example_lvgl_demo_ui(lv_obj_t *scr)
{
    // Create image
    img_logo = lv_img_create(scr);
    lv_img_set_src(img_logo, &esp_logo);
    lv_obj_center(img_logo);

    // Create arcs
    for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++) {
        arc[i] = lv_arc_create(scr);

        // Set arc caption
        lv_obj_set_size(arc[i], 220 - 30 * i, 220 - 30 * i);
        lv_arc_set_bg_angles(arc[i], 120 * i, 10 + 120 * i);
        lv_arc_set_value(arc[i], 0);

        // Set arc style
        lv_obj_remove_style(arc[i], NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(arc[i], 10, 0);
        lv_obj_set_style_arc_color(arc[i], arc_color[i], 0);

        // Make arc center
        lv_obj_center(arc[i]);
    }

    // Create timer for animation
    static my_timer_context_t my_tim_ctx = {
        .count_val = -90,
    };
    my_tim_ctx.scr = scr;
    lv_timer_create(anim_timer_cb, 20, &my_tim_ctx);
}
#endif