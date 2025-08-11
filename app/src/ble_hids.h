#ifndef SIMPLEMOUSE_HIDS_H
#define SIMPLEMOUSE_HIDS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <zephyr/bluetooth/conn.h>

#include "ble_hids_def.h"

    void ble_hids_connected(struct bt_conn *conn);
    void ble_hids_disconnected(void);

    ble_hids_prot_mode_t ble_hids_get_prot_mode(void);

    bool ble_hids_is_mouse_report_writable(void);
    void ble_hids_mouse_notify_input(const void *data, uint8_t dataLen);
    void ble_hids_mouse_notify_boot(const void *data, uint8_t dataLen);

    void ble_hids_send_mouse_notification(bool left, bool right, int16_t move_x, int16_t move_y, int8_t scroll_v);

    void ble_hids_test_mouse_square(void);

#ifdef __cplusplus
}
#endif

#endif /* SIMPLEMOUSE_HIDS_H */
