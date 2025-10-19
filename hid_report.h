#ifndef HID_REPORT_H_
#define HID_REPORT_H_

#define REPORT_MAX_SIZE		64
#define NUM_REPORT_IDS		16
#define REPORT_BUF_SIZE		256
#define DESCRIPTOR_BUF_SIZE	256
#define HID_DESCRIPTOR_SIZE	512

struct report_desc {
	uint8_t dev_addr;
	uint8_t instance;
	uint8_t descriptor[DESCRIPTOR_BUF_SIZE];
	uint16_t desc_len;
	uint8_t dev_instance;
	struct report_desc *next;
	struct report_dict *mappings;
	bool listening;
};

struct report_dict {
	uint8_t report_id;
	uint8_t ble_id;
	struct report_dict *next;
};

struct report_data {
	uint8_t dev_addr;
	uint8_t instance;
	uint8_t report[REPORT_MAX_SIZE];
	uint16_t len;
};

#define REPORT_DESC_ALLOC() (struct report_desc *)malloc(sizeof(struct report_desc))
#define REPORT_DICT_ALLOC() (struct report_dict *)malloc(sizeof(struct report_dict))
#define REPORT_DATA_ALLOC() (struct report_data *)malloc(sizeof(struct report_data))

extern uint8_t desc_hid_report[HID_DESCRIPTOR_SIZE];
extern uint16_t desc_hid_report_len;
extern uint8_t num_mounted;

bool request_hid_reports_all(void);
bool stop_hid_reports_all(void);
void send_report();
void queue_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);
void init_report_buf(void);
bool add_descriptor(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);
void remove_instance(uint8_t dev_addr, uint8_t instance);
bool generate_report_descriptor(void);
struct report_desc * get_report_desc(uint8_t dev_instance);

#endif
