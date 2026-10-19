# Silkscreen power on CrossInk: sleep budget, pin states and drain rules

Silkscreen has no hardware off (§3.5). 3V3 is live whenever a cell or USB is present, so "off" means ESP32-S3 deep sleep, and every awake minute comes out of the cell. This file gathers the battery-drain facts for the CrossInk port:

- the sleep-current budget (section 1) and the safe sleep state of every pin (section 2);
- each firmware rule and where CrossInk implements it (section 3);
- how to measure, and what the awake tail costs on CrossInk (sections 4 and 5);
- what only a board revision can fix (section 6), and the open questions (section 7).

**As of 2026-09-30 nothing here had been measured on a board.** The owner's unit has since run CrossPoint's battery-log firmware (its first CSV, 2026-10-10, holds 27 rows over 5.7 days from one build, `1.6.5-batlog`); that is a log of battery readings, not a meter reading, and the figures below are as written. No image of this repo has run on a board.

**Sections copied from CrossPoint.** Sections 1, 2, 6 and 7 come unchanged from CrossPoint's `docs/silkscreen/POWER.md` at crosspoint-reader `0d8ada1f` (last changed there in `0d8ada1f`; sections 6 and 7 have not changed since `f61a7a97`). Section 1 includes CrossPoint's own section on the 3.68" variant.

- On Silkscreen, CrossInk sleeps through the same freeink-sdk path, so the budget and the pin states apply as they stand.
- Their "this firmware" notes name SDK code, which CrossInk shares. SDK hashes are commits on the shared `silkscreen` branch.
- Where CrossInk's app differs, section 3 says so.

**Conventions:**

