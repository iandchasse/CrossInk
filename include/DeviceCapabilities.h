#pragma once

#include <BoardConfig.h>
#include <CrossInkHalFrontlight.h>
#include <HalGPIO.h>

#include "AppCapabilities.h"

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

// Touch readers can safely dedicate both side buttons to a chord because Back
// and Confirm remain available on-screen. X4 Classic has the same two edge
// buttons plus a separate four-button front cluster, so it has the same escape
// path without a touchscreen.
inline bool deviceSupportsSideButtonChord(const HalGPIO& gpio) {
#if CROSSINK_APP_DEVICE_X4CLASSIC || defined(SIMULATOR_DEVICE_X4_CLASSIC)
  return true;
#else
  return gpio.hasTouch();
#endif
}
