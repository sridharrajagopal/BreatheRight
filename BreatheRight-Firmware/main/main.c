
/*
 * Breathe Right - v1.0
 * Based on AWS IoT EduKit examples Cloud Connected Blinky, Factory-Firmware and Smart Thermostat
*/


/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Cloud Connected Blinky v1.3.1
 * main.c
 * 
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Additions Copyright 2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/**
 * @file main.c
 * @brief simple MQTT publish, subscribe, and device shadows for use with AWS IoT EduKit reference hardware.
 *
 * This example takes the parameters from the build configuration and establishes a connection to AWS IoT Core over MQTT.
 *
 * Some configuration is required. Visit https://edukit.workshop.aws
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

#include "core2forAWS.h"

#include "wifi.h"
#include "blink.h"
#include "ui.h"
#include "pms7003.h"
#include "edge_impulse.h"

/* The time between each MQTT message publish in milliseconds */
#define PUBLISH_INTERVAL_MS 3000
#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 200


/* The time prefix used by the logger. */
static const char *TAG = "MAIN";

/* The FreeRTOS task handler for the blink task that can be used to control the task later */
TaskHandle_t xBlink;

uint16_t hqiStatus = 0;

float temperature = 0.0f;
float humidity = 0.0f;
float pressure = 0.0f;
uint16_t pm1_0 = 0;
uint16_t pm2_5 = 0;
uint16_t pm10 = 0;
uint16_t coughs = 0;
uint16_t sneezes = 0;

extern PMS7003_DATA pmsData;
extern SemaphoreHandle_t xPmsSemaphore;

extern BME280_DATA bmeData;
extern SemaphoreHandle_t xBmeSemaphore;

extern EI_DATA eiData;
extern SemaphoreHandle_t xEISemaphore;

/* CA Root certificate */
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");

/* Default MQTT HOST URL is pulled from the aws_iot_config.h */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/* Default MQTT port is pulled from the aws_iot_config.h */
uint32_t port = AWS_IOT_MQTT_PORT;

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
                                    IoT_Publish_Message_Params *params, void *pData) {
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *)params->payload);

}

void disconnect_callback_handler(AWS_IoT_Client *pClient, void *data) {
    ESP_LOGW(TAG, "MQTT Disconnect");
    ui_textarea_add("Disconnected from AWS IoT Core...", NULL, 0);
    IoT_Error_t rc = FAILURE;

    if(pClient == NULL) {
        return;
    }

    if(aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

static bool shadowUpdateInProgress;

void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                                const char *pReceivedJsonDocument, void *pContextData) {
    IOT_UNUSED(pThingName);
    IOT_UNUSED(action);
    IOT_UNUSED(pReceivedJsonDocument);
    IOT_UNUSED(pContextData);

    shadowUpdateInProgress = false;

    if(SHADOW_ACK_TIMEOUT == status) {
        ESP_LOGE(TAG, "Update timed out");
    } else if(SHADOW_ACK_REJECTED == status) {
        ESP_LOGE(TAG, "Update rejected");
    } else if(SHADOW_ACK_ACCEPTED == status) {
        ESP_LOGI(TAG, "Update accepted");
    }
} 

void healthQualityIndex_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    uint16_t status = (uint16_t) (pContext->pData);

    if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - healthQualityIndex state changed to %d", status);
    }

    // Update UI with this information
    hqiStatus = status;
}



