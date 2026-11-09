# Silkscreen battery log on CrossInk

The battery sleep/wake log is documented once, in CrossPoint's `crosspoint-reader/docs/silkscreen/BATTERY-LOG.md`, next to
this repo in `silkscreen-firmware/`: what the build is, how to run a campaign, retrieve the file and read a row, what the log
can and cannot find, the bench proof and the DMM recipe. Both readers carry the same SDK code, write the same CSV and use the
same analyzer (`crosspoint-reader/scripts/silkscreen/battery_log_report.py`). This page lists only what differs on CrossInk.

## The build

- **Its own env.** `silkscreen-batlog` is `[env:silkscreen]`'s flags plus `-DSILKSCREEN_BATLOG=1`, with the same
  `LOG_LEVEL=1` and no USB-serial wait, so boot timing and the ghost-wake path match `silkscreen`. There is no setting, screen,
  i18n key or Quick Action: stop logging by flashing `silkscreen`.
- **The screen keeps 1% steps.** The release image uses 5% steps. Both now apply
  the [trusted display policy](#trusted-display-percentage), including its charge compensation and
  deadband. `e_ui` records the filtered estimate in tenths of a percent, while
  `w_ui` comes from the wake sample. Neither necessarily equals the number drawn.
  The existing analyzer rounds these fields to 1%; it does not reconstruct the
  display's new hysteresis or charge compensation. Older examples below describe the
  earlier display behavior. The CSV format and raw voltage measurements are unchanged.

- **Flash:** `pio run -e silkscreen-batlog -t upload`. `scripts/silkscreen/build_matrix.sh` builds it after `silkscreen`.
- **Size** (the final build matrix, 2026-10-04): 5622425 B of flash and 116080 B of static RAM, 8820 B and 544 B more than
  `silkscreen` (5613605 / 115536). The image is 5627616 B, 925984 B free in the 6553600 B slot (`silkscreen`: 934800 B free).
  Free RTC slow memory is 1464 B, against 1648 B on `silkscreen`: the record is 184 B. Every other env is the size it was,
  to the byte. CrossPoint's BATTERY-LOG.md section 1 has both readers' figures. Since then the second key-access audit moved
  the two to 5625565 / 116080 (`batlog`) and 5616853 / 115536 (`silkscreen`), and the 5% battery step (2026-10-05) added
  28 B to `silkscreen` (5616881) and nothing to this image: its footprint over a `silkscreen` built after the step is
  8684 B of flash and 544 B of RAM (the image is 5630768 B, 922832 B free). After the 2026-10-19 sync (SDK `6f87cbd`): `silkscreen-batlog` is 5367225 / 122144 against
  `silkscreen` 5358517 / 121600, a footprint of 8708 B of flash and 544 B of RAM (the image is 5372400 B, 1181200 B free);
  the record is still 184 B (`.rtc_noinit` 6152 B against 5968 B). After the fix pass of the same day (tree `05072cb1`, SDK `bee6382`): `silkscreen-batlog` is 5367285 / 122144 against `silkscreen` 5358577 / 121600, the same footprint, 8708 B of flash and 544 B of RAM (the image is 5372448 B, 1181152 B free), and the record is still 184 B. After the render watchdog (tree `9755e7ed`, SDK `bee6382`): `silkscreen-batlog` is 5368805 / 122176 against `silkscreen` 5360089 / 121640, a footprint of 8716 B of flash and 536 B of RAM (the image is 5373968 B, 1179632 B free), and the record is still 184 B. After the watchdog's fix pass (tree `42430c78`, SDK `bee6382`): `silkscreen-batlog` is 5370125 / 122200 against `silkscreen` 5361333 / 121664, a footprint of 8792 B of flash and 536 B of RAM (the image is 5375296 B, 1178304 B free), and the record is still 184 B.
- **Version and image.** `1.6.1-silkscreen-batlog`: `scripts/git_branch.py` injects `CROSSINK_VERSION` only for the envs it
  lists, so this env is in its set (without that, `BuildInfo` would report `dev`). The image is
  `.pio/build/silkscreen-batlog/firmware.bin`, with `firmware-silkscreen.bin` beside it. `custom_firmware_device_type` stays
  `silkscreen`, so the SD card firmware update swaps it with the `silkscreen` image in either direction.
- **The 3.68" variant has its own env.** `silkscreen-368-batlog` is `silkscreen-368`'s flags plus `-DSILKSCREEN_BATLOG=1`: the X3 panel, no touch, the same log code and hooks (the variant is the same device and `Board::Silkscreen`). Flash it with `pio run -e silkscreen-368-batlog -t upload`, and go back with `pio run -e silkscreen-368 -t upload`. Its version is `1.6.1-368-silkscreen-batlog` (`scripts/git_branch.py`), which still ends `-silkscreen-batlog`, so the analyzer takes CrossInk's 500 ms early-wake limit from it (`-368-batlog` would read as CrossPoint's). The board tag (`silkscreen-368`) and `custom_firmware_device_type` (`silkscreen368`, so that the 4.26" board's OTA asset matcher cannot take the variant's `firmware-silkscreen368.bin`) are the release image's, so the two swap over SD. Size (2026-10-12, with the SDK at `5fb944d`, after the UC8279d-only stage): 4,945,281 B of flash and 117,000 B of static RAM, 8,732 B and 536 B over `silkscreen-368` (4,936,549 / 116,464; the 4.26" pair differs by 8,564 B and 536 B); the image is 4,950,432 B, 1,603,168 B free in the slot. After the 2026-10-19 sync (SDK `6f87cbd`, built at `c23f1a16`): 5,230,825 B of flash and 118,392 B of static RAM, 8,776 B and 544 B over `silkscreen-368` (5,222,049 / 117,848); the image is 5,235,968 B, 1,317,632 B free (`silkscreen-368-batlog` was not rebuilt after the fix pass of the same day, which adds about 48 B to the 3.68" images: `silkscreen-368` is 5,222,097 / 117,848 since). Before the stage (2026-10-11, the review's fix pass) it was 4,952,841 B (4,944,513 for `silkscreen-368`), 8,328 B over: the UC8253 driver, the stock probe and the verdict cache left both images, and the log's own 404 B larger overhead is the linker re-encoding calls in unrelated code (the CSS parser's functions), not a symbol of the log. The analyzer's reference floor, the no-touch 58 µA (Rev 1.02: 48 µA) plus self-discharge, is the 4.26" board's: the `cfg` column's touch bit is 0 on this build, but this panel's own deep-sleep current is not in hand ([POWER.md](POWER.md) row 17). A floor a few µA above it is not by itself a fault, and the first campaign sets this panel's.
- **Gated in code.** Every line of it is inside `#if SILKSCREEN_BATLOG`, which only this env defines; every other env compiles
  to what it did before. The one blocker that runs the other way round is the release image's 5% display step
  (`#if FREEINK_DEVICE_SILKSCREEN && !SILKSCREEN_BATLOG`), which this image compiles out. CrossInk's own
  `BatteryDiagnosticLog` stays compiled out and untouched.
