#pragma once

#include "activities/Activity.h"

class BackupStatsActivity final : public Activity {
 public:
  explicit BackupStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BackupStats", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  // The backup runs inside loop(), so no later pass finds it working: with
  // PowerConfig::radioOffWhenIdle the result screens take the normal loop delay.
  bool skipLoopDelay() override;
  void render(RenderLock&&) override;

 private:
  enum State { WARNING, SUCCESS, FAILED };

  State state = WARNING;
  char backupFileName[64] = {};

  void goBack() { finish(); }
  void runBackup();
};
