# Wiring Guide — Phase 1 Breadboard Prototype

This guide matches the GPIO contract in [HARDWARE.md §6](HARDWARE.md) and the firmware in
[SOFTWARE.md](SOFTWARE.md). If a wire here moves, the docs and firmware move with it — the
Phase 4 PCB is laid out from this pin map.

## 0. Rules before you touch a wire

1. **3.3 V logic everywhere — no level shifters.** ESP32-C3, SSD1309 OLED and TCA8418 are all
   3.3 V parts with 5 V-tolerant I/O on their own (the only 5 V in the system is USB VBUS
   feeding the *charger*, never a logic pin).
2. **Double-check polarity on the LiPo and the OLED FPC.** Two of the three magic-smoke
   mistakes on this bench are reverse battery and backwards FPC.
3. **First power-up is USB only.** Do the whole rail-up sequence (step 6) before the LiPo is
   ever connected.
4. Antenna keep-out rules are in §4 — decide wire routing with them in mind from the start.

## 1. Signal wiring diagram (prototype, all modules)

```text
                         ESP32-C3 SuperMini
                      (USB-C at top edge = flash port)
            ┌────────────────────────────────────────┐
    USB ───►│ 5V  GND  3V3                           │
            │                                        │
            │ GPIO8  ──── SDA ──────────────┐        │
            │ GPIO9  ──── SCL ────────┐     │        │
            │ GPIO0  ◄─── BAT_ADC     │     │        │
            │ GND    ──── GND ◄────┼──┼─────┼───┐    │
            │ 3V3    ──── 3V3 ─────┼──┼─────┼─┐ │    │
            └──────────────────────┼──┼─────┼─┼─┼────┘
                                   │  │     │ │ │
             I2C bus (both devices │  │     │ │ │ share SDA/SCL/GND/3V3 —
             share the bus)        ▼  ▼     ▼ ▼ ▼
      ┌──────────────────┐   ┌───────────────────────────┐
      │  1.54" SSD1309   │   │      TCA8418 module       │
      │  OLED (I2C 0x3C) │   │      (I2C addr 0x34)      │
      │                  │   │                           │
      │ VCC ◄── 3V3      │   │ VCC ◄── 3V3               │
      │ GND ◄── GND      │   │ GND ◄── GND               │
      │ SDA ◄──►GPIO8    │   │ SDA ◄──►GPIO8             │
      │ SCL ◄──►GPIO9    │   │ SCL ◄──►GPIO9             │
      │ RES ── 10k →3V3  │   │ INT ── leave unconnected  │
      │ (or tie to 3V3)  │   │  (firmware polls I2C;     │
      │                  │   │   /INT unused in v1)      │
      │ FPC tail → glass │   │ ROW0..ROWn  ──► keypad rows│
      └──────────────────┘   │ COL0..COLn  ──► keypad cols│
                             └───────────────────────────┘
                                          ▲
                                          │ 8+7 = 15 wires (or ribbon)
                             ┌────────────┴─────────────┐
                             │  Membrane keypad (Phase1)│
                             │  → Casio silicone (P3)   │
                             │    8×7 grid, 50 keys     │
                             │    single key-press at a │
                             │    time (no diodes)      │
                             └──────────────────────────┘
```

### Connection table (signal)

| # | From | To | Wire colour suggestion |
|---|---|---|---|
| 1 | SuperMini GPIO8 | OLED **SDA** + TCA8418 **SDA** | blue |
| 2 | SuperMini GPIO9 | OLED **SCL** + TCA8418 **SCL** | green |
| 3 | SuperMini GPIO7 | (unused in v1 — OLED RES pulled to 3V3) | — |
| 4 | SuperMini GPIO3 | (unused in v1 — TCA8418 polled over I²C) | — |
| 5 | SuperMini 3V3 | OLED **VCC** + TCA8418 **VCC** | red (after rail check!) |
| 6 | SuperMini GND | OLED **GND** + TCA8418 **GND** + BAT divider GND | black |
| 7 | SuperMini GPIO0 | **BAT_ADC** node (midpoint of divider) | orange |
| 8 | TCA8418 ROW0–7 / COL0–6 | keypad membrane rows/cols (map per continuity test) | any, labelled |

## 2. Power wiring diagram (Phase 1 chain)

```text
            USB-C 5V                      3.7–4.2 V                      3.3 V rail
   ┌────────┐    ┌──────────────┐   ┌──────────────────┐   ┌───────────────┐
   │  USB-C │    │ MCP73831     │   │ LiPo LP402030    │   │ TPS63802      │
   │  cable │───►│ charge module├──►│ + DW01A/FS8205A  ├──►│ 3V3 module    ├──┬──► SuperMini 5V pin? NO:
   └────────┘    │ (PROG resistor│   │ PROTECTED cell   │   │ (VIN/VOUT/GND)│  │   feed 3V3 directly only
    VBUS ─ charge│  ≈ 20–50 mA  │   │ (B+, B− pads)    │   └───────────────┘  │   when running on battery
                 │  per HW §7.3)│   └────────┬─────────┘                       ├──► OLED VCC
                 └──────────────┘            │                                ├──► TCA8418 VCC
                                             │ 100kΩ                           │
                     BAT_ADC (GPIO0) ◄───────┤ (divider midpoint:             └──► GND tied to all GNDs
                                             │  100kΩ to BAT+, 100kΩ to GND,
                                             │  100nF to GND; 10kΩ series at
                                             │  the ADC pin)
```

