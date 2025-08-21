#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

#include "ble/gatt-service/hids_device.h"
#include "btstack_ring_buffer.h"
#include "btstack_defines.h"

#include "usb_device.h"
#include "usb_host.h"
#include "bt_device.h"

#include "hid_report.h"

extern hci_con_handle_t con_handle;

uint8_t desc_hid_report[HID_DESCRIPTOR_SIZE];
uint16_t desc_hid_report_len=0;

static uint8_t buffer_storage[REPORT_BUF_SIZE];
static btstack_ring_buffer_t report_buf;

static struct report_data *report;
static struct report_desc *descriptors;

static struct report_desc* report_desc_alloc(void);
static void report_desc_init(struct report_desc *descriptor);
static void report_desc_free(struct report_desc *descriptor);
static struct report_desc* report_desc_find(uint8_t dev_addr, uint8_t instance);
static struct report_dict* report_dict_alloc(struct report_desc *descriptor);
static void report_dict_init(struct report_dict *mapping);
static void report_dict_free(struct report_dict *mapping, struct report_desc *descriptor);
static struct report_dict* find_mapping(uint8_t dev_addr, uint8_t instance, uint8_t report_id);

// start listening to HID input reports on all mounted devices
bool request_hid_reports_all(void) {
	// send request to receive reports on all mounted devices
	struct report_desc * current;
	for (current=descriptors; current != NULL; current=current->next) {
		if (! current->listening) {
			if(request_hid_report(current->dev_addr, current->instance)) {
				cdc_count = sprintf(cdc_buf, "Listening to input reports on [%u:%u]\n", current->dev_addr, current->instance);
				cdc_print_str(cdc_buf, cdc_count);
				current->listening = true;
			} else {
				cdc_print_msg("Error listening to input report(s)\n");
				stop_hid_reports_all();
				return false;
			}
		}
	}

	host_state = HOST_LISTENING;
	return true;
}

// stop listening to HID input reports on all mounted devices
bool stop_hid_reports_all(void) {
	// send request to stop reports on all mounted devices
	struct report_desc * current;
	for (current=descriptors; current != NULL; current=current->next) {
		if (current->listening) {
			if(stop_hid_report(current->dev_addr, current->instance)) {
				cdc_count = sprintf(cdc_buf, "Stopping input reports on [%u:%u]\n", current->dev_addr, current->instance);
				cdc_print_str(cdc_buf, cdc_count);
				current->listening = false;
			} else {
				cdc_print_msg("Error stopping input report(s)\n");
				return false;
			}
		}
	}

	host_state = HOST_INACTIVE;
	return true;
}

// used to send next HID input report on the BLE interface
void send_report(){
	// process queue and send next report
	if (btstack_ring_buffer_bytes_available(&report_buf)) {
		uint8_t len8[2];
		uint32_t num_bytes_read;

		// retrieve dev_addr from ring_buffer
		btstack_ring_buffer_read(&report_buf, &report->dev_addr, 1, &num_bytes_read);

		// retrieve instance from ring buffer
		btstack_ring_buffer_read(&report_buf, &report->instance, 1, &num_bytes_read);
	
		// retrieve length as two uint8_t and turn into uint16_t
		btstack_ring_buffer_read(&report_buf, len8, 2, &num_bytes_read);
		memcpy(&report->len, len8, 2);

		// retrieve report from ring buffer
		btstack_ring_buffer_read(&report_buf, report->report, report->len, &num_bytes_read);

		// find report id mapping
		struct report_dict * mapping = find_mapping(report->dev_addr, report->instance, report->report[0]);
	
		cdc_count=sprintf(cdc_buf, "[%04x](%u)> ", con_handle, mapping->ble_id);
		cdc_print_str(cdc_buf, cdc_count);

		// send hid report to ble interface
		if (mapping != NULL) {
			if (mapping->report_id==0) {
				hids_device_send_input_report_for_id(con_handle, mapping->ble_id, report->report, report->len);
				cdc_print_hex(report->report, report->len);
			} else {
				// replace report ID in original report before sending
				hids_device_send_input_report_for_id(con_handle, mapping->ble_id, &report->report[1], report->len-1);
				cdc_print_hex(&report->report[1], report->len-1);
			}
		}
		
		// set host to poll USB
		if (host_state == HOST_WAIT_POLL) {
			host_state = HOST_POLL;
		}
		// request sending of next report
		if (btstack_ring_buffer_bytes_available(&report_buf)) {
			hids_device_request_can_send_now_event(con_handle);
		}
	}
}

