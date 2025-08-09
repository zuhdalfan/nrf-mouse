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
#include "ble_hid.h"

LOG_MODULE_REGISTER(business_logic, LOG_LEVEL_DBG);

/*
    * polling rate options: 1ms, 0.125ms
    * output from polling rate are:
        * cursor position
        * encoder increment/decrement
        * encoder button state
        * switch state: right, left
    * 
    * Pseudo code:
        * initiation
        * while(1)
            * get output value
            * delay for polling rate
            * send output value to host   

*/

// polling rate options 1ms, 0.125ms
#define POLLING_RATE_1000US 1000
#define POLLING_RATE_125US 125
paw3395_cpi_enum_t cpi_val = PAW3395_CPI_1600;
const struct device *paw3395 = DEVICE_DT_GET_ONE(pixart_paw3395);

// paw3395 setup
int cursor_position_x;
int cursor_position_y;
int cnt = 0;

// battery
int battery_percent = 0;

int get_battery_percent()
{
    return battery_percent;
}

void sensor_cursor_init()
{
    if (!device_is_ready(paw3395)) {
        LOG_ERR("PAW3395 device not ready");
    }
}

typedef enum {
    CONN_NONE,
    CONN_USB,
    CONN_ESB,
    CONN_BLE
} connection_type_enum_t;

int get_connection_type()
{
    // This function should return the current connection type
    // For now, we will return a default value
    return CONN_BLE; // Example: returning USB connection type
}

void rotate_dpi()
{
    cpi_val++;
    if (cpi_val == PAW3395_CPI_COUNT || cpi_val > PAW3395_CPI_COUNT){
        cpi_val = PAW3395_CPI_800;
    }
    struct sensor_value val;
    val.val1 = cpi_val;
    sensor_attr_set(paw3395, SENSOR_CHAN_ALL, PAW3395_ATTR_CPI_ALL, &val);
}

void update_led(paw3395_cpi_enum_t cpi_val)
{
    switch(cpi_val)
    {
        case PAW3395_CPI_800:
            led_set_color(LED_COLOR_RED); break;
        case PAW3395_CPI_1600:
            led_set_color(LED_COLOR_GREEN); break;
        case PAW3395_CPI_2400:
            led_set_color(LED_COLOR_YELLOW); break;
        case PAW3395_CPI_3200:
            led_set_color(LED_COLOR_ORANGE); break;
        case PAW3395_CPI_5000:
            led_set_color(LED_COLOR_PURPLE); break;
        case PAW3395_CPI_10000:
            led_set_color(LED_COLOR_CYAN); break;
        case PAW3395_CPI_26000:
            led_set_color(LED_COLOR_BLUE); break;
        default:
            led_set_color(LED_COLOR_OFF); break;
    }
}

int get_cursor_position_x()
{
    return cursor_position_x;
}

int get_cursor_positon_y()
{
    return cursor_position_y;
}

int get_encoder_increment()
{
    return encoder_get_scroll_delta();
}

bool get_encoder_button_state()
{
    return encoder_get_button_state();
}

bool get_switch_state_right()
{
    return switch_get_state_right();
}

bool get_switch_state_left()
{
    return switch_get_state_left();
}

void send_output_to_host(
    int cursor_x, int cursor_y, 
    int encoder_increment, bool encoder_button_state, 
    bool switch_right_state, bool switch_left_state,
    int battery_percent)
{
    // This function should send the output values to the host
    // For now, we will just print the values to the console
    // cnt++;
    // printk("%d,cursor_x,%d,cursor_y,%d,encoder_increment,%d,encoder_button,%d,switch_right,%d,switch_left,%d,battery,%d\n",
    //     cnt, cursor_x, cursor_y, 
    //     encoder_increment, encoder_button_state,
    //     switch_right_state, switch_left_state,
    //     battery_percent
    // );

    connection_type_enum_t connection_type = get_connection_type();
    switch (connection_type) {
        case CONN_USB:
            // Send over USB
            usb_hid_mouse_update(switch_right_state, switch_left_state, encoder_button_state, false, false, cursor_x, cursor_y, encoder_increment);
            break;
        case CONN_ESB:
            // Send over ESB
            break;
        case CONN_BLE:
            // Send over BLE
            break;
        default:
            // No connection
            break;
    }
}

