# Pin Mapping Documentation for DTS Configuration

This document explains the DeviceTree (DTS) configuration and maps logical device nodes to their physical pins and functions on the PCB.

---

## Aliases

| Alias       | Node               | Description                 |
| ----------- | ------------------ | --------------------------- |
| scrolla     | `&scroll_a`        | Scroll encoder A            |
| scrollb     | `&scroll_b`        | Scroll encoder B            |
| scrollbtn   | `&scroll_btn`      | Scroll button               |
| ledstrip    | `&led_strip`       | WS2812 LED strip            |
| sw0         | `&left_button`     | Left Click                  |
| sw1         | `&right_button`    | Right Click                 |
| sw2         | `&forward_button`  | Forward button (NC on PCB)  |
| sw3         | `&backward_button` | Backward button (NC on PCB) |
| dpi-btn     | `&dpi_button`      | DPI button                  |
| fuel-gauge0 | `&max17048`        | Fuel gauge (MAX17048)       |
| glow-en     | `&glow_en`         | LED enable pin              |

---

## Buttons (gpio-keys)

| Node              | Alias     | Pin   | Active Level | Pull    | Notes                       | PCB Node  |
| ----------------- | --------- | ----- | ------------ | ------- | --------------------------- | --------- |
| `scroll_a`        | scrolla   | P1.15 | Active High  | Pull-up | Scroll encoder A            | WHEEL_A   |
| `scroll_b`        | scrollb   | P1.13 | Active High  | Pull-up | Scroll encoder B            | WHEEL_B   |
| `scroll_btn`      | scrollbtn | P1.10 | Active Low   | Pull-up | Scroll button (press)       | BTN_WHEEL |
| `right_button`    | sw1       | P1.11 | Active Low   | Pull-up | Right Click                 | BTN_RIGHT |
| `left_button`     | sw0       | P0.29 | Active Low   | Pull-up | Left Click                  | BTN_LEFT  |
| `forward_button`  | sw2       | P1.06 | Active Low   | Pull-up | Forward button (NC on PCB)  | BTN_FWD   |
| `backward_button` | sw3       | P1.07 | Active Low   | Pull-up | Backward button (NC on PCB) | BTN_BACK  |
| `dpi_button`      | dpi-btn   | P0.31 | Active Low   | Pull-up | DPI switch button           | BTN_DPI   |

---

## Outputs (gpio-leds)

| Node      | Alias   | Pin   | Active Level | Purpose                    | PCB NODE |
| --------- | ------- | ----- | ------------ | -------------------------- | -------- |
| `glow_en` | glow-en | P1.00 | Active High  | LED enable / power control | GLOW_EN  |

> Note: `default-state` is not supported in NCS v2.8.0, so the LED state must be controlled in firmware.

---

## HID Device

| Node        | Description                                   |
| ----------- | --------------------------------------------- |
| `hid_dev_0` | HID device, 64-byte IN report, 125 µs polling |

---

## Peripherals

| Peripheral | Pins / Address                              | Device              | Notes                       | PCB NODE              |
| ---------- | ------------------------------------------- | ------------------- | --------------------------- | --------------------- |
| **I2S0**   | P0.24 (SDOUT → GLOW_LV)                     | WS2812 LED Strip    | Chain length 5, GRB mapping | GLOW_LV               |
| **SPI1**   | SCK=P1.09, MOSI=P0.08, MISO=P0.06, CS=P0.04 | PAW3395 sensor      | IRQ=P0.11, Max freq=2 MHz   | SCLK, MOSI, MISO, NCS |
| **I2C0**   | SDA=P0.26, SCL=P0.27                        | MAX17048 fuel gauge | Address 0x36                | SDA, SCL              |
| **USB**    | —                                           | CDC ACM UART        | Console output              | -                     |

---

## Pin Control Groups

| Group              | Pins                                    | Purpose       |
| ------------------ | --------------------------------------- | ------------- |
| `i2s0_default_alt` | P0.24 (I2S SDOUT)                       | LED strip     |
| `spi1_default`     | P1.09 (SCK), P0.08 (MOSI), P0.06 (MISO) | SPI bus       |
| `spi1_sleep`       | Same as above (low power)               | Low power SPI |
| `i2c0_default`     | P0.26 (SDA), P0.27 (SCL)                | I2C bus       |
| `i2c0_sleep`       | Same as above (low power)               | Low power I2C |

---

# Summary

- **Buttons:** Scroll encoder, left/right click, DPI button → mapped with pull-ups.
- **LED Control:** `glow_en` pin (manual enable required).
- **WS2812 LED Strip:** 5 LEDs driven via I2S0 on P0.24.
- **Sensor:** PAW3395 over SPI1, IRQ on P0.11.
- **Fuel Gauge:** MAX17048 on I2C0, address 0x36.
- **USB:** Enabled with CDC-ACM for console.
