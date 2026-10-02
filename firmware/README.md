# AI Calculator — ESP32-C3 firmware

A pocket scientific calculator with a built-in DeepSeek assistant, designed to
fit inside a Casio FX‑300MS Plus shell. The MX‑style keypad drives a local
shunting‑yard calculator, and a dedicated key streams an answer from the
DeepSeek API over HTTPS.

---

## 1. Hardware target

| Block      | Part                                         | Notes                                            |
| ---------- | -------------------------------------------- | ------------------------------------------------ |
| MCU        | ESP32‑C3 SuperMini (proto) / ESP32‑C3‑MINI‑1 | 2.4 GHz WiFi, single RISC‑V core                 |
| Display    | 1.54" 128×64 OLED, SSD1309 or SH1106, I²C    | addr `0x3C`, U8g2 full‑buffer mode               |
| Keypad     | Casio FX‑300MS membrane, 8×7 matrix          | 56 positions / 50 keys                           |
| Keypad ctrl| TCA8418 I²C controller (addr `0x34`)         | protoboard default, or direct GPIO scan          |
| Power      | 3.3 V buck‑boost                             | battery sensed on an ADC pin via 2×100 k divider |

### Pin map

| Signal        | GPIO | Notes                                             |
| ------------- | ---- | ------------------------------------------------- |
| I²C SDA       | 8    | shared by OLED + TCA8418 (C3 default)             |
| I²C SCL       | 9    | shared by OLED + TCA8418                          |
| Battery ADC   | 0    | 2×100 k divider, pack voltage = 2 × pin voltage   |

The direct‑GPIO keypad driver in `include/config.h`
(`KEYPAD_ROW_PINS`/`KEYPAD_COL_PINS`) is only selectable on a host board with
enough free pins — it is **mutually exclusive** with the I²C display because it
needs 15 GPIOs. The TCA8418 path is the one to use in the finished device.

---

## 2. Repository layout

```
firmware/
├── platformio.ini          # esp32-c3 + native test environments
├── partitions.csv          # 3 MiB app slot / NVS / otadata / spiffs
├── README.md
├── include/
│   ├── config.h            # every pin, buffer size, timeout and policy flag
│   ├── secrets.example.h   # template for secrets.h
│   ├── secrets.h           # local credentials (git-ignored)
│   └── deepseek_roots.h    # pinned Amazon Root CA 1 for api.deepseek.com
├── src/
│   ├── main.cpp            # setup(): init + start tasks. loop() is idle.
│   ├── app/
│   │   ├── events.h        # POD Event + EventType (inline char text[72])
│   │   ├── app.h/.cpp      # composition root, uiTask + netTask, queues
│   │   └── ui_fsm.h/.cpp   # BOOT/CALC/TEXT_INPUT/AI_RESPONSE/WIFI_CONFIG/ERROR
│   ├── hal/
│   │   ├── keypad.h/.cpp   # abstract keypad + TCA8418 + GPIO drivers
│   │   ├── keymap.h/.cpp   # key index -> Action, multi-tap char table
│   │   └── display.h/.cpp  # U8g2 wrapper + 4-line text / status bar
│   ├── services/
│   │   ├── wifi_manager.h/.cpp    # non-blocking connect/backoff/reconnect/NVS
│   │   ├── time_service.h/.cpp    # SNTP with timeout
│   │   └── deepseek_client.h/.cpp # HTTPS streaming SSE chat client
│   ├── ui/
│   │   ├── text_buffer.h/.cpp     # fixed line editor + multi-tap
│   │   └── renderer.h/.cpp        # wrapping / layout helpers
│   ├── calc/
│   │   ├── expr.h/.cpp            # shunting-yard evaluator (pure C++)
│   │   └── calculator.h/.cpp      # expression state + history (pure C++)
│   └── util/
│       ├── ring_buffer.h          # fixed-capacity ring template
│       └── sse_parser.h/.cpp      # incremental SSE data-line extractor
└── test/
    └── test_expr.cpp       # native Unity tests for the calculator engine
```

