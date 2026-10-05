#pragma once

#include <I18n.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "util/ButtonNavigator.h"

class XtcReaderMenuActivity final : public Activity {
 public:
  enum class MenuAction {
    SELECT_CHAPTER,
    READING_STATS,
    TOGGLE_BOOK_STATS_TRACKING,
    TOGGLE_COMPLETED,
    DELETE_STATS,
    DELETE_CACHE,
    SEND_NEARBY_BOOK,
    DISABLE_TOUCHSCREEN,
    LIGHT_CONTROLS,
  };

  XtcReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title, bool hasChapters,
                        bool isBookCompleted, bool globalStatsEnabled, bool bookStatsEnabled);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool allowPowerAsConfirmInReaderMode() const override { return true; }
  bool allowGlobalHomeGesture() const override { return false; }
  // A Sync & Transfer pick would leave the book from under this menu, so the panel opened from the Light Controls
  // row leaves that tile out on a board that keeps its front keys (a Silkscreen), as over the EPUB menu.
  bool handlesFrontlightPanelAction() const override { return false; }
  void onFrontlightPanelClosed() override;

 private:
  // Every row, the touch toggle and the Light Controls row on: nine. One interaction per row, and one spare.
  static constexpr size_t kMaxMenuItems = 9;
  using UiHost = UiAppHost<kMaxMenuItems + 1, 2>;
  using UiApp = UiHost::App;

  struct MenuItem {
    MenuAction action;
    StrId labelId;
  };

  static std::vector<MenuItem> buildMenuItems(bool hasChapters, bool isBookCompleted, bool hasTouch,
                                              bool globalStatsEnabled, bool bookStatsEnabled, bool lightControls);
  void finishCancelled();

  ButtonNavigator buttonNavigator;
  std::string title;
  std::vector<MenuItem> items;
  bool bookStatsEnabled = true;
  std::array<freeink::ui::ListItem, kMaxMenuItems> listItems{};
  int selectedIndex = 0;
  UiHost ui;
  int visibleRows = 1;
  int topIndex = 0;
  int listHeaderHeight = 0;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  void refreshListItems();
  void selectCurrent();
};
