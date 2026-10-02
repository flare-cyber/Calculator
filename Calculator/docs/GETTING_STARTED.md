# Getting Started

From zero to a first AI answer on the OLED. This covers **Phase 1 software bring-up** — the
mechanical steps live in [PLAN.md](PLAN.md), the wires in [WIRING.md](WIRING.md).

## 0. Read this first — skill level & the minimum-viable path

**What you need:** basic electronics confidence — soldering a header row, using a multimeter,
and following exact commands. You do **not** need prior ESP32 or C++ experience. (If you use a
4×4 keypad you will edit one small C++ array; the path below avoids even that.)

**Minimum-viable Phase 1 path.** This reaches a working AI calculator on the desk while
sidestepping the two steps beginners find hardest:

1. Use a keypad with **≥ 5 rows × 7 columns** → the shipped `kMatrix[]` works unchanged, so
   **no code editing** (details: [WIRING.md §3a](WIRING.md)).
2. **Bake your hotspot into `secrets.h`** (§3) → you never type a password on the keypad.
3. **Run from USB power first** and add the LiPo only after the software works
   ([WIRING.md §7](WIRING.md)).

Everything below still applies; those three choices just remove the sharpest edges.

This guide covers **Phase 1 only**. Phase 1 is the one phase you can finish without a donor
Casio — and stopping here still leaves you with a complete, working AI calculator on the desk.
The later phases (teardown → in-case → PCB → assembly) are laid out in the
[README build path](../README.md#build-path--follow-the-phases-to-a-finished-calculator).

## 1. Prerequisites

### Skills & accounts
- Basic PlatformIO/Arduino flashing — or willingness to follow §5 literally. First-timers: use
  the **minimum-viable path** in §0.
- A **DeepSeek API key**: create an account at `https://platform.deepseek.com` (verify URL in
  your region ⚠), generate an API key, and add a small credit balance — free-tier-only keys
  fail with `402` on the very first query.
- A phone you can tether: 2.4 GHz hotspot (ESP32-C3 is single-band; a 5 GHz-only hotspot
  will **not** be seen).

### Hardware (see [HARDWARE.md §2](HARDWARE.md) for full BOM)
- ESP32-C3 SuperMini ×1 (spare recommended)
- 1.54″ SSD1309 128×64 I²C OLED module (header version for the bench)
- TCA8418 module + a membrane keypad — **≥ 5×7 recommended** so no code edit is needed, or a
  4×4 with the §0 key-map paste. Phase 1 does **not** need the Casio yet.
- LiPo LP402030 + protection, MCP73831 module, TPS63802 module (or start bench-powered from
  USB and add battery wiring at step W7 in WIRING.md)
- USB-C data cable (not charge-only — the commonest first-hour trap)

### Toolchain
| Tool | Version guidance |
|---|---|
| PlatformIO Core (CLI or VS Code extension) | current stable ⚠ |
| platform `espressif32` | the build is verified with the current stable platform (Arduino core 2.0.17). No extra USB-CDC build flags are required by the project; if you hit C3 USB-serial issues, try the community `pioarduino/platform-espressif32` fork ⚠ |
| USB drivers | usually none (ESP32-C3 native USB-Serial-JTAG); Windows may enumerate via `usbser` ⚠ |

## 2. Clone and open the project

```bash
cd Calculator
# repo root contains README.md, docs/, firmware/
cd firmware
pio pkg update            # resolve platform + libs (U8g2, ArduinoJson)
```

## 3. Create `secrets.h` (required — do this before the first build)

`secrets.h` is **git-ignored** and is therefore **not present in a fresh clone**. The
firmware `#include`s it unconditionally, so `pio run` will fail until you create it.

```bash
cp include/secrets.example.h include/secrets.h
$EDITOR include/secrets.h          # or: notepad / nano — your call
```

`include/secrets.h` must define exactly:

```cpp
#pragma once
#define WIFI_SSID          "YourPhoneHotspotName"  // 2.4 GHz!
#define WIFI_PASS          "hunter2-here"
#define DEEPSEEK_API_KEY   "sk-.................." // from platform.deepseek.com
```

**Check `include/secrets.h` appears in `.gitignore` before you put a real key anywhere.**
The example file's comments list the same three macros; if they ever diverge, fix the example
in the same commit. Treat the API key like a password: revoke/re-issue if the repo ever
leaks it.

Then run the first build:

```bash
pio run                   # sanity compile before touching hardware
```

Expected: a green `SUCCESS` in under ~2 minutes on first run (downloads dominate).

> **WiFi credentials — two options.** The firmware reads credentials stored in NVS first,
> then falls back to the `WIFI_SSID`/`WIFI_PASS` values above at boot (the example
> `YourPhoneHotspotName` placeholder is ignored). So either:
> 1. **Bake your hotspot into `secrets.h`** (easiest, and it avoids typing a password with
>    the multi-tap keypad — the on-screen charset cannot produce every symbol), or
> 2. leave the placeholders and **enroll on the device** through the WIFI_CONFIG screen
>    (press `MODE` while offline).
>
> Only `DEEPSEEK_API_KEY` is required for the API. `secrets.h` is git-ignored, so baked-in
> credentials never leave your machine.

## 4. Wire it up

Follow [WIRING.md](WIRING.md) §3 steps 1–8. Short version, per the pin contract in
[HARDWARE.md §6](HARDWARE.md):

| Do | Pin |
|---|---|
| OLED SDA / SCL / VCC / GND | GPIO8 / GPIO9 / 3V3 / GND |
| TCA8418 SDA / SCL / VCC / GND | GPIO8 / GPIO9 / 3V3 / GND |
| Membrane keypad rows/cols | TCA8418 ROW0–7 / COL0–6 |
| Battery ADC (once wired to LiPo) | GPIO0 (via 2×100 kΩ ÷ 2 divider) |

## 5. Flash and monitor

```bash
pio run -t upload           # finds the serial port automatically
# if multiple ports exist, pass the one PlatformIO reports for your OS, e.g.:
#   macOS/Linux:  pio run -t upload --upload-port /dev/cu.usbmodemXXXX
#   Windows:      pio run -t upload --upload-port COM5
pio device monitor -b 115200        # PlatformIO serial monitor
```

Healthy boot log:

```text
AI Calculator 1.0.0 booting
[boot] free heap=<bytes>
[heap] free=<bytes> largest=<bytes>    (repeats every 30 s)
```

## 6. First-AI-query smoke test checklist

Run these in order; every line is a checkbox in the Phase-1 milestone M1.3 (PLAN.md).

- [ ] **1. Boot.** Power on → OLED shows the boot splash (`AI Calculator v1.0.0`), then the
      CALC screen (`-> type or press MODE`). Serial shows the boot heap line.
- [ ] **2. Local calc.** Type `2+2` then press `=` → the result `4` appears on the result line.
      This exercises the keypad → calculator → display path with no network.
- [ ] **3. WiFi.** Easiest: bake `WIFI_SSID`/`WIFI_PASS` into `secrets.h` (§3) and reboot — the
      device connects automatically; the serial WiFi-state lines and the status-bar `W` confirm
      it. Otherwise press `MODE` (offline) → the WIFI_CONFIG screen opens and you enter the SSID
      and password with multi-tap → `ENTER`. ⚠ The multi-tap charset is only `. , ? !`, letters,
      digits and space — it **cannot** type `-` `_` `@` `#`, so bake any such password into
      `secrets.h` instead.
- [ ] **4. Time.** SNTP runs automatically once connected; the first HTTPS request blocks
      until the clock is valid. If TLS fails, re-check this path first (SOFTWARE.md §7.5).
- [ ] **5. API handshake.** Type a query and press `AI` (or `ENTER` in TEXT_INPUT mode).
      Serial shows `[ai] http=200`. A curl equivalent from SOFTWARE.md §7.2 should also
      succeed — that separates key problems from firmware problems.
- [ ] **6. Render.** Answer text appears on the OLED **while the stream is still finishing**;
      ends when `[DONE]` is received (serial logs the reply byte count).
- [ ] **7. Error paths.** (a) turn the hotspot off → an error marker appears after the partial
      reply; (b) put a `wrong-key` in `secrets.h` → HTTP 401 is surfaced; (c) restore both →
      the query works.
- [ ] **8. Heap stability.** Repeat queries 1–5 ten times; the `[heap]` log line's free byte
      count stays within ~10 % of the first (SOFTWARE.md §8).
- [ ] **9. Battery.** Unplug USB, run the whole checklist on the LiPo chain — display
      brightness may drop slightly under WiFi TX; the battery indicator must not hit 0 % †.

All green → Phase 1 milestone M1.3 achieved; proceed to PLAN.md Phase 1 steps 6–7
(measured power table, soak) before touching the Casio.

## 7. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Port never appears on `pio device monitor -t port` | charge-only cable | swap for a data cable |
| Upload fails "connecting…" | board held in reset | hold BOOT during power-on; try `--upload-port` |
| Display blank | swapped SDA/SCL, no pull-ups, 5 V confusion | WIRING.md §6 table |
| OLED shifted / wrong columns | panel is SH1106 silicon | set `DISPLAY_CONTROLLER` to `DISPLAY_CTRL_SH1106` in `config.h` |
| TLS: handshake fails | clock not set (no RTC battery) | ensure SNTP precedes HTTPS; check hotspot allows NTP (UDP 123) |
| TLS: cert verify fail | stale CA / captive portal | update core; confirm the phone can reach `api.deepseek.com` (hotspot login pages break TLS) |
| `402` from API | no credit | top up DeepSeek account |
| Brownout resets during WiFi (`rst BOR`) | weak 3V3 wiring on breadboard | WIRING.md §5: short/thick supply wires |
| Heap shrinking across queries | leaks in dev | SOFTWARE.md §8 rules; diff against a clean build |
| Key does nothing | membrane wired ROW/COL transposed | continuity-test one key at a time; check `keymap.cpp` table |
| VS Code: `#include errors detected` | IntelliSense lacks PlatformIO's include paths — the build is fine | run `pio run -t compiledb` (see [firmware/README §13](../firmware/README.md)) |

## 8. What to read next

- [HARDWARE.md §5](HARDWARE.md) — the four caliper measurements (before any Casio order
  beyond the Phase-1 kit).
- [PLAN.md Phase 2](PLAN.md) — teardown day procedure.
- [RISKS.md](RISKS.md) — what usually goes wrong and its early-warning log line.