Layering is strict and one-directional:

```
HAL (keypad/display) -> services (wifi/time/deepseek) -> ui (text/renderer)
                                                              -> app (fsm/tasks)
```

---

## 3. Build

```bash
cd firmware

# 1. Provide credentials
cp include/secrets.example.h include/secrets.h
$EDITOR include/secrets.h         # WiFi + DeepSeek key

# 2. Firmware
pio run                            # build
pio run -t upload                  # flash (ESP32-C3 SuperMini)
pio device monitor                 # 115200 baud

# 3. Calculator unit tests on the host
pio test -e native
```

`platformio.ini` sets `board_build.partitions = partitions.csv`, so the app has
a 3 MiB slot — ample for WiFi + mbedTLS + U8g2 + ArduinoJson.

> **Toolchain note:** PlatformIO is not required to be pre-installed. On a fresh
> machine run `pip install platformio` first.

---

## 4. Credentials

`include/secrets.h` holds the WiFi SSID/password and the DeepSeek API key. It is
listed in `firmware/.gitignore`; never commit it.

```c
#define WIFI_SSID        "YourPhoneHotspotName"
#define WIFI_PASS        "simplepassword"
#define DEEPSEEK_API_KEY "sk-..."
```

WiFi credentials come from one of two places:

1. **NVS** — set on the device through the **WIFI_CONFIG** screen (press `MODE`
   while offline). Stored in `Preferences`, namespace `wifi`; takes precedence.
2. **`secrets.h`** — the compiled `WIFI_SSID`/`WIFI_PASS` macros are used as a
   boot-time fallback when no credentials are stored in NVS (the example
   `YourPhoneHotspotName` placeholder is ignored). Handy for baking in a hotspot
   and avoiding multi-tap password entry.

Only `DEEPSEEK_API_KEY` is required from `secrets.h` for the API itself.

---

## 5. TLS modes

`include/config.h` selects how the HTTPS connection is verified:

| Mode                            | What it does                                                        |
| ------------------------------- | ------------------------------------------------------------------- |
| `TLS_MODE_PINNED_CA` *(default)*| `setCACert(DEEPSEEK_ROOT_CA_PEM)` — Amazon Root CA 1 (the CA behind `*.deepseek.com`) |
| `TLS_MODE_BUNDLE`               | `setCACertBundle(...)` against an embedded Mozilla bundle           |
| `ALLOW_INSECURE_TLS = 1`        | `setInsecure()` — **bring-up only, never ship**                     |

The default anchor was verified against the live endpoint on 2026‑09‑26:

```
leaf : CN=*.deepseek.com
CA   : CN=Amazon RSA 2048 M01     (sent by the server)
root : CN=Amazon Root CA 1        (pinned; valid to 2038-01-17)
```

To switch to the bundle, generate `data/cert/x509_crt_bundle.bin` with ESP‑IDF's
`gen_crt_bundle.py`, uncomment `board_build.embed_files` in `platformio.ini`, and
set `TLS_MODE = TLS_MODE_BUNDLE` in `config.h`.

**SNTP is mandatory before TLS.** The client only runs after
`TimeService::ensureSynced()` succeeds, otherwise the ESP32's 1970 clock makes
every certificate look "not yet valid".

---

## 6. DeepSeek API

```http
POST https://api.deepseek.com/chat/completions
Content-Type: application/json
Authorization: Bearer <API_KEY>
```

Request body built with ArduinoJson v7:

```json
{
  "model": "deepseek-flash",
  "temperature": 0,
  "max_tokens": 64,
  "stream": true,
  "thinking": { "type": "disabled" },
  "messages": [ {"role":"system","content":"..."},
                {"role":"user","content":"..."} ]
}
```

The response is Server‑Sent Events. `SseParser` handles the framing:

* ignores blank lines and `: keep-alive` comments,
* emits only `data:` payloads,
* detects the `data: [DONE]` sentinel.

