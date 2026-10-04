#pragma once

#include <BoardConfig.h>  // FREEINK_CAP_FRONTLIGHT_PROBE: the class layout depends on it
#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <atomic>
#include <vector>

#include "FrontlightPanelModel.h"
#include "activities/Activity.h"
#include "components/OptionPopup.h"
#include "util/ButtonNavigator.h"

struct Rect;

// Frontlight quick panel (boards with a frontlight + home key, e.g. X4 Pro).
// Opened globally by the top-edge down-swipe, or, on a board with front keys and
// a light (Silkscreen), from Settings > Display > Frontlight: a top-anchored
// drop-down (only the upper third of the screen) with brightness and warmth
// sliders driving the light live, plus an on/off toggle with a frontlight icon.
// It renders as an overlay — the content underneath stays on screen below the
// panel, and a tap there dismisses it. State is persisted once on exit (SPIFFS
// write throttling). Keys: Left/Right brightness, Up/Down warmth (on a board
// with front keys), Confirm on/off, Back closes (as does the Light Controls
// shortcut that opened it, pressed again).
class FrontlightPanelActivity final : public Activity {
  // The sun toggle, two sliders, and four -/+ targets need seven interaction
  // slots. Keep small headroom for the panel's fixed controls.
  using UiApp = freeink::ui::FreeInkApp<20, 7>;

  ButtonNavigator buttonNavigator;

  freeink::ui::GfxRendererTarget uiTarget;  // must precede `app`: the app holds a reference to it
  UiApp app;
  std::atomic<bool> uiReady{false};

  uint8_t brightness = 60;
  uint8_t warmth = 50;
  bool lightOn = false;
  FrontlightPanelContext context;
  FrontlightDrawerState drawerState{};
  OptionPopup optionPopup;
  bool initialInversion = false;
  bool initialTouchscreenDisabled = false;
  bool pendingTouchscreenDisabled = false;
  bool ttfRenderingChanged = false;
  // Swallow the swipe/tap fallout of a slider drag so its release can't
  // trigger the back gesture and close the panel mid-adjustment.
  bool draggingSlider = false;
  // A Confirm press that began while the panel was open. With Long-press Confirm set to
  // Light Controls, holding it closes the panel as it opened it; the press that opened the
  // panel (still held) must not count.
  bool confirmPressedInPanel = false;
  // Up/Down/Left/Right act only once none of them is held. A side button set to Light Controls opens the
  // panel while it is still down, and ButtonNavigator would take that hold for a key held in the panel
  // and step the warmth at once, then every 150 ms until release. Armed from the start on a board
  // without front keys, where nothing opens the panel that way.
  bool navKeysArmed = false;
  // Bottom edge of the drop-down (px). Content lays out above it and the
  // handle occupies its final band. Set by render() before the app lays out.
  int panelBottom = 0;
  bool keyHintsShown = false;
  int16_t panelRowHeight = 0;
  int16_t panelSpaceSm = 0;
  int16_t panelSpaceLg = 0;
  freeink::ui::Rect drawerHandleRect{};
  std::vector<std::string> readerTitleLines;

