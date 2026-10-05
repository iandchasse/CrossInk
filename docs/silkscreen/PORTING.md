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
- [BATTERY-LOG.md](BATTERY-LOG.md): the battery sleep/wake log (the `silkscreen-batlog` build): a pointer to CrossPoint's manual, and what differs on CrossInk.

**Base:**

- CrossInk `development` at `9dd334d3` (2026-09-29), as cloned; merged up to `3efe61b6` (2026-10-02, v1.6.1 and what followed) in `a9f9ef64`, and up to `6fbc97d1` (2026-10-04) in `6e182dfe`.
- freeink-sdk at the shared Silkscreen SDK head, `d5dd6a7` ([The shared SDK](#the-shared-sdk)).
- The last CrossPoint `silkscreen` commit ported is `c9abee96` (2026-10-03), the rounding fix of the 1% battery (`844dde61`). CrossPoint's `silkscreen` head `18979516` has been read through: what follows `8cb94736` is ported or recorded below as not applicable.

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
| Key access | `8d0408f7`, `3ee96afa`, `0156d6d2`, `278acbd0`, `9c02f7e5`, `1aaca073`, `5164c0da`, `ccebb480`, `b164b3c4`, `d6433dec`, `15efe9c9`, `e8a6ea84`, `c28efd85`, `a5124976`, `41a2769e`, `7af113d2`, `84393a63`, the review fixes `58d0efb5`, `f1b5aeff`, `64bc8865`, `7cf46211`, `f1752e72`, `ca6f853e`, and the `docs:` commits `b914b585` and the one that records them | The key-access audit's batches D, A, B, C, E, F, H and G ([Key access](#key-access)): every function that needed a finger can be reached from the keys, with the touch tail fitted or not. |
| Battery log | `fea3aca5`, `10950eac`, `30a9845c`, `efd83ada`, `70560b82`, `7cdf99fe`, the fixes `a898b97c` to `ee4e5fcf`, `ce38fac7`, and the `docs:` commits after them | The `silkscreen-batlog` env, its own build and not a setting ([Battery log](#battery-log)). The SDK moves to the shared head `d5dd6a7` (the log's SDK half and its review fixes), `scripts/git_branch.py` injects the `-silkscreen-batlog` version, `src/util/BatteryLogWriter.{h,cpp}` is CrossPoint's SD writer, and the HAL and NTP feeds and the sleep, wake and `loop()` hooks sit in `src/main.cpp`, `lib/hal/HalPowerManager.*`, `HalGPIO.cpp` and `HalClock.cpp`, all inside `#if SILKSCREEN_BATLOG`. No setting, screen, i18n key or Quick Action: stop by flashing `silkscreen`. Expected sleep-current cost zero, **not measured**. |
| Key access, second audit | `6357ec7d`, `b5df7d65`, `e48bfc40` and the `docs:` commit that records them | Batch A of the second key-access audit (run on tree `c9157c4c`; [Key access](#key-access)): the Home/Reader shortcut is offered on the touch + keys board too, a Home/Reader Quick Actions slot survives a restart, and the Library scan screen names Back. Built: `silkscreen` 5,613,609 B flash and 115,536 B static RAM (the `a681d04a` build was 5,613,605 B and 115,536 B), the sleep-wake guard passing and the same 7 narrowing warnings. |
| Key access, second audit, batch B | `a5b764a3`, `b623df48` and the `docs:` commit that records them | Batch B of the second key-access audit (#1, [Key access](#key-access)): a Silkscreen without its touch tail (C2) gets the reader menu as a full-screen layout like the keys-only X4's, so the Spacing and Page Margins sliders and their step hints, the Go to % slider and keypad and the Auto Page Turn hints are on the screen and clear of the key hints, with no preview and no "Preview unavailable". In landscape the Spacing, Margins and Go to % panes leave out the book title and progress band. Built: `silkscreen` 5,614,317 B flash (708 B above batch A's 5,613,609 B) and 115,536 B static RAM, the sleep-wake guard passing and the same 7 narrowing warnings. Not run on a unit or in the simulator: the layout is checked by arithmetic. |
| Key access, second audit, batch C | `5575e585` and the `docs:` commit that records it | Batch C of the second key-access audit (#2, [Key access](#key-access)): in a Settings submenu (Frontlight, Font options, Side Buttons and the rest) the front Left and Right keys step the rows like Up and Down and the hint band names them Up and Down, where they switched the category and closed the submenu while the band called them Left and Right. Back and the top-level category switching are as before. Built: `silkscreen` 5,614,401 B flash (84 B above batch B's 5,614,317 B) and 115,536 B static RAM, the sleep-wake guard passing and no new warning (an incremental build, which printed the 3 narrowing warnings of the files it recompiled, none in this code). Not run on a unit or in the simulator. |
| Key access, second audit, batch D | `9196b0f1`, `4fd5da6e` and the `docs:` commit that records them | Batch D of the second key-access audit (#7 and #8, [Key access](#key-access)): the reader menu's More tab has a Dark Mode row, and Disable Touchscreen is in Controls > Taps and Gestures, which the menu's Controls row opens. Both are for the Silkscreen profile only (`boardKeepsKeysBesideTouch()`), with the touch tail fitted or not for the first and fitted for the second. Built: `silkscreen` 5,614,565 B flash (164 B above batch C's 5,614,401 B) and 115,536 B static RAM, the sleep-wake guard passing and the same 7 narrowing warnings (none in this code). Not run on a unit or in the simulator, and the host tests (`ReaderDrawerModelTest`, the simulator smoke test) have not run: the catalog's counts were checked with constexpr asserts under the xtensa g++. |

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
- `85d86188` and `e7e62f48` (CrossPoint's own docs: the record of the `5ab0f290` sync and its final build): nothing to port;
- `56a323b7`, CrossPoint's merge of upstream `cdac66ff` (Metalio E-Ink 4 with its haptics and page keys, #3848, #3802, #3410), with its SDK head `7279bae`: the SDK move is this repo's `6e182dfe`. No Silkscreen rule changed: its four conflicts keep CrossPoint's Silkscreen hunks beside Metalio code that CrossInk does not have (`HalGPIO::update()`, `MappedInputManager::update()`, the `AfterUSBPower` list and `ci.yml`), and CrossInk's own upstream has no Metalio board;
- `5d239070` (CrossPoint's own docs: the record of that sync): nothing to port;
- `4821b7c2`, the cutoff poll in WebDAV GET and PUT and in `/download` (`CutoffAbortPrint`): **not ported**. CrossInk's `handleDownload()` streams in its own loop (`src/network/CrossPointWebServer.cpp`) and its WebDAV GET is still upstream's `client.write(file)` (`src/network/WebDAVHandler.cpp`), which since the `HalFile` wrap sends one byte (upstream CrossPoint's #3410, not yet taken by CrossInk), so both keep the gap that CrossPoint closed. Port the adapter with the #3410 fix ([POWER.md](POWER.md), BAT-01, "Not covered");
- `f935da24` and `abdb2fa7` (CrossPoint's own docs: the record of that fix and of the SDK half of its `cdac66ff` sync): nothing to port;
- `1b1f5c27`, CrossPoint's merge of upstream `9ea86717` (#3857, skip unused Arduino SDK libraries): nothing to port. It edits `[firmware_tuned]` and `[firmware_tuned_c3]` in CrossPoint's `platformio.ini` only, with no SDK move (the shared head stays `7279bae`). CrossInk's own upstream has not taken #3857, so its `[firmware_tuned]` keeps the three deselections it has (RainMaker, Insights, ESP_SR), and only `default`, `debug`, `sticky` and `sticky-debug` inherit it: `silkscreen`, `silkscreen-debug`, `x4-pro` and `x4-classic` extend `[base]` alone;
- `9bf57487` (CrossPoint's own docs: the record of that sync): nothing to port;
- `0d8b4567` to `b30fd025`, CrossPoint's battery log (the `silkscreen-batlog` env, the SD writer, the HAL and SNTP feeds, the sleep and wake hooks, the analyzer and the review fixes): ported in `10950eac`, `30a9845c`, `efd83ada`, `70560b82` and the fixes after them, with the SDK moves `fea3aca5` and `7cdf99fe` (CrossPoint's `60820777` and `36a20d03`). The analyzer and its tests stay in CrossPoint's `scripts/silkscreen/` ([Battery log](#battery-log));
- `05fb0d59` (CrossPoint's gitlink bump for the SDK README's flag note): this repo's `ce38fac7`;
- `2767bee1` and `85765867` (CrossPoint's own docs: BATTERY-LOG.md, POWER.md BL-01, the sync runbook and `upstream_status.sh`): this repo's pointer page, rule BL-01 and the drain checklist sentence follow them;
- `e48a425b` (CrossPoint's own docs: the battery log's final build sizes): this repo's `a9c0c46c` is its counterpart;
- `3c7fe2c5`, CrossPoint's merge of upstream `50823ffa` (#3863 the keyboard's tips block, #3864 `goHome()` mapping the plugin catalog, #3865 hint labels centred by text height): nothing to port, and no SDK move (the shared head stays `d5dd6a7`). `crossink/development` (`6fbc97d1`) carries none of the three, and the code does not exist here: CrossInk has no plugin system and no `PluginCatalog` activity, no `BaseTheme::drawHintLabel()`, and its keyboard and key hints are its own (`showKeyHintsOnTouch`). If CrossInk's upstream takes them, re-check the keyboard's tips and the hint band against that setting in the same merge;
- `18979516` (CrossPoint's own docs: the record of that sync): nothing to port.

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
- **Cross-board changes.** Three kinds reach every board:
  - What the shared SDK brings: keyboard layouts and edge keys, glyph-centred header text, atomic settings writes, and SdFat write-back before sleep and USB Drive. Each has a "Silkscreen:" CHANGELOG line.
  - `ButtonNavigator` (`src/util/ButtonNavigator.{h,cpp}`, `8d0408f7`), not gated: a one-key braced list such as `{Button::Left}` used to bind to `std::array<Button, 2>` as `{Left, Back}`, so a Back press also fired the Left handler. Every list method gained a `std::initializer_list` overload and a one-key list is now one key, on every board. Home's cover grid, the folder picker, clipping selection, the interval picker, the on-screen keyboard and the reader menu's sliders no longer react to Back. It has its own CHANGELOG line (the one that starts "A screen that listens to a single key").
  - Size, mostly runtime-gated code and English strings. Against S2, on this host's proxy core and including the three later SDK moves, flash changed by (at the port review):
    - x4-pro: -5.3 KB (GCC now inlines the settings catalog differently there);
    - x4-classic: +3.3 KB;
    - the C3 proxy: +3.5 KB;
    - the Sticky proxy: +3.2 KB.

    RAM grew by 24-40 B. `getSettingsList()` is sensitive to this: a second `std::remove_if` over the catalog once made GCC outline `getBaseSettingsList()`'s lambda as well as inline it (+8 KB on every touch build), so a new condition goes into an existing erase.

  Apart from these three, behaviour on other boards is unchanged. `HalPowerManager::startDeepSleep()` on x4-pro and x4-classic was checked instruction-identical across the S4 change.

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
  - the Home/Reader shortcut (`QuickActions::isActionAvailable()`, since the second key-access audit);
  - the key help in File Browser, Nearby transfer and Home;
  - Settings > Display > Sunlight Fading Fix (`getSettingsList()`), so Silkscreen keeps the row whether or not its touch panel answered, as in CrossPoint.

  These routed gates use `deviceHasFrontButtons(gpio)`, which equals the old `!gpio.hasTouch()` on every other board. The gates the port added, the light panel's key entry points (`frontlightPanelFromKeys()`: Light Controls in Settings > Display > Frontlight, in the reader menu and as a shortcut) and the panel's Up/Down warmth keys and key steps, use `boardHasFrontButtons()` (the board profile) instead, so an X4 Pro whose touch panel fails its probe gains none of them.

  Key-route fixes that only Silkscreen needs (held Back discarding a Reading Stats date edit, Right creating a clipping in a dictionary lookup, the OPDS search and Wi-Fi network-list input leaks, the frontlight time picker's labels and 5-minute step) use `boardKeepsKeysBesideTouch()` instead: the board profile's keys-beside-touch flag (`TouchConfig::buttonsAlongsideTouch`, read as it is: the touch probe never changes it, so it holds with the touch tail fitted or not, where a test on `BoardConfig::hasTouch()` is false without the tail). Only Silkscreen's profile sets it, so it is false on every keys-only board as well as every touch-only one, and X3/X4, the X4 Pro, Sticky and Paper Mono keep their behaviour exactly.

  **Key hints on touch layouts.** Settings > Display > Key hints on touch screens (`showKeyHintsOnTouch`, on by default; the row and the web setting exist only where a touch panel and the front keys are both fitted) decides whether the touch layouts that otherwise show no key labels draw them. `shouldDrawKeyHints(gpio)` in `include/DeviceCapabilities.h` is the one test: true without a touch panel, false on a touch-only board, the setting on a touch unit with keys. The screens that always drew the band on Silkscreen (Home, Library and its scan screen's Cancel hint, Settings, the Nearby menus, KOReader sync's progress choice, ...) are not affected by it. With the setting on, these touch layouts draw their labels:
  - the hint band under the touch buttons of ClearCache, Nearby stats sync, Nearby position sync, Nearby book transfer (offer, collision and result prompts), the clipping detail, IntervalSelection (Back, Select, `-`, `+`, and a line for the side buttons' large step, below), and the dictionary lookup's alternate-form prompt. Each lays its touch buttons out in `UITheme::getScreenSafeArea(renderer, true, false)`, which already excludes the band in all four orientations, so nothing overlaps it. IntervalSelection used to lay out its touch layout on the full screen, so its safe area now follows the same test (`shouldDrawKeyHints()`) in `render()` and `loop()`;
  - Calibre's connection-failed screen, which waited for Back with no hint on any board, gets one on a touch unit with keys only;
  - the reader menu's sheet: a line above its tab bar (`keyHintLineText()`), which is where the key layout's band would sit. The Root line is about 560 px at the default UI scale and 670 px at Large, over a 474 px portrait panel, so `keyHintLineCount()` measures it and the line wraps onto a second one when it does not fit. `drawerHeight()` grows the sheet by those lines, so the list keeps its rows. The Spacing, Margins and Auto Page Turn panes name their keys in the step hints under their sliders instead (`showsStepHints()` follows the setting; `drawerHeight()` grows the Auto Page Turn sheet, as it does Spacing and Margins, until the slider and both hint lines fit above its Confirm button, which landscape needs: the 65 % sheet leaves about 100 px for a 150 px block). The panes Go to % and Go to Page never open on this layout;
  - the light panel: two lines at the foot of its sheet (`keyHintsShown`; the brightness and warmth line wraps onto a second one at the Large UI scale), added to `panelBottom` only when the taller panel still fits the screen;
  - IntervalSelection's side-step line (`showsSideStepHint()`: touch hardware, `shouldDrawKeyHints()` and a large step that differs from the small one): `drawStepHintLine()`'s "Side buttons: 5 min" for Up and Down, the text the layout without touch draws. The plain touch controls (Auto Page Turn, the idle threshold) carry it 8 px under their step controls, drawn only while it ends above the first action button (36 px to spare in landscape). The slider layout carries it 8 px under the slider: the centred slider (no preview) just needs the body to hold row and line, and the slider seated at the foot of a preview takes the line's 28 px from the preview, only while the preview keeps `MIN_PREVIEW_AREA_WITH_HINT` (120 px). By arithmetic with the Lyra metrics, the preview is about 392 px in portrait and keeps 364, and is 112 px held sideways (122 at the small UI scale), so Settings' margin and line spacing pickers go without the line there;
  - the keyboard: a few tip lines between the text field and the keys, as many as that gap holds, the first line first. Cursor mode names the way back to the keyboard, Left and Right for the cursor and, in a password field, the show/hide switch (or, with the switch selected, the switch and the way back to the cursor); typing with text in the field names the hold-Up edit key and hold-DEL clear. They are the strings the layout without touch draws, in the same cases; a later line wider than the screen in some translation is dropped. Portrait on Lyra has about 220 px (three lines need 68), minus 30 px per wrapped line of text; held sideways the gap is about 4 px and nothing is drawn. The keyboard's own band was already drawn.

  Still left out on purpose:
  - the side-button hints (`BaseTheme`/`LyraTheme::drawSideButtonHints()`, the keyboard's `>` `<`): their only caller is the keyboard, whose touch layout (56-px keys) starts at about y 430-490 in portrait, under the hints' fixed 345-506 band at the right edge, so they would cover its top-right keys; the key layout without touch starts below them;
  - IntervalSelection's front step line, which the band's `-` and `+` label and which would share the room of its touch step controls (its hint band is drawn);
  - the keyboard's "Tips:" header and its lines for the secondary characters, the URL snippets and leaving URL mode (the cursor, password and clear lines are drawn, above), which need more room than the taller touch keyboard leaves.

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
| Upload cutoff | Web uploads poll the cutoff | WebSocket uploads and, since the `6fbc97d1` merge (upstream's `c827d4b6`), multipart and font uploads: `CrossPointWebServerActivity::checkUploadCancellation()` also polls the latch (`a681d04a`). The Library scan's cancel poll does too |
| Front-light key path | Settings > Display > Frontlight opens the panel; Long-press Confirm > Frontlight and Short Power > Toggle Frontlight (both since `36ee5cfc`) | Settings > Display > Frontlight > Light Controls (`SettingAction::FrontlightPanel`, `ActivityManager::pushFrontlightPanel()`); the reader menu's Settings tab > Light Controls (`ReaderDrawerCatalogItem::LightControls`); the Light Controls shortcut, `SHORT_PWRBTN::FRONTLIGHT_PANEL` (37, after upstream's `HOME_READER` = 36) and `LONG_MENU_FRONTLIGHT_PANEL` (25), offered where `frontlightPanelFromKeys()` (a light and `boardHasFrontButtons()`) and handled by `ActivityManager::toggleFrontlightPanelFromShortcut()`: it opens the panel without lighting anything. Short Power, Long Power and a held Confirm (for Long-press Confirm) close it again, as Back does, but a side button or Quick Action set to Light Controls only opens it: Up and Down are the panel's warmth keys; CrossInk's own Toggle Frontlight |
| Restore at wake | Restore Light on Wake | Also CrossInk's daily light schedule; a brightness change from 0 while lit is a probed switch-on |
| Reader overlay | `xteinkClassPanel()` honours `x4ClassPanel` | No counterpart; check gray levels by eye (C12) |
| Version | Upstream's version string | `1.6.1-silkscreen` (`scripts/git_branch.py`), file `firmware-silkscreen.bin` |

## Key access

Every function must be reachable with the keys alone, and touch must keep working beside them. The keys that matter here are Back, Confirm, Left, Right, Up, Down and Power; there is no Home key, and the two side keys share one ADC ladder, so Up and Down never read as pressed together. **C1** is a board with the touch tail fitted, **C2** one without it.

The key-access audit (the `silkscreen-key-access-audit` skill, run on the tree after the `3efe61b6` merge) found 32 gaps: 1 blocker, 6 major, 25 minor. Its ids appear below as B1, M1-M6 and m1-m25. The user chose batches D, A, B, C, E, F, H and G, with a Settings toggle for G. The commits are the "Key access" row of the [Status](#status) table.

**Gates.** Each new behaviour is behind a test that is false on every other board, so those boards act as before. The three helpers in `include/DeviceCapabilities.h` are described under [Defaults chosen](#defaults-chosen):
- `deviceHasFrontButtons(gpio)`: true without a touch panel and on Silkscreen with it, false on Sticky and the X4 Pro. It gives a screen that kept a touch layout and dropped its key code the key code back on C1; C2 already had it.
- `boardHasFrontButtons()`: the same from the board profile, for the light-panel entry points (`frontlightPanelFromKeys()`) and the panel's key steps.
- `boardKeepsKeysBesideTouch()`: the profile's keys-beside-touch flag (`TouchConfig::buttonsAlongsideTouch`), true on Silkscreen with the touch tail fitted (C1) and without it (C2). It is not `BoardConfig::hasTouch() && hasFrontButtons()`: Silkscreen's touch panel is optional, and when neither controller answers the probe sets the profile's controller to None, so `hasTouch()` is false on C2 and a test built on it never fires there (the first form of this helper did exactly that, and made the second audit's C2 fixes dead until it was corrected). The flag itself is never changed, and no other profile sets it, so the fixes that only Silkscreen needs leave X3, X4, X4 Classic, the X4 Pro, Sticky and Paper Mono as they were.

The one ungated change is `ButtonNavigator` (below).

**Battery.** None of these adds a wake source, a timer or a task. The front light is lit only by a user action, as before: Light Controls opens the panel dark, and a step from 0 % or Confirm in the panel is the user's switch-on, which still goes through `HalFrontlight`'s probe and restore refusal ([drain checklist](#drain-checklist), item 10). The panel's faster key repeat is a different interval of the same `ButtonNavigator`, and its keys stay idle while the opening key is held.

### Where each function is reached

| Function (audit id) | Configs | Key route now |
|---|---|---|
| Reader menu tabs and rows | C1, C2 | Left/Right switch tabs on the Root pane and Up/Down move rows. Taken from upstream `1162fa43`, unconditional; see the sync record below. |
| Reading Stats, Sync Progress, Nearby Position Sync and Send Nearby Book, in the reader menu (M5, m2) | C1 | Listed by `EpubReaderDrawerActivity::keyNavigation()` (`!hasTouchHardware() \|\| deviceHasFrontButtons(gpio)`), where the catalog took upstream's `!hasTouchHardware()`. On touch-only boards front-light-panel tiles stand in for them, but the panel cannot be opened from a book without a Home key. The drawer resumes after Sync and the Nearby rows (their result sets `PendingOverlayResume`). |
| Go to % and Go to Stable Page (M3) | C1 | Both rows close the menu and open the full-screen `EpubReaderPercentSelectionActivity`, which takes taps and keys; its keypad focus shows when `deviceHasFrontButtons(gpio)`. Touch-only boards keep the hidden focus. A keys-only reader keeps its own in-sheet Percent pane. |
| Line Spacing, Word Spacing, Page Margins and the Auto Page Turn interval (M4, m3) | C1 | The sheet's sliders follow `keyNavigation()`: Confirm starts and stops adjusting, the front keys step by 1 and the side keys by 5. Spacing and Margins name those steps under their sliders (`showsStepHints()`), and `drawerHeight()` grows the sheet to hold them, in landscape too. |
| SD-card font family or size chosen in the reader menu (M6) | C2 | `commitSettings()` no longer rolls the choice back when the menu has no preview model at all, which a touch-less build never has. Only a touch build on a board with `boardHasFrontButtons()` skips the rollback; keys-only builds and touch-only boards keep it. |
| A cursor that Confirm can see (m4, m5): reader menu, Library, Library settings, File Transfer menu, Settings File Browser view, on-screen keyboard | C1 | They start with the cursor showing. A tap or a swipe still hides it; the next key press shows it before it moves or acts (`revealSelection()` in Library and Library settings, `revealHiddenSelection()` in the keyboard, `moveSelection()`'s reveal-only branch in the menu). Password show/hide in the keyboard's cursor mode takes one Confirm. |
| Reader menu sub-lists (m17) | C1, C2 | Left/Right step the rows like Up/Down (`frontKeysStepRows()`, `boardKeepsKeysBesideTouch()`), and the hints say Up and Down. |
| Home, Lyra Carousel theme (B1) | C1 | Back, Confirm and the arrows follow `carouselKeysOff = carouselTouchOnly && !deviceHasFrontButtons(gpio)`. `carouselTouchOnly` itself is unchanged, so swipes work beside the keys. The icon strip's highlight comes from `carouselMenuHighlightIndex()`: the key selection, and a finger-down icon while it is held. |
| Home, Minimal and Dashboard themes (m20) | C1 | Up and Down no longer move a highlight on the hint bar: the front keys act by slot (Back menu, Confirm Browse, Left Settings, Right Read), and each hint names its key. |
| The front light panel from a book (M1) | C1, C2 | The reader menu's Settings tab lists Light Controls (`ReaderDrawerCatalogItem::LightControls`, appended last in `ReaderDrawerAvailability` so positional initialisers keep their meaning); it opens the panel over the menu, and the menu is back when the panel closes. Short Power, Long Power, long-press Confirm, long-press Back, the side buttons and Quick Actions can be set to Light Controls ([persisted values](#persisted-values)). `handleGlobalPowerButtonAction()` handles it next to Toggle Frontlight through `ActivityManager::toggleFrontlightPanelFromShortcut()`, under the touch gesture's own checks (no modal open, the screen allows it, no panel already open), and the EPUB, TXT and XTC readers carry the counterparts of Toggle Frontlight's cases. It opens the panel without lighting anything. Back, Short Power, Long Power and a held Confirm (for long-press Confirm) close it; a side button or Quick Action set to Light Controls only opens it, because Up and Down are the panel's warmth keys. |
| The panel's keys (M2, m9) | C1, C2 | Left/Right step the brightness by 1 % up to 10 % and 5 % above, stopping on the 10 % mark going down; the panel's own `ButtonNavigator` starts repeating at 380 ms and repeats every 150 ms (0 to 100 % in about 5 s). The step keys act only once none of Up, Down, Left and Right is held, so a side button that opened the panel by a long press does not step the warmth while it is still down (R-LIGHT-1). Back no longer nudges the light, and `loop()` ignores a held Back. Its hints are in the last row of this table. |
| The panel's bottom row and Sync tile (m8) | C1, C2 | `showsFrontlightActionBar()` drops the row of touch-only icons where the board keeps its keys and has no working touch panel (C2). `offersFrontlightSyncTile()` drops the Sync tile where the screen that opened the panel does not act on its result (`Activity::handlesFrontlightPanelAction()`): since upstream's `25124472` the base `Activity::handleFrontlightPanelResult()` carries out the choice on every screen, so only Settings and the reader menu, which Silkscreen opens the panel from with no result route, say false. Each tile's function has its own key route, except Sync & Transfer from the File Browser, Library and the other screens that now act on it: that one has the touch tile only. |
| The Up + Down combo (m12) | C1 | Not offered: `deviceSupportsSideButtonChord()` is false while `boardSideKeysShareAdcLadder()` is, and `configuredSideButtonChordAction()` returns Disabled, so a value saved earlier does nothing and stays in the file. |
| Nearby "file exists" prompt (m1) | C1 | The touch buttons draw the key selection, and the prompt starts on Cancel, so a stray Confirm cannot overwrite the book. |
| Reading Stats date editor (m6) | C1, C2 | Back held for 1 s discards the edits (`finishDateEditing(false)`) and suppresses its own release; a short Back still saves. The editor says "Hold Back to discard changes". |
| Create Clipping from a dictionary lookup (m7) | C1, C2 | Right, hinted "Clip", on the definition and on the not-found and alternate-form prompts (`hasClippingKey()`). |
| OPDS search key (m21) | C1, C2 | The Left release that starts a search returns, instead of also moving the selection to the last row. |
| Wi-Fi "show networks" during auto-connect (m22) | C1, C2 | `suppressNextConfirmRelease()` keeps the Confirm release from connecting to the first network. |
| Frontlight schedule time picker (m18) | C1, C2 | The front Left/Right are labelled Prev Field and Next Field, a line says that Up/Down change the value, and a held Up or Down steps the minutes by 5. A held front Right no longer changes the value. The line is kept off the touch keypad in landscape (R-LAYOUT-2). |
| Which key does what, on touch layouts (m16, m17, m9) | C1 | [Key hints](#key-hints-toggle). |
| Home/Reader: back to the open book, or Home, in one key (second audit #3, #4) | C1, C2 | Offered by `QuickActions::isActionAvailable()` where `deviceHasFrontButtons(gpio)` holds (it was `!gpio.hasTouch()`), so Short Power, Long Power, a side button, the Power + Up chord and a Quick Actions slot can be set to it on C1 as on C2. `ActivityManager::handleHomeReaderShortcut()` takes no gate and every trigger reaches it through `dispatchShortcutAction()`, from any screen. A Quick Actions slot set to it is kept at restart on Silkscreen: `isValidQuickActionSlot()` knows value 36 where `boardKeepsKeysBesideTouch()` holds, and every other board keeps upstream's validator, which resets a 36 in a slot to Ignore ([persisted values](#persisted-values)). |
| Library scan cancel (second audit #17) | C1 | `LibraryActivity::drawScanScreen()` draws the Cancel hint for Back beside "Tap to cancel" when `deviceHasFrontButtons(gpio)` (C2 already drew it). The Library list draws its band whatever Key hints on touch screens says, so the scan screen does not ask `shouldDrawKeyHints()`. |
| Reader menu layout without the touch tail (second audit #1, m24) | C2 | The touch image drew the menu as its half-height touch sheet on a unit with no touch panel, with the keys-only layout (129 px book band, 66 px tab bar) inside it: about 200 px of body, the lowest 40 px under the key hints, and a "Preview unavailable" band above. `EpubReaderDrawerActivity::useFullScreenMenu()` (`readerDrawerUsesFullScreen()`: the keys-only build, or `!hasTouchHardware() && boardKeepsKeysBesideTouch()`) now gives that unit the full-screen layout of the keys-only X4: no sheet, `drawerHeight()` is the screen, `requiresFreshBackdrop()` is false and `renderPreview()` returns at once. There is no preview on C2 by design: the sample paragraph code is compiled out of the touch image and the page snapshot is taken only for touch hardware, and neither was added to the build. In landscape (480 px high) the Spacing, Margins and Go to % panes also leave out the book band (`showsBookBand()`, `readerDrawerShowsBookBand()`), which they cannot share the screen with. Go to % keeps its in-menu Percent pane on C2 and the full-screen selector on C1, and Auto Page Turn its own pane with its key hints. By arithmetic with the Lyra metrics at both UI scales: portrait body 562 px, 165 px or more to spare in the dual-slider panes, keypad cells 97 px or more; landscape body 411 px on those panes, 14 px or more to spare, keypad cells 60 px or more. C1 and every other board keep the sheet. |
| Settings submenus: Left and Right (second audit #2) | C1, C2 | `SettingsActivity::loop()` bound Left and Right to `changeCategory()` whatever was open, and `enterCategory()` clears the open submenu, so in a submenu they threw you out to another category's top-level list while the hint band over them read "Left" and "Right" (the side Up and Down, which step the rows, are not labelled on the touch layouts). `SettingsActivity::frontKeysStepRows()` (a submenu open on the Root view and `boardKeepsKeysBesideTouch()`) now makes the row keys {Up, Left} and {Down, Right}, as the File Browser view already has them, so Left and Right step the rows on release and repeat while held, skips the four `changeCategory()` bindings, and has `render()` label the front keys "Up" and "Down" as the reader menu's sub-lists do (`horizontalFront` asks `!frontKeysStepRows()`). Back still closes the submenu, Confirm on the tab band still switches category, and with no submenu open Left and Right switch the category as before. No row type loses a key route: Settings has no Left/Right value editing, every row (toggle, enum cycle or popup, stepper or picker, string, action, submenu) is operated by Confirm, and the popup and the pickers handle their own keys. Touch is untouched: taps go through `app.route()` first. Other boards: `boardKeepsKeysBesideTouch()` is true only for the profile that sets `TouchConfig::buttonsAlongsideTouch` (Silkscreen), so keys-only boards, X4 Pro, Sticky and Paper Mono keep the old arrays and bindings, and the simulator's helper is false (its Settings script presses Left and Right at the top level only). |
| Night mode while reading (second audit #7, m10) | C1, C2 | The reader menu's More tab lists a Dark Mode row (`ReaderDrawerCatalogItem::NightMode`, last in the tab; `ReaderDrawerAvailability::nightMode`, last in the struct like `lightControls`). It is a toggle row showing `SETTINGS.screenInverted` with the existing `STR_NIGHT_MODE` label. Confirm closes the menu with `EpubReaderMenuAction::TOGGLE_DARK_MODE` (appended to the enum), which `EpubReaderActivity::onReaderMenuConfirm()` runs through `executeReaderQuickAction(LONG_MENU_TOGGLE_DARK_MODE)`, the path the Dark Mode shortcut takes, so the page redraws inverted and the menu is not reopened. The touch tile in the light panel's action bar is unchanged. The tab holds 9 of the catalog's 13 slots with every conditional row on; the Settings tab (12 of 13, `add()` has no bounds check) is not used. Gate: `boardKeepsKeysBesideTouch()`, passed as the availability flag in `EpubReaderDrawerActivity::onEnter()`; `keyNavigation()` was not used because it is also true on every keys-only board and would add the row to the X3/X4 menu. X3, X4, X4 Classic (no touch), X4 Pro, Sticky and Paper Mono (touch only, `hasFrontButtons()` false) and the simulator (the helper is false) keep their menus. No new persisted value, string, timer or wake source. |
| Disable Touchscreen from the EPUB reader (second audit #8, m11) | C1 | `buildControlsTapsGesturesSettingsList()` appends `STR_DISABLE_TOUCHSCREEN` after the gesture submenus, so the reader menu's Settings tab > Controls row (which opens `ControlsOptionsActivity`) reaches it with no tab slot spent; it is the same `disableReaderTouchscreen` setting and toggle as Settings > Reader, and the activity saves on each toggle. Settings > Controls > Taps and Gestures lists it too. Gate: `boardKeepsKeysBesideTouch()` and the row being in `getSettingsList()`, which drops it when `gpio.hasTouch()` is false, so C2 does not list it. Boards without touch, the touch-only boards (X4 Pro, Sticky, Paper Mono) and the simulator keep their list, and the smoke test checks the row follows that gate. |

### Back and one-key lists

`ButtonNavigator` took `std::array<Button, 2>`, so `{Button::Left}` bound as `{Left, Back}` (Back is button 0) and a Back press fired the Left handler too (M2). In the light panel that raised an off light to 5 % and saved it as on, a battery-drain bug. Every list method gained a `std::initializer_list<Button>` overload; the one-key list now stays one key, and a ready-made `std::array` (`getNextButtons()`, Settings' `std::array{up, up}`) keeps the old overload. A list of one key twice (Library's `{Down, Down}`) is unchanged. This is deliberately not gated: it only removes a stray Back reaction, on every board. The same fix reaches Home's cover grid, the folder picker, clipping selection, the interval and on-screen keyboard, and the reader menu's sliders. It has its own CHANGELOG line, the one without the "Silkscreen:" prefix. Checked with the xtensa g++ `-fsyntax-only` against a mirror of the overload set; the host tests have not run.

### Persisted values

All three are appended; none is renumbered, an old settings file loads as before, and a board with no key route to the panel never offers the Light Controls values. **One exception, from the `6fbc97d1` merge:** the first build of the key-access fixes saved Light Controls as 36, and upstream's Home/Reader shortcut took 36 before it landed. Upstream's value stays, Light Controls moved to 37, and a 36 saved by an earlier Silkscreen build loads as Home/Reader. Silkscreen offers Home/Reader with the touch tail fitted or not (`deviceHasFrontButtons(gpio)`, since `6357ec7d`; the gate was `!gpio.hasTouch()`, which reset a 36 on a touchscreen unit to the default), so that setting now runs Home/Reader until it is changed, and Light Controls has to be chosen again. Before `6357ec7d` only a unit without the touch tail kept it as a working Home/Reader.

| Setting | Value | Stored as |
|---|---|---|
| Light Controls on Short Power, Long Power, a side button or a Quick Actions slot | `SHORT_PWRBTN::FRONTLIGHT_PANEL` = **37** (after upstream's `HOME_READER` = 36, which took the value Silkscreen first used; `SHORT_PWRBTN_COUNT` becomes 38) | The existing shortcut keys of `crossink-settings.json`: `shortPwrBtn`, `longPwrBtn`, `sideButtonUpShort`, `sideButtonUpLong`, `sideButtonDownShort`, `sideButtonDownLong` and the `quickActionSlots` entries. `isValidQuickActionSlot()` accepts 37 (and upstream's 36 on Silkscreen only, since `b5df7d65` and its gate: upstream left it out, so a Home/Reader slot is reset to Ignore at the next start, which every other board keeps), and the web settings list it from the shared catalog (`shortcutActionOrder`) where `frontlightPanelFromKeys()` holds. There is no chord value, because `POWER_CHORD_ACTION` must stay aligned with `ButtonShortcutController::ChordAction`, and none for the X4 Pro's Home key, which has its own front-light action. |
| Light Controls on long-press Confirm or long-press Back | `LONG_PRESS_MENU_ACTION::LONG_MENU_FRONTLIGHT_PANEL` = **25** (after `LONG_MENU_LIBRARY` = 24; the count becomes 26) | `longPressMenuAction` and `longPressBackAction` |
| Key hints on touch screens | `showKeyHintsOnTouch`: 0 off, **1 on** (the default) | `"showKeyHintsOnTouch"` in `crossink-settings.json`; a file without it loads as 1 |

Where the board offers no key route, the Light Controls shortcut does nothing (`toggleFrontlightPanelFromShortcut()` returns false), so a file carrying 37 or 25 does no harm on another board. A 36 is upstream's Home/Reader and is live on every board that has its front keys (every keys-only board, and Silkscreen with the touch tail fitted or not; not Sticky or the X4 Pro): a settings file saved by an early Silkscreen build (Light Controls as 36) runs Home/Reader there. `isValidQuickActionSlot()` accepts 37 in a Quick Actions slot whatever the board offers (the validator checks the value, `isQuickActionSlotActionAvailable()` hides what the board does not offer), and 36 where `boardKeepsKeysBesideTouch()` holds (Silkscreen, with the touch tail fitted or not: the flag is profile data, valid at load time and unchanged by the touch probe). On X3, X4, X4 Classic, Sticky and the X4 Pro a 36 in a slot is reset to Ignore at load, as upstream does; the first form of this fix accepted it on every board, which changed the keys-only boards' behaviour (a Home/Reader slot would survive a restart there) and was gated afterwards so no other board changes. Ungating it again is a one-line change if the upstream data loss is wanted fixed on those boards. The simulator smoke test checks that Light Controls is offered only where `frontlightPanelFromKeys()` holds, never as a chord, that Home/Reader is offered exactly where `deviceHasFrontButtons()` holds, that a Quick Actions slot keeps a Light Controls value from the settings file and keeps a Home/Reader value only where `boardKeepsKeysBesideTouch()` holds (the simulator's helper is false, so it expects the reset there), and that Home/Reader is 36 and Light Controls 37.

### Key hints toggle

Settings > Display > Key hints on touch screens (`showKeyHintsOnTouch`, `STR_KEY_HINTS_ON_TOUCH`) exists only where a touch panel and the front keys are both fitted: `getSettingsList()` and the web settings page drop it everywhere else, and in a build without touch `shouldDrawKeyHints()` is always true. On, the touch layouts that otherwise name no keys draw them (the list and the layout rules are under [Defaults chosen](#defaults-chosen)); off, every screen is the touch-only layout it was. Home, Library, Settings and the other screens that already drew the band on Silkscreen are not affected.

### Left out on purpose

- **Optional batch I** (m10, the Night mode toggle while reading, and m11, Disable Touchscreen in the EPUB reader's menu, were done later as the second audit's batch D, #7 and #8):
  - m13: tapping the status bar to hide it, which has no key;
  - m14: jumping Home from a nested screen (a bottom-edge swipe; Back pops one level, and a short Back in a reader goes Home);
  - m15: Quick Actions on long Back, long Confirm and the side keys in the TXT and XTC readers and Home, as on the X4;
  - m19: Reading Stats and Bookmarks & Clippings tabs on the Cover Grid Home;
  - m23: the highlight on two-choice prompts runs against Down (cosmetic, and the default is Cancel);
  - m25: the UI Theme picker drops Classic and RoundedRaff on touch boards, Silkscreen included (a theme choice, not a function).
- **`drawSideButtonHints()`** (`BaseTheme`, `LyraTheme`): the side-button hints stay off the touch layouts. Their only caller is the on-screen keyboard, whose 56 px keys start at about y 430-490 in portrait, under the hints' fixed 345-506 band at the right edge. The other deliberate gaps (IntervalSelection's front step line, the keyboard's "Tips:" block) are under [Defaults chosen](#defaults-chosen).
- **Not verified, from the audit:** the reader menu's layout on C2 (m24: it was a half-height sheet under the hint band, and the second audit's batch B gave it the full-screen layout, which is checked by arithmetic and not yet seen on a unit or in the simulator; the key rows always worked); one reviewer's report that upstream's front Left/Right category switching in Settings clears the open submenu, which the second audit confirmed (`enterCategory()` resets `activeSubmenu`) and its batch C fixed, checked by reading the code and not yet on a unit; the touch orientation (`BoardConfig.h`), on which every touch route depends; and the Up + Down finding, which rests on the ladder tables and not a bench measurement.
- **Not changed:** the KOReader progress screen's action buttons keep upstream's touch gate (the Watch note in the sync record).

### Check on the board

C8 and C9 of the [bring-up checklist](#bring-up-checklist-real-board) cover the light panel and the hint band. Then, with the touch tail fitted (C1) and again without it (C2), reach each of these with the keys alone:
1. The reader menu: every tab, Go to %, Go to Stable Page, the spacing and margin sliders, the Auto Page Turn interval, an SD-card font (it must stick), Reading Stats, Sync Progress and the Nearby rows.
2. Light Controls from a book (the menu row, and each shortcut you set). The light stays dark until you switch it on (Confirm) or raise the brightness; Back leaves the brightness and the on/off state alone; with a side button set to it, the warmth does not move while the button is still down.
3. Home in every theme, Lyra Carousel included (with touch, swipes still work).
4. The Nearby "file exists" prompt, the Reading Stats date editor (hold Back), a dictionary lookup (Right), an OPDS search, the Wi-Fi list while a saved network connects, and the schedule time picker.
5. With Key hints on touch screens on, the screens listed in C9 draw their hints; off, none changes.
6. Home/Reader on a key (second audit, batch A): with the touch tail fitted, Short Power, a side button or a Quick Actions slot set to Home/Reader leaves a nested screen for the open book (or Home with no book open) in one press; a Home/Reader Quick Actions slot is still set after a restart. Start a Library scan: the screen shows "Tap to cancel" and a Cancel hint over Back, and Back cancels it.
7. The reader menu without the touch tail (second audit, batch B), in portrait and held sideways, at the default and Large UI scale: open the menu in a book; Spacing and Page Margins show both sliders and both step hint lines above the key hints; Go to % shows the slider (hold Confirm for the keypad); Auto Page Turn shows its slider and hints; no "Preview unavailable" appears and no preview is drawn. Held sideways, those panes have no book title above the tabs, and Root and Auto Page Turn still do. With the touch tail fitted the menu is the half-height sheet as before.
8. Settings submenus (second audit, batch C), with the touch tail fitted and without it: open Settings > Display > Frontlight, Reader > Font options and Controls > Side Buttons in turn. Left and Right move the highlight along the rows and a held key repeats; the labels over those two keys read "Up" and "Down"; Up and Down still move it; Confirm still toggles, cycles or opens each row; Back closes the submenu and puts the highlight on the row that opened it. Back on the top-level list, then Left and Right switch the category and the labels read "Left" and "Right" again. With the touch tail fitted a tap on a category tab inside a submenu still switches the category.
9. Reader menu rows (second audit, batch D), with the touch tail fitted and without it: in a book, open the menu and go to the More tab; its last row is Dark Mode, with a switch showing the current setting. Confirm closes the menu and the page redraws inverted (or back); open the menu again and the switch matches. With the touch tail fitted, Settings tab > Controls > Taps and Gestures ends with Disable Touchscreen: Confirm flips it (touch stops answering in the book, the keys still work), and it matches Settings > Reader. Without the touch tail the row is not listed. Another board's menus and lists are as before.
10. Label-only gaps (second audit, #18 and #19), with the touch tail fitted and Key hints on touch screens on: open a Wi-Fi password field, type a few characters and hold Up. In portrait the tip lines name Down (back to the keyboard), Left and Right (the cursor) and, for the password, "Hold RIGHT then press [abc]"; hold Right, and they name the switch and Left (back to the cursor). With text in an ordinary field they name hold Up and hold DEL. Held sideways no tip is drawn, and none overlaps the keys or the field in any case. Open the sleep timeout picker (Settings > System > Device) and the Auto Page Turn picker (in a book, with a held key set to Auto Page Turn): "Side buttons: 5 min" (or 5) sits under the controls, Up and Down step by that much, and the line is clear of the Confirm button in both orientations. Open Settings > Reader > Line spacing and Page Margins: in portrait the line sits under the slider with the preview above it; held sideways there is no line and the preview is as before. Word Spacing has no line (both steps are 1). With the setting off, none of these draws a line.

## Battery log

The battery sleep/wake log is its own build, `silkscreen-batlog`: `silkscreen`'s flags plus `-DSILKSCREEN_BATLOG=1` and the version `1.6.1-silkscreen-batlog`, with nothing else added, so boot timing and the ghost-wake path match `silkscreen`. It appends one CSV row per sleep/wake cycle to `/silkscreen-battery/battery.csv` on the card, to find where the charge goes: the cell's volts at sleep and at wake, how the session ended, early re-sleeps, and the time with the light, the radio and the fast clock on. It has no setting, screen, i18n key or Quick Action: flash `silkscreen` to stop. Everything is inside `#if SILKSCREEN_BATLOG`, so every other env compiles to what it did before.

- [BATTERY-LOG.md](BATTERY-LOG.md) says what differs on CrossInk and where the hooks are. The manual (a campaign, retrieving the file, the columns, the analyzer, the bench proof and the DMM recipe) is CrossPoint's `crosspoint-reader/docs/silkscreen/BATTERY-LOG.md`.
- The SDK half (the RTC record, the ADC burst, the sleep tail) is on the shared SDK line, shared with CrossPoint. The SD writer is CrossPoint's file with one line and one include different: keep the two in step. The analyzer stays in CrossPoint.
- The drain rule is item 11 of the [drain checklist](#drain-checklist) and POWER.md BL-01.
- After each sync, re-check the SDK's BatteryLog files and the hooks; `crosspoint-reader/scripts/silkscreen/upstream_status.sh` reports a missing one.

## Build

```bash
pio run -e silkscreen
pio run -e silkscreen -t upload
```

- **Debug env.** `silkscreen-debug` logs at level 2 and shows the build on the sleep screen (`CROSSINK_SHOW_SLEEP_BUILD_INFO`). It waits 250 ms for USB serial on every boot, ghost wakes included, so take battery numbers from `silkscreen`.
- **Battery log env.** `silkscreen-batlog` is `silkscreen`'s flags plus `-DSILKSCREEN_BATLOG=1`, with the version `1.6.1-silkscreen-batlog` (`scripts/git_branch.py`): `pio run -e silkscreen-batlog -t upload`. It adds no serial wait or other flag, so boot timing matches `silkscreen`; flash `silkscreen` to stop logging. See [Battery log](#battery-log).
- **What runs with each build:**
  - before it, the sleep-source guard;
  - after it, CrossInk's `check_firmware_size.py`, `check_app_touch_gate.py` and `rename_firmware.py` (`firmware-silkscreen.bin`). The touch-gate audit returns at once on a touch build, as designed.
- **Version.** Settings shows `1.6.1-silkscreen`. The debug env shows `1.6.1-<branch>+<hash>`, which on this branch reads `1.6.1-silkscreen+<hash>`.
- **Size at the port review:** 5,911,177 B (90.2 % of the 6.25 MB app slot) and 114,992 B of static RAM. After the `3efe61b6` sync and its review fixes: 5,927,761 B and 115,000 B (the image is 5,932,960 B, 90.5 % of the slot). After the key-access fixes: 5,937,405 B and 115,000 B (the image is 5,942,608 B, 90.6 % of the slot). After the `6fbc97d1` sync and its BAT-01 ports: 5,613,605 B and 115,536 B (the image is 5,618,800 B, 85.7 % of the slot, 934,800 B free): 323,800 B less flash, from upstream's perf commits (among them `8ec1ac41`, unused layout data trimmed from the built-in fonts), and 536 B more static RAM.
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

**Check:** these two must print the same commit (today `d5dd6a7e04b50c24028610c8a7e84bdc03c1605e`):

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
| `a9f9ef64` (the merge), `61e220ea`, `8a720bf1`, the review fixes `b2e457f3` and `301b6d63`, and the `docs:` commits after them | `9dd334d3` to `3efe61b6`: 23 commits, 143 files (v1.6.1 and what followed: inline EPUB images, the Library search list, toggle switches, menu buttons that follow the device layout, the KOReader progress cards, a faster sleep entry) | `3d1c39a` to `bf196c8`, the shared line's `read the Li-ion curve in tenths of a percent` (a new function; no other board changes). Upstream's own new pin, `8bbc44c`, is an ancestor of the shared head, so the shared head stayed | Conflicts in `CHANGELOG.md` (the release commit renamed `[Unreleased]` to `[v1.6.1]` and took the Silkscreen lines with it: all 28 are back under a new `[Unreleased]`, 29 with the battery line `8a720bf1` added), `FrontlightPanelActivity.h` (both hooks kept) and `KeyboardEntryActivity.{h,cpp}` (Silkscreen's `preventAutoSleep()` with upstream's `inputPollDelayMs()` and `showTouchFeedback()`). Every other Silkscreen hunk was checked against the merge base's delta and survives, among them the Home carousel's hint gate, the Library hint-band lines (`deviceHasFrontButtons(gpio)` where upstream has `hasTouchHardware()`, in `buildListScreen()` and `render()`), the keyboard's hint-band rectangle, `SettingsActivity`, `SettingsList` and `main.cpp`'s front-light and sleep paths. `DeviceCapabilities.h` holds upstream's `deviceUsesHorizontalSideButtonsForMenus()` beside Silkscreen's helpers, and `SettingsActivity.cpp` includes it once. **Taken as is:** `1162fa43`, menu buttons that follow the device layout: Left and Right switch the reader menu's tabs on the Root pane (on Silkscreen `deviceHasEdgeSideButtons()` is false, so the directions are not swapped), which is how the tabs are reachable by keys. `461b7074`, a silent restart replays the live light state: the replay still goes through `HalFrontlight::begin()`, whose `restoreAllowed()` refusal and no-load memory gate it, so a restart relights only what a user action or Restore Light on Wake had lit. `c3e2d42f`, a faster sleep entry: `APP_STATE.saveToFile()` moves after `goToSleep()` and the 500 ms settle goes; every SD write in `enterDeepSleep()` still comes before `Storage.shutdown()` and Silkscreen's sleep tail is untouched. The Library scan token is an RTC word and an atomic flag, with no SD access before the wake decision. `setup()` consumes it just before the SD mount, once the early re-sleeps (a ghost wake, a release wake, the reset-loop and empty-battery sleeps) are past, so a re-sleep leaves the RTC token for the next real wake: consuming it at the top, as upstream does, made a short Power tap on the sleeping reader cost the next wake a Library rescan. **Watch:** with touch fitted, `KeyboardEntryActivity` polls every 2 ms instead of 10 ms (awake only, bounded by the ceiling); the KOReader progress screen's action buttons keep upstream's touch gate (`resultActionLayout(hasTouch)`), so with touch fitted they reserve no key-hint band and may overlap the hints drawn below them (the same gate as before the merge; check it on the panel, C12, with the key-access fixes). The reader drawer's `buttonDevice` argument (`EpubReaderDrawerActivity::onEnter()`) was upstream's `!mappedInput.hasTouchHardware()`, with no Silkscreen patch at the merge: with touch fitted the drawer left out Reading Stats, Sync Progress, Nearby Position Sync and Send Nearby Book (`EpubReaderMenuModel.h`). The key-access fixes closed that: the drawer's catalog is built with `keyNavigation()` (`!hasTouchHardware() || deviceHasFrontButtons(gpio)`), so Silkscreen lists them. **Also:** `1.6.0` to `1.6.1`, the battery in 1% steps, and the sizes in Build. Builds after the review fixes (`b2e457f3`, `301b6d63`): `silkscreen` 5,927,761 B flash, `x4-pro` 5,914,225 B, `x4-classic` 5,776,406 B, `c3check` 6,350,263 B (188,800 B of the OTA slot free), the sleep-wake guard and the touch-gate audit passing, and the same 7 narrowing warnings as before. |
| `8d0408f7` to `ca6f853e` (the key-access fixes: 23 commits) and the `docs:` commits `b914b585` and the one that records them | Nothing new: still `3efe61b6`, as merged in `a9f9ef64` | Unchanged: the shared head `bf196c8` | Not a sync: the fixes of the key-access audit's batches D, A, B, C, E, F, H and G, written on the merged tree ([Key access](#key-access)). The audit's merge items went as it advised: upstream's `1162fa43` (Left/Right switch the reader menu's tabs) stays unconditional, and the reader menu's `buttonDevice` argument, which the merge left on upstream's `!hasTouchHardware()`, is now `keyNavigation()`. **Cross-board:** `ButtonNavigator`'s `std::initializer_list` overloads (`8d0408f7`) are not gated; everything else is gated for Silkscreen, so the other boards' settings lists, persisted values and behaviour are as before. **Persisted, appended only:** `SHORT_PWRBTN::FRONTLIGHT_PANEL` 36, `LONG_MENU_FRONTLIGHT_PANEL` 25 and `showKeyHintsOnTouch` (default 1). **Watch:** the KOReader progress screen's action buttons (the touch gate noted in the row above), and the items under "Not verified" in Key access, still need a look on the panel (C12). **Build** after `ca6f853e`: `silkscreen` 5,937,405 B flash (the image is 5,942,608 B, 90.6 % of the slot, 610,992 B free) and 115,000 B static RAM, the sleep-wake guard passing and the same 7 narrowing warnings as before; flash is 9,644 B above the `3efe61b6` sync's 5,927,761 B. The final matrix then passed: `x4-pro` 5,923,805 B flash (624,624 B of the slot free), `x4-classic` 5,781,174 B (767,264 B free) and `c3check` 6,354,977 B (184,080 B of the OTA slot free, 4,720 B less than the `3efe61b6` sync's 188,800 B), the sleep-wake guard and the touch-gate audit passing on all of them and the same 7 narrowing warnings. |
| `6e182dfe` (the merge) and `a681d04a`, and the `docs:` commits after them | `3efe61b6` to `6fbc97d1`: 59 commits, 216 files (the Home/Reader shortcut, Library scans of large cards with progress and cancel, the Display status bar with a date, the Library/File Browser swap, EPUB link Back history, the X3 gauge calibration, SdFat's separate FAT cache, cancellable web uploads, and a second X4 Classic display) | `bf196c8` to `7279bae`, the shared line's merge of `aef1a6c` (done in CrossPoint's step). CrossInk's new pin `6993701` is an ancestor of it, so the shared head stayed the answer. What the line brought here: `BatteryMonitor::loadDesignCapacity()` (the incoming X3 calibration needs it), the SSD1677 external fast LUT and the UC8279 chip-detect refactor (not Silkscreen's panels), and the SD stream-transfer error fix | Thirteen conflicted paths, all resolved by hand (the merge commit's message lists them): the `freeink-sdk` gitlink (to the shared head, not `--theirs`) and twelve files, `english.yaml`, `HalPowerManager.cpp`, `CrossPointSettings.h`, `SettingsList.h`, `Activity.h`, `HomeActivity.h`, `FrontlightPanelActivity.{h,cpp}`, `BaseTheme.{h,cpp}`, `MinimalTheme.cpp` and `main.cpp`. **Persisted value:** upstream's `HOME_READER` took 36, so Light Controls moved to 37 ([Persisted values](#persisted-values)); a 36 saved by an earlier Silkscreen build is read as Home/Reader and resets to the setting's default. **Light panel and Home work:** upstream's `25124472` moved `handleFrontlightPanelResult()` into `Activity`, so Silkscreen's `handlesFrontlightPanelAction()` now defaults to true and Settings and the reader menu say false; upstream's `45a78d8f` squeezes the panel's spacing to fit a large UI scale, merged with the action-bar height, the notes and the key hints (the hints drop out when the panel is already squeezed); `553c65fa` moved the header battery into `drawDisplayStatusBar()`, where the "No battery" label already lived (`batteryLabel()` in `drawReaderStatusBar()`); `getBaseSettingsList()` moved to `SettingsList.cpp` (`0427ac5a`) and took the Key hints row with it; `HalPowerManager::startDeepSleep()` takes the X3 calibration wait in CrossInk's own tail only, so `sleepImpl()` is untouched; `8be52383` hides the wake Power hold from mapped input, which does not reach Silkscreen's release-wake check (`gpio.isPressed()`). **Ported in the merge:** CrossPoint's `.gitattributes` line for `scripts/sdfat_patches/*.patch`; in `a681d04a`, BAT-01 for the upload cancel hook (POWER.md, "Not covered" is gone) and for the Library scan's cancel poll. **Drain review:** no new timer, task, wake source, radio start, restart or sleep entry (`rg` of the checklist's patterns finds nothing but the upload hook); the new `delay()`s are 1 ms and 20 ms (the X3 calibration, off Silkscreen); `restoreVisibleFrame()` is a no-op on the shared SDK. **Watch:** the new Home/Reader shortcut is offered only without a touch panel (`QuickActions::isActionAvailable()`, `src/QuickActions.h:112`), so a Silkscreen with the touch tail has no key route to it (fixed by the second key-access audit's batch A, `6357ec7d`); the Library scan screen says "Tap to cancel" with touch on (`LibraryActivity::drawScanScreen()`, `src/activities/library/LibraryActivity.cpp:229`), though Back cancels it (fixed in `e48bfc40`); the interval picker's button preview keeps its `!hasTouchHardware()` gate (`IntervalSelectionActivity.cpp:626`); the front light panel's Sync tile now also appears over the File Browser and the other screens, as a touch tile only. **Builds** at `a681d04a`: `silkscreen` 5,613,605 B flash (the image is 5,618,800 B, 934,800 B free) and 115,536 B static RAM, `x4-pro` 5,601,053 B (947,376 B free), `x4-classic` 5,457,998 B (1,090,448 B free) and `c3check` 6,314,507 B (224,560 B of the OTA slot free, 40,480 B more than the key-access matrix), the sleep-wake guard and the touch-gate audit passing and the same 7 narrowing warnings. **Review of the sync:** one defect here, a stale sentence in [Persisted values](#persisted-values) (36 is now upstream's Home/Reader and live on a board without touch; 37 is the inert value), fixed in the docs only, so no build input changed after `a681d04a` and the matrix above is the final one. The review also found that CrossInk's `/download` and WebDAV GET and PUT run whole-file transfers with no cutoff poll, which CrossPoint's `4821b7c2` closed on its side; it predates the sync, and the port waits for the #3410 fix ([POWER.md](POWER.md), BAT-01, "Not covered"). |
| Only the `docs:` commit that records it; no code | Nothing new: still `6fbc97d1`, as merged in `6e182dfe` | Unchanged: the shared head `7279bae`. CrossPoint's `9ea86717` did not move upstream's pin (`aef1a6c`), and CrossInk's own pin `6993701` is an ancestor of the head | CrossPoint's sync to upstream `9ea86717` (`1b1f5c27`, #3857, skip unused Arduino SDK libraries), recorded in CrossPoint's PORTING.md as `9bf57487`. Only CrossPoint's `platformio.ini` changed, in `[firmware_tuned]` (`CONFIG_BT_ENABLED=n`, 20 more `CONFIG_ARDUINO_SELECTIVE_*=n`, the `esp-zboss-lib` and `esp-zigbee-lib` components dropped) and `[firmware_tuned_c3]` (two NimBLE options dropped). Nothing to port, no CHANGELOG line and no build: CrossInk's `crossink/development` has not taken #3857, so this repo's `platformio.ini` is as before, and even if it does later, only `default`, `debug`, `sticky` and `sticky-debug` inherit `[firmware_tuned]`; `silkscreen`, `silkscreen-debug`, `x4-pro` and `x4-classic` extend `[base]` alone. No CrossInk build input has changed since `a681d04a` (the commits after it touch `CHANGELOG.md`, `PORTING.md` and `POWER.md` only), so the matrix recorded above stands. The CrossPoint marker moved to `9bf57487`. |
| `fea3aca5` to `ce38fac7`, and the `docs:` commits after them | Nothing new: still `6fbc97d1` | `7279bae` to `d5dd6a7`, the shared line's eight battery log commits (fast-forwards: `fea3aca5`, `7cdf99fe`, `ce38fac7`) | Not a sync: the `silkscreen-batlog` env ([Battery log](#battery-log)), the `silkscreen` flags plus `-DSILKSCREEN_BATLOG=1` and the version `-silkscreen-batlog` (`scripts/git_branch.py` lists the env, and `scripts/test_build_identity.py` passes), its own image with the same board tag. It adds `src/util/BatteryLogWriter.{h,cpp}` and hooks in `src/main.cpp`, `lib/hal/HalPowerManager.*`, `HalGPIO.cpp` and `HalClock.cpp`, all inside the blocker, so every other env preprocesses to what it did before; `settingsLoaded` is new in `main.cpp`. CHANGELOG: one "Silkscreen:" line under `[Unreleased]`. **Next syncs:** `build_matrix.sh` builds it, and the battery log lens in CrossPoint's UPSTREAM-SYNC.md (step 7) re-checks the hooks. Drain review (POWER.md BL-01): no timer, task, pin, pull, hold, wake source, radio use or light path is added; the one ADC burst between the sleep decision and `esp_deep_sleep_start` is taken under a 20 ms bounded lock; the release-wake and reset-loop exits call no hook; one SD attempt per verified wake. Before the log (flash / RAM, bytes): `silkscreen` 5613605 / 115536 (7 narrowing warnings; the image is 5618800 B, 934800 B free in the slot). Free RTC slow memory before the log, read from `firmware.map`: 1648 B; the record is 184 B. **Final matrix after the log** (2026-10-04, `build_matrix.sh`, flash / RAM in bytes; the sleep-wake guard passes, the touch-gate audit passes on the non-touch builds and every image fits the slot): `silkscreen` 5613605 / 115536, `x4-pro` 5601053 / 115304, `x4-classic` 5457998 / 112432 and `c3check` 6314507 / 65668, each identical to the byte to the size before the log, so nothing leaks out of the blockers. `silkscreen-batlog`: **5622425 / 116080, +8820 B flash and +544 B RAM over `silkscreen`** (`firmware.bin` 5627616 B against 5618800 B, +8816 B). Free RTC slow memory, read from `firmware.map`: 1464 B on `silkscreen-batlog` (1648 B less the record's 184 B), 1648 B on `silkscreen`, 1728 B on `x4-pro` and 1736 B on `x4-classic`. Free in the 6553600 B OTA slot: `silkscreen` 934800 B, `silkscreen-batlog` 925984 B, `x4-pro` 947376 B, `x4-classic` 1090448 B and `c3check` 224560 B (image 6329040 B, the tightest and unchanged by the log). The only warnings are upstream's narrowing ones (the Quick Lock badge in `src/main.cpp`, `FileBrowserActivity`, `EpubReaderDrawerActivity`), the same as before the log; none is in the battery log's code. |
| Only the `docs:` commit that records it; no code | Nothing new: still `6fbc97d1` (2026-10-01), as merged in `6e182dfe`; after `git fetch crossink` this branch is behind `crossink/development` by 0 | Unchanged: the shared head `d5dd6a7`. CrossPoint's `50823ffa` sync did not move upstream's pin (`aef1a6c`), and CrossInk's own pin `6993701` is an ancestor of the head | CrossPoint's sync to upstream `50823ffa` (`3c7fe2c5`: #3863 the keyboard's tips block, #3864 `goHome()` mapping the plugin catalog, #3865 hint labels centred by text height), recorded in CrossPoint's PORTING.md as `18979516`. Nothing to port, no CHANGELOG line and no merge here: `crossink/development` has not taken the three, and CrossInk has no plugin catalog, no `BaseTheme::drawHintLabel()` and its own keyboard and key hints (`showKeyHintsOnTouch`), so there is nothing to conflict with or to carry over. **Key access:** the CrossPoint merge brings no touch gate and no touch-only control, and this repo's keyboard, hint band and touch gates are as before; the one watch item (CrossPoint's keyboard tips block, left out whole when the touch-sized keys leave it too little height) is in CrossPoint's row, and applies here only if CrossInk's upstream takes #3863. **Drain review:** nothing came in, so nothing to check beyond the battery log: `upstream_status.sh` finds the `silkscreen-batlog` env, the SDK's BatteryLog files, both writers and every hook in this repo and in CrossPoint. **Builds:** no CrossInk build input changed since the battery log's final matrix (built 2026-10-04 16:46-17:07, after its last code commit `ce38fac7`; the only commit after it, `a9c0c46c`, is docs), so no build was needed for this sync and that matrix stands (flash / RAM in bytes): `silkscreen` 5613605 / 115536, `silkscreen-batlog` 5622425 / 116080 (+8820 B flash and +544 B RAM over `silkscreen`), `x4-pro` 5601053 / 115304, `x4-classic` 5457998 / 112432 and `c3check` 6314507 / 65668 (224560 B of the OTA slot free), the sleep-wake guard and the touch-gate audit passing. The sources, the SDK commit and the config are the same, so a matrix run now should print the same figures. The CrossPoint marker moved to `18979516`. |

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
6. **Re-run the [drain checklist](#drain-checklist)** on what the merge brought, and re-check the battery log's hooks (`crosspoint-reader/docs/silkscreen/UPSTREAM-SYNC.md`, step 7; `upstream_status.sh` reports a missing one).
7. **Build** with `scripts/silkscreen/build_matrix.sh` (`--full` adds `silkscreen-debug`, `x4-pro-debug` and the
   Sticky proxy). It builds the `silkscreen` envs from the real config first and the proxy envs after them, because a
   proxy config wipes `.pio/build`. What it covers:
   - `silkscreen`, `silkscreen-batlog` and `silkscreen-debug`, with the guard, the size check and the touch-gate audit;
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
- **The upload cancel hook (upstream #3732):** done in `a681d04a`: `CrossPointWebServerActivity::checkUploadCancellation()` (the callback `setUploadCancelCheck()` takes) also returns true once `lowBatteryCutoffTripped()` has latched, which closes BAT-01's multipart-upload gap.
- **SdFat's second FAT cache (upstream #3685):** arrived with CrossInk's own `a60cf3a5` (`-DUSE_SEPARATE_FAT_CACHE=1` in `[base]`, `scripts/patch_sdfat.py`, two reviewed patches), which the `silkscreen` env takes with the rest of `[base]`. CrossPoint's `.gitattributes` line is in (`scripts/sdfat_patches/*.patch` stays LF on Windows). The SDK's pre-sleep and pre-USB-Drive write-back (`SDCardManager::syncVolume()`, a root `sync()`) reaches both caches: SdFat's `cacheSync()` flushes `m_cache`, `m_fatCache` and the device.

**Keeping up with CrossPoint.** Periodically:

```bash
git fetch crosspoint
git log --oneline 18979516..crosspoint/silkscreen -- . ':!src/activities/testsuite' ':!scripts/check_test_firmware.sh'
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
7. **Blocking transfers:** a new download, upload, scan or sync that blocks the loop polls `powerManager.lowBatteryCutoffTripped()` in its abort or cancel path (BAT-01). `HttpDownloader` users get it for free; the web upload cancel hook and the Library scan's cancel poll carry it since `a681d04a`.
8. **Client traffic:** a new web endpoint that clients poll on their own does not count as traffic (the middleware in `CrossPointWebServer::begin()`).
9. **Input:** a new input path counts as input only when the user meant it. Quick Lock rejections do not (CI-03).
10. **Front light:** a new path that switches the light on goes through `HalFrontlight`, so the probe and the restore refusal apply. Nothing lights it before the wake decision.
11. **Wake sources, timers, tasks:** nothing new runs in sleep or arms a wake (ZD-01). Nothing new runs before the wake decision in `setup()` (WK-01). The battery log's hooks keep this: the release-wake and reset-loop exits call none, the early-ghost exit stores RTC words only, and the one ADC burst before sleep is taken under a 20 ms bound (POWER.md BL-01).
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
| Upstream features that arrive later (plugins, upload cancel, the FAT cache) and bring unbounded radio paths | Procedure above. Upload cancel and the FAT cache have arrived and are handled (`a681d04a`, the SdFat note above). |
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
     - the keyboard (its bottom row above the hints, and a few tip lines above the keys) and clip selection;
     - KOReader sync and the Nearby menus;
     - with Key hints on touch screens on: ClearCache, the Nearby stats, position and transfer screens, the clipping detail, IntervalSelection (Sleep timeout, Auto Page Turn, line spacing, margins), the dictionary alternate-form prompt and Calibre's connection-failed screen draw the band under their buttons; the reader menu's sheet shows its key hint above the tab bar on every tab and sub-list (on two lines where one would be cut off, at the default and the Large UI scale; it keeps its rows); the light panel shows its key hints. With the setting off, none of these changes, and the layouts are the touch-only ones.
   - Controls > Front Buttons lists Remap (x2), Orientation, Long-press behaviour, Long Back and Long Menu, with no `Unexpected controls menu counts` in the log.
   - Remap shows the live mapping, and Quick Actions lists Long Back and Long Menu.
   - On Minimal and Dashboard home, the front keys act as labelled.
   - Every function in [Key access](#key-access) is reached with the keys alone, with the touch tail fitted and without it; the walk is at the end of that section.
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

The key-access fixes added cases that have not run either: `test/reader_drawer_model/ReaderDrawerModelTest.cpp` gained the Light Controls row and the two light-panel predicates (`ReaderDrawerAvailability::lightControls` is its last field, so the positional initialisers there keep their meaning, and the new full-catalog case lists all ten flags), and `src/simulator/SimulatorSmokeTest.cpp` checks the Light Controls shortcut values (offered only where `frontlightPanelFromKeys()` holds, never as a chord, kept by a Quick Actions slot).

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
  - The Library scan (a large Library on this board's PSRAM) blocks `loop()` for as long as the card takes, and only its cancel poll sees the low-battery cutoff, not the awake ceiling or the input cap. It is started by opening Library (or Refresh Library), never in the background; time a scan of a full card on the board (C11).
  - Phantom touch edges, such as a cover rubbing the screen in a bag, count as input and reset every clock; the input cap does not bound them.
  - After more than 60 min on USB with no input, unplugging sleeps the device about 1 s later, once the ladder decodes "on battery". USB Drive and web transfers count for this too.
  - ESP-NOW connectionless power save in the Nearby windows: not tried.
  - The new strings exist in English only, and other languages fall back to English: `STR_POWER_BUTTON_FAULT`, `STR_BATTERY_TOO_LOW_FOR_UPDATE`, `STR_NEARBY_STOPPED_WAITING`, `STR_FRONTLIGHT_CONTROLS`, `STR_FRONTLIGHT_NOT_DETECTED`, `STR_FRONTLIGHT_COOL_ONLY`, `STR_FRONTLIGHT_WARM_ONLY`, `STR_NO_BATTERY`, and the key-access strings `STR_HOLD_BACK_TO_DISCARD`, `STR_CLIP_SHORT`, `STR_TIME_FIELD_PREV`, `STR_TIME_FIELD_NEXT`, `STR_TIME_PICKER_KEYS_HINT`, `STR_KEY_HINTS_ON_TOUCH`, `STR_DRAWER_KEY_HINT_ROOT`, `STR_DRAWER_KEY_HINT_LIST`, `STR_PANEL_KEY_HINT_BRIGHTNESS`, `STR_PANEL_KEY_HINT_BRIGHTNESS_WARMTH` and `STR_PANEL_KEY_HINT_LIGHT`.
- **Before any release channel** (as for CrossPoint, [MODULARITY.md, Future work](MODULARITY.md#future-work)): a board tag per panel family, and an OTA feed for the fork.
- **The battery log's bench A/B** ([BATTERY-LOG.md](BATTERY-LOG.md), and CrossPoint's section 9): the `silkscreen` image against `silkscreen-batlog` on the same board and cell, M1 first. C2's set-up applies.
- **Optional SDK wording:** the `awakeCeilingMin` comment (`BoardConfig.h`) and the SDK README could say that the consumer may extend the ceiling to a longer user timeout. That would be an SDK commit on the shared line, then a gitlink bump in both repos.