Because DeepSeek streams with `Transfer-Encoding: chunked` and Arduino's
`HTTPClient::getStreamPtr()` does **not** de‑chunk, `deepseek_client.cpp`
contains a small `BodyReader` that transparently decodes chunk framing before
handing bytes to the SSE parser.

Only `choices[0].delta.content` is kept: the JSON parse uses an ArduinoJson
deserialization filter so nothing else is ever allocated.

Robustness covered:

* HTTP 401 / 402 / 429 / 5xx → surfaced to the UI, partial reply retained,
* `: keep-alive` lines skipped,
* stalled reads aborted after `DEEPSEEK_STREAM_STALL_MS`,
* mid‑stream cancel through a `volatile bool` flag (`AC` on the AI screen).

---

## 7. Concurrency model

Two FreeRTOS tasks communicate **only** through queues of POD `Event` structs
(each carries an inline `char text[72]`, so a queue send is a single memcpy and
never touches the heap):

| Task      | Core                          | Owns                                        | Stack   |
| --------- | ----------------------------- | ------------------------------------------- | ------- |
| `uiTask`  | 0                             | keypad, display, calculator, FSM            | 8192 B  |
| `netTask` | 1 (dual-core) / shared on C3  | WiFi, SNTP, DeepSeek client                 | 16384 B |

> The ESP32‑C3 is **single core**. `app.cpp` detects `CONFIG_FREERTOS_UNICORE`
> and creates `netTask` with `tskNO_AFFINITY` (sharing core 0) instead of pinning
> to core 1; on a dual‑core part it is pinned to core 1. No blocking I/O ever
> runs in `uiTask`.

Fixed buffers are used on every hot path: no `String`, no `http.getString()` on
the chat reply. The reply accumulates in a 2048‑byte buffer, one SSE line at a
time into `char line[1024]`. After each AI turn the firmware logs
`ESP.getFreeHeap()` and `heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)`.

---

## 8. UI state machine

```
            ┌──────┐  boot delay   ┌──────┐
            │ BOOT │ ────────────► │ CALC │ ◄────────────────────┐
            └──────┘               └──┬───┘                      │
                                      │ MODE                     │ MODE
                                      ▼                          │
                                 ┌────────────┐  ENTER/AI   ┌────┴─────────┐
                                 │ TEXT_INPUT │ ──────────► │ AI_RESPONSE  │
                                 └────────────┘             └──────────────┘
                                      ▲   ▲                    AI=follow-up
                                      │   │ WIFI_CONFIG (no creds / failed)
                                      └───┴───────────────────────────────
```

* **CALC** — digits/operators build an expression; `=` evaluates locally;
  `AI` sends the expression to DeepSeek; `MODE` opens text entry.
* **TEXT_INPUT** — multi‑tap entry (`2=abc … 9=wxyz`, `1` punctuation,
  `0` space). A glyph commits after 800 ms of inactivity. `SHIFT` toggles case.
  `ENTER` (the `=` key) or `AI` sends.
* **AI_RESPONSE** — streamed tokens append live; `UP`/`DOWN` scroll; `AC`
  cancels; `AI` starts a follow‑up.
* **WIFI_CONFIG** — enter SSID and password with the same multi‑tap engine,
  `ENTER` moves between fields and then connects. Reached by pressing `MODE`
  while offline, or automatically when an AI query is attempted with no
  connection. On success the UI returns to CALC.
* **ERROR** — transient failures render here; any key returns to CALC.

If there are no stored credentials at boot, or the connection keeps failing, the
net task reports the state and the UI can switch to WIFI_CONFIG.

---

## 9. Keypad

`hal/keypad.h` defines an abstract `Keypad` whose base class implements
**debounce (12 ms)**, **auto‑repeat (120 ms)** and **long‑press (600 ms)** on top
of a plain “currently pressed” bitmask. Two drivers implement the bitmask:

* `Tca8418Keypad` — reads the TCA8418 event FIFO, tracks press/release and maps
  `key = (row*10 + col + 1)` to the physical 8×7 index.
* `GpioKeypad` — drives rows low and reads pulled‑up columns.

