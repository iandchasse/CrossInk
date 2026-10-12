#!/usr/bin/env bash
# Silkscreen build matrix for CrossInk (docs/silkscreen/PORTING.md, "Upstream merges"; the whole
# multi-repo procedure is crosspoint-reader/docs/silkscreen/UPSTREAM-SYNC.md).
#
# Usage: scripts/silkscreen/build_matrix.sh [--full] [--label NAME] [env ...]
#   no envs : silkscreen silkscreen-batlog silkscreen-368 silkscreen-368-batlog x4-pro x4-classic c3check
#   --full  : adds silkscreen-debug, x4-pro-debug and stickycheck
# Prints one line per env (exit code, flash/RAM bytes, warning counts, log path) and exits
# non-zero if any env or check failed. Logs go to .cache/silkscreen-builds/ (gitignored).
#
# What builds on the Windows bench host, and how:
#   silkscreen*  : the real platformio.ini (these envs pin pioarduino 55.03.311 per env;
#       silkscreen-batlog is the release image plus the battery log, -DSILKSCREEN_BATLOG=1; the
#       silkscreen-368* envs are the 3.68" X3-panel variant's, docs/silkscreen/PORTING.md). Each
#       silkscreen-368* image goes through scripts/silkscreen/check_no_touch.sh before the proxy
#       envs start: it reads .pio/build/<env>, which they delete. The same audit then runs on the
#       4.26" silkscreen image, which it must fail on the touch names, the SSD1677 and probe names,
#       the probe's size and the missing UC8279d driver: the control that its patterns still match a
#       real touch build and a build with other panels' code (skipped when that image is not on disk).
#   x4-pro*, x4-classic* : CrossInk's real envs through a proxy config whose only change is
#       [base]'s platform, 55.03.37 -> the installed 55.03.311. 55.03.37 needs the Arduino 3.3.7
#       framework package; installing it would replace the framework copy CrossPoint builds with.
#   c3check, stickycheck : default's / sticky's flags and sources on [base]. The real default,
#       debug, sticky and sticky-debug extend [firmware_tuned], whose custom sdkconfig rebuild
#       cannot run on this host (pioarduino tool-scons vs PlatformIO Core).
# Every proxy env changes PlatformIO's project checksum, which wipes ALL of .pio/build, so the
# silkscreen envs (real config) build first and the proxy envs after them.
# Never run this while the user is building or flashing from this checkout; it refuses to start while any pio
# process runs (exit 3).
set -u
SELF="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
cd "$(dirname "$0")/../.." || exit 2
REPO=$(pwd)
# Refuse to start while any PlatformIO run is going: the user's own build or upload, or a build an earlier job left
# behind (on Windows, stopping a shell can leave its pio children running). Two runs in one checkout corrupt
# .pio/build, and the proxy envs below delete it outright.
if tasklist //FI "IMAGENAME eq pio.exe" 2>/dev/null | grep -qi "pio.exe"; then
  echo "a pio process is already running (a build or an upload): wait for it, or end it, then retry" >&2
  tasklist //FI "IMAGENAME eq pio.exe" 2>/dev/null | grep -i "pio.exe" >&2
  exit 3
fi
LOGS="$REPO/.cache/silkscreen-builds"
mkdir -p "$LOGS"
export PYTHONIOENCODING=utf-8 PYTHONUTF8=1
export GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=core.longpaths GIT_CONFIG_VALUE_0=true

label=$(date +%Y%m%d-%H%M%S)
full=0
envs=()
while [ $# -gt 0 ]; do
  case "$1" in
    --full) full=1 ;;
    --label) shift; label="$1" ;;
    -h|--help) awk 'NR > 1 && /^#/ { print; next } NR > 1 { exit }' "$SELF"; exit 0 ;;
    *) envs+=("$1") ;;
  esac
  shift
