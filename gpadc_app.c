/*
 * gpadc_app.c
 *
 *  Created on: Feb 6, 2026
 *      Author: User
 *      Application logic of GPADC
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "osal.h"
#include "ad_gpadc.h"
#include "platform_devices.h"
#include "gpadc_app.h"

#define CHAN0_DEVICE   ADC_CH0_DEVICE
#define CHAN1_DEVICE   ADC_CH1_DEVICE

#define BUF_SIZE  100   // 100 samples = 200ms worth at 500Hz

/*  The hardware configuration happens at main.c inside prvSetupHardware()
 *  because I chose centralized hardware init.
 *  So, this function will be empty!
 */
void gpadc_app_init(void) {}


/* The GPADC Adapter is used following the pattern of
 *      [  --> Open - Read - Close <--  ]
 * this happens for both channels. printf() happens in butches, not
 * for every conversion and the time for 100 sample reading is also printed.
 */
void gpadc_app_task(void *pvParameters)
{
    printf("\n\r***GPADC dual-channel read - Raw, mV ***\n\r\n");

    /* Buffer is static — 4 arrays × 100 × 2-4 bytes = ~1.2 KB. Declaring them as static
     * puts them in RAM rather than the task stack, avoiding stack overflow. */
    static uint16_t buf0[BUF_SIZE];
    static uint16_t buf1[BUF_SIZE];
    static uint32_t bufmv0[BUF_SIZE];
    static uint32_t bufmv1[BUF_SIZE];
    uint16_t idx = 0;

    TickType_t t_start    = xTaskGetTickCount();  // for vTaskDelayUntil — don't touch this
    TickType_t t_batch    = xTaskGetTickCount();  // separate reference for timing print

    for (;;) {

        /* ---- CH0 ---- */
        ad_gpadc_handle_t h0 = ad_gpadc_open(CHAN0_DEVICE);
        if (!h0) {
            printf("[GPADC] open ch0 failed\n"); fflush(stdout);
            vTaskDelayUntil(&t_start, pdMS_TO_TICKS(2));
            continue;
        }
        int ret = ad_gpadc_read_nof_conv(h0, 1, &buf0[idx]);
        if (ret == AD_GPADC_ERROR_NONE)
            bufmv0[idx] = ad_gpadc_conv_to_mvolt(CHAN0_DEVICE->drv, buf0[idx]);
        ad_gpadc_close(h0, false);

        /* ---- CH1 ---- */
        ad_gpadc_handle_t h1 = ad_gpadc_open(CHAN1_DEVICE);
        if (!h1) {
            printf("[GPADC] open ch1 failed\n"); fflush(stdout);
            vTaskDelayUntil(&t_start, pdMS_TO_TICKS(2));
            continue;
        }
        ret = ad_gpadc_read_nof_conv(h1, 1, &buf1[idx]);
        if (ret == AD_GPADC_ERROR_NONE)
            bufmv1[idx] = ad_gpadc_conv_to_mvolt(CHAN1_DEVICE->drv, buf1[idx]);
        ad_gpadc_close(h1, false);

        idx++;

        /* ---- Flush when full ---- */
        if (idx >= BUF_SIZE) {

            // Measure BEFORE printf — only sampling time, not print time
            uint32_t batch_ms = (uint32_t)((xTaskGetTickCount() - t_batch) * portTICK_PERIOD_MS);
            printf("[timing] 100 samples took %" PRIu32 " ms\n", batch_ms);

            for (int i = 0; i < BUF_SIZE; i++) {
                printf("%" PRIu16 ",%" PRIu32 ",%" PRIu16 ",%" PRIu32 "\n",
                       buf0[i], bufmv0[i], buf1[i], bufmv1[i]);
            }
            fflush(stdout);

            idx = 0;
            t_batch = xTaskGetTickCount();  // reset batch timer AFTER flush
        }

        /* ---- Pace to exactly 2ms per cycle ---- */
        vTaskDelayUntil(&t_start, pdMS_TO_TICKS(2));
    }
}


