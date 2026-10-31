struct t150_state_packet;

static int t150_input_configured(struct hid_device *hdev, struct hid_input *hidinput);
static inline void t150_free_input(struct t150 *t150);
static int t150_input_open(struct input_dev *dev);
static void t150_input_close(struct input_dev *dev);
static int t150_update_input(struct hid_device *hdev, struct hid_report *report, uint8_t *packet_raw, int size);
