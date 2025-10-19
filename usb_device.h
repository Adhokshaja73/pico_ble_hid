#ifndef USB_DEVICE_H_
#define USB_DEVICE_H_

typedef enum {
	DEVICE_INACTIVE=0,
	DEVICE_START,
	DEVICE_STOP,
	DEVICE_ACTIVE
} device_state_t;

extern char cdc_buf[64];
extern uint16_t cdc_len;
extern size_t cdc_count;
extern device_state_t device_state;

void usb_device_init(void);
bool forward_report(uint8_t instance, uint8_t const* report, uint16_t len);
void cdc_print_hex(uint8_t const* msg, uint16_t msg_len);
void cdc_print_str(char const* msg, uint16_t msg_len);
void cdc_print_msg(char const* msg);

#endif

