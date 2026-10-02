#pragma once
// ===========================================================================
//  config.h -- compile-time configuration for the AI Calculator firmware.
//
//  Everything that is board-, pin- or policy-specific lives here so the rest
//  of the code stays portable. Values can be overridden from the build system
//  with -D<NAME>=<value> on the PlatformIO `build_flags` line.
// ===========================================================================

// ---------------------------------------------------------------------------
//  Firmware identity
// ---------------------------------------------------------------------------
#define FW_NAME                     "AI Calculator"
#define FW_VERSION                  "1.0.0"

// ---------------------------------------------------------------------------
//  Serial console
// ---------------------------------------------------------------------------
#define ENABLE_SERIAL_DEBUG         1

// ---------------------------------------------------------------------------
//  I2C bus -- shared by the OLED and the TCA8418 keypad controller.
//  GPIO8/GPIO9 are the ESP32-C3 default I2C pins (used by the SuperMini).
// ---------------------------------------------------------------------------
#define I2C_SDA_PIN                 8
#define I2C_SCL_PIN                 9
#define I2C_FREQ_HZ                 400000

// ---------------------------------------------------------------------------
//  Display: 1.54" 128x64 monochrome OLED (SSD1309 or SH1106 controller).
// ---------------------------------------------------------------------------
#define DISPLAY_CTRL_SSD1309        1
#define DISPLAY_CTRL_SH1106         2
#ifndef DISPLAY_CONTROLLER
#define DISPLAY_CONTROLLER          DISPLAY_CTRL_SSD1309
#endif

#define DISPLAY_I2C_ADDR            0x3C        // 7-bit; U8g2 wants it shifted
#define DISPLAY_WIDTH               128
#define DISPLAY_HEIGHT              64
#define DISPLAY_TEXT_ROWS           4           // 4-line text helper layout
#define DISPLAY_TEXT_COLS           21          // 21 * 6px glyphs = 126px

// ---------------------------------------------------------------------------
//  Keypad
//  Two interchangeable drivers live behind the same Keypad interface:
//    KEYPAD_DRIVER_TCA8418 : I2C keypad controller (prototype default)
//    KEYPAD_DRIVER_GPIO    : direct row/column GPIO scan
// ---------------------------------------------------------------------------
#define KEYPAD_DRIVER_TCA8418       1
#define KEYPAD_DRIVER_GPIO          2
#ifndef KEYPAD_DRIVER
#define KEYPAD_DRIVER               KEYPAD_DRIVER_TCA8418
#endif

#define TCA8418_I2C_ADDR            0x34

// Physical keypad geometry (Casio FX-300MS membrane = 8 rows x 7 columns).
#define MATRIX_ROWS                 8
#define MATRIX_COLS                 7
#define MATRIX_KEYS                 (MATRIX_ROWS * MATRIX_COLS)   // 56 slots

// TCA8418 internally numbers keys as (row * 10 + col + 1), i.e. it always
// assumes the full 10-column scan order even when only 7 are wired.
#define TCA8418_STRIDE             10

// Direct-GPIO driver pin assignments. NOTE: this mode is mutually exclusive
// with the I2C display because it consumes almost every ESP32-C3 pin. Kept
// here so the abstraction can be exercised on a bigger board / breadboard.
#define KEYPAD_ROW_PINS             { 4, 5, 6, 7, 10, 20, 21, 3 }
#define KEYPAD_COL_PINS             { 0, 1, 2, 18, 19, 8, 9 }

// Keypad timing.
#define KEYPAD_SCAN_INTERVAL_MS     4           // internal scan cadence
#define KEYPAD_DEBOUNCE_MS          12          // contact debounce
#define KEYPAD_REPEAT_MS            120         // auto-repeat period
#define KEYPAD_LONG_MS              600         // long-press threshold
#define KEYPAD_QUEUE_LEN            16          // buffered raw key events

// Multi-tap text entry.
#define MULTITAP_TIMEOUT_MS         800         // inactivity commits a glyph

// ---------------------------------------------------------------------------
//  Battery monitor (2x 100k divider -> half the pack voltage on the ADC pin)
// ---------------------------------------------------------------------------
#define BATTERY_ADC_PIN             0
#define BATTERY_DIVIDER             2.0f
#define BATTERY_FULL_MV             4200
#define BATTERY_EMPTY_MV            3200
#define BATTERY_SAMPLE_MS           5000