// add report to the BTstack ring buffer for sending
void queue_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
	if (con_handle != HCI_CON_HANDLE_INVALID) {
		// convert len to array of two uint8_t from single uint16_t
		uint8_t len8[2];
		memcpy(len8, &len, 2);

		if (btstack_ring_buffer_bytes_free(&report_buf) >= len+5) {
			// put instance, length, and report into ring buffer if space available
			btstack_ring_buffer_write(&report_buf, &dev_addr, 1);
			btstack_ring_buffer_write(&report_buf, &instance, 1);
			btstack_ring_buffer_write(&report_buf, len8, 2);
			btstack_ring_buffer_write(&report_buf, report, len);
		}

		// request send on BLE HID interface
		hids_device_request_can_send_now_event(con_handle);
	}

	// HID report on device has not been requested, flag so it can be polled
	struct report_desc * descriptor;
	descriptor = report_desc_find(dev_addr, instance);
	descriptor->listening = false;
}

// allocate memory for HID report ring buffer
void init_report_buf(void) {
	btstack_ring_buffer_init(&report_buf, buffer_storage, sizeof(buffer_storage));
	report = REPORT_DATA_ALLOC();
}

// allocate memory for USB interface report descriptor
static struct report_desc * report_desc_alloc(void) {
	struct report_desc *ret = REPORT_DESC_ALLOC();

	if (ret != NULL) {
		report_desc_init(ret);
		if (descriptors == NULL) {
			descriptors = ret;
		} else {
			struct report_desc *last;
			for (last = descriptors; last->next != NULL; last=last->next);
			last->next = ret;
		}
	}

	return ret;
}

// initialize report descriptor struct
static void report_desc_init(struct report_desc *descriptor) {
	memset(descriptor, 0, sizeof(struct report_desc));
	descriptor->next = NULL;
	descriptor->mappings = NULL;
}

// free memory and teardown usb->bt report ID mappings for report descriptor struct
static void report_desc_free(struct report_desc *descriptor) {
	if (descriptor != NULL) {
		if (descriptors == descriptor) {
			descriptors = descriptor->next;
		} else {
			struct report_desc *last;
			for (last = descriptors; last->next != NULL; last = last->next) {
				if ((last->next) == descriptor) {
					last->next = descriptor->next;
					break;
				}
			}
		}
		while (descriptor->mappings != NULL) {
			report_dict_free(descriptor->mappings, descriptor);
		}
		free(descriptor);
	}
}

// add report descriptor for new HID interface
bool add_descriptor(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
	struct report_desc * descriptor = report_desc_alloc();
	if (descriptor == NULL) {
		return false;
	}
	
	memcpy(descriptor->descriptor, desc_report, desc_len);
	descriptor->desc_len = desc_len;
	descriptor->dev_addr = dev_addr;
	descriptor->instance = instance;
	descriptor->listening = false;

	return true;

}

