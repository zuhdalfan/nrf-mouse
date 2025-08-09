#pragma once

#include <zephyr/device.h>
#include <stdint.h>
#include <stdbool.h>

#define USB_MOUSE_REPORT_SIZE 4

#ifdef __cplusplus
extern "C" {
#endif

int usb_hid_mouse_init(void);

void usb_hid_mouse_update(bool left, bool right, bool middle, bool forward, bool back,
                          int8_t dx, int8_t dy, int8_t wheel);

void usb_hid_mouse_test(void);

#ifdef __cplusplus
}
#endif
