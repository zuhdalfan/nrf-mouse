#include "led.h"

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_DBG);

#define STRIP_NODE DT_ALIAS(ledstrip)

static const struct device *strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb pixel[1];

static const led_rgb_t led_colors[LED_COLOR_COUNT] = {
    [LED_COLOR_OFF]   = { .r = 0,   .g = 0,   .b = 0   },
    [LED_COLOR_RED]   = { .r = 255, .g = 0,   .b = 0   },
    [LED_COLOR_GREEN] = { .r = 0,   .g = 255, .b = 0   },
    [LED_COLOR_YELLOW]= { .r = 255, .g = 255, .b = 0   },
    [LED_COLOR_ORANGE]= { .r = 255, .g = 165, .b = 0   },
    [LED_COLOR_BLUE]  = { .r = 0,   .g = 0,   .b = 255 },
    [LED_COLOR_CYAN]  = { .r = 0,   .g = 255, .b = 255 },
    [LED_COLOR_PURPLE]= { .r = 128, .g = 0,   .b = 128 }
};

void led_init(void) {
    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip device not ready");
        return;
    }
    LOG_INF("LED strip initialized");
    led_set_color(LED_COLOR_OFF);
}

void led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    pixel[0].r = r;
    pixel[0].g = g;
    pixel[0].b = b;

    int ret = led_strip_update_rgb(strip, pixel, ARRAY_SIZE(pixel));
    if (ret) {
        LOG_ERR("Failed to update LED strip: %d", ret);
    }
}

void led_set_color(led_color_t color) {
    if (color >= LED_COLOR_COUNT) {
        LOG_WRN("Invalid LED color: %d", color);
        color = LED_COLOR_OFF;
    }

    led_rgb_t rgb = led_colors[color];
    led_set_rgb(rgb.r, rgb.g, rgb.b);
}