// generate new report descriptor for BLE device and save mappings for report IDs
bool generate_report_descriptor(void) {
	memset(desc_hid_report, 0, sizeof(desc_hid_report));
	desc_hid_report_len=0;

	uint8_t num_mounted = 0;

	struct report_desc * current;
	for (current=descriptors; current != NULL; current=current->next) {
		if (desc_hid_report_len + (current->desc_len) + 2 < HID_DESCRIPTOR_SIZE) {
			memcpy(&desc_hid_report[desc_hid_report_len], current->descriptor, current->desc_len);
		} else {
			cdc_print_msg("Err: HID report descriptor too long\n");
			return false;
		}

		bool no_id = true;

		// search for report ID (0x85) in report descriptor
		for (int i=0; i<(current->desc_len); i++) {
			if(current->descriptor[i] == 0x85) {
				// use report ID found in descriptor
				no_id=false;
				i++;

				// modify report ID and save mapping
				struct report_dict * report_map = report_dict_alloc(current);
				if (report_map == NULL) {
					return false;
				}
				num_mounted++;
				report_map->report_id = current->descriptor[i];
				report_map->ble_id = num_mounted;

				desc_hid_report[desc_hid_report_len+i]=num_mounted;
			}
		}

		// no report ID found in descriptor for the interface
		if (no_id) {
			// add report id field to beginning of descriptor after Collection (0xa1) field
			uint8_t col_pos=0;
			for (int i=0; i<(current->desc_len); i++) {
				if(current->descriptor[i] == 0xa1) {
					col_pos=i;
					break;
				}
			}
			desc_hid_report[desc_hid_report_len+col_pos+2] = 0x85;
			desc_hid_report[desc_hid_report_len+col_pos+3] = num_mounted+1;
			memcpy(&desc_hid_report[desc_hid_report_len+col_pos+4], &current->descriptor[col_pos+2], current->desc_len-col_pos);
			desc_hid_report_len += current->desc_len+2;

			// store mapping with report ID of 0
			struct report_dict * report_map = report_dict_alloc(current);
			num_mounted++;
			report_map->report_id = 0;
			report_map->ble_id = num_mounted;
		} else {
			desc_hid_report_len += current->desc_len;
		}
	}

	if (num_mounted > NUM_REPORT_IDS) {
		cdc_print_msg("Error: too many report IDs\n");
		return false;
	}

	btstack_ring_buffer_reset(&report_buf);

	return true;
}

// remove report descriptor for HID interface
void remove_instance(uint8_t dev_addr, uint8_t instance) {
	struct report_desc *descriptor = report_desc_find(dev_addr, instance);

	if (descriptor != NULL) {
		report_desc_free(descriptor);
	}
}

// find report descriptor by device address and instance
static struct report_desc * report_desc_find(uint8_t dev_addr, uint8_t instance) {
	struct report_desc *descriptor;
	for (descriptor = descriptors; descriptor != NULL; descriptor = descriptor->next) {
		if (descriptor->dev_addr==dev_addr && descriptor->instance==instance) {
			break;
		}
	}

	return descriptor;
}

// allocate memory for usb->bt report id mapping
static struct report_dict * report_dict_alloc(struct report_desc * descriptor) {
	struct report_dict *ret = REPORT_DICT_ALLOC();

	if (ret != NULL) {
		report_dict_init(ret);
		if (descriptor->mappings == NULL) {
			descriptor->mappings = ret;
		} else {
			struct report_dict *last;
			for (last = descriptor->mappings; last->next != NULL; last=last->next);
			last->next = ret;
		}
	}

	return ret;
}

// initialize usb->bt report id mapping struct
static void report_dict_init(struct report_dict *mapping) {
	memset(mapping, 0, sizeof(struct report_dict));
	mapping->next = NULL;
}

// free memory from report id mapping
static void report_dict_free(struct report_dict *mapping, struct report_desc *descriptor) {
	if (mapping != NULL) {
		if (descriptor->mappings == mapping) {
			descriptor->mappings = mapping->next;
		} else {
			struct report_dict *last;
			for (last = descriptor->mappings; last->next != NULL; last = last->next) {
				if ((last->next) == mapping) {
					last->next = mapping->next;
					break;
				}
			}
		}
		free(mapping);
	}
}

// find the usb->bt report id mapping struct for given device, instance, and usb report id
static struct report_dict * find_mapping(uint8_t dev_addr, uint8_t instance, uint8_t report_id) {
	struct report_desc * descriptor;
	descriptor = report_desc_find(dev_addr, instance);

	if (descriptor != NULL) {
		struct report_dict * mapping;
		for (mapping = descriptor->mappings; mapping != NULL; mapping = mapping->next) {
			if (mapping->report_id ==0 || mapping->report_id == report_id) {
				return mapping;
			}
		}
	}

	return NULL;
}
