
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/byteorder.h>
// #include <zephyr/>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "ble_hids.h"

#define REPORT_MOUSE_SIZE sizeof(ble_hids_report_mouse_t)
#define BOOT_REPORT_MOUSE_SIZE sizeof(ble_hids_report_mouse_boot_t)

enum
{
    HIDS_REPORT_ID_MOUSE = 0x01,
};
typedef uint8_t hids_report_id_t;

/* HIDS Info flags */
enum
{
    HIDS_REMOTE_WAKE = BIT(0),
    HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

/* Control Point possible values */
enum
{
    HIDS_CONTROL_POINT_SUSPEND = 0x00,
    HIDS_CONTROL_POINT_EXIT_SUSPEND = 0x01,

    HIDS_CONTROL_POINT_N
};
typedef uint8_t hids_ctrl_point_t;

/* HID Information Characteristic value */
typedef struct
{
    uint16_t version; /* version number of base USB HID Specification */
    uint8_t code;     /* country HID Device hardware is localized for. */
    uint8_t flags;

} __packed hids_info_t;

/* Type of reports */
enum
{
    HIDS_REPORT_INPUT = 0x01,
    HIDS_REPORT_OUTPUT = 0x02,
    HIDS_REPORT_FEATURE = 0x03,
};
typedef uint8_t hids_report_type_t;

/* HID Report Characteristic Descriptor */
typedef struct
{
    uint8_t id;              /* report id */
    hids_report_type_t type; /* report type */

} __packed hids_report_desc_t;

typedef struct
{
    uint8_t *const report_ref;
    uint8_t report_len;

} hids_report_info_t;

static const hids_info_t hids_info =
    {
        .version = 0x0101,
        .code = 0x00,
        .flags = HIDS_NORMALLY_CONNECTABLE,
};

/* Reports descriptors (read-only) */
static const hids_report_desc_t mse_input_desc =
    {
        .id = HIDS_REPORT_ID_MOUSE,
        .type = HIDS_REPORT_INPUT,
};

/* Module working data */
static struct bt_conn *active_conn;

/* Characteristics and descriptors cached values */
static bool mse_input_rep_notif_enabled;
static bool mse_boot_input_rep_notif_enabled;

static hids_ctrl_point_t ctrl_point;
static ble_hids_prot_mode_t prot_mode;
static uint8_t mse_input_report[REPORT_MOUSE_SIZE];
static uint8_t mse_boot_input_report[BOOT_REPORT_MOUSE_SIZE];

/* Report map */
static const uint8_t report_map[] = {
    /* MOUSE INPUT REPORT MAP */
    0x05, 0x01,                 // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,                 // Usage (Mouse)
    0xA1, 0x01,                 // Collection (Application)
    0x85, HIDS_REPORT_ID_MOUSE, //   Report ID (1)
    0x09, 0x01,                 //   Usage (Pointer)
    0xA1, 0x00,                 //   Collection (Physical)

    // Buttons
    0x95, 0x05, //     Report Count (5 buttons)
    0x75, 0x01, //     Report Size (1)
    0x15, 0x00, //     Logical Minimum (0)
    0x25, 0x01, //     Logical Maximum (1)
    0x05, 0x09, //     Usage Page (Button)
    0x19, 0x01, //     Usage Minimum (Button 1)
    0x29, 0x05, //     Usage Maximum (Button 5)
    0x81, 0x02, //     Input (Data,Var,Abs)

    // Padding
    0x95, 0x01, //     Report Count (1)
    0x75, 0x03, //     Report Size (3 bits padding)
    0x81, 0x03, //     Input (Cnst,Var,Abs)

    // X, Y
    0x05, 0x01,       //     Usage Page (Generic Desktop Ctrls)
    0x16, 0x00, 0x80, //     Logical Minimum (-32768)
    0x26, 0xFF, 0x7F, //     Logical Maximum (32767)
    0x75, 0x10,       //     Report Size (16)
    0x95, 0x02,       //     Report Count (2)
    0x09, 0x30,       //     Usage (X)
    0x09, 0x31,       //     Usage (Y)
    0x81, 0x06,       //     Input (Data,Var,Rel)

    // Wheel
    0x15, 0x81, //     Logical Minimum (-127)
    0x25, 0x7F, //     Logical Maximum (127)
    0x75, 0x08, //     Report Size (8)
    0x95, 0x01, //     Report Count (1)
    0x09, 0x38, //     Usage (Wheel)
    0x81, 0x06, //     Input (Data,Var,Rel)

    0xC0, //   End Collection
    0xC0, // End Collection
};

static const hids_report_info_t mse_input_rep_info =
    {
        .report_ref = mse_input_report,
        .report_len = sizeof(mse_input_report)};

static const hids_report_info_t mse_boot_input_rep_info =
    {
        .report_ref = mse_boot_input_report,
        .report_len = sizeof(mse_boot_input_report)};

/*
** --------------------------------------------------------
** Private Functions
** --------------------------------------------------------
*/

static ssize_t s_read_info(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr, void *buf,
                           uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &hids_info, sizeof(hids_info_t));
}

