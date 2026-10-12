#!/usr/bin/env bash
# No-touch audit of a built Silkscreen 3.68" image (docs/silkscreen/PORTING.md, "The 3.68" variant").
# CrossPoint's scripts/silkscreen/check_no_touch.sh, less its test-firmware checks (CrossInk has no test
# firmware) and its size cap (below).
#
# The variant has no touch at all: no tail on J4, FREEINK_CAP_TOUCH=0, the board's profile with NO_TOUCH.
# Nothing may then probe 0x38, 0x5D or 0x14, reset TP_RST or send a hibernate at a sleep entry, and the
# build proves it by what is linked. This reads the image's symbol table and fails when:
#   - a touch driver body is linked (the FT5x06 / FT6336U / GT911 / CST816S / GSLX680 / Picco / CHSC6x
#     begin, poll and sleep code, the FocalTech id check, the hibernate and the multi-touch classifiers),
#     by its demangled name or by its mangled one (a GCC clone, name$isra$0 or name$constprop$0, which
#     nm -C leaves mangled);
#   - any UC8253 driver (freeink::Uc8253*, the X3's or Murphy's, and their tables), the SSD1677 or UC8279 X4
#     driver, the UC8179 driver, or any display probe (the stock X3 VER read, probeX3DisplayController, or
#     the two-pass VER / FLG / MTP read) is linked, by demangled or by mangled name: the variant's panel is
#     the X3's UC8279d, fixed in its profile, and nothing identifies it. An inlined probe has no symbol of
#     its own, so the size of its two entry points, applyXteinkDisplayController() and
#     detectXteinkDisplayController(), is capped as well;
#   - the UC8279d X3 driver (Uc8279Driver) is not linked, so a pass would prove nothing;
#   - the image's board tag is not the variant's (silkscreen-368, which its battery-log image keeps).
# The app's own touch code is audited by scripts/check_app_touch_gate.py, which runs after every link of an env
# with CROSSINK_APP_CAP_TOUCH=0 (this script does not repeat it).
#
# What each check rests on, so a red or a green result is read right.
#   - The touch-body patterns are names. Some of them are symbols that a no-touch build still compiles
#     (InputManager::beginTouch(), serviceTouch(), prepareTouchForSleep(), the *MultiTouch* classifiers, ...):
#     they are out of the image only because nothing references them, the linker's --gc-sections drops them and,
#     for beginTouch(), the compiler inlines it. An -O0 / -Og build, or an app or SDK change that reaches one of
#     them, fails this audit on an image with no touch driver; that is a false alarm to look at, not touch code.
#     The other direction does not occur: with the driver built, beginTouch() is 268 bytes and out of line.
#   - A pattern that matches nothing passes. If the SDK renames a driver function, this audit goes quiet on the
#     image that links it. build_matrix.sh therefore runs this script on the 4.26" board's silkscreen image
#     (ELF= and BIN=) after the 3.68" ones, and that run must fail on the touch names, on the other panels'
#     names and on the missing UC8279d driver: the control.
#   - There is no size cap. CrossPoint's copy caps InputManager::resetAndHibernateTouch() at 32 bytes, a 7-byte
#     stub in its image (its HalGPIO still calls it). Nothing calls it here with FREEINK_CAP_TOUCH=0, so
#     --gc-sections drops it: it is not in this image at all (255 bytes in the 4.26" board's), and a cap on a
#     symbol that is absent measures nothing. InputManager::startTouch() is 15 bytes in a touch build too (it
#     only calls beginTouch()), so it carries no cap either.
#
# Usage: scripts/silkscreen/check_no_touch.sh [env]    (default: silkscreen-368; build it first)
# Exits 0 on a pass, 1 when a check fails (each is listed), 2 when the image or nm cannot be read.
# The nm binary comes from $NM, else from PlatformIO's Xtensa toolchain. ELF=<file> and BIN=<file> name
# another image than .pio/build/<env>/firmware.{elf,bin}: the control is this script on the 4.26" board's
# silkscreen image, which must fail on the touch driver, the SSD1677 driver and the missing UC8279d driver.
# A 3.68" image built before the UC8279d-only change links the UC8253 driver and the stock probe, and must
# fail on those two.
set -u
cd "$(dirname "$0")/../.." || exit 2

env="${1:-silkscreen-368}"
elf="${ELF:-.pio/build/${env}/firmware.elf}"
bin="${BIN:-.pio/build/${env}/firmware.bin}"
for f in "$elf" "$bin"; do
  if [ ! -f "$f" ]; then
    echo "missing $f: build the ${env} env first"
    exit 2
  fi
done

nm_bin="${NM:-}"
if [ -z "$nm_bin" ]; then
  for candidate in "$HOME"/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-nm \
    "$HOME"/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-nm.exe; do
    if [ -x "$candidate" ]; then
      nm_bin="$candidate"
      break
    fi
  done
fi
if [ -z "$nm_bin" ]; then
  echo "no nm found: set NM to the toolchain's nm"
  exit 2
fi

# Demangled names with sizes. A failed or foreign nm must not pass as an image with nothing in it, so the
# table has to hold a symbol every CrossInk image has.
if ! syms=$("$nm_bin" -C -S --defined-only "$elf"); then
  echo "nm failed on $elf"
  exit 2
fi
if ! grep -q 'board_tag::boardName' <<<"$syms"; then
  echo "nm output for $elf has no board_tag::boardName: wrong nm or ELF?"
  exit 2
fi
names=$(awk '{ $1 = ""; $2 = ""; $3 = ""; sub(/^ +/, ""); print }' <<<"$syms")
# The same symbols as the linker wrote them, for the clones nm -C cannot demangle.
if ! raw=$("$nm_bin" --defined-only "$elf" | awk '{ print $NF }'); then
  echo "nm failed on $elf"
  exit 2
