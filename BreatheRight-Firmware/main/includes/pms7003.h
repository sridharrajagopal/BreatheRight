/*
 * Plantower PMS 7003
 * BreatheRight v1.0
 * pms7003.h
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

#pragma once

#define PM_TAB_NAME "PMS7003"

typedef struct PMS7003_DATA {
    uint16_t PM1_0_SP_UGM3;
    uint16_t PM2_5_SP_UGM3;
    uint16_t PM10_SP_UGM3;

    uint16_t PM1_0_AE_UGM3;
    uint16_t PM2_5_AE_UGM3;
    uint16_t PM10_AE_UGM3;

    uint16_t NP_03_UM;
    uint16_t NP_05_UM;
    uint16_t NP_1_0_UM;
    uint16_t NP_2_5_UM;
    uint16_t NP_5_0_UM;
    uint16_t NP_10_UM;

} PMS7003_DATA;

typedef struct BME280_DATA {
    float temperatureC;
    float humidityP;
    float pressureB;
} BME280_DATA;

TaskHandle_t pm_handle;
TaskHandle_t pms7003_handle;

void display_pm_tab(lv_obj_t* tv);
void reset_pm_bg();