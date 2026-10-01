#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <HalGPIO.h>
#include <Logging.h>
#include <PowerManager.h>
#include <Preferences.h>
#include <SPI.h>
#include <XteinkDetect.h>
#include <esp_sleep.h>

#include <algorithm>

#if FREEINK_MCU_S3
#include <soc/usb_serial_jtag_reg.h>
#endif

#if FREEINK_DEVICE_X4PRO && !ARDUINO_USB_MODE
extern "C" bool tud_mounted(void);
#endif

// Global HalGPIO instance
HalGPIO gpio;

namespace {
constexpr unsigned long BUTTON_DEBOUNCE_REPOLL_MS = 6;
// Poll cadence adapted from Sichroteph/YACP commit
// 6d1f10f4bae52d282a088f9b2e45aaac96da8377 (MIT).
constexpr unsigned long X3_USB_POLL_MS = 1000;

// The X3-vs-X4 fingerprint (freeink::detectXteinkVerdict) only makes sense on
// Xteink hardware; other boards keep their compile-time BoardConfig profile.
#if FREEINK_DEVICE_X4 || FREEINK_DEVICE_X3
constexpr char HW_NAMESPACE[] = "cphw";
constexpr char NVS_KEY_DEV_OVERRIDE[] = "dev_ovr";  // 0=auto, 1=x4, 2=x3
constexpr char NVS_KEY_DEV_CACHED[] = "dev_det";    // 0=unknown, 1=x4, 2=x3

enum class NvsDeviceValue : uint8_t { Unknown = 0, X4 = 1, X3 = 2 };

NvsDeviceValue readNvsDeviceValue(const char* key, NvsDeviceValue defaultValue) {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, true)) {
    return defaultValue;
  }
  const uint8_t raw = prefs.getUChar(key, static_cast<uint8_t>(defaultValue));
  prefs.end();
  if (raw > static_cast<uint8_t>(NvsDeviceValue::X3)) {
    return defaultValue;
  }
  return static_cast<NvsDeviceValue>(raw);
}

void writeNvsDeviceValue(const char* key, NvsDeviceValue value) {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, false)) {
    return;
  }
  prefs.putUChar(key, static_cast<uint8_t>(value));
  prefs.end();
}

HalGPIO::DeviceType nvsToDeviceType(NvsDeviceValue value) {
  return value == NvsDeviceValue::X3 ? HalGPIO::DeviceType::X3 : HalGPIO::DeviceType::X4;
}

HalGPIO::DeviceType detectDeviceTypeWithFingerprint() {
  // Explicit override for recovery/support:
  // 0 = auto, 1 = force X4, 2 = force X3
  const NvsDeviceValue overrideValue = readNvsDeviceValue(NVS_KEY_DEV_OVERRIDE, NvsDeviceValue::Unknown);
  if (overrideValue == NvsDeviceValue::X3 || overrideValue == NvsDeviceValue::X4) {
    LOG_INF("HW", "Device override active: %s", overrideValue == NvsDeviceValue::X3 ? "X3" : "X4");
    return nvsToDeviceType(overrideValue);
  }

  const NvsDeviceValue cachedValue = readNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::Unknown);
  if (cachedValue == NvsDeviceValue::X3 || cachedValue == NvsDeviceValue::X4) {
    LOG_INF("HW", "Using cached device type: %s", cachedValue == NvsDeviceValue::X3 ? "X3" : "X4");
    return nvsToDeviceType(cachedValue);
  }

  // No cache yet: run the SDK's X3 fingerprint probe and persist the result.
  uint8_t score1 = 0;
  uint8_t score2 = 0;
  const freeink::XteinkVerdict verdict = freeink::detectXteinkVerdict(&score1, &score2);
  LOG_INF("HW", "X3 probe scores: pass1=%u pass2=%u", score1, score2);

  switch (verdict) {
    case freeink::XteinkVerdict::X3Confirmed:
      writeNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::X3);
      return HalGPIO::DeviceType::X3;
    case freeink::XteinkVerdict::X4Confirmed:
      writeNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::X4);
      return HalGPIO::DeviceType::X4;
    case freeink::XteinkVerdict::Inconclusive:
      break;
  }

  // Conservative fallback for first boot with inconclusive probes; not cached,
  // so the next boot re-probes.
  return HalGPIO::DeviceType::X4;
}

