#include "tusb.h"
#include "pico/unique_id.h"

// USB identity - all overridable via -DUSBNET_USB_* compile definitions
#ifndef USBNET_USB_VID
#define USBNET_USB_VID 0xCafe
#endif
#ifndef USBNET_USB_MANUFACTURER
#define USBNET_USB_MANUFACTURER "Pico USBNET"
#endif
#ifndef USBNET_USB_PRODUCT
#define USBNET_USB_PRODUCT "Pico USB Ethernet"
#endif
#ifndef USBNET_USB_INTERFACE
#define USBNET_USB_INTERFACE "USB Network Interface"
#endif

/* Product ID layout: NET interface */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(NCM, 5))

enum
{
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_INTERFACE,
  STRID_MAC
};

enum
{
  ITF_NUM_NET = 0,
  ITF_NUM_TOTAL = 2
};

enum
{
  CONFIG_ID_NCM   = 0,
  CONFIG_ID_COUNT
};

// MAC address - set by usbnet_init() (board unique ID or consumer-supplied)
// before enumeration. Placeholder is locally-administered, unicast.
uint8_t tud_network_mac_address[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00};

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
#if CFG_TUD_NCM
    .bcdUSB             = 0x0201,
#else
    .bcdUSB             = 0x0200,
#endif
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USBNET_USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = CONFIG_ID_COUNT
};

// Return the USB device descriptor to the host during enumeration
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
#define NCM_CONFIG_TOTAL_LEN     (TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN)

#define EPNUM_NET_NOTIF   0x81
#define EPNUM_NET_OUT     0x02
#define EPNUM_NET_IN      0x82

static uint8_t const ncm_configuration[] =
{
  TUD_CONFIG_DESCRIPTOR(CONFIG_ID_NCM+1, ITF_NUM_TOTAL, 0, NCM_CONFIG_TOTAL_LEN, 0, 100),
  TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_NET, STRID_INTERFACE, STRID_MAC, EPNUM_NET_NOTIF, 64, EPNUM_NET_OUT, EPNUM_NET_IN, 64, 1514, 16, 0x00),
};

static uint8_t const * const configuration_arr[1] =
{
  [CONFIG_ID_NCM] = ncm_configuration
};

// Return the requested USB configuration descriptor (or NULL if the index is
// out of range)
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  return (index < CONFIG_ID_COUNT) ? configuration_arr[index] : NULL;
}

//--------------------------------------------------------------------+
// BOS Descriptor (required for Windows NCM auto-driver loading)
//--------------------------------------------------------------------+
#if CFG_TUD_NCM

/* Microsoft OS 2.0 registry property descriptor to associate NCM interface
 * with the WINNCM driver on Windows 10+. Without this, manual driver install is needed. */

#define BOS_TOTAL_LEN      (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)
#define MS_OS_20_DESC_LEN  0xB2

uint8_t const desc_bos[] =
{
  TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),
  TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, 1)
};

// Return the Binary Object Store descriptor (used by Windows to auto-load the
// NCM driver)
uint8_t const * tud_descriptor_bos_cb(void)
{
  return desc_bos;
}

uint8_t const desc_ms_os_20[] =
{
  // Set header: length, type, windows version, total length
  U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

  // Configuration subset header: length, type, configuration index, reserved, configuration total length
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A),

  // Function Subset header: length, type, first interface, reserved, subset length
  U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_NET, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A-0x08),

  // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
  U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'N', 'C', 'M', 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  // MS OS 2.0 Registry property descriptor: length, type
  U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A-0x08-0x08-0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
  U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
  'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
  'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
  U16_TO_U8S_LE(0x0050), // wPropertyDataLength
  // bPropertyData: DeviceInterfaceGUIDs
  '{', 0x00, '1', 0x00, '2', 0x00, '3', 0x00, '4', 0x00, '5', 0x00, '6', 0x00, '7', 0x00, '8', 0x00, '-', 0x00,
  '0', 0x00, 'D', 0x00, '0', 0x00, '8', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
  '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '-', 0x00, '1', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
  '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'F', 0x00, '9', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect MS OS 2.0 descriptor size");

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
  // nothing to do with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  switch (request->bmRequestType_bit.type) {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest) {
        case 1:
          if (request->wIndex == 7) {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20 + 8, 2);
            return tud_control_xfer(rhport, request, (void*)(uintptr_t)desc_ms_os_20, total_len);
          } else {
            return false;
          }
        default: break;
      }
      break;
    default: break;
  }

  // stall unknown request
  return false;
}

#endif // CFG_TUD_NCM

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

static char const* string_desc_arr [] =
{
  [STRID_LANGID]       = (const char[]) { 0x09, 0x04 },
  [STRID_MANUFACTURER] = USBNET_USB_MANUFACTURER,
  [STRID_PRODUCT]      = USBNET_USB_PRODUCT,
  [STRID_SERIAL]       = NULL,
  [STRID_INTERFACE]    = USBNET_USB_INTERFACE
};

static uint16_t _desc_str[32 + 1];

// Build and return the requested USB string descriptor (language ID, serial
// number from the board unique ID, MAC address, or one of the static strings)
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  unsigned int chr_count = 0;

  switch ( index ) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL: {
      /* Use RP2040 unique ID as serial number */
      pico_unique_board_id_t uid;
      pico_get_unique_board_id(&uid);
      for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES && chr_count < 31; i++) {
        char c = "0123456789ABCDEF"[(uid.id[i] >> 4) & 0xf];
        _desc_str[1 + chr_count++] = c;
        c = "0123456789ABCDEF"[(uid.id[i] >> 0) & 0xf];
        _desc_str[1 + chr_count++] = c;
      }
      break;
    }

    case STRID_MAC:
      for (unsigned i=0; i<sizeof(tud_network_mac_address); i++) {
        _desc_str[1+chr_count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 4) & 0xf];
        _desc_str[1+chr_count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 0) & 0xf];
      }
      break;

    default:
      if ( !(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) ) return NULL;

      const char *str = string_desc_arr[index];
      chr_count = strlen(str);
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
      if ( chr_count > max_count ) chr_count = max_count;

      for ( size_t i = 0; i < chr_count; i++ ) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8 ) | (2*chr_count + 2));
  return _desc_str;
}
