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
#include <math.h>
#include "osal.h"
#include "ad_gpadc.h"
#include "platform_devices.h"
#include "gpadc_app.h"

#define CHAN0_DEVICE   ADC_CH0_DEVICE
#define CHAN1_DEVICE   ADC_CH1_DEVICE

#define OFFLINE_OFFSET_MV_3V6    149.0f
#define GAIN_ERROR_3V6           0.89f    // measured empirically

#define BUF_SIZE  100   // 100 samples = 200ms worth at 500Hz

/*  The hardware configuration happens at main.c inside prvSetupHardware()
 *  because I chose centralized hardware init.
 *  So, this function will be empty!
 */
void gpadc_app_init(void) {}

/* Calibrates mV measurements of ADC for 3.6V attenuation.
 * Input:  uncalibrated mV from ad_gpadc_conv_to_mvolt()
 * Output: corrected mV as float
 */
float correct_mv(uint32_t mv_uncalibrated) {
    if (mv_uncalibrated < OFFLINE_OFFSET_MV_3V6) return 0.0f;           // clamp — avoid underflow
    return (mv_uncalibrated - OFFLINE_OFFSET_MV_3V6) / GAIN_ERROR_3V6;  // remove offset, correct gain
}

/* The GPADC Adapter is used following the pattern of
 *      [  --> Open - Read - Close <--  ]
 * this happens for both channels. printf() happens in batches, not
 * for every conversion and the time for a 100 sample reading period is also printed.
 */
void gpadc_app_task(void *pvParameters)
{
    //printf("\n\r***GPADC dual-channel read - Raw, mV ***\n\r\n");

    /* Buffers declared as static — puts them in RAM rather than the task
     * stack, avoiding stack overflow. */
    static uint16_t buf0[BUF_SIZE];
    static uint16_t buf1[BUF_SIZE];
    static float    bufmv0_cal[BUF_SIZE];   // float: correct_mv() returns float
    static float    bufmv1_cal[BUF_SIZE];
    uint16_t idx = 0;
    int ret = 0;
    uint16_t dummy = 0;

    TickType_t t_start = xTaskGetTickCount();  // for vTaskDelayUntil — do not modify manually
    TickType_t t_batch = xTaskGetTickCount();  // separate reference for batch timing print

    /* --- One-time warmup dummy read for each channel --- */
    ad_gpadc_handle_t h_warm = ad_gpadc_open(CHAN0_DEVICE);
    if (h_warm) { ad_gpadc_read_nof_conv(h_warm, 1, &dummy); ad_gpadc_close(h_warm, false); }
    h_warm = ad_gpadc_open(CHAN1_DEVICE);
    if (h_warm) { ad_gpadc_read_nof_conv(h_warm, 1, &dummy); ad_gpadc_close(h_warm, false); }


    for (;;) {

        /* ---- CH0 Open - Read - Close ---- */
        ad_gpadc_handle_t h0 = ad_gpadc_open(CHAN0_DEVICE);
        if (!h0) {
            printf("[GPADC] open ch0 failed\n");
            fflush(stdout);
            vTaskDelayUntil(&t_start, pdMS_TO_TICKS(2));
            continue;
        }
        ret = ad_gpadc_read_nof_conv(h0, 1, &buf0[idx]);
        if (ret == AD_GPADC_ERROR_NONE) {
            bufmv0_cal[idx] = correct_mv(ad_gpadc_conv_to_mvolt(CHAN0_DEVICE->drv, buf0[idx]));
        }
        ad_gpadc_close(h0, false);

        /* ---- CH1 Open - Read - Close ---- */
        ad_gpadc_handle_t h1 = ad_gpadc_open(CHAN1_DEVICE);
        if (!h1) {
            printf("[GPADC] open ch1 failed\n");
            fflush(stdout);
            vTaskDelayUntil(&t_start, pdMS_TO_TICKS(2));
            continue;
        }
        ret = ad_gpadc_read_nof_conv(h1, 1, &buf1[idx]);
        if (ret == AD_GPADC_ERROR_NONE) {
            bufmv1_cal[idx] = correct_mv(ad_gpadc_conv_to_mvolt(CHAN1_DEVICE->drv, buf1[idx]));
        }
        ad_gpadc_close(h1, false);

        idx++;

        /* ---- Flush + stats when buffer full ---- */
        if (idx >= BUF_SIZE) {

            uint32_t batch_ms = (uint32_t)((xTaskGetTickCount() - t_batch) * portTICK_PERIOD_MS);
            t_batch = xTaskGetTickCount();  // reset BEFORE printf
            idx = 0;

            /* --- Compute stats for CH0 (calibrated mV) --- */
            float min0 = bufmv0_cal[0], max0 = bufmv0_cal[0];
            float sum_sq0 = 0.0f;
            for (int i = 0; i < BUF_SIZE; i++) {
                if (bufmv0_cal[i] < min0) min0 = bufmv0_cal[i];
                if (bufmv0_cal[i] > max0) max0 = bufmv0_cal[i];
                sum_sq0 += bufmv0_cal[i] * bufmv0_cal[i];
            }
            float rms0 = sqrtf(sum_sq0 / BUF_SIZE);

            /* --- Compute stats for CH1 (calibrated mV) --- */
            float min1 = bufmv1_cal[0], max1 = bufmv1_cal[0];
            float sum_sq1 = 0.0f;
            for (int i = 0; i < BUF_SIZE; i++) {
                if (bufmv1_cal[i] < min1) min1 = bufmv1_cal[i];
                if (bufmv1_cal[i] > max1) max1 = bufmv1_cal[i];
                sum_sq1 += bufmv1_cal[i] * bufmv1_cal[i];
            }
            float rms1 = sqrtf(sum_sq1 / BUF_SIZE);

            /* --- Print timing --- */
            printf("[timing] 100 samples took %" PRIu32 " ms\n", batch_ms);

            /* --- Print stats (integer math — no float printf) --- */
            printf("[CH0] min=%d mV, max=%d mV, pp=%d mV, rms=%d.%02d mV\n",
                   (int)min0, (int)max0, (int)(max0 - min0),
                   (int)rms0, (int)((rms0 - (int)rms0) * 100));
            printf("[CH1] min=%d mV, max=%d mV, pp=%d mV, rms=%d.%02d mV\n",
                   (int)min1, (int)max1, (int)(max1 - min1),
                   (int)rms1, (int)((rms1 - (int)rms1) * 100));

            /* --- Print raw samples: raw0, cal_mV0, raw1, cal_mV1 --- */
            for (int i = 0; i < BUF_SIZE; i++) {
                printf("%" PRIu16 ",%d,%" PRIu16 ",%d\n",
                       buf0[i], (int)bufmv0_cal[i],
                       buf1[i], (int)bufmv1_cal[i]);
            }
            fflush(stdout);
        }

        vTaskDelayUntil(&t_start, pdMS_TO_TICKS(2));
    }
}