void aws_iot_task(void *param) {
    IoT_Error_t rc = FAILURE;

    char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
    size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

    jsonStruct_t temperatureHandler;
    temperatureHandler.cb = NULL;
    temperatureHandler.pKey = "temperature";
    temperatureHandler.pData = &temperature;
    temperatureHandler.type = SHADOW_JSON_FLOAT;
    temperatureHandler.dataLength = sizeof(float);

    jsonStruct_t humidityHandler;
    humidityHandler.cb = NULL;
    humidityHandler.pKey = "humidity";
    humidityHandler.pData = &humidity;
    humidityHandler.type = SHADOW_JSON_FLOAT;
    humidityHandler.dataLength = sizeof(float);   

    jsonStruct_t pressureHandler;
    pressureHandler.cb = NULL;
    pressureHandler.pKey = "pressure";
    pressureHandler.pData = &pressure;
    pressureHandler.type = SHADOW_JSON_FLOAT;
    pressureHandler.dataLength = sizeof(float);    

    jsonStruct_t pm1_0Handler;
    pm1_0Handler.cb = NULL;
    pm1_0Handler.pKey = "PM1_0";
    pm1_0Handler.pData = &pm1_0;
    pm1_0Handler.type = SHADOW_JSON_UINT16;
    pm1_0Handler.dataLength = sizeof(uint16_t);     

    jsonStruct_t pm2_5Handler;
    pm2_5Handler.cb = NULL;
    pm2_5Handler.pKey = "PM2_5";
    pm2_5Handler.pData = &pm2_5;
    pm2_5Handler.type = SHADOW_JSON_UINT16;
    pm2_5Handler.dataLength = sizeof(uint16_t);    

    jsonStruct_t pm10Handler;
    pm10Handler.cb = NULL;
    pm10Handler.pKey = "PM10";
    pm10Handler.pData = &pm10;
    pm10Handler.type = SHADOW_JSON_UINT16;
    pm10Handler.dataLength = sizeof(uint16_t);

    jsonStruct_t coughsHandler;
    coughsHandler.cb = NULL;
    coughsHandler.pKey = "coughs";
    coughsHandler.pData = &coughs;
    coughsHandler.type = SHADOW_JSON_UINT16;
    coughsHandler.dataLength = sizeof(uint16_t);

    jsonStruct_t sneezesHandler;
    sneezesHandler.cb = NULL;
    sneezesHandler.pKey = "sneezes";
    sneezesHandler.pData = &sneezes;
    sneezesHandler.type = SHADOW_JSON_UINT16;
    sneezesHandler.dataLength = sizeof(uint16_t); 

    jsonStruct_t hqiStatusActuator;
    hqiStatusActuator.cb = healthQualityIndex_Callback;
    hqiStatusActuator.pKey = "hqiStatus";
    hqiStatusActuator.pData = &hqiStatus;
    hqiStatusActuator.type = SHADOW_JSON_UINT16;
    hqiStatusActuator.dataLength = sizeof(uint16_t);

    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);    

    // initialize the mqtt client    
    AWS_IoT_Client iotCoreClient;

    ShadowInitParameters_t sp = ShadowInitParametersDefault;
    sp.pHost = HostAddress;
    sp.port = port;
    sp.enableAutoReconnect = false;
    sp.disconnectHandler = disconnect_callback_handler;

    sp.pRootCA = (const char *)aws_root_ca_pem_start;
    sp.pClientCRT = "#";
    sp.pClientKey = "#0";    

    
