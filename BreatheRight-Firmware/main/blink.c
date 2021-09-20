/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Cloud Connected Blinky v1.3.1
 * blink.c
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "core2forAWS.h"
#include "blink.h"

#define RED_AMAZON_ORANGE 255
#define GREEN_AMAZON_ORANGE 153
#define BLUE_AMAZON_ORANGE 0
#define AMAZON_ORANGE 16750848 // Amazon Orange in Decimal

static const char *TAG = "Blink";

static xSemaphoreHandle color_lock;
static uint8_t red = RED_AMAZON_ORANGE, green = GREEN_AMAZON_ORANGE, blue = BLUE_AMAZON_ORANGE;

void blink_init() {
    color_lock = xSemaphoreCreateMutex();
}

void blink_task(void *arg) {
    xSemaphoreTake(color_lock, pdMS_TO_TICKS(10));
    uint8_t current_red = red, current_green = green, current_blue = blue;
    xSemaphoreGive(color_lock);

    while (1) {
        if((current_red != red) || (current_green != green) || (current_blue != blue)){
            xSemaphoreTake(color_lock, pdMS_TO_TICKS(10));
            current_red = red, current_green = green, current_blue = blue;
            xSemaphoreGive(color_lock);
            ESP_LOGI(TAG, "Color changed to #%.2x%.2x%.2x", current_red, current_green, current_blue);
        }

        Core2ForAWS_Sk6812_Clear();
        Core2ForAWS_Sk6812_Show();

        for (uint8_t i = 0; i < 10; i++) {
            Core2ForAWS_Sk6812_SetColor(i, (current_red << 16) + (current_green << 8) + (current_blue));
            Core2ForAWS_Sk6812_Show();
            vTaskDelay(pdMS_TO_TICKS(140));
        }

        for (uint8_t i = 0; i < 10; i++) {
            Core2ForAWS_Sk6812_SetColor(i, 0x000000);
            Core2ForAWS_Sk6812_Show();
            vTaskDelay(pdMS_TO_TICKS(140));
        }

        // Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0x232f3e);
        // Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0xffffff);
        // Core2ForAWS_Sk6812_Show();

        // for (uint8_t i = 40; i > 0; i--) {
        //     Core2ForAWS_Sk6812_SetBrightness(i);
        //     Core2ForAWS_Sk6812_Show();
        //     vTaskDelay(pdMS_TO_TICKS(25));
        // }

        // Core2ForAWS_Sk6812_SetBrightness(20);
    }
    vTaskDelete(NULL); // Should never get to here...
    // Should never get here. FreeRTOS tasks loop forever.
    ESP_LOGE(TAG, "Error in blink task. Out of loop.");
    abort();
}

void update_color(uint8_t r, uint8_t g, uint8_t b) {
    xSemaphoreTake(color_lock, pdMS_TO_TICKS(10));
    blue = b;
    green = g;
    red = r;
    xSemaphoreGive(color_lock);

}
