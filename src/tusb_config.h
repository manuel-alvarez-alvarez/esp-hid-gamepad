#pragma once

// Device stack enabled, full speed
#define CFG_TUD_ENABLED             1
#define CFG_TUD_MAX_SPEED           OPT_MODE_FULL_SPEED

// OS integration
#define CFG_TUSB_OS                 OPT_OS_FREERTOS
#define CFG_TUSB_DEBUG              0

/* rhport 0 = USB OTG, full-speed device */
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

// Control EP0 = 64 is the Full Speed standard
#define CFG_TUD_ENDPOINT0_SIZE      64

// Enable only what you use
#define CFG_TUD_CDC     1
#define CFG_TUD_MSC     0
#define CFG_TUD_HID     1
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

// CDC buffer sizes
#define CFG_TUD_CDC_RX_BUFSIZE      64
#define CFG_TUD_CDC_TX_BUFSIZE      64

// HID: use full 64B endpoint buffer (FS max packet)
#define CFG_TUD_HID_EP_BUFSIZE      64

