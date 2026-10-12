# Silkscreen battery log on CrossInk

The battery sleep/wake log is documented once, in CrossPoint's `crosspoint-reader/docs/silkscreen/BATTERY-LOG.md`, next to
this repo in `silkscreen-firmware/`: what the build is, how to run a campaign, retrieve the file and read a row, what the log
can and cannot find, the bench proof and the DMM recipe. Both readers carry the same SDK code, write the same CSV and use the
same analyzer (`crosspoint-reader/scripts/silkscreen/battery_log_report.py`). This page lists only what differs on CrossInk.

## The build

- **Its own env.** `silkscreen-batlog` is `[env:silkscreen]`'s flags plus `-DSILKSCREEN_BATLOG=1`, with the same
  `LOG_LEVEL=1` and no USB-serial wait, so boot timing and the ghost-wake path match `silkscreen`. There is no setting, screen,
  i18n key or Quick Action: stop logging by flashing `silkscreen`.
- **The screen keeps 1% steps.** The release image shows the battery percentage in 5% steps (truncated, 100 from 98
  up; [PORTING.md](PORTING.md)). This image leaves that out (`#if FREEINK_DEVICE_SILKSCREEN && !SILKSCREEN_BATLOG`: the
  helper and its `static_assert`s above `HalPowerManager::getBatteryPercentage()`, the return inside it), so what its
  screen shows still matches the log: `e_ui` and `w_ui` are tenths of a percent read from the raw cache and from the first
  fresh burst, so the display step would never have coarsened them, but the analyzer (CrossPoint's `ui_pct()`) derives the
  percent the screen showed from them, and that is only what a 1% screen shows. Apart from the log, it is the one place
  this image behaves differently from the release image.
- **Flash:** `pio run -e silkscreen-batlog -t upload`. `scripts/silkscreen/build_matrix.sh` builds it after `silkscreen`.
- **Size** (the final build matrix, 2026-10-04): 5622425 B of flash and 116080 B of static RAM, 8820 B and 544 B more than
  `silkscreen` (5613605 / 115536). The image is 5627616 B, 925984 B free in the 6553600 B slot (`silkscreen`: 934800 B free).
  Free RTC slow memory is 1464 B, against 1648 B on `silkscreen`: the record is 184 B. Every other env is the size it was,
  to the byte. CrossPoint's BATTERY-LOG.md section 1 has both readers' figures. Since then the second key-access audit moved
  the two to 5625565 / 116080 (`batlog`) and 5616853 / 115536 (`silkscreen`), and the 5% battery step (2026-10-05) added
  28 B to `silkscreen` (5616881) and nothing to this image: its footprint over a `silkscreen` built after the step is
  8684 B of flash and 544 B of RAM (the image is 5630768 B, 922832 B free).
- **Version and image.** `1.6.1-silkscreen-batlog`: `scripts/git_branch.py` injects `CROSSINK_VERSION` only for the envs it
  lists, so this env is in its set (without that, `BuildInfo` would report `dev`). The image is
  `.pio/build/silkscreen-batlog/firmware.bin`, with `firmware-silkscreen.bin` beside it. `custom_firmware_device_type` stays
  `silkscreen`, so the SD card firmware update swaps it with the `silkscreen` image in either direction.
- **The 3.68" variant has its own env.** `silkscreen-368-batlog` is `silkscreen-368`'s flags plus `-DSILKSCREEN_BATLOG=1`: the X3 panel, no touch, the same log code and hooks (the variant is the same device and `Board::Silkscreen`). Flash it with `pio run -e silkscreen-368-batlog -t upload`, and go back with `pio run -e silkscreen-368 -t upload`. Its version is `1.6.1-368-silkscreen-batlog` (`scripts/git_branch.py`), which still ends `-silkscreen-batlog`, so the analyzer takes CrossInk's 500 ms early-wake limit from it (`-368-batlog` would read as CrossPoint's). The board tag (`silkscreen-368`) and `custom_firmware_device_type` (`silkscreen368`, so that the 4.26" board's OTA asset matcher cannot take the variant's `firmware-silkscreen368.bin`) are the release image's, so the two swap over SD. Size (2026-10-11, after the review's fix pass): 4,952,841 B of flash and 117,104 B of static RAM, 8,328 B and 536 B over `silkscreen-368` (4,944,513 / 116,568; the 4.26" pair differs by 8,564 B and 536 B); the image is 4,958,000 B, 1,595,600 B free in the slot. Before the pass it was 4,953,181 B (4,944,953 for `silkscreen-368`). The analyzer's reference floor, the no-touch 58 µA plus self-discharge, is the 4.26" board's: the `cfg` column's touch bit is 0 on this build, but this panel's own deep-sleep current is not in hand ([POWER.md](POWER.md) row 17). A floor a few µA above it is not by itself a fault, and the first campaign sets this panel's.
- **Gated in code.** Every line of it is inside `#if SILKSCREEN_BATLOG`, which only this env defines; every other env compiles
  to what it did before. The one blocker that runs the other way round is the release image's 5% display step
  (`#if FREEINK_DEVICE_SILKSCREEN && !SILKSCREEN_BATLOG`), which this image compiles out. CrossInk's own
  `BatteryDiagnosticLog` stays compiled out and untouched.

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
