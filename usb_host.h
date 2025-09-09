#ifndef USB_HOST_H_
#define USB_HOST_H_

#define HOST_POLL_INTERVAL	8000

typedef enum {
	HOST_INACTIVE=0,
	HOST_NEW_DESCRIPTOR,
	HOST_MOUNTED,
	HOST_START_LISTEN,
	HOST_LISTENING,
	HOST_WAIT_POLL,
	HOST_POLL,
	HOST_STOP_LISTEN,
} host_state_t;

extern host_state_t host_state;

void usb_host_init(void);
void host_ready(void);
void poll_devices(void);
void set_host_poll_interval(uint16_t bt_conn_interval);
bool request_hid_report(uint8_t dev_addr, uint8_t instance);
bool stop_hid_report(uint8_t dev_addr, uint8_t instance);

#endif

