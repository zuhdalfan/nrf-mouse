#ifndef LED_H
#define LED_H

#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_rgb_t;

typedef enum {
    LED_COLOR_OFF,
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_YELLOW,
    LED_COLOR_ORANGE,
    LED_COLOR_BLUE,
    LED_COLOR_CYAN,
    LED_COLOR_PURPLE,
    LED_COLOR_COUNT  // For iteration or validation
} led_color_t;

void led_init(void);
void led_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void led_set_color(led_color_t color);

#endif // LED_H
