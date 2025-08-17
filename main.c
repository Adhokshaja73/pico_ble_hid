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

// main loop
int main(void) {

	set_sys_clock_khz(192000, true);

	sleep_ms(10);

	// setup BLE on core 1
	multicore_reset_core1();
	multicore_launch_core1(btstack_main);

	// init and run usb host
	usb_host_init();

	// init and run usb device
	//usb_device_init();

	while (true) {
		switch ( get_host_state() ) {
			case HOST_NEW_DESCRIPTOR:
				if ( host_ready() ) {
					if(generate_report_descriptor()) {
						if ( get_desc_hid_report_len() > 0 ) {
							set_host_state(HOST_MOUNTED);
							cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
						} else {
							set_host_state(HOST_INACTIVE);
						}
						cdc_print_msg("Updating HID report map\n");
						update_desc_hid_report();
					}
				}
				break;
			case HOST_START_LISTEN:
				if (request_hid_reports_all()) {
					set_host_state(HOST_LISTENING);
				} else {
					cdc_print_msg("Error listening to input report(s)\n");
					set_host_state(HOST_INACTIVE);
				}
				break;
			case HOST_STOP_LISTEN:
				if (stop_hid_reports_all()) {
					set_host_state(HOST_INACTIVE);
				} else {
					cdc_print_msg("Error stopping input report(s)\n");
				}
				break;
			default:
				break;
		}
		tuh_task(); // tinyusb host task
		//tud_task(); // tinyusb device task
		//tud_cdc_write_flush();
	}

	return 0;
}