⚠ Phase-1 divider note: the SuperMini's 3V3 pin is produced by its onboard LDO from 5V/USB
when plugged in — that is fine while developing, **but the deep-sleep current numbers you
measure with USB power are not the final numbers**. Run the final sleep measurements from the
TPS63802 rail only (see HARDWARE.md §6 note on the LDO quiescent †).

## 3. Step-by-step breadboard build

1. **Board prep.** Insert the SuperMini across the breadboard center notch so both pin rows
   are usable. Confirm the USB-C port's edge. Add a 4xAA-style or bench harness clip so you
   can unplug USB without flexing the module.
2. **I2C bus first, nothing powered.** Wire GPIO8→SDA rail point, GPIO9→SCL rail point, and
   a common GND rail. With USB power only, plug in the **TCA8418 module** and check its VCC
   is 3.3 V at the module pins before seating the OLED.
3. **OLED.** Insert the SSD1309 carrier board (or solder the 4-pin header to the driver PCB
   that comes pre-attached to most "1.54″ OLED modules" — the raw COG glass itself has only
   an FPC tail; in Phase 1 use the module version with header pins). Wire VCC/GND/SDA/SCL.
   4-pin modules have no reset pin; on 7-pin modules pull **RES to 3V3** (directly or through
   a 10 kΩ resistor) — the firmware does **not** drive GPIO7. Handle the glass by its edges;
   peel the protective film only after step 10 passes.
4. **Keypad.** Connect your test keypad's rows/columns to the TCA8418 ROW/COL pins. The
   default `kMatrix[]` is built for the full 8×7 Casio grid, so a small keypad will not reach
   every action. Pick one:
   - **Preferred — a keypad with ≥ 5 rows × 7 columns**, wired 1:1 to ROW0–4 / COL0–6. The
     shipped `kMatrix[]` then works unchanged.
   - **4×4 keypad (most common cheap option)** wired to ROW0–3 / COL0–3: apply the **Phase-1
     key map** in §3a so `MODE`, `AI`, `=`, `DEL` and all ten digits are reachable.
   Photograph the exact row/col wiring — reversed matrices cause "ghost keys" that look like
   firmware bugs.
5. **BAT_ADC divider.** Solder or breadboard 100 kΩ from the battery + pad to a test point,
   100 kΩ from that point to GND, and 100 nF from the point to GND; run orange wire to GPIO0.
   Verify with a multimeter: test point ≈ Vbat/2.
6. **Power rail bring-up (USB only, battery disconnected).**
   1. USB → SuperMini: board boots, serial port appears, and the log prints
      `[i2c] scan: 0x34 0x3C` (TCA8418 + OLED).
   2. USB → charger module, **no cell yet**: the charge LED lights and the BAT pads read
      close to 4.2 V. With no load a linear charger can pulse, so treat this as a sanity
      check only — not a calibrated reading. Power down before touching the cell pads.
   3. Buck-boost module VIN from a *bench* 4.0 V (or a coin LiPo test cell): confirm VOUT is
      3.30 V, then watch for dips while you flash a "WiFi TX storm" sketch.
   4. Charge-current check — do this **after** the cell is connected in step 7: confirm
      ~20–50 mA into the cell (10–25 %C for a 200 mAh pack; see HARDWARE.md §7.3).
7. **Battery integration.** Protected cell → charger BAT → buck-boost VIN → 3V3 rail feeds
   the SuperMini 3V3 pin (bypassing its LDO), OLED, TCA8418. **Star-ground point: all GNDs
   meet at the battery negative pad** †. One wire mistake here and I²C gets flaky.
   Do **not** power the SuperMini from USB and the buck-boost 3V3 pin at the same time —
   use one source at a time.
8. **Display + bus check.** Flash the firmware (see GETTING_STARTED.md). The serial log prints
   `[i2c] scan:` with every responding address — you want **`0x3C`** (OLED) and **`0x34`**
   (TCA8418). Fix wiring (see §1/§6) before continuing if either is missing. The OLED should
   show the boot splash, and each key press logs a `[key] idx=... action=...` line (OLED
   address is 0x3C; some modules are 0x3D ⚠).
9. **Keypad→display→network loop.** Run through the smoke test in GETTING_STARTED.md §6.
10. **Mechanical honesty check.** Wiggle every jumper while running a 20-query test; a
    flaky Phase-1 wire costs an hour to find and a day to distrust later.

### 3a. Phase-1 key map for a 4×4 keypad

If you use a 4×4 keypad wired to ROW0–3 / COL0–3, edit `kMatrix[]` in
`firmware/src/hal/keymap.cpp` so the reachable indices carry the actions the smoke test
needs. On this wiring only indices `row*7 + col` for rows 0–3 and columns 0–3 exist:

