/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Cloud Connected Blinky v1.3.1
 * .c
 * 
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "core2forAWS.h"
#include "ui.h"  
#include "power.h"
#include "sound.h"
#include "clock.h" 
#include "pms7003.h"
// #include "mic.h"
#include "edge_impulse.h"

LV_IMG_DECLARE(upbeatlabs_logo);

#define MAX_TEXTAREA_LENGTH 1024

static lv_obj_t *active_screen;
static lv_obj_t *out_txtarea;
static lv_obj_t *wifi_label;

static const char *TAG = MESSAGE_TAB_NAME;

static void tab_event_cb(lv_obj_t* slider, lv_event_t event);

static lv_obj_t* tab_view;

void display_message_tab(lv_obj_t* tv);

static void ui_textarea_prune(size_t new_text_length){
    const char * current_text = lv_textarea_get_text(out_txtarea);
    size_t current_text_len = strlen(current_text);
    if(current_text_len + new_text_length >= MAX_TEXTAREA_LENGTH){
        for(int i = 0; i < new_text_length; i++){
            lv_textarea_set_cursor_pos(out_txtarea, 0);
            lv_textarea_del_char_forward(out_txtarea);
        }
        lv_textarea_set_cursor_pos(out_txtarea, LV_TEXTAREA_CURSOR_LAST);
    }
}

void ui_textarea_add(char *baseTxt, char *param, size_t paramLen) {
    if( baseTxt != NULL ){
        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
        if (param != NULL && paramLen != 0){
            size_t baseTxtLen = strlen(baseTxt);
            ui_textarea_prune(paramLen);
            size_t bufLen = baseTxtLen + paramLen;
            char buf[(int) bufLen];
            sprintf(buf, baseTxt, param);
            lv_textarea_add_text(out_txtarea, buf);
        } 
        else{
            lv_textarea_add_text(out_txtarea, baseTxt); 
        }
        xSemaphoreGive(xGuiSemaphore);
    } 
    else{
        ESP_LOGE(TAG, "Textarea baseTxt is NULL!");
    }
}

void ui_wifi_label_update(bool state){
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    if (state == false) {
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    } 
    else{
        char buffer[25];
        sprintf (buffer, "#0000ff %s #", LV_SYMBOL_WIFI);
        lv_label_set_text(wifi_label, buffer);
    }
    xSemaphoreGive(xGuiSemaphore);
}

void ui_init() {
    /* Displays the Upbeat Labs logo */
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);   // Takes (blocks) the xGuiSemaphore mutex from being read/written by another task.
    lv_obj_t* opener_scr = lv_scr_act();   // Create a new LVGL "screen". Screens can be though of as a window.
    lv_obj_t* aws_img_obj = lv_img_create(opener_scr, NULL);   // Creates an LVGL image object and assigns it as a child of the opener_scr parent screen.
    lv_img_set_src(aws_img_obj, &upbeatlabs_logo);  // Sets the image object with the image data from the upbeatlabs_logo file which contains hex pixel matrix of the image.
    lv_obj_align(aws_img_obj, NULL, LV_ALIGN_CENTER, 0, 0); // Aligns the image object to the center of the parent screen.
    lv_obj_set_style_local_bg_color(opener_scr, LV_OBJ_PART_MAIN, 0, LV_COLOR_WHITE);   // Sets the background color of the screen to white.
    xSemaphoreGive(xGuiSemaphore);  // Frees the xGuiSemaphore so that another task can use it. In this case, the higher priority guiTask will take it and then read the values to then display.

    /* 
    You should release the xGuiSemaphore semaphore before calling a blocking function like vTaskDelay because 
    without doing so, no other task can access it that might need it. This includes the guiTask which
    writes the objects to the display itself over SPI.
    */
    vTaskDelay(pdMS_TO_TICKS(2000)); // FreeRTOS scheduler block execution for 2 seconds to keep showing the Upbeat Labs logo.
    
    xTaskCreatePinnedToCore(sound_task, "soundTask", 4096 * 2, NULL, 3, NULL, 1);

    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    lv_obj_clean(opener_scr);   // Clear the aws_img_obj and remove from memory space. Currently no objects exist on the screen.
    lv_obj_t* core2forAWS_obj = lv_obj_create(NULL, NULL); // Create a object to draw all with no parent 
    lv_scr_load_anim(core2forAWS_obj, LV_SCR_LOAD_ANIM_MOVE_LEFT, 400, 0, false);   // Animates the loading of core2forAWS_obj as a slide into view from the left
    tab_view = lv_tabview_create(core2forAWS_obj, NULL); // Creates the tab view to display different tabs with different hardware features
    lv_obj_set_event_cb(tab_view, tab_event_cb); // Add a callback for whenever there is an event triggered on the tab_view object (e.g. a left-to-right swipe)
    lv_tabview_set_btns_pos(tab_view, LV_TABVIEW_TAB_POS_NONE);  // Hide the tab buttons so it looks like a clean screen

    active_screen = core2forAWS_obj;
    wifi_label = lv_label_create(active_screen, NULL);
    lv_obj_align(wifi_label,NULL,LV_ALIGN_IN_TOP_RIGHT, 0, 6);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    lv_label_set_recolor(wifi_label, true);
    xSemaphoreGive(xGuiSemaphore);

    display_power_info(core2forAWS_obj);

    display_clock_info(active_screen);

    display_pm_tab(tab_view);

    display_message_tab(tab_view);

    // display_microphone_tab(tab_view);

    edge_impulse_start();

     

}

void display_message_tab(lv_obj_t* tv){
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);

    lv_obj_t* message_tab = lv_tabview_add_tab(tv, MESSAGE_TAB_NAME);
    out_txtarea = lv_textarea_create(message_tab, NULL);
    lv_obj_set_size(out_txtarea, 300, 180);
    lv_obj_align(out_txtarea, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -12);
    lv_textarea_set_max_length(out_txtarea, MAX_TEXTAREA_LENGTH);
    lv_textarea_set_text_sel(out_txtarea, false);
    lv_textarea_set_cursor_hidden(out_txtarea, true);
    lv_textarea_set_text(out_txtarea, "Starting BreatheRight...\n");
    xSemaphoreGive(xGuiSemaphore);

}


static void tab_event_cb(lv_obj_t* slider, lv_event_t event){
    if(event == LV_EVENT_VALUE_CHANGED) {
        lv_tabview_ext_t* ext = (lv_tabview_ext_t*) lv_obj_get_ext_attr(tab_view);
        const char* tab_name = ext->tab_name_ptr[lv_tabview_get_tab_act(tab_view)];
        ESP_LOGI(TAG, "Current Active Tab: %s\n", tab_name);

        // vTaskSuspend(pm_handle);
        
        // if(strcmp(tab_name, PM_TAB_NAME) == 0){
        //     reset_pm_bg();
        //     vTaskResume(pm_handle);
        // }

        // vTaskSuspend(mic_handle);

        // if (strcmp(tab_name, MICROPHONE_TAB_NAME) == 0){
        //     vTaskResume(mic_handle);
        //     vTaskResume(FFT_handle);
        // }
    }
}