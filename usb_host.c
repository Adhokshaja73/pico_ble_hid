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

#include "usb_host.h"

static host_state_t host_state;
static absolute_time_t request_time;

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

	set_host_state(HOST_INACTIVE);
}


// Invoked when device with hid interface is mounted
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
	uint16_t vid, pid;
	tuh_vid_pid_get(dev_addr, &vid, &pid);
	uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

	/// send device vid:pid information to CDC for debugging
	char tempbuf[CFG_TUD_CDC_TX_BUFSIZE];
	size_t count = sprintf(tempbuf, "Mount: [%04x:%04x][%u:%u] Protocol = %u\n", vid, pid, dev_addr, instance, itf_protocol);
	cdc_print_str(tempbuf, count);

	// print descriptor
	//cdc_print_hex(desc_report, desc_len);

	// add to HID report descriptor
	if ( add_descriptor(dev_addr, instance, desc_report, desc_len)) {
		set_host_state(HOST_NEW_DESCRIPTOR);
		request_time=get_absolute_time();
		cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
	}
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
	// send device address:instance to CDC for debugging
	char tempbuf[CFG_TUD_CDC_TX_BUFSIZE];
	size_t count = sprintf(tempbuf, "Unmount: [%u:%u]\n", dev_addr, instance);
	cdc_print_str(tempbuf, count);

	if (stop_hid_report(dev_addr, instance)) {
		cdc_print_msg("Successfully stopped receiving reports\n");
	}

	remove_instance(dev_addr, instance);
	set_host_state(HOST_NEW_DESCRIPTOR);
	request_time=get_absolute_time();
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
	char tempbuf[32];
	size_t count = sprintf(tempbuf, "[%04x:%04x](%u) ", dev_addr, instance, len);
	cdc_print_str(tempbuf,count);
	cdc_print_hex(report, len);

	queue_report(dev_addr, instance, report, len);

	// continue to request to receive report
	if ( !tuh_hid_receive_report(dev_addr, instance) )
	{
		cdc_print_msg("Error: cannot request report\r\n");
	}
}

// set the state of the host
void set_host_state(host_state_t new_host_state) {
	host_state = new_host_state;
}

// get the current state of the host
host_state_t get_host_state(void) {
	return host_state;
}

// indicate whether host is ready for updating descriptors
bool host_ready(void) {
	return (absolute_time_diff_us(request_time, get_absolute_time()) > 1000000);
}

// request HID input reports on specified device address and instance
bool request_hid_report(uint8_t dev_addr, uint8_t instance) {
	// request to receive reports HID devices
	if ( !tuh_hid_receive_report(dev_addr, instance) ) {
		char tempbuf[CFG_TUD_CDC_TX_BUFSIZE];
		size_t count = sprintf(tempbuf, "Error: cannot request report on [%u:%u]\n", dev_addr, instance);
		cdc_print_str(tempbuf, count);
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
