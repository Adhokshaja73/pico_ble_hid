#ifndef USB_DEVICE_H_
#define USB_DEVICE_H_

extern char cdc_buf[64];
extern uint16_t cdc_len;
extern size_t cdc_count;

void usb_device_init(void);
void cdc_print_hex(uint8_t const* msg, uint16_t msg_len);
void cdc_print_str(char const* msg, uint16_t msg_len);
void cdc_print_msg(char const* msg);

#endif

