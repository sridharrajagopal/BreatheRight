#include <stdio.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include "edge_impulse.h"
#include <Cough_Tutorial_inferencing.h> 

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "driver/i2s.h"
#include "esp_log.h"

extern "C" {
#include "microphone.h"

EI_DATA eiData;
SemaphoreHandle_t xEISemaphore;
}

static const char *TAG = EDGEIMPULSE_TAB_NAME;


static bool microphone_inference_start(uint32_t n_samples);
static bool microphone_inference_record(void);
static void microphone_inference_end(void);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);

/** Audio buffers, pointers and selectors */
typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples; 
} inference_t;

TaskHandle_t mic_handle, inference_handle;

static inference_t inference;
static bool record_ready = false;
static signed short *sampleBuffer;
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);


extern "C" void edge_impulse_start() {
    // summary of inferencing settings (from model_metadata.h)
    printf("Inferencing settings:\n");
    printf("\tInterval: %.2f ms.\n", (float)EI_CLASSIFIER_INTERVAL_MS);
    printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
    printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) /
                                            sizeof(ei_classifier_inferencing_categories[0]));

    xEISemaphore = xSemaphoreCreateMutex();
    xSemaphoreTake(xEISemaphore, portMAX_DELAY);
    eiData.coughs = 0;
    eiData.sneezes = 0;
    xSemaphoreGive(xEISemaphore);                                          

    run_classifier_init();
    if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
        printf("ERR: Failed to setup audio sampling\r\n");
        return;
    }

}

void microphoneTask(void* pvParameters) {
    ESP_LOGI(TAG, "Starting microphone Task");

    vTaskDelay(pdMS_TO_TICKS(10000));

    // static int8_t i2s_readraw_buff[1024];
    size_t bytesread;
    // int16_t* buffptr;
    double data = 0;
    // ESP_LOGI(TAG, "Calling Microphone_Init ...");
    Microphone_Init();
    // ESP_LOGI(TAG, "Done calling Microphone_Init ...");
    // QueueHandle_t queue = (QueueHandle_t) pvParameters;

    for (;;) {
        // ESP_LOGI(TAG, "Calling i2s_read ...");
        i2s_read(I2S_NUM_0, (char *)&sampleBuffer[0], (inference.n_samples >> 3), &bytesread, pdMS_TO_TICKS(100));
        // ESP_LOGI(TAG, "i2s_read bytesread:%d", bytesread);

        // buffptr = (int16_t*)i2s_readraw_buff;

        if (record_ready == true) {
            // ESP_LOGI(TAG, "Copying samples to inference buffers");
            for (int i = 0; i<bytesread>> 1; i++) {
                inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

                if (inference.buf_count >= inference.n_samples) {
                    // ESP_LOGI(TAG, "buffer full. flipping buffers");            
                    inference.buf_select ^= 1;
                    inference.buf_count = 0;
                    inference.buf_ready = 1;
                }
            }
        }       
        vTaskDelay(pdMS_TO_TICKS(10));
 

    }
    vTaskDelete(NULL); // Should never get to here...
}

void inferenceTask(void* pvParameters) {
    ESP_LOGI(TAG, "Starting inference Task");
    vTaskDelay(pdMS_TO_TICKS(9000));

    for (;;) {

        // ESP_LOGI(TAG, "Calling microphone_inference_record");
        bool m = microphone_inference_record();
        if (!m) {
            printf("ERR: Failed to record audio...\n");
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        signal_t signal;
        signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
        signal.get_data = &microphone_audio_signal_get_data;
        ei_impulse_result_t result = {0};

        // printf("calling run_classifier_continuous...");
        EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
        if (r != EI_IMPULSE_OK) {
            printf("ERR: Failed to run classifier (%d)\n", r);
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)) {
            int cough = 0;
            int sneeze = 0;
            // print the predictions
            printf("Predictions ");
            printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);
            printf(": \n");
            for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                printf("    %s: %.5f\n", result.classification[ix].label,
                        result.classification[ix].value);
                if (!strcmp(result.classification[ix].label, "cough") && result.classification[ix].value > 0.8) {
                    cough++;
                } else if (!strcmp(result.classification[ix].label, "sneeze") && result.classification[ix].value > 0.8) {
                    sneeze++;
                }
            }
    #if EI_CLASSIFIER_HAS_ANOMALY == 1
            printf("    anomaly score: %.3f\n", result.anomaly);
    #endif
            xSemaphoreTake(xEISemaphore, portMAX_DELAY);
            eiData.coughs += cough;
            eiData.sneezes += sneeze;
            xSemaphoreGive(xEISemaphore);       

            print_results = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL); // Should never get to here...

}


/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[0] == NULL) {
        return false;
    }

    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[0] == NULL) {
        free(inference.buffers[0]);
        return false;
    }

    sampleBuffer = (signed short *)malloc((n_samples >> 3) * sizeof(signed short));

    if (sampleBuffer == NULL) {
        free(inference.buffers[0]);
        free(inference.buffers[1]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    record_ready = true;

    xTaskCreatePinnedToCore(microphoneTask, "microphoneTask", 4096 * 2, NULL, 1, &mic_handle, 1);
    xTaskCreatePinnedToCore(inferenceTask, "inferenceTask", 4096 * 2, NULL, 1, &inference_handle, 1);

    // // configure the data receive callback
    // PDM.onReceive(&pdm_data_ready_inference_callback);

    // PDM.setBufferSize((n_samples >> 1) * sizeof(int16_t));

    // // initialize PDM with:
    // // - one channel (mono mode)
    // // - a 16 kHz sample rate
    // if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
    //     printf("Failed to start PDM!");
    // }

    // // set the gain, defaults to 20
    // PDM.setGain(127);
    return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void)
{
    bool ret = true;

    if (inference.buf_ready == 1) {
        printf(
            "Error sample buffer overrun. Decrease the number of slices per model window "
            "(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)\n");
        ret = false;
    }

    while (inference.buf_ready == 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    inference.buf_ready = 0;

    return ret;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);

    return 0;
}

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void)
{
    // PDM.end();
    free(inference.buffers[0]);
    free(inference.buffers[1]);
    free(sampleBuffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif