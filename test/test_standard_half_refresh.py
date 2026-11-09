"""Check the production HALF preference routes, persistence and early application."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


header = (ROOT / "src/SettingsList.h").read_text()
catalog = (ROOT / "src/SettingsList.cpp").read_text()
preferences = (ROOT / "src/CrossPointSettings.h").read_text()
persistence = (ROOT / "src/CrossPointSettings.cpp").read_text()
main = (ROOT / "src/main.cpp").read_text()
web = (ROOT / "src/network/CrossPointWebServer.cpp").read_text()
hal = (ROOT / "lib/hal/HalDisplay.cpp").read_text()
activity = (ROOT / "src/activities/settings/SettingsActivity.h").read_text()

setup_display = function(main, "void setupDisplayAndFonts(")
setter = "display.setStandardHalfRefresh(SETTINGS.standardHalfRefresh != 0);"
assert setup_display.index("display.begin(seamless);") < setup_display.index(setter) < setup_display.index("renderer.begin();")
assert setup_display.index(setter) < setup_display.index("activityManager.begin(")
setup = function(main, "void setup()")
assert setup.index("SETTINGS.loadFromFile();") < setup.index("setupDisplayAndFonts(SleepWakePolicy::")
assert setter in function(main, "void loop()")
assert "isWebSettingAvailable(s)" in function(web, "void CrossPointWebServer::handleGetSettings()")
assert "isWebSettingAvailable(s)" in function(web, "void CrossPointWebServer::handlePostSettings()")

descriptor = re.search(r"#if FREEINK_DEVICE_SILKSCREEN && !FREEINK_SILKSCREEN_368\s+"
                       r"add\(SettingInfo::Toggle\(StrId::STR_STANDARD_HALF_REFRESH,.*?#endif", catalog, re.S).group()
# Exercise the initial runtime filter without pulling unrelated font/dictionary catalogs into the fixture.
visibility = function(header, "inline std::vector<SettingInfo> getSettingsList(")
visibility = visibility[:visibility.index("  if (!SETTINGS.shouldTrackReadingStats())")] + "  return v;\n}"
web_gate = function(web, "bool isWebSettingAvailable(")
web_gate = web_gate[:web_gate.index("  if (setting.category ==")] + "  return true;\n}"
group = function(header, "inline std::vector<SettingInfo> buildGroupedDisplaySettingsList(")
capacity = header[header.index("#if CROSSINK_SCALABLE_FONTS\ninline constexpr size_t BASE_SETTINGS_CAPACITY"):
                  header.index("const std::vector<SettingInfo>& getBaseSettingsList();")]
toggle = function(activity, "static SettingInfo Toggle(")
hal_setter = function(hal, "void HalDisplay::setStandardHalfRefresh(")
field = re.search(r"uint8_t standardHalfRefresh\s*=\s*\d+;", preferences).group()
numeric_load = persistence[persistence.index("    const uint8_t fieldDefault =\n"):
                           persistence.index("\n  // Old settings assigned one layout")].rsplit("\n  }", 1)[0]
numeric_save = re.search(r"uint8_t value = this->\*\(info.valuePtr\);.*?doc\[info.key\] = value;", persistence, re.S).group()
clamp = function(persistence, "auto clamp =") + ";"
labels = sorted(set(re.findall(r"StrId::(STR_\w+)", descriptor + visibility + group + toggle)) | {"STR_SENTINEL"})

harness = r"""
#include <ArduinoJson.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
""" + "enum class StrId { " + ", ".join(labels) + " };\n" + r"""
enum class SettingType { TOGGLE, ENUM, VALUE };
enum class SettingAction { DisplaySleepScreen, DisplayFrontlight, DisplayStatusBar, ScreenCalibration };
struct CrossPointSettings {
""" + field + r"""
  uint8_t displayStatusBarTextSize=0;
  bool swap=false;
  bool supportsLibraryFileBrowserSwap() const { return swap; }
  void load(JsonVariantConst doc);
  void save(JsonDocument& doc) const;
} SETTINGS;
struct SettingInfo {
  StrId nameId=StrId::STR_SENTINEL;
  SettingType type=SettingType::TOGGLE;
  uint8_t CrossPointSettings::*valuePtr=nullptr;
  const char* key=nullptr;
  StrId category=StrId::STR_NONE_OPT;
  struct {uint8_t min=0,max=255;} valueRange;
""" + toggle + r"""
  static SettingInfo Submenu(StrId id, SettingAction) { SettingInfo s; s.nameId=id; return s; }
  static SettingInfo Action(StrId id, SettingAction a) { return Submenu(id,a); }
};
namespace BoardConfig {
enum class DisplayController { SSD1677, UC8279, UC8253 };
struct { DisplayController displayController; } ACTIVE;
bool silk=true;
bool isSilkscreen() { return silk; }
}
namespace QuickActions { bool tilt=false; bool supportsTiltPageTurn() {return tilt;} }
struct { bool fitted=true; bool present() {return fitted;} } Frontlight;
struct SdCardFontRegistry {};
struct DictionaryRegistry {};
""" + capacity + r"""
const std::vector<SettingInfo>& getBaseSettingsList() {
  static const auto rows = [] {
    std::vector<SettingInfo> v;
    auto add=[&](SettingInfo s){v.push_back(s);};
""" + descriptor + r"""
    for (auto id : {StrId::STR_REFRESH_FREQ,StrId::STR_READER_DARK_MODE,StrId::STR_UI_THEME,
                    StrId::STR_SWAP_LIBRARY_FILE_BROWSER,StrId::STR_UI_SCALE,StrId::STR_SUNLIGHT_FADING_FIX,
                    StrId::STR_KEY_HINTS_ON_TOUCH}) {
      SettingInfo row; row.nameId=id; v.push_back(row);
    }
    return v;
  }();
  return rows;
}
""" + visibility + web_gate + group + r"""
struct HalDisplay {
  struct { bool enabled=false; void setStandardHalfRefresh(bool on) {enabled=on;} } einkDisplay;
  void setStandardHalfRefresh(bool enabled);
};
""" + hal_setter + r"""
bool isSleepScreenSetting(const SettingInfo&) {return false;}
uint8_t sleepScreenModeToStorage(uint8_t v) {return v;}
uint8_t sleepScreenStorageToMode(uint8_t v) {return v;}
bool isEnumRawValueAllowed(const SettingInfo&,uint8_t) {return true;}
uint8_t defaultEnumRawValue(const SettingInfo&,uint8_t v) {return v;}
constexpr uint8_t SD_FONT_RANGE_NO_EMOJI_LEGACY=250,SD_FONT_RANGE_ALL=0,MIN_READER_FONT_POINT_SIZE=8;
enum FONT_SIZE {FONT_DUMMY};
uint8_t getReaderFontPointSize(FONT_SIZE) {return 10;}
void CrossPointSettings::load(JsonVariantConst doc) {
  bool needsResave=false;
  uint8_t legacySdFontSizeStep=0;
""" + clamp + r"""
  for (const auto& info : getBaseSettingsList()) {
    if (!info.key) continue;
""" + numeric_load + r"""
  }
  (void)needsResave; (void)legacySdFontSizeStep;
}
void CrossPointSettings::save(JsonDocument& doc) const {
  for (const auto& info : getBaseSettingsList()) {
    if (!info.key) continue;
""" + numeric_save + r"""
  }
}
bool hasToggle(const std::vector<SettingInfo>& rows) {
  return std::any_of(rows.begin(),rows.end(),[](const auto& s){return s.valuePtr==&CrossPointSettings::standardHalfRefresh;});
}
int main() {
  constexpr bool offered=FREEINK_DEVICE_SILKSCREEN && !FREEINK_SILKSCREEN_368;
  const size_t common=(CROSSINK_SCALABLE_FONTS?112:111)+(CROSSINK_APP_CAP_TOUCH?4:0);
  for (bool tilt : {false,true}) {
    QuickActions::tilt=tilt;
    assert(getBaseSettingsCapacity()==common+(offered?1:0)+(tilt?2:0));
  }
  using C=BoardConfig::DisplayController;
  for (auto controller : {C::SSD1677,C::UC8279,C::UC8253}) {
    BoardConfig::ACTIVE.displayController=controller;
    const bool visible=offered && controller==C::SSD1677;
    SETTINGS.standardHalfRefresh=1;
    assert(hasToggle(getBaseSettingsList())==offered);
    assert(hasToggle(getSettingsList())==visible);
    assert(SETTINGS.standardHalfRefresh==1);
    for (const auto& row : getBaseSettingsList()) if (row.key) assert(isWebSettingAvailable(row)==visible);
    for (bool light : {false,true}) for (bool swap : {false,true}) {
      Frontlight.fitted=light; SETTINGS.swap=swap;
      auto rows=buildGroupedDisplaySettingsList(getSettingsList());
      assert(hasToggle(rows)==visible);
      assert(rows.size()<=11+(offered?1:0));
    }
    for (const char* json : {"{}","{\"standardHalfRefresh\":0}","{\"standardHalfRefresh\":1}",
                            "{\"standardHalfRefresh\":2}","{\"standardHalfRefresh\":255}",
                            "{\"standardHalfRefresh\":null}","{\"standardHalfRefresh\":\"bad\"}"}) {
      CrossPointSettings restored;
      JsonDocument input; assert(!deserializeJson(input,json));
      restored.load(input.as<JsonVariantConst>());
      const bool enabled=std::string(json)=="{\"standardHalfRefresh\":1}";
      assert(restored.standardHalfRefresh==(offered && enabled ? 1:0));
    }
    CrossPointSettings s; s.standardHalfRefresh=1;
    JsonDocument output; s.save(output);
    assert(output["standardHalfRefresh"].is<uint8_t>()==offered);
    if (offered) assert(output["standardHalfRefresh"].as<uint8_t>()==1);
    for (bool silk : {false,true}) for (bool enabled : {false,true}) {
      BoardConfig::silk=silk;
      HalDisplay d; d.setStandardHalfRefresh(enabled);
      assert(d.einkDisplay.enabled==(enabled && silk && controller==C::SSD1677));
    }
  }
  puts("HALF preference: menu/web eligibility, HAL gate, capacity and JSON persistence passed");
}
"""

json_headers = ROOT / ".pio/libdeps/silkscreen/ArduinoJson/src"
assert (json_headers / "ArduinoJson.h").is_file(), "Use the existing silkscreen build's ArduinoJson headers"
with tempfile.TemporaryDirectory(prefix="crossink-half-") as directory:
    cpp=Path(directory)/"settings.cpp"
    cpp.write_text(harness, encoding="utf-8")
    for silk, small, touch, scalable in ((1,0,0,1),(1,0,1,1),(1,1,0,1),(0,0,0,0),(0,0,1,1)):
        exe=Path(directory)/f"settings-{silk}-{small}-{touch}-{scalable}.exe"
        subprocess.run(["c++","-std=c++20","-Wall","-Wextra","-Werror",
                        f"-DFREEINK_DEVICE_SILKSCREEN={silk}",f"-DFREEINK_SILKSCREEN_368={small}",
                        f"-DCROSSINK_APP_CAP_TOUCH={touch}",f"-DCROSSINK_SCALABLE_FONTS={scalable}",
                        "-I"+str(json_headers),str(cpp),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
print("Saved preference applies before boot/wake rendering and updates in the main loop")
