#ifndef HID_REPORT_H_
#define HID_REPORT_H_

#define NUM_REPORT_IDS		16
#define REPORT_BUF_SIZE		256
#define DESCRIPTOR_BUF_SIZE	256
#define HID_DESCRIPTOR_SIZE	512

struct report_desc {
	uint8_t dev_addr;
	uint8_t instance;
	uint8_t descriptor[DESCRIPTOR_BUF_SIZE];
	uint16_t desc_len;
	struct report_desc *next;
	struct report_dict *mappings;
	bool listening;
};

struct report_dict {
	uint8_t report_id;
	uint8_t ble_id;
	struct report_dict *next;
};

# define REPORT_DESC_ALLOC() (struct report_desc *)malloc(sizeof(struct report_desc))
# define REPORT_DICT_ALLOC() (struct report_dict *)malloc(sizeof(struct report_dict))

bool request_hid_reports_all(void);
bool stop_hid_reports_all(void);
void send_report();
void queue_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);
void init_report_buf(void);
bool add_descriptor(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);
void remove_instance(uint8_t dev_addr, uint8_t instance);
bool generate_report_descriptor(void);
uint16_t get_desc_hid_report_len(void);
uint8_t const* get_desc_hid_report(void);

#endif