static ssize_t s_read_report_map(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr, void *buf,
                                 uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map, sizeof(report_map));
}

static ssize_t s_read_report(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr, void *buf,
                             uint16_t len, uint16_t offset)
{
    const hids_report_info_t *rep_info_ref = (const hids_report_info_t *)attr->user_data;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, rep_info_ref->report_ref, rep_info_ref->report_len);
}

static ssize_t s_read_prot_mode(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr, void *buf,
                                uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &prot_mode, sizeof(prot_mode));
}

static ssize_t s_read_report_desc(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr, void *buf,
                                  uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(hids_report_desc_t));
}

static void s_mse_boot_input_rep_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    mse_boot_input_rep_notif_enabled = (value == BT_GATT_CCC_NOTIFY) ? true : false;
}

static void s_mse_input_rep_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    mse_input_rep_notif_enabled = (value == BT_GATT_CCC_NOTIFY) ? true : false;
}

static ssize_t s_write_ctrl_point(struct bt_conn *conn,
                                  const struct bt_gatt_attr *attr,
                                  const void *buf, uint16_t len, uint16_t offset,
                                  uint8_t flags)
{
    uint8_t *const ctrl_point_ref = (uint8_t *const)&ctrl_point;
    const uint8_t new_ctrl_pnt = *((const uint8_t *)buf);

    /* Validate flags */
    if (!(flags & BT_GATT_WRITE_FLAG_CMD))
    {
        /* Only write without response accepted */
        return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
    }

    /* Validate length */
    if ((offset + len) > sizeof(ctrl_point))
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    /* Validate value */
    if (new_ctrl_pnt >= HIDS_CONTROL_POINT_N)
    {
        return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
    }

    memcpy(ctrl_point_ref + offset, buf, len);

    return len;
}

static ssize_t s_write_prot_mode(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset,
                                 uint8_t flags)
{
    uint8_t *const prot_mode_ref = (uint8_t *const)&prot_mode;
    const ble_hids_prot_mode_t newPm = *((const ble_hids_prot_mode_t *)buf);

    /* Validate flags */
    if (!(flags & BT_GATT_WRITE_FLAG_CMD))
    {
        /* Only write without response accepted */
        return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
    }

    /* Validate length */
    if ((offset + len) > sizeof(prot_mode))
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    /* Validate value received */
    if (newPm >= BLE_HIDS_PM_N)
    {
        return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
    }

    memcpy(prot_mode_ref + offset, buf, len);

    return len;
}

