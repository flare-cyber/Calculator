# Software Reference — Firmware Architecture

Firmware lives in [`../firmware/`](../firmware/) as a PlatformIO (Arduino-ESP32) project.
This document is the **contract** between the firmware and the rest of the docs: the GPIO map
is [HARDWARE.md §6](HARDWARE.md), the wiring is [WIRING.md](WIRING.md), the phases are
[PLAN.md](PLAN.md). If code and doc disagree, fix both in one commit.

## 1. Design constraints that shape the code

| Constraint | Consequence |
|---|---|
| ESP32-C3: single RISC-V core, ~400 KB SRAM, **no PSRAM** (MINI-1-H4) | TLS buffers are the budget. Hard allocation rules in §8. |
| 128×64 monochrome display, ~21 × 6 text cells (6×10 font) | Answers must be short — enforced by `max_tokens: 64` and a terse system prompt (§7) |
| 200 mAh cell | Aggressive sleep ladder (§6); network is duty-cycled, session reused where safe |
| No RTC battery; boots with invalid clock | **SNTP before every TLS attempt** (§7.5 TLS row) or cert validation fails |
| One button press at a time is the interaction model | Keypad read is event-queue driven, no rollover logic needed |
| Deep-sleep kill: all RAM/state lost | UI resume semantics are intentionally "cold" (§6.3) |

## 2. Task & queue architecture (FreeRTOS)

Two application tasks (on the single-core ESP32-C3 they share core 0; on a dual-core part
`netTask` is pinned to core 1):

```text
   uiTask (prio 2, stack 8 KB)                    netTask (prio 1, stack 16 KB)
   ───────────────────────────                    ────────────────────────────────
   • scans the keypad (polled, 4 ms)              • WiFi connect / backoff / reconnect
   • runs the UI state machine                    • SNTP sync (before every TLS attempt)
   • renders the OLED (owns I²C bus)              • HTTPS POST + SSE stream parse
   • battery ADC sampling                         • DeepSeek client, retry bookkeeping
          │                                               ▲
          │   netQueue_ (Event, depth 8)                  │
          └───────────────────────────────────────────────┘
                      uiQueue_ (Event, depth 16)
                      (NetTask → UiTask: tokens, status, wifi state)
```

**Ownership rules (the whole concurrency design):**

1. **uiTask** is the only task that touches the display, the I²C bus, and the state machine.
2. **netTask** is the only task that touches WiFi/TLS/HTTP and the `api.deepseek.com` socket.
3. Communication is **only** through two FreeRTOS queues of POD `Event` structs (an inline
   `char text[72]`, so a queue send is a single `memcpy` — no heap, no shared pointers):
   - `netQueue_` (depth 8) — uiTask → netTask: `AiStart`, `WifiSetSsid`, `WifiSetPass`,
     `WifiConnect`, `WifiForget`, `AiCancel`.
   - `uiQueue_` (depth 16) — netTask → uiTask: `AiToken`, `AiDone`, `AiError`, `WifiState`,
     `TimeSynced`, `Status`.
4. The only shared object is the `aiCancel_` flag (`volatile bool`) used to abort a stream;
   see RISKS.md R-12 for its known limitation.

Rationale: keeping I²C out of netTask prevents interleaved bus transactions mid-display-update,
and keeping heap growth inside netTask localizes fragmentation to one task (see §8).

> ⚠ The keypad is **polled** (every ~4 ms from uiTask), not interrupt-driven. There is no
> TCA8418 `/INT` wiring in the current firmware and no sleep module. This is a deliberate
> simplification for the first cut; interrupt wake is a documented target (PLAN.md, RISKS.md
> R-11).

## 3. Repository / file-by-file map

