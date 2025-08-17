#define BTSTACK_FILE__ "bt_device.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "pico/multicore.h"
#include "btstack.h"
#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "ble/gatt-service/hids_device.h"

#include "hid_report.h"
#include "usb_device.h"
#include "usb_host.h"

#include "bt_device.h"
#include "ble_hid.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t l2cap_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static uint8_t battery = 100;
hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static uint8_t protocol_mode = 1;

static hids_device_report_t *dev_report_storage;

const uint8_t adv_data[] = {
	// Flags general discoverable, BR/EDR not supported
	0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
	// Name
	0x0d, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'P', 'i', 'c', 'o', ' ', 'B', 'L', 'E', ' ', 'H','I','D',
	// 16-bit Service UUIDs
	0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE & 0xff, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE >> 8,
	// Appearance HID - Keyboard (Category 15, Sub-Category 1)
	0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xC1, 0x03,
};
const uint8_t adv_data_len = sizeof(adv_data);

static void bt_hid_setup(void);
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// start BTstack
void btstack_main(void){
	bt_hid_setup();

	init_report_buf();

	hci_power_control(HCI_POWER_ON);
}

static void bt_hid_setup(void) {
	cyw43_arch_init();

	l2cap_init();

	// setup SM: Display only
	sm_init();
	sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
	//sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
	sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

	// setup ATT server
	att_server_init(profile_data, NULL, NULL);

	// setup battery service
	battery_service_server_init(battery);

	// setup device information service
	device_information_service_server_init();

	// setup HID Device service
	dev_report_storage = (hids_device_report_t *)malloc(sizeof(hids_device_report_t)*NUM_REPORT_IDS);
	hids_device_init_with_storage(0, get_desc_hid_report(), get_desc_hid_report_len(), NUM_REPORT_IDS, dev_report_storage);

	// setup advertisements
	uint16_t adv_int_min = 0x0030;
	uint16_t adv_int_max = 0x0030;
	uint8_t adv_type = 0;
	bd_addr_t null_addr;
	memset(null_addr, 0, 6);
	gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
	gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
	gap_advertisements_enable(1);

	// register for HCI events
	hci_event_callback_registration.callback = &packet_handler;
	hci_add_event_handler(&hci_event_callback_registration);

	// register for connection parameter updates
	l2cap_event_callback_registration.callback = &packet_handler;
	l2cap_add_event_handler(&l2cap_event_callback_registration);

	// register for SM events
	sm_event_callback_registration.callback = &packet_handler;
	sm_add_event_handler(&sm_event_callback_registration);

	// register for HIDS
	hids_device_register_packet_handler(packet_handler);
}

// handler for BTStack packets
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	(void) channel;
	(void) size;

	uint16_t conn_interval;

	if (packet_type != HCI_EVENT_PACKET) return;

	switch (hci_event_packet_get_type(packet)) {
		case HCI_EVENT_DISCONNECTION_COMPLETE:
			con_handle = HCI_CON_HANDLE_INVALID;
			printf("Disconnected\n");

			set_host_state(HOST_STOP_LISTEN);
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
			break;
		case SM_EVENT_JUST_WORKS_REQUEST:
			printf("Just Works requested\n");
			sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
			break;
		case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
			printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
			sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
			break;
		case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
			printf("Display Passkey: %"PRIu32"\n", sm_event_passkey_display_number_get_passkey(packet));
			break;
		case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
			printf("L2CAP Connection Parameter Update Complete, response: %x\n", l2cap_event_connection_parameter_update_response_get_result(packet));
			break;
		case HCI_EVENT_LE_META:
			switch (hci_event_le_meta_get_subevent_code(packet)) {
				case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
					// print connection parameters (without using float operations)
					conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
					printf("LE Connection Complete:\n");
					printf("- Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
					printf("- Connection Latency: %u\n", hci_subevent_le_connection_complete_get_conn_latency(packet));
					break;
				case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
					// print connection parameters (without using float operations)
					conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
					printf("LE Connection Update:\n");
					printf("- Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
					printf("- Connection Latency: %u\n", hci_subevent_le_connection_update_complete_get_conn_latency(packet));
					break;
				default:
					break;
			}
			break;	
		case HCI_EVENT_HIDS_META:
			switch (hci_event_hids_meta_get_subevent_code(packet)){
				case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
					con_handle = hids_subevent_input_report_enable_get_con_handle(packet);
					printf("Report Characteristic Subscribed %u\n", hids_subevent_input_report_enable_get_enable(packet));

					set_host_state(HOST_START_LISTEN);
					cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
					// request connection param update via L2CAP following Apple Bluetooth Design Guidelines
					// gap_request_connection_parameter_update(con_handle, 12, 12, 4, 100);    // 15 ms, 4, 1s

					// directly update connection params via HCI following Apple Bluetooth Design Guidelines
					// gap_update_connection_parameters(con_handle, 12, 12, 4, 100);	// 60-75 ms, 4, 1s

					break;
				case HIDS_SUBEVENT_PROTOCOL_MODE:
					protocol_mode = hids_subevent_protocol_mode_get_protocol_mode(packet);
					printf("Protocol Mode: %s mode\n", hids_subevent_protocol_mode_get_protocol_mode(packet) ? "Report" : "Boot");
					break;
				case HIDS_SUBEVENT_CAN_SEND_NOW:
					printf("HIDS_SUBEVENT_CAN_SEND_NOW\n");
					send_report();
					cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1-cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN));
					break;
				default:
					break;
			}
			break;

		default:
			break;
	}
}

// called to update the report map aka HID report descriptor on the BT interface
void update_desc_hid_report(void) {
	hci_power_control(HCI_POWER_OFF);

	uint16_t len = get_desc_hid_report_len();
	if (len>0) {
		hids_device_init_with_storage(0, get_desc_hid_report(), len, NUM_REPORT_IDS, dev_report_storage);
		hci_power_control(HCI_POWER_ON);
	}
}
