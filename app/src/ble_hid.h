#ifndef BLE_HID_H
#define BLE_HID_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>

#define MOUSE_BTN_LEFT     0x01
#define MOUSE_BTN_RIGHT    0x02
#define MOUSE_BTN_MIDDLE   0x04
#define MOUSE_BTN_FORWARD  0x08
#define MOUSE_BTN_BACKWARD 0x10

int ble_hid_init(void);
int ble_hid_mouse_report(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel, uint8_t battery);

#endif // BLE_HID_H
