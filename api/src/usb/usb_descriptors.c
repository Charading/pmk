/* USB descriptors for TinyUSB HID keyboard + raw HID (vendor) - Mina65
 * Matches Shego75 4-interface structure for reliable bidirectional HID
 */
#include "tusb.h"
#include <string.h>

// Vendor/Product IDs - Mina65
#define USB_VID 0xDEAD
#define USB_PID 0xFADE

// String descriptors
const char* string_desc_arr[] = {
	(const char[]){0}, // 0: supported language (handled specially)
	"Mina Labs",       // 1: Manufacturer
	"Mina65",          // 2: Product
	"0001",            // 3: Serial
};

// Device descriptor
tusb_desc_device_t const desc_device = {
	.bLength = sizeof(tusb_desc_device_t),
	.bDescriptorType = TUSB_DESC_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0x00,
	.bDeviceSubClass = 0x00,
	.bDeviceProtocol = 0x00,
	.bMaxPacketSize0 = 64,
	.idVendor = USB_VID,
	.idProduct = USB_PID,
	.bcdDevice = 0x0100,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1
};

// HID report descriptor (keyboard + consumer control combined)
const uint8_t hid_report_desc_kbd[] = {
	// Report ID 1: Keyboard
	0x05, 0x01,       // Usage Page (Generic Desktop)
	0x09, 0x06,       // Usage (Keyboard)
	0xA1, 0x01,       // Collection (Application)
	0x85, 0x01,       //   Report ID (1)
	0x05, 0x07,       //   Usage Page (Key Codes)
	0x19, 0xE0,       //   Usage Minimum (224)
	0x29, 0xE7,       //   Usage Maximum (231)
	0x15, 0x00,       //   Logical Minimum (0)
	0x25, 0x01,       //   Logical Maximum (1)
	0x75, 0x01,       //   Report Size (1)
	0x95, 0x08,       //   Report Count (8)
	0x81, 0x02,       //   Input (Data,Var,Abs) - Modifiers
	0x95, 0x01,       //   Report Count (1)
	0x75, 0x08,       //   Report Size (8)
	0x81, 0x01,       //   Input (Constant) - Reserved byte
	0x95, 0x05,       //   Report Count (5)
	0x75, 0x01,       //   Report Size (1)
	0x05, 0x08,       //   Usage Page (LEDs)
	0x19, 0x01,       //   Usage Minimum (1)
	0x29, 0x05,       //   Usage Maximum (5)
	0x91, 0x02,       //   Output (Data,Var,Abs) - LED report
	0x95, 0x01,       //   Report Count (1)
	0x75, 0x03,       //   Report Size (3)
	0x91, 0x01,       //   Output (Constant) - LED padding
	0x95, 0x06,       //   Report Count (6)
	0x75, 0x08,       //   Report Size (8)
	0x15, 0x00,       //   Logical Minimum (0)
	0x25, 0x65,       //   Logical Maximum (101)
	0x05, 0x07,       //   Usage Page (Key Codes)
	0x19, 0x00,       //   Usage Minimum (0)
	0x29, 0x65,       //   Usage Maximum (101)
	0x81, 0x00,       //   Input (Data,Array) - Key array
	0xC0,             // End Collection

	// Report ID 2: Consumer Control (for volume/mute/media)
	0x05, 0x0C,       // Usage Page (Consumer)
	0x09, 0x01,       // Usage (Consumer Control)
	0xA1, 0x01,       // Collection (Application)
	0x85, 0x02,       //   Report ID (2)
	0x15, 0x00,       //   Logical Minimum (0)
	0x26, 0xFF, 0x03, //   Logical Maximum (1023)
	0x19, 0x00,       //   Usage Minimum (0)
	0x2A, 0xFF, 0x03, //   Usage Maximum (1023)
	0x75, 0x10,       //   Report Size (16)
	0x95, 0x01,       //   Report Count (1)
	0x81, 0x00,       //   Input (Data,Array)
	0xC0              // End Collection
};

