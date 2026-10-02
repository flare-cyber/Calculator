# Risk Register — AI Calculator

Risk IDs are referenced from the other documents (e.g. [README](../README.md),
[HARDWARE.md](HARDWARE.md), [PLAN.md](PLAN.md)) — do not renumber, only close or add.

## 1. Method

- **Likelihood (L):** 1 Low · 2 Medium · 3 High (before mitigation, honest).
- **Impact (I):** 1 Minor · 2 Major · 3 Project-killing.
- **Score = L × I.** ≥ 6 critical (act now), 3–5 monitor (re-check at each phase gate),
  ≤ 2 accept.
- Every risk needs: a mitigation (reduce L or I), an early-warning trigger (something you
  can *observe*, ideally a log line or measurement), and a contingency (what we do if it
  lands anyway).
- Review points: end of every PLAN.md phase; the register is updated in the same commit as
  phase acceptance.

## 2. Register

### Mechanical / hardware

| ID | Risk | L | I | S | Mitigation (where) | Early warning | Contingency |
|---|---|---|---|---|---|---|---|
| **R-01** | **Display fit mismatch** — 1.54″ active area (≈35.05 × 17.51 mm) doesn't clear the window minus bezel lip; light cut off, or panel doesn't seat | 3 | 3 | **9** | Measurement gate §M1 **before** ordering (PLAN Phase 2); documented fallback chain 1.54″→1.3″(WEO012864AH)→0.96″ (HARDWARE §2.4) | Caliper sheet shows aperture − 0.8 mm margin < active size | Order 1.3″ Winstar module instead; worst case keep 1.54″ but center on visible window and accept slight text-margin crop (font/window check in Phase 3 photo) |
| **R-02** | **Cavity stack too short** — OLED+PCB+LiPo+connectors exceed the assumed 5–7 mm; case won't close or keys bind | 3 | 3 | **9** | §M2 per-zone depths + §M3 battery envelope; blu-tack mock fit before any soldering (PLAN Phase 2 step 6); zone-by-zone stack budget (HARDWARE §4) | Mock fit needs > 0.5 mm relief anywhere; lid flex when screwed | Reorder stack: 0.8 mm PCB, direct-FPC (no carrier), move charge/protect ICs to dead space beside battery bay; last resort thinner cell (lower mAh → re-run HARDWARE §7.2) — "looks stock" renegotiation is a documented project decision, not a silent squeeze |
| **R-03** | **Keypad matrix surprises** — donor is not exactly 8×7/50; some keys use dual carbon pills (two intersections per key), or shared-column diode-equivalent patterns | 3 | 2 | 6 | Phase 2 §M4: DMM-mapped position→keyname table *and* dual-pill detection before `keymap.cpp` is frozen; TCA8418 scans the physical grid as-is | Continuity test shows one key shorting two grid cells; phantom keypresses in Phase 3 step 4 | keymap layer collapses known dual-pill pairs into one logical key (software merge within 30 ms window); if grid ≠ 8×7, re-verify TCA8418 config against its datasheet (HARDWARE §2.1 ⚠ already flagged) |
| **R-04** | **Keypad ghosting / multi-key artefacts** — carbon-pill matrix has no diodes; two+ pills bridging (case flex, debris, sweaty pills) create false third-key events | 3 | 2 | 6 | Single-keypress UI model (keypad event queue, no rollover); 12 ms debounce + 120 ms repeat + 600 ms long-press in `config.h` | Ghosting rate measured in Phase 3 acceptance ("30 edits + 5 queries with zero missed/doubled keys") | Add 0.5 mm spacer to stiffen pill contact area; if debris-driven, sealed keypad zone w/ Kapton frame |
| **R-05** | **Contact wear / pill resistance drift** — HASL pads wear under repeated pill flex; carbon resistance rises with humidity/age | 2 | 2 | 4 | ENIG or gold-finger finish on keypad pads (HARDWARE §8); pills cleaned with IPA at build | Keys needing hard presses in Phase 5 field week | Re-spin pads with ENIG; interim fix: conductive elastomer patch between pill and pad |
| **R-10** | **Custom PCB needs >1 spin** — QFN TPS63802 / FPC connector / antenna clearance mistakes eat the schedule | 3 | 2 | 6 | Reuse Phase-1 pin map verbatim (SOFTWARE §3.2 `config.h` contract); fab web-ERC + placement review before order; 5-board batch; PLAN rule: one spin budgeted, second triggers scope review | First-populated board fails rail test or I²C scan that Phase-1 rig passed | Assemble the second known-good path (SuperMini in Phase-3 harness) as fallback "it still works ugly" state while re-spin fab runs |

