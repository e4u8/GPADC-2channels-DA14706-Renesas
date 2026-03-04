# Using the GPADC adapter

## Overview
Two channels active, at pins P05 and P06. 

At both channels it is expected
    input: possitive signal with range that depends on attenuator (see platform_devices.c)
    output: Raw value [range between] and measurement at mV 

    Continuous Mode is false and synchronous reading is implemented, 4x oversampling, chopper is not enabled.
    Hardware interval is set to 0 since Continuous Mode is false. A small "software" interval is used for 
    better readability. Gain, offset and coarse offset calibration are not used. 

The Flowchart
┌─────────────────────────────────┐
│            main()               │
│  OS_TASK_CREATE("SysInit")      │
│  OS_TASK_SCHEDULER_RUN()        │
└────────────────┬────────────────┘
                 │
┌────────────────▼────────────────┐
│         system_init()           │
│  cm_sys_clk_init(XTAL32M)       │
│  cm_apb/ahb_set_clock_divider() │
│  cm_lp_clk_init()               │
│  prvSetupHardware()  ───────────┼──► periph_init()
│  pm_sleep_mode_set(pm_mode_idle)│      configure GPIO pins
│  OS_TASK_CREATE("GPADC")        │      P0_5 → ADC func
│  OS_TASK_DELETE(self)           │      P0_6 → ADC func
└────────────────┬────────────────┘
                 │          │
                 │          └──► ad_gpadc_io_config(CH0, OFF)
                 │               ad_gpadc_io_config(CH1, OFF)
                 │
┌────────────────▼────────────────┐
│       gpadc_app_task()          │
│  Print header to terminal       │
└────────────────┬────────────────┘
                 │
         ┌───────▼────────┐
         │   for(;;)      │◄─────────────────────────┐
         └───────┬────────┘                          │
                 │                                   │
   ┌─────────────▼──────────────┐                    │
   │  ad_gpadc_open(CH0)        │                    │
   │  OS_DELAY_MS(1)  (settle)  │                    │
   │  read_nof_conv → dummy     │  discard 1st       │
   │  read_nof_conv → raw0      │                    │
   │  conv_to_mvolt  → mv0      │                    │
   │  ad_gpadc_close(CH0)       │                    │
   └─────────────┬──────────────┘                    │
                 │                                   │
   ┌─────────────▼──────────────┐                    │
   │  ad_gpadc_open(CH1)        │                    │
   │  OS_DELAY_MS(1)  (settle)  │                    │
   │  read_nof_conv → dummy     │  discard 1st       │
   │  read_nof_conv → raw1      │                    │
   │  conv_to_mvolt  → mv1      │                    │
   │  ad_gpadc_close(CH1)       │                    │
   └─────────────┬──────────────┘                    │
                 │                                   │
   ┌─────────────▼──────────────┐                    │
   │  printf(raw0,mv0,raw1,mv1) │                    │
   │  OS_DELAY_MS(0) % this should change at the merged project
   └─────────────┬──────────────┘                    │
                 └───────────────────────────────────┘

  * The logic that was implemented with the GPADC Adapter follows the pattern [Open - Read - Close] for ch0 and ch1 accordingly. 
    The Adapter doesn't allow to use ad_gpadc_reconfig() to switch between P0_5 and P0_6 on an already-open handle. 

## HW and SW configuration

- **Hardware configuration**

  This example runs on the DA14706 Bluetooth Smart SoC device.
  - positive signal at pin P005 ch0
  - positive signal at pin P006 ch1
  - 4x attenuation (0V - 3.6V)
  - single-endeed mode: ON
  - sample time: 0        -> that is 1 ADC_CLK
  - hardware interval: 0
  - oversampling: 1
  - continuous: FALSE
  - chopping: FALSE  
  - result mode: Normal
  - internal temp sensor: FALSE 

  note about:           .voltage_level = HW_GPIO_POWER_VDD1V8P 
  This field doesn't switch the voltage — it declares to the SDK which rail is physically connected to that 
  pad on your board, so the driver knows the correct I/O voltage for that pin.
         


- **Software configuration**
  This example was tested with:
  - Smartsnippets Studio 2.0.20
  - SDK10.2.6.49
  - **SEGGER J-Link** tools should be downloaded and installed.

## How to run the example

- Import this project into the workspace
- Compile and launch RAM or OQSPI target
- Run from RAM or load in the flash
- Open a serial terminal with settings 115200/8-N-1
- The ADC reading and the voltage measurement in mV are displayed on the terminal without pressing any Key
