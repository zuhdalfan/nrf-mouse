#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include "business_logic.h"
#include "encoder.h"
#include "switch.h"
#include "led.h"
#include "paw3395.h"
#include "battery.h"
#include "usb_hid.h"
#include "ble.h"
#include "ble_hids.h"

LOG_MODULE_REGISTER(business_logic, LOG_LEVEL_DBG);

// polling rate options 1ms, 0.125ms
#define POLLING_RATE_1000US 1000
#define POLLING_RATE_125US 125
paw3395_cpi_enum_t cpi_val = PAW3395_CPI_1600;
const struct device *paw3395 = DEVICE_DT_GET_ONE(pixart_paw3395);

typedef enum
{
    CONN_NONE,
    CONN_USB,
    CONN_ESB,
    CONN_BLE
} connection_type_enum_t;

static void sensor_cursor_init()
{
    if (!device_is_ready(paw3395))
    {
        LOG_ERR("PAW3395 device not ready");
    }
}

static int get_connection_type()
{
    bool ble_connected = ble_is_connected();
    bool usb_connected = usb_hid_mouse_is_connected();
    bool esb_connected = false; // Placeholder for ESB connection check

    // connection selection logic: USB > ESB > BLE
    if (usb_connected)
    {
        return CONN_USB;
    }
    else if (esb_connected)
    {
        return CONN_ESB;
    }
    else if (ble_connected)
    {
        return CONN_BLE;
    }
    else
    {
        return CONN_NONE; // No connection
    }
}

static void rotate_dpi()
{
    cpi_val++;
    if (cpi_val == PAW3395_CPI_COUNT || cpi_val > PAW3395_CPI_COUNT)
    {
        cpi_val = PAW3395_CPI_800;
    }
    struct sensor_value val;
    val.val1 = cpi_val;
    sensor_attr_set(paw3395, SENSOR_CHAN_ALL, PAW3395_ATTR_CPI_ALL, &val);
}

static void update_led(paw3395_cpi_enum_t cpi_val)
{
    switch (cpi_val)
    {
    case PAW3395_CPI_800:
        led_set_color(LED_COLOR_RED);
        break;
    case PAW3395_CPI_1600:
        led_set_color(LED_COLOR_GREEN);
        break;
    case PAW3395_CPI_2400:
        led_set_color(LED_COLOR_YELLOW);
        break;
    case PAW3395_CPI_3200:
        led_set_color(LED_COLOR_ORANGE);
        break;
    case PAW3395_CPI_5000:
        led_set_color(LED_COLOR_PURPLE);
        break;
    case PAW3395_CPI_10000:
        led_set_color(LED_COLOR_CYAN);
        break;
    case PAW3395_CPI_26000:
        led_set_color(LED_COLOR_BLUE);
        break;
    default:
        led_set_color(LED_COLOR_OFF);
        break;
    }
}

static int get_encoder_increment()
{
    return encoder_get_scroll_delta();
}

static bool get_encoder_button_state()
{
    return encoder_get_button_state();
}

static bool get_switch_state_right()
{
    return switch_get_state_right();
}

static bool get_switch_state_left()
{
    return switch_get_state_left();
}

static bool get_switch_state_forward()
{
    return switch_get_state_forward();
}

static bool get_switch_state_backward()
{
    return switch_get_state_backward();
}

static void send_output_to_host(
    int cursor_x, int cursor_y,
    int encoder_increment, bool encoder_button_state,
    bool switch_right_state, bool switch_left_state, bool switch_forward_state, bool switch_backward_state)
{
    connection_type_enum_t connection_type = get_connection_type();
    switch (connection_type)
    {
    case CONN_USB:
        // Send over USB
        usb_hid_mouse_update(switch_right_state, switch_left_state, encoder_button_state, switch_forward_state, switch_backward_state, cursor_x, cursor_y, encoder_increment);
        // usb_hid_mouse_test();
        break;
    case CONN_ESB:
        // Send over ESB
        break;
    case CONN_BLE:
        // Send over BLE
        ble_hids_send_mouse_notification(switch_right_state, switch_left_state, encoder_button_state, switch_forward_state, switch_backward_state, cursor_x, cursor_y, encoder_increment);
        // ble_hids_test_mouse_square();
        break;
    default:
        // No connection
        break;
    }
}

static void handle_encoder_button(bool encoder_button_state)
{
    static bool prev_encoder_button_state = false;
    // Detect rising edge: previously LOW, now HIGH
    if (!prev_encoder_button_state && encoder_button_state)
    {
        rotate_dpi();
        update_led(cpi_val);
    }

    // Remember last state for next call
    prev_encoder_button_state = encoder_button_state;
}

static void get_cursor_position(int *x, int *y)
{
    struct sensor_value dx, dy;
    if (sensor_sample_fetch(paw3395) == 0)
    {
        sensor_channel_get(paw3395, SENSOR_CHAN_POS_DX, &dx);
        sensor_channel_get(paw3395, SENSOR_CHAN_POS_DY, &dx);

        // Update cursor position
        *x = dx.val1;
        *y = dy.val1;
    }
    else
    {
        LOG_ERR("Failed to fetch sensor data");
    }
}

void polling_init()
{
    usb_hid_mouse_init();
    encoder_init();
    switch_init();
    led_init();
    ble_init();
    battery_init();
    sensor_cursor_init();
}

void polling_run(void)
{
    // GET CURSOR POSITION
    int cursor_position_x = 0;
    int cursor_position_y = 0;
    get_cursor_position(&cursor_position_x, &cursor_position_y);

    // GET SCROLL WHEEL
    int encoder_increment = get_encoder_increment();

    // GET SCROLL WHEEL BUTTON
    bool encoder_button_state = get_encoder_button_state();

    // GET MAIN SWITCH STATE
    bool switch_right_state = get_switch_state_right();
    bool switch_left_state = get_switch_state_left();
    bool switch_forward_state = get_switch_state_forward();
    bool switch_backward_state = get_switch_state_backward();

    send_output_to_host(
        cursor_position_x,
        cursor_position_y,
        encoder_increment,
        encoder_button_state,
        switch_right_state,
        switch_left_state,
        switch_forward_state,
        switch_backward_state);

    // GET BATTERY PERCENTAGE
    int battery_percent = 0;
    battery_get_percentage(&battery_percent);

    // HANDLE DPI & LED UPDATE
    handle_encoder_button(encoder_button_state);

    k_usleep(UPDATE_RATE);
}