### Electrical / power

| ID | Risk | L | I | S | Mitigation | Early warning | Contingency |
|---|---|---|---|---|---|---|---|
| **R-09** | **LiPo safety** — 200 mAh pouch in a sealed, pocketable, unattended product: crush, short, thermal event | 2 | 3 | **6** | DW01A/FS8205A protection is mandatory (HARDWARE §7.3); cell framed/taped, routed away from screw towers (Phase 2 §M3 pocket); ≤25 %C charging (cool, linear, case-open monitored in Phase 1); firmware charge-status shown | Cell >45 °C while charging (bench check); any visible swelling — stop and scrap | Battery bay redesigned with hard carrier + vent path to case seam; if user-visible heat ever occurs, ship with "charge while case open" documented guidance and lower PROG current further |
| **R-11** | **Battery life shortfall** — † estimates (HARDWARE §7.2) optimistic: real queries cost more (TLS handshake, hotspot retries), self-discharge, aged cell | 3 | 2 | 6 | Phase 1 M1.5 replaces all † with measured currents before anything is promised; WiFi grace window + session policy tuned on measurements (SOFTWARE §6.2); sleep ladder audited in Phase 3 acceptance | Measured standby > 120 µA in Phase 3 config; runtime projection < 2 weeks at 20 q/day | Trim: OLED power-gated (not just 0xAE), TCA8418 slow-scan config, disconnect WiFi after 15 s instead of 45 s; worst case accept "weekly charge" positioning (still fine for a calculator nobody opens anyway) |
| **R-13** | **Rail brownout during WiFi TX** — 350–370 mA peaks † on a tiny buck-boost through thin wires → 3V3 dips → `BOR` resets | 2 | 2 | 4 | TPS63802 sized for ≥500 mA transient (HARDWARE §7.1 ⚠ verify); bulk caps per datasheet; Phase 1 step 6.3 scope check at 3.4 V cell *while streaming*; star ground (WIRING §3 step 7) | `rst:0xB (RST_BROWNOUT)` in boot log; reset during "REQUEST" state specifically | Increase bulk capacitance; if on SuperMini bench, shorten/thicken supply leads — this is a bench-wiring risk far more than a PCB risk |

### Software / cloud

