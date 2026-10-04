# Silkscreen port of CrossInk

This branch (`silkscreen`) ports [CrossInk](https://github.com/uxjulia/CrossInk) to **Silkscreen**, an open-hardware ESP32-S3 e-paper reader board:

- ESP32-S3-WROOM-1-N16R8;
- a 4.26" 800x480 panel, SSD1677 or the custom UC8279D;
- an FT6336U touch tail alongside eight ADC-ladder keys, with power on IO18;
- 4-bit SDMMC, a DS3231MZ RTC and a TPS923610 front light.

The hardware is described in `de-link_pcb/docs/HARDWARE.md` ([iandchasse/silkscreen-pcb](https://github.com/iandchasse/silkscreen-pcb)).

**The reference port.** The finished, reviewed port of CrossPoint Reader is `silkscreen-firmware/crosspoint-reader`, branch `silkscreen`. This port carries the same board support into CrossInk, adapted to CrossInk's own code.

**The docs here:**

- [REQUIREMENTS.md](REQUIREMENTS.md): the firmware-facing hardware facts. A verbatim copy from CrossPoint.
- [MODULARITY.md](MODULARITY.md): how the firmware tells the board's variants apart. A verbatim copy from CrossPoint.
- [POWER.md](POWER.md): the sleep budget, the pin states and every battery-drain rule, with the CrossInk file and function that implement it.
- [TEST-SUITE.md](TEST-SUITE.md): a pointer to CrossPoint's hardware test firmware, which CrossInk uses for bring-up.

**Base:**

- CrossInk `development` at `9dd334d3` (2026-09-29), as cloned; merged up to `3efe61b6` (2026-10-02, v1.6.1 and what followed) in `a9f9ef64`.
- freeink-sdk at the shared Silkscreen SDK head, `bf196c8f` ([The shared SDK](#the-shared-sdk)).
- The last CrossPoint `silkscreen` commit ported is `c9abee96` (2026-10-03), the rounding fix of the 1% battery (`844dde61`). CrossPoint's `silkscreen` head `e7e62f48` has been read through: what follows `8cb94736` is ported or recorded below as not applicable.

## Status

Every stage has landed in these local commits on `silkscreen`. **Nothing has run on a board yet**, so every hardware claim still waits for the [bring-up checklist](#bring-up-checklist-real-board).

| Stage | Commits | What landed |
|---|---|---|
| S0 (in CrossPoint) | crosspoint-reader `5d58fdc0` | The awake-ceiling rule for both readers: on battery, the longer of 15 min and the user's sleep timeout. |
| S1 Shared SDK | `9149527e` | freeink-sdk moved from CrossInk's `8bbc44c` to the shared head `f613450` (a fast-forward). `.gitmodules` names the same SDK URL and branch as CrossPoint. |
| S2 Build target | `290b9a90` | `[silkscreen]`, `[env:silkscreen]` and `[env:silkscreen-debug]` on pioarduino 55.03.311. `scripts/git_branch.py` (release and debug sets, the `-silkscreen` suffix). The `silkscreen` board tag (`src/network/FirmwareBoardTag.cpp`). The USB Serial/JTAG handoff after USB Drive (`src/platform/UsbSerialJtagHandoff.cpp`). |
| S3 HAL | `c240df98`, `07318ec8`, `e5466f66`, `1a9c2c09`, `6932399d`, `838b27b1` | The front-light HAL with the switch-on load probe, the restore refusal and the shutdown handler. The RTC validity and resync. USB presence from the charge-status ladder. Short key taps at idle. The GPIO boot, touch and front-key hooks. The power-policy queries and the input and client-traffic clocks. |
| S4 Sleep path | `e59a7cfc` | Silkscreen sleeps through the SDK power path (`HalPowerManager::sleepImpl()`); other boards keep CrossInk's tail. The sleep-source guard, with its one exception. |
| S5 Boot checks | `f9544b2e` | The release-wake re-sleep, the IO18 idle check and its fault screen, the reset-loop sleep with driver-free panel and touch sleep, the boot cutoff, the early wake decision with the deferred touch probe, and the panel controller resolved before `display.begin()`. |
| S6 Awake limits | `647fc531`, `47e8e470`, `7cc2982d`, `37d61efe` | The battery awake limits in `loop()`: the cutoff, the ceiling, the input cap, the wake-no-input and SD-error sleeps. Quick Lock input accounting, the clocks carried across silent restarts, client traffic, the USB Drive cable pull, the downloader's cutoff abort, the SD firmware floor, the front-light settle. SDK moved to `3a42533`. |
| S7 Radio | `3312a1eb`, `c369039e` | Network screens release the radio and stop spinning when their work ends. The Nearby windows. Radio shortcuts refused near empty. The OTA install floor. The SDK moved to `dae5523`, which blends the light's warm and cool strings. |
| S8 UI | `41c02221` | The front-key gates routed through `hasFrontButtons()`. Light Controls in Settings. The no-light popup and the light panel's notes. The "No battery" label. |
| S9 Docs | `f429fae7` | `docs/silkscreen/`. |
| Port review | `ff68feee`, `381440c8`, `51fb86d3`, `b65fb929`, `f0ffb481`, `e4a755b6`, `e4982497` and the docs commit after them | The SDK moved to `468b8e2`, which checks a blend's second string while lit and lets the rail settle before COLOR_SEL moves at a switch-off or sleep mid-probe. The charge state is read through the power HAL; `rawInputActive()` is kept to the ladder boards. A silent restart on USB no longer sleeps on its first pass; Quick Lock and held chords no longer spin at full clock. The light panel takes over a switch-on started elsewhere. Sunlight Fading Fix stays with touch; Light Controls and the panel's warmth keys follow the board profile. The Nearby stats restart error is translated. |

CrossPoint commits after the last review (`b605f87d`) were taken as follows:

- `5d58fdc0`, the ceiling rule: ported in `838b27b1` and `647fc531`;
- `4566ddcd` and `16e42656`, the SSD1677 drive values: through the SDK move in `37d61efe`;
- `92973915`, the front-light blend: in `c369039e`;
- `36ee5cfc`, the front-light key shortcuts and the power double-click: its SDK move in `ff68feee`, and the panel's takeover of a switch-on started elsewhere in `f0ffb481`. Its Short Power > Toggle Frontlight and double-click toggle stay CrossPoint's: CrossInk has its own Toggle Frontlight and shortcut actions. Its Long-press Confirm > Frontlight has a CrossInk counterpart in the Light Controls shortcut (below);
- `8127e3e8`, the battery checks before the chord returns: CrossInk has had them since `647fc531` (CI-02), and `b65fb929` adds the held-chord wait;
- `6b4f807d` (test firmware formatting) and `28fdd857` (CrossPoint's own docs): nothing to port;
- `bb7f80d3`, the upstream-sync runbook and scripts: CrossInk's build matrix script and the runbook pointers in `8b46e691`;
- `ad1b5ce8`, upstream `5b1f0605` (#3824: per-book content keys, plugin isolation): its SDK move in `1dcf47d9`, nothing else to port. CrossInk has no plugin system and links neither the SDK's ContentProtection nor its Opds;
- `8cb94736` (CrossPoint's own docs): REQUIREMENTS.md refreshed from it;
- `854536e9`, the build-matrix guard against concurrent pio runs: ported in `5ddff99f`;
- `04dd14ea`, the power double-click waits out every short-click action: CrossPoint's own (its Short Power actions and double-click toggle stay CrossPoint's, as for `36ee5cfc`); nothing to port;
- `844dde61`, the battery in 1% steps, with the SDK's `bf196c8`: the SDK move in `61e220ea`, the port in `8a720bf1` (see Defaults chosen), and its smoothing fix `c9abee96` (the cache rounds to nearest in both directions, so a full cell settles at 100 %) in `b2e457f3`;
- `1deb7c41`, CrossPoint's merge of upstream `5ab0f290` (#3847 the power-mode mutex, #3737 log lines at the low-power clock, #3830, #3733, #3834, #3838): CrossPoint upstream's own fixes, which CrossInk's HalPowerManager and logger do not carry; CrossInk takes them only if its own upstream does, and no Silkscreen rule depends on them;
- `18417dfe` (CrossPoint's own docs): nothing to port;
- `85d86188` and `e7e62f48` (CrossPoint's own docs: the record of the `5ab0f290` sync and its final build): nothing to port.

**Battery use.** The image has carried every drain rule since S6, but none has been measured. Run C2 and C6 of the [bring-up checklist](#bring-up-checklist-real-board) before relying on it.

## Architecture: Silkscreen is data

CrossInk follows freeink-sdk's rule that feature code never keys off a device name. The SDK side is CrossPoint's, shared unchanged: `FREEINK_DEVICE_SILKSCREEN`, `Board::Silkscreen` and the `SILKSCREEN` profile live only in `BoardConfig.h`, and every Silkscreen need is a defaulted profile field or a `FREEINK_CAP_*` mechanism (CrossPoint's PORTING.md, "Architecture", and [MODULARITY.md](MODULARITY.md#how-it-maps-onto-freeink-sdk)). On the app side:

- **Profile data gates the behaviour.** A Silkscreen rule tests a field only `SILKSCREEN` sets, through `BoardConfig::powerPolicy()`, `BoardConfig::hasChargeStatusLadder()`, `hasAdcLadders()`, `TouchConfig::buttonsAlongsideTouch` or `FrontlightConfig`. Otherwise it compiles under a capability flag only Silkscreen sets: `FREEINK_CAP_POWER_POLICY`, `FREEINK_CAP_FRONTLIGHT_PROBE` or `FREEINK_CAP_DISPLAY_PROBE_POLICY`. On every other board the fields are zero, and the flags are off. The `HalGPIO` touch sleep calls compile on every touch build (`FREEINK_CAP_TOUCH`), but only the Silkscreen sleep path calls them.
- **The device macro only registers the build.** `FREEINK_DEVICE_SILKSCREEN` appears only where CrossInk itself registers a device: `platformio.ini`, `src/network/FirmwareBoardTag.cpp` and `src/platform/UsbSerialJtagHandoff.cpp`. `scripts/git_branch.py` knows the two env names. `include/AppCapabilities.h` needed no change: `CROSSINK_APP_CAP_TOUCH=1` and `CROSSINK_APP_CAP_USB_DRIVE=1` match the SDK's `FREEINK_CAP_TOUCH` and `FREEINK_CAP_USB_MSC`.
- **HAL, not SDK.** App code calls `lib/hal`, as AGENTS.md asks, so main.cpp makes no new SDK calls. The HAL gained these wrappers:
  - `HalPowerManager`: `takeReleaseWake()`, `abnormalResetLimitReached()`, `batteryEmptyAtBoot()`, the awake clocks and limits, `radioStartBlockedByBattery()`, `firmwareUpdateBlockedByBattery()`, `noChargeableCell()` and `chargeState()` (the charge-status ladder's decoded state, for USB Drive's cable-pull exit);
  - `HalGPIO`: `begin(deferTouch)`, `startTouch()`, the touch sleep calls, `checkPowerButtonIdle()`, `idleDelay()` and `hasFrontButtons()`;
  - `HalDisplay`: `sleepWithoutDriver()` and `logController()`;
  - `HalClock`: `needsResync()`;
  - `HalFrontlight`: the probe settle and notices.
- **The simulator.** Its HAL is the external `uxjulia/crossink-simulator` library, which this repo cannot stub. Every new HAL call from `src/` is therefore under `#ifndef SIMULATOR` or behind an `include/` or `src/` shim that returns the old answer in the simulator:
  - `include/CrossInkHalFrontlight.h`;
  - `include/DeviceCapabilities.h` `deviceHasFrontButtons()`, `boardHasFrontButtons()` and `boardKeepsKeysBesideTouch()`;
  - `src/network/RadioIdle.h`;
  - `BaseTheme::showsNoBattery()`.
- **Cross-board changes.** Two kinds reach every board:
  - What the shared SDK brings: keyboard layouts and edge keys, glyph-centred header text, atomic settings writes, and SdFat write-back before sleep and USB Drive. Each has a "Silkscreen:" CHANGELOG line.
  - Size, mostly runtime-gated code and English strings. Against S2, on this host's proxy core and including the three later SDK moves, flash changed by (at the port review):
    - x4-pro: -5.3 KB (GCC now inlines the settings catalog differently there);
    - x4-classic: +3.3 KB;
    - the C3 proxy: +3.5 KB;
    - the Sticky proxy: +3.2 KB.

    RAM grew by 24-40 B. `getSettingsList()` is sensitive to this: a second `std::remove_if` over the catalog once made GCC outline `getBaseSettingsList()`'s lambda as well as inline it (+8 KB on every touch build), so a new condition goes into an existing erase.

  Behaviour on other boards is otherwise unchanged. `HalPowerManager::startDeepSleep()` on x4-pro and x4-classic was checked instruction-identical across the S4 change.

## Defaults chosen

The user's decisions for this port:

- **Touch and keys from the start.** `CROSSINK_APP_CAP_TOUCH=1`. CrossInk already lets keys and touch coexist (a key press reveals the selection cursor), but treated "touch" as "no front keys" at some sites. Only those front-key gates now ask `hasFrontButtons()` (`!hasTouch() || TouchConfig::buttonsAlongsideTouch`) through `include/DeviceCapabilities.h`:
  - the theme hint bands (Base, Lyra, RoundedRaff, Minimal, Dashboard) and `UITheme::getMetrics()`'s hint-band height;
  - the Library, Controls, Reader and TTF option layouts;
  - clip selection;
  - the keyboard's bottom row;
  - the Cover Grid home;
  - Controls > Front Buttons (`SettingsList.h`, `SettingsActivity::rebuildSettingsLists()`, `ControlsOptionsActivity`);
  - Quick Actions' Long Back and Long Menu triggers;
  - the key help in File Browser, Nearby transfer and Home;
  - Settings > Display > Sunlight Fading Fix (`getSettingsList()`), so Silkscreen keeps the row whether or not its touch panel answered, as in CrossPoint.

  These routed gates use `deviceHasFrontButtons(gpio)`, which equals the old `!gpio.hasTouch()` on every other board. The gates the port added, the light panel's key entry points (`frontlightPanelFromKeys()`: Light Controls in Settings > Display > Frontlight, in the reader menu and as a shortcut) and the panel's Up/Down warmth keys and key steps, use `boardHasFrontButtons()` (the board profile) instead, so an X4 Pro whose touch panel fails its probe gains none of them.

  Key-route fixes that only Silkscreen needs (held Back discarding a Reading Stats date edit, Right creating a clipping in a dictionary lookup, the OPDS search and Wi-Fi network-list input leaks, the frontlight time picker's labels and 5-minute step) use `boardKeepsKeysBesideTouch()` instead: the board profile's touch + keys flag, which is false on every keys-only board as well, so X3/X4 and the touch-only boards keep their behaviour exactly.

  **Key hints on touch layouts.** Settings > Display > Key hints on touch screens (`showKeyHintsOnTouch`, on by default; the row and the web setting exist only where a touch panel and the front keys are both fitted) decides whether the touch layouts that otherwise show no key labels draw them. `shouldDrawKeyHints(gpio)` in `include/DeviceCapabilities.h` is the one test: true without a touch panel, false on a touch-only board, the setting on a touch unit with keys. The screens that always drew the band on Silkscreen (Home, Library, Settings, the Nearby menus, KOReader sync's progress choice, ...) are not affected by it. With the setting on, these touch layouts draw their labels:
  - the hint band under the touch buttons of ClearCache, Nearby stats sync, Nearby position sync, Nearby book transfer (offer, collision and result prompts), the clipping detail, IntervalSelection (Back, Select, `-`, `+`), and the dictionary lookup's alternate-form prompt. Each lays its touch buttons out in `UITheme::getScreenSafeArea(renderer, true, false)`, which already excludes the band in all four orientations, so nothing overlaps it. IntervalSelection used to lay out its touch layout on the full screen, so its safe area now follows the same test (`shouldDrawKeyHints()`) in `render()` and `loop()`;
  - Calibre's connection-failed screen, which waited for Back with no hint on any board, gets one on a touch unit with keys only;
  - the reader menu's sheet: a line above its tab bar (`keyHintLineText()`), which is where the key layout's band would sit. The Root line is about 560 px at the default UI scale and 670 px at Large, over a 474 px portrait panel, so `keyHintLineCount()` measures it and the line wraps onto a second one when it does not fit. `drawerHeight()` grows the sheet by those lines, so the list keeps its rows. The Spacing, Margins and Auto Page Turn panes name their keys in the step hints under their sliders instead (`showsStepHints()` follows the setting; `drawerHeight()` grows the Auto Page Turn sheet, as it does Spacing and Margins, until the slider and both hint lines fit above its Confirm button, which landscape needs: the 65 % sheet leaves about 100 px for a 150 px block). The panes Go to % and Go to Page never open on this layout;
  - the light panel: two lines at the foot of its sheet (`keyHintsShown`; the brightness and warmth line wraps onto a second one at the Large UI scale), added to `panelBottom` only when the taller panel still fits the screen;
  - the keyboard: one tip line between the text field and the keys, only when that gap holds it. The keyboard's own band was already drawn.

  Still left out on purpose:
  - the side-button hints (`BaseTheme`/`LyraTheme::drawSideButtonHints()`, the keyboard's `>` `<`): their only caller is the keyboard, whose touch layout (56-px keys) starts at about y 430-490 in portrait, under the hints' fixed 345-506 band at the right edge, so they would cover its top-right keys; the key layout without touch starts below them;
  - IntervalSelection's front and side step hint lines, which share the room of its touch step buttons (its hint band is drawn);
  - the keyboard's full tip block (`hasTouchHardware()`), which needs more room than the taller touch keyboard leaves.

  On the reader menu's sub-lists the front Left and Right keys, which only switch tabs on the Root pane, step the rows on Silkscreen (`frontKeysStepRows()`, `boardKeepsKeysBesideTouch()`), so the key layout's band can label them Up and Down there, as the edge-button boards do.

  `STR_FRONT_BTN_FOLLOW_ORIENTATION` in `getSettingsList()`'s touch erase names no setting in CrossInk; the front-key orientation setting is `frontButtonOrientationAware` under Controls > Front Buttons, which is routed.
- **Network OTA and the Nearby screens kept**, reworked with battery limits on Silkscreen only ([POWER.md §3.1](POWER.md#31-network-screens-aw-02), CI-06).
  - OTA queries `uxjulia/CrossInk`'s releases, which carry no `firmware-silkscreen` asset, so a check ends at "no update".
  - Pointing it at a private feed is a `-DCROSSINK_OTA_RELEASE_URL=...` build flag (`src/network/OtaUpdater.cpp`).
- **Awake ceiling on battery:** the longer of 15 min and the user's sleep timeout. It never shortens a timeout the user chose; it bounds Never and the keep-awake screens. The 60-min input cap and the low-battery cutoff stay. CrossPoint uses the same rule.
- **Base:** `development` `9dd334d3` as cloned, not `release/v1.6.1`; later merges are in the sync records below.
- **Platform:** the Silkscreen envs override `platform` to pioarduino 55.03.311 (Arduino 3.3.11), the core the Silkscreen sleep path and front light were validated on. `[base]` keeps CrossInk's 55.03.37 for every other env.
- **Board tag** `silkscreen`, the same as CrossPoint, and one SD card shared by both readers (C11 below).
- **Sleep path:** Silkscreen reaches the SDK sleep path, and every other board keeps CrossInk's tail byte for byte. The guard script documents that one exception ([POWER.md](POWER.md#guard-script-and-its-exception)).
- **Automatic page turns count as client traffic.** They extend the ceiling, not the input cap.
- **`.gitmodules`** names `https://github.com/iandchasse/freeink-sdk.git`, branch `silkscreen`, as CrossPoint does, with local overrides ([Repository layout](#repository-layout-and-remotes)).
- **SDK pin:** always the shared head, never CrossInk's pre-merge PR pins.
- **Wake:** CrossInk's short-press-wake setting is kept, defaulting to the long press (Short Power Button = Ignore).
- **Registration:** Silkscreen is registered only in build-relevant files. The release workflows, the release catalog and `docs/catalog` are untouched.
- **Framebuffer:** `FREEINK_FB_PSRAM` is not set, as in CrossPoint, so the framebuffer stays in internal RAM.
- **Front light:** probed only on a user action or a restore at wake. CrossInk's daily light schedule counts as a restore and respects the SDK's no-load and missing-string memory.
- **Battery percentage in 1% steps** (CrossPoint `844dde61`, with the SDK's `BatteryMonitor::percentTenthsFromMillivolts()` from `bf196c8`). Upstream snaps the percentage to 10% notches (one noisy ADC read looked up in an 11-entry Li-ion curve). Silkscreen has no fuel gauge, so `HalPowerManager::getBatteryPercentage()` averages 16 conversions of `P+` and reads the same curve linearly between its notches (tenths of a percent), smooths it as before and rounds to the nearest percent, so a cell resting just under 4.20 V still reads 100%. The smoothing step (a tenth of the gap to the new sample) rounds to nearest in both directions on Silkscreen: the other boards' truncating form stops up to 0.9% short of a rising target, which would leave a full cell at 99% until the next boot, and rounding only upward would leave an empty one at 1%. The cache settles within 0.4% of the sample either way. The step is as fine as the hardware resolves; the accuracy is the voltage's (ADC calibration, sag under load, the rise while charging), so it is a state-of-charge estimate, not a gauge. Gated by `BoardConfig::isSilkscreen()`: every other board keeps the 10% notches and its floor.

## What differs from CrossPoint

| Area | CrossPoint `silkscreen` | CrossInk `silkscreen` |
|---|---|---|
| Platform | 55.03.311 for every env | 55.03.311 for the Silkscreen envs only; `[base]` stays on 55.03.37 |
| Envs | `silkscreen`, `silkscreen-gh_release(_rc)`, `silkscreen-test` | `silkscreen` (release, `LOG_LEVEL=1`) and `silkscreen-debug` (`LOG_LEVEL=2`, 250 ms USB wait). No test env: use CrossPoint's |
| Sleep tail | Every board through the SDK path | Silkscreen only. Other boards keep CrossInk's tail, the guard's one exception |
| Sleep guard | `scripts/check_sleep_wake_sources.sh`, run by CI | `scripts/check_sleep_wake_sources.py`, a pre-build script of the Silkscreen envs, also standalone |
| Wake rule | An RTC word (`sleepWakePolicy`); the first wake after power-on waits for the settings | CrossInk's NVS mirror (`readWakeShortPressFromNvs()`): every wake is decided before SD |
| Battery checks in `loop()` | Before the wake-release and screenshot-chord returns since `8127e3e8`; CrossPoint has no Quick Lock | Before every early return (CI-02), since CrossInk's Quick Lock and chords return early; the Quick Lock branch idles and held chords wait 10 ms |
| Quick Lock, silent-restart clocks, radio shortcuts | n/a (CrossPoint has none of them) | CI-03, CI-04, CI-05 |
| Nearby (ESP-NOW) | n/a | Kept, with 2-min listen and discover windows (CI-06) |
| Client traffic | All requests except WebDAV polling methods and the plugin-job poll | All requests except WebDAV polling methods and `handleNotFound()` answers (captive probes); also automatic page turns and a USB host using the drive |
| Plugin system, loan clock | Drain rules (`4505ceec`) | Not in CrossInk: rows marked not applicable in POWER.md |
| Upload cutoff | Web uploads poll the cutoff | WebSocket uploads covered; a blocking multipart upload is not (no upstream #3732) |
| Front-light key path | Settings > Display > Frontlight opens the panel; Long-press Confirm > Frontlight and Short Power > Toggle Frontlight (both since `36ee5cfc`) | Settings > Display > Frontlight > Light Controls (`SettingAction::FrontlightPanel`, `ActivityManager::pushFrontlightPanel()`); the reader menu's Settings tab > Light Controls (`ReaderDrawerCatalogItem::LightControls`); the Light Controls shortcut, `SHORT_PWRBTN::FRONTLIGHT_PANEL` (36) and `LONG_MENU_FRONTLIGHT_PANEL` (25), offered where `frontlightPanelFromKeys()` (a light and `boardHasFrontButtons()`) and handled by `ActivityManager::toggleFrontlightPanelFromShortcut()`: it opens the panel without lighting anything, and the same key closes it; CrossInk's own Toggle Frontlight |
| Restore at wake | Restore Light on Wake | Also CrossInk's daily light schedule; a brightness change from 0 while lit is a probed switch-on |
| Reader overlay | `xteinkClassPanel()` honours `x4ClassPanel` | No counterpart; check gray levels by eye (C12) |
| Version | Upstream's version string | `1.6.1-silkscreen` (`scripts/git_branch.py`), file `firmware-silkscreen.bin` |

## Build

```bash
pio run -e silkscreen
pio run -e silkscreen -t upload
```

- **Debug env.** `silkscreen-debug` logs at level 2 and shows the build on the sleep screen (`CROSSINK_SHOW_SLEEP_BUILD_INFO`). It waits 250 ms for USB serial on every boot, ghost wakes included, so take battery numbers from `silkscreen`.
- **What runs with each build:**
  - before it, the sleep-source guard;
  - after it, CrossInk's `check_firmware_size.py`, `check_app_touch_gate.py` and `rename_firmware.py` (`firmware-silkscreen.bin`). The touch-gate audit returns at once on a touch build, as designed.
- **Version.** Settings shows `1.6.1-silkscreen`. The debug env shows `1.6.1-<branch>+<hash>`, which on this branch reads `1.6.1-silkscreen+<hash>`.
- **Size at the port review:** 5,911,177 B (90.2 % of the 6.25 MB app slot) and 114,992 B of static RAM. After the `3efe61b6` sync and its review fixes: 5,927,761 B and 115,000 B (the image is 5,932,960 B, 90.5 % of the slot).
- **Local hooks.** Set these under `[silkscreen]` in `platformio.local.ini` (gitignored), as in CrossPoint:
  - `frontlight_flags =` drops the light; `-DSILKSCREEN_FRONTLIGHT_BLEND_HZ=0` keeps the fixed warm/cool choice;
  - `display_flags = -DSILKSCREEN_DISPLAY_CONTROLLER=uc8279` (or `ssd1677`) skips the panel probe;
  - `touch_flags = -DSILKSCREEN_TOUCH_ALT_JUMPERS=1` adds the GT911 fallback.

  A default env goes in the same file: `[platformio]` `default_envs = silkscreen`.
- **Two cores.** pioarduino keeps one Arduino framework package installed. Switching between a Silkscreen env (3.3.11) and any other env (3.3.7) makes PlatformIO reinstall the framework, so batch builds by core.
- **The other envs on this Windows host.** They were built only through a scratch proxy config that changes nothing but `[base]`'s platform (55.03.37 → 55.03.311), plus two proxies on `base`: a C3 one with `default`'s flags and an S3 one with `sticky`'s. The 3.3.7 framework would replace the shared 3.3.11 copy that CrossPoint builds with, and `[firmware_tuned]`'s `custom_sdkconfig` cannot run here. Confirm the real images on a host with CrossInk's core: `pio run -e default -e sticky -e x4-pro -e x4-classic`.
- **Windows.** Run PlatformIO with `PYTHONUTF8=1` and `PYTHONIOENCODING=utf-8` (CrossInk's scripts print UTF-8), and keep `core.longpaths=true`. Run one PlatformIO command at a time: the platform may reinstall a tool package or its Python dependencies when it starts, and a second `pio` process running beside it can leave them half-installed (once, a `pio check` beside a build removed `tool-esptoolpy`, `urllib3` and `esptool` from PlatformIO's environment).
- **Formatting:** `bin/clang-format-fix -g` reformats, as whole files, the tracked C/C++ files whose working tree differs from the index; run it before `git add`. A new file is formatted only once it is tracked: `git add -N` it first, or run clang-format on it directly. Never mass-reformat.

## Repository layout and remotes

- **This clone:** `silkscreen-firmware/crossink`, branch `silkscreen`, with no upstream tracking, so a stray pull or push does nothing. Every remote's push URL is `DISABLED-no-push`.
  - `crossink`: `uxjulia/CrossInk`.
  - `crosspoint`: the local `silkscreen-firmware/crosspoint-reader`, branch `silkscreen`. Its tags are fetched under `refs/tags/crosspoint/*`, because CrossPoint's `v1.5.0` collides with CrossInk's.
  - `cpupstream`: `crosspoint-reader/crosspoint-reader` `develop`, to read upstream PRs while resolving conflicts, as AGENTS.md asks.
- **Commits** are local only: `<type>: <summary>`, no AI trailers, a "Silkscreen:" CHANGELOG line for anything user-facing. Never push, open a PR, or edit crosspoint-reader from this fork.
- **Submodule overrides** in `.git/config`:
  - `submodule.freeink-sdk.url` is the local shared SDK, `silkscreen-firmware/crosspoint-reader/freeink-sdk`;
  - `fetch.recurseSubmodules = false`.

  The submodule itself keeps `origin` (Free-Ink GitHub) and adds `silkscreen-sdk` (the local shared SDK, push disabled).
- **Fresh clones.** A fresh clone resolves the SDK commits only once the shared `silkscreen` branch is pushed to `iandchasse/freeink-sdk`. Until then, clone this folder rather than GitHub.
- **Windows:** `git config core.longpaths true`.
- **GitHub Pages.** CrossInk's Pages site globs `docs/**/*.md` (`site/src/lib/docs.ts`), so these files would appear on the site if this branch were ever deployed to Pages. That never happens on an unpushed fork. If it ever might, move them under `docs/development/`, which the site leaves out.

## The shared SDK

There is one Silkscreen SDK line: the branch `silkscreen` of `silkscreen-firmware/crosspoint-reader/freeink-sdk`, published as `iandchasse/freeink-sdk`. CrossPoint owns it. CrossInk's gitlink is **always that branch's head**: never a commit that is not on it, and never CrossInk's own pre-merge PR pins.

**Check:** these two must print the same commit (today `bf196c8f6d339b826fab02a01359c83487cb913e`):

```bash
git ls-tree HEAD freeink-sdk
git -C ../crosspoint-reader/freeink-sdk rev-parse silkscreen
```

**Moving to a new shared head** is always a fast-forward, landed as a `chore:` commit:

```bash
git -C freeink-sdk fetch silkscreen-sdk
git -C freeink-sdk merge-base --is-ancestor HEAD silkscreen-sdk/silkscreen   # must succeed
git -C freeink-sdk checkout --detach silkscreen-sdk/silkscreen
git add freeink-sdk
git commit -m "chore: move freeink-sdk to the shared SDK head"
```

Then rebuild `silkscreen`, `x4-pro`, `x4-classic` and the C3 and Sticky builds. Read the shared commits for app-facing API changes; S7's `hasColourSelect()` follow-up is an example. Note what other boards inherit in the CHANGELOG.

## Upstream merges

The branch takes CrossInk by merge, never by rebase: older commits pin SDK commits that must stay reachable. Merge `crossink/development` about once a week. It moves fast: September 2026 brought 429 commits, with `main.cpp`, `SettingsList.h` and `ActivityManager.cpp` among the most touched, and about five SDK pin moves a week. Expect conflicts in `src/main.cpp`, `lib/hal/HalPowerManager.*` and `HalGPIO.*`, the settings files, `platformio.ini` and `CHANGELOG.md`.

The whole multi-repo procedure (the shared SDK line first, then CrossPoint, then this repo, then the pins, builds,
review and records) is `crosspoint-reader/docs/silkscreen/UPSTREAM-SYNC.md`; a Claude Code session in
`silkscreen-firmware/` runs it as the saved workflow `silkscreen-upstream-sync`. Its step 0,
`crosspoint-reader/scripts/silkscreen/upstream_status.sh`, reports what this repo would take.

**Sync records.** One row per sync, beside the row in CrossPoint's PORTING.md "Upstream merges" table:

| CrossInk commits | `crossink/development` | SDK gitlink | What it brought |
|---|---|---|---|
| `1dcf47d9`, `332fb4ac`, and the `docs:` commit after them | Nothing new: `9dd334d3`, as cloned | `468b8e2` to `3d1c39a`, the shared line's merge of `bbd528c` | CrossPoint's sync to upstream `5b1f0605` (`ad1b5ce8`, #3824). Only the SDK move: `bbd528c` changes ContentProtection and the OPDS parser, and CrossInk links neither (it has no plugin system and parses OPDS with its own `lib/OpdsParser`), so there is nothing to port and no CHANGELOG line. REQUIREMENTS.md refreshed from CrossPoint `8cb94736`. |
| `a9f9ef64` (the merge), `61e220ea`, `8a720bf1`, the review fixes `b2e457f3` and `301b6d63`, and the `docs:` commits after them | `9dd334d3` to `3efe61b6`: 23 commits, 143 files (v1.6.1 and what followed: inline EPUB images, the Library search list, toggle switches, menu buttons that follow the device layout, the KOReader progress cards, a faster sleep entry) | `3d1c39a` to `bf196c8`, the shared line's `read the Li-ion curve in tenths of a percent` (a new function; no other board changes). Upstream's own new pin, `8bbc44c`, is an ancestor of the shared head, so the shared head stayed | Conflicts in `CHANGELOG.md` (the release commit renamed `[Unreleased]` to `[v1.6.1]` and took the Silkscreen lines with it: all 28 are back under a new `[Unreleased]`, 29 with the battery line `8a720bf1` added), `FrontlightPanelActivity.h` (both hooks kept) and `KeyboardEntryActivity.{h,cpp}` (Silkscreen's `preventAutoSleep()` with upstream's `inputPollDelayMs()` and `showTouchFeedback()`). Every other Silkscreen hunk was checked against the merge base's delta and survives, among them the Home carousel's hint gate, the Library hint-band lines (`deviceHasFrontButtons(gpio)` where upstream has `hasTouchHardware()`, in `buildListScreen()` and `render()`), the keyboard's hint-band rectangle, `SettingsActivity`, `SettingsList` and `main.cpp`'s front-light and sleep paths. `DeviceCapabilities.h` holds upstream's `deviceUsesHorizontalSideButtonsForMenus()` beside Silkscreen's helpers, and `SettingsActivity.cpp` includes it once. **Taken as is:** `1162fa43`, menu buttons that follow the device layout: Left and Right switch the reader menu's tabs on the Root pane (on Silkscreen `deviceHasEdgeSideButtons()` is false, so the directions are not swapped), which is how the tabs are reachable by keys. `461b7074`, a silent restart replays the live light state: the replay still goes through `HalFrontlight::begin()`, whose `restoreAllowed()` refusal and no-load memory gate it, so a restart relights only what a user action or Restore Light on Wake had lit. `c3e2d42f`, a faster sleep entry: `APP_STATE.saveToFile()` moves after `goToSleep()` and the 500 ms settle goes; every SD write in `enterDeepSleep()` still comes before `Storage.shutdown()` and Silkscreen's sleep tail is untouched. The Library scan token is an RTC word and an atomic flag, with no SD access before the wake decision. `setup()` consumes it just before the SD mount, once the early re-sleeps (a ghost wake, a release wake, the reset-loop and empty-battery sleeps) are past, so a re-sleep leaves the RTC token for the next real wake: consuming it at the top, as upstream does, made a short Power tap on the sleeping reader cost the next wake a Library rescan. **Watch:** with touch fitted, `KeyboardEntryActivity` polls every 2 ms instead of 10 ms (awake only, bounded by the ceiling); the KOReader progress screen's action buttons keep upstream's touch gate (`resultActionLayout(hasTouch)`), so with touch fitted they reserve no key-hint band and may overlap the hints drawn below them (the same gate as before the merge; check it on the panel, C12, with the key-access fixes). The reader drawer's `buttonDevice` argument (`EpubReaderDrawerActivity::onEnter()`) is upstream's `!mappedInput.hasTouchHardware()`, with no Silkscreen patch before or after the merge: with touch fitted the drawer leaves out Reading Stats, Sync Progress, Nearby Position Sync and Send Nearby Book (`EpubReaderMenuModel.h`). That belongs to the key-access fixes. **Also:** `1.6.0` to `1.6.1`, the battery in 1% steps, and the sizes in Build. Builds after the review fixes (`b2e457f3`, `301b6d63`): `silkscreen` 5,927,761 B flash, `x4-pro` 5,914,225 B, `x4-classic` 5,776,406 B, `c3check` 6,350,263 B (188,800 B of the OTA slot free), the sleep-wake guard and the touch-gate audit passing, and the same 7 narrowing warnings as before. |

**Each sync:**

1. `git fetch crossink`, then `git rev-list --left-right --count crossink/development...silkscreen` to see what comes in.
2. `git merge crossink/development`. For every conflict, follow AGENTS.md: find the upstream PR in the commit messages and read it before resolving. Keep the upstream intent, and re-apply the Silkscreen hunk on top.
3. **Resolve `freeink-sdk` to the shared head.** Let `T` be CrossInk's new pin (`git ls-tree crossink/development freeink-sdk`) and `S` the shared head.
   - **`T` is an ancestor of `S`** (`git -C freeink-sdk merge-base --is-ancestor T S`): keep `S`.
   - **`T` is a Free-Ink `main` commit not yet on the shared line:** first merge `T` into the shared `silkscreen` branch at its home, `crosspoint-reader/freeink-sdk`. That is outside this repo and follows CrossPoint's PORTING.md, "Upstream merges", step 2: upstream's new struct fields first, Silkscreen's after. Then resolve to the new head. CrossPoint's gitlink then needs the same bump.
   - **`T` is on no Free-Ink branch** (a pre-merge PR pin, as `cc05859`, `4bd18a2` and `481da7d` were in September; after `git -C freeink-sdk fetch origin`, `git -C freeink-sdk branch -r --contains T` lists nothing, or `T` cannot be fetched at all): keep `S`, and build. If CrossInk's new app code needs that PR's SDK API, wait until the PR lands in Free-Ink `main`, then take the case above.
   - **Never resolve with `--theirs`** without checking: it drops every Silkscreen SDK commit.
4. `git -C freeink-sdk fetch silkscreen-sdk`, then `git submodule update`, then the check under [The shared SDK](#the-shared-sdk).
5. **CHANGELOG:** move every "Silkscreen:" line back under `## [Unreleased]`. A CrossInk release commit renames `[Unreleased]` to its version and sweeps our lines into it. Recreate `[Unreleased]` if the merge left none.
6. **Re-run the [drain checklist](#drain-checklist)** on what the merge brought.
7. **Build** with `scripts/silkscreen/build_matrix.sh` (`--full` adds `silkscreen-debug`, `x4-pro-debug` and the
   Sticky proxy). It builds the `silkscreen` envs from the real config first and the proxy envs after them, because a
   proxy config wipes `.pio/build`. What it covers:
   - `silkscreen` and `silkscreen-debug`, with the guard, the size check and the touch-gate audit;
   - `x4-pro` and `x4-classic`;
   - the C3 and Sticky builds: the real `default` and `sticky` where CrossInk's core is installed, the proxies otherwise.
   - On a host with a C++ compiler, also the simulator envs and the `test/` CMake suite ([Simulator and host tests](#simulator-and-host-tests)).
   - Where CrossInk's core is installed, CI's cppcheck step over the `default_envs`: `pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high`.
8. Commit the merge. List the commits it takes and each conflict's resolution. Add the sync's row to the sync records above.
9. **Measure:** spot-check the sleep current (MEAS-01) whenever the merge changed sleep, boot, `loop()` or network code.

**Upstream features that bring CrossPoint's rules with them.** CrossInk takes upstream CrossPoint selectively. When one of these arrives, port CrossPoint's counterpart in the same merge:

- **The SD-card plugin system (upstream #3114)**, with the loan clock. Port CrossPoint `4505ceec`:
  - `pluginevents::drain()` stops on `HalPowerManager::backgroundNetworkMustStop()`, which already exists here;
  - `shouldAbort` is threaded through the plugin HTTP helpers, and `/api/fetch` and `/api/relay` abort on the cutoff;
  - the plugin-job claim poll is left out of the client traffic;
  - the radio goes off on the plugin picker and when the loan-clock Wi-Fi picker is cancelled or fails;
  - a WK-02 sleep skips the plugin delivery: `enterDeepSleep(true, /*unattended=*/true)` in CrossPoint.

  Then update the "rows that do not apply" in [POWER.md §3](POWER.md#3-firmware-rules-and-where-they-live).
- **The upload cancel hook (upstream #3732):** add `lowBatteryCutoffTripped()` to `setUploadCancelCheck`, which closes BAT-01's multipart-upload gap.
- **SdFat's second FAT cache (upstream #3685):** add CrossPoint's `.gitattributes` line, which keeps `scripts/sdfat_patches/*.patch` LF on Windows. Consider `USE_SEPARATE_FAT_CACHE`.

**Keeping up with CrossPoint.** Periodically:

```bash
git fetch crosspoint
git log --oneline e7e62f48..crosspoint/silkscreen -- . ':!src/activities/testsuite' ':!scripts/check_test_firmware.sh'
```

Port new drain fixes and board behaviour, update [POWER.md §3](POWER.md#3-firmware-rules-and-where-they-live)'s anchors, and move the "last CrossPoint commit ported" at the top of this file. CrossPoint's own uncommitted work is not ported until it lands as a commit.

**Refreshing the copied docs.** REQUIREMENTS.md, MODULARITY.md and POWER.md's sections 1, 2, 6 and 7 are verbatim copies:

1. Take them from the new CrossPoint commit with `git -C ../crosspoint-reader show <commit>:docs/silkscreen/<file>`.
2. Keep each provenance note, with the commit updated.
3. Keep this folder's section titles that their links land on:
   - "Architecture: Silkscreen is data", "Bring-up checklist (real board)", "Upstream merges" and "Next" here;
   - "Bench bring-up (BRING_UP.md)" and "Variant coverage" in TEST-SUITE.md;
   - "6. Hardware notes for a future revision" in POWER.md.

**Platform.** If CrossInk changes `[base]`'s platform, re-check the Silkscreen override in `[silkscreen]`. If CrossInk moves to 55.03.311 or later itself, the override can go.

### Drain checklist

Run after every merge and after any change to sleep, boot, `loop()`, network, USB or front-light code:

1. **Guard:** `python scripts/check_sleep_wake_sources.py` passes.
2. **Keep-awake screens:** `rg "preventAutoSleep\(\) override|skipLoopDelay\(\) override" src`. Each new or changed screen either holds auto-sleep off only while it works, bounded by its own timeout, or is gated on `radioidle::enabled()` like its neighbours ([POWER.md §3.1](POWER.md#31-network-screens-aw-02)).
3. **Radio starts:** `rg "WiFi\.(mode|begin|softAP)|esp_now_init|startRadio|beginEspNow" src`. A screen that brings the radio up must switch it off at its result screens. A new shortcut that starts the radio goes into `radioShortcutRefused()` (CI-05). A new network-boot screen keeps its restart on exit, with a `radioReleased` flag where it releases the radio early.
4. **`loop()` order:** nothing new may return from `loop()` before `sleepOnBatteryAwakeLimits()`, except the exclusive-storage branch (CI-02).
5. **Sleep entries:** `rg "esp_deep_sleep|startDeepSleep|enterDeepSleep\(" src lib`. Every sleep goes through `enterDeepSleep()` or `HalPowerManager`. New SD writes in `enterDeepSleep()` come before `Storage.shutdown()`.
6. **Restarts:** `rg "ESP\.restart|esp_restart" src`. A restart that continues the session goes through `restartWithSilentToken()`, so it carries the awake clocks (CI-04).
7. **Blocking transfers:** a new download, upload or sync that blocks the loop polls `powerManager.lowBatteryCutoffTripped()` in its abort or cancel path (BAT-01). `HttpDownloader` users get it for free.
8. **Client traffic:** a new web endpoint that clients poll on their own does not count as traffic (the middleware in `CrossPointWebServer::begin()`).
9. **Input:** a new input path counts as input only when the user meant it. Quick Lock rejections do not (CI-03).
10. **Front light:** a new path that switches the light on goes through `HalFrontlight`, so the probe and the restore refusal apply. Nothing lights it before the wake decision.
11. **Wake sources, timers, tasks:** nothing new runs in sleep or arms a wake (ZD-01). Nothing new runs before the wake decision in `setup()` (WK-01).
12. **Simulator:** every new HAL call from `src/` is under `#ifndef SIMULATOR` or behind a shim.
13. **Front-key UI:** a new `hasTouch()` gate that is really about the front keys uses `deviceHasFrontButtons()` or `boardHasFrontButtons()` instead. New HAL methods avoid the names `scripts/check_app_touch_gate.py` forbids.
14. **Rules:** re-read every row of [POWER.md §3](POWER.md#3-firmware-rules-and-where-they-live) whose code the merge touched.

## Silent hazards

The re-scope of 2026-10-01 listed these traps: each compiles fine and fails only on the board. Most are handled; the rest are watch items for every merge.

| Hazard | State |
|---|---|
| CrossInk's own sleep tail on Silkscreen: no touch hibernate (+165 µA), no I2C release, the light not handed over, the release wait's result ignored | Handled (S4). The guard fails the build if the tail could run on Silkscreen. |
| USB presence without the charge ladder: the battery limits fire on a charger, the cutoff sleeps a charging cell, USB Drive never ends | Handled (CI-01). |
| Battery checks placed after `loop()`'s early returns: Quick Lock with Never, or a held chord, never sleeps | Handled (CI-02). **Watch:** new early returns. |
| The ceiling on `lastActivityTime`: it never trips on a keep-awake screen | Handled: HalPowerManager's own clocks. |
| Network-boot restarts skipped when a screen releases the radio early, which leaves the app without fonts or a reader stack | Handled (`radioReleased` in OTA and Calibre; KOReader Auth always restarts). **Watch:** new network screens. |
| CrossInk-only keep-awake screens (Nearby, BackupStats; OTA, FontDownload, ClockSync, KeyboardEntry and WifiSelection spinning or holding auto-sleep off) | Handled (AW-02, CI-06). **Watch:** conflict resolution dropping a hunk in these both-touched files. |
| An env missing from `scripts/git_branch.py` gets version `dev`; one missing from `device_suffix` raises `KeyError` | Handled (S2). **Watch:** CrossInk reworking the script. |
| The front light's LEDC hand-over and `WebServer::addMiddleware` are validated on 3.3.11 only | Handled: per-env platform. **Watch:** `[base]` platform changes. |
| Touch gates hiding the key UI; a new HAL method name that `check_app_touch_gate.py` forbids | Handled (S8). **Watch:** new `hasTouch()` gates. |
| The touch probe before the wake decision, which wakes a hibernated controller on every ghost wake | Handled (WK-01). |
| Short key taps lost at idle | Handled (CI-07). **Bench:** C4. |
| The SDK gitlink in merges: `--theirs` drops the Silkscreen SDK; a forgotten `git submodule update` builds a stale SDK | Procedure above. |
| Upstream features that arrive later (plugins, upload cancel, the FAT cache) and bring unbounded radio paths | Procedure above. |
| "Silkscreen:" CHANGELOG lines swept into a release heading | Procedure above. |
| The shared SD card: caches rebuild on every switch (sections 79 vs 50, `book.bin` 9 vs 10, `library.idx` 6 vs 2) | Accepted; **bench:** C11. |
| The residual awake tail (no `CONFIG_PM_ENABLE`, 80 MHz idle) and the debug env's 250 ms boot wait | Documented ([POWER.md §5](POWER.md#5-the-awake-idle-tail)); take numbers from `silkscreen`. |
| The shared SDK changes other boards (keyboard, header text, settings and SdFat writes) | CHANGELOG lines; matters only if those boards are flashed from this fork. |
| Quick Lock counting rejected presses; single-press radio shortcuts; the light switched on by the schedule; silent restarts resetting the clocks; USB Drive's exclusive loop; the Serial/JTAG handoff; the ceiling against the user's timeout | Handled (CI-03, CI-05, ZD-08, CI-04, AW-03, S2, AW-01). |

## Device-macro sites left at the defaults

CrossInk keys a few layout and naming choices on device macros. Silkscreen gets the default branch at each; check them on the panel (C12):

- `src/CrossPointSettings.cpp` `getDefaultDeviceName()`: "CrossInk".
- `src/components/UITheme.cpp` `getTopStatusBarInset()`: no inset; the X4 Pro and X4 Classic get 5 px in portrait.
- `src/activities/reader/EpubReaderActivity.cpp` `publisherPageNumberX()`: no landscape-CCW offset; that is X4 Pro and X4 Classic only.
- `src/components/UIThemeTokens.h` `listScrollInset`: 0.
- `src/activities/boot_sleep/SleepActivity.cpp`: the sleep popup shows, as on every board but the X4 Pro and X4 Classic.
- `src/activities/settings/SettingsActivity.cpp`: the Sunlight Fading Fix row's X4 Pro / X4 Classic device-macro gate leaves it in; its touch gate in `src/SettingsList.h` is routed (above), so the row stays on Silkscreen with or without the touch panel, as in CrossPoint.
- `src/main.cpp`: the recovery chord is Up + Power, which on Silkscreen is a ladder key, not a strap pin.

## Bring-up checklist (real board)

**First the board, with CrossPoint's images.** Do CrossPoint's bring-up checklist (its PORTING.md, B0-B10) with its `silkscreen-test` image ([TEST-SUITE.md](TEST-SUITE.md)), and its reader image for that list's reader-only steps. A failure there belongs to the board or the shared SDK, not to CrossInk. The test image goes on and comes off over USB only.

**Then CrossInk.** Flash `pio run -e silkscreen -t upload` and work through C1-C12, on battery unless a step says otherwise. Use a bench supply on J5 where a voltage is named. Take every timing and current from the release env, and the log lines that are `LOG_DBG` from `silkscreen-debug` ([POWER.md §4](POWER.md#4-measurement-and-bench-checks)).

1. **C1 - Boot and identity.**
   - The log shows `Device: silkscreen`, then `Panel controller SSD1677/UC8279 (probed)`, or `(kept from the probe before the sleep)` after a wake.
   - The touch probe lines come after the wake decision (`silkscreen-debug`).
   - Settings > System shows `1.6.1-silkscreen`.
   - After a USB Drive session and eject, USB Serial/JTAG comes back without a power cycle.
   - The SD firmware update rejects an image built for another board and accepts CrossPoint's `silkscreen` image.
2. **C2 - Sleep current** (MEAS-01, [POWER.md §4](POWER.md#4-measurement-and-bench-checks)).
   - ≈58-73 µA without touch, ≈113-130 µA with the FT6336U tail (`silkscreen-debug` logs `Touch controller asleep`).
   - Within ≈2 µA of CrossPoint's figure on the same board, after every CrossInk sleep path.
3. **C3 - Wake and boot checks.**
   - A short tap re-sleeps with no touch probe.
   - Short Power Button = Sleep makes a click wake.
   - A hold through a sleep entry gives one release wake and a re-sleep.
   - 3 forced panics (a test build) log `3 abnormal resets in a row` and sleep at the floor; a press then boots normally.
   - At ≈3.35 V a boot sleeps before SD starts; on USB it must not.
   - With R72 fitted next to R36 (a rework only), the power-button fault screen stays up on USB and sleeps on the cell within ≈12 s; SW11 starts the board again.
4. **C4 - Keys.**
   - After 3 s idle, quick taps of ≈20-30 ms on every key of both ladders and a quick IO18 press all register, also on the USB Drive screen.
   - 50 presses each give 50 events.
5. **C5 - Charging and the label.**
   - A wall charger shows the charging bolt ≈1.5 s after plugging in (up to ≈9.5 s with a full cell), and the bolt goes on unplug.
   - On USB with no cell, the header and the status bar say "No battery", with an empty glyph and no bolt, also with Hide Battery % = Always. The sleep overlay strip clears it fully, and a long title in the Minimal header does not overlap it.
6. **C6 - Awake limits** ([POWER.md §4](POWER.md#4-measurement-and-bench-checks), "Awake limits"): Never on Home, the file-transfer screen at a 20-min timeout (no client, a browser uploading, a phone only), the reader's own timeout, a held chord, an automatic page turn, Quick Lock with Never, a wake with no input, no SD card, the cutoff ramp with a download in flight, a low-heap restart, and a USB Drive cable pull.
7. **C7 - Radio** ([POWER.md §4](POWER.md#4-measurement-and-bench-checks), "Radio"): the network result screens idle with Wi-Fi off and still restart on Back; KOReader's modem sleep and Upload; the Wi-Fi list; the Nearby windows, results and offer timeout against a second reader; radio shortcuts at ≈3.4 V; OTA and SD updates below 3.6 V.
8. **C8 - Front light.**
   - **No light fitted:** switching it on leaves it dark within ≈0.5 s, then "No front light detected" appears with the 10 s advice. After a reboot the light reads off.
   - **Restore with no light:** with Restore Light on Wake on, or inside the daily schedule's window, unplug the light while asleep and wake. The restore tries once, and the popup follows the first screen. The next wake logs `Frontlight not restored` and leaves the boost off. A power cycle tries once again.
   - **Single-string light:** it lights the string that is there; the panel says "Warm light only" or "Cool light only" and pins the warmth.
   - **Light Controls** (Settings > Display > Frontlight): Left/Right set the brightness, Up/Down the warmth, Confirm switches the light, Back closes it, and the light is saved on exit.
   - **Light Controls from a book:** the reader menu's Settings tab lists Light Controls, and it returns to the menu when the panel closes. With Short Power, Long Power or Long-press Confirm set to Light Controls, that key opens the panel in a book and on Home without lighting anything, and the same key closes it (a long press of Confirm, when that is the shortcut). Left and Right step 1 % up to 10 % and 5 % above it, and a held key takes 0 to 100 % in about 5 s (10 to 100 % in about 3 s) where it took about 10 s.
   - **Shortcut with the panel open:** with no light fitted and Short Power set to Toggle Frontlight, open Light Controls and press Power. The panel's "No front light detected" note shows, Left/Right only set the level and do not relight the boost, and after closing the panel and rebooting the light reads off. In the reader's light overlay (reader details), the same press shows the popup at once.
   - **Blend:** warmth 30-70 % mixes the strings, and the first switch-on after a reset lights each string through its probe window.
   - **Ghost wake:** it never lights the LEDs.
   - **Sleep:** the light goes dark at the start of sleep entry.
9. **C9 - Keys alongside touch.**
   - The hint band shows, with nothing overlapping it, on:
     - Home in Lyra, Minimal, Dashboard and Cover Grid;
     - Library, File Browser and Settings;
     - Controls, Reader and TTF options;
     - the keyboard (its bottom row above the hints, and one tip line above the keys) and clip selection;
     - KOReader sync and the Nearby menus;
     - with Key hints on touch screens on: ClearCache, the Nearby stats, position and transfer screens, the clipping detail, IntervalSelection (Sleep timeout, Auto Page Turn, line spacing, margins), the dictionary alternate-form prompt and Calibre's connection-failed screen draw the band under their buttons; the reader menu's sheet shows its key hint above the tab bar on every tab and sub-list (on two lines where one would be cut off, at the default and the Large UI scale; it keeps its rows); the light panel shows its key hints. With the setting off, none of these changes, and the layouts are the touch-only ones.
   - Controls > Front Buttons lists Remap (x2), Orientation, Long-press behaviour, Long Back and Long Menu, with no `Unexpected controls menu counts` in the log.
   - Remap shows the live mapping, and Quick Actions lists Long Back and Long Menu.
   - On Minimal and Dashboard home, the front keys act as labelled.
10. **C10 - Clock.** After a full power loss the boot log shows `RTC time invalid`, and the next Wi-Fi connection sets the clock.
11. **C11 - Shared SD card.** Switch CrossPoint → CrossInk → CrossPoint → CrossInk on one card:
    - each switch rebuilds the section, `book.bin` and `library.idx` caches and opens the current book;
    - `state.json`, `recent.json`, `wifi.json`, `opds.json` and `koreader.json` read fine in both readers, with neither rejecting or rewriting the other's file;
    - CrossInk imports CrossPoint's `settings.json` only while `crossink-settings.json` is missing, so CrossPoint's choices (a Never timeout included) carry over once.
12. **C12 - Panel.**
    - FULL, HALF and FAST refreshes are clean on both panels with the SDK's Silkscreen SSD1677 drive values.
    - Closing the reader menu leaves the page's gray levels clean.
    - The device-macro defaults above look right: the top bar in portrait, the publisher page number in landscape CCW.

## Simulator and host tests

This host has no C++ compiler, so neither the simulator nor the `test/` CMake suite has run since the port began. Every change was written to compile there, through the gates above. One `test/` target compiles a changed file: `firmware_board_tag` builds `src/network/FirmwareBoardTag.cpp` for Sticky, so the new Silkscreen branch is never reached. `option_popup` gets `BaseTheme.h` and `UITheme.h` from its own stubs (`test/option_popup/stubs`), not the changed `src` headers.

On Linux or macOS (or WSL; see `docs/simulator.md` for the macOS-only paths), run:

```bash
pio run -e simulator
pio run -e x4-pro-simulator
pio run -e sticky-simulator
python scripts/run_simulator_smoke_test.py
```

Also run the `test/` CTest suite.

**`x4-pro-simulator` is the closest to Silkscreen.** CrossInk has no Silkscreen simulator profile; the X4 Pro profile approximates touch and keys together:

- touch input and the keys (arrows, Return, Escape, P, and H for the X4 Pro's Home key) both work;
- the front light is a PWM light.

It differs from Silkscreen in three ways:

- **Front-key UI.** The front-key helpers fall back to `!hasTouch()` in the simulator, so it shows the X4 Pro's touch-board UI: no hint band, no Front Buttons settings. Check that side in `simulator`, the keys-only profile, which draws them.
- **No Silkscreen code.** Every Silkscreen mechanism (`FREEINK_CAP_POWER_POLICY`, the front-light probe, the radio rules) is compiled out of every simulator env.
- **What it is for.** The simulator checks that the shared code builds and that the other boards' UI is unchanged. Silkscreen's own behaviour needs the board (C1-C12).

## Next

- **The bring-up checklist** on a real board, then the fixes it finds (the plan's S10 stage).
- **Real-core builds:** `pio run -e default -e sticky -e x4-pro -e x4-classic` on 55.03.37, then `pio run -e silkscreen`.
- **CI's cppcheck step,** never run on this host: `pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high` (the `default_envs`). A standalone cppcheck 2.20 run with the X4 Pro's defines flagged board-constant folds in port code: `knownConditionTrueFalse` at `UsbDriveActivity::loop()`'s `cablePulledOnBattery() || batteryLimitReached()` and `cablePulledOnBattery()`'s `hasChargeStatusLadder()` test, and at `HalFrontlight.h`'s `noLightFound()` and `stringMissing()`; and `functionStatic` for `UsbDriveActivity::batteryLimitReached()`. That run also flags unchanged upstream files, so it does not prove CI fails. If `pio check` reports them, add `// cppcheck-suppress knownConditionTrueFalse` above each line, as `LyraCarouselTheme.cpp` does, and make `batteryLimitReached()` static.
- **The simulator and host tests**, on Linux or macOS.
- **Open items carried from the stages:**
  - The multipart-upload cutoff gap (BAT-01), until CrossInk takes upstream #3732.
  - Phantom touch edges, such as a cover rubbing the screen in a bag, count as input and reset every clock; the input cap does not bound them.
  - After more than 60 min on USB with no input, unplugging sleeps the device about 1 s later, once the ladder decodes "on battery". USB Drive and web transfers count for this too.
  - ESP-NOW connectionless power save in the Nearby windows: not tried.
  - The new strings exist in English only, and other languages fall back to English: `STR_POWER_BUTTON_FAULT`, `STR_BATTERY_TOO_LOW_FOR_UPDATE`, `STR_NEARBY_STOPPED_WAITING`, `STR_FRONTLIGHT_CONTROLS`, `STR_FRONTLIGHT_NOT_DETECTED`, `STR_FRONTLIGHT_COOL_ONLY`, `STR_FRONTLIGHT_WARM_ONLY`, `STR_NO_BATTERY`.
- **Before any release channel** (as for CrossPoint, [MODULARITY.md, Future work](MODULARITY.md#future-work)): a board tag per panel family, and an OTA feed for the fork.
- **Optional SDK wording:** the `awakeCeilingMin` comment (`BoardConfig.h`) and the SDK README could say that the consumer may extend the ceiling to a longer user timeout. That would be an SDK commit on the shared line, then a gitlink bump in both repos.