| Key | Index | Action |
|---|---|---|
| R0C0 | 0 | `1` |
| R0C1 | 1 | `2` |
| R0C2 | 2 | `3` |
| R0C3 | 3 | `+` |
| R1C0 | 7 | `4` |
| R1C1 | 8 | `5` |
| R1C2 | 9 | `6` |
| R1C3 | 10 | `=` |
| R2C0 | 14 | `7` |
| R2C1 | 15 | `8` |
| R2C2 | 16 | `9` |
| R2C3 | 17 | `MODE` |
| R3C0 | 21 | `0` |
| R3C1 | 22 | `.` |
| R3C2 | 23 | `AI` |
| R3C3 | 24 | `DEL` |

This covers local `2+2=`, entry of the hotspot SSID/password, and the `MODE`/`AI` keys the
smoke test needs; it drops the scientific functions and non-essential operators, which is
fine for Phase 1. (The real Casio membrane is mapped properly in Phase 2 — see `HARDWARE.md §M4`.)

**Copy-paste.** Replace the first **four rows** of `kMatrix[]` in `firmware/src/hal/keymap.cpp`
with exactly this (leave rows 4–7 as they are):

```cpp
    // row 0
    (uint8_t)Action::D1, (uint8_t)Action::D2, (uint8_t)Action::D3,
    (uint8_t)Action::Add, (uint8_t)Action::None,
    (uint8_t)Action::None, (uint8_t)Action::None,
    // row 1
    (uint8_t)Action::D4, (uint8_t)Action::D5, (uint8_t)Action::D6,
    (uint8_t)Action::Eq, (uint8_t)Action::None,
    (uint8_t)Action::None, (uint8_t)Action::None,
    // row 2
    (uint8_t)Action::D7, (uint8_t)Action::D8, (uint8_t)Action::D9,
    (uint8_t)Action::Mode, (uint8_t)Action::None,
    (uint8_t)Action::None, (uint8_t)Action::None,
    // row 3
    (uint8_t)Action::D0, (uint8_t)Action::Dot, (uint8_t)Action::Ai,
    (uint8_t)Action::Del, (uint8_t)Action::None,
    (uint8_t)Action::None, (uint8_t)Action::None,
```

Columns 4–6 are not wired on a 4×4 pad, so their `None` entries are never reported. The
friendly names in the table above map to the enum like this: `1`→`D1`, `+`→`Add`, `=`→`Eq`,
`MODE`→`Mode`, `.`→`Dot`, `AI`→`Ai`, `DEL`→`Del`.

## 4. Antenna keep-out (matters even on the breadboard)

- The SuperMini's antenna is the **meander trace at the far end of the board from the USB
  connector** (varies by clone ⚠ — identify yours visually). Keep that end **up and outward**.
- **No wire bundle, battery, PCB copper, or hands within ~15 mm of the antenna end †.**
  The LiPo sitting in front of the antenna is the classic "why is RSSI −85 dBm today" bug.
- Route the 8+7 keypad ribbon away from the antenna end; the OLED (glass + metal tracks)
  also detunes if it lies in the radiating half-plane.
- Phase 3/4 consequence: the ESP32 module lands in the **corner of the case farthest from
  the battery pocket**, antenna edge toward the case side wall, with keep-out copper void on
  all layers under and beside the antenna (per the Espressif PCB design guidelines).
- Expect a real-case link loss of a few dB from ABS only (plastic is mostly transparent at
  2.4 GHz); your actual risk is battery/ribbon placement, not the case.

## 5. What must NOT be wired

- **No 5 V to any logic pin** (charger OUT goes to battery and to the TPS63802 — never to
  GPIO).
- **No pull-downs on GPIO2/GPIO8/GPIO9** — C3 strapping pins. I²C pull-ups (high at boot)
  are required by GPIO8's strap spec anyway; GPIO2 stays unconnected in this design.
- **No shared solderless-breadboard power rails for WiFi spikes.** Long breadboard rails add
  impedance; the ESP32 brownout ("RST was: BOR" boot reason) during WiFi TX almost always
  means weak power wiring, not a broken module. Keep 3V3/GND wires to the SuperMini short
  and thick (double them if needed †).

## 6. Quick fault map

| Symptom | First check |
|---|---|
| I²C scan finds nothing | SDA/SCL swapped? Pull-ups present? Module VCC really 3.3 V? |
| Boot scan shows only `0x34` (keypad, no OLED) | OLED straps to `0x3D` — set `DISPLAY_I2C_ADDR` to `0x3D` in `config.h` |
| Boot scan shows only `0x3C` (OLED, no keypad) | TCA8418 is fixed at `0x34`; check its SDA/SCL/VCC/GND and that the breakout actually wires the ROW/COL pins |
| OLED lights wrong columns / shifted | Panel is SH1106-class (132 segments) → set `DISPLAY_CONTROLLER` to `DISPLAY_CTRL_SH1106` in `config.h` |
| Keys work but wrong grid position | Row/col wire order reversed vs. the membrane's tails |
| Boot reason `BOR` / brownout during WiFi | Power wiring impedance (§5) and battery wire gauge |