| ID | Risk | L | I | S | Mitigation | Early warning | Contingency |
|---|---|---|---|---|---|---|---|
| **R-06** | **TLS/time failure** — no RTC battery; every cold boot has invalid clock → cert validity checks fail; hotspots that block NTP (captive portals) | 3 | 2 | 6 | SNTP is a hard prerequisite of `CONNECT` with its own timeout and error screen (SOFTWARE §3.2, §7.5); retry path re-syncs before re-attempting TLS (GETTING_STARTED §7 row `ssl -0x2700`); test matrix includes "hotspot with login page" | Boot logs show `N sntp timeout`; failures only on first wake of the day | Cache "time synced this session" via millis + one synced epoch in RTC_NOINIT to survive light sleep; if NTP is blocked, degrade gracefully to explicit "no network clock" error — **never** ship setInsecure() in a device that holds an API key |
| **R-07** | **Heap exhaustion / fragmentation** — no PSRAM; WiFi+TLS+HTTP stack is ~100 KB-class transient; slow leak turns "works" into "reboots at query 40" | 3 | 3 | **9** | SOFTWARE §8 rule set (fixed buffers, filter-limited JSON, streaming); heap telemetry logged every 30 s + after each AI turn; soak test M1.5 with heap low-water assertion (to be written) | Heap log trend >5 % drift across soak | Reduce TLS footprint: single reused client already planned; drop HTTPClient layer for raw `WiFiClientSecure` with hand-rolled POST; as last resort cap concurrent SSE parse buffer and shorten system prompt |
| **R-08** | **WiFi range / in-case attenuation** — phone hotspot across the room + antenna buried near LiPo and OLED glass | 3 | 2 | 6 | Antenna keep-out enforced from breadboard rules onward (WIRING §4); ESP32 placed farthest corner from battery (Phase 4 step 3); Phase 3 acceptance includes "case closed, 5 m, 10 queries" | Wi-Fi state stuck in `Connecting`/`Failed` with the hotspot 3 m away; timeout failures only when phone is in a pocket | Tune position by testing two module orientations in Phase 3; evaluate external-antenna variant / reposition (HARDWARE §2.1 U1f ⚠); a 0.3 dB case loss is survivable — a 12 dB battery-shadow loss is not, which is why placement is decided by measurement, not by CAD |
| **R-12** | **API drift & credential risk** — DeepSeek renames models again (already happened: `deepseek-chat`/`deepseek-reasoner` retired as of 2026-09-26 verification), changes base URL, or key leaks from a repo | 3 | 2 | 6 | Re-run curl contract check (SOFTWARE §7.2) at every milestone; model/base-URL strings live only in `config.h` for a 1-line fix; `secrets.h` git-ignored (GETTING_STARTED §3); key shown nowhere in logs (mask in diag) | `400 model not found` / `401` with a *known-good* key; docs' verification date > 90 days old | Switch to replacement model name and re-verify streaming shape; rotate key immediately if exposure suspected; optional offline mode (PLAN "non-goals" stretch) keeps the device usable as a dumb calculator during an outage |
| **R-14** | **Component variability** — SuperMini clones differ (LED pinout, regulator, footprint); OLED batches ship as SSD1309 *or* SH1106; module I²C addr 0x3D | 3 | 1 | 3 | `DISPLAY_CONTROLLER` flag in `config.h` selects SSD1309 vs SH1106 (manual, no auto-detect); buy display + MCU spares (HARDWARE §2.1); Phase 4 replaces clones with genuine module anyway | Boot banner "AI Calculator" appears but panel shows wrong columns | Fine for prototype; final PCB uses ESP32-C3-MINI-1 (Espressif-supplied, stable footprint ⚠ confirm suffix) — clone variance is retired at Phase 4 |

### Process

| ID | Risk | L | I | S | Mitigation | Early warning | Contingency |
|---|---|---|---|---|---|---|---|
| **R-15** | Measurement-free ordering — display/PCB bought on the 5–7 mm / 35 mm *assumptions* to save a day; wrong part arrives and the schedule eats weeks | 2 | 3 | 6 | PLAN gates: M2.3 "display ordered only after M1 logged"; README's first warning bullet; nothing orderable except Phase-1 kit before Phase 2 | Any purchase request citing "research says ~X" for X that the donor can be measured for in 5 min | Rely on the fallback chain (R-01/R-02 contingencies); document lead-time hit honestly in the plan |

## 3. Top risks by score (act-first list)

| Rank | ID | Score | One-line action this week |
|---|---|---|---|
| 1 | R-01 / R-02 | 9 | Do not buy display or PCB until §M1–§M3 are measured (gates M2.2–M2.3). |
| 2 | R-07 | 9 | Enforce SOFTWARE §8 rules in code review; soak test with heap assertion is M1.5, not optional. |
| 3 | R-03 / R-04 / R-06 / R-08 / R-11 / R-12 | 6 | Each has a named plan-phase verification step — treat the acceptance checkbox as mandatory, not aspirational. |
| 4 | R-09 | 6 | Protection board is in the BOM, not an option; charge tests happen monitored/open-case in Phase 1. |

## 4. Closed risks

*(Nothing closed yet — add rows here as phases retire risks, e.g. "R-03 closed at Phase 2
acceptance after §M4 mapping".)*

## 5. Assumptions register (things that would become risks if false)

1. Donor display window clears ≈35 × 17.5 mm minus margin — **§M1 will settle it**.
2. Cavity offers ≥5 mm somewhere and ≥5.5 mm in one zone — **§M2**.
3. Matrix is exactly 8×7 with 50 populated positions — **§M4**.
4. LP402030 delivers near its 200 mAh label (cheap cells often deliver 80–90 % †) — runtime
   table re-derived after first capacity check in Phase 1 step 6.
5. DeepSeek shape as of 2026-09-26 holds through Phase 1 (curl contract test catches drift).
6. ABS shell is WiFi-transparent enough (plastic at 2.4 GHz is nearly lossless; the real loss
   budget is component placement) — verified by R-08's Phase-3 closed-case test, not assumed.