// VIA/SignalRGB Raw HID report descriptor (vendor-defined, 32-byte IN/OUT, no Report ID)
const uint8_t hid_report_desc_via_raw[] = {
	0x06, 0x60, 0xFF, // Usage Page (Vendor Defined 0xFF60)
	0x09, 0x61,       // Usage (0x61)
	0xA1, 0x01,       // Collection (Application)
	0x75, 0x08,       //   Report Size (8)
	0x95, 0x20,       //   Report Count (32)
	0x15, 0x00,       //   Logical Minimum (0)
	0x25, 0xFF,       //   Logical Maximum (255)
	0x09, 0x61,       //   Usage (0x61)
	0x81, 0x02,       //   Input (Data,Var,Abs)
	0x95, 0x20,       //   Report Count (32)
	0x09, 0x61,       //   Usage (0x61)
	0x91, 0x02,       //   Output (Data,Var,Abs)
	0xC0
};

// App Raw HID report descriptor (vendor-defined, 64-byte IN/OUT, Report ID 2)
const uint8_t hid_report_desc_raw[] = {
	0x06, 0x00, 0xFF, // Usage Page (Vendor Defined 0xFF00)
	0x09, 0x01,       // Usage (0x01)
	0xA1, 0x01,       // Collection (Application)
	0x85, 0x02,       //   Report ID (2)
	0x75, 0x08,       //   Report Size (8)
	0x95, 0x40,       //   Report Count (64)
	0x15, 0x00,       //   Logical Minimum (0)
	0x25, 0xFF,       //   Logical Maximum (255)
	0x09, 0x01,       //   Usage (0x01)
	0x81, 0x02,       //   Input (Data,Var,Abs)
	0x95, 0x40,       //   Report Count (64)
	0x09, 0x01,       //   Usage (0x01)
	0x91, 0x02,       //   Output (Data,Var,Abs)
	0xC0
};

// Response Raw HID report descriptor (vendor-defined, 64-byte IN/OUT, NO Report ID)
const uint8_t hid_report_desc_resp_raw[] = {
	0x06, 0x00, 0xFF, // Usage Page (Vendor Defined 0xFF00)
	0x09, 0x02,       // Usage (0x02)
	0xA1, 0x01,       // Collection (Application)
	0x75, 0x08,       //   Report Size (8)
	0x95, 0x40,       //   Report Count (64)
	0x15, 0x00,       //   Logical Minimum (0)
	0x25, 0xFF,       //   Logical Maximum (255)
	0x09, 0x02,       //   Usage (0x02)
	0x81, 0x02,       //   Input (Data,Var,Abs)
	0x95, 0x40,       //   Report Count (64)
	0x09, 0x02,       //   Usage (0x02)
	0x91, 0x02,       //   Output (Data,Var,Abs)
	0xC0
};

// Interface numbers (same as Shego)
enum {
	ITF_NUM_HID_KBD = 0,
	ITF_NUM_HID_VIA_RAW,
	ITF_NUM_HID_APP_RAW,
	ITF_NUM_HID_RESP_RAW,
	ITF_NUM_TOTAL
};

// Configuration descriptor total length
// Config + Keyboard (IF+HID+EP) + VIA raw (IF+HID+2EP) + App raw (IF+HID+2EP) + Resp raw (IF+HID+2EP)
#define DESC_TOTAL_LEN (9 + (9 + 9 + 7) + (9 + 9 + 7 + 7) + (9 + 9 + 7 + 7) + (9 + 9 + 7 + 7))

