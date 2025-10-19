#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "usb_device.h"

char cdc_buf[64];
uint16_t cdc_len;
size_t cdc_count;

device_state_t device_state;

void usb_device_init(void) {
	// run TinyUSB device
	tusb_rhport_init_t dev_init = {
		.role = TUSB_ROLE_DEVICE,
		.speed = TUSB_SPEED_AUTO,
	};
	tusb_init(BOARD_TUH_RHPORT, &dev_init);
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
	(void) instance;
	(void) report_id;
	(void) report_type;
	(void) buffer;
	(void) bufsize;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
	(void) instance;
	(void) report_id;
	(void) report_type;
	(void) buffer;
	(void) reqlen;

	return 0;
}

bool forward_report(uint8_t instance, uint8_t const* report, uint16_t len) {
	return tud_hid_n_report(instance, 0, report, len);
}

// print message to CDC in raw hex
void cdc_print_hex(uint8_t const* msg, uint16_t msg_len) {
	(void) msg;
	(void) msg_len;
	for (int i=0; i<msg_len; i++) {
		cdc_count=sprintf(cdc_buf, "%02X ", msg[i]);
		tud_cdc_write(cdc_buf, cdc_count);
	}
	tud_cdc_write_str("\n");
}

// print text message to CDC
void cdc_print_str(char const* msg, uint16_t msg_len) {
	(void) msg;
	(void) msg_len;
	tud_cdc_write(msg, msg_len);
}

void cdc_print_msg(char const* msg) {
	cdc_len = strlen(msg);
	cdc_print_str(msg, cdc_len);
}
