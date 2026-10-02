# AGENTS.md

ESP32-C3 "AI calculator" firmware (PlatformIO/Arduino) designed to fit a Casio FX-300MS Plus shell, plus build docs. See `README.md` and `docs/` for the human-facing plan.

## Layout & where commands run

- The repo root is **not** a PlatformIO project. All `pio` commands must run from `firmware/`.
- `firmware/` — PlatformIO project (the only code).
- `docs/` — PLAN / HARDWARE / WIRING / SOFTWARE / GETTING_STARTED / RISKS.
- `.opencode/plan` — canonical agent plan; read it before non-trivial changes.

## Commands (run in `firmware/`)

```bash
pio run                 # build firmware (env esp32-c3-devkitm-1)
pio run -t upload       # flash
pio device monitor -b 115200
pio test -e native      # host unit tests for the calculator engine
```

- `pio` may not be installed (`pip install platformio`). On Apple Silicon macOS the RISC-V toolchain needs Rosetta 2.
- `[env:native]` has `build_src_filter = +<calc/>`: it compiles **only `src/calc/`**. Tests for any other module require extending that filter and cannot include Arduino headers.

## Architecture (not obvious from filenames)

- Two FreeRTOS tasks in `src/app/app.cpp`; `main.cpp` `loop()` is intentionally idle.
  - `uiTask` — keypad polling, UI FSM, OLED rendering. The only task that touches I²C/display.
  - `netTask` — WiFi, SNTP, HTTPS/SSE. Never touch the display from here.
- Tasks communicate **only** via FreeRTOS queues of POD `Event` structs (`src/app/events.h`, inline `char text[72]`). No shared mutable state except `volatile bool aiCancel_`; keep it that way.
- UI modes: `Boot → Calc → TextInput ↔ AiResponse → WifiConfig`. `=` evaluates **locally** (`src/calc/` shunting-yard engine); the `AI` key sends to DeepSeek. `MODE` is context-sensitive: offline → WIFI_CONFIG, online → TEXT_INPUT.
- `src/calc/expr.cpp` declares its working arrays `static` on purpose — they are ~8.7 KB and must stay off the 8 KB `uiTask` stack. Do not "clean up" them into locals.

## Hardware / API gotchas

- DeepSeek model string is **`deepseek-flash`**. `deepseek-chat` and `deepseek-reasoner` were retired 2026-07-24 — do not reintroduce them.
- SNTP must succeed **before** any TLS call, or the 1970 boot clock fails certificate validation. `deepseek_client` is only reached via `TimeService::ensureSynced()`.
- Active pins (`firmware/include/config.h`): I²C SDA=8, SCL=9; battery ADC=0 (2×100 kΩ divider). GPIO3 (TCA8418 `/INT`) and GPIO7 (OLED RST) are **reserved but not wired** in v1.
- The keypad is **polled** over I²C (`TCA8418`, addr 0x34). There is no interrupt path.
- `keymap.cpp` `kMatrix[]` is a **placeholder** — it must be re-mapped against the real Casio membrane. Do not treat mismatched keys as a firmware bug.
- TLS defaults to a pinned Amazon Root CA 1 (`include/deepseek_roots.h`). `ALLOW_INSECURE_TLS` is a bring-up escape hatch only.

## Constraints

- ESP32-C3, ~400 KB SRAM, no PSRAM. No `String` on hot paths; stream SSE line-by-line instead of buffering (`http.getString()` on the chat reply is forbidden). JSON uses ArduinoJson `DeserializationOption::Filter` to keep only `choices[0].delta.content`.
- `firmware/include/config.h` is the single source of truth for pins/sizes/timeouts. If you change it, update `docs/HARDWARE.md §6` and `docs/WIRING.md` in the same change — the three are a contract.
- `include/secrets.h` is git-ignored and is **not** part of a clone; copy `secrets.example.h` to `secrets.h` before building. Never commit real keys.

## Docs vs. firmware

Some documented behaviour is a **target, not yet implemented**: deep/light sleep, keypad-interrupt wake, low-battery forced shutdown, and SSD1309/SH1106 auto-detect. Keep "implemented" and "planned" clearly separated when editing docs; do not describe these as working. (A boot I²C scan and per-key serial log *are* implemented.)
