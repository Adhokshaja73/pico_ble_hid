#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"
#include "pio_usb.h"
#include "tusb.h"

#include "pico/cyw43_arch.h"

#include "hid_report.h"
#include "bt_device.h"
#include "usb_device.h"
#include "usb_host.h"

static void usb_main(void);

// main loop
int main(void) {
	// for PIO USB, we want clock speed to be multiple of 12MHz
	set_sys_clock_khz(144000, true);

	// need so BTstack can save pairing information to flash
	flash_safe_execute_core_init();

	sleep_ms(10);

	// run BLE on core 1
	multicore_reset_core1();
	multicore_launch_core1(btstack_main);

	// run usb on core 0
	usb_main();

	return 0;
}

static void usb_main(void) {
	// init and run usb host
	usb_host_init();

	// init and run usb device
	usb_device_init();

	while (true) {
		switch ( host_state ) {
			case HOST_NEW_DESCRIPTOR:
				host_ready();
				break;
			case HOST_START_LISTEN:
				request_hid_reports_all();
				break;
			case HOST_STOP_LISTEN:
				stop_hid_reports_all();
				break;
			case HOST_POLL:
				poll_devices();
				break;
			default:
				break;
		}
		tuh_task(); // tinyusb host task
		tud_task(); // tinyusb device task
		tud_cdc_write_flush();
	}
}