```text
firmware/
├── platformio.ini              build environments (see §3.1)
├── partitions.csv
├── include/
│   ├── config.h                pins, sizes, timeouts, feature flags
│   ├── secrets.example.h       committed template (WIFI_SSID / WIFI_PASS / DEEPSEEK_API_KEY)
│   ├── secrets.h               local copy — GIT-IGNORED
│   └── deepseek_roots.h        pinned Amazon Root CA 1 PEM
├── src/
│   ├── main.cpp                boot, task spawn
│   ├── app/
│   │   ├── events.h            Event POD + EventType + factory helpers
│   │   ├── app.h / app.cpp     composition root; owns modules + both tasks
│   │   └── ui_fsm.h / .cpp     UI state machine (§5)
│   ├── hal/
│   │   ├── keypad.h / .cpp     TCA8418 + direct-GPIO drivers behind one interface
│   │   ├── keymap.h / .cpp     key index → Action; multi-tap T9 table
│   │   └── display.h / .cpp    U8g2 wrapper (status bar, large/small font, text)
│   ├── services/
│   │   ├── wifi_manager.h/.cpp hotspot connect, backoff, NVS credentials
│   │   ├── time_service.h/.cpp SNTP sync with timeout
│   │   └── deepseek_client.h/.cpp HTTPS POST + SSE stream + chunked decode
│   ├── ui/
│   │   ├── text_buffer.h/.cpp  fixed-capacity line editor + multi-tap
│   │   └── renderer.h/.cpp     wrap / scroll / labelled-input helpers
│   ├── calc/
│   │   ├── expr.h / .cpp       shunting-yard tokenizer + evaluator
│   │   └── calculator.h/.cpp   expression state + history
│   └── util/
│       ├── ring_buffer.h       fixed-capacity ring template
│       └── sse_parser.h/.cpp   incremental SSE data-line extractor
└── test/
    └── test_expr.cpp           native unit tests for the evaluator
```

### 3.1 `platformio.ini` (as-built)

```ini
[platformio]
default_envs = esp32-c3-devkitm-1

[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.partitions = partitions.csv
build_flags =
    -DCORE_DEBUG_LEVEL=2
    -DCONFIG_ARDUINO_LOOP_STACK_SIZE=8192
    -Isrc
lib_deps =
    olikraus/U8g2@^2.35.19
    bblanchon/ArduinoJson@^7.0.4

[env:native]
platform = native
test_framework = unity
build_flags = -std=gnu++17 -Iinclude -Isrc -DUNITY_INCLUDE_DOUBLE
build_src_filter = +<calc/>
```

There is **no `supermini` or `pcb` environment**; the single `esp32-c3-devkitm-1` env builds
the firmware for both the prototype and final board (identical GPIO map).

### 3.2 Module responsibilities (file by file)

| File | Does | Does NOT |
|---|---|---|
| `main.cpp` | create the App, call `begin()` + `startTasks()` | networking, UI |
| `app.cpp` | own all modules; run uiTask + netTask loops; queue IPC; battery sampling; heap logging | display, sockets directly |
| `ui_fsm.cpp` | state machine §5; dispatch Key/AiToken/AiDone/AiError/WifiState events; render each screen | I²C, sockets |
| `keymap.cpp` | map keypad index → `Action`; multi-tap T9 table; digit→char helper | debouncing |
| `keypad.cpp` | TCA8418 + direct-GPIO scan, 12 ms debounce, 120 ms repeat, 600 ms long-press, event queue | key interpretation |
| `display.cpp` | U8g2 setup for `ssd1309`/`sh1106` 128×64; status bar; large/small font | re-entrancy from netTask |
| `text_buffer.cpp` | fixed-capacity line editor, multi-tap commit (800 ms), caps toggle | display |
| `wifi_manager.cpp` | connect with 12 s timeout + exponential backoff (2→30 s); NVS credentials | HTTP |
| `time_service.cpp` | SNTP sync (`pool.ntp.org`/`time.google.com`), `ensureSynced()` with timeout | HTTP |
| `deepseek_client.cpp` | build JSON body, `WiFiClientSecure`+`HTTPClient`, POST, chunked decode + SSE parse (§7.3), emit tokens/status | display, retry policy |
| `expr.cpp` | shunting-yard evaluator (`+ - * / ^ ( )`, `sin/cos/tan/log/ln/sqrt/abs/exp`, `pi`/`e`) | I/O |
| `calculator.cpp` | expression buffer, local evaluate, history ring | network |
| `sse_parser.cpp` | incremental `data:` line extraction, `[DONE]` latch | HTTP |
| `config.h` | **single source of truth** for pins/timeouts/sizes — must equal HARDWARE.md §6 | — |
| `secrets.h` | credentials only; never printed; never committed | logic |

