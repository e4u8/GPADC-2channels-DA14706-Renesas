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


/*  The hardware configuration happens at main.c inside prvSetupHardware()
 *  because I chose centralized hardware init.
 *  So, this function will be empty!
 */
void gpadc_app_init(void) {}

/* Task moved from main.c (preserves exact logic and delays) */
void gpadc_app_task(void *pvParameters)
{
    printf("\n\r***GPADC dual-channel read - Raw, mV *** \r [Single-Ended with printed conversion time at each pair measurement] \n\r\n");

    for (;;) {
        //TickType_t t0 = xTaskGetTickCount();

        /* Open - Read - Close ch0 */
        ad_gpadc_handle_t h0 = ad_gpadc_open(CHAN0_DEVICE);
        if (!h0) {
                printf("[GPADC] open ch0 failed\n");
                fflush(stdout);
                OS_DELAY_MS(100);
                continue;  // retry next iteration
        }
        OS_DELAY_MS(1);
        uint16_t dummy = 0 , raw0 = 0;
        uint32_t mv0 = 0;
        int ret = 0;    // returned value of functions

        /* Discard first ADC result after changing mux: the S/H cap can hold previous voltage and the front-end needs time to settle. */
        ad_gpadc_read_nof_conv(h0, 1, &dummy);
        ret = ad_gpadc_read_nof_conv(h0, 1, &raw0);
        if (ret == AD_GPADC_ERROR_NONE) {
            mv0 = ad_gpadc_conv_to_mvolt(CHAN0_DEVICE->drv, raw0);
        } else {
            printf("[GPADC] read ch0 failed: %d\n", ret);
            fflush(stdout);
        }
        ad_gpadc_close(h0, true);

        /* Open - Read - Close ch1 */
        ad_gpadc_handle_t h1 = ad_gpadc_open(CHAN1_DEVICE);
        if (!h1) {
            printf("[GPADC] open ch1 failed\n");
            fflush(stdout);
            OS_DELAY_MS(100);
            continue;  // retry next iteration
        }
        OS_DELAY_MS(1);
        uint16_t raw1 = 0;
        uint32_t mv1 = 0;
        
        ad_gpadc_read_nof_conv(h1, 1, &dummy);
        ret = ad_gpadc_read_nof_conv(h1, 1, &raw1);
        if (ret == AD_GPADC_ERROR_NONE) {
            mv1 = ad_gpadc_conv_to_mvolt(CHAN1_DEVICE->drv, raw1);
        } else {
            printf("[GPADC] read ch1 failed: %d\n", ret);
            fflush(stdout);
        }
        ad_gpadc_close(h1, true);

        //TickType_t t1 = xTaskGetTickCount();
        //uint32_t ms = (uint32_t)((t1 - t0) * portTICK_PERIOD_MS);
        /* Correct, portable printf using inttypes macros */
        //printf("Cycle ms: %" PRIu32 ", ch0: %" PRIu16 " -> %" PRIu32 " mV, ch1: %" PRIu16 " -> %" PRIu32 " mV\n", ms, raw0, mv0, raw1, mv1);
        printf("%" PRIu16 ",%" PRIu32 ",%" PRIu16 ",%" PRIu32 "\n", raw0, mv0, raw1, mv1);
        fflush(stdout);

        /* keep overall app rate or remove if hardware interval governs */
        OS_DELAY_MS(0);
    }

}

#if 0

    /* Here I tried with reconfig() to change the channel without closing the adapter,
     * but it is not allowed. So I followed the logic Open - Read - Close for ch0 and
     * then ch1. Kept that code for reference.  */
    ad_gpadc_handle_t h = ad_gpadc_open(CHAN0_DEVICE);

    if (!h) {
        printf("[GPADC] open failed\n"); fflush(stdout);
        for (;;) { OS_DELAY_MS(1000); } /* stop for debug */
    }

    for (;;) {
        uint16_t raw0 = 0, raw1 = 0;
        uint32_t mv0 = 0, mv1 = 0;
        int ret;        // returned value of functions

        /* Configure for channel 0 (drv_conf_ch0 must be available) */
        ret = ad_gpadc_reconfig(h, CHAN0_DEVICE->drv);
        if (ret != AD_GPADC_ERROR_NONE) {
            printf("[GPADC] reconfig ch0 failed: %d\n", ret);
            fflush(stdout);
        } else {
            /* Wait a short time for the analog mux / driver to settle.
            Single-stepping was accidentally doing this for you. */
            OS_DELAY_MS(2); /* try 2 ms; increase to 5 ms if you still see problems */

            /* Read single conversion */
            ret = ad_gpadc_read_nof_conv(h, 1, &raw0);
            if (ret == AD_GPADC_ERROR_NONE) {
                mv0 = ad_gpadc_conv_to_mvolt(CHAN0_DEVICE->drv, raw0);
            } else {
                printf("[GPADC] read ch0 failed: %d\n", ret);
                fflush(stdout);
            }
        }

        OS_DELAY_MS(1);

        /* Configure for channel 1 */
        ret = ad_gpadc_reconfig(h, CHAN1_DEVICE->drv);
        if (ret != AD_GPADC_ERROR_NONE) {
            printf("[GPADC] reconfig ch1 failed: %d\n", ret);
            fflush(stdout);
        } else {
            OS_DELAY_MS(2); /* same settle for ch1 */
            ret = ad_gpadc_read_nof_conv(h, 1, &raw1);
            if (ret == AD_GPADC_ERROR_NONE) {
                mv1 = ad_gpadc_conv_to_mvolt(CHAN1_DEVICE->drv, raw1);
            } else {
                printf("[GPADC] read ch1 failed: %d\n", ret);
                fflush(stdout);
            }
        }

       /* Print both results (CSV) */
       printf("%" PRIu16 ",%" PRIu32 ",%" PRIu16 ",%" PRIu32 "\n", raw0, mv0, raw1, mv1);
       fflush(stdout);

       /* If you want hardware interval to pace acquisition, you may not need extra delay.
       If you used vTaskDelayUntil previously, keep it. Otherwise a small OS_DELAY_MS(1)
       is harmless. */
       OS_DELAY_MS(100); /* keep the overall application rate as before */

    }
    /* unreachable */
    ad_gpadc_close(h, true);
}
#endif

