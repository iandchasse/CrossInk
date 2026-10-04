#pragma once

#include <Arduino.h>
#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <InputManager.h>
#include <Logging.h>
#include <Wire.h>
#include <freertos/semphr.h>

#include <cassert>

#include "HalGPIO.h"

// Opt-in battery telemetry for diagnosing a miscalibrated fuel gauge. Off in
// every shipping environment; the `debug` PlatformIO env sets it to 1. Defined
// here because this is the lowest layer that consumes it.
#ifndef CROSSINK_BATTERY_DIAG_LOG
#define CROSSINK_BATTERY_DIAG_LOG 0
#endif

class HalPowerManager;
extern HalPowerManager powerManager;  // Singleton

class HalPowerManager {
  int normalFreq = 0;  // MHz
  bool isLowPower = false;
  bool batteryCalibrationPending = false;

  mutable int _batteryCachedPercent = 0;  // Last read battery percentage * 10 (0-1000); callers divide by 10 (ADC/X4
                                          // path only — I2C/X3 path stores 0-100 directly)
  mutable unsigned long _batteryLastPollMs = 0;  // Timestamp of last battery read in milliseconds
  mutable bool _noCellCached = false;            // noChargeableCell()'s last answer
  mutable unsigned long _noCellPolledMs = 0;     // and when it was read, 0 = never

  enum LockMode { None, NormalSpeed };
  LockMode currentLockMode = None;
  SemaphoreHandle_t modeMutex = nullptr;  // Protect access to currentLockMode
  unsigned long lastClientTrafficMs = 0;  // noteClientTraffic()
  unsigned long lastInputMs = 0;          // noteInput()
  bool cutoffTripped = false;             // lowBatteryCutoffTripped() latch

#if FREEINK_CAP_POWER_POLICY
  // The SDK sleep path of startDeepSleep() and startDeepSleepMinimal().
  [[noreturn]] void sleepImpl(HalGPIO& gpio, bool sleepCommands) const;
#endif

 public:
#if defined(BOARD_HAS_PSRAM)
  static constexpr int LOW_POWER_FREQ = 80;  // MHz
#else
  static constexpr int LOW_POWER_FREQ = 10;  // MHz
#endif
  static constexpr unsigned long IDLE_POWER_SAVING_MS = 3000;  // ms
  static constexpr unsigned long BATTERY_POLL_MS = 1500;       // ms

  void begin();

  // Main-loop only, with rendering excluded while the gauge is reconfigured.
  // Returns true while another short calibration step is needed.
  bool updateBatteryCalibration();

  // Control CPU frequency for power saving
  void setPowerSaving(bool enabled);

  // Setup wake up GPIO and enter deep sleep
  // Should be called inside main loop() to handle the currentLockMode
  // Boards with the SDK power policy (FREEINK_CAP_POWER_POLICY, Silkscreen) sleep
  // through the SDK's own path: touch controller asleep, sensor bus released,
  // front light handed over, rails and pins parked, then a bounded power-button
  // release wait that falls back to a wake on the release. Every other board
  // keeps CrossInk's own sleep sequence.
  void startDeepSleep(HalGPIO& gpio) const;
  // Deep sleep without a word to any peripheral (no Wi-Fi teardown, no touch
  // sleep command): for a boot that sleeps before starting them, or that must
  // not rerun code that may have crashed it. Pins and rails are parked exactly
  // as startDeepSleep() does. Boards without the SDK power policy take no such
  // sleep, and get startDeepSleep().
  void startDeepSleepMinimal(HalGPIO& gpio) const;

  // Boot-time sleep decisions (BoardConfig::powerPolicy(), Silkscreen). Each is
  // false, or a no-op, on boards whose profile sets none of these fields.
  //
  // PowerConfig::releaseWaitMaxMs: the last sleep began with the power button
  // still held and woke on its release, which asks for nothing. True once; call
  // first thing in setup(), then sleep again with startDeepSleepMinimal().
  static bool takeReleaseWake();