fi

status=0
fail() {
  echo "$1"
  status=1
}

# Touch driver bodies: every InputManager touch function the 4.26" board's image has and a no-touch one
# does not (the controllers' begin and poll, the sleep and hibernate code, the id check, the gesture
# classifiers), and the helpers they use. The member names, then the free functions, as alternations.
touch_members='begin(Cst816s|Ft5x06|Ft6336u|Gt911|PiccoTouch|TouchController|Touch)|poll(Cst816s|Ft5x06|Ft6336u|Gslx680|Gt911|PiccoTouch)|serviceTouch|updateTouchFromIrq|focaltechHibernate|gt911(ClearStatus|ReadReg|Sleep)|ft5x06(ReadReg|WriteReg)|prepareTouchForSleep|pulseTouchReset|holdTouchIntForSleep|decodeChsc6xFrame|readChsc6xPoint|touchDiagnostics|[A-Za-z]*MultiTouch[A-Za-z]*'
touch_free='focalTechIdentified|cachedGt911Asleep|cachedTouchProbe|driveGt911IntLow|cstTouchInterrupt'
touch_bodies="InputManager::(${touch_members})[(]|(^|[^A-Za-z0-9_])(${touch_free})[(]"
if hits=$(grep -E "$touch_bodies" <<<"$names"); then
  fail "Touch driver code is linked into ${env}:"
  echo "$hits" | head -20
fi
# A clone keeps the length-prefixed identifier of its original: ..InputManager10beginGt911Ev..$isra$0.
touch_mangled="InputManager[0-9]+(${touch_members})|[0-9]+(${touch_free})"
if hits=$(grep -E "$touch_mangled" <<<"$raw"); then
  fail "Touch driver code is linked into ${env} (mangled names, clones included):"
  echo "$hits" | head -20
fi

# The other panels' code and every probe: the variant links the X3's UC8279d driver and nothing of any
# UC8253 driver, the SSD1677, the UC8279 X4 driver, the UC8179 driver, the stock X3 VER read
# (probeX3DisplayController) or the generic two-pass probe. Its controller is fixed in its profile and
# nothing identifies it at a boot or a wake. The drivers are matched by their prefix (freeink::Uc8253*,
# freeink::uc8253*, ...), so a renamed or a new UC8253 driver (Uc8253MurphyDriver is one) is not missed.
panel_names='[Ss]sd1677|[Uu]c8253|[Uu]c8279X4|[Uu]c8179'
other_panels="freeink::[A-Za-z0-9_:]*(${panel_names})|probeX3DisplayController|runDisplayProbePass|probeDisplayController|matchUc81xx|verIsFloating|epdCmdRead|epdReadByte"
if hits=$(grep -E "$other_panels" <<<"$names"); then
  fail "A UC8253, SSD1677, UC8279 X4 or UC8179 driver, or display-probe code (the stock X3 read, the two-pass read) is linked into ${env}:"
  echo "$hits" | head -20
fi
# Clones of those drivers' functions stay mangled: _ZN7freeink14Uc8253X3Driver5beginE..$isra$0.
other_mangled="7freeink.*[0-9](${panel_names})"
if hits=$(grep -E "$other_mangled" <<<"$raw"); then
  fail "A UC8253, SSD1677, UC8279 X4 or UC8179 driver is linked into ${env} (mangled names, clones included):"
  echo "$hits" | head -20
fi

# The stock X3 VER read and the two-pass read are inlined into freeink::applyXteinkDisplayController() and
# detectXteinkDisplayController(), so an image that still has them carries no symbol of its own for the
# names above to find (the variant's image built before the UC8279d-only change had neither
# probeX3DisplayController nor runDisplayProbePass in its table). Without the probe the first only logs and
# returns (112 bytes in the SDK's block for the variant, 1106 with a probe) and the second is not linked.
probe_entry_cap() {  # name( max_bytes
  local line size
  line=$(grep -F -- " $1" <<<"$syms" | head -1)
  [ -z "$line" ] && return 0
  size=$((16#$(awk '{print $2}' <<<"$line")))
  if [ "$size" -gt "$2" ]; then
    fail "${1}...) is $size bytes in ${env} (without a probe it is at most $2): display-probe code is linked"
  fi
}
probe_entry_cap 'freeink::applyXteinkDisplayController(' 256
probe_entry_cap 'freeink::detectXteinkDisplayController(' 256

driver='freeink::Uc8279Driver::'
count=$(grep -c -F -- "$driver" <<<"$names" || true)
if [ "${count:-0}" -lt 10 ]; then
  fail "${driver} has ${count:-0} symbols in ${env}, expected the X3 UC8279d driver linked"
fi

# One tag, the variant's own: OTA and the SD update refuse the 4.26" board's image by it.
expected_tag="silkscreen-368"
tags=$(LC_ALL=C grep -a -o -E 'CROSSPOINT-BOARD-V1:[!-:<-~]{1,23};' "$bin" || true)
count=$(printf '%s' "$tags" | grep -c . || true)
if [ "$count" -ne 1 ] || [ "$tags" != "CROSSPOINT-BOARD-V1:${expected_tag};" ]; then
  fail "Expected exactly one board tag CROSSPOINT-BOARD-V1:${expected_tag}; in ${bin}, found ${count}:"
  printf '%s\n' "$tags"
fi

if [ "$status" -eq 0 ]; then
  echo "${env}: no touch driver, no UC8253 / SSD1677 / UC8279 X4 driver, no display-probe code, the UC8279d X3 driver linked; board tag ${expected_tag} present once."
fi
exit "$status"
