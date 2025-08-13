#ifndef MAIN_DEVICE_H_
#define MAIN_DEVICE_H_

void usb_device_init(void);
void cdc_print_hex(uint8_t const* msg, uint16_t msg_len);
void cdc_print_str(char const* msg, uint16_t msg_len);
void cdc_print_msg(char const* msg);

#endif

