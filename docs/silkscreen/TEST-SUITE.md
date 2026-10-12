# Silkscreen hardware test firmware (CrossPoint's)

CrossInk has no test-suite build. Board bring-up uses CrossPoint's `silkscreen-test` image, and its documentation stays with it: `crosspoint-reader/docs/silkscreen/TEST-SUITE.md`, next to this repo in `silkscreen-firmware/`.

Both readers drive the board through the same freeink-sdk `silkscreen` branch, so a board result from the test image holds for CrossInk too.

The test image runs CrossPoint's app around those SDK managers, not CrossInk's: its boot order, sleep entry, loop, network screens and UI. So it does not cover CrossInk's own paths. Those are the CrossInk image checks in [PORTING.md, Bring-up checklist](PORTING.md#bring-up-checklist-real-board).

## Build and flash

In `crosspoint-reader` (branch `silkscreen`):

```bash
pio run -e silkscreen-test -t upload
```

- **USB only, both ways.** The test image carries the board tag `silkscreen-test`, and both readers carry `silkscreen`.
  - Both readers' OTA and SD firmware update refuse an image with another tag (`src/network/FirmwareBoardTag.cpp`), and the test image has neither.
  - So it goes on over USB and comes off over USB. Come back with `pio run -e silkscreen -t upload` in this repo, or the same command in `crosspoint-reader` for CrossPoint's reader.
- **Switching between the two readers.** They share the `silkscreen` tag, so either reader's SD firmware update accepts the other's image.
- **Build hooks.** The test image takes its `[silkscreen]` hooks (`frontlight_flags`, `display_flags`, `touch_flags`) from `crosspoint-reader/platformio.local.ini`, not from this repo's. Set them the same in both repos for a board that needs them. For the 3.68" variant they are `[silkscreen368]`'s `frontlight_flags` and `display_flags` (there is no `touch_flags`: no touch tail).

## The shared card

- **Writes.** The test image never writes `/.crosspoint`. It writes only `/.silkscreen-test/`.
- **Settings.** It reads CrossPoint's `/.crosspoint/settings.json` once and pins its own settings in RAM. CrossInk keeps its settings in `/.crosspoint/crossink-settings.json`, which the test image does not read.
- **P8 (reader-data integrity).** P8 hashes the files directly in `/.crosspoint` (up to 32) and the names of the folders there. CrossInk adds its own files to that folder. After running either reader, take a new P8 baseline, as CrossPoint's TEST-SUITE.md says for its own reader.

## Bench bring-up (BRING_UP.md)

The bench procedure is `de-link_pcb/docs/audit-2026-09-30/BRING_UP.md`. CrossPoint's TEST-SUITE.md section "Bench bring-up (BRING_UP.md)" maps each of its sections to the tests that serve it, and lists:

- the board options (OPT), which you set before the tests;
- the bounded bench utilities (T4's scope loop, T9's switch run, T10 step 0, P3's power cycles, U1, U2);
- the three dated exceptions.

Run them from the test image as written there. The CrossInk image adds nothing to that list.

### Variant coverage

CrossPoint's TEST-SUITE.md section "Variant coverage" lists each variant the SDK detects and where the test image reports it:

- the display controller;
- the touch tail;
- the RTC;
- the key ladders;
- the power button's idle level;
- the board build (cut-down board, UP(2) as POWER);
- the front-light load and strings;
- the front-light driver.

The detection is SDK code, so the results hold for CrossInk. What CrossInk shows the user for each is in [PORTING.md](PORTING.md#bring-up-checklist-real-board) (C1, C8).

## The 3.68" variant

CrossInk has no test firmware for the 3.68" X3-panel variant either ([PORTING.md, The 3.68" variant](PORTING.md#the-368-variant)). Its bench is CrossPoint's: its `silkscreen-368-test` image, and the section "The 3.68" variant (`silkscreen-368-test`)" of `crosspoint-reader/docs/silkscreen/TEST-SUITE.md`, which lists what each test judges there (T11 and T12 say "no touch in this build"; P1 prints the VER bytes and the driver that ran; T4 has no SSD1677 reference, and T5, T6 and T8 read "not reported" where the X3 drivers report nothing) and a bench list that starts with the power key and a first-power gate and then reads VER. Both readers' variant images share the board tag `silkscreen-368`, so either's SD firmware update accepts the other's; the test image has `silkscreen-368-test`, which neither accepts, so it goes on and comes off over USB, as the 4.26" one does.

Run it in this order:

1. CrossPoint's `silkscreen-368-test` and its bench list: the power-key rework and the first-power gate, then the VER read, and on to the logic-analyser check that nothing talks to the touch addresses.
2. `pio run -e silkscreen-368 -t upload` here, and the C1-C12 steps of [PORTING.md](PORTING.md#bring-up-checklist-real-board) with the differences listed under "On a 3.68" unit".
3. The battery log's A/B with `silkscreen-368-batlog` ([BATTERY-LOG.md](BATTERY-LOG.md)).

A board result from the test image holds for CrossInk's image, as on the 4.26" board: both readers drive the panel through the same SDK drivers. The CrossInk image's own paths (its boot order, sleep entry, Quick Resume, themes) are the C steps.

## Checks that need the reader image

CrossPoint's TEST-SUITE.md lists the paths the test image cannot exercise. They need a reader image, and with CrossInk on the board that means CrossInk's:

- the wake policy;
- Quick Resume;
- the front light across sleep and restarts;
- the SD-card error screen;
- the power-button fault;
- the battery cutoff with a bench supply;
- the web server and updates;
- the reset loop;
- the reader overlay.

On CrossInk they are steps C1-C12 of [PORTING.md, Bring-up checklist](PORTING.md#bring-up-checklist-real-board). Where CrossInk differs:

- **Wake policy.** The rule is CrossInk's NVS mirror of Short Power Button, not CrossPoint's RTC word ([POWER.md WK-01](POWER.md#3-firmware-rules-and-where-they-live)).
- **No plugins.** CrossInk has no plugin system, so CrossPoint's plugin and loan-clock checks do not apply.
- **Reader overlay.** CrossInk has no counterpart of CrossPoint's `xteinkClassPanel()` overlay policy. Check by eye that closing the reader menu leaves the page's gray levels clean on both panels.
- **Not in CrossPoint.** Quick Lock, the Nearby screens, the radio shortcuts and automatic page turns are CrossInk's, and are checked in C6 and C7.