## 4. Key mapping (initial proposal — locked after Phase 2 §M4)

`keymap.cpp` holds a `kMatrix[]` table (index → `Action`) that must be re-buzzed against the
real Casio membrane. Logical actions (see `hal/keymap.h`):

| Physical key | Action |
|---|---|
| `0–9` `.` | digits + decimal point |
| `+ − × ÷ ^ ( )` | append operator/parenthesis (`+ - * / ^ ( )`) |
| `sin cos tan log ln √` | append function text (e.g. `sin(`) |
| `DEL` | delete back |
| `AC` | short = clear entry, long = clear all |
| `=` | evaluate locally (shunting-yard engine) |
| `MODE` | offline → WIFI_CONFIG; online → TEXT_INPUT |
| `AI` (or `ENTER` in TEXT_INPUT) | send expression/text to DeepSeek |
| `UP`/`DOWN` | history recall (CALC) / scroll reply (AI) / switch WiFi field |
| `SHIFT` | toggle text caps (multi-tap alpha case) |

**Multi-tap T9** (TEXT_INPUT and WIFI_CONFIG): `2`=`abc`, `3`=`def`, `4`=`ghi`, `5`=`jkl`,
`6`=`mno`, `7`=`pqrs`, `8`=`tuv`, `9`=`wxyz`, `1`=`.`, `0`=space. 800 ms inactivity commits
the current glyph.

## 5. UI state machine

Actual modes (`ui_fsm.h::Mode`): `Boot`, `Calc`, `TextInput`, `AiResponse`, `WifiConfig`,
`Error`.

```text
                         BOOT (splash ~900 ms)
                              │
                              ▼
                          ┌────────┐  MODE (offline) ┌────────────┐
                          │  CALC  │ ───────────────►│ WIFI_CONFIG│
                          │        │◄── MODE/back ───│  (SSID/pass)│
                          └───┬────┘                  └─────┬──────┘
              MODE (online)   │                            │ WifiState=Connected
                              ▼                            ▼
                        ┌───────────┐  ENTER/AI       ┌────────────┐
                        │ TEXT_INPUT│ ───────────────►│ AI_RESPONSE│
                        │ (multi-tap)│                  │ (stream)   │
                        └─────┬─────┘◄── MODE ────────│ AC=cancel  │
                              │                        └────────────┘
                        AiError / AiDone / AiToken      AiToken appends
```

| Mode | Entered by | Exited by |
|---|---|---|
| `Boot` | power on | ~900 ms timer → `Calc` |
| `Calc` | boot, `AC`/`MODE` from AI, WiFi connect | `MODE` → `TextInput`/`WifiConfig`; `AI` → `AiResponse`; `=` local eval |
| `TextInput` | `MODE` (online), `AI` follow-up | `ENTER`/`AI` → `AiResponse`; `MODE` → `Calc` |
| `AiResponse` | `AI`/`ENTER` | `AC` → cancel+`Calc`; `MODE` → `Calc`; `AI` → `TextInput` |
| `WifiConfig` | `MODE` (offline), AI-while-offline | connect → `Calc`; `MODE` → `Calc` |
| `Error` | (reserved; not yet reached by any transition) | any key → `Calc` |

**Key behaviour notes:**

- `=` evaluates **locally** (the `src/calc/` shunting-yard engine) and stores history — it
  does **not** contact the network. `AI` sends.
- The reply buffer is `char[2048]`; tokens append as they stream, oldest scroll out. `UP`/
  `DOWN` scrolls when the reply wraps past 4 rows.
- On `AiError` the partial reply is kept and a compact `[message]` marker is appended.
- WiFi setup is reached either by pressing `MODE` while offline, or automatically when an AI
  query is attempted with no connection.

> ⚠ No sleep ladder, no `ERR_LINK`/`ERR_API`/`HOME`/`CONNECT`/`REQUEST`/`ANSWER` states, no
> dedicated on-screen diagnostics mode, and no deep-sleep wake are implemented in the current
> firmware. (A boot I²C scan and per-key serial log *are* implemented — see §10.) The rest are
> documented targets in PLAN.md / RISKS.md, not shipped behaviour.

