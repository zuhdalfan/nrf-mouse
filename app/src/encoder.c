#include "encoder.h"
#include <zephyr/kernel.h>

#define SCROLL_A_NODE DT_ALIAS(scrolla)
#define SCROLL_B_NODE DT_ALIAS(scrollb)
#define SCROLL_BTN_NODE DT_ALIAS(scrollbtn)

static const struct gpio_dt_spec scroll_a = GPIO_DT_SPEC_GET_OR(SCROLL_A_NODE, gpios, {0});
static const struct gpio_dt_spec scroll_b = GPIO_DT_SPEC_GET_OR(SCROLL_B_NODE, gpios, {0});
static const struct gpio_dt_spec scroll_btn = GPIO_DT_SPEC_GET_OR(SCROLL_BTN_NODE, gpios, {0});


static struct gpio_callback scroll_a_cb;
static struct gpio_callback scroll_b_cb;
static struct gpio_callback btn_cb;

static volatile int scroll_delta = 0;
static volatile bool button_pressed = false;
static int last_a = 0;
static int last_b = 0;

#include <zephyr/kernel.h>

#define DEBOUNCE_MS 20
static struct k_work_delayable debounce_work;

static void update_scroll_direction(void) {
    int a = gpio_pin_get_dt(&scroll_a);
    int b = gpio_pin_get_dt(&scroll_b);

    // Gray code decoding
    if ((last_a == 0 && last_b == 0 && a == 1 && b == 0) ||
        (last_a == 1 && last_b == 1 && a == 0 && b == 1) ||
        (last_a == 1 && last_b == 0 && a == 1 && b == 1) ||
        (last_a == 0 && last_b == 1 && a == 0 && b == 0)) {
        scroll_delta++;
    } else if (
        (last_a == 0 && last_b == 0 && a == 0 && b == 1) ||
        (last_a == 1 && last_b == 1 && a == 1 && b == 0) ||
        (last_a == 1 && last_b == 0 && a == 0 && b == 0) ||
        (last_a == 0 && last_b == 1 && a == 1 && b == 1)) {
        scroll_delta--;
    }

    last_a = a;
    last_b = b;
}

static void scroll_a_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    update_scroll_direction();
}

static void scroll_b_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    update_scroll_direction();
}

static void debounce_btn(struct k_work *work)
{
    button_pressed = (gpio_pin_get_dt(&scroll_btn) == 0);  // active low
}

static void btn_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (pins & BIT(scroll_btn.pin)) {
        k_work_schedule(&debounce_work, K_MSEC(DEBOUNCE_MS));
    }
}

int encoder_get_scroll_delta(void) {
    int delta = scroll_delta;
    scroll_delta = 0;
    return delta;
}

bool encoder_get_button_state(void) {
    return button_pressed;
}

int encoder_init(void) {
    int ret;

    if (!device_is_ready(scroll_a.port) || !device_is_ready(scroll_b.port)) {
        printk("Scroll encoder GPIO devices not ready\n");
        return -ENODEV;
    }

    if(!device_is_ready(scroll_btn.port)) {
        printk("Scroll switch GPIO devices not ready\n");
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&scroll_a, GPIO_INPUT);
    if (ret) {
        printk("Failed to configure scroll A GPIO: %d\n", ret);
        return ret;
    }
    ret = gpio_pin_configure_dt(&scroll_b, GPIO_INPUT);
    if (ret) {
        printk("Failed to configure scroll B GPIO: %d\n", ret);   
        return ret;
    }
    ret = gpio_pin_configure_dt(&scroll_btn, GPIO_INPUT | GPIO_PULL_UP);
    if (ret) { 
        printk("Failed to configure scroll button GPIO: %d\n", ret);
        return ret;
    }
    // Save initial state
    last_a = gpio_pin_get_dt(&scroll_a);
    last_b = gpio_pin_get_dt(&scroll_b);

    // Interrupts for scroll A and B
    ret = gpio_pin_interrupt_configure_dt(&scroll_a, GPIO_INT_EDGE_BOTH);
    if (ret) {
        printk("Failed to configure scroll A interrupt: %d\n", ret);   
        return ret;
    }
    gpio_init_callback(&scroll_a_cb, scroll_a_isr, BIT(scroll_a.pin));
    gpio_add_callback(scroll_a.port, &scroll_a_cb);

    ret = gpio_pin_interrupt_configure_dt(&scroll_b, GPIO_INT_EDGE_BOTH);
    if (ret) {
        printk("Failed to configure scroll B interrupt: %d\n", ret);   
        return ret;
    }
    gpio_init_callback(&scroll_b_cb, scroll_b_isr, BIT(scroll_b.pin));
    gpio_add_callback(scroll_b.port, &scroll_b_cb);

    // Interrupt for scroll button (center click)
    k_work_init_delayable(&debounce_work, debounce_btn);

    ret = gpio_pin_interrupt_configure_dt(&scroll_btn, GPIO_INT_EDGE_BOTH);
    if (ret) {
        printk("Failed to configure scroll button interrupt: %d\n", ret);
        return ret;
    }
    gpio_init_callback(&btn_cb, btn_isr, BIT(scroll_btn.pin));
    gpio_add_callback(scroll_btn.port, &btn_cb);

    return 0;
}
