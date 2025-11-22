#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pio_usb.h"
#include "tusb.h"

#include "pico/cyw43_arch.h"

#include "hid_report.h"
#include "usb_device.h"
#include "bt_device.h"

#include "usb_host.h"


// -----------------------------------------------------------------------------
// Interface 0 
// -----------------------------------------------------------------------------
uint8_t const static_desc_report_0[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07,
    0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x08, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01,
    0x75, 0x08, 0x81, 0x03, 0x95, 0x06, 0x75, 0x08,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x05, 0x07, 0x19,
    0x00, 0x2A, 0xFF, 0x00, 0x81, 0x00, 0x25, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01,
    0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03,
    0x91, 0x03, 0xC0
};
uint16_t const static_desc_report_len_0 = sizeof(static_desc_report_0);

// -----------------------------------------------------------------------------
// Interface 1
// -----------------------------------------------------------------------------
uint8_t const static_desc_report_1[] = {
    0x05, 0x01, 0x09, 0x80, 0xA1, 0x01, 0x85, 0x01,
    0x19, 0x81, 0x29, 0x83, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x03, 0x81, 0x02, 0x95, 0x05,
    0x81, 0x01, 0xC0, 0x05, 0x0C, 0x09, 0x01, 0xA1,
    0x01, 0x85, 0x02, 0x19, 0x00, 0x2A, 0x3C, 0x02,
    0x15, 0x00, 0x26, 0x3C, 0x02, 0x95, 0x01, 0x75,
    0x10, 0x81, 0x00, 0xC0, 0x06, 0x00, 0xFF, 0x09,
    0x01, 0xA1, 0x01, 0x85, 0x05, 0x15, 0x00, 0x26,
    0xFF, 0x00, 0x19, 0x01, 0x29, 0x02, 0x75, 0x08,
    0x95, 0x05, 0xB1, 0x02, 0xC0, 0x05, 0x01, 0x09,
    0x06, 0xA1, 0x01, 0x85, 0x06, 0x05, 0x07, 0x19,
    0x04, 0x29, 0x70, 0x15, 0x00, 0x25, 0x01, 0x75,
    0x01, 0x95, 0x78, 0x81, 0x02, 0xC0
};
uint16_t const static_desc_report_len_1 = sizeof(static_desc_report_1);

host_state_t host_state;
static absolute_time_t request_time;
static absolute_time_t last_report;
static uint16_t host_poll_interval=HOST_POLL_INTERVAL;

// initialize usb host
void usb_host_init(void) {
	// configure PIO USB for TinyUSB host
	pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
	pio_cfg.alarm_pool = (void*) alarm_pool_create(2,1);
	tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

	// run TinyUSB host
	tusb_rhport_init_t host_init = {
		.role = TUSB_ROLE_HOST,
		.speed = TUSB_SPEED_AUTO,
	};
	tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT);
	tusb_init(BOARD_TUH_RHPORT, &host_init);

	host_state=HOST_INACTIVE;
}


// Invoked when device with hid interface is mounted
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
	uint16_t vid, pid;
	tuh_vid_pid_get(dev_addr, &vid, &pid);
	uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

	/// send device vid:pid information to CDC for debugging
	cdc_count = sprintf(cdc_buf, "Mount: [%04x:%04x][%u:%u] Protocol = %u  Desc Len = %u\n", vid, pid, dev_addr, instance, itf_protocol, desc_len);
	cdc_print_str(cdc_buf, cdc_count);
	cdc_print_hex(desc_report, desc_len);
	// add to HID report descriptor
	if(instance == 0){
		// use static keyboard descriptor for instance 0
		if ( add_descriptor(dev_addr, instance, static_desc_report_0, static_desc_report_len_0)) {
			host_state=HOST_NEW_DESCRIPTOR;
			request_time=get_absolute_time();
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
		}
	} else if (instance == 1){
		if ( add_descriptor(dev_addr, instance, static_desc_report_1, static_desc_report_len_1)) {
			host_state=HOST_NEW_DESCRIPTOR;
			request_time=get_absolute_time();
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
		}
	}
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
	// send device address:instance to CDC for debugging
	cdc_count = sprintf(cdc_buf, "Unmount: [%u:%u]\n", dev_addr, instance);
	cdc_print_str(cdc_buf, cdc_count);

	if (stop_hid_report(dev_addr, instance)) {
		cdc_print_msg("Successfully stopped receiving reports\n");
	}

	remove_instance(dev_addr, instance);
	host_state=HOST_NEW_DESCRIPTOR;
	request_time=get_absolute_time();
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
	if(len != 12){ // 12 writes garbage data for some reason, ignore
		cdc_count = sprintf(cdc_buf, "[%u:%u](%u)< ", dev_addr, instance, len);
		cdc_print_str(cdc_buf,cdc_count);
		cdc_print_hex(report, 8);
		
		last_report = get_absolute_time();
		host_state = HOST_WAIT_POLL;
		queue_report(dev_addr, instance, report, 8);
	}
}

// get host ready by updating descriptors
void host_ready(void) {
	if (absolute_time_diff_us(request_time, get_absolute_time()) >= 1000000){
		if ( generate_report_descriptor() ) {
			if ( num_mounted > 0 ) {
				host_state = HOST_MOUNTED;
				cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
			} else {
				host_state = HOST_INACTIVE;
			}

			cdc_print_msg("Updating HID report map\n");
			update_desc_hid_report();

			if ( device_state == DEVICE_ACTIVE ) {
				device_state = DEVICE_START;
			}
		}
	}
}

// set devices to listening once polling interval has passed
void poll_devices(void) {
	if (absolute_time_diff_us(last_report, get_absolute_time()) >= host_poll_interval) {
		request_hid_reports_all();
		host_state = HOST_LISTENING;
	}
}

// set the USB host polling interval based on the BT connection interval
void set_host_poll_interval(uint16_t bt_conn_interval) {
	if (bt_conn_interval*1250 > HOST_POLL_INTERVAL) {
		host_poll_interval = bt_conn_interval*1250;
	} else {
		host_poll_interval = HOST_POLL_INTERVAL;
	}
}

// request HID input reports on specified device address and instance
bool request_hid_report(uint8_t dev_addr, uint8_t instance) {
	// request to receive reports HID devices
	if ( !tuh_hid_receive_report(dev_addr, instance) ) {
		cdc_count = sprintf(cdc_buf, "Error: cannot request report on [%u:%u]\n", dev_addr, instance);
		cdc_print_str(cdc_buf, cdc_count);
		return false;
	}
	return true;
}

// stop receiving HID input reports on specified device address and instance
bool stop_hid_report(uint8_t dev_addr, uint8_t instance) {
	if (!tuh_hid_receive_abort(dev_addr, instance)) {
		cdc_print_msg("Error: could not stop receiving reports\n");
		return false;
	}

	return true;
}
