#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

int encoder_init(void);
int encoder_get_scroll_delta(void);
bool encoder_get_button_state(void);