  static void panelScreen(UiApp::ScreenType& screen, void* user);
  static void onBrightnessEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onWarmthEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onToggleEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onBrightnessStepEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onWarmthStepEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onQuickActionEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onDismissEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildPanelScreen(UiApp::ScreenType& screen);
  void addStepSlider(UiApp::ScreenType& screen, const freeink::ui::Rect& row, uint8_t value,
                     freeink::ui::ActionId sliderAction, freeink::ui::ActionId stepAction);
  Rect homeButtonRect() const;
  // The action bar is touch hits only, so a board with front keys and no working touch panel
  // draws none (actionBarHeight() is then 0); its Sync tile is left out where the screen
  // underneath does not act on the choice. See FrontlightPanelModel.h.
  bool showsActionBar() const;
  bool showsSyncTile() const;
  int actionBarHeight() const;
  // Height of the drop-down, derived from the content it holds (header +
  // sliders + toggle). Same layout math as buildPanelScreen so the frame,
  // content margin, and dismiss threshold all agree.
  int computePanelBottom();
  // Two lines of key hints at the end of the content, for Silkscreen (its profile, with or
  // without a working touch panel) unless Settings > Display > Key hints on touch screens is
  // off. The reader-details panel has no key controls. render() adds keyHintsHeight() to
  // panelBottom and sets keyHintsShown only when the taller panel still fits the screen;
  // buildPanelScreen() draws them from that flag, so the frame and the content agree.
  bool showsKeyHints() const;
  // The brightness hint, with the warmth keys only where the light has a colour to set.
  const char* keyHintBrightnessText() const;
  // One line, or two when the text is wider than the panel at the UI scale in use (it wraps). The
  // brightness and warmth hint is about 481 px at the Large scale, over a 480 px panel.
  int keyHintLineCount(const freeink::ui::ThemeTokens& tokens, const char* text) const;
  int16_t keyHintsHeight(const freeink::ui::ThemeTokens& tokens) const;
  void drawKeyHints(UiApp::ScreenType& screen) const;
  void prepareReaderDetailsLayout();
  void drawReaderDetails(freeink::ui::Screen<20>& screen);
  void adjustBrightness(int delta);
  void adjustWarmth(int delta);
  // Left/Right on a board with front keys: 5 % a step, 1 % below 10 % so the lowest levels
  // can be set. Other boards step 5 %, as before.
  void stepBrightnessByKey(bool up);
  // Up/Down on a board with front keys: warmer/cooler. A warmth that only selects
  // a string (HalFrontlight::warmthIsSelection()) jumps straight to the other one.
  void stepWarmthByKey(bool warmer);
  void toggleLight();
  // FrontlightConfig::loadProbeWindowMs (Silkscreen). After a switch-on or a
  // colour change from this panel, a load probe that found no light leaves the
  // light off, and the panel says so under the brightness row; the probe decides
  // on the SDK's timer, so loop() asks again until it has. One string of a
  // colour-select light is missing (a single-colour light): the warmth row stays
  // on the other string, says so, and ignores input. Without
  // FREEINK_CAP_FRONTLIGHT_PROBE neither can happen.
#if FREEINK_CAP_FRONTLIGHT_PROBE
  void noteSwitchOnResult();
  bool warmthLocked() const;
  // A brightness change switches the light on, except after a switch-on found no
  // light: then the slider and the steps only set the level, so one drag or held
  // key does not run the unloaded probe again on every event, and the light
  // toggle is the retry.
  bool levelSwitchesOn() const { return !noLightNote; }
  // From loop(): take a switch-on's result once it has decided.
  void syncLightState();
  // The line under the warmth row of a single-colour light, or nullptr.
  static const char* singleStringNote();
  // A note under a row: small text wrapped to the panel's content width, at most
  // 3 lines. noteHeight() is the height drawNote() takes, less the gap after it.
  static freeink::ui::TextStyle noteStyle(const freeink::ui::ThemeTokens& tokens);
  int16_t noteHeight(const freeink::ui::ThemeTokens& tokens, const char* text) const;
  void drawNote(UiApp::ScreenType& screen, const char* text) const;
  // The last switch-on found no light: the note stays until the light comes on.
  bool noLightNote = false;
  // A switch-on from this panel has not decided its load yet.
  bool awaitingSwitchOnResult = false;
#else
  void noteSwitchOnResult() {}
  bool warmthLocked() const { return false; }
  bool levelSwitchesOn() const { return true; }
#endif
  void toggleReaderTouchscreen();
  void close();
  void activateQuickAction(int index);
  void openSyncDialog();
  void closeSyncDialog();
  void openReadingStats();
  void openGlobalSettings();
  void drawHeader();
  bool showsBookProgress() const;
  void drawBookProgress();

 public:
  explicit FrontlightPanelActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   FrontlightPanelContext context = {});
  void onEnter() override;
  void onExit() override;
  // Keep the visible sliders and exit-time save in sync with a global edge action.
  void onExternalFrontlightChange() override;
  void loop() override;
  void render(RenderLock&&) override;
  // From an active reader, Home returns to the library; elsewhere it dismisses
  // the overlay back to the current screen.
  bool handleHomeGesture() override;
  // The Light Controls shortcut that opened the panel, pressed again.
  bool closeForLightControlsShortcut() override {
    close();
    return true;
  }
  bool requiresFreshBackdrop() const override { return true; }
  // The panel shows a switch-on's "no light" result itself (noteSwitchOnResult()),
  // except in reader-details mode, which has no light rows: main.cpp's popup
  // shows it there and saves the light as off at once.
  bool showsFrontlightNotice() const override { return !context.showReaderDetails; }
#ifdef SIMULATOR
  freeink::ui::Rect simulatorHandleRect() const { return drawerHandleRect; }
  Rect simulatorSyncOptionRect(const int index) const { return optionPopup.simulatorOptionRect(index); }
  bool simulatorSyncOptionDisabled(const int index) const { return optionPopup.simulatorOptionDisabled(index); }
  int simulatorContentBottom = 0;
  int simulatorActionBarTop = 0;
  void simulatorActivateQuickAction(int index) { activateQuickAction(index); }
#endif
};