/* HID Service Definition */
BT_GATT_SERVICE_DEFINE(ble_svc,

                       BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),

                       /* Information Characteristic */
                       BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO,
                                              BT_GATT_CHRC_READ,
                                              BT_GATT_PERM_READ_ENCRYPT,
                                              s_read_info, NULL, NULL),

                       /* Control Point Characteristic */
                       BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
                                              BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_WRITE_ENCRYPT,
                                              NULL, s_write_ctrl_point, NULL),

                       /* Report Map Characteristic */
                       BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP,
                                              BT_GATT_CHRC_READ,
                                              BT_GATT_PERM_READ_ENCRYPT,
                                              s_read_report_map, NULL, NULL),

                       /* Protocol Mode Characteristic */
                       BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_PROTOCOL_MODE,
                                              BT_GATT_CHRC_READ |
                                                  BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_READ_ENCRYPT |
                                                  BT_GATT_PERM_WRITE_ENCRYPT,
                                              s_read_prot_mode, s_write_prot_mode, NULL),

                       /* Boot Mouse Input Report Characteristic */
                       BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_BOOT_MOUSE_IN_REPORT,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ_ENCRYPT,
                                              s_read_report, NULL,
                                              (hids_report_info_t *)&mse_boot_input_rep_info),
                       BT_GATT_CCC(s_mse_boot_input_rep_ccc_changed,
                                   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT),

                       /* Mouse Input Report Characteristic (+ descriptor) */
                       BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ_ENCRYPT,
                                              s_read_report, NULL,
                                              (hids_report_info_t *)&mse_input_rep_info),
                       BT_GATT_CCC(s_mse_input_rep_ccc_changed,
                                   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT),
                       BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF,
                                          BT_GATT_PERM_READ,
                                          s_read_report_desc, NULL,
                                          (hids_report_desc_t *)&mse_input_desc), );

void ble_hids_connected(struct bt_conn *conn)
{
    active_conn = conn;

    /* Set Protocol Mode back to default (Report), as dictated by the spec */
    prot_mode = BLE_HIDS_PM_REPORT;
}

void ble_hids_disconnected(void)
{
    active_conn = NULL;
}

ble_hids_prot_mode_t ble_hids_get_prot_mode(void)
{
    return prot_mode;
}

bool ble_hids_is_mouse_report_writable(void)
{
    /* Return false if connection is still not encrypted */
    if ((active_conn == NULL) || (bt_conn_get_security(active_conn) < BT_SECURITY_L2))
    {
        printk("Connection is not encrypted, mouse report is not writable\n");
        return false;
    }

    if (prot_mode == BLE_HIDS_PM_BOOT)
    {
        printk("Boot Protocol Mode PM BOOT is active, mouse report is writable\n");
        return mse_boot_input_rep_notif_enabled;
    }
    else
    {
        printk("Report Protocol Mode is active, mouse report is writable, ccc is %d\n", mse_input_rep_notif_enabled);
        return mse_input_rep_notif_enabled;
    }
}

void ble_hids_mouse_notify_input(const void *data, uint8_t dataLen)
{
    __ASSERT_NO_MSG(dataLen == sizeof(mse_input_report));
    __ASSERT_NO_MSG(mse_input_rep_notif_enabled);
    __ASSERT_NO_MSG(bt_conn_get_security(active_conn) >= BT_SECURITY_L2);

    int err;
    struct bt_gatt_notify_params params = {0};

    memcpy(mse_input_report, data, dataLen);

    params.uuid = BT_UUID_HIDS_REPORT;
    params.data = data;
    params.len = dataLen;
    params.func = NULL;

    printk("Sending mouse notification: buttons %02x, move_x %d, move_y %d, scroll_v %d\n",
           ((ble_hids_report_mouse_t *)data)->buttons_bitmask,
           ((ble_hids_report_mouse_t *)data)->move_x_lsb |
               (((ble_hids_report_mouse_t *)data)->move_x_msb << 8),
           ((ble_hids_report_mouse_t *)data)->move_y_lsb |
               (((ble_hids_report_mouse_t *)data)->move_y_msb << 8),
           ((ble_hids_report_mouse_t *)data)->scroll_v);
    err = bt_gatt_notify_cb(active_conn, &params);
}

void ble_hids_mouse_notify_boot(const void *data, uint8_t dataLen)
{
    __ASSERT_NO_MSG(dataLen == sizeof(mse_boot_input_report));
    __ASSERT_NO_MSG(mse_boot_input_rep_notif_enabled);
    __ASSERT_NO_MSG(bt_conn_get_security(active_conn) >= BT_SECURITY_L2);

    int err;
    struct bt_gatt_notify_params params = {0};

    memcpy(mse_boot_input_report, data, dataLen);

    params.uuid = BT_UUID_HIDS_BOOT_MOUSE_IN_REPORT;
    params.data = data;
    params.len = dataLen;
    params.func = NULL;

    printk("Sending boot mouse notification: buttons %02x, move_x %d, move_y %d, scroll_v %d\n",
           ((ble_hids_report_mouse_boot_t *)data)->buttons_bitmask,
           ((ble_hids_report_mouse_boot_t *)data)->move_x,
           ((ble_hids_report_mouse_boot_t *)data)->move_y,
           ((ble_hids_report_mouse_boot_t *)data)->scroll_v);
    err = bt_gatt_notify_cb(active_conn, &params);
}

