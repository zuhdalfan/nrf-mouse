/* ble_hid.c - advertising resume fixes + existing HID */

#include "ble_hid.h"
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(ble_hid, LOG_LEVEL_DBG);

#ifndef DEVICE_NAME
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#endif

#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/* HID Report Descriptor (Mouse) */
static const uint8_t hid_report_map[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x02,  // Usage (Mouse)
    0xA1, 0x01,  // Collection (Application)
    0x09, 0x01,  //   Usage (Pointer)
    0xA1, 0x00,  //   Collection (Physical)
    0x05, 0x09,  //     Usage Page (Buttons)
    0x19, 0x01,  //     Usage Minimum (01)
    0x29, 0x05,  //     Usage Maximum (05)
    0x15, 0x00,  //     Logical Minimum (0)
    0x25, 0x01,  //     Logical Maximum (1)
    0x95, 0x05,  //     Report Count (5)
    0x75, 0x01,  //     Report Size (1)
    0x81, 0x02,  //     Input (Data, Var, Abs)
    0x95, 0x01,  //     Report Count (1)
    0x75, 0x03,  //     Report Size (3)
    0x81, 0x03,  //     Input (Const, Var, Abs)
    0x05, 0x01,  //     Usage Page (Generic Desktop)
    0x09, 0x30,  //     Usage (X)
    0x09, 0x31,  //     Usage (Y)
    0x15, 0x81,  //     Logical Minimum (-127)
    0x25, 0x7F,  //     Logical Maximum (127)
    0x75, 0x08,  //     Report Size (8)
    0x95, 0x02,  //     Report Count (2)
    0x81, 0x06,  //     Input (Data, Var, Rel)
    0x09, 0x38,  //     Usage (Wheel)
    0x15, 0x81,  //     Logical Minimum (-127)
    0x25, 0x7F,  //     Logical Maximum (127)
    0x75, 0x08,  //     Report Size (8)
    0x95, 0x01,  //     Report Count (1)
    0x81, 0x06,  //     Input (Data, Var, Rel)
    0xC0,        //   End Collection
    0xC0         // End Collection
};

struct mouse_report {
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
} __packed;

static struct mouse_report report;

/* Boot Mouse Input Report (boot protocol uses buttons,x,y only) */
struct boot_mouse_input_report {
    uint8_t buttons;
    int8_t x;
    int8_t y;
} __packed;
static struct boot_mouse_input_report boot_report;

static struct bt_conn *conn_handle;

/* Read handler for Report Map (existing) */
static ssize_t read_report_map(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, void *buf,
                               uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             hid_report_map, sizeof(hid_report_map));
}

/* ----------------- HID additions (kept) ----------------- */

/* HID Information */
static const uint8_t hid_info[] = { 0x11, 0x01, 0x00, 0x02 }; /* bcdHID 1.11, country 0, flags */
static ssize_t read_hid_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, hid_info, sizeof(hid_info));
}

/* Protocol Mode */
static uint8_t protocol_mode = 1;
static ssize_t read_protocol_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                  void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &protocol_mode, sizeof(protocol_mode));
}
static ssize_t write_protocol_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                   const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    protocol_mode = *((const uint8_t *)buf);
    LOG_DBG("Protocol Mode set to %u", protocol_mode);
    return len;
}

/* HID Control Point */
static ssize_t write_hid_control_point(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                       const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    uint8_t val = *((const uint8_t *)buf);
    LOG_DBG("HID Control Point write: %u", val);
    return len;
}

/* HID Service Declaration (with added HID characteristics) */
BT_GATT_SERVICE_DEFINE(hid_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0x1812)),

    /* HID Information */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2A4A),
        BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_hid_info, NULL, NULL),

    /* HID Control Point */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2A4C),
        BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE, NULL, write_hid_control_point, NULL),

    /* Protocol Mode */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2A4E),
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        read_protocol_mode, write_protocol_mode, &protocol_mode),

    /* Report Map */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2A4B),
        BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_report_map, NULL, NULL),

    /* Boot Mouse Input Report */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2A33),
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ, NULL, NULL, &boot_report),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Normal Report */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2A4D),
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ, NULL, NULL, &report),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

/* ----------------- Advertising helper (ADV ADD) ----------------- */

/* advertising payloads (keep as constants) */
static const uint8_t ad_flags[] = { BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR };
static const uint8_t hid_uuid[] = { 0x12, 0x18 }; /* HID Service UUID 0x1812, little-endian */
static const struct bt_data ad[] = {
    BT_DATA(BT_DATA_FLAGS, ad_flags, sizeof(ad_flags)),
    BT_DATA(BT_DATA_UUID16_ALL, hid_uuid, sizeof(hid_uuid)),
};
/* scan/adv data (name) */
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* Start advertising with safe stop/retry logic
 * [ADV ADD] stops any existing advertising first, sleeps a little, then starts.
 * If start returns -ENOMEM (-12), try stopping and retry once.
 */
static int start_advertising(void)
{
    int err;

    /* Ensure advertising stopped first to avoid duplicate instances */
    (void)bt_le_adv_stop();

    /* small gap to let controller settle */
    k_sleep(K_MSEC(50));

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("bt_le_adv_start failed (err %d). Attempting stop+retry", err);
        /* Try again after explicitly stopping and small delay */
        (void)bt_le_adv_stop();
        k_sleep(K_MSEC(100));
        err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
        if (err) {
            LOG_ERR("bt_le_adv_start retry failed (err %d)", err);
            return err;
        }
    }

    LOG_INF("Advertising successfully started");
    return 0;
}

/* ----------------- Connection handling ----------------- */

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    conn_handle = bt_conn_ref(conn);
    LOG_INF("Connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason 0x%02x)", reason);

    if (conn_handle) {
        bt_conn_unref(conn_handle);
        conn_handle = NULL;
    }

    /* [ADV ADD] Resume advertising cleanly after disconnect */
    start_advertising();
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

static void bt_ready(int err)
{
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return;
    }

    LOG_INF("Bluetooth initialized");

    /* [ADV ADD] register connection callbacks (safe to call even if already registered) */
    bt_conn_cb_register(&conn_callbacks);

    /* [ADV ADD] start advertising using helper (handles stop/retry) */
    start_advertising();
}

int ble_hid_init(void)
{
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("bt_enable failed (err %d)", err);
        return -1;
    }

#if defined(CONFIG_BT_SETTINGS)
    settings_load();
#endif

    bt_ready(0);

    return 0;
}

/* Send mouse report (normal) */
int ble_hid_mouse_report(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel, uint8_t battery)
{
    if (!conn_handle) {
        return -ENOTCONN;
    }

    report.buttons = buttons;
    report.x = dx;
    report.y = dy;
    report.wheel = wheel;

    /* Note: attr index 8 corresponds to the Report characteristic in this layout.
     * If you change the GATT layout, update this index or use attr lookup.
     */
    return bt_gatt_notify(conn_handle, &hid_svc.attrs[8], &report, sizeof(report));
}