- **What has run.** As of 2026-10-12 the one CSV recorded (2026-10-10, 27 rows over 5.7 days) came from one build, CrossPoint's `1.6.5-batlog`. No CSV from this repo's `-silkscreen-batlog` versions is recorded, and CrossInk's variant images have never been flashed.

## Where it is hooked

All of it reads or notes state; nothing lights the light, drives a pin, starts the radio or an I2C transaction, and the only SD
access is the writer's one attempt per wake.

| Where | Hook |
|---|---|
| `src/main.cpp`, `enterDeepSleep()` | `stageBatteryLogEntry()` at the very top, before the CPU is forced to full speed; `flushAtEntry()` after the reading-stats backup, before the card goes down |
| `src/main.cpp`, the call sites that know why the sleep was ordered | `tagPath()` for the low-battery cutoff, the awake ceiling, the input cap, the unattended wake, the SD-error screen and the Quick Lock timeout |
| `src/main.cpp`, `setup()` | `early(Ghost)` at the early ghost, `early(Cutoff)` at the boot cutoff; `onWake()` after the verify block and before the touch probe; a new `settingsLoaded` flag, set right after `SETTINGS.loadFromFile()` (the SD-error boot returns before it, and `cfg` and `tmo` are then left empty) |
| `src/main.cpp`, `loop()` | `tickBatteryLog()` before the exclusive-storage branch, so USB Drive time counts; `flushIfDue()` after the render lock's scope, once the render task has been seen busy |
| `src/main.cpp`, `noteBatteryInput()`, `restartWithSilentToken()` | `noteInput()` counts input passes at the battery limits' one funnel; `carry()` keeps the session's accumulators across a silent restart |
| `lib/hal/HalPowerManager.{h,cpp}` | getters `batteryCachedTenths()` and `atFullClock()`; the first battery percentage after a boot is handed to `noteUiSeed()` from the seed branch of `getBatteryPercentage()`; a stuck SDA is noted in `sleepImpl()` |
| `lib/hal/HalGPIO.cpp`, `prepareForSleep()` | a touch controller that is fitted but did not accept hibernate is noted |
| `lib/hal/HalClock.cpp`, `syncSystemTimeFromNTP()` | `noteClockSynced()` on the success path, which KOReader sync calls |
| `src/util/BatteryLogWriter.{h,cpp}` | CrossPoint's file; one line and one include differ (the `# fw` line takes the version from `AppVersion::version()`, because `CROSSINK_VERSION` reaches `BuildInfo.cpp` only) |