static inline uint8_t mouse_buttons_mask(bool left, bool right, bool middle, bool back, bool forward)
{
    uint8_t mask = 0;

    if (left)
    {
        mask |= 1 << 0;
    } // Button 1
    if (right)
    {
        mask |= 1 << 1;
    } // Button 2
    if (middle)
    {
        mask |= 1 << 2;
    } // Button 3
    if (back)
    {
        mask |= 1 << 3;
    } // Button 4
    if (forward)
    {
        mask |= 1 << 4;
    } // Button 5

    return mask;
}

void ble_hids_send_mouse_notification(bool left, bool right, bool mid, bool forward, bool backward, int16_t move_x, int16_t move_y, int8_t scroll_v)
{
    ble_hids_prot_mode_t currProtMode = ble_hids_get_prot_mode();
    printk("currProtMode = %d\n", currProtMode);

    uint8_t buttons_bitmask = mouse_buttons_mask(left, right, mid, backward, forward);

    if (ble_hids_is_mouse_report_writable())
    {
        if (BLE_HIDS_PM_REPORT == currProtMode)
        {
            printk("Sending mouse report: buttons %02x, move_x %d, move_y %d, scroll_v %d\n",
                   buttons_bitmask, move_x, move_y, scroll_v);
            ble_hids_report_mouse_t mse_report =
                {
                    .buttons_bitmask = buttons_bitmask,
                    .move_x_lsb = (uint8_t)move_x,
                    .move_x_msb = (uint8_t)(move_x >> 8) & 0xFF,
                    .move_y_lsb = (uint8_t)move_y,
                    .move_y_msb = (uint8_t)(move_y >> 8) & 0xFF,
                    .scroll_v = scroll_v};

            ble_hids_mouse_notify_input(&mse_report, sizeof(ble_hids_report_mouse_t));
        }
        else if (BLE_HIDS_PM_BOOT == currProtMode)
        {
            printk("Sending boot mouse report: buttons %02x, move_x %d, move_y %d, scroll_v %d\n",
                   buttons_bitmask, move_x, move_y, scroll_v);
            ble_hids_report_mouse_boot_t mse_boot_report =
                {
                    .buttons_bitmask = buttons_bitmask,
                    .move_x = move_x, /* implicit cast to 8 bits */
                    .move_y = move_y, /* implicit cast to 8 bits */
                    .scroll_v = scroll_v};

            ble_hids_mouse_notify_boot(&mse_boot_report, sizeof(ble_hids_report_mouse_boot_t));
        }
    }
}

#define STEP_SIZE 10  // pixels per step
#define STEP_COUNT 20 // number of steps per side
#define DELAY_MS 20   // delay between moves

void ble_hids_test_mouse_square(void)
{
    int i;

    /* Move right */
    for (i = 0; i < STEP_COUNT; i++)
    {
        ble_hids_send_mouse_notification(0, 0, 0, 0, 0, STEP_SIZE, 0, 0);
        k_msleep(DELAY_MS);
    }

    /* Move down */
    for (i = 0; i < STEP_COUNT; i++)
    {
        ble_hids_send_mouse_notification(0, 0, 0, 0, 0, 0, STEP_SIZE, 0);
        k_msleep(DELAY_MS);
    }

    /* Move left */
    for (i = 0; i < STEP_COUNT; i++)
    {
        ble_hids_send_mouse_notification(0, 0, 0, 0, 0, -STEP_SIZE, 0, 0);
        k_msleep(DELAY_MS);
    }

    /* Move up */
    for (i = 0; i < STEP_COUNT; i++)
    {
        ble_hids_send_mouse_notification(0, 0, 0, 0, 0, 0, -STEP_SIZE, 0);
        k_msleep(DELAY_MS);
    }
}