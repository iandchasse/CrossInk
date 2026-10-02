#!/usr/bin/env bash
# Silkscreen build matrix for CrossInk (docs/silkscreen/PORTING.md, "Upstream merges"; the whole
# multi-repo procedure is crosspoint-reader/docs/silkscreen/UPSTREAM-SYNC.md).
#
# Usage: scripts/silkscreen/build_matrix.sh [--full] [--label NAME] [env ...]
#   no envs : silkscreen x4-pro x4-classic c3check
#   --full  : adds silkscreen-debug, x4-pro-debug and stickycheck
# Prints one line per env (exit code, flash/RAM bytes, warning counts, log path) and exits
# non-zero if any env or check failed. Logs go to .cache/silkscreen-builds/ (gitignored).
#
# What builds on the Windows bench host, and how:
#   silkscreen*  : the real platformio.ini (these envs pin pioarduino 55.03.311 per env).
#   x4-pro*, x4-classic* : CrossInk's real envs through a proxy config whose only change is
#       [base]'s platform, 55.03.37 -> the installed 55.03.311. 55.03.37 needs the Arduino 3.3.7
#       framework package; installing it would replace the framework copy CrossPoint builds with.
#   c3check, stickycheck : default's / sticky's flags and sources on [base]. The real default,
#       debug, sticky and sticky-debug extend [firmware_tuned], whose custom sdkconfig rebuild
#       cannot run on this host (pioarduino tool-scons vs PlatformIO Core).
# Every proxy env changes PlatformIO's project checksum, which wipes ALL of .pio/build, so the
# silkscreen envs (real config) build first and the proxy envs after them.
# Never run this while the user is building or flashing from this checkout.
set -u
cd "$(dirname "$0")/../.." || exit 2
REPO=$(pwd)
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
    -h|--help) sed -n '2,22p' "$0"; exit 0 ;;
    *) envs+=("$1") ;;
  esac
  shift
done
if [ ${#envs[@]} -eq 0 ]; then
  envs=(silkscreen x4-pro x4-classic c3check)
  [ $full = 1 ] && envs=(silkscreen silkscreen-debug x4-pro x4-pro-debug x4-classic c3check stickycheck)
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
if [ ${#proxy[@]} -gt 0 ]; then
  echo "proxy envs next: they wipe .pio/build (all envs) when they start"
fi
for env in "${proxy[@]}"; do
  log="$LOGS/$label-$env.log"
  pio run -c "$PROXY" -e "$env" > "$log" 2>&1
  report "$env" $? "$log"
done

exit $failed