// --- X3 panel-controller fingerprint (UC8253 vs UC8279) ----------------------
// Newer X3 production units ship a UC8279d panel controller on the same board,
// glass and pins. The SDK probe reads the UC8279's VER/FLG registers over a
// bit-banged half-duplex SPI on the EPD pins; same override/cache scheme as
// the device fingerprint (values reuse NvsDeviceValue: 1 = UC8253, 2 = UC8279).
constexpr char NVS_KEY_EPD_OVERRIDE[] = "epd_ovr";  // 0=auto, 1=uc8253, 2=uc8279
constexpr char NVS_KEY_EPD_CACHED[] = "epd_det";    // 0=unknown, 1=uc8253, 2=uc8279

bool detectX3DisplayIsUc8279() {
  const NvsDeviceValue overrideValue = readNvsDeviceValue(NVS_KEY_EPD_OVERRIDE, NvsDeviceValue::Unknown);
  if (overrideValue != NvsDeviceValue::Unknown) {
    LOG_INF("HW", "EPD controller override active: %s", overrideValue == NvsDeviceValue::X3 ? "UC8279" : "UC8253");
    return overrideValue == NvsDeviceValue::X3;
  }

  const NvsDeviceValue cachedValue = readNvsDeviceValue(NVS_KEY_EPD_CACHED, NvsDeviceValue::Unknown);
  if (cachedValue != NvsDeviceValue::Unknown) {
    LOG_INF("HW", "Using cached EPD controller: %s", cachedValue == NvsDeviceValue::X3 ? "UC8279" : "UC8253");
    return cachedValue == NvsDeviceValue::X3;
  }

  uint8_t ver[5] = {0};
  uint8_t flg = 0;
  const freeink::X3DisplayVerdict verdict = freeink::detectX3DisplayController(ver, &flg);
  LOG_INF("HW", "EPD probe: ver=%02X %02X %02X %02X %02X flg=%02X verdict=%u", ver[0], ver[1], ver[2], ver[3], ver[4],
          flg, static_cast<unsigned>(verdict));
  if (verdict == freeink::X3DisplayVerdict::Uc8279Confirmed) {
    writeNvsDeviceValue(NVS_KEY_EPD_CACHED, NvsDeviceValue::X3);
    return true;
  }
  if (verdict == freeink::X3DisplayVerdict::Uc8253Assumed) {
    writeNvsDeviceValue(NVS_KEY_EPD_CACHED, NvsDeviceValue::X4);
  }
  // Inconclusive: run as UC8253 (the shipping controller) but don't persist,
  // so a flaky first boot gets re-probed.
  return false;
}

#endif  // FREEINK_DEVICE_X4 || FREEINK_DEVICE_X3

#if FREEINK_CAP_ADC_LADDERS
// PowerConfig::ladderPresenceProbe: a key ladder built without its pull-up floats,
// and InputManager has switched it off.
void logLadderPresence() {
  if (!BoardConfig::powerPolicy().ladderPresenceProbe) return;
  const InputManager::LadderPresence p = InputManager::ladderPresence();
  for (uint8_t i = 0; i < 2; ++i) {
    if (p.pin[i] < 0) continue;
    const bool absent = (p.absentMask & (1u << i)) != 0;
    if (p.probed && absent) {
      LOG_INF("HW", "Key ladder IO%d has no pull-up (%u mV pulled down, %u mV pulled up): its keys are off", p.pin[i],
              p.pullDownMv[i], p.pullUpMv[i]);
    } else if (p.probed) {
      LOG_DBG("HW", "Key ladder IO%d fitted (%u mV pulled down, %u mV pulled up)", p.pin[i], p.pullDownMv[i],
              p.pullUpMv[i]);
    } else if (absent) {
      LOG_DBG("HW", "Key ladder IO%d off: no pull-up, found before the sleep", p.pin[i]);
    }
  }
}
#endif

#if FREEINK_CAP_PROBE_IDENTITY
// TouchConfig::requireFocalTechId: this boot's probe refused the device at the
// touch address. If that is a genuine tail whose id the check does not know, it
// is never hibernated and draws its Monitor current in every sleep, so say so.
void logRefusedTouchId(const InputManager& input) {
  if (!input.touchIdRefused()) return;
  LOG_ERR("HW",
          "Device at 0x%02X is not a FocalTech touch controller (chip id 0x%02X); nothing was written to it and it "
          "is not put to sleep",
          BoardConfig::ACTIVE.touch.i2cAddress, input.refusedTouchId());
}
#endif

}  // namespace