done
if [ ${#envs[@]} -eq 0 ]; then
  envs=(silkscreen silkscreen-batlog silkscreen-368 silkscreen-368-batlog x4-pro x4-classic c3check)
  [ $full = 1 ] && envs=(silkscreen silkscreen-batlog silkscreen-debug silkscreen-368 silkscreen-368-batlog \
    x4-pro x4-pro-debug x4-classic c3check stickycheck)
fi

real=()
proxy=()
for e in "${envs[@]}"; do
  case "$e" in
    silkscreen*) real+=("$e") ;;
    default|debug|sticky|sticky-debug)
      echo "$e: [firmware_tuned] env, not buildable on this host; use c3check / stickycheck" >&2; exit 2 ;;
    *) proxy+=("$e") ;;
  esac
done

PROXY="$LOGS/proxy-platformio.ini"
OLD_PLATFORM='releases/download/55.03.37/platform-espressif32.zip'
NEW_PLATFORM='releases/download/55.03.311/platform-espressif32.zip'
if [ ${#proxy[@]} -gt 0 ]; then
  if ! grep -q "$OLD_PLATFORM" platformio.ini; then
    echo "platformio.ini no longer names $OLD_PLATFORM: update OLD_PLATFORM in $0" >&2
    exit 2
  fi
  sed -e "s#$OLD_PLATFORM#$NEW_PLATFORM#" platformio.ini > "$PROXY"
  cat >> "$PROXY" <<'EOI'

[env:c3check]
extends = base
custom_firmware_device_type = x3-x4
build_flags =
  ${base.build_flags}
  -DARDUINO_USB_MODE=1
  -DCROSSINK_APP_CAP_USB_DRIVE=0
  -DFREEINK_DEVICE_X4=1
  -DFREEINK_DEVICE_X3=1
  -DCROSSINK_APP_CAP_TOUCH=0
  -DCROSSINK_FIRMWARE_DEVICE_TYPE=\"x3-x4\"
  -DENABLE_SERIAL_LOG
  -DLOG_LEVEL=1

[env:stickycheck]
extends = base
lib_deps =
  ${base.lib_deps}
  FreeInkFont=symlink://freeink-sdk/libs/font/FreeInkFont
custom_firmware_device_type = sticky
board = esp32-s3-devkitc1-n16r8
board_build.mcu = esp32s3
build_flags =
  -DCROSSINK_SCALABLE_FONTS=1
  -DFREEINK_FONT_ENABLE_AUTOHINT=1
  -DFREEINK_FONT_ENABLE_NATIVE_HINTING=1
  -DFREEINK_FONT_ENABLE_MONOCHROME=1
  ${base.build_flags}
  -DARDUINO_USB_MODE=1
  -DCROSSINK_APP_CAP_USB_DRIVE=0
  -DFREEINK_DEVICE_STICKY=1
  -DCROSSINK_APP_CAP_TOUCH=1
  -DFREEINK_FB_PSRAM=1
  -DCROSSINK_FIRMWARE_DEVICE_TYPE=\"sticky\"
  -DENABLE_SERIAL_LOG
  -DLOG_LEVEL=1
EOI
fi

failed=0
report() {  # env rc log
  local flash ram warnings narrowing
  flash=$(grep -oE "Flash: \[[^]]*\] +[0-9.]+% \(used [0-9]+ bytes" "$3" | grep -oE "used [0-9]+" | tail -1)
  ram=$(grep -oE "RAM: +\[[^]]*\] +[0-9.]+% \(used [0-9]+ bytes" "$3" | grep -oE "used [0-9]+" | tail -1)
  warnings=$(grep -cE "warning:" "$3")
  narrowing=$(grep -c "Wnarrowing" "$3")
  echo "$1 exit=$2 flash=${flash#used } ram=${ram#used } warnings=$warnings narrowing=$narrowing log=$3"
  grep -E "App touch gate audit|Firmware image|Non-touch firmware|Sleep and wake source" "$3" | sed 's/^/  /' | head -4
  if [ "$2" -ne 0 ]; then grep -m8 -E "error:|Error " "$3" | sed 's/^/  /'; failed=1; fi
}

shared=$(git -C ../crosspoint-reader/freeink-sdk rev-parse --short silkscreen 2>/dev/null)
echo "SDK HEAD $(git -C freeink-sdk rev-parse --short HEAD), gitlink $(git ls-tree HEAD freeink-sdk | awk '{print substr($3,1,7)}'), shared head ${shared:-?}"
if [ -f scripts/check_sleep_wake_sources.py ]; then
  python scripts/check_sleep_wake_sources.py > "$LOGS/$label-sleepguard.log" 2>&1
  rc=$?; echo "sleep-wake guard exit=$rc: $(head -1 "$LOGS/$label-sleepguard.log")"; [ $rc -ne 0 ] && failed=1
fi

for env in "${real[@]}"; do
  log="$LOGS/$label-$env.log"
  pio run -e "$env" > "$log" 2>&1
  report "$env" $? "$log"
done
# The 3.68" variant has no touch: its image must carry no touch driver and none of the other panel
# families' code, and its own board tag.
audited=0
for env in "${real[@]}"; do
  case "$env" in
    silkscreen-368*)
      log="$LOGS/$label-notouch-$env.log"
      bash scripts/silkscreen/check_no_touch.sh "$env" > "$log" 2>&1
      rc=$?; echo "no-touch audit ($env) exit=$rc: $(tail -1 "$log")"; [ $rc -ne 0 ] && failed=1
      audited=1
      ;;
  esac
done
# The audit's control. Its patterns are names, and a name that matches nothing passes: if the SDK renames a
# driver function, a 3.68" image that links the touch driver goes green. The 4.26" board's image has the touch
# driver, the SSD1677 and UC8279 X4 drivers and the generic probe, and links no UC8279d X3 driver, so the same
# audit on it must fail on all of them: the touch names, the other panels' names, the probe's size and the
# missing UC8279d driver. Run only after an audit above, and only when that image is on disk (silkscreen
# builds first in the default list).
if [ $audited = 1 ]; then
  if [ -f .pio/build/silkscreen/firmware.elf ] && [ -f .pio/build/silkscreen/firmware.bin ]; then
    log="$LOGS/$label-notouch-control.log"
    ELF=.pio/build/silkscreen/firmware.elf BIN=.pio/build/silkscreen/firmware.bin \
      bash scripts/silkscreen/check_no_touch.sh silkscreen-368 > "$log" 2>&1
    rc=$?
    if [ $rc -eq 1 ] && grep -q "^Touch driver code is linked" "$log" \
        && grep -q "^The UC8253 X3 driver, the SSD1677 or UC8279 X4 driver, or display-probe code" "$log" \
        && grep -q "^freeink::applyXteinkDisplayController(\.\.\.) is " "$log" \
        && grep -q "^freeink::Uc8279Driver:: has 0 symbols" "$log"; then
      echo "no-touch audit control (4.26\" silkscreen image): fails on the touch names, the other panels' names, the probe's size and the missing UC8279d driver, as it must"
    else
      echo "no-touch audit control (4.26\" silkscreen image) exit=$rc: did NOT fail on the touch names, the other" \
        "panels' names, the probe's size and the missing UC8279d driver, so the audit's patterns no longer match" \
        "a touch build or another panel's code: see $log"
      failed=1
    fi
  else
    echo "no-touch audit control skipped: no .pio/build/silkscreen image (build the silkscreen env first)"
  fi
fi
if [ ${#proxy[@]} -gt 0 ]; then
  echo "proxy envs next: they wipe .pio/build (all envs) when they start"
fi
for env in "${proxy[@]}"; do
  log="$LOGS/$label-$env.log"
  pio run -c "$PROXY" -e "$env" > "$log" 2>&1
  report "$env" $? "$log"
done

exit $failed