uint8_t const desc_configuration[] = {
	// Configuration Descriptor
	0x09,                         // bLength
	0x02,                         // bDescriptorType (Configuration)
	DESC_TOTAL_LEN & 0xFF,        // wTotalLength LSB
	(DESC_TOTAL_LEN >> 8) & 0xFF, // wTotalLength MSB
	ITF_NUM_TOTAL,                // bNumInterfaces
	0x01,                         // bConfigurationValue
	0x00,                         // iConfiguration
	0x80,                         // bmAttributes (bus powered)
	0xFA,                         // bMaxPower (500mA)

	// Interface Descriptor (Keyboard)
	0x09,                         // bLength
	0x04,                         // bDescriptorType (Interface)
	ITF_NUM_HID_KBD,              // bInterfaceNumber
	0x00,                         // bAlternateSetting
	0x01,                         // bNumEndpoints
	0x03,                         // bInterfaceClass (HID)
	0x01,                         // bInterfaceSubClass (Boot)
	0x01,                         // bInterfaceProtocol (Keyboard)
	0x00,                         // iInterface

	// HID Descriptor (Keyboard)
	0x09,                         // bLength
	0x21,                         // bDescriptorType (HID)
	0x11, 0x01,                   // bcdHID 1.11
	0x00,                         // bCountryCode
	0x01,                         // bNumDescriptors
	0x22,                         // bDescriptorType (Report)
	sizeof(hid_report_desc_kbd) & 0xFF,
	(sizeof(hid_report_desc_kbd) >> 8) & 0xFF,

	// Endpoint Descriptor (Keyboard IN)
	0x07,                         // bLength
	0x05,                         // bDescriptorType (Endpoint)
	0x81,                         // bEndpointAddress (IN endpoint 1)
	0x03,                         // bmAttributes (Interrupt)
	0x40, 0x00,                   // wMaxPacketSize (64 bytes)
	0x01,                         // bInterval (1 ms)

	// Interface Descriptor (VIA/SignalRGB Raw HID)
	0x09,                         // bLength
	0x04,                         // bDescriptorType (Interface)
	ITF_NUM_HID_VIA_RAW,          // bInterfaceNumber
	0x00,                         // bAlternateSetting
	0x02,                         // bNumEndpoints (IN + OUT)
	0x03,                         // bInterfaceClass (HID)
	0x00,                         // bInterfaceSubClass (None)
	0x00,                         // bInterfaceProtocol
	0x00,                         // iInterface

	// HID Descriptor (VIA/SignalRGB Raw)
	0x09,                         // bLength
	0x21,                         // bDescriptorType (HID)
	0x11, 0x01,                   // bcdHID 1.11
	0x00,                         // bCountryCode
	0x01,                         // bNumDescriptors
	0x22,                         // bDescriptorType (Report)
	sizeof(hid_report_desc_via_raw) & 0xFF,
	(sizeof(hid_report_desc_via_raw) >> 8) & 0xFF,

	// Endpoint Descriptor (VIA Raw IN)
	0x07,                         // bLength
	0x05,                         // bDescriptorType (Endpoint)
	0x82,                         // bEndpointAddress (IN endpoint 2)
	0x03,                         // bmAttributes (Interrupt)
	0x20, 0x00,                   // wMaxPacketSize (32 bytes)
	0x01,                         // bInterval (1 ms)

	// Endpoint Descriptor (VIA Raw OUT)
	0x07,                         // bLength
	0x05,                         // bDescriptorType (Endpoint)
	0x02,                         // bEndpointAddress (OUT endpoint 2)
	0x03,                         // bmAttributes (Interrupt)
	0x20, 0x00,                   // wMaxPacketSize (32 bytes)
	0x01,                         // bInterval (1 ms)

	// Interface Descriptor (App Raw HID)
	0x09,                         // bLength
	0x04,                         // bDescriptorType (Interface)
	ITF_NUM_HID_APP_RAW,          // bInterfaceNumber
	0x00,                         // bAlternateSetting
	0x02,                         // bNumEndpoints (IN + OUT)
	0x03,                         // bInterfaceClass (HID)
	0x00,                         // bInterfaceSubClass (None)
	0x00,                         // bInterfaceProtocol
	0x00,                         // iInterface

	// HID Descriptor (App Raw)
	0x09,                         // bLength
	0x21,                         // bDescriptorType (HID)
	0x11, 0x01,                   // bcdHID 1.11
	0x00,                         // bCountryCode
	0x01,                         // bNumDescriptors
	0x22,                         // bDescriptorType (Report)
	sizeof(hid_report_desc_raw) & 0xFF,
	(sizeof(hid_report_desc_raw) >> 8) & 0xFF,

	// Endpoint Descriptor (App Raw IN)
	0x07,                         // bLength
	0x05,                         // bDescriptorType (Endpoint)
	0x83,                         // bEndpointAddress (IN endpoint 3)
	0x03,                         // bmAttributes (Interrupt)
	0x40, 0x00,                   // wMaxPacketSize (64 bytes)
	0x01,                         // bInterval (1 ms)

	// Endpoint Descriptor (App Raw OUT)
	0x07,                         // bLength
	0x05,                         // bDescriptorType (Endpoint)
	0x03,                         // bEndpointAddress (OUT endpoint 3)
	0x03,                         // bmAttributes (Interrupt)
	0x40, 0x00,                   // wMaxPacketSize (64 bytes)
	0x01,                         // bInterval (1 ms)

	// Interface Descriptor (Response Raw HID)
	0x09,                         // bLength
	0x04,                         // bDescriptorType (Interface)
	ITF_NUM_HID_RESP_RAW,         // bInterfaceNumber
	0x00,                         // bAlternateSetting
	0x02,                         // bNumEndpoints (IN + OUT)
	0x03,                         // bInterfaceClass (HID)
	0x00,                         // bInterfaceSubClass (None)
	0x00,                         // bInterfaceProtocol
	0x00,                         // iInterface

	// HID Descriptor (Response Raw)
	0x09,                         // bLength
	0x21,                         // bDescriptorType (HID)
	0x11, 0x01,                   // bcdHID 1.11
	0x00,                         // bCountryCode
	0x01,                         // bNumDescriptors
	0x22,                         // bDescriptorType (Report)
	sizeof(hid_report_desc_resp_raw) & 0xFF,
	(sizeof(hid_report_desc_resp_raw) >> 8) & 0xFF,

	// Endpoint Descriptor (Response Raw IN)
	0x07,                         // bLength
	0x05,                         // bDescriptorType (Endpoint)
	0x84,                         // bEndpointAddress (IN endpoint 4)
	0x03,                         // bmAttributes (Interrupt)
	0x40, 0x00,                   // wMaxPacketSize (64 bytes)
	0x01,                         // bInterval (1 ms)

	// Endpoint Descriptor (Response Raw OUT)
	0x07,                         // bLength
	0x05,                         // bDescriptorType (Endpoint)
	0x04,                         // bEndpointAddress (OUT endpoint 4)
	0x03,                         // bmAttributes (Interrupt)
	0x40, 0x00,                   // wMaxPacketSize (64 bytes)
	0x01                          // bInterval (1 ms)
};

// TinyUSB callbacks
uint8_t const * tud_descriptor_device_cb(void)
{
	return (uint8_t const*)&desc_device;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
	(void) index;
	return desc_configuration;
}

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
	switch (instance) {
		case 0: return hid_report_desc_kbd;
		case 1: return hid_report_desc_via_raw;
		case 2: return hid_report_desc_raw;
		case 3: return hid_report_desc_resp_raw;
		default: return hid_report_desc_raw;
	}
}

// String descriptor helper
static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	(void) langid;

	if ( index == 0 ) {
		_desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | 4);
		_desc_str[1] = 0x0409;
		return _desc_str;
	}

	if ( index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0]) ) return NULL;

	const char* str = string_desc_arr[index];

	size_t len = strlen(str);
	if (len > 31) len = 31;

	for (size_t i = 0; i < len; i++) {
		_desc_str[1 + i] = (uint16_t) str[i];
	}
	uint16_t bLength = (uint16_t)(len * 2 + 2);
	_desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | bLength);
	return _desc_str;
}
