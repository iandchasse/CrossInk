#pragma once

#include <BoardConfig.h>
#include <CrossInkHalFrontlight.h>
#include <HalGPIO.h>

#include <cstdint>
#include <iterator>

#include "AppCapabilities.h"
#include "CrossPointSettings.h"

// Physical front keys (Back, Confirm, Left, Right) are fitted: every board
// without a working touch panel, and touch boards that keep their keys
// (TouchConfig::buttonsAlongsideTouch, Silkscreen). Key-only UI (the button-hint
// band, key help text, the Front Buttons settings and the remap screen) keys off
// this instead of !hasTouch(). On every other board the two are the same. The
// simulator's HAL has no such query, and no simulated board keeps its keys.
inline bool deviceHasFrontButtons(const HalGPIO& gpio) {
#ifdef SIMULATOR
  return !gpio.hasTouch();
#else
  return gpio.hasFrontButtons();
#endif
}

// deviceHasFrontButtons() from the board profile rather than the touch probe,
// for layout that already follows BoardConfig::hasTouch().
inline bool boardHasFrontButtons() {
#ifdef SIMULATOR
  return !BoardConfig::hasTouch();
#else
  return BoardConfig::hasFrontButtons();
#endif
}

// A touch board that keeps its front keys beside the panel (Silkscreen), from the board profile
// (TouchConfig::buttonsAlongsideTouch): it holds whether or not the touch panel answered, and
// unlike deviceHasFrontButtons() it is false on every keys-only board. Key-route fixes the audit
// found on Silkscreen (hold-to-discard, extra key bindings and hints, input leaks) use this, so
// every other board's keys behave exactly as they did. False in the simulator, whose boards keep
// no keys beside touch.
inline bool boardKeepsKeysBesideTouch() {
#ifdef SIMULATOR
  return false;
#else
  return BoardConfig::hasTouch() && BoardConfig::hasFrontButtons();
#endif
}

// Whether a touch layout draws its key hint band. A board without a touch panel always has the
// band, as before; a touch-only board never does. A touch board that keeps its front keys
// (Silkscreen) draws it unless the user turned off Settings > Display > Key hints on touch
// screens. Only the touch layouts that otherwise show no hints ask this: the screens that always
// drew the band on such a board (Home, Library, Settings, ...) still draw it through the theme,
// which asks deviceHasFrontButtons().
inline bool shouldDrawKeyHints(const HalGPIO& gpio) {
#if CROSSINK_APP_CAP_TOUCH
  return !gpio.hasTouch() || (deviceHasFrontButtons(gpio) && SETTINGS.showKeyHintsOnTouch != 0);
#else
  (void)gpio;
  return true;
#endif
}

// The front-light panel otherwise opens by touch gestures only. A board with a light
// that keeps its front keys (Silkscreen) opens it from the keys: Settings > Display >
// Frontlight, the reader menu's Settings tab, and the Light Controls shortcuts. Decided
// by the board profile, not the touch probe, so a touch panel that fails its probe adds
// nothing on a touch-only board.
inline bool frontlightPanelFromKeys() { return Frontlight.present() && boardHasFrontButtons(); }

inline bool deviceHasEdgeSideButtons(const HalGPIO& gpio) {
#ifdef SIMULATOR
  return gpio.deviceIsX3();
#else
  return gpio.hasEdgeSideButtons();
#endif
}

// Menu navigation follows physical placement without changing the legacy
// layout rules used by sliders, step controls, and side-button hints.
inline bool deviceUsesHorizontalSideButtonsForMenus(const HalGPIO& gpio) {
#if defined(SIMULATOR_DEVICE_X4_PRO) || defined(SIMULATOR_DEVICE_X4_CLASSIC)
  return true;
#else
  return deviceHasEdgeSideButtons(gpio) || gpio.deviceIsX3();
#endif
}

inline bool deviceUsesSideButtonHintGutters(const HalGPIO& gpio) {
  if (!deviceHasEdgeSideButtons(gpio)) return false;
#if CROSSINK_APP_CAP_TOUCH
  return !gpio.hasTouch();
#else
  return true;
#endif
}

// The Up and Down keys sit on one ADC ladder (Silkscreen, IO4): the ladder reads two keys
// pressed together as one of them or as no key, so they can never be held at once. From the
// board profile's key windows (BoardConfig::adcLadders), not a board name. False on every
// board whose keys are not data-driven ADC ladders, and in the simulator, whose boards have
// none (its BoardConfig has no ladder data).
inline bool boardSideKeysShareAdcLadder() {
#ifdef SIMULATOR
  return false;
#else
  if (!BoardConfig::hasAdcLadders()) return false;
  for (const auto& ladder : BoardConfig::ACTIVE.adcLadders.ladders) {
    bool hasUp = false;
    bool hasDown = false;
    for (uint8_t i = 0; i < ladder.count && i < std::size(ladder.keys); ++i) {
      hasUp = hasUp || ladder.keys[i].button == BoardConfig::LadderButton::Up;
      hasDown = hasDown || ladder.keys[i].button == BoardConfig::LadderButton::Down;
    }
    if (hasUp && hasDown) return true;
  }
  return false;
#endif
}

// Touch readers can safely dedicate both side buttons to a chord because Back
// and Confirm remain available on-screen. X4 Classic has the same two edge
// buttons plus a separate four-button front cluster, so it has the same escape
// path without a touchscreen. A board whose side keys share an ADC ladder
// (Silkscreen) cannot press them together, so it offers no chord.
inline bool deviceSupportsSideButtonChord(const HalGPIO& gpio) {
  if (boardSideKeysShareAdcLadder()) return false;
#if CROSSINK_APP_DEVICE_X4CLASSIC || defined(SIMULATOR_DEVICE_X4_CLASSIC)
  return true;
#else
  return gpio.hasTouch();
#endif
}