- `§` refers to `de-link_pcb/docs/HARDWARE.md` ([iandchasse/silkscreen-pcb](https://github.com/iandchasse/silkscreen-pcb)).
- Tags: **[DS]** datasheet figure, **[calc]** Ohm's law on netlist values, **[est]** engineering estimate.
- Code paths: app paths are relative to this repo, SDK paths to `freeink-sdk/libs/`.
- Section 3 names functions rather than line numbers, because CrossInk takes upstream every week.
- A CrossInk commit hash in section 3 is the commit on `silkscreen` that landed the rule ([PORTING.md, Status](PORTING.md#status)).

**Silkscreen only.** Every rule in section 3 runs on Silkscreen and nowhere else. Its app code is either:

- compiled only under a flag set only for Silkscreen: `FREEINK_CAP_POWER_POLICY`, `FREEINK_CAP_FRONTLIGHT_PROBE` or `FREEINK_CAP_DISPLAY_PROBE_POLICY`;
- called only from such code. The `HalGPIO` touch sleep calls are gated on the SDK's `FREEINK_CAP_TOUCH`, which the Sticky and X4 Pro set too, but only the Silkscreen sleep path calls them; or
- gated on profile data that only `SILKSCREEN` sets. `BoardConfig::powerPolicy()` is all zero on other boards.

X3/X4, Sticky, X4 Pro and X4 Classic keep CrossInk's power behaviour and its own sleep tail.

## 1. Sleep-current budget

**Conditions**

- Battery side, with USB and any UART adapter on TP1/TP2 unplugged.
- Panel in its controller's deep sleep: SSD1677 `0x10`/`0x03` (Good Display GDEQ0426T82) or UC8279 DSLP `0x07`/`0xA5` (the custom UC8279D panel). Light off. SD gated off with a card fitted.
- U13 DS3231MZ fitted, U14 not fitted. J6 empty. Touch tail not connected (priced separately below).
- **typ**: 3.7 V cell at 25 °C.
- **max**: datasheet maxima, 4.2 V cell. This is **not** a worst case over temperature:
  - several maxima are specified at 25 °C only (panel 6 µA, DW01A 6 µA, DS3231M 3.0 µA);
  - Espressif publishes no deep-sleep maximum.

| # | Source (rail) | typ µA | max µA | Condition | Reference |
|---|---|---|---|---|---|
| 1 | U4 ESP32-S3-WROOM-1-N16R8 deep sleep (3V3) | 7 [DS] | 13, placeholder [est] | ext1 wake on IO18, RTC peripherals off, RTC memory on, VDD_SPI off, no ULP or timer | S3 DS v2.2 Table 5-10; IDF 5.5 sleep_modes.c:1282 |
| 2 | U4 pad leakage, pins held above 0 V | ~0 | 0.4 [DS] | 50 nA × about 8 pins | S3 DS v2.2 Table 5-4 |
| 3 | U3 TLV75533P IQ at no load (LDO_IN) | 25 [DS] | 33 [DS, 85 °C] | EN tied to IN | TI SBVS320D §5.5 |
| 4 | U3 extra ground current at the real 20-40 µA load | 0-15 [est] | unknown | Not characterized below 1 mA (≈145 µA at 1 mA). An adaptive-bias curve could add ≈20 µA at 30 µA. **Measure.** | SBVS320D Fig 5-17/5-18 |
| 5 | R81 + R82, Fix-4 detector (3V3) | 6.6 [calc] | 6.9 | Correct cell fitted | netlist |
| 6 | R79 + R80 across the raw cell | 3.4 [calc] | 3.8 | 3.7 / 4.2 V | netlist |
| 7 | R56 + R57, Q3 gate bias (raw cell) | 0.37 [calc] | 0.42 | Outside the protection FETs | netlist |
| 8 | U5 DW01A (clone) (P+) | 3.0 [DS] | 6.0 [DS] | FETs on | DW01A ICC; clone datasheet not obtained |
| 9 | U2 TPS2116 on VIN2 (P+) | 1.35 [DS] | 3.7 [DS] | VIN1 = 0 V | TI SLVSFG1A §6.5 |
| 10 | U11 TP4056 BAT pin, VCC = 0 (P+) | 1.0 [DS] | 2.0 [DS] | CE held at 3V3 by Fix-4 (no datasheet row for that) | TOPPOWER TP4056 p.5 |
| 11 | BAT_MONIT R12/R10 1M/1M (P+) | 1.85 [calc] | 2.1 | IO8 with no pulls | §3.6 |
| 12 | USB_STAT R70 1M + R17 2M (3V3) | 1.1 [calc] | 2.1 [calc] | On battery; max assumes the unpowered TP4056 status pins clamp | §3.7 |
| 13 | LED_MONIT R39/R41 1M/120k from LED_SW via U10's body diode (LDO_IN) | 2.9 [calc/est] | 3.4 | Light off; in §16 since its 2026-09-30 re-itemization | TI SNVSCN8A §7.3.3 |
| 14 | U10 TPS923610 shutdown (LDO_IN) | 0.13 [DS] | 0.5 [DS, 85 °C] | ADIM LOW for > 2.5 ms (IO42 held LOW) | SNVSCN8A §6.5 |
| 15 | U12 74LVC1G04 (clone) (3V3) | 0.1 [DS] | 4 [DS Nexperia]; 10 if TI's limit applies | Input at GND (IO40 LOW) | Nexperia Rev 18.1; TI SCES214AF |
| 16 | U13 DS3231MZ, VBAT-only (3V3) | 2.0 [DS] | 3.0 [DS] | EN32KHZ = 0, I2C idle; conversions already averaged in | Maxim DS3231M Rev 5 p.3 (Note 4) |
| 17 | GDEQ0426T82 panel in deep sleep (3V3) | 2 [DS] | 6 [DS] | 0x10/0x03 sent, RST HIGH. The custom UC8279D panel (DSLP `0x07`/`0xA5`) has no datasheet figure in hand: measure it (MEAS-01). Nor has the 3.68" variant's UC8279d (the X3 driver's DSLP `0x07`/`0xA5`): its floor is this budget with this row unknown, so the figures of this table are the 4.26" board's. | GDEQ0426T82 §7.1 |
| 18 | Q7 AO3401A leakage, SD gate off | ~0.01 [est] | 1 [DS, at −30 V] | IO10 HIGH (R77 carries the same current) | AO3401A DS |
| 19 | Q4 IRLML6346 leakage | ~0.01 [est] | 1 [DS, at 24 V] | EINK_SW at 3V3 | IRLML6346 DS |
| 20 | ESD/TVS parts (TPD4E1U06 ×5, CR2/CR3 SD05C, D3, CR1, D8) | 0.05 [est] | 2 [DS bounds] | SD05C clones rated 1 µA max at 5 V each | TI SLVSBQ9D; SD05C-01FTG Rev 1.4 |
| 21 | MLCC insulation leakage, always-on rails | 0.05 [est] | 0.5 [est] | Room temperature | Murata GRM21BR61E226ME44 |
| 22 | Idle pull networks (R4, R28, R5, R7, R13, R40, R47, R48, R62/R76, R38/R51), D2, R49/R50 | 0 | 0 | Nothing driven against them, no key held, USB absent. R49/R50 (1 MΩ LED bleeders) and Q6 draw nothing because each FT01C string is 5 LEDs (≈15 V Vf) against LED_SW ≈3.2 V | netlist |
| **Σ** | **Floor, no touch** | **≈58** (57.9) | **≈95** (94.8) | ≈91 (90.9) with the high-voltage leakage specs of rows 18-20 discounted; 58-73 typ and up to ≈110 max once row 4 is counted | rows re-added by the verifier from the netlist |

- 58 µA ≈ 1.4 mAh/day; 95 µA ≈ 2.3 mAh/day. The cell capacity is still an open question.
- **Not in the budget** (small, unquantified):
  - U10's SW-pin off-state leakage from LDO_IN (SNVSCN8A specifies shutdown current for VIN only);
  - the 3V3 → L1 → D5 → PREVGH/VGH path (C14 4.7 µF/50 V), live through sleep;
  - the rise of every leakage term at 60-85 °C.

### Scenarios on top of the floor

| Scenario | Adder µA | Total µA | In this firmware |
|---|---|---|---|
| FT6336U tail on J4, hibernated (`0xA5 = 0x03`), TP_RST HIGH | +55 typ [DS, at 2.8 V, no max] | ≈113 (≈2.7 mAh/day) | Every normal sleep entry with the tail fitted. The lowest possible while J4 VDD is unswitched (HW-02). |
| Tail in Monitor mode | +220 typ [DS] | ≈278 | A tail that missed the cold-boot probe and did not take the one best-effort write, until the next cold boot. The abnormal-reset sleep resets the controller and sends hibernate, and a 4.26" Silkscreen build with `FREEINK_CAP_TOUCH=0` no longer compiles (the 3.68" variant has no tail and is built with it). |
| TP_RST held LOW with the tail fitted | ≈+1100 [est: 3.3 V into the ~3 kΩ RSTN pull-up] plus the reset-state current | ≈1150 (≈28 mAh/day) | This port's profile did this until freeink-sdk `a26e398`, which holds IO11 HIGH. |
| GPIO47/48 pads unpowered in sleep, so R5 and panel BUSY back-feed VDD_SPI | +100-300 [est]; the R5 path alone ≤ ≈280 | ≈160-360 | Open hardware risk (HW-01). Measure ([section 4](#4-measurement-and-bench-checks)). |
| Near-empty cell: LDO_IN below ≈3.3-3.35 V, U3 in dropout | IQ 25 → ≈190-225 at 25 °C, ≈260 at 85 °C [DS typ curve] | ≈250-300 | The reason for the 3.4 V cutoff (BAT-01). Unavoidable for a device stored empty. |
| Power button held or stuck | +30 [calc, R62 + R76] | ≈88 | Firmware sleeps after 5 s and wakes on the release (PB-01). |
| After a DW01A over-discharge trip | 2.5-6.5 from the raw cell | — | DW01A IPD ≤ 4 [DS] + R79/R80 + R56/R57 (HW-09). |
| EPD SCLK/MOSI/DC floating | 0 to tens [est] | — | Fixed: held LOW (freeink-sdk `983f2a7`). |
| DS3231 EN32KHZ left set | +0.1 at 25 °C, +0.8 at 85 °C [DS] | — | Does not occur here: `Rtc.cpp` `beginDevice()` clears it (freeink-sdk `abca595`). |

### Against HARDWARE.md §16

§16 was re-itemized on 2026-09-30 from this budget, part by part, and now agrees with it:

- ≈58 µA typical (≈56 µA without the panel) and ≈91-95 µA maximum, with the ESP32 at 7 µA typ for ext1. Every row above is in it, the LED_MONIT divider and the panel included.
- It states the touch-tail case (+55 µA hibernated, ≈220 µA in Monitor, ≈1.1 mA with TP_RST LOW) and the near-empty cell (LDO dropout).
- Like this budget, its maximum leaves out the LDO's load term (row 4) and the GPIO47/48 back-feed (HW-01), and names both.
- It has no row for the custom UC8279D panel, whose deep-sleep figure is not in hand (row 17).
- **Double counts to avoid** (both documents follow them):
  - §3.3's "≈7 µA incl. DW01A" overlaps the Fix-4 figure (R79/R80).
  - DS3231M conversions are already in its IBATT.
  - R77 and Q7 leakage are one current.
  - TPS2116 ISTBY,VIN1 does not apply on battery.

### The 3.68" variant (`silkscreen-368`)

The same board and budget with the Xteink X3's panel in place of the GDEQ0426T82, and no touch tail ([MODULARITY.md](MODULARITY.md#the-368-variant-silkscreen-368)). Its panel is the X3's UC8279d and nothing else (decided 2026-10-11, made 2026-10-12). **What has run on a board (as of 2026-10-12).** One module (`SEO368NQW25`) has run, in the owner's first session (2026-10-11, a CrossPoint build): the stock X3 probe read `VER=00 0F 00`, the UC8253 default recipe left it blank (`Wait complete: X3_PON (41 ms)`), and with the UC8279d driver forced by a local build flag it displayed a picture. That forced build then froze (no panic, no watchdog) on its first grayscale image page. The code reads as a bus transaction held across a bus command (a deadlock on arduino-esp32's non-recursive SPI mutex): that cause is inferred from the code, not from a backtrace. Fixed upstream: Free-Ink PR #139 (`c2d711a`, merged as `7fcd3b3` on 2026-10-12, the same one-line change); CrossPoint #3960 (`93a14afd`) bumped the pin. Our own gated copy (`4988be4`) was reverted by SDK `b460001` to take upstream's text, and the C3 X3 build has the fix too. The 2026-10-12 builds show it in their disassembly (CrossPoint's `silkscreen-368` and `c3check`, CrossInk's `silkscreen-368`: `Transaction::end()` ahead of `EpdBus::cmd(0x92)` in `writeGrayscalePlaneStrip()`); not yet seen on glass by us (the owner has not yet flashed it). The owner also reported a quiet, harsh buzz at every display refresh, from the e-paper boost stage; it is unexplained, and the boost inductor `L1` (47 µH, sized to the SSD1677 reference and unverified for this panel: HARDWARE.md §6.2) and the 4.7 µF / 50 V X5R MLCCs `C11` and `C13`-`C17` are the usual singers. Nothing else has run on a board, and CrossInk's variant images have never been flashed. Nothing in this section has been measured.

- **Row 17 is not in hand.** The UC8279d has no datasheet figure in the repo. The floor is the budget above less the 4.26" panel's 2-6 µA, plus this panel's own: unknown. P5 records the J2 flex-in/flex-out gap with no reference, and MEAS-01 is the test.
- **No touch terms.** The +55 µA hibernate, +220 µA Monitor and ≈1.1 mA `TP_RST` rows do not exist: no tail, and no code that could miss a sleep command. `IO11` is still parked HIGH and held (inherited, at no cost), and `IO41` is never configured.
- **`BUSY_N`.** IO48 has the internal pull-up while awake (the X3 driver's active-low `BUSY_N`; ≈73 µA only while the controller drives it LOW, in a refresh wait) and is a plain unheld input in sleep, as for the UC8279. Its level in sleep is not in hand, so HW-01's R34 check is the one that matters.
- **The panel stays in PON between refreshes (current not in hand).** The reader passes the Sunlight Fading Fix as `turnOffScreen` (`GfxRenderer.cpp:1753`) and it is off by default (`CrossPointSettings.h:340`), so after a refresh the UC8279d driver leaves the controller powered on (`_isScreenOn` stays true, `Uc8279Driver.cpp:243-247`) until the next sleep entry's power-off (`:262-273`); `controllerIdle()` has no caller and this driver does not override it. The PON-idle current is not in hand (the same gap as row 17). Bench: the panel's share of the 3V3 current with a page on screen, PON idle against after a power-off ([TEST-SUITE.md](TEST-SUITE.md#the-368-variant-silkscreen-368-test), step 9).
- **Awake.** Computed from the driver and bus code (SPI at the profile's 10 MHz, 99-byte rows, 528 rows, 52,272 B a plane; the 10-15 µs of per-row call overhead is an estimate), none measured:

  | When | What | Cost |
  |---|---|---|
  | A cold boot, or any reset but a deep-sleep wake (and the boot cutoff's and the reset loop's panel sleep) | Nothing: the controller is the UC8279d, fixed in the profile. The earlier build's stock probe (reset 10 + 50 + 50 ms, one `BUSY_N` read, three VER bytes) is not compiled | 0 ms. That probe took about 111 ms, 410 ms at most when `BUSY_N` stayed LOW through its 300 ms bound |
  | A deep-sleep wake | Nothing: there is no verdict to reuse, check or drop, no 300 ms `BUSY_N` guard and no restart that probes | 0 ms. The earlier build re-probed at every wake with an unknown id (about 111 ms each), and a `BUSY_N` that stayed LOW 300 ms after the reset restarted the boot |
  | Every `display.begin()`: a cold boot, and each wake that reaches the UI (not a ghost wake) | `Uc8279Driver::begin()`: the 80 ms reset (10 + 10 + 10 ms and a 50 ms settle) and a register script of 9 commands and 23 data bytes, each its own short transaction. No plane fill and no RAM clear: the first refresh writes both planes | About 80 ms plus well under 1 ms of SPI (about 32 transactions of 10-15 µs, an estimate), about equal to the SSD1677's roughly 70-80 ms (inference). The UC8253's two white planes (+95-110 ms per `begin()`, about 1.3-1.8 µAh) are not in this variant |
  | The first paint after `begin()` (the splash, then the first screen) | `displayStart()`: PTIN, the old plane seeded white, the frame, the GC bank, PON, DRF; then `displayFinish()` writes the frame to the old plane. The driver's boot budget makes the first two content paints after `begin()` run the GC bank, whatever mode was asked | Three planes of 52,272 B at 10 MHz: about 140-150 ms of SPI (41.8 ms of clock per plane, plus the per-row call overhead), then the PON wait and the GC waveform, whose time on this panel is not in hand (the UC8253 X3 driver's header quotes about 130-770 ms for a waveform, and `HalDisplay.h` about 770 ms for a full sync on an X3: neither is the UC8279d's). Computed, not measured |
  | A Quick Resume wake (`SplashlessWake` with `/.crosspoint/sleep_frame.bin`) | `begin(seamless)` defuses the driver's boot budget (`skipInitialResync()`), then `cleanupGrayscaleWithFrameBuffer()` rebases the controller RAM that `begin()` cleared: PTIN, both planes rewritten (DTM2, DTM1), PTOUT. No refresh, no PON, no `BUSY_N` wait | **About 85-110 ms** of SPI-bound CPU, about 1.4-1.8 µAh (inference). It replaces the first reader page's HALF, which `HalDisplay` turns into a resync (a GC refresh), with a FAST differential on the DU bank, because only this branch sets `allowFastInitialReaderRefresh`. Net: a Quick Resume wake saves time and energy |
  | A HALF anywhere on the X3 panel (every N-th page, sleep covers, overlays) | `requestResync(1)`: the next refresh runs the GC bank against the controller's previous frame (the UC8279d driver has no half bank and ignores the settle count) | The X3 panel's own cost, as on the C3 X3, switched on for this variant on purpose; not measured |

  The driver-free panel sleep (the reset loop) is about 72 ms idle and 172 ms at most (a 10 + 1 ms reset, a 60 + 1 ms settle and a BUSY_N wait of up to 100 ms); the boot cutoff's is `begin()` and the deep-sleep command (no probe in this variant). The 60 ms settle is host-tested only (bench list step 10).
- **Saved against a board with a tail.** The ≈307 ms touch reset and settle at each cold boot, and the NACKed hibernate write at each sleep entry.
- **Wake sources, timers, holds, pulls.** None added: `scripts/check_sleep_wake_sources.sh` passes.

## 2. Pin states in sleep

**How the pads behave in deep sleep.**

- IO0-21 are VDD3P3_RTC pads; `gpio_hold_en` on them goes to the RTC hold.
- IO38-48 are digital pads. At deep-sleep entry ESP-IDF isolates every **non-held** digital pad (input and output off, pulls off). The exceptions are U0TXD/U0RXD and the MSPI pads, and it does this only while `gpio_deep_sleep_hold_en()` is set (IDF 5.5 `sleep_gpio.c:129-184`).
- ESP-IDF never isolates un-held RTC pads, and their sleep state is undocumented. So every RTC pad that matters is held.
- Digital-pad holds are dropped at a deep-sleep wake (IDF `gpio.h:401-403`). IO40, IO42 and IO47 are therefore undriven from reset until `holdPowerRails()`/`EpdBus`. R75, U10's ADIM pull-down and R5 keep their levels meanwhile. IO41 has no board pull: for a GT911 (alternate-jumper builds only) the firmware drives it LOW again as soon as `InputManager::begin(false)` runs, and re-holds it on every sleep entry before the probe.
- Costs below are at 3.3 V with the internal pulls at 45 kΩ typ.

| GPIO | Net | Safe in deep sleep | Forbidden (cost µA) | This firmware |
|---|---|---|---|---|
| IO0 | Boot strap, R13 10k to 3V3 | Untouched | LOW 330; pull-down 60 | Untouched |
| IO1 / IO4 | Key ladders (R4/R28 10k to 3V3) | Input, no pulls, not a wake source | Pull-down 60 each; LOW 330; HIGH ≈33 mA into a pressed 100 Ω key | `INPUT` (`InputManager::begin`), not held. A key held through sleep leaves the input buffer mid-rail (unquantified). The presence test at a reset other than a deep-sleep wake turns each pull on for about 2 ms, then off; a ladder found with no pull-up is switched off and its pad stays as the ADC left it (input and pulls off), so a floating node costs no input current. The awake idle poll takes one conversion per released ladder (`InputManager::adcLadderContact()`); the 16× decode runs only in the debounced poll. |
| IO2 | LED_MONIT (1M/120k) | ADC input, no pulls | Pull-up +17-20; HIGH +25; any pull corrupts the reading | Read awake only, through `SharedAdc.h`: every 100 ms while lit (`FrontlightManager::checkMonitor`), every 20 ms for 500 ms after each enable (the load probe, on the same timer), and every 20-100 ms after a trip until the rail is at 15 V (at most 4 s) |
| IO3 | Spare to J6.9 | J6 empty: optional hold LOW (low priority) | Re-derive if an accessory is fitted | Untouched (floating) |
| IO5-7, IO15-17 | SD bus (10k pull-ups to the **gated** SD_VDD; CLK has none) | LOW, pulls off, held | While gated: HIGH back-feeds SD_VDD (32 without a card, 200-2000 with one); pull-up 30 to ≈440 | Parked LOW and held (`PowerManager::powerDownRailsForSleep` → `parkSdmmcBus` + `gpio_hold_en`, SDK `983f2a7`); released at boot by `holdPowerRails()` |
| IO8 | BAT_MONIT (1M/1M from P+) | ADC input, no pulls | Any pull or drive +1.3-2 and ruins the reading | ADC only |
| IO9 | USB_STAT ladder | ADC input, no pulls | Pull-down +2.1; LOW +2.2; pull-up +0.5 | `chargeLadder` at ≤1 Hz; `batteryChargeStatus` unassigned, so no `INPUT_PULLUP` |
| IO10 | SD_ACTIVATE (active LOW; R40 100k to 3V3) | HIGH + hold | LOW 33 + card 200-2000; a pull-down turns Q7 on (Vgs −2.26 V) | `holdRailOff` HIGH, level preset before the output is enabled |
| IO11 | TP_RST (FT6336U RSTN has ~3 kΩ to IOVCC) | **HIGH + hold** | LOW with an FT6336U ≈1100 [est] and it resets out of hibernate; LOW with a GT911 330 | `parkedPins {11, High, High}` (SDK `a26e398`); LOW only as the ≥5 ms reset pulse (`InputManager::pulseTouchReset`) |
| IO12 / IO13 / IO21 | EPD MOSI / SCLK / DC | LOW + hold (0 µA) | Floating: undefined inputs on a powered SSD1677 | `parkedPins` LOW at boot and in sleep (SDK `983f2a7`) |
| IO14 | EPD CS (no pull-up) | HIGH + hold | Floating or LOW: panel selected | `parkedPins` HIGH at boot and in sleep |
| IO18 | Power button (R62 10k to 3V3 when pressed, R76 100k pull-down) | ext1 ANY_HIGH; input with no IO_MUX or RTC pulls | Pull-up 23 and released level 2.28 V (false wakes); pull-down drops the pressed level to 2.50 V against VIH 2.475 V | `preparePowerPinForSleep` (`powerExternalPull`); `armWakeOnPins()` returns right after arming ext1, so upstream's RTC pull and RTC_PERIPH override (freeink-sdk `ed88f03`) never reach it (SDK `8b87038`); ANY_LOW only for a release wake (PB-01). Found idling between the logic levels at boot (R36 and R72 both fitted, ≈1.76 V), it arms nothing and is held with its input off: R28 + R36 + R72 against R76 then draw ≈18 µA, and only reset wakes the chip. |
| IO19 / IO20 | USB D− / D+ | Leave to USB | Repurposing loses the flashing path | USB-Serial-JTAG |
| IO38 / IO39 | I2C SDA / SCL (R48/R47 2.2k to 3V3) | Released, not held; R47/R48 keep them HIGH | Held or stuck LOW 1500 each; pull-down 70 each | `freeink::i2c::releaseSensorBusForSleep()` (Wire end, lines released, a stuck bus clocked free), SDK `a26e398` |
| IO40 | COLOR_SEL (R75 100k pull-down) | LOW + hold | HIGH 33 + U12; pull-up 23 + U12 shoot-through (up to several hundred [est]) | `FrontlightManager::prepareForSleep()`, then `parkedPins` LOW held. After a trip on the warm string it stays HIGH while awake until LED_SW is at 15 V (at most 4 s), so C9's charge cannot reach the cool string; `prepareForSleep()` waits for that too, then rests it LOW |
| IO41 | TP_INT (no board pull) | FT6336U: not held (isolated). GT911 (alternate-jumper builds only): held LOW after its sleep command | FT6336U: a LOW wakes it from hibernate (→ 220); GT911: floating or HIGH for 2-5 ms wakes it (3300) | Awake `INPUT_PULLUP` on the FT path; in sleep unheld for an FT6336U, held LOW for a GT911 (`InputManager::prepareTouchForSleep`). The hold ends at the wake, so a sleep entry before the probe re-sleeps a cached GT911 (ghost wake) or re-holds INT (release wake, `holdTouchIntForSleep()`). Never configured in the 3.68" variant (no touch in the build). |
| IO42 | ADIM / enable (U10 600k pull-down) | LOW + hold | HIGH or pull-up: 260-300 IQ, ≈60 mA lit [est]; `gpio_reset_pin` turns the pull-up on | Duty 0 and LEDC detached (`FrontlightManager::prepareForSleep`), then `parkedPins` LOW held |
| IO43 / IO44 | UART TX / RX (TP2/TP1) | Defaults (not isolated by IDF) | An unpowered adapter left clipped on is back-fed by TX | Default; unplug adapters before measuring |
| IO45 / IO46 | Spare straps to J6 | Defaults, isolated | IO45 HIGH at reset selects 1.8 V VDD_SPI: boot fails, chip stays awake | Untouched |
| IO47 | EPD RST (R5 10k to 3V3); pad likely on VDD_SPI | HIGH + hold | LOW 330 and the panel leaves deep sleep; pull-down 60. If the pad is unpowered in sleep, R5 back-feeds VDD_SPI (HW-01) | `holdRailOff` HIGH; `EpdBus::begin` presets HIGH before releasing the hold (SDK `983f2a7`) |
| IO48 | EPD BUSY (SSD1677 drives it HIGH in deep sleep; the BUSY_N level of a UC8279 or of the 3.68" variant's UC8279d in deep sleep is not in hand); pad likely on VDD_SPI | **Not held**, no pulls | Pull-down held into sleep 73 (SSD1677); a pull-up held against a LOW BUSY_N the same; any drive: contention; unpowered pad back-feeds VDD_SPI (HW-01) | Awake only, by the probed controller: pull-down for an SSD1677, pull-up for a UC8279 (`busyPullUp`). `powerDownRailsForSleep` takes both off and leaves a plain input, never held (SDK `983f2a7`; the pull-up in the display-probe commit) |
| IO26-37 | Flash / octal PSRAM | Never touch | Reconfiguring breaks boot | Untouched |

## 3. Firmware rules and where they live

The rule IDs are the drain audit's, as in CrossPoint's POWER.md. Rules that only CrossInk needs are in [section 3.2](#32-crossink-only-rules).

| Rule | Requirement | CrossInk status and location |
|---|---|---|
| ZD-01 | The only deep-sleep wake is ext1 ANY_HIGH on IO18. No timer, ULP, touch-pad, ext0 or GPIO wake, no `esp_sleep_pd_config`, no light sleep. (RTC_PERIPH on costs +1 µA; the ULP 170-190 µA.) | **Done** (`e59a7cfc`). Silkscreen sleeps only through `lib/hal/HalPowerManager.cpp` `sleepImpl()` → SDK `PowerManager::deepSleepUntilPowerButton()`. That path calls `armPowerButtonWakeup()` → `armWakeOnPins()` (the only `esp_sleep_enable_ext1_wakeup`) and `deepSleep()` (the only `esp_deep_sleep_start()` a Silkscreen build compiles). If the boot check finds IO18 idling between the logic levels (`HalGPIO::checkPowerButtonIdle()`), the sleep arms no wake source. **Guard:** `scripts/check_sleep_wake_sources.py` ([below](#guard-script-and-its-exception)). Like CrossPoint's, it bans every `esp_sleep_pd_config` but one: upstream's `ESP_PD_DOMAIN_RTC_PERIPH` call in the SDK's `PowerManager::armWakeOnPins()` (freeink-sdk `ed88f03`), allowed only behind the `InputPins::powerExternalPull` exemption (SDK `8b87038`; guard `c2913272`), which keeps IO18 out of the RTC pull and the RTC_PERIPH override ([below](#guard-script-and-its-exception)). OPT-01 would need an exception of its own. |
| ZD-02 | Every sleep path ends in `powerDownRailsForSleep` → `deepSleepUntilPowerButton` with `gpio_deep_sleep_hold_en()`, from a task with an internal-RAM stack. | **Done** (`e59a7cfc`). `HalPowerManager::startDeepSleep()` → `sleepImpl(gpio, true)`; `startDeepSleepMinimal()` → `sleepImpl(gpio, false)`. `sleepImpl()` runs, in order: Wi-Fi off (full entry only); `HalGPIO::prepareForSleep()` (or `prepareForMinimalSleep()`); `freeink::i2c::releaseSensorBusForSleep()`; `logSerial.end()`; `setPowerLatchesForSleep()` (shared with the other boards; Silkscreen has no latch); `Frontlight.prepareForSleep()`; SDK `powerDownRailsForSleep()`; 80 MHz; `deepSleepUntilPowerButton()`. Every caller runs on the Arduino loop task. In `src/main.cpp` they are `enterDeepSleep()` (menus, shortcuts, auto-sleep, the Quick Lock timeout, every battery limit), the failed wake verification in `setup()`, the release-wake re-sleep, `sleepIfResetLoop()`, `sleepIfBatteryEmptyAtBoot()` and `haltOnPowerButtonFault()`. The readers' shortcut handlers call `enterDeepSleep()` from their `loop()`. IDF asserts on a PSRAM stack (`sleep_gpio.c:150`). |
| ZD-03 | Where the panel was begun or reset, send the panel controller's deep-sleep command before sleeping. | **Done** (`f9544b2e`). Every path that began or reset the panel sends its controller's command (SSD1677 `0x10`/`0x03`; UC8279 power-off, then DSLP `0x07`/`0xA5`):<br>- **Normal sleep:** `enterDeepSleep()` → `display.deepSleep()` (`HalDisplay::deepSleep()`), after the sleep screen and `Storage.shutdown()`.<br>- **Boot cutoff** after anything but a deep-sleep wake: `sleepIfBatteryEmptyAtBoot()` resolves the controller, begins the panel without a refresh (`display.begin(true)`) and sleeps it.<br>- **Power-button fault:** `haltOnPowerButtonFault()` sleeps the panel it drew on.<br>- **Reset loop:** `sleepIfResetLoop()` → `resolvePanelController()` → `HalDisplay::sleepWithoutDriver()` (SDK `PowerManager::sleepPanelWithoutDriver()`).<br>**The probe:** `resolvePanelController()` → SDK `freeink::applyXteinkDisplayController()` (`XteinkDetect`, under the profile's `DisplayProbeConfig`) runs before `display.begin()`, and `HalDisplay::logController()` logs the verdict. The probe block moved, unchanged, out of `setupDisplayAndFonts()` into `resolvePanelController()`. A ghost wake and a release wake never begin or probe the panel. The probe's timing, its RTC cache and the `BUSY` check at a wake are SDK code, as in CrossPoint.<br>**3.68" variant:** the controller is the UC8279d, fixed in its profile, so nothing is probed at a cold boot, at a deep-sleep wake or in `sleepIfResetLoop()`, and there is no verdict to keep, check or drop: `applyXteinkDisplayController()` only logs "forced by the build" and leaves the bus alone, and `HalDisplay::logController()` prints `Panel controller UC8279 (forced by the build)`. Its sleep command is the X3 driver's DSLP `0x07`/`0xA5`, and the driver-free sleep settles 60 ms after the reset (SDK `0e53723`, re-examined in `664369b` and `5fb944d`). |
| ZD-04 | EPD RST (IO47) held HIGH through sleep, no LOW glitch on wake. | **Done** in the shared SDK: `holdRailOff()` presets the level, and `EpdBus::begin()` releases the hold last. |
| ZD-05 | CS HIGH, SCLK/MOSI/DC LOW, all held in sleep. | **Done** in the shared SDK: `SILKSCREEN` `parkedPins`, released by `BoardConfig::holdPowerRails()`, the first call in `setup()`. |
| ZD-06 | Never pull down or hold IO48 in sleep. | **Done** in the shared SDK (`powerDownRailsForSleep()`). |
| ZD-07 | SD off: write back, unmount, deinit, bus LOW with pulls off, IO10 HIGH, ≥20 ms; held in sleep; never a pull-down on IO10. | **Done.**<br>- **Shutdown:** `enterDeepSleep()` → `Storage.shutdown()` (`lib/hal/HalStorage.cpp` `HalStorage::shutdown()` → SDK `SDCardManager::shutdown()`). The SDK writes back SdFat's cached sectors (`syncVolume()`), unmounts, parks the bus LOW, gates SD_VDD off and waits 20 ms. `sleepIfBatteryEmptyAtBoot()` and `haltOnPowerButtonFault()` call the same shutdown.<br>- **In sleep:** `powerDownRailsForSleep()` holds the bus. A failed mount parks (SDK `SdmmcParkedMount.cpp`).<br>- **USB Drive:** `HalStorage::beginUsbDrive()` → `detachFilesystemForRawAccess()` writes back first.<br>- **Writes at sleep entry:** before the shutdown, CrossInk's sleep entry writes `APP_STATE` and whatever the outgoing screen saves on exit, writes or removes the Quick Resume frame, and runs the stats backup (`backupGlobalStats(false)`, skipped when unchanged). The NVS wake mirror is written after it, only when it changed. `BatteryDiagnosticLog` is compiled out (`CROSSINK_BATTERY_DIAG_LOG` 0).<br>- **Early sleeps:** the release-wake, ghost-wake and reset-loop sleeps come before `Storage.begin()`.<br>- **2026-10-12 (upstream's `2ca5e31`, SDK `336ec77`):** `SdmmcBlockDevice::begin()` now clears the holds on the six bus pads first, for every board (Picco holds them LOW before its rail drops, in code behind `FREEINK_DEVICE_PICCO` that this board does not compile). Here `holdPowerRails()` has already released them at boot (`parkSdmmcBus()`: IO5, IO6, IO7, IO15, IO16 and IO17), so the loop finds nothing held and changes no level, direction or pull; it runs after the early wake decision (WK-01), at `Storage.begin()`, and the park and the hold in `powerDownRailsForSleep()` are as they were. |
| ZD-08 | Light dark, LEDC detached, IO42 and IO40 held LOW in sleep; never `gpio_reset_pin` on them; restore the light only after a verified wake. | **Done** (`c240df98`, `e59a7cfc`, `f9544b2e`, `7cc2982d`, `41c02221`).<br>**Sleep:** `sleepImpl()` → `Frontlight.prepareForSleep()` (SDK `FrontlightManager::prepareForSleep()`) runs before the parked holds. `enterDeepSleep()` calls `handleFrontlightTrip()` and then `Frontlight.prepareForSleep()` first, so the light is dark before the sleep screen and the SD writes. The saved preference is unchanged. A switch-off or sleep entry while a load-probe window is open on a string not yet lit since the reset counts the light's rail as charged (SDK `abandonProbe()`, `468b8e2`), and the rail check ignores readings within 35 ms of that mark, so COLOR_SEL never moves onto the other string while an open string's charge may still sit on the boost output.<br>**Restart:** `HalFrontlight::begin()` registers an `esp_restart()` shutdown handler (`darkBeforeRestart()`) for an enable-gated light.<br>**Restore:** `Frontlight.begin()` runs in `setup()` after the wake verification, the settings load and `haltOnPowerButtonFault()`. Three things can ask it to switch on: Restore Light on Wake, CrossInk's daily light schedule (`FrontlightSchedule::shouldApplyOnWakeSchedule()`), or a silent restart's replay (`shouldPreserveLightAcrossRestart()`). The replay uses the light's live state: `restartWithSilentToken()` records `Frontlight.isOn()` in the top bits of the restart's target word (upstream `461b7074`), and `setup()` replays that instead of the saved on flag, so it relights only what a user action or Restore Light on Wake had lit. `HalFrontlight::begin()` refuses all three while the SDK records no light (`FrontlightManager::restoreAllowed()`, logged as `Frontlight not restored`). The SDK keeps a missing string's mark until a cold boot.<br>**Load probe:** `HalFrontlight::noteSwitchOn()` marks every switch-on for the probe:<br>- `setOn(true)`, including a Quick Lock unlock that restores the light;<br>- a warmth change while lit; when a blend first needs the other string since the reset, the colour select moves to it while lit and the monitor checks it for `blendConfirmWindowMs` (500 ms on Silkscreen) instead of the light going dark first;<br>- the restore;<br>- a brightness change from 0 while lit (CrossInk only: brightness 0 takes an enable-gated light dark).<br>`src/main.cpp` `settleFrontlight()` runs on every `loop()` pass (CI-08). Nothing new runs in sleep or on a ghost wake. |
| TP-01 | TP_RST held HIGH; FT6336U hibernate `0xA5 = 0x03` at every sleep entry; TP_INT unheld. | **Done** (`e59a7cfc`, `f9544b2e`).<br>**Sleep commands:**<br>- full entry: `sleepImpl()` → `HalGPIO::prepareForSleep()` → SDK `InputManager::prepareTouchForSleep()`;<br>- minimal entry: `prepareForMinimalSleep()` → `holdTouchIntForSleep()`;<br>- reset loop: `HalGPIO::resetTouchForSleep()` → `resetAndHibernateTouch()`.<br>**Touch-gate audit:** the three are gated on the SDK's `FREEINK_CAP_TOUCH`, not the app's touch flag, and their names match nothing `scripts/check_app_touch_gate.py` forbids.<br>**Refused id:** a device that fails the FocalTech id check is logged (`HalGPIO::startTouch()`, or `begin()` when the probe is not deferred) and is never written.<br>**Timing:** the probe itself runs after the wake decision (WK-01).<br>**Flags:** Silkscreen builds with `CROSSINK_APP_CAP_TOUCH=1`, which `include/AppCapabilities.h` requires to equal `FREEINK_CAP_TOUCH`. |
| TP-02 | If the hibernate write NACKs, reset (≥5 ms, ≥300 ms) and retry. | **Done** in the shared SDK. |
| TP-03 | GT911: INT LOW ≥5 ms, `0x8040 = 0x05`, ≥58 ms, hold INT LOW. | **Done** in the shared SDK, built only with `[silkscreen] touch_flags = -DSILKSCREEN_TOUCH_ALT_JUMPERS=1` in `platformio.local.ini` (contingency, as in CrossPoint). |
| PB-01 | Bound the power-button release wait; if still held, sleep with ext1 ANY_LOW and re-sleep on that wake before any peripheral starts. | **Done** (`e59a7cfc`, `f9544b2e`).<br>- **Release wait:** SDK `deepSleepUntilPowerButton()` (5 s, `releaseWaitMaxMs`). `sleepImpl()` drops the clock to 80 MHz first.<br>- **Release wake:** `setup()` calls `HalPowerManager::takeReleaseWake()` right after the serial setup, then `startDeepSleepMinimal()`.<br>- **CrossInk's own tail:** it ignores `waitForPowerButtonRelease()`'s result and arms ext1 with the button still held. It never runs on Silkscreen (ZD-01's guard). |
| PB-02 | IO18 plain input, no pulls, awake and asleep. | **Done** in the shared SDK (`InputPins::powerExternalPull`). |
| WK-01 | Decide sleep-or-boot right after the wake-hold check, before SD, settings, light, panel, radio; re-sleep an unverified wake even if SD fails. | **Done** (`f9544b2e`).<br>**Order in `setup()`:**<br>1. `gpio.begin(deferTouch)`, with `deferTouch` = `PowerConfig::earlyWakeDecision`, so the touch probe waits;<br>2. `powerManager.begin()`;<br>3. `HalGPIO::verifyPowerButtonWakeup(readWakeShortPressFromNvs(), POWER_BUTTON_LONG_PRESS_MS)`: an unverified wake calls `startDeepSleep()`, an accepted one `gpio.startTouch()`.<br>Nothing before it starts SD, the settings, the light, the panel or the radio. Only `holdPowerRails()`, the serial setup, the release-wake and IO18 checks, `HalSystem::begin()`, the reset-loop check and RTC-memory bookkeeping run first. Since the 2026-10-19 sync `HalSystem::begin()` also asks whether PSRAM is in the heap (upstream's `fadf8462`, for an app-only firmware update over an old bootloader): the Silkscreen bootloader starts PSRAM, so on this board it is one `heap_caps_get_total_size(MALLOC_CAP_SPIRAM)` call and nothing is initialised.<br>**Differs from CrossPoint:** the wake rule is CrossInk's NVS mirror of the Short Power Button setting, not an RTC word. `readWakeShortPressFromNvs()` reads it; `mirrorWakeShortPressToNvs()` writes it after the settings load and at every sleep entry, only when it changed. NVS survives power loss, so every wake is decided before SD and the settings, the first wake after a power-on included. CrossInk's setting is kept, and it defaults to a long press (Short Power Button = Ignore). |
| WK-02 | A 30-60 s idle timeout after a verified wake with no input. | **Done with 180 s** (`wakeNoInputTimeoutS`, `647fc531`), in `src/main.cpp` `sleepOnBatteryAwakeLimits()`.<br>**Arming:** `setup()` sets `wakeNoInputArmed` on a `WakeupReason::PowerButton` boot. `noteBatteryInput()` disarms it, except for the wake press's own release (`wakePowerReleasePending`). A silent restart carries it (CI-04).<br>**When it fires:** on battery only, and not while a screen holds auto-sleep off. It sleeps with `enterDeepSleep(true)`, so "Quick Resume after timeout" applies. CrossInk has no plugin delivery to skip.<br>**SD-card error screen:** `setup()`'s SD-error path sets `storageErrorScreen`, and the screen sleeps after 60 s on battery (`storageErrorSleepS`).<br>Both count from `HalPowerManager::msSinceInput()`. |
| WK-03 | After 3 abnormal resets in a row, sleep at once without SD, panel or radio. | **Done** (`f9544b2e`). `sleepIfResetLoop()` runs after `HalSystem::begin()` and before `gpio.begin()`.<br>- It calls `HalPowerManager::abnormalResetLimitReached()`, which wraps SDK `takeAbortedSleepInfo()`, `countAbnormalResets()` and `clearAbnormalResets()`.<br>- At the limit: `resolvePanelController()`, `HalDisplay::sleepWithoutDriver()`, `HalGPIO::resetTouchForSleep()`, then `startDeepSleepMinimal()`.<br>- `loop()` calls `HalPowerManager::endAbnormalResetRunWhenHealthy()`, which ends the run after 60 s of uptime. It runs before the exclusive-storage return. |
| BAT-01 | Low-battery cutoff at boot and while awake, with Wi-Fi on too; block firmware writes near empty. | **Done** (`f9544b2e`, `647fc531`, `47e8e470`, `3312a1eb`, `a681d04a`, `aa3f8544`, `4779a55a`).<br>**Boot:** `sleepIfBatteryEmptyAtBoot()` → `HalPowerManager::batteryEmptyAtBoot()` runs after `halClock.begin()` and before `Storage.begin()`. It takes one 16× reading. Only a reading within 100 mV of the cutoff waits for the divider and ladder to settle, then takes `CUTOFF_SAMPLES` readings 1 s apart.<br>**Loop:** `sleepOnBatteryAwakeLimits()` → `lowBatteryCutoffTripped()` → `enterDeepSleep()`. The trip latches, and allows 100 mV while `WiFi.getMode()` is not `WIFI_MODE_NULL` (`isBelowCutoff(…, radioOn)`).<br>**Blocking work polls the same latch:**<br>- `src/network/HttpDownloader.cpp` `isCancelRequested()` is the abort predicate of every `HttpDownloader` transfer: the OPDS feed and books, the OTA check and download, and Manage Fonts with its retry loops. It also sets the caller's cancel flag.<br>- `UsbDriveActivity::batteryLimitReached()` (AW-03).<br>- WebSocket uploads return to `loop()` between frames.<br>- `CrossPointWebServerActivity::checkUploadCancellation()`, the callback of upstream #3732's `setUploadCancelCheck()` (`a681d04a`): a multipart (HTTP form) upload and a font upload, which block until they end, stop on the latch.<br>- `LibraryActivity::scanCancelRequested()` (`a681d04a`): the Library scan blocks the loop for as long as the card takes, and ends on the latch. Nothing else bounds it: it is a user action (opening Library or Refresh Library), and the awake ceiling and input cap are not polled while it runs.<br>- `CrossPointWebServer::handleDownload()` (`/download`) and WebDAV GET write through `CutoffAbortPrint` (`src/network/CutoffAbortPrint.h`, CrossPoint's `4821b7c2`; `CrossPointWebServer.cpp:971`, `WebDAVHandler.cpp:393`): its `write()` returns 0 once the latch is set, which ends the stream, and the next loop pass sleeps. WebDAV GET streams with `Storage.readFileToStream()` since upstream's #3410 came in with the `67100e66` sync (`aa3f8544`).<br>- WebDAV PUT's raw callback polls the latch and drops the connection (`WebDAVHandler.cpp:135`), which makes the server raise `RAW_ABORTED` and remove the temp file (`aa3f8544`).<br>- `StatsUploadClient::put()` and `probe()` ask the latch before each request (`StatsUploadClient.cpp:27`, `:65`, `:133`): a bulk sync is one request per day of stats, per book and per clipping (`StatsUploadActivity`, `KOReaderSyncActivity`), each step blocking the loop (`aa3f8544`).<br>- `StatsUploadActivity::flushProgressBatch()` asks it before each PUT of its one-book-at-a-time fallback (a server without the batch progress route: up to eight blocking requests in one loop pass; `4779a55a`). The other steps of upstream's incremental bulk sync (`88af432d`) are one request per loop pass (a change-feed page, one batch PUT, one book's progress) and go through `StatsUploadClient` for stats and clippings, so the loop's own check runs between them. The raw `KOReaderSyncClient` requests have no poll of their own (it is a library without the HAL); every loop that issues several of them in one pass must.<br>**Radio starts:** CI-05.<br>**Firmware writes:** a 3.6 V floor, less the radio allowance (`HalPowerManager::firmwareUpdateBlockedByBattery()`). It is checked in `OtaUpdateActivity::runUpdateInstall()`, and in `SdFirmwareUpdateActivity::validateFirmware()` and again in `performUpdate()`. A refusal shows "Battery too low, connect USB" (`STR_BATTERY_TOO_LOW_FOR_UPDATE`).<br>**Charge ladder:** the unknown-band hold and the `P+` cross-check (HW-08) are SDK `BatteryMonitor::pollChargeStatus()`, polled by CrossInk (CI-01). |
| I2C-01 | Nothing in flight at sleep; SDA/SCL released HIGH; stuck-bus recovery at boot. | **Done** (`e59a7cfc`). `sleepImpl()` → `freeink::i2c::releaseSensorBusForSleep()` on every Silkscreen sleep, minimal included. Recovery is in the shared SDK (`SharedI2c.h`). |
| RTC-01 | Clear DS3231 EN32KHZ; RV-8263 CLKOUT off. | **Done** in the shared SDK (`Rtc.cpp` `beginDevice()`). CrossInk adds (`07318ec8`):<br>- `HalClock::begin()` logs a device refused by the identity check, and a lost time;<br>- `HalClock::needsResync()` makes `WifiSelectionActivity::checkConnectionStatus()` run the NTP sync on every connection while the RTC has lost its time. |
| ADC-01 | IO1/2/4/8/9 inputs with no pulls; never `gpio_reset_pin` on them or on IO18/40/42/SD. | **Done** in the shared SDK (`InputManager::begin()`, `SharedAdc.h`, the ladder presence test). CrossInk adds no pin configuration on these pins. `HalPowerManager::begin()` sets the battery pin (IO8) to plain `INPUT`, as CrossPoint does. `HalGPIO::rawInputActive()` reads one conversion per released ladder (`InputManager::adcLadderContact()`). |
| AW-01 | Auto-sleep always armed on battery: a hard inactivity ceiling. | **Done** (`838b27b1`, `647fc531`) in `src/main.cpp` `sleepOnBatteryAwakeLimits()` → `HalPowerManager::awakeCeilingPassed(SETTINGS.getSleepTimeoutMs())`.<br>**The ceiling** is `HalPowerManager::awakeCeilingMs()`: the longer of 15 min (`awakeCeilingMin`) and the user's sleep timeout (1-30 min; Never counts as 15). It bounds Never and the screens that hold auto-sleep off, and never cuts short a timeout the user chose. `userTimeoutGoverns()` lets the user's timeout end the session alone wherever `loop()` reaches that check this pass: a timeout is set, no screen holds auto-sleep off, and no key is held. The timeout is CrossInk's own or Quick Lock's.<br>**Its own clocks:** it never counts from `loop()`'s `lastActivityTime`, which a keep-awake screen resets every pass.<br>- *Input:* `noteInput()`, through `noteBatteryInput()`. A key, touch or tilt input that Quick Lock did not reject counts; so do a key in the exclusive-storage branch and a Quick Lock unlock.<br>- *Client traffic:* `noteClientTraffic()`. It counts every web-server request through a middleware in `CrossPointWebServer::begin()` (`addMiddleware`, compiled only with `FREEINK_CAP_POWER_POLICY`). It also counts every WebSocket event (`CrossPointWebServer::onWebSocketEvent()`, reached through `wsEventCallback()`), every automatic page turn (`EpubReaderActivity::loop()`), and a USB host using the drive (`UsbDriveActivity::loop()`, Connected or Accessed).<br>- *Not traffic:* `OPTIONS`, `HEAD`, `PROPFIND`, `LOCK` and `UNLOCK`, which an OS sends to poll a mounted WebDAV share, and anything `handleNotFound()` answers, such as a phone's captive-portal probes in hotspot mode.<br>**Input cap:** 60 min with no input ends any screen on battery, whatever the traffic (`awakeInputCapPassed()`).<br>**Battery only:** both checks test `!gpio.isUsbConnected()`. |
| AW-02 | Radio off when a network screen's work ends; skip the loop delay only while working. | **Done** (`3312a1eb`). Per screen in [section 3.1](#31-network-screens-aw-02); the Nearby screens are CI-06. |
| AW-03 | USB Drive must end when the cable is pulled on battery. | **Done** (`647fc531`).<br>**The checks:** `UsbDriveActivity::loop()` checks two things in every state, IoError, Ejected and host suspend included, and calls `restartToHome()` on either:<br>- `cablePulledOnBattery()`: the ladder has shown external power this session, then reads OnBattery for 3 s, about 4-5 s after the pull;<br>- `batteryLimitReached()`: the cutoff, the ceiling or the input cap.<br>**Why it is needed:** `loop()`'s exclusive-storage branch returns before `sleepOnBatteryAwakeLimits()`, so nothing else can sleep while the host holds the card.<br>**After the restart:** the restart carries the awake clocks (CI-04), so `loop()` sleeps if a limit still holds. |
| BL-01 | The battery log (the `silkscreen-batlog` image only; [BATTERY-LOG.md](BATTERY-LOG.md)) adds nothing to the sleep current, to the wake path before the wake decision, or to the awake tail beyond one bounded ADC burst and one SD append per wake. | **Done, in that image only** (`10950eac` to `ee4e5fcf`): every line is inside `#if SILKSCREEN_BATLOG`, which only the `silkscreen-batlog` env defines.<br>**Sleep:** the only log code between `sleepImpl()` and `esp_deep_sleep_start` is `batterylog::tail()` at the head of the shared SDK's `PowerManager::deepSleep()`: a RAM-flag compare, or one ADC burst on IO8 under a `freeink::adc::Lock` taken with a 20 ms bound (a miss skips the burst and goes on) and one RTC store. No pin call, pull, hold, timer, task, wake source, I2C, Wi-Fi, NTP or light call is added. Expected added current zero, **not measured**: BATTERY-LOG.md M1 is the proof.<br>**Wake (WK-01):** the release-wake and reset-loop exits and `haltOnPowerButtonFault()` call no hook; the only log code they reach is the `tail()` compare, which returns at once because nothing is staged. `early(Ghost)` at the early ghost stores RTC words only; `onWake()` follows the early wake decision and precedes the touch probe; the boot-cutoff exit reuses its sample. CrossInk has no SD-fail or late-ghost exit.<br>**SD:** `BatteryLogWriter` makes one attempt per verified wake, from `loop()` after the render lock's scope (once the render task has been seen busy and 2.5 s have passed), plus `flushAtEntry()` in `enterDeepSleep()` before `Storage.shutdown()`, only with the radio off and no earlier bad attempt. The back-off (two bad attempts in a row skip the next 63 wakes, then one probe) lives in the RTC record. No file is left open, there is a 2 MiB cap, and nothing goes to NVS or flash.<br>**Where:** `src/main.cpp` (`stageBatteryLogEntry()`, `tickBatteryLog()` and the hooks), `src/util/BatteryLogWriter.cpp`, `lib/hal/HalPowerManager.*`, `HalGPIO.cpp` and `HalClock.cpp`; the SDK's `BatteryLog.cpp`. |
| MEAS-01 | Sleep-current acceptance test. | Procedure in [section 4](#4-measurement-and-bench-checks). **Not yet run** on either reader. |
| OPT-01 | Optional: switch RTC slow memory off in sleep. | **Not done**, as in CrossPoint (its POWER.md lists what that would lose). The `silkscreen-batlog` image's log record would be lost too (BL-01), and the S3's IDF sleep API cannot do it anyway: `SOC_PM_SUPPORT_RTC_SLOW_MEM_PD` is undefined and `rtc_sleep.c` forces both RTC memories on. |

**Rows that do not apply.** CrossInk has no plugin system and no loan clock (upstream #3114), so CrossPoint's plugin rules have no counterpart here:

- AW-01: the plugin-job claim poll, and the drain stop (`backgroundNetworkMustStop()` exists, but nothing calls it yet);
- BAT-01: the `/api/fetch`, `/api/relay` and plugin-catalog aborts;
- WK-02: the skipped `sleep.enter` delivery;
- AW-02: the plugin picker and the loan-clock Wi-Fi picker.

Port CrossPoint `4505ceec` when CrossInk takes them ([PORTING.md, Upstream merges](PORTING.md#upstream-merges)).

### Guard script and its exception

`scripts/check_sleep_wake_sources.py` (`e59a7cfc`) is CrossPoint's `check_sleep_wake_sources.sh`, rewritten in Python for CrossInk.

**When it runs:**

- as a `pre:` extra script of every Silkscreen env (`[silkscreen] extra_scripts`: `silkscreen`, `silkscreen-debug`, `silkscreen-batlog`, `silkscreen-368` and `silkscreen-368-batlog`), so a violation stops the build;
- standalone: `python scripts/check_sleep_wake_sources.py` exits with 1 on a violation.

It scans `src/`, `lib/`, `include/` and `freeink-sdk/libs/`, skipping comment lines and `test/` folders.

**It fails on:**

- any timer, ULP, touch-pad, ext0, GPIO, UART or radio wake, light sleep or the ultra-low sleep mode;
- `esp_sleep_pd_config()` anywhere but once in the SDK's `PowerManager::armWakeOnPins()`, after a statement that names `powerExternalPull` and a line that returns on `gpioMask`, with `ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON` (see below);
- `esp_deep_sleep_start()`, `esp_sleep_enable_ext1_wakeup()` or `esp_deep_sleep_enable_gpio_wakeup()` outside the SDK's `hardware/PowerManager/src/PowerManager.cpp`;
- CrossInk's sleep-tail calls (`waitForPowerButtonRelease()`, `armPowerButtonWakeup()`, `esp_sleep_config_gpio_isolate()`) outside the SDK;
- a `#if FREEINK_CAP_POWER_POLICY` branch of `lib/hal/HalPowerManager.cpp` that no longer calls `freeink::PowerManager::deepSleepUntilPowerButton()`.

**First exception: CrossInk's own sleep tail.** Every other board keeps it, byte for byte. The tail is the `#else` branch of `#if FREEINK_CAP_POWER_POLICY` in `HalPowerManager::startDeepSleep()`, and it calls the SDK's release wait, the GPIO isolation, the wake arming and `esp_deep_sleep_start()` itself. The guard allows those calls only in `lib/hal/HalPowerManager.cpp`, and there only in an `#else` branch of `#if FREEINK_CAP_POWER_POLICY`, which a Silkscreen build (`FREEINK_CAP_POWER_POLICY=1`) never compiles. Only the latch block was moved, unchanged, into `setPowerLatchesForSleep()`, which both paths share. objdump showed `startDeepSleep()` on x4-pro and x4-classic identical, instruction for instruction, before and after the move.

**The second exception: upstream's RTC pull.** `armWakeOnPins()` keeps RTC_PERIPH powered and puts the idle-level RTC pull on each ext1 wake pad (`ed88f03`). On IO18, which R62 and R76 bias, a pull-down leaves the pressed level at 2.50 V against VIH 2.475 V and a pull-up leaves the released level at 2.28 V, so `armWakeOnPins()` returns right after `esp_sleep_enable_ext1_wakeup()` when its mask is only a power pin the board biases itself (`InputPins::powerExternalPull`, set by `SILKSCREEN` alone). The call stays in the source and in the image (about 500 B) for the boards that bias nothing, and the guard allows exactly that one, behind that exemption: in the same function a statement naming `powerExternalPull` and then a line that returns on `gpioMask`, both ahead of the call, the arguments above, no second call, none in any other file. Tested against the tree and six broken copies of `PowerManager.cpp` and its surroundings (`c2913272`; the early return is `f7ca5843`).

The guard reads `#if`/`#else` lines as text; it does not run the preprocessor. So keep the exception's condition exactly `#if FREEINK_CAP_POWER_POLICY`, with nothing after it on the line. A reworded condition (`#if defined(...)`, or a trailing comment) turns the exception off, and the guard then fails on the tail's calls.

### 3.1 Network screens (AW-02)

**The gate.** `src/network/RadioIdle.{h,cpp}` holds `radioidle::enabled()` and `radioidle::release(tag)`, which wrap `HalPowerManager::radioOffWhenIdle()` and `releaseIdleRadio()`. Both are false in the simulator, whose HAL is external. Every rule below sits behind `radioidle::enabled()`, so other boards keep CrossInk's behaviour. Each network-boot screen still restarts on exit to clear the heap, also after it has switched the radio off.

- **OTA** (`OtaUpdateActivity::loop()`):
  - Wi-Fi goes off on NO_UPDATE and FAILED.
  - `radioReleased` keeps `onExit()`'s restart out of the network boot.
  - `skipLoopDelay()` holds only while checking or installing.
  - CrossInk's release feed has no Silkscreen asset, so a check ends at "no update" ([PORTING.md, Defaults](PORTING.md#defaults-chosen)).
- **Clock Sync** (`ClockSyncActivity::loop()`):
  - a radio this screen started goes off right after `runSync()`;
  - `skipLoopDelay()` holds only while SYNCING;
  - CrossInk's Clock Sync does not restart on exit.
- **KOReader Auth** (`KOReaderAuthActivity::loop()`): off on SUCCESS and FAILED. `onExit()` always restarts.
- **KOReader Sync** (`KOReaderSyncActivity::loop()`):
  - off on SYNC_FAILED of a single-book sync. A book of a bulk sync (`folderSync`) keeps the radio: its failure still uploads the book's stats and clippings, and the next book reuses the connection (`StatsUploadActivity::closeTransfer()` switches it off when the run ends). Upstream's bulk sync rework (`88af432d`, 2026-10-19 sync) made every such book a background child that fails to the next one on the same pass; before it, a release here cost a Wi-Fi reconnect after each failed book;
  - modem sleep (`modemSleepOn`) on SHOWING_RESULT, which can still upload (upstream removed NO_REMOTE_PROGRESS in the same rework: a missing remote position uploads at once);
  - modem sleep too on a bulk sync's SYNC_FAILED that only waits for Skip or Exit: the review of a kept conflict (`folderSync`, not `background`, after the extras and background branches of `loop()`). It is not released there, because the next choice reuses the connection, but its failed upload had left the station at full power until a key or the awake ceiling (estimated at 13-20 mAh over a 10-15 minute timeout, not measured; `5a0284e0`);
  - `performUpload()` turns modem sleep off again, and so does `uploadExtras()` before it sends the stats and clippings (Apply remote and Skip book reach it from the result screen, `d39bb3a7`); the next book's `onWifiSelectionComplete()` does too. Only the power-save mode changes in these places: every exit still ends as it did (a single-book sync's `wifiOff()` and `onExit()`, and, for a connection the run started, a bulk sync's `closeTransfer()`).
- **Bulk sync** (`StatsUploadActivity`): a connection it started (`ownsWifi`) goes off in `closeTransfer()` on Failed and Done, and `preventAutoSleep()` holds only while Uploading.
- **Asset downloads** (`AssetDownloadActivity`, Download Languages and Hyphenation Packs; upstream's `AssetDownloadService` transfers all go through `HttpDownloader`): `showError()` releases the radio when the popup can only leave the screen (`Pending::Exit`) or restart into a language that is already prepared (`Pending::Restart`, after a failed settings save: nothing is left to download; `9cb23736`). A Retry popup (`Pending::Fetch`, `Pending::Install`) keeps the connection until the user answers it or the awake ceiling ends the run, as Font Download does by design, because its Retry downloads again. `preventAutoSleep()` holds only while a step runs (`busy_`).
- **Calibre** (`CalibreConnectActivity::loop()`): off on ERROR, and `radioReleased` keeps the restart. While its server runs, AW-01 bounds it.
- **Font Download:**
  - `FontDownloadActivity::preventAutoSleep()` refreshes the idle timer once on COMPLETE or ERROR (`resultPolled_`), then the user's timeout governs.
  - `skipLoopDelay()` holds only while the manifest loads or a font downloads.
  - The radio stays on, since the lists behind the result screens download again.
- **Clear Cache** (`ClearCacheActivity::skipLoopDelay()`): only while CLEARING.
- **Backup Stats** (`BackupStatsActivity::skipLoopDelay()`): never, because the backup runs inside `loop()`.
- **Wi-Fi picker:**
  - `WifiSelectionActivity::preventAutoSleep()` holds only while auto-connecting, scanning or connecting.
  - `loop()` switches Wi-Fi off on CONNECTION_FAILED; every way on from that state already did.
- **Keyboard** (`KeyboardEntryActivity::preventAutoSleep()`): false, because each key is input.
- **Kept as CrossInk has them:**
  - The file-transfer web server keeps auto-sleep off while it runs, bounded by AW-01 with its traffic rules.
  - The OPDS browser keeps the radio while its list is open, because it browses over the network. While browsing it leaves sleep to the user's timeout (`OpdsBookBrowserActivity::preventAutoSleep()` is false in BROWSING and ERROR).

### 3.2 CrossInk-only rules

These cover CrossInk code that CrossPoint does not have, or that CrossInk arranges differently.

| Rule | Requirement | CrossInk status and location |
|---|---|---|
| CI-01 | USB presence comes from the charge-status ladder, so the battery-only rules never fire on a charger. | **Done** (`e5466f66`).<br>- `HalGPIO::update()` calls `BatteryMonitor::pollChargeStatus()` on every pass. That includes the exclusive-storage loop, through `mappedInputManager.update()`.<br>- `HalGPIO::isUsbConnected()` asks the ladder first (`isExternalPowerPresent(&known)`). It keeps its last answer until the ladder decodes, and reads "not connected" after `chargeStatusLost()`.<br>- Every "on battery" test above uses it, so a data-less wall charger counts as external power and shows the charging bolt.<br>Without it: a charger read as "no USB", the ceiling, cap and wake timeouts fired while charging, and USB Drive's cable check never fired. |
| CI-02 | The battery checks run before every early return in `loop()`. | **Done** (`647fc531`). `loop()` runs in this order:<br>1. `mappedInputManager.update()`;<br>2. `endAbnormalResetRunWhenHealthy()`;<br>3. the exclusive-storage branch (USB Drive), which returns early; AW-03 covers it;<br>4. the tilt sensor;<br>5. `settleFrontlight()`;<br>6. the serial service and the input bookkeeping;<br>7. `noteBatteryInput()`;<br>8. `sleepOnBatteryAwakeLimits()`;<br>9. only then every other early return (the wake release, the screenshot chord, the side-button and power chords, Quick Lock with its own timeout, the chord release, the Home key), and CrossInk's own auto-sleep.<br>Placed after them, as CrossPoint had them until its held-key fix, Quick Lock with Never or a key held in a bag would keep the board awake until the cell was flat.<br>**Rate:** those early returns also skip the idle wait at the end of `loop()`. On `FREEINK_CAP_POWER_POLICY` boards the Quick Lock branch ends with `idleWaitBeforeEarlyReturn()` (the same 3-s step down to the low clock and `gpio.idleDelay(50)`, which a key contact ends early), and the held-chord returns (screenshot, side-button and power chords) wait 10 ms, so a locked board or a held chord no longer spins at full clock until its limit. |
| CI-03 | Presses that Quick Lock rejects do not count as input. | **Done** (`647fc531`).<br>- `noteBatteryInput()` is skipped while `ButtonShortcutController::isQuickLocked()`.<br>- Every unlock passes `notifyQuickLockChanged()`, which counts it.<br>So a locked reader with Never, pressed now and then in a bag, still sleeps 15 min after it was locked. This matches CrossInk's own Quick Lock timer, which counts from the moment of locking (`QuickLockState::shouldSleep()`). |
| CI-04 | A silent restart does not reset the awake clocks. | **Done** (`647fc531`).<br>**Carry:** `restartWithSilentToken()` calls `HalPowerManager::carryAwakeClocks(wakeNoInputArmed)`. Every `silentRestart*()` passes through it: low-heap reader restarts that resume an automatic page turn, network-boot entry and exit, and the USB Drive handoff. It saves the time since the last input, the time since the last client traffic, and the WK-02 flag in `RTC_NOINIT` memory, with a magic word.<br>**Resume:** `setup()` calls `resumeAwakeClocks()` right after `powerManager.begin()`. It takes the values back only after `ESP_RST_SW`, and always clears them.<br>**USB:** the carry also keeps whether external power was present (`HalGPIO::isUsbConnected()`). After a restart on external power, `resumeAwakeClocks()` seeds `HalGPIO::seedUsbConnected(true)`, which `isUsbConnected()` answers until the charge-status ladder decodes (about 1.5 s, or about 9.5 s for Complete). Without it, the first `loop()` pass read "on battery" before the ladder decoded, and carried clocks past the 60-min cap slept a board still on the cable (USB Drive ejected after an hour's copy).<br>So no restart extends a limit. The `ESP.restart()` after a firmware update starts fresh. |
| CI-05 | A shortcut does not start the radio near empty. | **Done** (`3312a1eb`).<br>**The check:** `radioShortcutRefused()` is declared in `src/GlobalActions.h` and defined in `src/main.cpp`; without `FREEINK_CAP_POWER_POLICY` it is always false. It covers SYNC_PROGRESS, NEARBY_POSITION_SYNC, FILE_TRANSFER, CALIBRE_WIRELESS, JOIN_NETWORK and CREATE_HOTSPOT, and their long-press-menu forms. While `HalPowerManager::radioStartBlockedByBattery()` holds (the cutoff has latched, or the last reading on the cell, with no radio allowance, is at or below the cutoff), it consumes the shortcut: the log shows "not starting the radio" and nothing opens.<br>**Callers:**<br>- `handleGlobalPowerButtonAction()` and `dispatchShortcutAction()` (short and long Power, the power and side chords, the Home key, Quick Actions);<br>- `EpubReaderActivity::executeReaderQuickAction()`, and the SYNC and NEARBY_POSITION_SYNC entries of its `onReaderMenuConfirm()`;<br>- the TXT and XTC readers' `executeReaderShortcutAction()` and `executeLongPressBackAction()`.<br>A shortcut that restarts into the network boot also meets the boot cutoff. |
| CI-06 | The ESP-NOW Nearby screens bound their radio time. | **Done** (`3312a1eb`). Each of `NearbyStatsSyncActivity`, `NearbyBookPositionSyncActivity` and `NearbyBookTransferActivity` runs `stopIdleRadio()` from `loop()`.<br>**Waiting:** listening or discovering stops after `radioidle::NEARBY_WAIT_MS` (2 min) with ESP-NOW off. The screen shows "Stopped waiting to save battery" (`STR_NEARBY_STOPPED_WAITING`), or, for a sender with no receiver, "No nearby reader found".<br>**Found readers:** the transfer's list keeps its peers but drops the radio after 2 min; `selectPeer()` → `startRadio()` brings it back.<br>**Offers:** an offer prompt ends after the sender's approval window (`MAX_APPROVAL_RETRIES` × `RETRY_INTERVAL_MS` × 2, about 54 s), with `STR_NEARBY_TRANSFER_TIMEOUT`.<br>**Results:** errors switch ESP-NOW off at once. Results switch it off at once, or after `NEARBY_RESULT_LINGER_MS` (5 s) where the peer may resend: the stats SYNCED screen, the position SHOWING_RESULT screen and the receiver's Success.<br>**Keep-awake:** `preventAutoSleep()` holds only while an exchange runs, which each screen's own timeouts bound:<br>- stats: DISCOVERING and SYNCING;<br>- position: also APPLYING;<br>- transfer: Discovering, WaitingForApproval, Validating, Sending and Receiving.<br>**Restarting:** `startSync()` restarts ESP-NOW when it is off, so Sync and Share work again.<br>Leaving still restarts (`radioUsed_`, `radioActivated_`). `WIFI_PS_NONE` now runs only inside these windows. |
| CI-07 | A short key tap at idle is not lost. | **Done** (`1a9c2c09`).<br>- `HalGPIO::idleDelay(50)` replaces the flat 50 ms idle wait in `loop()` and in the exclusive-storage branch.<br>- On `InputStyle::AdcLadders` boards it waits in 10 ms slices and returns on a raw contact: `HalGPIO::rawInputActive()`, which is IO18, or `InputManager::adcLadderContact()` with one conversion per released ladder. Other boards keep one `delay(50)`.<br>Without it, a 20-30 ms ladder tap could fall between two idle samples, because the SDK's debounce sees it only at its second sample. |
| CI-08 | A front-light switch-on's result is settled while awake. | **Done** (`7cc2982d`, `41c02221`). `src/main.cpp` `settleFrontlight()` runs on every `loop()` pass (`FREEINK_CAP_FRONTLIGHT_PROBE` only). `HalFrontlight::update()` acts on each probe result:<br>- **Trip:** `handleFrontlightTrip()` switches a tripped light off and saves it as off.<br>- **User switch-on with no light:** saved as off (`takeNoLightNotice()`), unless the screen shows the result itself (`Activity::showsFrontlightNotice()`, which only the light panel overrides).<br>- **Restore with no light:** keeps the saved "on" (`takeRestoreNoLightNotice()`).<br>- **Missing string:** the saved warmth is moved onto the string that is there.<br>Each of the first three shows "No front light detected. Wait about 10 s before connecting a light." (`showFrontlightNotFound()` → `BaseTheme::drawWrappedPopup()`). The popup comes from the second loop pass, after the screen has been drawn again from the settled state. |

## 4. Measurement and bench checks

The board-level procedure is CrossPoint's: its POWER.md section 4 and the test firmware's P5 ([TEST-SUITE.md](TEST-SUITE.md)). On the same board, the CrossInk image should then give the same floor on every CrossInk sleep path. A difference points at CrossInk's app, not the board.

**Sleep-current test (MEAS-01) on the CrossInk image**

1. Use the release env, `silkscreen` (`silkscreen-368` on a 3.68" unit, [PORTING.md](PORTING.md#the-368-variant)), for every current and timing. `silkscreen-debug` adds a 250 ms USB-serial wait (`CROSSPOINT_WAIT_FOR_USB_SERIAL`) to every boot, ghost wakes and re-sleeps included.

   **Log lines.** Lines marked *(debug)* below are `LOG_DBG`, so only `silkscreen-debug` (`LOG_LEVEL=2`) prints them; the release env (`LOG_LEVEL=1`) prints errors and info only. Check those lines on the debug env, apart from the current and timing runs.
2. Put the cell, or a 3.8 V source, on J5 through a µA meter or a PPK2-class logger.
3. Set up as for CrossPoint:
   - USB unplugged, and any UART adapter off TP1/TP2;
   - the SD card fitted, the panel connected;
   - the light off, J6 empty.
4. Wait at least 60 s after sleep entry, then average over at least 60 s. The DS3231 converts every 10 s (≈575 µA for ≈10 ms), so short readings mislead.
5. Check a 30-min log: the conversion blip should be the only structure, with no boot spikes. After a wake, the log must show wake cause EXT1.

**Expected results.** As in section 1:

- ≈58-73 µA without touch;
- ≈113-130 µA with an FT6336U hibernated: the log shows `Touch controller asleep` *(debug)*;
- the 3.68" variant has no touch: the first figure, adjusted by that panel's own deep-sleep draw, which is not in hand (row 17; the J2 flex A/B finds it);
- above ≈150 µA, work through CrossPoint's A/B list and "what a high reading points at" (its POWER.md section 4).

**Repeat after each CrossInk sleep path.** All should agree within ≈2 µA, and with CrossPoint's figure on the same board:

- menu sleep, auto-sleep and the Quick Lock timeout;
- the awake ceiling (15 min, or the sleep timeout if longer) and the 60-min input cap;
- the loop and boot cutoffs;
- the ghost-wake and release-wake re-sleeps;
- the SD-error screen's 60 s and the 180 s wake-no-input timeout;
- the USB Drive session ended by the cable pull or a limit, then the sleep after its restart;
- 3 forced panics in a row, with the panel refreshed fast before them: the driver-free panel sleep must bring it to the same floor. No image forces a panic (CrossInk has no test firmware, and no shipping image gets a hook): [TEST-SUITE.md's reader-image checks](TEST-SUITE.md#checks-that-need-the-reader-image) give the scratch patch. On a 3.68" unit this is also the only run of the 60 ms driver-free settle on a board (host-tested only so far).
- With R72 fitted next to R36 (a rework only), the power-button fault sleep, at the floor plus ≈18 µA.

**Bench checks specific to CrossInk.** The pin, back-feed, charger, touch, RTC and key-ladder checks are board checks and stay with CrossPoint's procedure. On the CrossInk image, on battery (a bench supply on J5 where a voltage is named):

- **Wake:**
  - a short tap with the default long-press wake stays awake ≤0.3 s, with no `Touch controller` probe lines before `Power-button wake not held` *(debug)*;
  - with Short Power Button set to Sleep, a click wakes;
  - hold Power through a sleep entry for more than 5 s: the board sleeps, wakes on the release, logs `Woke on the power button's release; sleeping again` *(debug)*, and sleeps again.
- **Battery:**
  - the supply ramped down sleeps the board at ≈3.40 V, or ≈3.30 V as read with Wi-Fi on;
  - an OPDS or font download aborts first;
  - at ≈3.35 V a boot logs the boot cutoff and sleeps before SD starts. On USB it must not.
  - Below 3.6 V, OTA and the SD firmware update show "Battery too low, connect USB".
  - At ≈3.4 V a radio shortcut logs `not starting the radio`, and nothing opens.
- **Awake limits:**
  - Timeout Never on Home: sleeps ≈15 min after the last key (`No input or client traffic for 15 min`).
  - Timeout 20 min on the file-transfer screen:
    - with no client, it sleeps at ≈20 min;
    - with a browser uploading, it stays up;
    - with only a phone joined to the hotspot (captive-portal probes), it sleeps at ≈20 min, as with no client (probes are not traffic).
  - Timeout 10 min in the reader: sleeps at 10 min.
  - Up+Down held on a reader with a 10-min timeout: sleeps at ≈15 min, not never.
  - An automatic page turn left running is still turning after 15 min, and sleeps 60 min after the last key.
  - Quick Lock with Never and occasional presses: sleeps 15 min after locking.
  - A power-button wake left alone: sleeps after 180 s.
  - No SD card: sleeps ≈60 s after "SD card error".
  - A low-heap reader restart: `silkscreen-debug` logs `Awake clocks resumed`.
- **Radio:**
  - The idle current on the OTA "no update" and failed screens, the Clock Sync and KOReader sign-in results, a failed KOReader sync and the Calibre error equals the normal idle current (Wi-Fi off; the log shows `Work done; Wi-Fi off` *(debug)*). Back still restarts out of the network boot.
  - A KOReader result that offers Upload sits in modem sleep, and Upload still succeeds; so do the reading stats and clippings after Apply remote (they leave modem sleep before they go out). In a bulk sync's review of a kept conflict, a failed upload leaves the station in modem sleep, not at full power, until Skip book or Exit.
  - The Wi-Fi list with a 1-min timeout sleeps after 1 min, and a failed manual connection turns Wi-Fi off.
  - Nearby: each listen or discover window ends after 2 min with the radio off, and results release the radio. Sync, Share and a later pick still work against another reader. An unanswered offer prompt ends after ≈54 s.
- **USB Drive:** pull the cable while connected: `USB cable gone` ≈3-5 s later, then a restart to Home.
- **Guard:** `python scripts/check_sleep_wake_sources.py` passes. It also runs before every Silkscreen build.

## 5. The awake idle tail

After every session the device stays awake until auto-sleep, and this tail outweighs the sleep floor.

- **The idle path.** After 3 s at the normal clock (`HalPowerManager::IDLE_POWER_SAVING_MS`), `loop()` drops to 80 MHz (`HalPowerManager::LOW_POWER_FREQ` with PSRAM). It then waits in `HalGPIO::idleDelay(50)`, in 10 ms slices. The prebuilt core has no `CONFIG_PM_ENABLE` or tickless idle.
- **Full clock.** Any screen that holds auto-sleep off also keeps the normal clock, because `loop()` calls `setPowerSaving(false)` on every pass. Wi-Fi on forces power saving off (`HalPowerManager::setPowerSaving()`).
- **Draw:** ≈22-36 mA for the chip at 80 MHz with the CPU waiting [DS, S3 DS v2.2 Table 5-9]. PSRAM standby and a powered, mounted SD card (0.2-2 mA) take the board to ≈25-40 mA [est].
- **CrossInk's default 10-min auto-sleep** (`sleepTimeoutMinutes` 10): ≈3.7-6 mAh per session for the chip, ≈4-6.5 mAh for the board [est]. That equals 2.7-4.3 days of the 58 µA floor.
- **Awake ceiling** (Never, or a screen that holds auto-sleep off): 15 min, or up to 30 min when the user's sleep timeout is longer.
  - Never on an ordinary screen idles at 80 MHz: ≈5.5-9 mAh per 15 min.
  - A keep-awake screen runs at the normal clock, so more.
  - With Wi-Fi on, the clock is full and the radio draws tens to over 100 mA: ≈12-30 mAh per 15 min [est].
- **Automatic page turns** count as client traffic, so a session left turning in a bag ends at the 60-min input cap: up to an hour at the normal clock, plus the refreshes [est: tens of mAh].
- **Nearby (ESP-NOW)** runs at `WIFI_PS_NONE`, ≈100 mA [est], for at most 2 min per listen or discover window (≈3.3 mAh), plus the exchange itself. Before CI-06 a forgotten Nearby screen ran until the 15-min ceiling.
- **A wake with no input** pays 3 min of the tail on battery (WK-02), not the whole timeout.
- **Sleep entry.** CrossInk has no plugin events, so a sleep entry costs only its SD writes (ZD-07) and the panel's sleep screen.
- **Levers not used yet** (measure first):
  - a lower idle clock (40 MHz: 13.2-18.8 mA [DS]; not checked with octal PSRAM);
  - a shorter default timeout;
  - ESP-NOW's connectionless power save (`esp_now_set_wake_window()` and the wake interval) inside the Nearby windows. It needs a bench check of receive reliability first.

## 6. Hardware notes for a future revision

- **HW-01 (highest risk, unresolved): GPIO47/48 pad supply.**
  - The problem: EPD RST (IO47) and BUSY (IO48) are on the SPICLK_P/N pads. S3 DS v2.2 Table 2-1 gives their supply as VDD_SPI/VDD3P3_CPU, chosen by `EFUSE_PIN_POWER_SELECTION`, and IDF 5.5 always powers VDD_SPI off in deep sleep (keeping it on is not an option there).
  - The cost if these pads are unpowered: R5 and the SSD1677's HIGH BUSY output back-feed flash/PSRAM through the ESD diodes, ≈100-300 µA [est].
  - The fix: move RST and BUSY to VDD3P3_RTC or VDD3P3_CPU pads. IO3 is free for RST. BUSY needs a strap-safe pad: not IO45, and IO46 only after checking the boot-mode strap table.
  - HARDWARE.md §6.1 carries it as a first-article check since 2026-09-30.
- **HW-02: J4 touch VDD is always on (0 Ω R42).** A hibernated FT6336U still draws ≈55 µA and a GT911 70-120 µA. Only a load switch on J4 VDD reaches the ≈58 µA floor with touch. That switch needs bus isolation, or TP_RST/TP_INT held LOW, because R47/R48 and the control lines back-power an unpowered controller.
- **HW-03: the U3 TLV75533P is the largest floor term.** Its 25 µA IQ has an uncharacterized light-load term, and it rises to ≈190-260 µA in dropout. The fix: an LDO with flat dropout IQ and nA shutdown, rated for Wi-Fi peaks, with EN driven by a supervisor or load switch that disconnects the system below ≈3.4 V.
- **HW-04: the LED_MONIT divider (R39/R41) is always fed from LED_SW,** ≈2.9-3.4 µA with the light off. Scale it ×10 or gate it, after checking the ADC and monitor settling.
- **HW-05: RTC choice.** U14 RV-8263-C7 (0.19/0.24 µA) instead of U13 DS3231MZ (2/3 µA) saves ≈1.8-3 µA. The firmware already probes 0x51 as the fallback.
- **HW-06: the Fix-4 detector (≈10 µA) and BAT_MONIT (1.85 µA) are deliberate.** An N-FET gate on BAT_MONIT would save ≈1.9 µA.
- **HW-07: clone parts have unverified leakage** (SD05C-01FTG, TPD4E1U06, DW01A, 74LVC1G04, BSS138). Only a board measurement settles them.
- **HW-08: TP4056 CE is held at 3V3 while its VCC is 0 V on battery.** No datasheet row covers this. If CE, CHRG or STDBY conduct into VCC, D2 lights (≈500 µA) and USB_STAT reads ≈1.2 V ("charging"). Bench check in [section 4](#4-measurement-and-bench-checks). The firmware no longer trusts such a reading near empty: Charging/Complete with `P+` at or below the cutoff and not rising for 5 min is treated as on battery, and logged once. T2 stage 1 checks the level on each board (`hw08_clamp`).
  - **Contingency: a `P+` trend check. Not implemented; implement it only if T2 stage 1 fails `hw08_clamp` on real boards.** With the clamp, a board on its cell decodes "Charging" and is taken for one on USB. The cutoff still protects the cell (the cross-check above, about 5 min late), but until then the battery-only timeouts (the 15-min ceiling, the 60-min input cap, the 3-min wake timeout) do not apply, USB presence reads connected and the UI shows a charge.
  - The check would extend the HW-08 cross-check above the cutoff (`BatteryMonitor::pollChargeStatus()`). While the ladder decodes Charging, a real charge holds `P+` at the charger's output, rising in CC or at the 4.2 V float in CV, so `P+` falling across a window means the cell carries the load, and the state would be published as OnBattery, as the cross-check does today.
  - It would sample once a minute and 16× averaged, as the cross-check does, but (unlike it) only at a quiet moment, since the 0.24-0.35 Ω path reads low under load (§3.6), and with the window restarted on any ladder change (the cross-check restarts it only when the state leaves Charging/Complete).
  - The window and the fall that counts must come from the board: log T1 and T2 on a clamped board, on the cell and on a charger, before choosing them. Complete is harder, since a charged cell relaxes after termination.
  - It adds no wake source, hold or pull, and nothing in sleep.
- **HW-09: 2.5-6.5 µA keep flowing from the raw cell after a DW01A over-discharge trip.** §3.3's "years from 2.4 V to flat" is doubtful; months is more likely.
- **HW-10: held keys.** A held power button draws 30 µA through R62/R76, and a held ladder key 50-330 µA. Higher-value ladder resistors are the only mitigation.
- **HW-11: no GPIO senses VBUS.** A VBUS divider to a spare RTC pad (IO3) would make cable-pull detection deterministic; today USB_STAT stands in for it.
- **HW-12: TP_INT (IO41) is a digital pad and cannot wake the chip** (§8). That is fine for power. A GT911's INT hold works only because `gpio_deep_sleep_hold_en` is set, and it ends at the wake reset, so the firmware re-drives it on every boot and sleep entry before the probe. An RTC pad (or a board pull-down) would keep it LOW through the wake.

**Follow-ups for HARDWARE.md** (not edited from this repo):

- Done in HARDWARE.md on 2026-09-30:
  - §9.2 and §13.1 now say ext1 ANY_HIGH, not `ext0`;
  - §16 is itemized as in [section 1](#1-sleep-current-budget), with ESP32 7 µA typ and the touch and dropout cases;
  - §6.1 has the GPIO47/48 back-feed as a first-article check (HW-01);
  - §9.2 forbids R72 with R36 still fitted, and says a board shortened at the cut line needs the UP(2)-as-power option;
  - §13.1 covers the front light's open-load enable and the 10 s wait before connecting a light.
- Still open:
  - §3.3's "years" claim should change (HW-09).
  - §13.1 should list three parts as sleep/wake-critical: R75 (COLOR_SEL pull-down), U10's ADIM pull-down and R5 (EPD RST pull-up). S3 drops digital-pad holds at a deep-sleep wake, so only these parts hold IO40/IO42/IO47 until firmware drives them.
  - R49/R50: a 1-2 LED string on J3 would conduct in sleep through Q6 and R37. The bonded 5-LED strings do not.

## 7. Open questions

- **GPIO47/48 supply on this N16R8,** and whether it back-feeds in sleep (HW-01, bench check in [section 4](#4-measurement-and-bench-checks)).
- **The custom UC8279D panel in deep sleep:** its current after DSLP (row 17) and the level its `BUSY_N` holds on IO48. A LOW `BUSY_N` would feed the IO48 pad differently from the SSD1677's HIGH `BUSY` (HW-01).
- **FT6336U behaviour:**
  - the hibernate current at 3.3 V (55 µA is at 2.8 V, no max);
  - whether `0xA5 = 0x03` ACKs in Monitor mode; if not, every sleep entry adds the ≈307 ms reset retry;
  - the real RSTN pull-up current;
  - whether the fitted tail has its own pull-ups.
- **TP_INT floats in sleep.** IDF isolates IO41 and the board has no pull. If noise wakes an FT6336U from hibernate (an INT LOW pulse does), sleep current rises toward Monitor (220 µA). If measured, hold IO41 HIGH in sleep.
- **The TLV755P ground current at a 20-40 µA load:** the largest budget uncertainty.
- **The cutoff:**
  - Does 3400 mV keep the resting P+ at or above ≈3.5 V?
  - What P+ offset (12-175 mV, load-dependent) applies at the moment of the check?
  - Is the 100 mV Wi-Fi allowance right (0.24-0.35 Ω × 100-300 mA; ≈175 mV at 500 mA)?
  - Is a brown-out level change (PWR-04) possible on the prebuilt core?
  - Does the chip brown out or loop through resets in LDO dropout, and at what cell voltage?
- **The ext1 ANY_LOW release wake** relies on R76 holding IO18 LOW with RTC_PERIPH off. IDF holds the pad in its ext1 prepare, but this is not bench-verified. A bouncing or stuck button cycles with ≈5 s awake per cycle.
- **What state un-held RTC pads take in S3 deep sleep** with RTC_PERIPH off: IO1-4, IO8, IO9 (ESP-IDF is silent). Probe them in sleep.
- **Temperature:** ESP32 deep-sleep current above 25 °C (13 µA is a placeholder), and the MLCC, FET and TVS leakage terms at 60-85 °C (a device left in a hot car).
- **Gating the SD card between accesses while awake** (0.2-2 mA against a ≈200 ms remount): a product decision.
- **Do the prebuilt Arduino-ESP32 3.3.11 IDF libraries match the local framework-espidf 5.5 source** that these conclusions read (pad isolation, pull routing, VDD_SPI power-down)?
- **The cell capacity (mAh),** needed to turn these figures into storage-life claims.
