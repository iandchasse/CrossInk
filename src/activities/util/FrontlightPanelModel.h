#pragma once

#include <cstdint>
#include <memory>
#include <string>

class Activity;

enum class FrontlightPanelAction : uint8_t {
  None = 0,
  SyncProgress,
  NearbyPositionSync,
  SendNearbyBook,
};

enum class FrontlightBookSource : uint8_t { CurrentBook = 0, LastBook, DeviceOnly };

constexpr FrontlightBookSource chooseFrontlightBookSource(const bool activeEpub, const bool currentBookValid,
                                                          const bool lastBookValid) {
  if (activeEpub && currentBookValid) return FrontlightBookSource::CurrentBook;
  if (lastBookValid) return FrontlightBookSource::LastBook;
  return FrontlightBookSource::DeviceOnly;
}

constexpr bool hasFrontlightActiveReaderBook(const bool isReaderActivity, const bool currentBookValid) {
  return isReaderActivity && currentBookValid;
}

constexpr bool shouldShowStickyReaderDetails(const bool hasStickyReaderDetailsPanel, const bool hasFrontlight,
                                             const bool activeReaderBook) {
  return hasStickyReaderDetailsPanel && !hasFrontlight && activeReaderBook;
}

constexpr bool supportsFrontlightDrawer(const bool hasTouchHardware, const bool hasFrontlight,
                                        const bool hasReaderDetailsPanel = false) {
  return hasTouchHardware && (hasFrontlight || hasReaderDetailsPanel);
}

// The panel's action bar (Reading Stats, Sync & Transfer, Night mode, Settings, Touch toggle)
// is touch hits only. A board that keeps its front keys but has no working touch panel
// (Silkscreen without its touch tail) draws no bar rather than icons nothing answers. Every
// other board, touch-only included, draws it as before.
constexpr bool showsFrontlightActionBar(const bool boardHasFrontKeys, const bool touchWorks) {
  return !boardHasFrontKeys || touchWorks;
}

// The Sync & Transfer tile hands its choice back to the screen that opened the panel, and only
// Home and the readers act on it (Activity::handlesFrontlightPanelAction()): over Settings or
// the reader menu a pick would be dropped. A board with front keys (Silkscreen) leaves the tile
// out there; every other board keeps it, as before.
constexpr bool offersFrontlightSyncTile(const bool boardHasFrontKeys, const bool sourceActsOnResult) {
  return !boardHasFrontKeys || sourceActsOnResult;
}

struct FrontlightPanelBookDetails {
  std::string title;
  std::string author;
  std::string chapter;
  uint32_t chapterPage = 0;
  uint32_t chapterPageCount = 0;
  bool chapterPageCountEstimated = false;
  int progressPercent = 0;
};

struct FrontlightDrawerState {
  int8_t selectedAction = 0;
  bool syncDialogOpen = false;
};

struct FrontlightPanelResult {
  FrontlightPanelAction action = FrontlightPanelAction::None;
  FrontlightDrawerState state{};
  bool activeEpub = false;
  bool ttfRenderingChanged = false;
  std::string bookPath;
};

struct FrontlightPanelContext {
  Activity* sourceActivity = nullptr;
  // An open reader of any supported format. This controls reader header chrome
  // and Home navigation; EPUB-only actions remain gated by activeEpub.
  bool activeReaderBook = false;
  bool activeEpub = false;
  bool showReaderDetails = false;
  bool showReadingStatsAction = true;
  std::string bookTitle;
  std::string bookPath;
  FrontlightPanelBookDetails bookDetails;
  std::unique_ptr<Activity> readingStatsActivity;
  FrontlightDrawerState drawerState{};
};