The release-wake and reset-loop exits and `haltOnPowerButtonFault()` call no hook.

## What differs in the rows

- **The `# fw` version ends `-silkscreen-batlog`.** The analyzer takes its early-wake limit from it: 500 ms, where CrossPoint's
  is 300 ms. CrossInk's wake verify holds the boot until `millis()` reaches the long-press time (400 ms) unless the click-wake
  rule is on, so an early ghost wake takes about 0.4 s and `w_dt` is later: the wake sample has seen more of the boot-load step
  (55-75 %, against 26-55 % on CrossPoint).
- **No SD-fail ghost and no late ghost:** the early exits are the early ghost (`g_n`) and the boot cutoff (`g_o`).
- **`path`:** `q` is the Quick Lock timeout and `w` the unattended wake; both are tagged at their call sites.
- **A render-watchdog sleep leaves an orphan row, with no tag** ([POWER.md AW-04](POWER.md#33-the-render-watchdog-aw-04)). A render with no progress for 5 minutes on battery, or a sleep entry that has not slept after 3, restarts the chip, and the boot after the restart sleeps by the reset loop's sequence, so `enterDeepSleep()` is never reached (or its staged entry is gone with the RAM), no entry is staged and no log hook runs, as for the reset loop's sleep. At the next power-key wake the SDK's record is *Idle*, a wake from a sleep nothing recorded, so it writes an orphan row: `k` = B, wake flag `ORPHAN`, no entry half, a new session. `Path::Ceiling` (`k`) would fit by meaning, but a path lives in the staged entry, which this sleep never stages. The hung session's awake time (the bound, 5 min or 3 min for a sleep entry, plus up to one 5 s tick, at 30-45 mA [est]) is in no row, so the voltage step across that cycle is larger than any recorded awake time explains. The format and the analyzer are unchanged; CrossPoint's `scripts/silkscreen/test_battery_log_report.py` has no case for it and still passes.
- **`utc`** is written only after a KOReader sync (`HalClock::syncSystemTimeFromNTP()`): CrossInk has no restored clock floor and
  no SNTP at a Wi-Fi join.
- **`cfg` bit 1** (the click-wake rule) comes from `SETTINGS.shortPowerPressWakes()`.
- **The radio** goes down inside `sleepImpl()`, not before it, so `flushAtEntry()` checks the radio itself, and `fl_e` bit 1 says
  it was on at the trigger.
- **USB Drive** is behind `CROSSINK_APP_CAP_USB_DRIVE=1`, which the Silkscreen env sets.
- Never compare CrossPoint rows with CrossInk rows without reading `fw` and `w_dt`.

## Rules and upkeep

The drain rules are item 11 of the [drain checklist](PORTING.md#drain-checklist) and POWER.md BL-01. After each upstream sync,
re-check the SDK's BatteryLog files and these hooks (`crosspoint-reader/docs/silkscreen/UPSTREAM-SYNC.md`, step 7);
`crosspoint-reader/scripts/silkscreen/upstream_status.sh` reports a missing env or hook.

## Trusted display percentage

CrossPoint and CrossInk use the same Silkscreen-only `BatteryChargeEstimator.h`
in their shared SDK line, owned by the main loop through `HalPowerManager`.
The SDK is a submodule; the estimator, profile constants and host tests are
mirrored in both local SDK checkouts. Both apps follow the shared SDK line.
The voltage curve, cutoff, external-power disproof and ladder windows/debounce
are unchanged. Other boards retain their existing battery paths.

The estimator keeps updating while awake, including charging and Complete.
Battery percentage and USB-state changes do not request a screen redraw on
Silkscreen; the next ordinary UI redraw paints the current value and charging
icon. These updates do not consume the reader refresh cadence. Battery logs
report the cached estimate, which may be newer than the number still on paper.

- Sample 16 averaged ADC conversions about once per second while awake. Skip
  while Wi-Fi is enabled (also BLE in CrossPoint), during refresh, and for
  2 seconds after those loads. This conservative radio gate covers scans and
  transfers that do not report client traffic. It can leave the display stale.
- Battery estimates use quiet history 4-8 seconds old, beyond the ladder's
  2-3 second Charging publication delay. Initial battery seeding requires three
  quiet readings within a three-second window, allowing loop/ADC timing jitter.
  This prevents the plug-in voltage lift entering the starting
  estimate before the ladder reports it. Unknown history displays `--%`.
- Keep the usual 5% display steps with a 2-point deadband; the battery-log build
  retains 1% resolution. An ordinary on-battery increase must reach 15 points
  above the shown value. Only the decoded Complete state can newly show 100%;
  the charging estimate can reach 99%.
- At a battery light on/off, brightness or warmth change, compare quiet readings
  before and after settling. Add the measured sag back at that stable light
  level, so discharge continues to update during long lit sessions. Short slider
  drags retain the original quiet baseline. Reject noisy or implausible steps;
  an unmeasured lit level holds the percentage until a usable calibration or
  light-off. A cached battery-light correction is not added again under USB.
- At Battery -> Charging, compare quiet history from before the delayed ladder
  edge with settled readings after it (roughly 5-6 seconds after physical plug).
  Reject the measurement if the light changed. Use that session's step S; keep
  a running mean of valid normal charge edges in NVS, one write per learned
  edge. Boot/wake already charging uses that mean, or 150 mV before calibration.
  Complete -> Charging measures a new session step without teaching the normal
  constant-current mean from a small top-off step. Already confirmed 100% stays
  at 100% through top-off.
- During charging, use curve(P+ - S), with a fixed pre-plug light reference when
  needed. Only increase the estimate, at no more than 250 mA / 500 mAh, about
  0.833 percentage points per minute. Held or asleep time creates no catch-up
  allowance. A named factor can ramp S over the first 20 minutes; its default
  1.0 leaves S unchanged until a real trace supports another value.
- After an observed rise of at least 10 mV, a quiet relative plateau lasting
  3 minutes enables a 20-minute-time-constant approach to 99%, still rate capped.
  No fixed 4.20 V ADC threshold is used. A flat rail seen from boot cannot prove
  the transition into taper; it waits for evidence or Complete. Weak USB can
  make voltage-based phase detection ambiguous; this is an estimate.
- After unplug, retain the estimate for 10 seconds of settling, then permit only
  voltage-driven decreases for 30 minutes. Normal battery behavior follows.
  Unknown, NoBattery and ExternalIdle hold the last trusted estimate; existing
  missing-cell notices still apply. RTC memory carries the estimate, session
  reference and unplug deadline through normal sleep/software restarts.

`BoardConfig/include/SilkscreenBattery.h` names the 500 mAh capacity, 250 mA
charging current, 150 mV initial step and `STEP_CREEP_PERMILLE=1000`.
The estimator names the noise limits (8 mV), normal USB step range (30-400 mV),
light correction range (0-150 mV), settling times and plateau/taper criteria.
The actual step, creep and phase thresholds need a logged charge on a real board;
the curve itself is still the existing generic voltage-to-charge curve.

### Per-sample charge log (battery-log builds)

A separate `/silkscreen-battery/charge.csv` records accepted raw ADC samples,
including samples held for light/edge settling. Its columns are:

```text
rtc_ms,uptime_ms,event,raw_mv,ladder,phase,step_mv,light_mv,comp_mv,tenths,shown,flags,cal_count,reference_ms,reference_mv
```

`ladder`: 0 Unknown, 1 Battery, 2 Charging, 3 Complete, 4 ExternalIdle, 5 NoBattery.
`phase`: 0 Unknown, 1 Battery, 2 Step, 3 Charging, 4 Taper, 5 Complete, 6 Relax.
`flags` is a bitmask: 1 unmeasured lit level held, 2 busy, 4 rejected USB step,
8 learned USB step, 16 previously confirmed full. `light_mv` is the current
battery correction or the fixed charge-session reference. `step_mv` is S;
`comp_mv` includes the configured creep adjustment. `tenths` is the internal
estimate; `shown` is the UI percentage (101 means unknown).

`complete_ref` rows retain the last accepted raw voltage and its RTC timestamp
before decoded Charging -> Complete. Complete takes eight stable ladder samples,
so this is a diagnostic reference near completion, not the exact physical
termination voltage. It never calibrates the ADC or percentage curve.

The BATLOG-only writer uses a bounded 1 KiB batch, closes the file after each
append, flushes the remaining batch before normal sleep/storage shutdown, and
stops appending at 32 MiB. Abrupt resets can lose the last buffered samples.
It does not run during early minimal sleep or exclusive raw USB-card access.
The existing `/silkscreen-battery/battery.csv` wake/sleep format and analyzer
remain unchanged. The per-sample log adds SD activity while awake; do not use
it as proof of release-firmware current consumption.

For tuning: start near 40%, turn radios off, and allow 10 seconds of quiet
awake readings before plugging in. Charge 30 minutes, unplug, then rest
30 minutes. Samples exist only while awake; logging never extends the normal
sleep limits. Use normal interaction to keep a continuous awake trace, or record
the sleep/wake gaps. Keep the light/radio state stable around the edges. Check S, the
corrected rise, the light-hold flags and Complete reference before tuning the
creep factor. Also test a long light-on discharge and brightness changes.

### Host evidence

Run `freeink-sdk/libs/hardware/BatteryMonitor/test/host/run.sh`. The pure-class
traces compile the production notch table and its existing static assertions.
They cover delayed edges, boot already charging, learned/default offsets, rate
and gap limits, noisy light steps, slider changes, fixed USB light references,
Complete/top-off, unplug relaxation and timestamp wrap.

For sustained quiet changes after smoothing converges, with a 5% display:

| Starting voltage (shown) | Old decrease / increase | New decrease / increase |
| --- | --- | --- |
| 3750 mV (20%) | 16 / 5 mV | 28 / 31 mV |
| 3850 mV (55%) | 10 / 20 mV | 20 / 73 mV |
| 4000 mV (80%) | 26 / 20 mV | 38 / 136 mV |

These are directional changes from the named starting voltage, not a universal
noise tolerance. Upward thresholds include the deliberate 15-point recovery
gate. The new history adds time lag; short transient changes may never appear.

The scripted 40% -> charge 30 min -> rest 30 min trace at 1 Hz gives:

| Time since plug | Original display | New display |
| --- | --- | --- |
| Before plug | 40% | 40% |
| 1 minute | 70% | 40% |
| 5 minutes | 75% | 40% |
| 15 minutes | 80% | 50% |
| 30 minutes, unplug | 85% | 60% |
| 31 minutes | 65% | 60% |
| 45 minutes | 65% | 60% |
| 60 minutes | 65% | 60% |

This synthetic trace includes a 150 mV plug step and 15 mV slow creep; its
underlying state rises from 40% to 65%. The final new internal estimate is 64.6%,
with 5% display hysteresis holding 60%. The original comparison uses its ADC
smoothing at the same 1 Hz cadence; real old UI getter cadence varied. These are
host results, not a measured charge curve. The separate full-charge trace
reaches 99% in taper and 100% only when Complete is explicitly supplied.

### Build verification (2026-10-28)

All six reader builds passed, with sleep/wake and render-watchdog checks, OTA
partition fit and the 3.68-inch no-touch audits. The BatteryMonitor host suite
passed, including the charge estimator and existing ladder/gauge/USB tests.
CrossInk reports existing icon/UI narrowing warnings. No board was flashed.

| Folder | App bytes | Flash used | Static RAM | PIO seconds |
| --- | --- | --- | --- | --- |
| silkscreen-batlog | 5382672 | 5377362 | 114316 | 306.16 |
| silkscreen | 5417264 | 5411954 | 112756 | 742.95 |
| silkscreen-368 | 5401152 | 5395874 | 112596 | 379.92 |
| crossink-silkscreen-batlog | 5619104 | 5613881 | 130080 | 1521.44 |
| crossink-silkscreen | 5609552 | 5604325 | 128520 | 403.36 |
| crossink-silkscreen-368 | 5483360 | 5478153 | 124768 | 531.11 |

The bundle includes all four flash parts per folder, ESP Web Tools manifests,
SHA-256 sums and source fingerprints covering the source used for each build.