void HalGPIO::begin(const bool deferTouch) {
#if FREEINK_DEVICE_X4 || FREEINK_DEVICE_X3
#ifdef FORCE_DEVICE_X3
  _deviceType = DeviceType::X3;
  LOG_INF("HW", "Device override active via build flag: X3");
#else
  _deviceType = detectDeviceTypeWithFingerprint();
#endif
  // The panel-controller probe bit-bangs the EPD pins, so it must run before
  // SPI.begin() attaches them to the SPI matrix. I2C-only device fingerprint
  // above doesn't care about SPI ordering.
  const bool x3IsUc8279 = deviceIsX3() && detectX3DisplayIsUc8279();
  BoardConfig::selectDevice(!deviceIsX3() ? BoardConfig::Board::XteinkX4
                            : x3IsUc8279  ? BoardConfig::Board::XteinkX3Uc8279
                                          : BoardConfig::Board::XteinkX3);

  // CrossInk's X3 override/cache remains authoritative. X4 has no equivalent
  // local override, so use the SDK's factory-aware controller selection before
  // SPI claims the display pins.
  if (deviceIsX4()) {
    freeink::applyXteinkDisplayController();
  }

  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);

  if (deviceIsX4()) {
    pinMode(BAT_GPIO0, INPUT);
    pinMode(UART0_RXD, INPUT);
  }
#endif
  inputMgr.begin(/*startTouch=*/!deferTouch);
#if FREEINK_CAP_ADC_LADDERS
  logLadderPresence();
#endif
#if FREEINK_CAP_PROBE_IDENTITY
  if (!deferTouch) logRefusedTouchId(inputMgr);
#endif
}

void HalGPIO::startTouch() {
  inputMgr.startTouch();
#if FREEINK_CAP_PROBE_IDENTITY
  logRefusedTouchId(inputMgr);
#endif
}

void HalGPIO::checkPowerButtonIdle() {
#if FREEINK_CAP_POWER_POLICY
  if (!BoardConfig::powerPolicy().powerPinIdleCheck) return;
  const auto check = freeink::PowerManager::checkPowerPinIdle();
  switch (check.idle) {
    case freeink::PowerManager::PowerPinIdle::Undefined:
      LOG_ERR("PWR", "Power button IO%d idles at %u mV, between the logic levels (R36 and R72 both fitted?)", check.pin,
              check.mv);
      LOG_ERR("PWR", "Power button off for this session; sleep has no wake source, the reset button starts the board");
      break;
    case freeink::PowerManager::PowerPinIdle::High:
      LOG_INF("PWR", "Power button IO%d held at boot (%u mV)", check.pin, check.mv);
      break;
    case freeink::PowerManager::PowerPinIdle::Low:
      LOG_DBG("PWR", "Power button IO%d idles at %u mV", check.pin, check.mv);
      break;
    case freeink::PowerManager::PowerPinIdle::NotChecked:
      break;
  }
#endif
}

bool HalGPIO::powerButtonFaulted(uint16_t* const mv) const {
#if FREEINK_CAP_POWER_POLICY
  const auto check = freeink::PowerManager::powerPinCheck();
  if (check.idle != freeink::PowerManager::PowerPinIdle::Undefined) return false;
  if (mv) *mv = check.mv;
  return true;
#else
  (void)mv;
  return false;
#endif
}

void HalGPIO::update() {
  inputMgr.update();
  if (inputMgr.isDebouncePending()) {
    // The SDK commits a state change after it remains stable for more than
    // 5 ms. Re-poll before the low-power loop's next ~100 ms sample so short
    // physical button presses are not discarded.
    delay(BUTTON_DEBOUNCE_REPOLL_MS);
    inputMgr.update();
  }
  // Boards with an analog charge-status ladder sample it here, rate-limited by
  // the SDK; every other caller (setup, render task) only reads its cache.
  if (BoardConfig::hasChargeStatusLadder()) {
    BatteryMonitor::pollChargeStatus();
  }

  usbStateChanged = false;
  const unsigned long now = millis();
  if (deviceIsX3() && usbStateSampled && now - lastUsbPollMs < X3_USB_POLL_MS) {
    return;
  }

  const bool connected = isUsbConnected();
  usbStateChanged = (connected != lastUsbConnected);
  lastUsbConnected = connected;
  lastUsbPollMs = now;
  usbStateSampled = true;
}

