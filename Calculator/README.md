# AI Calculator — Casio FX-300MS Plus, Reborn with an ESP32-C3 and DeepSeek

A Casio FX-300MS Plus scientific calculator, gutted and rebuilt around an **ESP32-C3**, a
**1.54″ 128×64 OLED**, a **200 mAh LiPo**, and the original silicone keypad. From the outside it
looks stock. From the inside, every key press is text that gets sent over a phone hotspot to the
**DeepSeek AI API**, and the answer streams back onto a replacement display that sits behind the
original window.

This repository contains the **complete build documentation**. The firmware lives in
[`firmware/`](firmware/) (a PlatformIO project) and is documented in
[`docs/SOFTWARE.md`](docs/SOFTWARE.md).

---

## Who this is for (read before you buy anything)

This is a **multi-phase build**, and the phases need very different skill levels. Be honest
about where you sit.

| Phase | What it is | Skills required | Beginner-friendly? |
|---|---|---|---|
| **1** | Breadboard prototype on the desk — no Casio harmed | Solder headers + a few wires, install PlatformIO, flash, use a multimeter, (optionally) edit one small C++ array | 🟡 **Yes, with effort** — the realistic first-timer target |
| **2** | Casio teardown + caliper measurements | Careful disassembly, digital calipers, DMM continuity | 🟡 Doable with care |
| **3** | In-case prototype | Cut/3D-print a bezel, fabricate keypad contact pads, tight fitting | 🟠 Intermediate |
| **4** | Custom PCB design + fab | EDA (KiCad etc.), QFN/SMD soldering or an assembly service, RF/antenna layout | 🔴 **Advanced — not a beginner task** |
| **5** | Assembly & polish | Reliability testing, fine mechanical work | 🟠 Intermediate |

> **If you are a beginner:** you can realistically reach a **working Phase 1 breadboard** — an
> OLED that evaluates `2+2=` and streams a DeepSeek answer. Phases 2–5 require you to learn PCB
> design and SMD soldering first. [GETTING_STARTED.md](docs/GETTING_STARTED.md) opens with a
> **minimum-viable path** that avoids code editing and avoids typing your WiFi password on the
> keypad.

---

## Current state — what's done vs. what's left

> ⚠ **Nothing here has run on real hardware yet.** The software is written and compiles; every
> hardware result below is a documented *target* until you build and test it on your own bench.

**✅ Already done — in this repo, no hardware required**

| Thing | Where |
|---|---|
| Firmware v1 written and **compiles** (`pio run` → SUCCESS; Flash 31.9 %, RAM 16.6 %) | [`firmware/`](firmware/) · verified 2026-09-26 |
| Calculator engine host unit tests (`pio test -e native`; needs a working host C++ toolchain) | `firmware/test/test_expr.cpp` |
| Complete build documentation (plan, BOM, wiring, software, risks) | [`docs/`](docs/) |
| DeepSeek API contract verified (model `deepseek-flash`, SSE streaming) | [SOFTWARE.md §7](docs/SOFTWARE.md) |

**🛠 Can be done right now with only a computer**

- Read the plan and decide your go/no-go → [PLAN.md](docs/PLAN.md).
- Build the firmware (`pio run`), and flash it once a board arrives.
- Bake your hotspot into `secrets.h`, and pick your keypad mapping (or use a ≥ 5×7 keypad and
  change nothing) → [GETTING_STARTED.md §0](docs/GETTING_STARTED.md).
- Generate the TLS CA bundle if you want `TLS_MODE_BUNDLE`.

**⬜ Still to do before a finished, working calculator**

| Phase | What it produces | Needs | Status |
|---|---|---|---|
| **1** | Breadboard prototype that streams an AI answer | Your parts + soldering | ⬜ Not started |
| **2** | Casio teardown + the 4 caliper measurements (**gate for every purchase**) | Donor unit + calipers | ⬜ Not started |
| **3** | Working unit inside the real case | 3D-printed/cut bezel, keypad contacts | ⬜ Gated on 2 |
| **4** | Custom PCB designed, fabbed and populated | EDA skills, fab lead time | ⬜ Gated on 3 |
| **5** | Finished, stock-looking unit; repository tagged `v1.0` | Reliability testing | ⬜ Gated on 4 |

## Build path — follow the phases to a finished calculator

