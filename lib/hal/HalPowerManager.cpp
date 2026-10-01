#include "HalPowerManager.h"

#include <BoardConfig.h>
#include <Logging.h>
#include <PowerManager.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <soc/soc_caps.h>

#include <algorithm>
#include <cassert>

#include "HalGPIO.h"

HalPowerManager powerManager;  // Singleton instance

namespace {
void disableWiFiBeforeDeepSleep() {
  const wifi_mode_t wifiMode = WiFi.getMode();
  if (wifiMode == WIFI_MODE_NULL) {
    return;
  }

  LOG_DBG("PWR", "Disabling WiFi before deep sleep (mode=%d)", static_cast<int>(wifiMode));
  if (wifiMode & WIFI_MODE_AP) {
    WiFi.softAPdisconnect(true);
  }
  if (wifiMode & WIFI_MODE_STA) {
    WiFi.disconnect(true);
  }
  delay(30);
  WiFi.mode(WIFI_OFF);
  delay(30);
}
}  // namespace

void HalPowerManager::begin() {
  if (BoardConfig::ACTIVE.batteryAdc >= 0) {
    pinMode(BoardConfig::ACTIVE.batteryAdc, INPUT);
  }
  normalFreq = getCpuFrequencyMhz();
  modeMutex = xSemaphoreCreateMutex();
  assert(modeMutex != nullptr);
}

void HalPowerManager::setPowerSaving(bool enabled) {
  if (normalFreq <= 0) {
    return;  // invalid state
  }

  if (modeMutex != nullptr) {
    xSemaphoreTake(modeMutex, portMAX_DELAY);
  }

  auto wifiMode = WiFi.getMode();
  if (wifiMode != WIFI_MODE_NULL) {
    // Wifi is active, force disabling power saving
    enabled = false;
  }

  const LockMode mode = currentLockMode;

  if (mode == None && enabled && !isLowPower) {
    LOG_DBG("PWR", "Going to low-power mode");
    if (!setCpuFrequencyMhz(LOW_POWER_FREQ)) {
      LOG_DBG("PWR", "Failed to set CPU frequency = %d MHz", LOW_POWER_FREQ);
      if (modeMutex != nullptr) {
        xSemaphoreGive(modeMutex);
      }
      return;
    }
    isLowPower = true;

  } else if ((!enabled || mode != None) && isLowPower) {
    LOG_DBG("PWR", "Restoring normal CPU frequency");
    if (!setCpuFrequencyMhz(normalFreq)) {
      LOG_DBG("PWR", "Failed to set CPU frequency = %d MHz", normalFreq);
      if (modeMutex != nullptr) {
        xSemaphoreGive(modeMutex);
      }
      return;
    }
    isLowPower = false;
  }

  if (modeMutex != nullptr) {
    xSemaphoreGive(modeMutex);
  }

  // Otherwise, no change needed
}

void HalPowerManager::startDeepSleep(HalGPIO& gpio) const {
  disableWiFiBeforeDeepSleep();

#ifdef ENABLE_SERIAL_LOG
  // Tear down HWCDC so the host sees a clean disconnect and the peripheral
  // doesn't hold power domains that interfere with USB-powered GPIO wake.
  // logSerial is the raw HWCDC reference; Serial is the MySerialImpl proxy
  // (which doesn't expose end()).
  logSerial.end();
#endif

#if !SOC_PM_SUPPORT_EXT1_WAKEUP
  // Release every configured battery latch. BoardConfig owns the pin mapping;
  // the collision guard prevents a stale/mismatched profile from driving a
  // display or SD bus pin low and holding it through sleep.
  for (const int8_t pin : {BoardConfig::ACTIVE.power.latch0, BoardConfig::ACTIVE.power.latch1}) {
    if (pin < 0 || BoardConfig::latchConflictsWithBus(pin)) continue;
    const auto latch = static_cast<gpio_num_t>(pin);
    gpio_set_direction(latch, GPIO_MODE_OUTPUT);
    gpio_set_level(latch, 0);
    gpio_hold_en(latch);
  }
#else
  // Keep configured power latches asserted through deep sleep. The SDK isolates
  // GPIO pads before sleeping, so an unheld latch can float LOW once external
  // power is removed and turn a fast wake into a cold boot. This is deliberately
  // complementary to the C3 path above, where the battery latch must go LOW.
  for (const int8_t pin : {BoardConfig::ACTIVE.power.latch0, BoardConfig::ACTIVE.power.latch1}) {
    if (pin < 0 || BoardConfig::latchConflictsWithBus(pin)) continue;
    const auto latch = static_cast<gpio_num_t>(pin);
    gpio_hold_dis(latch);
    gpio_set_direction(latch, GPIO_MODE_OUTPUT);
    gpio_set_level(latch, 1);
    gpio_hold_en(latch);
  }
#endif

  // Cut the gated peripheral rails (touch/SD/EPD on boards like the Sticky) and
  // hold the enables off through deep sleep — otherwise the GT911 and SD card
  // stay powered all through "off" and drain the battery. No-op on boards with
  // no switched rails (X4/X3). Trade-off: no touch-to-wake; wake is the power
  // button. Must run after display.deepSleep() so the panel controller gets its
  // deep-sleep command while its rail is still up (enterDeepSleep() in main.cpp
  // guarantees that ordering).
  freeink::PowerManager::powerDownRailsForSleep();

  // The SDK convenience helper currently isolates every GPIO after arming the
  // wake source. On the ESP32-C3 that overwrites the power pin's sleep input
  // configuration, so short presses can be missed. Isolate first, then restore
  // and arm the board-configured power pin immediately before sleeping.
  freeink::PowerManager::waitForPowerButtonRelease();
  esp_sleep_config_gpio_isolate();
  freeink::PowerManager::armPowerButtonWakeup();
  gpio_deep_sleep_hold_en();
  esp_deep_sleep_start();
}

