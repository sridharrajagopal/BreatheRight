/*
 * Plantower PMS 7003
 * BreatheRight v1.0
 * pms7003.c
 * 
 * Copyright (C) 2020 Upbeat Labs LLC or its affiliates.  All Rights Reserved.
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


#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"

#include "esp_log.h"

#include "core2forAWS.h"

#include "pms7003.h"
#include "blink.h"
#include <time.h>

static const char* TAG = PM_TAB_NAME;

// Should create a struct to pass pointers to task, but globals are easier to understand.
static uint8_t r = 0, g = 70, b = 79;
static lv_style_t bg_style;
static lv_obj_t* pm_bg;
static lv_obj_t* aqi_bg;


static lv_obj_t* aqi_label;
static lv_style_t aqi_style;

static lv_obj_t* temp_label;
static lv_style_t temp_style;

static lv_obj_t* humidity_label;
static lv_style_t humidity_style;

static lv_obj_t* pressure_label;
static lv_style_t pressure_style;

static lv_obj_t* pm1_label;
static lv_style_t pm1_style;

static lv_obj_t* pm2_5_label;
static lv_style_t pm2_5_style;

static lv_obj_t* pm10_label;
static lv_style_t pm10_style;

static lv_style_t title_style;
static lv_style_t body_style;


int mapAQItoColor(float aq);
int rgbToInt(int red, int green, int blue);
float getAQIfromPM25(float pm25);
float calcAQI(float Cp, float Ih, float Il, float BPh, float BPl);
void update_AQI(float aq);
char *getStringForAQI(float aq);
int mapAQItoAltColor(float aq);

static void pm_task(void* pvParameters);
static void readpms7003_task(void* pvParameters);
void pms7003Data(uint8_t *data, int length);

PMS7003_DATA pmsData;
SemaphoreHandle_t xPmsSemaphore;

BME280_DATA bmeData;
SemaphoreHandle_t xBmeSemaphore;


void display_pm_tab(lv_obj_t* tv){
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);

    lv_obj_t* pm_tab = lv_tabview_add_tab(tv, PM_TAB_NAME);

    /* Create the main body object and set background within the tab*/
    pm_bg = lv_obj_create(pm_tab, NULL);
    lv_obj_align(pm_bg, NULL, LV_ALIGN_IN_TOP_LEFT, 16, 36);
    lv_obj_set_size(pm_bg, 290, 190);
    lv_obj_set_click(pm_bg, false);

    aqi_bg = lv_obj_create(pm_tab, NULL);
    lv_obj_align(aqi_bg, NULL, LV_ALIGN_IN_TOP_LEFT, 16, 36);
    lv_obj_set_size(aqi_bg, 290, 70);
    lv_obj_set_click(aqi_bg, false);
 
    lv_style_init(&aqi_style);
    lv_style_set_bg_color(&aqi_style, LV_STATE_DEFAULT, LV_COLOR_GREEN);
    lv_obj_add_style(aqi_bg, LV_OBJ_PART_MAIN, &aqi_style);

    lv_style_init(&bg_style);
    lv_style_set_bg_color(&bg_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_add_style(pm_bg, LV_OBJ_PART_MAIN, &bg_style);

    /* Create the title within the main body object */
    lv_style_init(&title_style);
    lv_style_set_text_font(&title_style, LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
    lv_style_set_text_color(&title_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);


    aqi_label = lv_label_create(aqi_bg, NULL);
    lv_obj_add_style(aqi_label, LV_OBJ_PART_MAIN, &title_style);
    lv_label_set_text(aqi_label, "Air Quality Index: 50\nGood");
    lv_label_set_long_mode(aqi_label, LV_LABEL_LONG_EXPAND);

    lv_label_set_align(aqi_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_align(aqi_label, pm_bg, LV_ALIGN_IN_TOP_MID, 0, 10);
    lv_obj_set_width(aqi_label, 290);


    /* Create the sensor information label object */
    /* lv_obj_t* body_label = lv_label_create(pm_bg, NULL);
    lv_label_set_long_mode(body_label, LV_LABEL_LONG_BREAK);
    lv_label_set_static_text(body_label, "The PMS7003 is a particulate matter sensor that provides data on particulate matter of different size buckets."
        "\n\n\n\nPress the touch buttons below.");
    lv_obj_set_width(body_label, 252);
    lv_obj_align(body_label, pm_bg, LV_ALIGN_IN_TOP_LEFT, 20, 40); */

    lv_obj_t* temp_label_s = lv_label_create(pm_bg, NULL);
    lv_label_set_long_mode(temp_label_s, LV_LABEL_LONG_EXPAND);
    lv_label_set_static_text(temp_label_s, "Temp");
    lv_obj_set_width(temp_label_s, 100);
    lv_obj_align(temp_label_s, pm_bg, LV_ALIGN_IN_TOP_LEFT, 20, 80);

    temp_label = lv_label_create(pm_bg, NULL);
    lv_label_set_text(temp_label, "0.0 C");
    lv_label_set_align(temp_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_align(temp_label, pm_bg, LV_ALIGN_IN_TOP_LEFT, 20, 100);

    lv_obj_t* humidity_label_s = lv_label_create(pm_bg, NULL);
    lv_label_set_long_mode(humidity_label_s, LV_LABEL_ALIGN_LEFT);
    lv_label_set_static_text(humidity_label_s, "RH");
    lv_obj_set_width(humidity_label_s, 100);
    lv_obj_align(humidity_label_s, pm_bg, LV_ALIGN_IN_TOP_MID, 0, 80);    

    humidity_label = lv_label_create(pm_bg, NULL);
    lv_label_set_text(humidity_label, "0%");
    lv_label_set_align(humidity_label, LV_LABEL_ALIGN_LEFT);
    lv_obj_align(humidity_label, pm_bg, LV_ALIGN_IN_TOP_MID, 0, 100);   

    lv_obj_t*  pressure_label_s = lv_label_create(pm_bg, NULL);
    lv_label_set_text(pressure_label_s, "Pressure");
    lv_label_set_align(pressure_label_s, LV_LABEL_ALIGN_LEFT);
    lv_obj_align(pressure_label_s, pm_bg, LV_ALIGN_IN_TOP_RIGHT, -20, 80);       

    pressure_label = lv_label_create(pm_bg, NULL);
    lv_label_set_text(pressure_label, "0 bar");
    lv_label_set_align(pressure_label, LV_LABEL_ALIGN_LEFT);
    lv_obj_align(pressure_label, pm_bg, LV_ALIGN_IN_TOP_RIGHT, -20, 100);   

    lv_obj_t*  pm1_label_s = lv_label_create(pm_bg, NULL);
    lv_label_set_text(pm1_label_s, "PM1.0");
    lv_label_set_align(pm1_label_s, LV_LABEL_ALIGN_LEFT);
    lv_obj_align(pm1_label_s, pm_bg, LV_ALIGN_IN_TOP_LEFT, 20, 140); 

    pm1_label = lv_label_create(pm_bg, NULL);
    lv_label_set_text(pm1_label, "0.0");
    lv_label_set_align(pm1_label, LV_LABEL_ALIGN_LEFT);
    lv_obj_set_width(pm1_label, 252);
    lv_obj_align(pm1_label, pm_bg, LV_ALIGN_IN_TOP_LEFT, 20, 160);       

    lv_obj_t*  pm2_5_label_s = lv_label_create(pm_bg, NULL);
    lv_label_set_text(pm2_5_label_s, "PM2.5");
    lv_label_set_align(pm2_5_label_s, LV_LABEL_ALIGN_LEFT);
    lv_obj_align(pm2_5_label_s, pm_bg, LV_ALIGN_IN_TOP_MID, 0, 140); 

    pm2_5_label = lv_label_create(pm_bg, NULL);
    lv_label_set_text(pm2_5_label, "0.0");
    lv_label_set_align(pm2_5_label, LV_LABEL_ALIGN_LEFT);
    lv_obj_set_width(pm2_5_label, 252);
    lv_obj_align(pm2_5_label, pm_bg, LV_ALIGN_IN_TOP_MID, 0, 160);        

    lv_obj_t*  pm10_label_s = lv_label_create(pm_bg, NULL);
    lv_label_set_text(pm10_label_s, "PM10");
    lv_label_set_align(pm10_label_s, LV_LABEL_ALIGN_LEFT);
    lv_obj_align(pm10_label_s, pm_bg, LV_ALIGN_IN_TOP_RIGHT, -20, 140); 

    pm10_label = lv_label_create(pm_bg, NULL);
    lv_label_set_text(pm10_label, "0.0");
    lv_label_set_align(pm10_label, LV_LABEL_ALIGN_LEFT);
    lv_obj_set_width(pm10_label, 252);
    lv_obj_align(pm10_label, pm_bg, LV_ALIGN_IN_TOP_RIGHT, -20, 160);      


    lv_style_init(&body_style);
    lv_style_set_text_color(&body_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    lv_style_init(&temp_style);
    lv_style_set_text_color(&temp_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    lv_style_init(&humidity_style);
    lv_style_set_text_color(&humidity_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);    

    lv_style_init(&pressure_style);
    lv_style_set_text_color(&pressure_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    
    lv_style_init(&pm1_style);
    lv_style_set_text_color(&pm1_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    lv_style_init(&pm2_5_style);
    lv_style_set_text_color(&pm2_5_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    lv_style_init(&pm10_style);
    lv_style_set_text_color(&pm10_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);        
           

    lv_obj_add_style(pm1_label, LV_OBJ_PART_MAIN, &pm1_style);
    lv_obj_add_style(pm2_5_label, LV_OBJ_PART_MAIN, &pm2_5_style);
    lv_obj_add_style(pm10_label, LV_OBJ_PART_MAIN, &pm10_style);
    lv_obj_add_style(temp_label, LV_OBJ_PART_MAIN, &temp_style);
    lv_obj_add_style(humidity_label, LV_OBJ_PART_MAIN, &humidity_style);
    lv_obj_add_style(pressure_label, LV_OBJ_PART_MAIN, &pressure_style);

    lv_obj_add_style(temp_label_s, LV_OBJ_PART_MAIN, &body_style);
    lv_obj_add_style(humidity_label_s, LV_OBJ_PART_MAIN, &body_style);
    lv_obj_add_style(pressure_label_s, LV_OBJ_PART_MAIN, &body_style);
    lv_obj_add_style(pm1_label_s, LV_OBJ_PART_MAIN, &body_style);
    lv_obj_add_style(pm2_5_label_s, LV_OBJ_PART_MAIN, &body_style);
    lv_obj_add_style(pm10_label_s, LV_OBJ_PART_MAIN, &body_style);



    /*Create an array for the points of the line*/
    static lv_point_t line_points[] = { {20, 0}, {70, 0} };

    /*Create style*/
    static lv_style_t red_line_style;
    lv_style_init(&red_line_style);
    lv_style_set_line_width(&red_line_style, LV_STATE_DEFAULT, 6);
    lv_style_set_line_color(&red_line_style, LV_STATE_DEFAULT, LV_COLOR_RED);
    lv_style_set_line_rounded(&red_line_style, LV_STATE_DEFAULT, true);

    static lv_style_t green_line_style;
    lv_style_init(&green_line_style);
    lv_style_set_line_width(&green_line_style, LV_STATE_DEFAULT, 6);
    lv_style_set_line_color(&green_line_style, LV_STATE_DEFAULT, LV_COLOR_GREEN);
    lv_style_set_line_rounded(&green_line_style, LV_STATE_DEFAULT, true);

    static lv_style_t blue_line_style;
    lv_style_init(&blue_line_style);
    lv_style_set_line_width(&blue_line_style, LV_STATE_DEFAULT, 6);
    lv_style_set_line_color(&blue_line_style, LV_STATE_DEFAULT, LV_COLOR_BLUE);
    lv_style_set_line_rounded(&blue_line_style, LV_STATE_DEFAULT, true);

    /*Create a line and apply the new style*/
    lv_obj_t* left_line = lv_line_create(pm_tab, NULL);
    lv_line_set_points(left_line, line_points, 2);
    lv_obj_add_style(left_line, LV_LINE_PART_MAIN, &red_line_style);
    lv_obj_align(left_line, NULL, LV_ALIGN_IN_LEFT_MID, 8, 108);

    lv_obj_t* middle_line = lv_line_create(pm_tab, NULL);
    lv_line_set_points(middle_line, line_points, 2);
    lv_obj_add_style(middle_line, LV_LINE_PART_MAIN, &green_line_style);
    lv_obj_align(middle_line, NULL, LV_ALIGN_CENTER, -12, 108);
    
    lv_obj_t* right_line = lv_line_create(pm_tab, NULL);
    lv_line_set_points(right_line, line_points, 2);
    lv_obj_add_style(right_line, LV_LINE_PART_MAIN, &blue_line_style);
    lv_obj_align(right_line, NULL, LV_ALIGN_IN_RIGHT_MID, -30, 108);

    xSemaphoreGive(xGuiSemaphore);

    xPmsSemaphore = xSemaphoreCreateMutex();
    xSemaphoreTake(xPmsSemaphore, portMAX_DELAY);
    pmsData.PM1_0_SP_UGM3 = 0;
    pmsData.PM2_5_SP_UGM3 = 0;
    pmsData.PM10_SP_UGM3 = 0;
    pmsData.PM1_0_AE_UGM3 = 0;
    pmsData.PM2_5_AE_UGM3 = 0;
    pmsData.PM10_AE_UGM3 = 0;
    pmsData.NP_03_UM = 0;
    pmsData.NP_05_UM = 0;
    pmsData.NP_1_0_UM = 0;
    pmsData.NP_2_5_UM = 0;
    pmsData.NP_5_0_UM = 0;
    pmsData.NP_10_UM = 0;
    xSemaphoreGive(xPmsSemaphore);

    xBmeSemaphore = xSemaphoreCreateMutex();
    xSemaphoreTake(xBmeSemaphore, portMAX_DELAY);
    bmeData.temperatureC = 0.0f;
    bmeData.humidityP = 0.0f;
    bmeData.pressureB = 0.0f;
    xSemaphoreGive(xBmeSemaphore);


    xTaskCreatePinnedToCore(pm_task, "pmTask", configMINIMAL_STACK_SIZE * 3, NULL, 1, &pm_handle, 1);
    xTaskCreatePinnedToCore(readpms7003_task, "pms7003Task", configMINIMAL_STACK_SIZE * 3, NULL, 1, &pms7003_handle, 1);

}

void reset_pm_bg(){
    r=0x00, g=0x00, b=0x00;
    lv_style_set_bg_color(&bg_style, LV_STATE_DEFAULT, lv_color_make(r, g, b));
    lv_obj_add_style(pm_bg, LV_OBJ_PART_MAIN, &bg_style);
}

static void pm_task(void* pvParameters){

    // vTaskSuspend(NULL);
    long startTime = pdTICKS_TO_MS(xTaskGetTickCount());
    long endTime = startTime+5000;
    srand(time(NULL));
    for(;;){
        uint16_t x, y;
        bool press;

        FT6336U_GetTouch(&x, &y, &press);
        char temp_str[64];
        char pm1_str[64];
        char pm2_5_str[64];
        char pm10_str[64];
       
        
        if (Button_WasPressed(button_left)) {
            r+=0x10;
            lv_style_set_bg_color(&bg_style, LV_STATE_DEFAULT, lv_color_make(r, g, b));
            lv_obj_add_style(pm_bg, LV_OBJ_PART_MAIN, &bg_style);
            update_color(r, g, b);
            ESP_LOGI(TAG, "Left Button pressed. R: %x G: %x B:%x", r, g, b);
        }
        if (Button_WasPressed(button_middle)) {
            g+=0x10;
            lv_style_set_bg_color(&bg_style, LV_STATE_DEFAULT, lv_color_make(r, g, b));
            lv_obj_add_style(pm_bg, LV_OBJ_PART_MAIN, &bg_style);
            update_color(r, g, b);
            ESP_LOGI(TAG, "Middle Button pressed. R: %x G: %x B:%x", r, g, b);
        }
        if (Button_WasPressed(button_right)) {
            b+=0x10;
            lv_style_set_bg_color(&bg_style, LV_STATE_DEFAULT, lv_color_make(r, g, b));
            lv_obj_add_style(pm_bg, LV_OBJ_PART_MAIN, &bg_style);
            update_color(r, g, b);
            ESP_LOGI(TAG, "Right Button pressed. R: %x G: %x B:%x", r, g, b);
        }
        float temp;

        if (endTime - startTime >= 5000) {
            MPU6886_GetTempData(&temp);
            // Apply calibration offset 
            // calculated from tempF = tempC * 1.8 + 32 - 50 (calibration offset for F)
            temp = temp - 27.78;
            sprintf(temp_str, "%.2f °C", temp);

            xSemaphoreTake(xBmeSemaphore, portMAX_DELAY);
            bmeData.temperatureC = temp;
            xSemaphoreGive(xBmeSemaphore);

            xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
            lv_label_set_text(temp_label, temp_str);
            lv_style_set_text_color(&temp_style, LV_STATE_DEFAULT, LV_COLOR_GREEN);
            xSemaphoreGive(xGuiSemaphore);

            // ESP_LOGI(TAG, "Temp: %3.2f", temp);
            startTime = pdTICKS_TO_MS(xTaskGetTickCount());

            // update_color(255, 0, 0);
            // uint16_t aqi = (uint16_t)(rand() % 500);
            PMS7003_DATA myData;
            
            xSemaphoreTake(xPmsSemaphore, portMAX_DELAY);
            myData.PM1_0_SP_UGM3 = pmsData.PM1_0_SP_UGM3;
            myData.PM2_5_SP_UGM3 = pmsData.PM2_5_SP_UGM3;
            myData.PM10_SP_UGM3 = pmsData.PM10_SP_UGM3;
            myData.PM1_0_AE_UGM3 = pmsData.PM1_0_AE_UGM3;
            myData.PM2_5_AE_UGM3 = pmsData.PM2_5_AE_UGM3;
            myData.PM10_AE_UGM3 = pmsData.PM10_AE_UGM3;
            myData.NP_03_UM = pmsData.NP_03_UM;
            myData.NP_05_UM = pmsData.NP_05_UM;
            myData.NP_1_0_UM = pmsData.NP_1_0_UM;
            myData.NP_2_5_UM = pmsData.NP_2_5_UM;
            myData.NP_5_0_UM = pmsData.NP_5_0_UM;
            myData.NP_10_UM = pmsData.NP_10_UM;
            xSemaphoreGive(xPmsSemaphore);   
            sprintf(pm1_str, "%d", myData.PM1_0_AE_UGM3);
            sprintf(pm2_5_str, "%d", myData.PM2_5_AE_UGM3);
            sprintf(pm10_str, "%d", myData.PM10_AE_UGM3);


            uint16_t aqi = getAQIfromPM25(myData.PM2_5_AE_UGM3);
            update_AQI(aqi);
            sprintf(temp_str, "Air Quality Index: %d\n%s", aqi, getStringForAQI(aqi));

            int rgb = mapAQItoColor(aqi);
            int red = (rgb >> 16) & 0xFF;
            int green = (rgb >> 8) & 0xFF;
            int blue = rgb & 0xFF;

            int rgbAlt = mapAQItoAltColor(aqi);
            int redAlt = (rgbAlt >> 16) & 0xFF;
            int greenAlt = (rgbAlt >> 8) & 0xFF;
            int blueAlt = rgbAlt & 0xFF;

            xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
            lv_label_set_text(aqi_label, temp_str);
            lv_style_set_bg_color(&aqi_style, LV_STATE_DEFAULT, lv_color_make(red, green, blue));
            lv_obj_add_style(aqi_bg, LV_OBJ_PART_MAIN, &aqi_style);

            lv_style_set_bg_color(&bg_style, LV_STATE_DEFAULT, lv_color_make(redAlt, greenAlt, blueAlt));
            lv_obj_add_style(pm_bg, LV_OBJ_PART_MAIN, &bg_style);

            lv_label_set_text(pm1_label, pm1_str);
            lv_label_set_text(pm2_5_label, pm2_5_str);
            lv_label_set_text(pm10_label, pm10_str);

            xSemaphoreGive(xGuiSemaphore);     
        }
        endTime = pdTICKS_TO_MS(xTaskGetTickCount());

        

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelete(NULL); // Should never get to here...
}

void update_AQI(float aq) {
    if (aq >= 301) {
        // Maroon 
        update_color(126, 0, 35);
    } else if (aq >= 201) {
        // Purple
        update_color(143, 63, 151);
    } else if (aq >= 151) {
        // Red
        update_color(255, 0, 0);        
    } else if (aq >= 101) {
        // Orange
        update_color(255, 126, 0);
    } else if (aq >= 51) {
        // Yellow
        update_color(255, 255, 0);
    } else if (aq >= 0) {
        // Green
        update_color(0, 128, 0);
    }

}

float calcAQI(float Cp, float Ih, float Il, float BPh, float BPl) {
    float a = (Ih - Il);
    float b = (BPh - BPl);
    float c = (Cp - BPl);
    float aq = ((a/b) * c + Il);
    return aq;
}

char *getStringForAQI(float aq){
    if (aq >= 301) {
        // Maroon 
        return "Hazardous";
    } else if (aq >= 201) {
        // Purple
        return "Very Unhealthy";
    } else if (aq >= 151) {
        // Red
        return "Unhealthy";        
    } else if (aq >= 101) {
        // Orange
        return "Unhealthy for\nSensitive Groups";
    } else if (aq >= 51) {
        // Yellow
        return "Moderate";
    } else if (aq >= 0) {
        // Green
        return "Good";
    }
    return "Unknown";    
}

float getAQIfromPM25(float pm25) {
    float aq = 0.0;
    if (pm25 > 350.5) {
        aq = calcAQI(pm25, 500, 401, 500, 350.5);
    } else if (pm25 > 250.5) {
         aq = calcAQI(pm25, 400, 301, 350.4, 250.5);       
    } else if (pm25 > 150.5) {
         aq = calcAQI(pm25, 300, 201, 250.4, 150.5);      
    } else if (pm25 > 55.5) {
         aq = calcAQI(pm25, 200, 151, 150.4, 55.5); 
    } else if (pm25 > 35.5) {
         aq = calcAQI(pm25, 150, 101, 55.4, 35.5);       
    } else if (pm25 > 12.1) {
         aq = calcAQI(pm25, 100, 51, 35.4, 12.1);       
    } else if (pm25 > 0) {
         aq = calcAQI(pm25, 50, 0, 12, 0);
    }
    return aq;
}

int rgbToInt(int red, int green, int blue)
{
    int rgb =  (red << 16) + (green << 8) + (blue);  
    return rgb;
}

int mapAQItoColor(float aq) {
    if (aq >= 301) {
        // Maroon 
        return rgbToInt(126, 0, 35);
    } else if (aq >= 201) {
        // Purple
        return rgbToInt(143, 63, 151);
    } else if (aq >= 151) {
        // Red
        return rgbToInt(255, 0, 0);        
    } else if (aq >= 101) {
        // Orange
        return rgbToInt(255, 126, 0);
    } else if (aq >= 51) {
        // Yellow
        return rgbToInt(255, 255, 0);
    } else if (aq >= 0) {
        // Green
        return rgbToInt(0, 128, 0);
    }
    return rgbToInt(0,0,0);
}

int mapAQItoAltColor(float aq) {
    if (aq >= 301) {
        // Maroon 
        return rgbToInt(200, 110, 135);
    } else if (aq >= 201) {
        // Purple
        return rgbToInt(200, 153, 204);
    } else if (aq >= 151) {
        // Red
        return rgbToInt(245, 157, 157);        
    } else if (aq >= 101) {
        // Orange
        return rgbToInt(235, 171, 108);
    } else if (aq >= 51) {
        // Yellow
        return rgbToInt(242, 242, 153);
    } else if (aq >= 0) {
        // Green
        return rgbToInt(123, 224, 123);
    }
    return rgbToInt(0,0,0);
}

#define BUF_SIZE (1024)

static void readpms7003_task_2(void* pvParameters)
{
    ESP_LOGI(TAG, "readpms7003_task started: will start reading after 30 seconds");
    vTaskDelay(pdMS_TO_TICKS(30000)); // Read more frequently than transmit to ensure the messages are not erased from buffer.


    esp_err_t err = Core2ForAWS_Port_PinMode(PORT_C_UART_TX_PIN, UART);
    if (err == ESP_OK){
        Core2ForAWS_Port_C_UART_Begin(9600);
        int rxBytes;
        uint8_t *data = heap_caps_malloc(UART_RX_BUF_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM); // Allocate space for message in external RAM
        while (1) {
            rxBytes = Core2ForAWS_Port_C_UART_Receive(data);
            if (rxBytes > 0) {
                // ESP_LOGI(TAG, "Read %d bytes from UART. Received: '%s'", rxBytes, data);
            } else {
                // ESP_LOGI(TAG, "Read NO Data from UART: %d bytes", rxBytes);
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // Read more frequently than transmit to ensure the messages are not erased from buffer.
        }
        free(data); // Free memory from external RAM
    }

}

static void readpms7003_task(void* pvParameters)
{
    ESP_LOGI(TAG, "readpms7003_task started");
    // Read more frequently than transmit to ensure the messages are not erased from buffer.

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    uart_param_config(PORT_C_UART_NUM, &uart_config);
    esp_err_t err;
    err = uart_driver_install(PORT_C_UART_NUM, UART_RX_BUF_SIZE, 0, 0, NULL, 0);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "UART driver installation failed for UART num %d. Error code: 0x%x.", PORT_C_UART_NUM, err);
    }    
    err = uart_set_pin(PORT_C_UART_NUM, PORT_C_UART_TX_PIN, PORT_C_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to set pins %d, %d, to  UART%d. Error code: 0x%x.", PORT_C_UART_RX_PIN, PORT_C_UART_TX_PIN, PORT_C_UART_NUM, err);
    }    

    int rxBytes = 0;
    int cached_buffer_length = 0;
    // Configure a temporary buffer for the incoming data
    uint8_t *data = heap_caps_malloc(UART_RX_BUF_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM); // Allocate space for message in external RAM

    while (1) {
        rxBytes = 0;
        esp_err_t err = uart_get_buffered_data_len(PORT_C_UART_NUM, (size_t*)&cached_buffer_length);
        if (err != ESP_OK){
            ESP_LOGE(TAG, "Failed to get UART ring buffer length. Check if pins were set to UART and has been configured.");
            abort();
        }

        if (cached_buffer_length) {
            // ESP_LOGI(TAG, "UART cache buffer length %d", cached_buffer_length);

            rxBytes = uart_read_bytes(PORT_C_UART_NUM, data, (size_t)&cached_buffer_length, 20 / portTICK_RATE_MS);
        }
        if (rxBytes > 0) {
            // ESP_LOGI(TAG, "Read %d bytes from UART. Received: '%s'", rxBytes, data);

            pms7003Data(data,rxBytes);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Read more frequently than transmit to ensure the messages are not erased from buffer.
    }
    free(data); // Free memory from external RAM
}

void pms7003Data(uint8_t *data, int length) 
{
    if (length != 32) {
        ESP_LOGE(TAG, "pms7003Data: ERROR: Not 32 bytes");
        return;
    }
    int startChar1 = data[0];
    int startChar2 = data[1];

    if (startChar1 != 0x42) {
        ESP_LOGE(TAG, "pms7003Data: ERROR: Start Char 1 is not 0x42");
        return;
    }

    if (startChar2 != 0x4d) {
        ESP_LOGE(TAG, "pms7003Data: ERROR: Start Char 2 is not 0x4d");
        return;
    }

    uint16_t frameLength = ((data[2] << 8) | data[3]);
    if (frameLength != 28) {
        ESP_LOGE(TAG, "pms7003Data: ERROR: frameLength NOT 28 bytes");
        return;
    }
    uint16_t PM1_0_SP_UGM3 = ((data[4] << 8) | data[5]);
    uint16_t PM2_5_SP_UGM3 = ((data[6] << 8) | data[7]);
    uint16_t PM10_SP_UGM3 = ((data[8] << 8) | data[9]);

    uint16_t PM1_0_AE_UGM3 = ((data[10] << 8) | data[11]);
    uint16_t PM2_5_AE_UGM3 = ((data[12] << 8) | data[13]);
    uint16_t PM10_AE_UGM3 = ((data[14] << 8) | data[15]);

    uint16_t NP_03_UM = ((data[16] << 8) | data[17]);
    uint16_t NP_05_UM = ((data[18] << 8) | data[19]);
    uint16_t NP_1_0_UM = ((data[20] << 8) | data[21]);
    uint16_t NP_2_5_UM = ((data[22] << 8) | data[23]);
    uint16_t NP_5_0_UM = ((data[24] << 8) | data[25]);
    uint16_t NP_10_UM = ((data[26] << 8) | data[27]);

    uint16_t DATA_RESERVED = ((data[28] << 8) | data[29]);

    uint16_t checksum = ((data[30] << 8) | data[31]);

    uint16_t calculatedChecksum = 0;
    for (int i = 0; i< 30; i++) {
        calculatedChecksum += data[i];
    }

    if (calculatedChecksum != checksum) {
        ESP_LOGE(TAG, "pms7003Data: ERROR: Checksum mismatch! Calculated = %d , Actual = %d", calculatedChecksum, checksum);
    }             
    xSemaphoreTake(xPmsSemaphore, portMAX_DELAY);
    pmsData.PM1_0_SP_UGM3 = PM1_0_SP_UGM3;
    pmsData.PM2_5_SP_UGM3 = PM2_5_SP_UGM3;
    pmsData.PM10_SP_UGM3 = PM10_SP_UGM3;
    pmsData.PM1_0_AE_UGM3 = PM1_0_AE_UGM3;
    pmsData.PM2_5_AE_UGM3 = PM2_5_AE_UGM3;
    pmsData.PM10_AE_UGM3 = PM10_AE_UGM3;
    pmsData.NP_03_UM = NP_03_UM;
    pmsData.NP_05_UM = NP_05_UM;
    pmsData.NP_1_0_UM = NP_1_0_UM;
    pmsData.NP_2_5_UM = NP_2_5_UM;
    pmsData.NP_5_0_UM = NP_5_0_UM;
    pmsData.NP_10_UM = NP_10_UM;
    xSemaphoreGive(xPmsSemaphore);                     

    // printf("PM 1.0 (SP) = %d\n", PM1_0_SP_UGM3);
    // printf("PM 2.5 (SP) = %d\n", PM2_5_SP_UGM3);
    // printf("PM 10 (SP) = %d\n", PM10_SP_UGM3);
    // printf("PM 1.0 (AE) = %d\n", PM1_0_AE_UGM3);
    // printf("PM 2.5 (AE) = %d\n", PM2_5_AE_UGM3);
    // printf("PM 10 (AE) = %d\n", PM10_AE_UGM3);

    
}