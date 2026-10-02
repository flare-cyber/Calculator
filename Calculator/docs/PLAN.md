# Build Plan — AI Calculator (Casio FX-300MS Plus + ESP32-C3)

This plan takes the project from an empty workbench to a finished unit that looks stock and
answers questions via DeepSeek. Five phases, each with a **gate**: do not start the next phase
until the acceptance criteria pass. Effort estimates are focused-person-time and are honest,
not optimistic.

> **Where we are today (2026-09-26).** The **software is written and compiles**, and all
> documentation exists. **No phase below has been built on real hardware yet**, so the project
> is at the very start of **Phase 1**. Overall status lives in the
> [README](README.md#current-state--whats-done-vs-whats-left).

```text
Phase 1          Phase 2              Phase 3            Phase 4            Phase 5
Breadboard ────► Casio teardown ────► In-case ─────────► Custom PCB ───────► Assembly
prototype        & measurement        prototype          design & fab       & polish
(external,       (data that gates    (real case,        (the product        (bezel, fit,
 safe)           what we buy)         real UI)          boards)             power, docs)
```

**Global rules**

- Every phase ends with a commit/tag and a short photo record in `docs/photos/phase-N/`.
- Anything discovered that contradicts this document gets written back into
  [HARDWARE.md](HARDWARE.md) / [RISKS.md](RISKS.md) the same day.
- The four caliper measurements (HARDWARE.md §M1–§M4) are the single biggest de-risking
  action in the project. Phase 2 exists to make them trustworthy.

---

## Phase 1 — Breadboard Prototype (proof of the whole loop)

**Goal:** ESP32-C3 + OLED + keypad + LiPo power chain + WiFi + DeepSeek all working on a
desk, outside any case. Nothing is glued, nothing is sacrificed (the Casio stays intact).

**Entry criteria:** BOM ordered (HARDWARE.md §2, prototype column); PlatformIO installed;
DeepSeek API key created.

### Steps

1. **Bench setup.** Flash blank firmware onto the ESP32-C3 SuperMini over USB-C; verify the
   native USB-Serial-JTAG port and serial monitor at 115200.
2. **Display bring-up.** Wire the 1.54″ SSD1309 OLED on I²C (`SDA=GPIO8`, `SCL=GPIO9`,
   address `0x3C`); run the U8g2 `fulltest` example; confirm whether the panel is SSD1309 or
   SH1106 (offset difference of 2 columns matters in software).
3. **Keypad bring-up (external first).** Use any membrane/keypad initially — do **not** open
   the Casio yet — wired to the TCA8418 (`ADDR=0x34`). Verify: key press → key event over
   I²C (polled by the firmware; the `/INT` line is unused in v1); measure debounce behaviour.
4. **Power chain.** Wire LiPo → protection → MCP73831 charge module → TPS63802 3.3 V module →
   board. Check: charge with USB-C, measure charge current at the PROG node, confirm 3.3 V
   rail under OLED-WiFi load, confirm battery voltage on `BAT_ADC=GPIO0` within ±100 mV of a
   multimeter.
5. **Network + AI loop.** Implement WiFi (2.4 GHz hotspot) connect, SNTP time sync, TLS to
   `api.deepseek.com`, streaming SSE parse, render first token to last on the OLED. This is
   the software contract in [SOFTWARE.md](SOFTWARE.md) §2 (task model) and §7 (API contract).
6. **Battery truth.** Measure actual currents for the states the firmware actually uses —
   awake idle, WiFi TX/TLS, active display — with a power profiler (Joulescope/PPK2/
   µCurrent-ish). Replace the matching † estimates in HARDWARE.md §7. (Deep/light-sleep
   currents cannot be measured until a sleep module exists — see the note at the end of this
   phase.)
7. **Soak test.** Scripted test: 100 sequential queries via a simulated keypad (or serial
   injection), heap logged every cycle, no reboots, no watchdogs, heap low-water stable.

### Milestones

| # | Milestone | Gate for |
|---|---|---|
| M1.1 | OLED shows test pattern | — |
| M1.2 | Keypad events appear on serial | — |
| M1.3 | First AI answer rendered (key press → streamed text) | M1.4 |
| M1.4 | Runs from battery ≥ 24 h with daily use | Phase 2 |
| M1.5 | 100-query soak passed; measured power table | Phase 2, 4 |

### Acceptance criteria

- [ ] From a key press, a streamed DeepSeek answer is fully rendered in **< 6 s** on a phone
      hotspot (LTE/5G tether), including WiFi+TLS setup.
- [ ] Device recovers automatically from: hotspot off, API 4xx/5xx, WiFi dropout mid-stream.
- [ ] Heap low-water after soak ≥ 30 % of free-at-boot (no creep).
- [ ] All Phase-1 learnings merged back into HARDWARE.md, SOFTWARE.md, RISKS.md.

> Sleep/wake power measurements (deep-sleep current, GPIO3 `/INT` wake) are **not** Phase 1:
> the firmware has no sleep implementation yet. They are tracked for Phase 3 (RISKS.md R-11).

**Exit gate decision:** if the answer pipeline is flaky (TLS/time, streaming, heap), fix it
here where it is free. Do not proceed to the case with an unstable loop.

---

## Phase 2 — Casio Disassembly & Measurement (data that gates purchases)

**Goal:** trustworthy mechanical and electrical data about the donor so the Phase 3/4 design
fits on paper before it fits on the bench.

**Entry criteria:** Phase 1 accepted; a donor unit + (recommended) a second donor for
practice; digital calipers; a spare afternoon.

### Steps

1. **Practice teardown** on the spare/cheap unit: 6 screws, shell separators, note screw
   lengths (they differ), watch the PCB edge under the display window.
2. **Full teardown of the real donor.** Bag and label screws. Photograph each stage.
3. **Take the four critical measurements** (§M1–M4 of HARDWARE.md): window aperture, cavity
   stack heights per zone, mounting/boss geometry + battery envelope, keypad matrix pitch and
   pad pattern. Log each as min/nom/max with a photo showing caliper placement.
4. **Confirm the matrix.** Count rows/columns with a DMM on the original PCB edge contacts or
   ribbon: expect 8×7 grid, 50 populated keys. Sketch the position→key map (which grid
   position is `7`, which is `AC`, etc.) and check for dual-pill keys (one key shorting two
   intersections) — the Casio MS series is known to fake extra keys this way.
5. **Display decision.** With M1 in hand, lock the display SKU (1.54″ SSD1309 if the window
   clears ~35 × 17.5 mm with bezel margin; otherwise the 1.3″ fallback). Order the chosen
   panel plus one spare now.
6. **Mock stack fit.** Without soldering: place OLED, a 0.8–1.0 mm PCB blank, LiPo, and the
   charge/boost module in the cavity using blu-tack; photograph side profile against a ruler;
   confirm no zone exceeds cavity height (5–7 mm assumed). Decide relief plan (which ribs get
   shaved, where the battery sits) if anything is tight.
7. **Window light path check.** Confirm the donor display window is clear plastic (it is on
   the Plus variant per research; verify — some FX-300MS variants have a printed mask or a
   solar cell directly behind the window). If a solar cell sits behind the window, plan to
   remove or mask it.

### Milestones

| # | Milestone |
|---|---|
| M2.1 | Teardown complete, nothing broken |
| M2.2 | Measurement sheet signed off (all four values + sketches in repo) |
| M2.3 | Display SKU locked and ordered |
| M2.4 | In-case stack mock fits with ≥ 0.5 mm clearance everywhere |

### Acceptance criteria

- [ ] M1–M4 recorded with photos; every number has min/nom/max and a "how measured" note.
- [ ] Keypad matrix confirmed 8×7/50 with a position→keyname table committed to
      `docs/keypad-map.md` (new file, or an appendix in HARDWARE.md).
- [ ] Documented decision (one paragraph) on display size, with margins to the bezel.
- [ ] A stacking table: OLED glass + FPC bend + PCB + tallest component + LiPo + clearance,
      all inside measured cavity numbers.

**Exit gate decision:** if the stack does not fit, choose now between (a) relieving the case,
(b) smaller display, (c) thinner battery (e.g. 10180 at lower capacity — re-run the power
budget). If none are acceptable, the "looks stock" requirement needs renegotiating before
Phase 4 money is spent.

---

## Phase 3 — In-Case Prototype (ugly but real)

**Goal:** a working device inside the actual Casio case using the SuperMini and jumper wires.
This is a fit-and-function rehearsal for the PCB, not a product build.

**Entry criteria:** Phase 2 accepted; display panel arrived; donor case prepped.

### Steps

1. **Display mount.** Print a 3D/PLA or resin (or cut from 0.5 mm sheet) bezel that locates
   the OLED glass under the window with the active area centered on the visible aperture;
   attach with double-sided tape; route the FPC to the logic zone with a bend radius ≥ 3×
   glass thickness; photograph legibility from straight-on and ~45° before committing.
2. **Keypad contacts.** Expose the 8×7 pad array on a scrap PCB or veroperf piece with
   carbon-pill contact surfaces (bare copper with flux residue, ENIG scrap, or conductive
   tape); press-fit the original silicone keypad over it using existing bosses; verify every
   one of the 50 keys makes reliably and releases cleanly.
3. **Wire it inside.** Move the Phase 1 system into the case: SuperMini on standoffs or
   tape, OLED harness, keypad harness, LiPo in its final pocket, USB-C module at the original
   charging-port location or a cut edge for now.
4. **UI on the real keypad.** Port the serial key-injector flow to the real 50-key map
   (SOFTWARE.md §4–§5): typing, editing with `DEL`/`AC`, `=` to evaluate locally, `AI` to ask,
   function keys (`sin`, `log`, `√` etc. as text), scroll of long answers.
5. **Power polish in firmware (stretch — not in v1 firmware).** Idle → light sleep (≤ 5 min)
   → deep sleep (with TCA8418 `/INT` on GPIO3 as the wake source — requires adding the INT
   wiring and an `esp_sleep` module to the firmware); verify wake→first-render latency.
6. **Stress the fit.** Close the case with all 6 screws; flex the case; re-test every key and
   the display; check for light bleed around the OLED bezel in a dark room.

### Milestones

| # | Milestone |
|---|---|
| M3.1 | Display legible through the stock window with the case closed |
| M3.2 | All 50 keys functional inside the case |
| M3.3 | Full user flow works on battery, in case, 24 h of normal use |

### Acceptance criteria

- [ ] Case closes with no bulge and no visible gap; screws all seat.
- [ ] Worst-case typing session (30 edits + 5 AI queries) with zero missed/doubled keys.
- [ ] Key-to-answer-render < 6 s on hotspot from awake.
- [ ] *(Conditional — only if the Phase 3 sleep stretch is implemented)* deep-sleep wake→render
      < 9 s and standby drain ≤ 120 µA †; measured in the final config (update HARDWARE.md §7).
- [ ] A written list of every thing the custom PCB must improve (routing, mounts, connectors).

**Exit gate decision:** any acceptance failure that is *mechanical* is fixed by re-designing
the mount/contact in this phase. Any failure that is *electrical* (e.g. I²C integrity with
long wires) is an explicit board requirement handed to Phase 4.

---

## Phase 4 — Custom PCB Design

**Goal:** one small board that integrates MCU module, display connector, keypad pads, power
chain, and USB-C — designed to the Phase 2 numbers and Phase 3 lessons.

**Entry criteria:** Phase 3 accepted; locked display SKU; locked battery placement;
schematic symbols/footprints available for all BOM parts.

### Steps

1. **Schematic** from HARDWARE.md: ESP32-C3-MINI-1 (antenna variant decided after Phase 3
   in-case RF tests — PCB antenna if the hotspot links reliably inside the closed case,
   otherwise an edge-positioned/external antenna so it can sit away from the battery), I²C
   bus with test points, TCA8418, OLED FPC connector (match the chosen panel's tail), MCP73831 + DW01A/FS8205A + TPS63802, USB-C
   with CC pulldowns (5.1 kΩ) and ESD protection, battery connector (JST-PH 1.25 or solder
   pads with strain relief), BOOT + RESET buttons, UART pads for a debug probe.
2. **Keep the Phase 3 contract**: same GPIO map as WIRING.md (SDA=8, SCL=9, BAT_ADC=0;
   GPIO3 reserved for a future TCA8418 `/INT` wake, GPIO7 for a future OLED RST). Strapping
   rules respected: GPIO8/9 pull-ups satisfy the high-at-boot requirement; GPIO2 left
   unconnected or pulled high only; the BAT_ADC divider is high-Z enough not to fight boot
   straps (GPIO0 is not a strap pin — fine).
3. **Layout to the mechanical drawing:** board outline from §M3 boss/screw geometry, keypad
   pad array from §M4 pitch, edge connector or FPC route under the display zone, antenna
   keep-out ≥ 15 mm from battery, copper and shellings, no copper under the antenna on every
   layer.
4. **DFM sanity:** 2-layer prototype run is acceptable for a first spin if trace/space is
   kept conservative; send to the fab's web ERC (JLCPCB/SFPCB/Pertog) before ordering.
5. **Order:** 5 boards minimum (one spin will be re-spun at least once); stencil + assembly
   of the hard hand-solder parts (QFN TPS63802, FPC connector) or pick-and-place the reel
   parts at the fab if the BOM maps to their library.
6. **Board bring-up:** power rail first (no MCU), then I²C scan, then USB flashing, then
   same acceptance flow as Phase 1 step 7 (soak).
7. **Design spin rule:** any required re-spin is allowed exactly once in the schedule; a
   second re-spin triggers a scope review (RISKS.md R-10).

### Milestones

| # | Milestone |
|---|---|
| M4.1 | Schematic + layout reviewed against M1–M4 numbers |
| M4.2 | Boards received, first population done |
| M4.3 | Board passes the Phase 1 soak suite unchanged in behaviour |

### Acceptance criteria

- [ ] Board physically fits and screws to donor bosses; keypad contacts align within ± 0.3 mm
      (measured, not assumed).
- [ ] 3.3 V rail holds ≥ 3.0 V during WiFi TX bursts at 3.5 V battery.
- [ ] *(Requires the Phase 3 sleep module)* Deep-sleep on the custom board (module-level, no
      SuperMini LDO overhead) ≤ 60 µA whole device including TCA8418 scanning † → re-measured
      and logged.
- [ ] USB-C charge and data-flashes work from the populated board alone.
- [ ] BOM cost per unit ≤ target in HARDWARE.md §2 (production column).

---

## Phase 5 — Assembly & Polish

**Goal:** finish the object: mounts, light sealing, UX polish, reliability, documentation of
the actual build.

**Entry criteria:** Phase 4 board accepted; Phase 2/3 mounts proven.

### Steps

1. **Final assembly:** PCB to bosses, LiPo to its pocket with Kapton/frame protection,
   display to bezel, keypad seated, shells closed, screws torqued by hand.
2. **Optical clean-up:** light-blocking paint/tape on bezel seams if night-mode bleed shows;
   anti-scratch film on the window if the donor's is hazy.
3. **Firmware polish:** boot logo matching Casio style, font sizes tuned against real window
   legibility, error copy (WiFi fail / API fail / low battery), low-battery charge warning at
   the OCV threshold chosen in HARDWARE.md §7.3 (mirrored in SOFTWARE.md §9), time-stamped
   heap/current log mode behind a key combo.
4. **Reliability pass:** 500-query soak, 100 deep-sleep/wake cycles (requires the Phase 3
   sleep module), drop check (it is a pocket object: 1 m onto a carpet + a 50 cm table drop,
   then re-test), charge while powered on test.
5. **Battery endurance field test:** calendar usage for one week at the planned query rate;
   extrapolate; compare with the power budget and fix the doc with real numbers.
6. **Documentation of the real build:** photos, as-built BOM diffs, measurement sheet, any
   deviation from this plan — update this repo's docs so the next builder can repeat it.

### Milestones

| # | Milestone |
|---|---|
| M5.1 | Assembled, closed, boots to HOME, answers queries |
| M5.2 | Reliability pass complete |
| M5.3 | Week-long field test complete, power numbers real |

### Acceptance criteria (project definition of done)

- [ ] A stranger looking at the closed device cannot tell it is not a calculator at a glance
      (subjective, but photograph and judge honestly).
- [ ] All 50 keys, all UI states, recovery from every error path, wake-on-keypress, and USB-C
      charging verified in the final assembly.
- [ ] Battery life at 20 queries/day ≥ 3 weeks without charging † → replaced by the measured
      value.
- [ ] `docs/` is updated with as-built data; GETTING_STARTED.md reproduces a working flash
      from a clean checkout.
- [ ] Repository tagged `v1.0`.

---

## Summary timeline

| Phase | Focus | Rough effort | Key risk retired |
|---|---|---|---|
| 1 | Breadboard prototype | 3–6 evenings | API/TLS/streaming, heap, battery-current reality |
| 2 | Teardown & measurement | 1–2 days | Display fit, cavity stack, matrix unknowns |
| 3 | In-case prototype | 4–8 days | Fit, keypad contacts, real UX, in-case power |
| 4 | Custom PCB | 1–2 weeks + fab lead time | Board-level integration, antenna, assembly |
| 5 | Assembly & polish | 3–5 days | Reliability, endurance, stock look |

**Critical path:** Phase 1 software stability → Phase 2 measurement gate → display SKU lead
time → PCB fab lead time. Order the display the day Phase 2 closes.