bool HalGPIO::wasUsbStateChanged() const { return usbStateChanged; }

bool HalGPIO::isPressed(uint8_t buttonIndex) const { return inputMgr.isPressed(buttonIndex); }

bool HalGPIO::wasPressed(uint8_t buttonIndex) const { return inputMgr.wasPressed(buttonIndex); }

bool HalGPIO::wasAnyPressed() const { return inputMgr.wasAnyPressed(); }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const { return inputMgr.wasReleased(buttonIndex); }

bool HalGPIO::wasAnyReleased() const { return inputMgr.wasAnyReleased(); }

unsigned long HalGPIO::getHeldTime() const { return inputMgr.getHeldTime(); }

unsigned long HalGPIO::getPowerButtonHeldTime() const { return inputMgr.getPowerButtonHeldTime(); }

bool HalGPIO::rawInputActive() {
  if (inputMgr.isPowerButtonPhysicallyPressed()) return true;
  // The ladder decoder's own released rule, from one sample per ladder; the
  // oversampled decode stays with the debounced poll.
  if (BoardConfig::hasAdcLadders()) return inputMgr.adcLadderContact();
  InputManager::ButtonAdcSample g1{}, g2{};
  inputMgr.readButtonAdc(g1, g2);
  // ADC ladders idle at the full-scale rail (~4095); Xteink button bands sit below 3900. A decoded key also
  // counts, for ladder keys that read close to the rail.
  constexpr int kIdleRailMin = 4000;
  const auto active = [](const InputManager::ButtonAdcSample& g) {
    return g.button >= 0 || (g.raw >= 0 && g.raw < kIdleRailMin);
  };
  return active(g1) || active(g2);
}

void HalGPIO::idleDelay(const unsigned long ms) {
  // The ladder decoder commits a key only after two clean samples at least 5 ms
  // apart, so a tap shorter than one idle wait could land between two loop
  // passes, or in a single sample, and be lost. Wake the loop on the raw
  // contact instead; its next passes sample the key until it commits. Other
  // boards keep the flat wait, with update()'s debounce re-poll.
  if (!BoardConfig::hasAdcLadders()) {
    delay(ms);
    return;
  }
  constexpr unsigned long IDLE_SLICE_MS = 10;
  const unsigned long idleStart = millis();
  while (millis() - idleStart < ms) {
    delay(IDLE_SLICE_MS);
    if (rawInputActive()) return;
  }
}

#if CROSSINK_APP_CAP_TOUCH
bool HalGPIO::hasTouch() const { return inputMgr.hasTouch(); }

bool HalGPIO::hasFrontButtons() const { return !hasTouch() || BoardConfig::ACTIVE.touch.buttonsAlongsideTouch; }

bool HalGPIO::supportsMultiTouch() const { return inputMgr.supportsMultiTouch(); }

HalGPIO::TouchSnapshot HalGPIO::getTouchSnapshot() const {
  TouchSnapshot result;
  if (!supportsMultiTouch()) return result;

  const auto source = inputMgr.getTouchSnapshot();
  const auto& touch = BoardConfig::ACTIVE.touch;
  const uint16_t width = touch.rawMaxX > touch.rawMinX ? touch.rawMaxX - touch.rawMinX : 1;
  const uint16_t height = touch.rawMaxY > touch.rawMinY ? touch.rawMaxY - touch.rawMinY : 1;
  result.count = std::min<uint8_t>(source.count, TouchSnapshot::MAX_CONTACTS);
  result.reportedCount = source.reportedCount;
  for (uint8_t i = 0; i < result.count; ++i) {
    const auto& point = source.points[i];
    result.contacts[i].id = point.id;
    result.contacts[i].nx = std::clamp(static_cast<float>(point.point.x) / width, 0.0f, 1.0f);
    result.contacts[i].ny = std::clamp(static_cast<float>(point.point.y) / height, 0.0f, 1.0f);
  }
  return result;
}

bool HalGPIO::wasCompletedMultiTouchSwipe(CompletedMultiTouchSwipe& swipe) const {
  if (!supportsMultiTouch()) return false;
  return inputMgr.wasMultiTouchSwipe(swipe.contactCount, swipe.nxStart, swipe.nyStart, swipe.nxEnd, swipe.nyEnd,
                                     swipe.durationMs);
}

