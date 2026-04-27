# GPADC Dual-Channel — DA14706 (Renesas)

Read and calibrate two analog input signals simultaneously using the GPADC peripheral of the DA14706 DevKit. Built on FreeRTOS with the Renesas adapter layer (`ad_gpadc`).

---

## Overview

The firmware samples two ADC channels sequentially at a high rate and outputs calibrated millivolt values over UART. It is designed as a building block for a larger energy monitoring system that measures voltage and current signals (50 Hz, 0–3 V range) and transmits data via BLE to a central node.

---

## Hardware

| Item | Detail |
|---|---|
| MCU | Renesas DA14706 Pro DevKit |
| ADC pins | CH0 → P0\_5, CH1 → P0\_6 |
| Input range | 0 V to 3.0 V (3.6 V attenuator setting) |
| Signal type | Single-ended |
| Tested input | 50 Hz sine wave, 0.2 V – 3.0 V, from a function generator |

---

## Software

**RTOS:** FreeRTOS (via Renesas SDK)

**Key files:**

| File | Purpose |
|---|---|
| `main.c` | Hardware init, clock setup, task creation |
| `gpadc_app.c` | ADC read loop, calibration, UART output |
| `gpadc_app.h` | Task and init function declarations |
| `config/` | Platform device configuration (pin assignments, ADC driver config) |

---

## ADC Configuration

Defined in `platform_devices.c` (inside `config/`):

```c
.input_mode       = HW_GPADC_INPUT_MODE_SINGLE_ENDED
.input_attenuator = HW_GPADC_INPUT_VOLTAGE_UP_TO_3V6
.oversampling     = HW_GPADC_OVERSAMPLING_4_SAMPLES
.chopping         = true
.sample_time      = 4
```

Oversampling and chopping are enabled to reduce noise and internal ADC offset. Each channel is opened, read, and closed individually per sample following the adapter pattern required by the DA14706 driver.

---

## Calibration

A two-point linear calibration is applied per channel to correct for offset and gain error introduced by the SDK's `ad_gpadc_conv_to_mvolt()` conversion:

```c
corrected_mv = (uncalibrated_mv - OFFSET) / GAIN
```

Calibration constants are defined separately for each channel in `gpadc_app.c`:

```c
// will be added in a future calibration, right now the measurements are really good
#define OFFSET_MV_CH0    0.0f
#define GAIN_CH0         1.0f

#define OFFSET_MV_CH1    0.0f
#define GAIN_CH1         1.0f
```

These were derived empirically by applying known DC voltages (measured with a DMM directly at the MCU pin) and recording the raw `ad_gpadc_conv_to_mvolt()` output. They are specific to the 3.6 V attenuator setting and the current ADC configuration — recalibration is required if either changes.

> **Note:** Final calibration against a reference power meter (HAMEG HM8115-2) is planned as a later step.

---

## UART Output Format

Each line contains one sample pair in CSV format:

```
mv_ch0,mv_ch1
```

Example:
```
1542,1487
1603,1551
```

Baud rate: **115200**. Output is continuous with no headers or delimiters between samples.

---

## Companion Script

A Python UART analyzer (`uart_analyzer.py`, separate repository) reads this output and provides:

- Live waveform plot for both channels
- Per-channel min, max, peak-to-peak, RMS, mean
- Measured sample rate (Hz)
- Measured signal frequency via zero-crossing detection (Hz)

---

## Known Limitations

- The two channels are sampled sequentially, not simultaneously. At ~800 Hz loop rate, CH1 is sampled approximately 0.5–1 ms after CH0, introducing a small phase offset between the two signals. A software correction will be applied in the central node for power factor calculations.
- Calibration accuracy at the low end of the range (~200 mV) is limited by function generator instability at low DC output levels. This will be improved with the bench power meter calibration step.

---

## Project Context

This ADC module is one component of a larger BLE-based energy monitoring system. The full system includes:
- Temperature and humidity sensing (I²C, MikroBUS 1)
- Relay control via BLE (MikroBUS 2)
- Voltage and current measurement (this module)
- A Python central node on a laptop for data collection, processing, and control