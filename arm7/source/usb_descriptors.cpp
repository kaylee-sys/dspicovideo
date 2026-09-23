#include <nds/ndstypes.h>
#include <cstring>
#include "common.h"

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_NONE
#endif

#ifndef CFG_TUD_ENABLED
#define CFG_TUD_ENABLED 1
#endif

#ifndef CFG_TUD_VENDOR
#define CFG_TUD_VENDOR 1
#endif

#ifndef TUP_DCD_ENDPOINT_MAX
#define TUP_DCD_ENDPOINT_MAX 8
#endif

extern "C" {
#include "tusb_config.h"

#undef CFG_TUD_ENABLED
#define CFG_TUD_ENABLED 1

#include "tusb.h"
#include "class/vendor/vendor_device.h"
}

#include "usb_descriptors.h"

#ifndef STRID_LANGID
#define STRID_LANGID            0
#define STRID_MANUFACTURER      1
#define STRID_PRODUCT           2
#define STRID_SERIAL            3
#define STRID_AUDIO_INTERFACE   4
#endif

#define USB_PID 0x4001

#define TU_U16_LOW(u16)  (uint8_t)((u16) & 0x00FF)
#define TU_U16_HIGH(u16) (uint8_t)(((u16) >> 8) & 0x00FF)

extern "C" const u8* tud_descriptor_device_cb(void)
{
    static const tusb_desc_device_t deviceDescriptor =
    {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB             = 0x0200,

        .bDeviceClass       = TUSB_CLASS_VENDOR_SPECIFIC,
        .bDeviceSubClass    = 0x00,
        .bDeviceProtocol    = 0x00,
        .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

        .idVendor           = 0xCafe,
        .idProduct          = USB_PID,
        .bcdDevice          = 0x0100,

        .iManufacturer      = STRID_MANUFACTURER,
        .iProduct           = STRID_PRODUCT,
        .iSerialNumber      = STRID_SERIAL,

        .bNumConfigurations = 0x01
    };

    return (const u8*)&deviceDescriptor;
}

#define EPNUM_VENDOR_OUT    0x01
#define EPNUM_VENDOR_IN     0x81

// Длина: 9 байт (Config Desc) + 23 байта (Vendor Desc: 9 + 7 + 7) = 32 байта
#define CONFIG_TOTAL_LEN    (9 + 9 + 7 + 7)

extern "C" const u8* tud_descriptor_configuration_cb(u8 index)
{
    (void)index;

    static const u8 configurationDescriptor[] =
    {
        // 1. Config Descriptor
        9, TUSB_DESC_CONFIGURATION, TU_U16_LOW(CONFIG_TOTAL_LEN), TU_U16_HIGH(CONFIG_TOTAL_LEN), 1, 1, 0, 0x00, 50,

        // 2. Vendor Interface Descriptor
        9, TUSB_DESC_INTERFACE, 0, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, STRID_AUDIO_INTERFACE,

        // 3. Endpoint OUT Descriptor
        7, TUSB_DESC_ENDPOINT, EPNUM_VENDOR_OUT, TUSB_XFER_BULK, TU_U16_LOW(64), TU_U16_HIGH(64), 0,

        // 4. Endpoint IN Descriptor
        7, TUSB_DESC_ENDPOINT, EPNUM_VENDOR_IN, TUSB_XFER_BULK, TU_U16_LOW(64), TU_U16_HIGH(64), 0
    };

    return configurationDescriptor;
}

static u16 _desc_str[32];

extern "C" const u16* tud_descriptor_string_cb(u8 index, u16 langid)
{
    (void)langid;

    static const char* string_arr[] = {
        (const char[]){ 0x09, 0x04 }, // 0: English (0x0409)
        "DSpico",                      // 1: Manufacturer
        "DSpico Video Streamer",       // 2: Product
        "123456789",                   // 3: Serial
        "DSpico Video Interface"       // 4: Interface
    };

    if (index >= sizeof(string_arr) / sizeof(string_arr[0])) {
        return nullptr;
    }

    if (index == 0) {
        memcpy(&_desc_str[1], string_arr[0], 2);
        _desc_str[0] = (TUSB_DESC_STRING << 8) | 4;
        return _desc_str;
    }

    const char* str = string_arr[index];
    u8 len = strlen(str);
    if (len > 31) len = 31;

    for (u8 i = 0; i < len; i++) {
        _desc_str[1 + i] = str[i];
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);
    return _desc_str;
}