## 6. Power management (status)

> ⚠ **Not implemented in the current firmware.** There is no sleep ladder, no deep sleep, no
> wake-on-keypress, and no low-battery force-off. The device stays awake while powered. This
> is the single largest gap vs. the power budget in HARDWARE.md §7 and is tracked as RISKS.md
> R-11 with a concrete remediation path (TCA8418 `/INT` + `esp_sleep` + GPIO hold on the
> regulator).

What *is* implemented: battery voltage sampling on `BATTERY_ADC_PIN` (GPIO0) through a 2×100 kΩ
divider, every 5 s, mapped to a 0–100 % estimate shown on the status bar.

### 6.1 Planned sleep ladder (target, not shipped)

| Stage | Expected † | Notes |
|---|---|---|
| Awake | 8–18 mA | current behaviour |
| Light sleep | < 1.5 mA | TCA8418 keeps scanning → `/INT` wake |
| Deep sleep | ≈ 60 µA | `/INT` EXT0 wake → cold boot |

### 6.2 WiFi lifecycle

Currently WiFi associates and stays associated; there is no 45 s disconnect grace window and
no duty-cycling. `wifi_manager` uses a 12 s per-attempt timeout with exponential backoff
(2 s → 30 s cap) and persists credentials in NVS.

## 7. DeepSeek API contract (verified 2026-09-26)

> ⚠ **Model naming churns.** Legacy `deepseek-chat` / `deepseek-reasoner` endpoint names are
> **retired** as of the verification date. Re-run the curl smoke test below at every software
> milestone. Base URL: `https://api.deepseek.com/chat/completions` (OpenAI-compatible).

### 7.1 Request

Headers:

```text
Content-Type: application/json
Authorization: Bearer $DEEPSEEK_API_KEY
```

Body:

```json
{
  "model": "deepseek-flash",
  "messages": [
    { "role": "system",
      "content": "You are the answer engine of a pocket calculator with a 128x64 text display. Reply in plain text only, no markdown, no lists, no emoji, at most 3 short sentences, and show a numeric result first when asked to compute." },
    { "role": "user", "content": "17% of 58=" }
  ],
  "temperature": 0,
  "max_tokens": 64,
  "thinking": { "type": "disabled" },
  "stream": true
}
```

Notes: `"thinking": {"type": "disabled"}` is **required** for low-latency replies from
`deepseek-flash`; `max_tokens: 64` bounds both cost and the answer window; `temperature: 0`
keeps math deterministic.

### 7.2 curl smoke test (run before trusting firmware)

```bash
curl -sS https://api.deepseek.com/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $DEEPSEEK_API_KEY" \
  -d '{
    "model": "deepseek-flash",
    "messages": [{"role":"user","content":"17% of 58="}],
    "temperature": 0, "max_tokens": 64,
    "thinking": {"type":"disabled"}, "stream": true
  }'
```

Expected: a series of SSE lines like §7.3, ending in `data: [DONE]`. Drop `"stream": true`
to see the single-object variant of §7.4.

### 7.3 Streaming (SSE) format and parser

```text
data: {"id":"...","object":"chat.completion.chunk","created":1777000000,"model":"deepseek-flash","choices":[{"index":0,"delta":{"role":"assistant","content":"9.86"},"finish_reason":null}]}

data: {"id":"...","object":"chat.completion.chunk",...,"choices":[{"index":0,"delta":{"content":" (17% of 58)"},"finish_reason":null}]}

data: {"id":"...","object":"chat.completion.chunk",...,"choices":[{"index":0,"delta":{},"finish_reason":"stop"}]}

data: [DONE]
```

Parser rules (`deepseek_client.cpp`):
1. Read the response body byte-by-byte; decode HTTP chunked framing (DeepSeek streams with
   `Transfer-Encoding: chunked`) before feeding lines to the SSE parser.
2. Ignore blank keep-alive lines and `: keep-alive` comments; accept only `data: ` prefix.
3. `[DONE]` → emit `AiDone`. Otherwise parse with a **filter** document (ArduinoJson
   `DeserializationOption::Filter`), take `choices[0].delta.content`, emit `AiToken` if
   non-empty.