#define CLIENT_ID_LEN (ATCA_SERIAL_NUM_SIZE * 2)

    char *client_id = malloc(CLIENT_ID_LEN + 1);
    ATCA_STATUS ret = Atecc608_GetSerialString(client_id);
    if (ret != ATCA_SUCCESS) {
        ESP_LOGE(TAG, "Failed to get device serial from secure element. Error: %i", ret);
        abort();
    }

    ui_textarea_add("\nDevice client Id:\n>> %s <<\n", client_id, CLIENT_ID_LEN);

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Shadow Init");

    rc = aws_iot_shadow_init(&iotCoreClient, &sp);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_shadow_init returned error %d, aborting...", rc);
        abort();
    }

    ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
    scp.pMyThingName = client_id;
    scp.pMqttClientId = client_id;
    scp.mqttClientIdLen = CLIENT_ID_LEN;

    ESP_LOGI(TAG, "Shadow Connect");
    rc = aws_iot_shadow_connect(&iotCoreClient, &scp);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_shadow_connect returned error %d, aborting...", rc);
        abort();
    }
    ui_textarea_add("\nConnected to AWS IoT Core and pub/sub to the device shadow state\n", NULL, 0);    



    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_shadow_set_autoreconnect_status(&iotCoreClient, true);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d, aborting...", rc);
        abort();
    }

    // register delta callback for hqiStatus
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &hqiStatusActuator);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Shadow Register Delta Error");
    }

    // loop and publish changes
    while(NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc) {
        rc = aws_iot_shadow_yield(&iotCoreClient, 200);
        if(NETWORK_ATTEMPTING_RECONNECT == rc || shadowUpdateInProgress) {
            rc = aws_iot_shadow_yield(&iotCoreClient, 1000);
            // If the client is attempting to reconnect, or already waiting on a shadow update,
            // we will skip the rest of the loop.
            continue;
        }

        // START get sensor readings
        // sample temperature, convert to fahrenheit
        MPU6886_GetTempData(&temperature);
        temperature = (temperature * 1.8)  + 32 - 50;

        xSemaphoreTake(xBmeSemaphore, portMAX_DELAY);
        temperature = bmeData.temperatureC;
        humidity = bmeData.humidityP;
        pressure = bmeData.pressureB;
        xSemaphoreGive(xBmeSemaphore);

        xSemaphoreTake(xPmsSemaphore, portMAX_DELAY);
        pm1_0 = pmsData.PM1_0_AE_UGM3;
        pm2_5 = pmsData.PM2_5_AE_UGM3;
        pm10 = pmsData.PM10_AE_UGM3;
        xSemaphoreGive(xPmsSemaphore); 

        xSemaphoreTake(xEISemaphore, portMAX_DELAY);
        coughs = eiData.coughs;
        sneezes = eiData.sneezes;
        // Reset the coughs/sneezes for the next interval
        eiData.coughs = 0;
        eiData.sneezes = 0;
        xSemaphoreGive(xEISemaphore);
    

        // END get sensor readings

        ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "On Device: temperature %f", temperature);
        ESP_LOGI(TAG, "On Device: humidity %f", humidity);
        ESP_LOGI(TAG, "On Device: pressure %f", pressure);
        ESP_LOGI(TAG, "On Device: pm1_0 %d", pm1_0);
        ESP_LOGI(TAG, "On Device: pm2_5 %d", pm2_5);
        ESP_LOGI(TAG, "On Device: pm10 %d", pm10);
        ESP_LOGI(TAG, "On Device: coughs %d", coughs);
        ESP_LOGI(TAG, "On Device: sneezes %d", sneezes);    
        ESP_LOGI(TAG, "On Device: hqiStatus %d", hqiStatus);
       

        rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
        if(SUCCESS == rc) {
            rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 9, &temperatureHandler,
                                             &humidityHandler, &pressureHandler, &pm1_0Handler, &pm2_5Handler, 
                                             &pm10Handler, &coughsHandler, &sneezesHandler, &hqiStatusActuator);
            if(SUCCESS == rc) {
                rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
                if(SUCCESS == rc) {
                    ESP_LOGI(TAG, "Update Shadow: %s", JsonDocumentBuffer);
                    rc = aws_iot_shadow_update(&iotCoreClient, client_id, JsonDocumentBuffer,
                                               ShadowUpdateStatusCallback, NULL, 9, true);
                    shadowUpdateInProgress = true;
                }
            }
        }
        ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));

        // Update every 60 seconds
        vTaskDelay(pdMS_TO_TICKS(60000));
    }

    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "An error occurred in the loop %d", rc);
    }

    ESP_LOGI(TAG, "Disconnecting");
    rc = aws_iot_shadow_disconnect(&iotCoreClient);

    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Disconnect error %d", rc);
    }

    vTaskDelete(NULL);

}

void app_main()
{
    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(80);
    
    blink_init();
    ui_init();
    initialise_wifi();

    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 4096 * 2, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(&blink_task, "blink_task", 4096 * 1, NULL, 2, &xBlink, 1);
}