Every phase has entry criteria, numbered steps, milestones and an acceptance **gate** in
[docs/PLAN.md](docs/PLAN.md). Work them in order; **do not start a phase until the previous
gate passes.** The whole path at a glance:

| Step | Do this | Guide | Phase is done when |
|---|---|---|---|
| **0** | Check your skill level | [Who this is for](#who-this-is-for-read-before-you-buy-anything) | You know which phase you can reach |
| **1** | Assemble the breadboard prototype | [GETTING_STARTED.md](docs/GETTING_STARTED.md) → [WIRING.md](docs/WIRING.md) → [HARDWARE.md §2](docs/HARDWARE.md) | OLED evaluates `2+2=4` **and** streams a DeepSeek answer; runs on battery |
| **2** | Tear down a donor Casio and measure | [PLAN.md Phase 2](docs/PLAN.md) · [HARDWARE.md §5](docs/HARDWARE.md) | 4 measurements + keypad map recorded; display SKU ordered |
| **3** | Fit the working system inside the case | [PLAN.md Phase 3](docs/PLAN.md) | Case closes; all 50 keys work in-case; 24 h on battery |
| **4** | Design and fab the custom PCB | [PLAN.md Phase 4](docs/PLAN.md) · [HARDWARE.md §2.1](docs/HARDWARE.md) | Board fits, and passes the Phase 1 soak unchanged |
| **5** | Final assembly, polish, reliability | [PLAN.md Phase 5](docs/PLAN.md) | Looks stock; reliability pass done; repo tagged `v1.0` |

**Critical path:** Phase 1 stability → Phase 2 measurement gate → display lead time → PCB fab
lead time. Order the display the day Phase 2 closes.

> **Want the AI calculator but not the Casio build?** Do **Phase 1** and stop. That is already a
> complete, working device on your desk — just not inside the donor case.

---

## Quick facts

| Item | Choice | Notes |
|---|---|---|
| Donor | Casio FX-300MS Plus | ~155–162 × 77–85 × 10–12.2 mm, two ABS shells, 6 screws |
| MCU (prototype) | ESP32-C3 SuperMini | Cheap, USB-C native, throwaway form factor |
| MCU (final) | ESP32-C3-MINI-1 (PCB mount) | ~18 × 19.8 mm module; antenna variant per HARDWARE.md §2.1 ⚠ |
| Display | 1.54″ 128×64 COG OLED (SSD1309/SH1106), I2C | Active area ≈ 35.05 × 17.51 mm |
| Display fallbacks | 1.3″ 128×64 (≈ 29.4 × 14.2 mm), Winstar WEO012864AH; 0.96″ 128×64 | Pick after caliper measurement (see HARDWARE.md §M1) |
| Keypad | Reused Casio silicone + carbon pills, 8×7 matrix (56 positions, 50 keys) | TCA8418 I²C controller (recommended) or direct GPIO scan |
| Battery | LiPo 200 mAh (LP402030) | MCP73831 charger + DW01A/FS8205A protection + USB-C |
| Power rail | TPS63802 buck-boost → 3.3 V | Single cell, LiPo discharge curve |
| Connectivity | 2.4 GHz WiFi (phone hotspot) | HTTPS to DeepSeek |
| AI backend | `POST https://api.deepseek.com/chat/completions`, model `deepseek-flash` | Verified 2026-09-26; legacy names retired |
| Stack | Arduino / PlatformIO (`platform-espressif32`), FreeRTOS, U8g2, ArduinoJson | See SOFTWARE.md |

## Features

- **Looks stock.** Original shells, keypad and display window are retained. All new hardware
  hides inside the case.
- **A real calculator first.** `src/calc/` is a shunting-yard expression engine (`+ - * / ^ ( )`,
  `sin/cos/tan/log/ln/sqrt/abs/exp`, constants `pi`/`e`) with a native unit-test suite. `=`
  evaluates locally and stores history.
- **Ask the AI anything.** The `AI` key (or `ENTER` in TEXT_INPUT mode) sends the current
  expression or typed text to DeepSeek, and the streamed answer renders on the OLED.
- **Answers in ≤ 64 tokens.** Short, deterministic replies (`temperature: 0`,
  `max_tokens: 64`, thinking disabled) chosen to fit the tiny display and the battery budget.
- **Two ways to set WiFi.** Bake `WIFI_SSID`/`WIFI_PASS` into `secrets.h` (easiest), or use the
  on-device WIFI_CONFIG screen (`MODE` while offline) for multi-tap entry. The keypad charset
  cannot produce every symbol — see [GETTING_STARTED.md §3](docs/GETTING_STARTED.md).
- **USB-C charging.** Charges from any 5 V source; ~10–25 % C-rate by design (slow is quiet).
- **Single-keypress input model.** The un-dioded carbon-pill matrix is read by a TCA8418; the
  UI accepts one key at a time, which sidesteps ghosting (RISKS.md R-04).

> ⚠ **Not yet implemented** (documented as targets, not shipped code): deep-sleep /
> keypad-interrupt wake. The firmware currently polls the keypad and has no sleep module.
> See RISKS.md R-11.

## Repository layout

```text
Calculator/
├── README.md                 ← you are here
├── docs/
│   ├── PLAN.md               5-phase build plan, milestones, acceptance criteria
│   ├── HARDWARE.md            BOM, prices, caliper measurements, pinout, power budget
│   ├── WIRING.md              breadboard wiring diagram + step-by-step guide
│   ├── SOFTWARE.md            firmware architecture, modules, UI state machine, API
│   ├── GETTING_STARTED.md     setup, secrets, flashing, first-query smoke test
│   └── RISKS.md               risk register with mitigations and contingencies
└── firmware/                  PlatformIO project
    ├── platformio.ini         env:esp32-c3-devkitm-1 (firmware) + env:native (tests)
    ├── partitions.csv
    ├── src/
    │   ├── main.cpp           composition root
    │   ├── app/               events, app (tasks), ui_fsm (state machine)
    │   ├── hal/               keypad (TCA8418/GPIO), keymap (multi-tap), display (OLED)
    │   ├── services/          wifi_manager, time_service (SNTP), deepseek_client
    │   ├── ui/                text_buffer (multi-tap), renderer
    │   ├── calc/              expr (shunting-yard evaluator), calculator
    │   └── util/              ring_buffer, sse_parser
    ├── include/               config.h, secrets.example.h, secrets.h (git-ignored), deepseek_roots.h
    └── test/                  test_expr.cpp (native unit tests for the evaluator)
```

## Where to start

1. **Never built software for it?** → [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)
2. **About to spend money?** → [docs/HARDWARE.md](docs/HARDWARE.md) (BOM + the 4 measurements
   you must take before ordering the display or PCB)
3. **Holding a screwdriver to a Casio?** → [docs/PLAN.md](docs/PLAN.md) Phase 2
4. **Wiring the breadboard?** → [docs/WIRING.md](docs/WIRING.md)
5. **Writing or reviewing firmware?** → [docs/SOFTWARE.md](docs/SOFTWARE.md)
6. **Deciding whether this can work?** → [docs/RISKS.md](docs/RISKS.md)

## Assumptions and warnings (read before ordering anything)

- **Display fit is assumed, not proven.** The 1.54″ panel (≈ 35.05 × 17.51 mm active) is chosen
  against estimated window dimensions. Measure the window *first* (HARDWARE.md §M1). ABS
  bezels can intrude into the opening; subtract the bezel lip, not just the glass edge.
- **Internal cavity is assumed ~5–7 mm** behind the keypad. Everything in the stack
  (PCB + OLED + LiPo + connectors) must fit this, in zones, or material must be relieved
  (measured in Phase 2; see RISKS.md R-01/R-02).
- **The donor calculator is destroyed.** Budget a spare FX-300MS Plus for practice teardown.
- **Prices are street estimates (2026-09)** marked with † and must be re-checked at order time.
- **Currents marked † are design estimates**, not measured. The power budget becomes real data
  after Phase 1 with a µA-capable power profiler.
- **LiPo in a pocket product is a safety item.** Protection IC mandatory (included), no
  unprotected cells in the case, charge only with the case open or monitored in Phase 1.
- **DeepSeek API shape was verified 2026-09-26.** Model names have already churned once
  (`deepseek-chat`/`deepseek-reasoner` are retired); re-verify before every software milestone.

## Non-goals (for now)

- No solar cell harvesting (the donor's solar cell, if present, becomes a decorative window).
- No OTA updates in v1; reflashing over USB is acceptable for a single build.
- No multi-key rollover, no chorded input.
- No deep-sleep / wake-on-keypress in the first firmware cut (target, not yet shipped).
