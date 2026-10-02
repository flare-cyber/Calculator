# Hardware Reference — AI Calculator

Everything you buy, measure, and wire is specified here. Read §5 (the four caliper
measurements) **before ordering the display or the PCB** — those parts are gated on real
numbers from your donor unit.

Conventions: **†** = design estimate, not measured. **⚠ verify** = re-check this item before
spending money or finalizing a layout. Prices are street prices observed 2026-09 (USD,
single-unit unless noted) and will drift.

---

## 1. System block diagram

```text
                        ┌────────────────────────────────────────────────┐
                        │                Casio FX-300MS+                 │
                        │                                                │
 USB-C 5V ──► MCP73831 charger ──► LiPo LP402030 ──► TPS63802 ──► 3V3 rail
 (charge      (10–25 %C †)     ◄── DW01A + FS8205A    (buck-boost)    │
  only)                     (protect: OVP/OCP/UVP)                    │
                                                                      ├──► ESP32-C3 (MINI-1 / SuperMini)
   ┌───────────────┐   I²C (SDA=8, SCL=9)   ┌────────────────────┐    │
   │ 1.54″ OLED    │ ◄──────────────────────► │ TCA8418 keypad    │◄──── Casio silicone 8×7 matrix
   │ SSD1309 128×64│                          │ controller 0x34   │      (50 keys, carbon pills)
   └───────────────┘                          └────────────────────┘      (polled over I²C — no /INT in v1)
```

WiFi (2.4 GHz) → phone hotspot → HTTPS → `api.deepseek.com` (see [SOFTWARE.md §7](SOFTWARE.md)).

---

## 2. Bill of Materials

### 2.1 Core electronics