// ---------------------------------------------------------------------------
//  Fixed buffer sizes (no dynamic allocation on any hot path)
// ---------------------------------------------------------------------------
#define EVENT_TEXT_LEN              72          // Event::text[] capacity
#define TEXT_INPUT_CAP              64          // multi-tap line editor
#define EXPR_CAP                    96          // calculator expression
#define RESULT_CAP                  48          // formatted result
#define REPLY_CAP                   2048        // accumulated AI reply
#define CALC_HISTORY_LEN            8           // stored expression lines
#define CALC_HISTORY_CHARS          48
#define SSE_LINE_CAP                1024        // one SSE data line
#define HTTP_BODY_CAP               1024        // serialized request JSON
#define HTTP_READ_CHUNK             512         // TLS read granularity

// ---------------------------------------------------------------------------
//  FreeRTOS tasks and IPC queues
//  NOTE: the ESP32-C3 is single core. On dual-core parts netTask is pinned to
//  core 1; on the C3 it shares core 0 with tskNO_AFFINITY.
// ---------------------------------------------------------------------------
#define UI_TASK_STACK_BYTES         8192
#define NET_TASK_STACK_BYTES        16384
#define UI_TASK_PRIORITY            2
#define NET_TASK_PRIORITY           1
#define UI_QUEUE_LEN                16
#define NET_QUEUE_LEN               8
#define UI_TICK_MS                  5           // uiTask loop period

// ---------------------------------------------------------------------------
//  WiFi
// ---------------------------------------------------------------------------
#define WIFI_HOSTNAME               "ai-calc"
#define WIFI_CONNECT_TIMEOUT_MS     12000       // per attempt
#define WIFI_RETRY_MIN_MS           2000        // first backoff
#define WIFI_RETRY_MAX_MS           30000       // backoff ceiling
#define WIFI_NVS_NAMESPACE          "wifi"
#define WIFI_MAX_SSID               32
#define WIFI_MAX_PASS               64

// ---------------------------------------------------------------------------
//  Time (SNTP must succeed before TLS or the 1970 clock breaks cert checks)
// ---------------------------------------------------------------------------
#define NTP_SERVER_1                "pool.ntp.org"
#define NTP_SERVER_2                "time.google.com"
#define TZ_STRING                   "UTC0"
#define SNTP_TIMEOUT_MS             10000
#define SNTP_MIN_VALID_EPOCH        1600000000UL   // ~Sep 2020

// ---------------------------------------------------------------------------
//  DeepSeek API (verified 2026-09-26)
// ---------------------------------------------------------------------------
#define DEEPSEEK_HOST               "api.deepseek.com"
#define DEEPSEEK_PORT               443
#define DEEPSEEK_PATH               "/chat/completions"
#define DEEPSEEK_MODEL              "deepseek-flash"   // chat/reasoner retired
#define DEEPSEEK_MAX_TOKENS         64
#define DEEPSEEK_TEMPERATURE        0
#define DEEPSEEK_HTTP_TIMEOUT_MS    20000
#define DEEPSEEK_STREAM_STALL_MS    12000       // abort if no bytes for this long
#define DEEPSEEK_SYSTEM_PROMPT \
    "You are the AI assistant built into a pocket calculator. " \
    "Reply in plain text, under 40 words, no markdown and no code fences."

// ---------------------------------------------------------------------------
//  TLS policy
//   TLS_MODE_PINNED_CA : verify against DEEPSEEK_ROOT_CA_PEM (default,
//                        the Amazon Root CA 1 that signs *.deepseek.com)
//   TLS_MODE_BUNDLE    : verify against an embedded Mozilla CA bundle
//                        (requires board_build.embed_files, see README)
//   ALLOW_INSECURE_TLS : bring-up escape hatch, set to 1 to call setInsecure()
// ---------------------------------------------------------------------------
#define TLS_MODE_PINNED_CA          1
#define TLS_MODE_BUNDLE             2
#ifndef TLS_MODE
#define TLS_MODE                    TLS_MODE_PINNED_CA
#endif
#ifndef ALLOW_INSECURE_TLS
#define ALLOW_INSECURE_TLS          0
#endif

// ---------------------------------------------------------------------------
//  UI colours / behaviour constants
// ---------------------------------------------------------------------------
#define UI_STATUS_BAR_H             11          // status bar pixel height
#define UI_MESSAGE_MS               2500        // transient toast duration
