

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <soc.h>
#include <assert.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include "ble_hids.h"

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define BT_ADV_INT_MIN 48 /* 0.625ms units --> 30ms */
#define BT_ADV_INT_MAX 80 /* 0.625ms units --> 50ms */

/* Advertising Data */
static const struct bt_data ad[] = {

    /* Appearance */
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
                  (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
                  (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),

    /* Flags */
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_LIMITED | BT_LE_AD_NO_BREDR)),

    /* List of UUID16 Services Implemented */
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_DIS_VAL)),
};

/* Scan Response Data */
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static struct k_work adv_work;

static bt_addr_le_t bonded_addr;
static bool bonded_addr_present;

static bool dir_adv_timeout;
static bool is_connected;
static int64_t last_advertising_start_ms;

bool ble_is_connected(void)
{
    return is_connected;
}

static void s_bonded_addr_cache(const struct bt_bond_info *info, void *user_data)
{
    memcpy(&bonded_addr, &info->addr, sizeof(bt_addr_le_t));
    bonded_addr_present = true;
}

static void s_advertising_start(void)
{
    /* NOTE: only one paired device is supported, so the function passed as argument is invoked once at most */
    bt_foreach_bond(BT_ID_DEFAULT, s_bonded_addr_cache, NULL);
    k_work_submit(&adv_work);
}

static void s_advertising_exec(void)
{
    int err = 0;
    struct bt_le_adv_param adv_param;

    /* Directed advertising if bonding is present and we did not already timed out */
    if ((bonded_addr_present) && (!dir_adv_timeout))
    {
        char addr_buf[BT_ADDR_LE_STR_LEN];

        adv_param = *BT_LE_ADV_CONN_DIR(&bonded_addr);
        adv_param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;

        err = bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);
        if (err)
        {
            printk("Directed advertising failed to start (err %d)\n", err);
            return;
        }

        bt_addr_le_to_str(&bonded_addr, addr_buf, BT_ADDR_LE_STR_LEN);
        printk("Direct advertising to %s started\n", addr_buf);
    }
    /* Regular advertising if no bonding is present */
    else
    {
        adv_param = *BT_LE_ADV_CONN;
        adv_param.interval_min = BT_ADV_INT_MIN;
        adv_param.interval_max = BT_ADV_INT_MAX;
        adv_param.options |= BT_LE_ADV_OPT_ONE_TIME | BT_LE_ADV_OPT_SCANNABLE;

        err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
        if (err)
        {
            printk("Advertising failed to start (err %d)\n", err);
            return;
        }

        printk("Regular advertising started\n");
    }

    if (err == 0)
    {
        /* Cache last start of advertising */
        last_advertising_start_ms = k_uptime_get();
    }
}

static void s_advertising_process(struct k_work *work)
{
    s_advertising_exec();
}

static void s_connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err)
    {
        if (err == BT_HCI_ERR_ADV_TIMEOUT)
        {
            printk("Direct advertising to %s timed out\n", addr);
            dir_adv_timeout = true;
            k_work_submit(&adv_work);
        }
        else
        {
            printk("Failed to connect to %s (%u)\n", addr, err);
        }

        return;
    }

    printk("Connected %s\n", addr);

    is_connected = true;
    dir_adv_timeout = false;
    last_advertising_start_ms = 0;

    /* Inform HID Service */
    ble_hids_connected(conn);
}

static void s_disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Disconnected from %s (reason %u)\n", addr, reason);

    /* Inform HID Service */
    ble_hids_disconnected();

    is_connected = false;

    /* Re-start advertising */
    s_advertising_start();
}

static void s_security_changed(struct bt_conn *conn, bt_security_t level,
                               enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err)
    {
        printk("Security changed: %s level %u\n", addr, level);
    }
    else
    {
        printk("Security failed: %s level %u err %d\n", addr, level, err);
    }
}

static void s_conn_params_updated(struct bt_conn *conn, uint16_t interval,
                                  uint16_t latency, uint16_t timeout)
{
    struct bt_conn_info info;
    bt_conn_get_info(conn, &info);

    printk("Conn params updated, interval: %u, latency: %u, timeout: %u\n",
           info.le.interval, info.le.latency, info.le.timeout);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = s_connected,
    .disconnected = s_disconnected,
    .security_changed = s_security_changed,
    .le_param_updated = s_conn_params_updated};

static void s_pairing_complete(struct bt_conn *conn, bool bonded)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void s_pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing failed conn: %s, reason %d\n", addr, reason);
}

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
    .pairing_complete = s_pairing_complete,
    .pairing_failed = s_pairing_failed};

void ble_init(void)
{
    int err;

    printk("Starting ble application\n");

    err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
    if (err)
    {
        printk("Failed to register authorization info callbacks.\n");
        return;
    }

    err = bt_enable(NULL);
    if (err)
    {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");

    if (IS_ENABLED(CONFIG_SETTINGS))
    {
        settings_load();
    }

    /* Init workqueue item to help during advertising */
    k_work_init(&adv_work, s_advertising_process);

    /* Immediately request advertising to start */
    s_advertising_start();
}
