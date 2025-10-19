#ifndef BT_DEVICE_H_
#define BT_DEVICE_H_

#define BT_LOOP_POLL_INTERVAL 100

typedef enum {
	BT_STATE_INACTIVE=0,
	BT_STATE_STOP,
	BT_STATE_START,
	BT_STATE_ACTIVE
} bt_state_t;

typedef enum {
	BT_DEVICE_COMMAND_NONE=0,
	BT_DEVICE_COMMAND_SWITCH,
	BT_DEVICE_COMMAND_UNPAIR,
	BT_DEVICE_COMMAND_UPDATE,
} bt_device_command_t;

extern bt_state_t bt_state;
extern bt_device_command_t bt_device_command;
extern uint16_t conn_interval;

void btstack_main(void);
void update_desc_hid_report(void);

#endif
