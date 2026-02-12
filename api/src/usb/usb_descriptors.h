/* USB descriptor helper definitions for HID composite example */

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

enum {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_RAW = 2,
    REPORT_ID_MOUSE,
    REPORT_ID_CONSUMER_CONTROL,
    REPORT_ID_GAMEPAD,
    REPORT_ID_COUNT
};

// HID interface numbers (4-interface layout: matches Shego)
enum {
    ITF_NUM_HID_KBD = 0,
    ITF_NUM_HID_VIA_RAW,
    ITF_NUM_HID_APP_RAW,
    ITF_NUM_HID_RESP_RAW,
};

#endif /* USB_DESCRIPTORS_H_ */