| Ref | Item | Part number / spec | Key dimensions (nom.) | Qty | Price † | Source |
|---|---|---|---|---|---|---|
| U1 | MCU module — prototype | **ESP32-C3 SuperMini** (ESP32-C3-S3-style clone, RISC-V, 4 MB flash) | ≈ 22 × 17 mm (varies by clone ⚠) | 1 | $3–6 | AliExpress/Amazon; chip datasheet: [espressif.com → ESP32-C3](https://www.espressif.com/en/products/socs/esp32-c3) |
| U1f | MCU module — final PCB | **Espressif ESP32-C3-MINI-1-H4** (4 MB flash, onboard PCB antenna). If in-case RF testing shows weak hotspot signal, evaluate an external-antenna variant — confirm available antenna suffixes and the exact footprint from the datasheet ⚠ before ordering | small LGA module, ~16 × 16 mm class ⚠ verify from datasheet | 1 | $2.5–4 | Digi-Key/Mouser/LCSC; [module datasheet PDF (verify link)](https://www.espressif.com/sites/default/files/documentation/esp32-c3-mini-1_datasheet_en.pdf) |
| U2 | Display — primary | **1.54″ 128×64 COG OLED, SSD1309 (some batches SH1106), I²C 4-pin** (e.g. BuyDisplay MTDO-154S6B / generic "1.54 inch SSD1309 I2C") | active ≈ **35.05 × 17.51 mm**; glass ≈ 40 × 20 mm ⚠ verify per listing | 2 (1 spare) | $6–10 | AliExpress/BuyDisplay/Mouser |
| U2a | Display — fallback 1.3″ | **Winstar WEO012864AH** (1.3″ 128×64 OLED module, SSD1306-class, I²C/SPI) — closest module-format fallback; matches the 1.3″ envelope ≈ **29.4 × 14.2 mm** active | per Winstar drawing | 1 (if M1 says so) | $12–18 | [winstar.com.tw](https://www.winstar.com.tw) — search `WEO012864AH`; distributors: Mouser/Digi-Key |
| U2b | Display — fallback 0.96″ | Generic **0.96″ 128×64 SSD1306** I²C module | active ≈ 21.7 × 10.9 mm ⚠ | 1 | $3–5 | any hobby vendor |
| U2c | Display — ultra-low-power exotic | **Sharp LS013B7DH03** 1.3″ 128×128 Memory LCD (SPI, reflective, ~no refresh power). **Caveat: square 128×128 format almost certainly does NOT fit a wide 35 × 17.5 mm window** — listed for completeness/plan-B experiments only ⚠ verify outline vs M1 | ≈ 1.3″ square ⚠ | 0 | $15–25 | Digi-Key/Mouser, search `LS013B7DH03` |
| U3 | Keypad controller | **TI TCA8418** (low-power I²C keypad matrix scanner). Scans our **8×7 = 56-position grid (50 keys)**; chip matrix capacity per TI datasheet is larger than 8×7 ⚠ confirm exact row/col pin budget from the datasheet before PCB layout | 24-pin class; breakout fine for proto | 1 | IC $1–2; module $3–6 | [ti.com/product/TCA8418](https://www.ti.com/product/TCA8418); breakouts on AliExpress (search "TCA8418 module") |
| U4 | LiPo charger | **Microchip MCP73831T-…/MC** (single-cell 500 mA linear charger, SOT-23-5). Pick the current-limited USB-safe variant marking per datasheet ⚠ | 2.9 × 1.6 mm | 1 | $0.6–1 | [microchip.com MCP73831 page (verify path)](https://www.microchip.com/en-us/product/MCP73831); Digi-Key/Mouser/LCSC |
| U5 | Buck-boost 3.3 V | **TI TPS63802** (high-efficiency single-inductor buck-boost, tiny QFN) | 2 × 2.5 mm class QFN ⚠ verify package code | 1 | $4–7 | [ti.com/product/TPS63802](https://www.ti.com/product/TPS63802) |
| L1 | Inductor for U5 | Per TPS63802 datasheet app design (≈ 1 µH, ≥ 2 A Sat, low DCR, e.g. TDK SPM/CTL series) ⚠ value from datasheet | 2.5–4 mm class | 2 | $0.5 | Digi-Key/Mouser |
| U6 | Battery protection | **DW01A + FS8205A** pair (single-cell OVP/UVP/OCP module; buy as pre-made PCB strip for proto, discrete for final) | strip ≈ 15 × 6 mm | 1 | $0.5–2 | AliExpress ("1S protection board DW01A FS8205A") |
| B1 | Battery | **LiPo 200 mAh, cell code LP402030** (4.0 × 20 × 30 mm), protected, JST-PH 1.25 or solder tabs | 4.0 × 20 × 30 mm | 2 (1 spare) | $3–6 | AliExpress/BatteryBrothers-type vendors ⚠ verify real capacity reviews |
| J1 | USB-C connector | Mid-mount SMT USB-C receptacle (16-pin power/data; data pins may be unconnected — charging only) | 8.94 × 7.4 mm footprint ⚠ pick per layout | 1 | $0.3 | LCSC/Digi-Key |
| D1 | USB ESD | **TI TPD4E05U06DPLR** (or equivalent 4-line ESD array on USB pairs) ⚠ package verify | 1 × 0.6 mm class | 1 | $0.2 | [ti.com/product/TPD4E05U06](https://www.ti.com/product/TPD4E05U06) |
| R1,R2 | USB-C CC pulldowns | 5.1 kΩ, 0402 (request 500 mA from any source) | 0402 | 2 | <$0.1 | any |
| R3,R4 | Battery divider for `BAT_ADC` | 2 × 100 kΩ, 0402/0603, 0.1 % preferred (~21 µA @ 4.2 V) ⚠ add 100 nF + 10 kΩ series near ADC pin | 0402 | — | <$0.1 | any |
| R5,R6 | I²C pull-ups | Verify modules' onboard pull-ups first (most OLED + TCA8418 boards have them; final PCB: 4.7 kΩ to 3V3) | 0402 | 4 | <$0.1 | any |
| S1,S2 | BOOT / RESET tact switches | 3 × 4 mm SMT tact | 3.2 × 1.6 mm pad | 2 | $0.1 | any |
| — | Passives | Decoupling per datasheets (0.1 µF × several, 4.7–22 µF bulk × 2), LED (optional status) | — | lot | $5 | any |
| PC1 | Final PCB | 2-layer FR4, 1.0–1.2 mm, per Phase 2 outline; ENIG recommended if keypad pills contact it directly ⚠ | ≈ 70 × 55 mm envelope max ⚠ | 5 | $20–40 setup | JLCPCB/PcbWay/SFPCB |
| M | Hardware | M2 screws (donor uses shell screws — reuse), double-sided tape, Kapton, 0.5 mm PET or printed bezel | — | lot | $5 | any |

### 2.2 Prototype-only items (Phase 1)

| Item | Purpose | Price † |
|---|---|---|
| Small solderless breadboard + jumper wires | Phase 1 | $8 |
| **3.3 V buck-boost module** (e.g. TI TPS63020/TPS63060 breakout, or an Adafruit/Pololu 3.3 V buck-boost). ⚠ A plain LDO (AMS1117 etc.) only works while USB-powered — it cannot hold 3.3 V once a LiPo drops below ~3.5 V, so it is not a substitute for the battery run | Phase 1 power | $4–8 |
| **1-cell LiPo charger module** (MCP73831 or TP4056 with USB input; TP4056 boards usually include DW01A+FS8205A protection) | Phase 1 charging (+ protection) | $2–3 |
| **Membrane keypad** — ≥ 5×7 to match the default `kMatrix[]`, or a 4×4 with the Phase-1 key map ([WIRING.md §3a](WIRING.md)) | TCA8418 testing **without opening the Casio** | $2–5 |
| **TCA8418 breakout** — check it actually fans out the ROW/COL pins you need (some route only a subset) | keypad controller | $3–6 |
| Joulescope/PPK2-class µA profiler (borrow if possible) | measured power budget | $50–120 (optional) |
| Digital calipers (0.01 mm) | Phase 2 — **mandatory** | $15–30 |
| Spare donor **Casio FX-300MS Plus** (for practice teardown) | Phase 2 | $10–15 |

> **Sourcing note.** Pre-made *TPS63802* modules are rare (it's a tiny QFN); for Phase 1 **any**
> 3.3 V buck-boost module works — you only need the exact TPS63802 on the final PCB. Likewise,
> any single-cell LiPo charger module (TP4056 **or** MCP73831) is fine on the bench. The one
> part to match exactly is the **TCA8418** (or a breakout that fans out enough ROW/COL pins).

### 2.3 Cost summary †

| Scope | Estimate |
|---|---|
| Phase 1 prototype (excluding tools & donor) | ≈ **$35–55** |
| Phase 2–3 additions (display, spares, printing, hardware) | ≈ **$25–40** |
| Phase 4 PCB batch (5 boards + stencil + components) | ≈ **$60–120** |
| **Per finished unit** (amortized boards) | ≈ **$35–50** |

### 2.4 Display option comparison

| Option | Active area | Interface | Fit verdict |
|---|---|---|---|
| **1.54″ SSD1309 COG** (primary) | ≈ 35.05 × 17.51 mm | I²C | Best match to a ~35 × 17.5 mm window *if M1 confirms clearance*; COG = bare glass + FPC tail, needs a small driver carrier or direct 4-pin FPC connector |
| 1.3″ Winstar **WEO012864AH** (fallback A) | ≈ 29.4 × 14.2 mm class | I²C/SPI | Module format is easier to mount; slightly smaller text area |
| 0.96″ SSD1306 (fallback B) | ≈ 21.7 × 10.9 mm ⚠ | I²C | Last resort; cramped for prose answers |
| Sharp **LS013B7DH03** (exotic) | ~1.3″ **square** 128×128 | SPI | Ultra-low power and gorgeous in sunlight, but square format breaks the "behind the wide window" concept — not recommended unless M1 surprises us |

**Rule:** the display is ordered only after §M1 is measured. ⚠

---

## 3. Donor unit (Casio FX-300MS Plus) — what we know

| Property | Value | Source/confidence |
|---|---|---|
| Overall size | ≈ 155–162 × 77–85 × 10–12.2 mm (varies with how you measure the keys) | research; per-unit variance expected — measure yours |
| Construction | Two ABS shells, **6 screws**, snap lips | research; confirm screw count on your unit ⚠ |
| Keypad | Silicone sheet with carbon pills, **8×7 grid of positions (56), 50 keys** | research (MS-series family); confirm in Phase 2 |
| Internal cavity behind keypad | ≈ 5–7 mm | research assumption — **measure (§M2)** |
| Display window | Replacement display must fit *behind* it | **measure (§M1)** |
| Original power | Battery bay (button/coin format per family) + solar cell behind display on some variants | confirm in Phase 2 |

## 4. What the new stack must fit into

```text
      top view (keypad removed)                 side view (not to scale)
 ┌──────────────────────────────┐          shell lid ─┬─ keypad
 │  [OLED glass]   (screws o o) │          │  bezel   │
 │                              │          │  PCB 1.2 │  ← cavity ≈ 5–7 mm †
 │  (o) ESP32-C3-MINI-1   USB-C ╞═╡        │  LiPo    │     per zone (§M2)
 │  charger  boost  protect     │          shell base ┴
 └──────────────────────────────┘
```

Zone-by-zone stack budget (to be finalized from §M2/§M3 data):
`OLED glass (~1 mm) + FPC bend allowance (~2 mm arc, 3× thickness rule) + PCB 1.2 mm +
tallest SMD (~2.5 mm over TPS63802/L1, ~3.4 mm over the MCU module) + LiPo 4.0 mm + tape`.
This is why the battery and the tall supplies live **below the keypad zone** (deepest), the
OLED lives **directly under the window** (shallowest), and the LiPo goes in the largest
continuous pocket — validated by the Phase 2 mock-fit (PLAN.md step 2.6).

---

## 5. THE FOUR CRITICAL CALIPER MEASUREMENTS (take before ordering display/PCB)

Record every value as min/nom/max over 3 measurement passes, with a photo of the calipers in
place. These gate purchases; do not eyeball them.

### §M1 — Display window aperture and visible area
- **What:** width × height of the *inner opening* of the window (the clear area not covered
  by the bezel lip), plus the depth from the lid outer surface to the underside of the
  window, plus the flat glass area available inside.
- **How:** caliper inside-jaws into the aperture; depth gauge rod to the recess floor;
  also measure the old LCD module's glass and active area as a "what Casio thinks fits"
  reference.
- **Decision rule:** order the 1.54″ panel only if active area (35.05 × 17.51 mm) clears the
  aperture **minus 0.8 mm all-round bezel margin**. Otherwise 1.3″ (≈ 29.4 × 14.2 mm), else
  0.96″. Log the subtraction in the measurement sheet.

### §M2 — Cavity stack height per zone
- **What:** internal height between shell-floor bosses and the lid at (a) under the display
  window, (b) centre keypad zone, (c) battery bay, (d) any rib/screw tower in your routing
  path.
- **How:** outside-jaws/depth rod at each zone with the shell separated; or a calibrated
  paper-gauge strip where the rod won't reach. Measure with the silicone keypad *out* and
  note the keypad's own compressed thickness (~1.5–2 mm) separately.
- **Decision rule:** usable height = measured − 0.5 mm safety. If no ≥ 5.5 mm zone exists
  for the power/battery stack, decide: relieve ribs, thinner cell (e.g. 10180-size, lower
  mAh — re-run §6), or thinner PCB (0.8 mm).

### §M3 — Mounting geometry + battery envelope
- **What:** all 6 shell screw hole positions relative to a corner (x, y) and hole diameters;
  board boss locations and heights; the **maximum rectangular battery envelope**
  L × W × T in the chosen pocket (after ribs); position of the original PCB edge slots.
- **How:** photograph the base shell on a printed mm grid; caliper each boss; try-fit the
  LP402030 in the pocket and measure clearance to every wall/rib.
- **Decision rule:** board outline is derived from screw/boss coordinates (± 0.3 mm);
  battery pocket needs ≥ 0.4 mm free on the largest face for tape/compression relief.

### §M4 — Keypad matrix pitch and pad pattern
- **What:** centre-to-centre pitch of the carbon-pill grid (rows and columns), pill diameter,
  count of rows × columns (expect 8 × 7 = 56 positions, 50 populated), and which positions
  are *dual-pill* keys (one key pressing two intersections — Casio's trick for "2-shift"
  functions). Also the pad geometry the original PCB uses (round/tentative ring, exposure of
  solder mask).
- **How:** calipers across 10 pitches and divide (averaging reduces error 10×); DMM
  continuity between the original PCB trace stubs / ribbon tails while pressing keys to map
  position → key; photograph under raking light.
- **Decision rule:** the custom PCB contact array uses the measured pitch (± 0.3 mm max
  misregistration across the array) and ≥ pill-diameter pad overlap; record the dual-pill
  positions — SOFTWARE.md maps them as composite keys.

---

## 6. ESP32-C3 ↔ peripherals pinout (the firmware/WIRING contract)

Identical on SuperMini (prototype) and C3-MINI-1 (final). ⚠ Do not "temporarily" move pins —
docs, firmware and PCB all assume this table.

| ESP32-C3 GPIO | Peripheral | Signal | Type | Notes |
|---|---|---|---|---|
| **GPIO8** | OLED + TCA8418 | I²C **SDA** | OD/bidir | Strapping pin: keep high at boot — the bus pull-ups do this for free. Default Wire SDA on C3 board defs |
| **GPIO9** | OLED + TCA8418 | I²C **SCL** | OD | Strapping-adjacent: GPIO9 low at boot redirects console to UART0; pull-ups keep it high, USB-JTAG stays default |
| **GPIO0** | Battery divider | **BAT_ADC** | analog | 2×100 kΩ ÷2 → ~0–2.1 V for 0–4.2 V; `analogSetPinAttenuation(ADC_11db)`; not a strap pin on C3 |
| 5V (pin) | charger feed | VBUS | power | On SuperMini: USB 5 V passthrough → charger module |
| 3V3 (pin) | rail | 3.3 V | power | From TPS63802 in final; SuperMini onboard LDO in Phase 1 only (its LDO quiescent is † ~60–100 µA — module-level sleep numbers assume direct 3V3) |
| GND | — | GND | power | Star at battery for high-current loops |
| GPIO18/19 | USB-Serial-JTAG | D−/D+ | USB | Native flashing/monitor — no bridge chip |
| GPIO20/21 | UART0 | RX/TX | debug | Secondary console / logs when USB is used for charging only |
| **GPIO1,3,7** | free | — | — | GPIO3 reserved for the planned TCA8418 `/INT` wake (RISKS.md R-11); GPIO7 reserved for a planned OLED RST. Not used by the current firmware. |
| **GPIO2,4–6,10** | free | — | — | Reserved for: GPIO2 = strap (do not pull low); extras like a status LED. Direct-GPIO-scan fallback would need 15 pins (8 row + 7 col) and is why the TCA8418 is recommended |

**I²C addresses:** OLED `0x3C` (commonly; some modules jumper to `0x3D`), TCA8418 `0x34`
(check your part marking ⚠). The firmware scans the bus at boot and logs
`[i2c] scan: 0x34 0x3C`; if a device is missing, fix the wiring before flashing further.

**Logic levels:** everything on this design is 3.3 V (ESP32-C3, SSD1309, TCA8418,
MCP73831/TPS63802 logic). **No level shifters anywhere.** 5 V exists only at the USB VBUS
→ charger path.

---

## 7. Power budget (design estimates † — replace after Phase 1 step 6)

### 7.1 Per-rail current estimates †

| State | ESP32-C3 | OLED (SSD1309) | TCA8418 | Regulator+misc | Total |
|---|---|---|---|---|---|
| Deep sleep (INT-wake armed) | ~30 µA (RTC timer+wake enabled; ~5–10 µA bare-chip sleep, datasheet) | off (RST held, or power-gated) | ~2–10 µA standby (scan off per config) | TPS63802 Iq + divider + board, ~25 µA | **≈ 60–70 µA** (target — not implemented yet) |
| Light sleep (CPU off, WiFi keep-alive off) | ~0.5–1 mA | off | ~10–100 µA scanning | ~5 µA + | **≈ 0.6–1.1 mA** (target — not implemented yet) |
| Awake idle (display on, typing) | ~5–15 mA modem-sleep avg (WiFi assoc kept) | 0.5–2 mA | scanning | — | **≈ 8–18 mA** (current firmware behaviour) |
| WiFi TX bursts (connect+TLS+stream, ~10 s/query) | avg 115–240 mA; **peaks ~350–370 mA** during TX ramp | ~1–2 mA | — | TPS63802 ~85–90 % eff | **≈ 130–260 mA avg / 400 mA peak design target** |

⚠ All semiconductor figures are datasheet-class estimates; battery-life claims are invalid
until measured (Phase 1 M1.5). The regulator must supply ≥ 500 mA transient headroom † →
bulk capacitance sized from TPS63802 datasheet + bench scope check at 3.4 V cell.

### 7.2 Runtime on the LP402030 200 mAh †

Usable energy assumption: **170 mAh** (200 mAh nominal derated for regulator losses, cutoff
margin, protection circuit, and cell age †). Per-query energy: ~10 s active at ~200 mA †
≈ 0.56 mAh per query (WiFi reconnect + TLS dominates; keep-alive reuse halves it).

| Scenario | Duty | Est. average current † | Est. runtime † |
|---|---|---|---|
| Pure standby (never used) | deep sleep 100 % | ~65 µA | **~10–11 weeks** (also bounded by cell self-discharge) |
| Light use | 20 queries/day + 5 min typing/day | ~0.7 mA | **~3–4 months** (self-discharge starts to dominate) |
| Normal use | 60 queries/day | ~2.2 mA | **~6–7 weeks** |
| Heavy use | 150 queries/day | ~5.4 mA | **~1 month** |
| Continuous streaming + WiFi TX | 100 % active | ~260 mA | **~40 min** |
| Charge time from empty | 20 mA (0.1C) / 50 mA (0.25C) | — | ~12 h / ~5 h (CV tail included †) |

### 7.3 Charger design notes

- **MCP73831:** set charge current via the PROG resistor to **~20–50 mA** (10–25 %C for a
  200 mAh cell; keeps the linear charger cool inside a sealed case). ⚠ Use the datasheet's
  PROG-resistor equation/table for your exact marking — the coefficient varies across the
  family and this document deliberately does not guess it.
- **USB-C:** 5.1 kΩ Rd pulldowns on both CC pins → we are always a 500 mA *device*, which
  the worst-case 400 mA peak † fits with margin.
- **DW01A/FS8205A:** provides over-charge (~4.25–4.3 V †), over-discharge (~2.3–2.5 V †) and
  over-current protection; **this is mandatory** — a LiPo inside a pocket product with no
  protection is a non-starter (RISKS.md R-09). Firmware *additionally* warns at 3.45 V and
  force-deep-sleeps at 3.30 V measured at the cell (OCV table in SOFTWARE.md §9) so the IC's
  hard cutoff is never the user's last experience.

---

## 8. Mechanical / assembly notes

- **ABS + no metal:** the case is non-conductive; WiFi survives closing the lid, but the
  SuperMini/MINI-1 antenna must keep clear of the 200 mAh cell (§WIRING antenna keep-out).
- **COG OLED handling:** the 1.54″ panel is bare glass with an FPC tail. Minimum bend radius
  at the tail ≈ 3× glass thickness †; support the glass on a rigid printed bezel; never glue
  the glass face — tape around the perimeter only.
- **Keypad contact:** pills press directly onto PCB pads. Use ENIG or hard gold-finger style
  finish if affordable; HASL works but wears (RISKS.md R-05). Clean pills with IPA at build.
- **Screws:** six, likely mixed lengths — bag per position with a photo map (Phase 2).
- **USB-C position:** put it where the donor's charging port is (the FX-300MS Plus variant
  has a USB charging port per research — verify location/size during teardown ⚠ and match
  the cutout precisely; this is the one opening you can't hide).
