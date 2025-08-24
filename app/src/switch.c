#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include "switch.h"

#define LEFT_SWITCH_NODE DT_ALIAS(sw0)
#define RIGHT_SWITCH_NODE DT_ALIAS(sw1)
#define FORWARD_SWITCH_NODE DT_ALIAS(sw2)
#define BACKWARD_SWITCH_NODE DT_ALIAS(sw3)
#define DPI_BTN_NODE DT_ALIAS(dpi_btn)

#define DEBOUNCE_DELAY_MS 50

static const struct gpio_dt_spec left_switch = GPIO_DT_SPEC_GET_OR(LEFT_SWITCH_NODE, gpios, {0});
static const struct gpio_dt_spec right_switch = GPIO_DT_SPEC_GET_OR(RIGHT_SWITCH_NODE, gpios, {0});
static const struct gpio_dt_spec forward_switch = GPIO_DT_SPEC_GET_OR(FORWARD_SWITCH_NODE, gpios, {0});
static const struct gpio_dt_spec backward_switch = GPIO_DT_SPEC_GET_OR(BACKWARD_SWITCH_NODE, gpios, {0});
static const struct gpio_dt_spec dpi_btn = GPIO_DT_SPEC_GET_OR(DPI_BTN_NODE, gpios, {0});

static struct gpio_callback left_cb_data;
static struct gpio_callback right_cb_data;
static struct gpio_callback forward_cb_data;
static struct gpio_callback backward_cb_data;
static struct gpio_callback dpi_cb_data;

static struct k_work_delayable left_work;
static struct k_work_delayable right_work;
static struct k_work_delayable forward_work;
static struct k_work_delayable backward_work;
static struct k_work_delayable dpi_work;

static bool left_pressed = false;
static bool right_pressed = false;
static bool forward_pressed = false;
static bool backward_pressed = false;
static bool dpi_pressed = false;

static void debounce_left(struct k_work *work)
{
    // Active-low: pressed when pin reads 0
    left_pressed = (gpio_pin_get_dt(&left_switch) == 1);
}

static void debounce_right(struct k_work *work)
{
    right_pressed = (gpio_pin_get_dt(&right_switch) == 1);
}

static void debounce_forward(struct k_work *work)
{
    forward_pressed = (gpio_pin_get_dt(&forward_switch) == 1);
}

static void debounce_backward(struct k_work *work)
{
    backward_pressed = (gpio_pin_get_dt(&backward_switch) == 1);
}

static void debounce_dpi(struct k_work *work)
{
    dpi_pressed = (gpio_pin_get_dt(&dpi_btn) == 1);
}

static void switch_pressed_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (pins & BIT(left_switch.pin))
    {
        k_work_schedule(&left_work, K_MSEC(DEBOUNCE_DELAY_MS));
    }

    if (pins & BIT(right_switch.pin))
    {
        k_work_schedule(&right_work, K_MSEC(DEBOUNCE_DELAY_MS));
    }
    if (pins & BIT(forward_switch.pin))
    {
        k_work_schedule(&forward_work, K_MSEC(DEBOUNCE_DELAY_MS));
    }
    if (pins & BIT(backward_switch.pin))
    {
        k_work_schedule(&backward_work, K_MSEC(DEBOUNCE_DELAY_MS));
    }
    if (pins & BIT(dpi_btn.pin))
    {
        k_work_schedule(&dpi_work, K_MSEC(DEBOUNCE_DELAY_MS));
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

bool switch_get_state_forward(void)
{
    return forward_pressed;
}

bool switch_get_state_backward(void)
{
    return backward_pressed;
}

bool switch_get_state_dpi(void)
{
    return dpi_pressed;
}

void switch_init()
{

    if (!device_is_ready(left_switch.port))
    {
        printk("Error: left switch device not ready\n");
        return;
    }

    if (!device_is_ready(right_switch.port))
    {
        printk("Error: right switch device not ready\n");
        return;
    }

    if (!device_is_ready(forward_switch.port))
    {
        printk("Error: forward switch device not ready\n");
        return;
    }

    if (!device_is_ready(backward_switch.port))
    {
        printk("Error: backward switch device not ready\n");
        return;
    }

    if (!device_is_ready(dpi_btn.port))
    {
        printk("Error: DPI button device not ready\n");
        return;
    }

    gpio_pin_configure_dt(&left_switch, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure_dt(&right_switch, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure_dt(&forward_switch, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure_dt(&backward_switch, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure_dt(&dpi_btn, GPIO_INPUT | GPIO_PULL_UP);

    gpio_pin_interrupt_configure_dt(&left_switch, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure_dt(&right_switch, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure_dt(&forward_switch, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure_dt(&backward_switch, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure_dt(&dpi_btn, GPIO_INT_EDGE_BOTH);

    gpio_init_callback(&left_cb_data, switch_pressed_isr, BIT(left_switch.pin));
    gpio_init_callback(&right_cb_data, switch_pressed_isr, BIT(right_switch.pin));
    gpio_init_callback(&forward_cb_data, switch_pressed_isr, BIT(forward_switch.pin));
    gpio_init_callback(&backward_cb_data, switch_pressed_isr, BIT(backward_switch.pin));
    gpio_init_callback(&dpi_cb_data, switch_pressed_isr, BIT(dpi_btn.pin));

    gpio_add_callback(left_switch.port, &left_cb_data);
    gpio_add_callback(right_switch.port, &right_cb_data);
    gpio_add_callback(forward_switch.port, &forward_cb_data);
    gpio_add_callback(backward_switch.port, &backward_cb_data);
    gpio_add_callback(dpi_btn.port, &dpi_cb_data);

    k_work_init_delayable(&left_work, debounce_left);
    k_work_init_delayable(&right_work, debounce_right);
    k_work_init_delayable(&forward_work, debounce_forward);
    k_work_init_delayable(&backward_work, debounce_backward);
    k_work_init_delayable(&dpi_work, debounce_dpi);
}