4. A clean EOF that never saw `[DONE]` is treated as an incomplete stream (`-7`), not success.
5. Malformed JSON is skipped (best-effort); HTTP non-200 surfaces as `AiError` with the status
   code. There is no automatic retry loop in the client.

### 7.4 Non-streaming response shape (for reference/tests)

```json
{
  "id": "a1b2c3...",
  "object": "chat.completion",
  "created": 1777000000,
  "model": "deepseek-flash",
  "choices": [{
    "index": 0,
    "message": { "role": "assistant", "content": "9.86 (17% of 58)" },
    "finish_reason": "stop"
  }],
  "usage": { "prompt_tokens": 74, "completion_tokens": 11, "total_tokens": 85 }
}
```

### 7.5 Errors & policies

| HTTP | Meaning | Firmware behaviour |
|---|---|---|
| 401 | key revoked/expired, typo in `secrets.h` | `AiError` with HTTP code surfaced |
| 402 | out of credit | `AiError` "top up account" |
| 429 | rate limited | `AiError` (no auto-retry) |
| 5xx | DeepSeek side | `AiError` (no auto-retry) |
| TLS/timeout | clock drift, network | `AiError`; SNTP is re-verified before every query |

## 8. Memory management rules (no-PSRAM discipline)

Budget at boot (Arduino core, WiFi up): ≈ 250–280 KB heap on a stock C3 build † — TLS can eat
100+ KB transiently. These rules guide the code:

1. **uiTask: no heap on hot paths.** Framebuffer (1 KB, U8g2 full-buffer mode) and all screen
   state (reply `char[2048]`, event `char[72]`) are fixed-size. The `uiTask` stack is 8 KB;
   the expression evaluator's ~8.7 KB of working arrays are `static` precisely to keep them off
   that stack (§ expr.cpp).
2. **Stream never buffered whole.** `AiToken` events (≤ 72 B) → append to the 2048-byte reply
   ring; overflow scrolls. `max_tokens: 64` keeps answers short.
3. **JSON parsing is bounded.** ArduinoJson 7 `JsonDocument` (dynamic) with a
   `DeserializationOption::Filter` that keeps only `choices[0].delta.content`; never
   deserialize the whole body.
4. **Chunked decoding is in a small fixed buffer.** `SSE_LINE_CAP` = 1024; `HTTP_BODY_CAP` =
   1024. The request body is serialized into a fixed `char[1024]`.
5. **No `String` on hot paths.** Fixed `char[]` + `snprintf`. A `String` is used only once per
   request (reading the `Transfer-Encoding` header) on the cold path.
6. **Heap telemetry.** netTask logs `ESP.getFreeHeap()` and
   `heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)` every 30 s (and after each AI turn).
7. **Task stacks.** netTask 16 KB (TLS), uiTask 8 KB (U8g2). Tune once with
   `uxTaskGetStackHighWaterMark` at milestones.

## 9. Battery voltage monitoring († typical LiPo curve — calibrate in Phase 5)

The firmware maps the divided ADC reading (2×100 kΩ, `BATTERY_DIVIDER = 2.0`) to a linear
0–100 % estimate between `BATTERY_FULL_MV = 4200` and `BATTERY_EMPTY_MV = 3200`, shown on the
status bar. There is no OCV lookup table and no low-battery forced shutdown yet (see §6).

## 10. Logging & debugging

- Serial 115200 via native USB. Tags are minimal (`[boot]`, `[i2c]`, `[key]`, `[heap]`,
  `[ai]`, `[tls]`, `[app]`, `[display]`).
- Boot prints firmware name/version and `[boot] free heap=<bytes>`.
- Boot I²C scan: `[i2c] scan: 0x34 0x3C` (TCA8418 + OLED); `none` points at a wiring fault.
- Every key press: `[key] idx=<n> action=<NAME>` (guarded by `ENABLE_SERIAL_DEBUG`).
- Every 30 s (and after each AI turn) netTask prints `[heap] free=<bytes> largest=<bytes>`.
- After each query: `[ai] http=<status> bytes=<n> free=<bytes> largest=<bytes>`.
- `test/` runs `pio test -e native` and covers the expression evaluator only
  (`test_expr.cpp`); the SSE parser and keymap are not yet covered by host tests.