bool HalGPIO::wasCompletedMultiTouchRotation(CompletedMultiTouchRotation& rotation) const {
  if (!supportsMultiTouch()) return false;
  return inputMgr.wasMultiTouchRotation(rotation.degrees, rotation.nxCenter, rotation.nyCenter, rotation.durationMs);
}

bool HalGPIO::hasHomeKey() const { return BoardConfig::hasHomeKey(); }

bool HalGPIO::wasHomeKeyPressed() const { return inputMgr.wasHomeKeyPressed(); }

bool HalGPIO::wasHomeKeyTapped() const { return inputMgr.wasHomeKeyTapped(); }

bool HalGPIO::wasHomeKeyLongPressed() const { return inputMgr.wasHomeKeyLongPressed(); }

bool HalGPIO::wasTouchTap(float& nx, float& ny) const { return inputMgr.wasTouchTap(nx, ny); }

bool HalGPIO::wasTouchDown(float& nx, float& ny) const { return inputMgr.wasTouchPressedAt(nx, ny); }

bool HalGPIO::wasTouchReleased() const { return inputMgr.wasTouchReleased(); }

bool HalGPIO::isTouchTapCandidate(float& nx, float& ny, unsigned long& heldMs) const {
  return inputMgr.isTouchTapCandidate(nx, ny, heldMs);
}

bool HalGPIO::wasTouchLongPress(float& nx, float& ny) const { return inputMgr.wasTouchLongPress(nx, ny); }

void HalGPIO::suppressTouchContact() { inputMgr.suppressTouchContact(); }

bool HalGPIO::isTouchHeldAt(float& nx, float& ny) const { return inputMgr.isTouchHeldAt(nx, ny); }

unsigned long HalGPIO::lastTouchHeldMs() const { return inputMgr.lastTouchHeldMs(); }

bool HalGPIO::wasSwipe(float& nxStart, float& nyStart, float& nxEnd, float& nyEnd) const {
  return inputMgr.wasSwipe(nxStart, nyStart, nxEnd, nyEnd);
}

bool HalGPIO::wasTouchActivity() const { return inputMgr.wasTouchActivity(); }
#endif

void HalGPIO::setSharedConfirmPowerShortPressEmitsPower(const bool enabled) {
  InputManager::setSharedConfirmPowerShortPressEmitsPower(enabled);
}

bool HalGPIO::isXteinkDevice() const {
  return BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3Uc8279 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX4 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX4Classic;
}

bool HalGPIO::hasEdgeSideButtons() const {
  return BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX4Pro ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX4Classic;
}

bool HalGPIO::verifyPowerButtonWakeup(const bool shortPressWakes, const uint16_t longHoldMs) {
  // M5Paper v1.1 reaches setup after a normal wheel click has already been
  // released. Its hardware pull-ups make this ghost-wake debounce unnecessary.
  if (BoardConfig::isPaperMono() || BoardConfig::isM5PaperV11() || BoardConfig::ACTIVE.input.power < 0) {
    return true;
  }

  constexpr unsigned long POWER_WAKE_STABILITY_MS = 10;
  const bool heldAtFirstSample = inputMgr.isPowerButtonPhysicallyPressed();
  const unsigned long sampleStart = millis();
  inputMgr.update();
  while (millis() - sampleStart < POWER_WAKE_STABILITY_MS || inputMgr.isDebouncePending()) {
    delay(1);
    inputMgr.update();
  }
  if (shortPressWakes) return true;
  if (!heldAtFirstSample || !inputMgr.isPowerButtonPhysicallyPressed()) return false;

  // Deep sleep wakes as soon as the GPIO changes. Keep the panel and SD card
  // asleep until the held press qualifies as a long Power gesture. millis()
  // starts at reset, so time spent reaching this early boot check counts too.
  while (millis() < longHoldMs) {
    delay(1);
    inputMgr.update();
    if (!inputMgr.isPowerButtonPhysicallyPressed()) return false;
  }
  return true;
}

