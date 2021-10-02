#include "usb_device_drivers.h"
#include "utils.h"
#include "wiimote.h"

struct xbx1_private_data_t {
	enum wiimote_mgr_ext_u extension;
};
static_assert(sizeof(struct xbx1_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

struct xbx1_input_report {
	u8 report_id;
	u8 left_x;
	u8 left_y;
	u8 right_x;
	u8 righty_;

	u8 a        : 1;
	u8 b        : 1;
	u8 y        : 1;
	u8 x        : 1;
	u8 dpad	    : 4;

	u8 r3      : 1;
	u8 l3      : 1;
	u8 options : 1;
	u8 share   : 1;
	u8 r2      : 1;
	u8 l2      : 1;
	u8 r1      : 1;
	u8 l1      : 1;

	u8 cnt1   : 6;
	u8 tpad   : 1;
	u8 home     : 1;

	u8 l_trigger;
	u8 r_trigger;

	u8 cnt2;
	u8 cnt3;

	u8 battery;

	s16 accel_x;
	s16 accel_y;
	s16 accel_z;

	union {
		s16 roll;
		s16 gyro_z;
	};

	union {
		s16 yaw;
		s16 gyro_y;
	};

	union {
		s16 pitch;
		s16 gyro_x;
	};

	u8 unk1[5];

	u8 padding       : 1;
	u8 microphone    : 1;
	u8 headphones    : 1;
	u8 usb_plugged   : 1;
	u8 battery_level : 4;

	u8 unk2[2];
	u8 trackpadpackets;
	u8 packetcnt;

	u32 finger1_nactive : 1;
	u32 finger1_id      : 7;
	u32 finger1_x       : 12;
	u32 finger1_y       : 12;

	u32 finger2_nactive : 1;
	u32 finger2_id      : 7;
	u32 finger2_x       : 12;
	u32 finger2_y       : 12;
} ATTRIBUTE_PACKED;

static inline void xbx1_map_buttons(const struct xbx1_input_report *input, u16 *buttons)
{
	if (input->dpad == 0 || input->dpad == 1 || input->dpad == 7)
		*buttons |= WPAD_BUTTON_UP;
	else if (input->dpad == 3 || input->dpad == 4 || input->dpad == 5)
		*buttons |= WPAD_BUTTON_DOWN;
	if (input->dpad == 1 || input->dpad == 2 || input->dpad == 3)
		*buttons |= WPAD_BUTTON_RIGHT;
	else if (input->dpad == 5 || input->dpad == 6 || input->dpad == 7)
		*buttons |= WPAD_BUTTON_LEFT;
	if (input->a)
		*buttons |= WPAD_BUTTON_A;
	if (input->b)
		*buttons |= WPAD_BUTTON_B;
	if (input->y)
		*buttons |= WPAD_BUTTON_1;
	if (input->x)
		*buttons |= WPAD_BUTTON_2;
	if (input->home)
		*buttons |= WPAD_BUTTON_HOME;
	if (input->share)
		*buttons |= WPAD_BUTTON_MINUS;
	if (input->options)
		*buttons |= WPAD_BUTTON_PLUS;
}

static int xbx1_set_leds_rumble(usb_input_device_t *device, u8 r, u8 g, u8 b)
{
	u8 buf[] ATTRIBUTE_ALIGN(32) = {
		0x05, // Report ID
		0x03, 0x00, 0x00,
		0x00, // Fast motor
		0x00, // Slow motor
		r, g, b, // RGB
		0x00, // LED on duration
		0x00  // LED off duration
	};

	return usb_device_driver_issue_intr_transfer(device, 1, buf, sizeof(buf));
}

static inline int xbx1_request_data(usb_input_device_t *device)
{
	return usb_device_driver_issue_intr_transfer_async(device, 0, device->usb_async_resp,
							   sizeof(device->usb_async_resp));
}

int xbx1_driver_ops_init(usb_input_device_t *device)
{
	struct xbx1_private_data_t *priv = (void *)device->private_data;

	/* Set initial extension */
	priv->extension = WIIMOTE_MGR_EXT_NUNCHUK;
	fake_wiimote_mgr_set_extension(device->wiimote, priv->extension);

	return xbx1_request_data(device);
}

int xbx1_driver_ops_disconnect(usb_input_device_t *device)
{
	xbx1_set_leds_rumble(device, 0, 0, 0);
	return 0;
}

int xbx1_driver_ops_slot_changed(usb_input_device_t *device, u8 slot)
{
	static u8 colors[5][3] = {
		{  0,   0,   0},
		{  0,   0, 255},
		{255,   0,   0},
		{0,   255,   0},
		{255,   0, 255},
	};

	slot = slot % ARRAY_SIZE(colors[0]);

	u8 r = colors[slot][0],
	   g = colors[slot][1],
	   b = colors[slot][2];

	return xbx1_set_leds_rumble(device, r, g, b);
}

int xbx1_driver_ops_usb_async_resp(usb_input_device_t *device)
{
	struct xbx1_private_data_t *priv = (void *)device->private_data;
	struct xbx1_input_report *report = (void *)device->usb_async_resp;
	u16 buttons = 0;
	struct wiimote_extension_data_format_nunchuk_t nunchuk;

	if (report->report_id == 0x01) {
		xbx1_map_buttons(report, &buttons);

		if (priv->extension == WIIMOTE_MGR_EXT_NUNCHUK) {
			memset(&nunchuk, 0, sizeof(nunchuk));
			nunchuk.jx = report->left_x;
			nunchuk.jy = 255 - report->left_y;
			nunchuk.bt.c = !report->l1;
			nunchuk.bt.z = !report->l2;
			fake_wiimote_mgr_report_input_ext(device->wiimote, buttons,
							  &nunchuk, sizeof(nunchuk));
		} else {
			fake_wiimote_mgr_report_input(device->wiimote, buttons);
		}
	}

	return xbx1_request_data(device);
}