void polling_init()
{
    usb_hid_mouse_init();
    encoder_init();
    switch_init();
    led_init();
    ble_hid_init();
}

void polling_run(void)
{
    // Initiation code here if needed
    polling_init();
    while (1) {
        //TODO: Send output values to host
        send_output_to_host(
            get_cursor_position_x(),
            get_cursor_positon_y(), 
            get_encoder_increment(),
            get_encoder_button_state(), 
            get_switch_state_right(), 
            get_switch_state_left(),
            get_battery_percent()
        );

        // Delay for polling rate
        k_usleep(125);
    }
}


// ============== POLLING THREAD =================

#define POLLING_STACK_SIZE 1024
#define POLLING_THREAD_PRIORITY 5

void polling_thread(void *arg1, void *arg2, void *arg3)
{
    polling_run();
}

// Define the thread statically
K_THREAD_DEFINE(polling_thread_id, POLLING_STACK_SIZE,
                polling_thread, NULL, NULL, NULL,
                POLLING_THREAD_PRIORITY, 0, 0);

// ============== CURSOR THREAD =================

#define CURSOR_STACK_SIZE 1024
#define CURSOR_THREAD_PRIORITY 5

// create thread for cursor paw3395 sensor data fetching
void cursor_thread(void *arg1, void *arg2, void *arg3)
{
    sensor_cursor_init();
    while(1)
    {
        // Fetch sensor data
        if (sensor_sample_fetch(paw3395) == 0) {
            struct sensor_value dx, dy;
            sensor_channel_get(paw3395, SENSOR_CHAN_POS_DX, &dx);
            sensor_channel_get(paw3395, SENSOR_CHAN_POS_DY, &dy);

            // Update cursor position
            cursor_position_x = dx.val1;
            cursor_position_y = dy.val1;
        } else {
            LOG_ERR("Failed to fetch sensor data");
        }

        k_usleep(75); // put sleep less than lowest polling rate
    }
}

// Define the thread statically
K_THREAD_DEFINE(cursor_thread_id, CURSOR_STACK_SIZE,
                cursor_thread, NULL, NULL, NULL,
                CURSOR_THREAD_PRIORITY, 0, 0);


// ================= DPI CONTROL THREAD =================
// encoder button, paw3395 cpi value, led

#define DPI_CONTROL_STACK_SIZE 1024
#define DPI_CONTROL_THREAD_PRIORITY 5

void dpi_control_thread(void *arg1, void *arg2, void *arg3)
{
    sensor_cursor_init();
    while (1)
    {
        // if encoder button pressed, increse DPI level
        if(get_encoder_button_state())
        {
            // increase DPI
            rotate_dpi();
            // set LED
            // update_led(cpi_val);
        }

        k_msleep(1);
    }
    
}

// Define the thread statically
K_THREAD_DEFINE(dpi_control_thread_id, DPI_CONTROL_STACK_SIZE,
                dpi_control_thread, NULL, NULL, NULL,
                DPI_CONTROL_THREAD_PRIORITY, 0, 0);


#define BATTERY_STACK_SIZE 1024
#define BATTERY_THREAD_PRIORITY 5

void battery_thread(void *arg1, void *arg2, void *arg3)
{
    battery_init();
    while (1)
    {
        battery_get_percentage(&battery_percent);
        k_msleep(1000);
    }
    
}

// Define the thread statically
K_THREAD_DEFINE(battery_thread_id, BATTERY_STACK_SIZE,
                battery_thread, NULL, NULL, NULL,
                BATTERY_THREAD_PRIORITY, 0, 0);