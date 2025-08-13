#ifndef MAIN_HOST_H_
#define MAIN_HOST_H_

typedef enum {
	HOST_INACTIVE=0,
	HOST_NEW_DESCRIPTOR,
	HOST_MOUNTED,
	HOST_START_LISTEN,
	HOST_LISTENING,
	HOST_STOP_LISTEN,
} host_state_t;


void usb_host_init(void);
void set_host_state(host_state_t new_host_state);
host_state_t get_host_state(void);
bool host_ready(void);
bool request_hid_report(uint8_t dev_addr, uint8_t instance);
bool stop_hid_report(uint8_t dev_addr, uint8_t instance);

#endif

