# **Pin Mapping – Zephyr DTS Reference**

This document summarizes the hardware pin assignments for buttons, sensors, SPI, I²S LED strip, and I²C peripherals as defined in the DTS.

---

## **1. Buttons & Scroll Wheel**

| Function         | Alias       | Port | Pin | Active State | Pull    | Notes                               |
| ---------------- | ----------- | ---- | --- | ------------ | ------- | ----------------------------------- |
| Scroll Encoder A | `scrolla`   | 0    | 10  | High         | Pull-up | Rotary encoder channel A            |
| Scroll Encoder B | `scrollb`   | 0    | 9   | High         | Pull-up | Rotary encoder channel B            |
| Scroll Button    | `scrollbtn` | 0    | 8   | Low          | Pull-up | Press switch on scroll wheel        |
| Right Click      | `sw1`       | 0    | 7   | Low          | Pull-up | `label = "Right Click"`             |
| Left Click       | `sw0`       | 0    | 6   | Low          | Pull-up | `label = "Left Click"`              |
| Forward Button   | `sw2`       | 0    | 28  | Low          | Pull-up | Side button for forward navigation  |
| Backward Button  | `sw3`       | 0    | 29  | Low          | Pull-up | Side button for backward navigation |

---

## **2. LED Strip (WS2812 via I²S)**

| Signal          | Port | Pin | Notes                                       |
| --------------- | ---- | --- | ------------------------------------------- |
| I²S SCK (Clock) | 0    | 22  | Master clock                                |
| I²S LRCK        | 0    | 23  | Left/right clock                            |
| I²S SDOUT       | 0    | 5   | LED strip data output                       |
| I²S SDIN        | 0    | 24  | Not typically used for WS2812 (but defined) |

**LED Strip Config:**

- Type: `worldsemi,ws2812-i2s`
- Chain length: **8 LEDs**
- Color mapping: GRB
- Reset delay: 120 μs

---

## **3. SPI Bus (paw3395 Optical Sensor)**

| Signal | Port | Pin | Notes                          |
| ------ | ---- | --- | ------------------------------ |
| SCK    | 0    | 19  | SPI clock                      |
| MOSI   | 0    | 20  | SPI data out                   |
| MISO   | 0    | 21  | SPI data in                    |
| CS     | 0    | 2   | Active-low chip select         |
| IRQ    | 0    | 3   | Sensor interrupt (active-high) |

**SPI Config:**

- Max frequency: 2 MHz
- Device: `pixart,paw3395`

---

## **4. I²C Bus (MAX17048 Fuel Gauge)**

| Signal | Port | Pin | Notes |
| ------ | ---- | --- | ----- |
| SDA    | 0    | 26  | Data  |
| SCL    | 0    | 27  | Clock |

**MAX17048 Config:**

- Address: 0x36
- Compatible: `maxim,max17048`

---

## **5. USB**

- Device: `zephyr,cdc-acm-uart`
- Used for console and serial over USB.

---

## **6. Alias Summary**

| Alias         | Connected To       | Purpose                    |
| ------------- | ------------------ | -------------------------- |
| `scrolla`     | `&scroll_a`        | Scroll encoder A           |
| `scrollb`     | `&scroll_b`        | Scroll encoder B           |
| `scrollbtn`   | `&scroll_btn`      | Scroll wheel button        |
| `ledstrip`    | `&led_strip`       | WS2812 LED strip           |
| `sw0`         | `&left_button`     | Left mouse button          |
| `sw1`         | `&right_button`    | Right mouse button         |
| `sw2`         | `&forward_button`  | Forward navigation button  |
| `sw3`         | `&backward_button` | Backward navigation button |
| `fuel-gauge0` | `&max17048`        | Battery fuel gauge         |