#if FREEINK_MCU_S3
// USB host presence via the USB-Serial-JTAG peripheral's SOF frame counter: a
// connected host clocks 1 kHz start-of-frame packets, so the counter advancing
// between polls means a data-capable host is attached. Held for a short window
// so multiple callers within one loop tick (update() plus header renders) all
// see the same answer. Limitation: a data-less wall charger sends no SOFs and
// stays invisible — on boards with no VBUS line (X4 Pro, see
// xteink-x4pro-support.md) this is the only observable USB signal.
static bool usbHostSofActive() {
  static uint32_t lastFrame = 0;
  static unsigned long lastAdvanceMs = 0;
  static bool seeded = false;
  if (!seeded) {
    // First probe must not fabricate a connection: getWakeupReason() calls this
    // at boot, and a false positive turns a power-button wake (POWERON reset)
    // into AfterUSBPower, which goes straight back to deep sleep — the device
    // never wakes. Seed the counter and wait one SOF period out; a real host
    // clocks SOFs at 1 kHz, so 3 ms guarantees advancement when attached.
    seeded = true;
    lastFrame = REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG);
    delay(3);
  }
  const uint32_t frame = REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG);
  if (frame != lastFrame) {
    lastFrame = frame;
    lastAdvanceMs = millis();
    return true;
  }
  return lastAdvanceMs != 0 && millis() - lastAdvanceMs < 1500;
}
#endif

bool HalGPIO::isUsbConnected() const {
  if (BoardConfig::hasChargeStatusLadder()) {
    // No VBUS sense: infer USB from the decoded charge-status ladder (charger idle,
    // charging or complete), not isCharging(), which a full cell on a charger
    // reads as false. Until it decodes (about 1.5 s after boot, or about
    // 8.5-9.5 s with the cell already charged, because Complete needs 8 stable
    // samples to rule out the no-battery blink), and during the no-battery blink,
    // keep the last known answer, which starts as "not connected" at boot. Once a
    // decoded state has been lost (readings stuck outside every window), report
    // "not connected", the conservative answer for battery-only rules.
    static const BatteryMonitor ladderBattery;
    bool known = false;
    const bool present = ladderBattery.isExternalPowerPresent(&known);
    if (known) return present;
    return BatteryMonitor::chargeStatusLost() ? false : lastUsbConnected;
  }
#if FREEINK_DEVICE_X3
  if (deviceIsX3()) {
    // X3 has no USB-detect pin; infer external power from the gauge's charge
    // current via the SDK's BatteryMonitor (BQ27220 Current() > 0 = charging).
    static const BatteryMonitor battery;
    return battery.isCharging();
  }
#endif
#if FREEINK_DEVICE_X4PRO && !ARDUINO_USB_MODE
  // X4 Pro uses native TinyUSB for its composite CDC+MSC device. The mounted
  // state is the reliable bus-presence signal for this OTG configuration.
  if (tud_mounted()) return true;
#endif
  if (BoardConfig::ACTIVE.usbDetect >= 0) {
    return digitalRead(BoardConfig::ACTIVE.usbDetect) == HIGH;
  }
#if FREEINK_MCU_S3
  // Without a VBUS pin, prefer native-USB host traffic, then use charging as a
  // fallback for power-only adapters that do not emit USB frames. Charge
  // termination at 100% can still report disconnected.
  if (usbHostSofActive()) return true;
  static const BatteryMonitor battery;
  static bool sampled = false;
  static bool charging = false;
  static unsigned long lastSampleMs = 0;
  const unsigned long now = millis();
  if (!sampled || now - lastSampleMs >= 500) {
    charging = battery.isCharging();
    lastSampleMs = now;
    sampled = true;
  }
  return charging;
#else
  return false;
#endif
}

bool HalGPIO::isUsbConnectedCached() const { return usbStateSampled ? lastUsbConnected : isUsbConnected(); }

bool HalGPIO::coldBootImpliesPowerButton() const {
  // These boards use a button-energized or otherwise known latch topology, so
  // a no-USB POWERON can be trusted as a power-button boot. Unknown/future
  // topologies must continue booting instead of risking an immediate sleep
  // after flashing or when charge detection is unavailable.
  return isXteinkDevice() || BoardConfig::isPaperMono() || BoardConfig::isSticky();
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();

  const bool usbConnected = isUsbConnected();

  if (resetReason == ESP_RST_DEEPSLEEP &&
      (wakeupCause == ESP_SLEEP_WAKEUP_GPIO || wakeupCause == ESP_SLEEP_WAKEUP_EXT1)) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && !usbConnected &&
      coldBootImpliesPowerButton()) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN && usbConnected) {
    return WakeupReason::AfterFlash;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && usbConnected) {
    return WakeupReason::AfterUSBPower;
  }
  return WakeupReason::Other;
}
