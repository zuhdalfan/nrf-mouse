#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include "switch.h"

#define LEFT_SWITCH_NODE  DT_ALIAS(sw0)
#define RIGHT_SWITCH_NODE DT_ALIAS(sw1)

#define DEBOUNCE_DELAY_MS 50

static const struct gpio_dt_spec left_switch  = GPIO_DT_SPEC_GET_OR(LEFT_SWITCH_NODE, gpios, {0});
static const struct gpio_dt_spec right_switch = GPIO_DT_SPEC_GET_OR(RIGHT_SWITCH_NODE, gpios, {0});

static struct gpio_callback left_cb_data;
static struct gpio_callback right_cb_data;

static struct k_work_delayable left_work;
static struct k_work_delayable right_work;

static bool left_pressed = false;
static bool right_pressed = false;

static void debounce_left(struct k_work *work)
{
    // Active-low: pressed when pin reads 0
    left_pressed = (gpio_pin_get_dt(&left_switch) == 1);
}

static void debounce_right(struct k_work *work)
{
    right_pressed = (gpio_pin_get_dt(&right_switch) == 1);
}

static void switch_pressed_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (pins & BIT(left_switch.pin)) {
        k_work_schedule(&left_work, K_MSEC(DEBOUNCE_DELAY_MS));
    }

    if (pins & BIT(right_switch.pin)) {
        k_work_schedule(&right_work, K_MSEC(DEBOUNCE_DELAY_MS));
    }
}

bool switch_get_state_left(void)
{
    return left_pressed;
}

bool switch_get_state_right(void)
{
    return right_pressed;
}

void switch_init()
{

    if (!device_is_ready(left_switch.port)) {
        printk("Error: left switch device not ready\n");
        return;
    }

    if (!device_is_ready(right_switch.port)) {
        printk("Error: right switch device not ready\n");
        return;
    }

    gpio_pin_configure_dt(&left_switch, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure_dt(&right_switch, GPIO_INPUT | GPIO_PULL_UP);

    gpio_pin_interrupt_configure_dt(&left_switch, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure_dt(&right_switch, GPIO_INT_EDGE_BOTH);

    gpio_init_callback(&left_cb_data, switch_pressed_isr, BIT(left_switch.pin));
    gpio_init_callback(&right_cb_data, switch_pressed_isr, BIT(right_switch.pin));

    gpio_add_callback(left_switch.port, &left_cb_data);
    gpio_add_callback(right_switch.port, &right_cb_data);

    k_work_init_delayable(&left_work, debounce_left);
    k_work_init_delayable(&right_work, debounce_right);
}
