
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/logging/log.h>
#include <math.h>

#include "usb_hid.h"

LOG_MODULE_REGISTER(usb_hid_c, LOG_LEVEL_INF);

const struct device *hid_dev;
static const uint8_t hid_report_desc[] = HID_MOUSE_REPORT_DESC(5);
static K_SEM_DEFINE(ep_write_sem, 0, 1);

static uint8_t last_report[USB_MOUSE_REPORT_SIZE] = {0};

static enum usb_dc_status_code usb_status;

void usb_hid_mouse_build_report(uint8_t *report,
                                 bool btn_left, bool btn_right, bool btn_middle,
                                 bool btn_forward, bool btn_back,
                                 int8_t dx, int8_t dy, int8_t wheel)
{
    report[0] = (btn_left   ? 1 << 0 : 0) |
                (btn_right  ? 1 << 1 : 0) |
                (btn_middle ? 1 << 2 : 0) |
                (btn_forward? 1 << 3 : 0) |
                (btn_back   ? 1 << 4 : 0);
    report[1] = dx;
    report[2] = dy;
    report[3] = wheel;
}



static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	usb_status = status;
}

static void int_in_ready_cb(const struct device *dev)
{
	ARG_UNUSED(dev);
	k_sem_give(&ep_write_sem);
}

static const struct hid_ops ops = {
	.int_in_ready = int_in_ready_cb,
};

int usb_hid_mouse_init(void)
{
	
	int ret;

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
	hid_dev = DEVICE_DT_GET_ONE(zephyr_hid_device);
#else
	hid_dev = device_get_binding("HID_0");
#endif

	if (!hid_dev) {
		LOG_ERR("Failed to get HID device");
		return 0;
	}

	usb_hid_register_device(hid_dev,
		hid_report_desc, sizeof(hid_report_desc), &ops);
	usb_hid_init(hid_dev);

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
	extern int enable_usb_device_next(void);
	ret = enable_usb_device_next();
#else
	ret = usb_enable(status_cb);
#endif

	if (ret != 0) {
		LOG_ERR("USB enable failed: %d", ret);
		return 0;
	}

	k_sleep(K_SECONDS(2)); // Allow USB enumeration

	return 0;
}

static void build_report(uint8_t *report,
                         bool btn_left, bool btn_right, bool btn_middle,
                         bool btn_forward, bool btn_back,
                         int8_t dx, int8_t dy, int8_t wheel)
{
    report[0] = (btn_left   ? 1 << 0 : 0) |
                (btn_right  ? 1 << 1 : 0) |
                (btn_middle ? 1 << 2 : 0) |
                (btn_forward? 1 << 3 : 0) |
                (btn_back   ? 1 << 4 : 0);
    report[1] = dx;
    report[2] = dy;
    report[3] = wheel;
}

void usb_hid_mouse_update(bool left, bool right, bool middle, bool forward, bool back,
                          int8_t dx, int8_t dy, int8_t wheel)
{
    if (!hid_dev) {
        LOG_ERR("HID device not initialized");
        return;
    }

    uint8_t report[USB_MOUSE_REPORT_SIZE];
    build_report(report, left, right, middle, forward, back, dx, dy, wheel);

	// Only send if report changed
    if (memcmp(report, last_report, USB_MOUSE_REPORT_SIZE) == 0) {
        return; // No change, skip
    }

	memcpy(last_report, report, USB_MOUSE_REPORT_SIZE); // Update stored report

	// print report in 1 line
	// Decode button states into a readable string
	char btns[6]; // 5 buttons + null terminator
	snprintf(btns, sizeof(btns), "%c%c%c%c%c",
			left    ? 'L' : '-',
			right   ? 'R' : '-',
			middle  ? 'M' : '-',
			forward ? 'F' : '-',
			back    ? 'B' : '-');

	LOG_INF("Mouse report: btns=%s dx=%d dy=%d wheel=%d", btns, dx, dy, wheel);


    int ret = hid_int_ep_write(hid_dev, report, USB_MOUSE_REPORT_SIZE, NULL);
    if (ret == 0) {
        k_sem_take(&ep_write_sem, K_FOREVER);
    } else {
        LOG_ERR("Failed to write HID report: %d", ret);
    }
}

void usb_hid_mouse_test(void)
{
    static bool toggle = false;

    // Alternate movement and wheel
    int8_t dx = toggle ? 5 : -5;
    int8_t dy = toggle ? 3 : -3;
    int8_t wheel = toggle ? 1 : -1;

    // Call the update function with no buttons pressed
    usb_hid_mouse_update(false, false, false, false, false, dx, dy, wheel);

    toggle = !toggle;

    // Optional delay (for manual loop or thread)
    k_sleep(K_MSEC(500));
}