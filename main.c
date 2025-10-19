#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pio_usb.h"
#include "tusb.h"

#include "pico/cyw43_arch.h"

#include "hid_report.h"
#include "bt_device.h"
#include "usb_device.h"
#include "usb_host.h"

#include "blehid_config.h"

static void usb_main(void);

#ifdef BUTTON_PIN
static absolute_time_t last_button_time;
static uint32_t last_button_event;
static void gpio_callback(uint pin, uint32_t events);
#endif

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
	// setup GPIO button for pair selection
#ifdef BUTTON_PIN
	gpio_init(BUTTON_PIN);
	gpio_pull_up(BUTTON_PIN);
	gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
#endif
	

	// init and run usb host
	usb_host_init();

	// init and run usb device
	usb_device_init();

	// set initial state
	bt_state = BT_STATE_ACTIVE;
	device_state = DEVICE_INACTIVE;

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

		switch ( device_state ) {
			case DEVICE_STOP:
			case DEVICE_START:
				if ( tud_disconnect() ) {
					sleep_ms(10);
					tud_connect();
				}

				if ( device_state == DEVICE_STOP ) {
					device_state = DEVICE_INACTIVE;
					host_state = HOST_STOP_LISTEN;
				} else if (device_state == DEVICE_START ) {
					device_state = DEVICE_ACTIVE;
					if ( num_mounted > 0 ) {
						set_host_poll_interval(1);
						host_state = HOST_START_LISTEN;
					}
				}

				break;
			default:
				break;
		}

		switch ( bt_state ) {
			case BT_STATE_STOP:
				bt_state = BT_STATE_INACTIVE;
				update_desc_hid_report();
				break;
			case BT_STATE_START:
				bt_state = BT_STATE_ACTIVE;
				update_desc_hid_report();
				break;
			default:
				break;
		}

		tuh_task(); // tinyusb host task
		tud_task(); // tinyusb device task
		tud_cdc_write_flush();
	}
}

#ifdef BUTTON_PIN
static void gpio_callback(uint pin, uint32_t events) {
	uint64_t diff = absolute_time_diff_us(last_button_time, get_absolute_time());
	if (pin == BUTTON_PIN && diff >= BUTTON_DEBOUNCE) {
		if (events & GPIO_IRQ_EDGE_FALL) {
			// button pressed
			last_button_time = get_absolute_time();
		} else if (events & GPIO_IRQ_EDGE_RISE) {
			// button released
			if (diff >= BUTTON_LONG_PRESS) {
				//bt_device_command = BT_DEVICE_COMMAND_UNPAIR;
			} else {
				if ( bt_state == BT_STATE_ACTIVE ) {
					bt_state = BT_STATE_STOP;
				} else if ( bt_state == BT_STATE_INACTIVE ) {
					bt_state = BT_STATE_START;
				}

				if ( device_state == DEVICE_ACTIVE ) {
					device_state = DEVICE_STOP;
				} else if ( device_state == DEVICE_INACTIVE ) {
					device_state = DEVICE_START;
				}
				//bt_device_command = BT_DEVICE_COMMAND_SWITCH;
			}
			last_button_time = get_absolute_time();
		}
	}
}
#endif