bool HalPowerManager::lowBatteryCutoffTripped() {
  if (cutoffTripped) return true;
  const uint16_t cutoffMv = BoardConfig::powerPolicy().lowBatteryCutoffMv;
  if (cutoffMv == 0) return false;
  static const BatteryMonitor cutoffBattery;
  const bool radioOn = WiFi.getMode() != WIFI_MODE_NULL;
  uint16_t mv = 0;
  if (!cutoffBattery.isBelowCutoff(nullptr, &mv, radioOn)) return false;
  cutoffTripped = true;
  LOG_ERR("PWR", "Battery at %u mV%s, below the %u mV cutoff; sleeping", mv, radioOn ? " with Wi-Fi on" : "", cutoffMv);
  return true;
}

unsigned long HalPowerManager::awakeCeilingMs(const unsigned long sleepTimeoutMs) {
  const unsigned long profileMs = BoardConfig::powerPolicy().awakeCeilingMin * 60UL * 1000UL;
  if (profileMs == 0) return 0;
  return std::max(profileMs, sleepTimeoutMs);
}

bool HalPowerManager::awakeCeilingPassed(const unsigned long sleepTimeoutMs) const {
  const unsigned long ceilingMs = awakeCeilingMs(sleepTimeoutMs);
  return ceilingMs > 0 && msSinceInput() >= ceilingMs && msSinceClientTraffic() >= ceilingMs && !gpio.isUsbConnected();
}

bool HalPowerManager::awakeInputCapPassed() const {
  const unsigned long capMs = BoardConfig::powerPolicy().awakeInputCapMin * 60UL * 1000UL;
  return capMs > 0 && msSinceInput() >= capMs && !gpio.isUsbConnected();
}

bool HalPowerManager::backgroundNetworkMustStop(const unsigned long sleepTimeoutMs) {
  return lowBatteryCutoffTripped() || awakeCeilingPassed(sleepTimeoutMs) || awakeInputCapPassed();
}

bool HalPowerManager::radioOffWhenIdle() { return BoardConfig::powerPolicy().radioOffWhenIdle; }

bool HalPowerManager::releaseIdleRadio(const char* tag) {
  if (!radioOffWhenIdle() || WiFi.getMode() == WIFI_MODE_NULL) return false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  LOG_DBG(tag, "Work done; Wi-Fi off");
  return true;
}

bool HalPowerManager::firmwareUpdateBlockedByBattery() const {
  const uint16_t minMv = BoardConfig::powerPolicy().firmwareUpdateMinMv;
  if (minMv == 0) return false;
  const BatteryMonitor battery;
  bool known = false;
  if (battery.isExternalPowerPresent(&known) && known) return false;  // USB carries the write
  const uint16_t mv = battery.readMillivoltsAveraged(16);
  if (mv < BatteryMonitor::CELL_PRESENT_MIN_MV) return false;  // no cell: USB powers the board
  const uint16_t allowanceMv = WiFi.getMode() != WIFI_MODE_NULL ? BoardConfig::powerPolicy().cutoffRadioAllowanceMv : 0;
  if (mv + allowanceMv >= minMv) return false;
  LOG_ERR("PWR", "Battery at %u mV (+%u mV radio allowance), below the %u mV a firmware write needs", mv, allowanceMv,
          minMv);
  return true;
}