`hal/keymap.cpp` maps index → `Action`. **The matrix table is a placeholder**:
buzz out the real FX‑300MS membrane ribbon and edit `kMatrix[]` until each
physical key reports the right `Action`. Nothing else needs to change.

---

## 10. Display

`hal/display.cpp` instantiates U8g2 in full‑buffer mode
(`U8G2_SSD1309_128X64_NONAME0_F_HW_I2C`, 1024‑byte buffer). To use an SH1106
panel set `DISPLAY_CONTROLLER = DISPLAY_CTRL_SH1106` in `config.h`. I²C address
defaults to `0x3C` (`0x3D` is a common alternative — change
`DISPLAY_I2C_ADDR`).

---

## 11. Testing

`test/test_expr.cpp` is a Unity suite covering operator precedence, right‑
associative `^`, unary minus edge cases (`-2^2 == -4`, `2^-2 == 0.25`),
functions, constants, domain errors, unbalanced parentheses, and the
`Calculator` state machine.

```bash
pio test -e native
```

`[env:native]` compiles only `src/calc/**`, so no Arduino headers are needed.

---

## 12. Build caveats

* **Single-core C3:** “Core 0 / Core 1” becomes “core 0 / no affinity” on the
  C3 (see §7). Both tasks still run preemptively; the split is logical.
* **Direct-GPIO keypad** cannot coexist with the I²C display on an ESP32‑C3
  (not enough pins). It exists for breadboard experiments on larger boards.
* **TLS default is pinned to Amazon Root CA 1.** If DeepSeek rotates to a CA
  outside Amazon Trust Services, validation will fail — replace
  `include/deepseek_roots.h`, or switch to `TLS_MODE_BUNDLE`, or (temporarily)
  set `ALLOW_INSECURE_TLS = 1`.
* **`setCACertBundle` differs between Arduino cores** (2.x: one argument;
  3.3+: `useBuiltinCACertBundle()`). The bundled path here targets the classic
  one‑argument API used with `board_build.embed_files`; update
  `configureTls()` in `deepseek_client.cpp` if you move to a newer core.
* **U8g2 constructor names** depend on the library version; the SSD1309/SH1106
  full‑buffer constructors used here exist in U8g2 ≥ 2.35.
* The TCA8418 register map and key‑code formula (`row*10 + col + 1`) follow the
  vendor datasheet; if your wiring differs, adjust `Tca8418Keypad::readMatrix`.
* `secrets.h` is git-ignored and is **not** part of a fresh clone; copy
  `secrets.example.h` to `secrets.h` (see §4) before building. It will not
  connect until you fill in your own values (or use WIFI_CONFIG on device).
* Host tests need a working C++ standard library and toolchain; if `pio test -e
  native` cannot find standard headers like `<cstdio>`, install or repair your
  platform's C++ toolchain (e.g. Xcode Command Line Tools on macOS,
  `build-essential` on Debian/Ubuntu).

---

## 13. VS Code IntelliSense

If VS Code shows **`#include errors detected. Please update your includePath`** (from the
Microsoft C/C++ extension), it is an **IntelliSense** problem, not a build problem — the code
still compiles with `pio run`. IntelliSense simply needs PlatformIO's include paths, which live
in its compile database:

```bash
cd firmware
pio run -t compiledb        # writes firmware/compile_commands.json
# or: python3 -m platformio run -t compiledb
```

The checked-in [`.vscode/c_cpp_properties.json`](../.vscode/c_cpp_properties.json) already
points at that file, so once it exists the red squiggles clear (reload the window if not).
Re-run `compiledb` whenever you change `platformio.ini` — new libraries, flags, or envs change
the paths. `compile_commands.json` is **git-ignored**, because it contains machine-specific
absolute paths.

Recommended extensions are listed in [`.vscode/extensions.json`](../.vscode/extensions.json):
the **PlatformIO IDE** extension installs its own IntelliSense integration and Build/Upload/Test
buttons, which is the smoothest path.
