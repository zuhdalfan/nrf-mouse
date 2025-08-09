#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include <math.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

int usb_console(void)
{
	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;

	if (usb_enable(NULL)) {
		return 0;
	}

	/* Poll if the DTR flag was set */
	while (!dtr) {
		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		/* Give CPU resources to low priority threads. */
		k_sleep(K_MSEC(100));
	}

    return 0;
}

int main(void) {

	k_msleep(1000);
	
	

    while (1) {
        // threads created on business_logic.c
        // polling thread - to get all data and send to host
        // cursor thread - to get dx dy data from paw3395 sensor
        // dpi control thread - to check dpi and implement led
        k_msleep(1000); 
		// usb_hid_mouse_test();
    }

    return 0;
}