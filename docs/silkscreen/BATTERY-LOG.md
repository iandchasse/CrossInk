# Silkscreen battery log on CrossInk

The battery sleep/wake log is documented once, in CrossPoint's `crosspoint-reader/docs/silkscreen/BATTERY-LOG.md`, next to
this repo in `silkscreen-firmware/`: what the build is, how to run a campaign, retrieve the file and read a row, what the log
can and cannot find, the bench proof and the DMM recipe. Both readers carry the same SDK code, write the same CSV and use the
same analyzer (`crosspoint-reader/scripts/silkscreen/battery_log_report.py`). This page lists only what differs on CrossInk.

## The build

- **Its own env.** `silkscreen-batlog` is `[env:silkscreen]`'s flags plus `-DSILKSCREEN_BATLOG=1`, with the same
  `LOG_LEVEL=1` and no USB-serial wait, so boot timing and the ghost-wake path match `silkscreen`. There is no setting, screen,
  i18n key or Quick Action: stop logging by flashing `silkscreen`.
- **Flash:** `pio run -e silkscreen-batlog -t upload`. `scripts/silkscreen/build_matrix.sh` builds it after `silkscreen`.
- **Version and image.** `1.6.1-silkscreen-batlog`: `scripts/git_branch.py` injects `CROSSINK_VERSION` only for the envs it
  lists, so this env is in its set (without that, `BuildInfo` would report `dev`). The image is
  `.pio/build/silkscreen-batlog/firmware.bin`, with `firmware-silkscreen.bin` beside it. `custom_firmware_device_type` stays
  `silkscreen`, so the SD card firmware update swaps it with the `silkscreen` image in either direction.
- **Gated in code.** Every line of it is inside `#if SILKSCREEN_BATLOG`, which only this env defines; every other env compiles
  to what it did before. CrossInk's own `BatteryDiagnosticLog` stays compiled out and untouched.

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
