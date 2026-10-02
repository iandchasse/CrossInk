#include "UsbDriveActivity.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <SdCardFontSystem.h>
#ifndef SIMULATOR
#include <BoardConfig.h>
#include <HalPowerManager.h>
#endif

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "components/CompactHeader.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "platform/UsbSerialJtagHandoff.h"

void UsbDriveActivity::onEnter() {
  Activity::onEnter();
  state = State::Unsupported;
  preparing = true;
  startFailed = false;
  restartRequested = false;
  forcedDisconnectRequested = false;
  hostSuspendPending = false;
  hostWaitStartedAt = 0;
  startFailureStartedAt = 0;
  forcedDisconnectRequestedAt = 0;
  hostSuspendStartedAt = 0;
  sawExternalPower = false;
  onBatterySince = 0;

  // Paint the instruction screen before detaching the filesystem and exposing
  // its block device to the host. The two operations must never overlap.
  requestUpdateAndWait();
#ifndef SIMULATOR
  // The host can replace fonts without going through firmware file APIs.
  sdFontSystem.markRegistryDirty();
  if (!Storage.beginUsbDrive()) {
    LOG_ERR("USB", "Unable to start USB Drive");
    preparing = false;
    startFailed = true;
    state = State::IoError;
    startFailureStartedAt = millis();
    requestUpdate();
    return;
  }

#endif
  preparing = false;
  state = State::WaitingForHost;
  hostWaitStartedAt = millis();
  requestUpdate();
}

void UsbDriveActivity::onExit() {
  library::invalidateLibraryIndex();
#ifndef SIMULATOR
  if (!restartRequested) Storage.endUsbDrive();
#endif
  Activity::onExit();
}

void UsbDriveActivity::loop() {
#ifndef SIMULATOR
  if (!startFailed) {
    const auto storageState = Storage.usbDriveState();
    const State nextState = static_cast<State>(storageState);
    if (nextState != state) {
      const bool messageChanged = state != State::Connected || nextState != State::Accessed;
      state = nextState;
      if (messageChanged) requestUpdate();
    }
  }
#endif

// The simulator HAL does not expose TinyUSB's host-suspend signal.
#if !defined(SIMULATOR)
  // In every state: with the cable gone (or on the battery limits that loop()
  // sleeps on) nothing can use the session, and this loop cannot sleep while
  // the host holds the card. The restart lands where those limits sleep.
  if (cablePulledOnBattery() || batteryLimitReached()) {
    restartToHome();
    return;
  }
  // A host using the drive is client traffic for the battery awake ceiling, so
  // pulling the cable after a long copy does not sleep the restart at once. The
  // input cap still counts keys only.
  if (state == State::Connected || state == State::Accessed) powerManager.noteClientTraffic();

  // Cable removal can leave a battery-powered USB device mounted but
  // suspended. A sleeping host intentionally follows the same timeout policy.
  if ((state == State::Connected || state == State::Accessed) && Storage.usbDriveHostSuspended()) {
    if (!hostSuspendPending) {
      hostSuspendPending = true;
      hostSuspendStartedAt = millis();
    } else if (millis() - hostSuspendStartedAt >= HOST_SUSPEND_TIMEOUT_MS) {
      LOG_INF("USB", "USB Drive host suspend timed out; ending session");
      restartToHome();
      return;
    }
  } else {
    hostSuspendPending = false;
  }
#endif

  if (state == State::WaitingForHost && millis() - hostWaitStartedAt >= HOST_WAIT_TIMEOUT_MS) {
    LOG_INF("USB", "USB Drive host wait timed out");
    restartToHome();
    return;
  }

  if (startFailed && millis() - startFailureStartedAt >= START_FAILURE_TIMEOUT_MS) {
    LOG_INF("USB", "USB Drive startup failure timed out");
    restartToHome();
    return;
  }

  if (forcedDisconnectRequested) {
    if (millis() - forcedDisconnectRequestedAt >= FORCED_DISCONNECT_TIMEOUT_MS) {
      LOG_ERR("USB", "USB Drive host disconnect grace period ended; forcing restart");
      restartToHome();
    }
    return;
  }

  if (!startFailed && state == State::IoError) {
    forcedDisconnectRequested = true;
    forcedDisconnectRequestedAt = millis();
    LOG_ERR("USB", "USB Drive I/O error; disconnecting host");
#ifndef SIMULATOR
    if (!Storage.disconnectUsbDriveHost()) {
      LOG_ERR("USB", "Unable to request USB Drive host disconnect");
    }
#endif
    return;
  }

  const bool canExitWithInput = state == State::WaitingForHost || state == State::IoError;
  if (canExitWithInput) {
    if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
        mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Power) || mappedInput.wasHomeGesture()) {
      restartToHome();
      return;
    }
    if (state == State::WaitingForHost) return;
  }

  if (state == State::Ejected || state == State::Disconnected || state == State::Unsupported) {
    restartToHome();
  }
}

void UsbDriveActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const char* title = tr(STR_USB_DRIVE);
  const bool canExitWithInput = state == State::WaitingForHost || state == State::IoError;
  if (mappedInput.hasTouchHardware() && canExitWithInput) {
    TouchHeaderBackButton::drawCompact(renderer, title);
  } else {
    CompactHeader::drawTitle(renderer, title);
  }

  if (preparing) {
    renderMessage(tr(STR_USB_DRIVE_PREPARING), tr(STR_USB_DRIVE_EJECT_HINT));
  } else if (forcedDisconnectRequested) {
    renderMessage(tr(STR_USB_DRIVE_ERROR));
  } else
    switch (state) {
      case State::WaitingForHost:
        renderMessage(tr(STR_USB_DRIVE_WAITING));
        break;
      case State::Connected:
      case State::Accessed:
        renderMessage(tr(STR_USB_DRIVE_CONNECTED), tr(STR_USB_DRIVE_EJECT_HINT));
        break;
      case State::IoError:
        renderMessage(startFailed ? tr(STR_USB_DRIVE_START_ERROR) : tr(STR_USB_DRIVE_ERROR));
        break;
      case State::Ejected:
      case State::Disconnected:
      case State::Unsupported:
        break;
    }

  if (state == State::WaitingForHost || state == State::IoError) {
    const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer(screenTransitionRefresh.modeFor(0));
}

void UsbDriveActivity::renderMessage(const char* message, const char* detail) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect textArea{metrics.contentSidePadding, 0, renderer.getScreenWidth() - metrics.contentSidePadding * 2,
                      renderer.getScreenHeight()};
  int y = renderer.getScreenHeight() / 2 - renderer.getLineHeight(UI_10_FONT_ID);
  y += UITheme::drawCenteredWrappedText(renderer, textArea, UI_10_FONT_ID, y, message, 2, true, EpdFontFamily::BOLD);
  if (detail) {
    y += metrics.verticalSpacing;
    UITheme::drawCenteredWrappedText(renderer, textArea, UI_10_FONT_ID, y, detail, 3);
  }
}

#ifndef SIMULATOR
// There is no VBUS sense, and TinyUSB may never see a detach on battery, so a
// board with an analog charge-status ladder watches the cable through it: once
// it has shown external power this session (the ladder samples at 1 Hz and
// debounces), a steady "on battery" means the cable left. False on boards
// without the ladder.
bool UsbDriveActivity::cablePulledOnBattery() {
  if (!BoardConfig::hasChargeStatusLadder()) return false;
  using BoardConfig::ChargeState;
  const ChargeState charge = HalPowerManager::chargeState();
  if (charge == ChargeState::ExternalIdle || charge == ChargeState::Charging || charge == ChargeState::Complete) {
    sawExternalPower = true;
  }
  if (!sawExternalPower || charge != ChargeState::OnBattery) {
    onBatterySince = 0;
    return false;
  }
  if (onBatterySince == 0) onBatterySince = millis() | 1UL;
  if (millis() - onBatterySince < CABLE_PULLED_TIMEOUT_MS) return false;
  LOG_INF("USB", "USB cable gone (charge status on battery); ending USB Drive session");
  return true;
}

// The battery limits main.cpp's sleepOnBatteryAwakeLimits() sleeps on: the
// low-battery cutoff, the awake ceiling and the input cap (BoardConfig::
// powerPolicy(); never on boards without them, and never on external power).
bool UsbDriveActivity::batteryLimitReached() const {
  if (powerManager.lowBatteryCutoffTripped()) return true;  // logs the reading
  const unsigned long sleepTimeoutMs = SETTINGS.getSleepTimeoutMs();
  if (!powerManager.awakeCeilingPassed(sleepTimeoutMs) && !powerManager.awakeInputCapPassed()) return false;
  LOG_INF("USB", "Awake limit on battery reached; ending USB Drive session");
  return true;
}
#endif

void UsbDriveActivity::restartToHome() {
  if (restartRequested) return;
  restartRequested = true;
#ifndef SIMULATOR
  Storage.endUsbDrive();
#endif
#ifdef SIMULATOR
  activityManager.goHome();
#else
  delay(20);
  handoffUsbOtgToSerialJtag();
  restartToHomeAfterStorageHandoff();
#endif
}
