#include "tusb.h"
#include "bsp/board_api.h"

#include "hid_report.h"
#include "usb_device.h"

#include "usb_descriptors.h"

// reserve space for HID descriptor
static uint8_t desc_configuration[DESC_CFG_MAX];
static uint16_t _desc_str[32+1];

// USB device descriptor
tusb_desc_device_t const desc_device =
{
	.bLength            = sizeof(tusb_desc_device_t),
	.bDescriptorType    = TUSB_DESC_DEVICE,
	.bcdUSB             = USB_BCD,

	// Use Interface Association Descriptor (IAD) for CDC
	// As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
	.bDeviceClass       = TUSB_CLASS_MISC,
	.bDeviceSubClass    = MISC_SUBCLASS_COMMON,
	.bDeviceProtocol    = MISC_PROTOCOL_IAD,

	.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

	.idVendor           = USB_VID,
	.idProduct          = USB_PID,
	.bcdDevice          = 0x0100,

	.iManufacturer      = 0x01,
	.iProduct           = 0x02,
	.iSerialNumber      = 0x03,

	.bNumConfigurations = 0x01
};

// string labels for device
char const* string_desc_arr [] =
{
	(const char[]) { 0x09, 0x04 },	// 0: is supported language is English (0x0409)
	"Raspberry Pi",					// 1: Manufacturer
	"Pico BLE HID",					// 2: Product
	NULL,							// 3: Serials, should use chip ID
	"Pico BLE HID CDC",				// 4: CDC
	"Pico USB HID",					// 5: USB HID Device
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
	return (uint8_t const *) &desc_device;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
	(void) index; // for multiple configurations
	// set configuration descriptor and CDC descriptor
	memset(desc_configuration, 0, sizeof(desc_configuration));

	if ( device_state == DEVICE_ACTIVE ) {
		uint8_t desc_initial[TUD_CONFIG_DESC_LEN+TUD_CDC_DESC_LEN+1] = {
			TUD_CONFIG_DESCRIPTOR(1, 4+num_mounted, 0, TUD_CONFIG_DESC_LEN+TUD_CDC_DESC_LEN+num_mounted*TUD_HID_DESC_LEN, 0x00, 100),
			TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
		};
		memcpy(desc_configuration, desc_initial, TUD_CONFIG_DESC_LEN+TUD_CDC_DESC_LEN);
	} else {
		uint8_t desc_initial[TUD_CONFIG_DESC_LEN+TUD_CDC_DESC_LEN+1] = {
			TUD_CONFIG_DESCRIPTOR(1, 4, 0, TUD_CONFIG_DESC_LEN+TUD_CDC_DESC_LEN, 0x00, 100),
			TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
		};
		memcpy(desc_configuration, desc_initial, TUD_CONFIG_DESC_LEN+TUD_CDC_DESC_LEN);
	}

	// add a HID descriptor for each interface mounted on host
	if ( device_state == DEVICE_ACTIVE ) {
		struct report_desc * descriptor;
		for (uint8_t i=0; i<num_mounted;i++) {
			descriptor = get_report_desc(i);
			if ( descriptor != NULL ) {
				uint8_t hid_desc[TUD_HID_DESC_LEN+1] = {
						TUD_HID_DESCRIPTOR(ITF_NUM_HID+i, 5, HID_ITF_PROTOCOL_NONE, descriptor->desc_len, EPNUM_HID+i, CFG_TUD_HID_EP_BUFSIZE, 1)
					};
				memcpy(&desc_configuration[TUD_CONFIG_DESC_LEN+TUD_CDC_DESC_LEN+i*TUD_HID_DESC_LEN], hid_desc, TUD_HID_DESC_LEN);
			}
		}
	}

	return desc_configuration;
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	(void) langid;

	uint8_t chr_count;

	switch (index) {
		case 0: // langid
			memcpy(&_desc_str[1], string_desc_arr[0], 2);
			chr_count = 1;
			break;
		case 3: // serial
			chr_count = board_usb_get_serial(_desc_str+1, 32);	
			break;
		default:
			// Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
			// https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

			if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

			char* str = string_desc_arr[index];

			// Cap at max char
			chr_count = (uint8_t) strlen(str);
			if ( chr_count > 31 ) chr_count = 31;

			// Convert ASCII string into UTF-16
			for(uint8_t i=0; i<chr_count; i++) {
				_desc_str[1+i] = str[i];
			}
			break;
	}

	// first byte is length (including header), second byte is string type
	_desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

	return _desc_str;
}

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf)
{
	// find HID report descriptor for indicated interface on the host and forward to device
	struct report_desc * descriptor = get_report_desc(itf);
	if ( descriptor != NULL ) {
		return descriptor->descriptor;
	}

	return NULL;
}