bool HalPowerManager::noChargeableCell() const {
  if (!BoardConfig::powerPolicy().noChargeableCellNotice) return false;
  const unsigned long now = millis();
  if (_noCellPolledMs != 0 && now - _noCellPolledMs < BATTERY_POLL_MS) return _noCellCached;
  _noCellPolledMs = now != 0 ? now : 1;
  // The charger's no-battery blink decides by itself: the charger then holds P+
  // between its recharge threshold and 4.2 V, so P+ cannot show the missing cell.
  bool noCell = BatteryMonitor::chargeStatus() == BoardConfig::ChargeState::NoBattery;
  if (noCell) {
    if (!_noCellCached) LOG_INF("PWR", "Charger's no-battery blink: no chargeable cell");
  } else if (gpio.isUsbConnected()) {
    // Set below CELL_PRESENT_MIN_MV, cleared only at NO_CELL_CLEAR_MV, so a flat
    // cell charging up through the threshold does not flip the label.
    static const BatteryMonitor battery;
    const uint16_t mv = battery.readMillivoltsAveraged(4);
    noCell = mv < (_noCellCached ? NO_CELL_CLEAR_MV : BatteryMonitor::CELL_PRESENT_MIN_MV);
    if (noCell && !_noCellCached) LOG_INF("PWR", "External power, battery %u mV: no chargeable cell", mv);
  }
  _noCellCached = noCell;
  return _noCellCached;
}

uint16_t HalPowerManager::getBatteryPercentage() const {
  static const BatteryMonitor battery;
  if (BoardConfig::ACTIVE.batteryGauge.gaugeAddr != 0) {
    const unsigned long now = millis();
    if (_batteryLastPollMs != 0 && (now - _batteryLastPollMs) < BATTERY_POLL_MS) {
      return _batteryCachedPercent;
    }

    _batteryLastPollMs = now;
    uint16_t percent = 0;
    if (!battery.readPercentageChecked(percent)) {
      return _batteryCachedPercent;
    }
    _batteryCachedPercent = percent;
    return _batteryCachedPercent;
  }

  // smooth the battery %.
  if (_batteryCachedPercent == 0) {
    _batteryCachedPercent = 10 * battery.readPercentage();
  } else {
    _batteryCachedPercent = (_batteryCachedPercent * 9 + battery.readPercentage() * 10) / 10;
  }
  return _batteryCachedPercent / 10;
}

#if CROSSINK_BATTERY_DIAG_LOG
bool HalPowerManager::getBatteryDiagnostics(BatteryDiagnostics& out) const {
  // Function-local like getBatteryPercentage()'s: BoardConfig::ACTIVE is only
  // resolved once HalGPIO::begin() has run the X3/X4 probe, so a file-scope
  // instance could be constructed against an unresolved profile.
  static const BatteryMonitor battery;
  const BatteryMonitor::Status status = battery.readStatus();
  if (!status.supported) {
    LOG_ERR("PWR", "Battery diagnostics unsupported on this board");
    return false;
  }
  out.soc = status.percentage;
  out.millivolts = status.millivolts;
  out.charging = status.charging;
  out.socKnown = status.percentageKnown;
  out.millivoltsKnown = status.millivoltsKnown;
  out.chargingKnown = status.chargingKnown;
  return true;
}
#endif

HalPowerManager::Lock::Lock() {
  xSemaphoreTake(powerManager.modeMutex, portMAX_DELAY);
  // Current limitation: only one lock at a time
  if (powerManager.currentLockMode != None) {
    LOG_ERR("PWR", "Lock already held, ignore");
    valid = false;
  } else {
    powerManager.currentLockMode = NormalSpeed;
    valid = true;
  }
  xSemaphoreGive(powerManager.modeMutex);
  if (valid) {
    // Immediately restore normal CPU frequency if currently in low-power mode
    powerManager.setPowerSaving(false);
  }
}

HalPowerManager::Lock::~Lock() {
  xSemaphoreTake(powerManager.modeMutex, portMAX_DELAY);
  if (valid) {
    powerManager.currentLockMode = None;
  }
  xSemaphoreGive(powerManager.modeMutex);
}