  // PowerConfig::abnormalResetLimit: counts this boot in the run of abnormal
  // resets (panic, watchdog, brown-out, aborted sleep entry), and logs an aborted
  // last sleep entry. True when the run has reached the limit; the run is then
  // cleared and the caller sleeps at once, before storage, touch, the panel or
  // the radio start. Call once, early in setup().
  static bool abnormalResetLimitReached();
  // From loop(): once this boot has been up HEALTHY_UPTIME_MS, the run of
  // abnormal resets ends.
  static void endAbnormalResetRunWhenHealthy();
  static constexpr unsigned long HEALTHY_UPTIME_MS = 60UL * 1000UL;

  // PowerConfig::lowBatteryCutoffMv: the battery is below the cutoff at boot. A
  // normal boot pays for one averaged reading; only a reading near the cutoff
  // waits for the divider and the charge-status ladder to settle, then takes
  // BatteryMonitor::CUTOFF_SAMPLES readings a second apart. Never true on
  // external power or without a chargeable cell (BatteryMonitor::isBelowCutoff()).
  // Call after begin() and before storage, the panel or the radio start; the
  // caller then sleeps.
  static bool batteryEmptyAtBoot();

  // For a screen that must stay up while external power is present (so the board
  // can still be flashed) and end on the cell: returns once the battery monitor
  // reports no external power, or once undecodedMaxMs have passed while it cannot
  // tell (a charge-status ladder decodes about 1.5 s after boot, or up to about
  // 9.5 s with the cell already charged). From the loop task (it polls the
  // ladder).
  static void waitWhileOnExternalPower(unsigned long undecodedMaxMs);

  // Battery policy (BoardConfig::powerPolicy(), Silkscreen). Every query below
  // is false (or 0) on boards whose profile sets none of these fields, so their
  // callers change nothing there.
  //
  // A network client talked to the device (web server request, WebSocket
  // frame, or an automatic page turn). The awake ceiling (awakeCeilingMs())
  // counts idle time from the later of this and the last input.
  void noteClientTraffic() { lastClientTrafficMs = millis(); }
  unsigned long msSinceClientTraffic() const { return millis() - lastClientTrafficMs; }

  // A key, touch or tilt input the user meant (noteBatteryInput() in main.cpp);
  // resumeAwakeClocks() starts this clock in setup(). The awake ceiling, the input
  // cap and the wake and storage-error timeouts count from it. Kept apart from
  // loop()'s inactivity timer, which screens that hold auto-sleep off reset on
  // every pass.
  void noteInput() { lastInputMs = millis(); }
  unsigned long msSinceInput() const { return millis() - lastInputMs; }

  // A silent restart (SilentRestart.h) continues the session, so these clocks
  // carry across it: carryAwakeClocks() right before the ESP.restart() keeps how
  // long ago the last input and client traffic were, whether the caller's
  // PowerConfig::wakeNoInputTimeoutS wait was still armed, and whether external
  // power was present, in RTC memory. resumeAwakeClocks(), once from setup()
  // after gpio.begin(), starts the clocks: where they were after a silent
  // restart, or now otherwise. After a restart on external power it seeds
  // HalGPIO's USB answer until the charge-status ladder decodes. It returns the
  // carried armed flag (false when nothing was carried). Without the SDK power
  // policy nothing is carried and the clocks just start.
  void carryAwakeClocks(bool wakeNoInputArmed) const;
  bool resumeAwakeClocks();

  // The awake ceiling in ms: the longer of PowerConfig::awakeCeilingMin and the
  // user's sleep timeout (sleepTimeoutMs, 0 = Never), so it bounds Never and the
  // screens that hold auto-sleep off without cutting short a timeout the user
  // chose. 0 on boards without a ceiling.
  static unsigned long awakeCeilingMs(unsigned long sleepTimeoutMs);
  // On battery, no input and no client traffic for awakeCeilingMs() (always
  // false on boards without a ceiling).
  bool awakeCeilingPassed(unsigned long sleepTimeoutMs) const;
  // PowerConfig::awakeInputCapMin (always false on boards without one): on
  // battery, no input for the cap, whatever the client traffic.
  bool awakeInputCapPassed() const;

  // PowerConfig::lowBatteryCutoffMv, the awake ceiling and awakeInputCapMin
  // (always false on boards without them): network work nobody is waiting on
  // must stop or not start. Such work blocks loop(), whose cutoff, ceiling and
  // cap checks cannot run meanwhile; loop() sleeps on them once it returns.
  // sleepTimeoutMs as for awakeCeilingMs(). Call from the loop task.
  bool backgroundNetworkMustStop(unsigned long sleepTimeoutMs);

  // PowerConfig::lowBatteryCutoffMv (always false on boards without one): true
  // once BatteryMonitor::isBelowCutoff() fires (with the radio allowance while
  // Wi-Fi is on), and from then on for the session, so a reading that recovers
  // when a transfer's load stops cannot undo it. At most one reading a second.
  // Call from the loop task, including a blocking transfer's callbacks (which
  // abort on it); loop() then sleeps.
  bool lowBatteryCutoffTripped();

  // PowerConfig::radioOffWhenIdle: a screen whose work has ended stops skipping
  // the loop delay, and a network screen turns Wi-Fi off then instead of on exit.
  static bool radioOffWhenIdle();
  // With radioOffWhenIdle, switch Wi-Fi off now if it is on. Returns true when it
  // did; the caller still restarts on exit to clear the heap the radio left.
  static bool releaseIdleRadio(const char* tag);

  // PowerConfig::lowBatteryCutoffMv (always false on boards without one): on the
  // cell, the last battery reading was at or below the cutoff, or the cutoff has
  // tripped. loop() sleeps once BatteryMonitor::CUTOFF_SAMPLES readings in a row
  // are that low; until then a shortcut must not start Wi-Fi or ESP-NOW, whose
  // load only drains the cell further. At most one reading a second; call from
  // the loop task.
  bool radioStartBlockedByBattery();

  // True when writing a firmware image now could brown out: on battery below
  // PowerConfig::firmwareUpdateMinMv (less PowerConfig::cutoffRadioAllowanceMv
  // while the radio is on). False on boards without the check, on external
  // power, and with no cell fitted.
  bool firmwareUpdateBlockedByBattery() const;

  // Get battery percentage (range 0-100)
  uint16_t getBatteryPercentage() const;

  // PowerConfig::noChargeableCellNotice: the charge-status ladder shows the
  // charger's no-battery blink, or external power runs the board and the battery
  // reads below BatteryMonitor::CELL_PRESENT_MIN_MV (until it reads
  // NO_CELL_CLEAR_MV again), so there is no cell or one too flat to charge;
  // battery displays say that instead of a charge state. Always false on boards
  // without the field. Reads at most once per BATTERY_POLL_MS.
  bool noChargeableCell() const;
  static constexpr uint16_t NO_CELL_CLEAR_MV = BatteryMonitor::CELL_PRESENT_MIN_MV + 100;

  // The charge-status ladder's decoded state (BoardConfig::hasChargeStatusLadder(),
  // Silkscreen), as BatteryMonitor last debounced it from HalGPIO::update(): Unknown
  // until it decodes and once it is lost, and always Unknown on boards without
  // the ladder. Reads no ADC.
  static BoardConfig::ChargeState chargeState() { return BatteryMonitor::chargeStatus(); }

#if CROSSINK_BATTERY_DIAG_LOG
  // Raw battery telemetry for the diagnostic log, kept behind the flag so
  // shipping builds carry neither the struct nor the extra I2C traffic.
  struct BatteryDiagnostics {
    uint16_t soc = 0;         // 0-100, as reported by the backend
    uint16_t millivolts = 0;  // cell voltage; gauge boards report it directly
    bool charging = false;
    // Each field can fail independently, so a valid zero stays distinguishable
    // from an unread one.
    bool socKnown = false;
    bool millivoltsKnown = false;
    bool chargingKnown = false;
  };

  // Samples the battery backend directly, bypassing getBatteryPercentage()'s
  // poll cache and smoothing so every row is a fresh read. Returns false when
  // the active board reports no battery telemetry at all.
  bool getBatteryDiagnostics(BatteryDiagnostics& out) const;
#endif

  // RAII helper class to manage power saving locks
  // Usage: create an instance of Lock in a scope to disable power saving, for example when running a task that needs
  // full performance. When the Lock instance is destroyed (goes out of scope), power saving will be re-enabled.
  class Lock {
    friend class HalPowerManager;
    bool valid = false;

   public:
    explicit Lock();
    ~Lock();

    // Non-copyable and non-movable
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    Lock(Lock&&) = delete;
    Lock& operator=(Lock&&) = delete;
  };
};
