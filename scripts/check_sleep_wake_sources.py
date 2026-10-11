"""Deep-sleep and wake-source guard for the Silkscreen builds.

Silkscreen has no hardware off: deep sleep is "off", and the power button is the
only thing allowed to end it. This guard keeps it that way:

- the only deep-sleep entry is freeink::PowerManager::deepSleep(), reached
  through deepSleepUntilPowerButton() from HalPowerManager on the loop task;
- the only wake source is the power button, armed by the SDK's PowerManager
  (ext1 on Xtensa, the GPIO wake on RISC-V);
- no timer, ULP, touch-pad, ext0, UART or radio wake, no light sleep, no RTC
  power-domain override and no ultra-low sleep mode, anywhere in src/, lib/,
  include/ or the SDK's libs/.

The one RTC power-domain call allowed is upstream's, in the SDK's
PowerManager::armWakeOnPins(): esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,
ESP_PD_OPTION_ON) keeps the RTC pull it sets on each wake pad powered. It is
allowed only behind the InputPins::powerExternalPull exemption (a statement
naming it ahead of the call, in the same function), which keeps a power pin the
board biases itself, Silkscreen's IO18, out of both the pull and the override
(POWER.md ZD-01 and its pin table).

Documented exception: CrossInk's own sleep sequence for every other board, in
lib/hal/HalPowerManager.cpp, calls esp_deep_sleep_start(), the SDK's
waitForPowerButtonRelease() (ignoring its result), esp_sleep_config_gpio_isolate()
and armPowerButtonWakeup() itself. Those calls are allowed only in the #else
branch of `#if FREEINK_CAP_POWER_POLICY`, which a Silkscreen build
(FREEINK_CAP_POWER_POLICY=1) never compiles; its sleep goes through the SDK path
in the #if branch, which must call deepSleepUntilPowerButton().

Comment lines and test/ directories are ignored. Runs as a pre-build script of
the silkscreen environments, or standalone (`python scripts/check_sleep_wake_sources.py`,
exit status 1 on a violation).
"""

import os
import re
import sys

ROOTS = ("src", "lib", "include", os.path.join("freeink-sdk", "libs"))
EXTENSIONS = (".c", ".cpp", ".h", ".hpp")
SDK_POWER_MANAGER = "freeink-sdk/libs/hardware/PowerManager/src/PowerManager.cpp"
EXCEPTION_FILE = "lib/hal/HalPowerManager.cpp"
EXCEPTION_MACRO = "FREEINK_CAP_POWER_POLICY"

FORBIDDEN = re.compile(
    r"esp_sleep_enable_timer_wakeup|esp_deep_sleep\(|ESP\.deepSleep|esp_sleep_enable_ulp_wakeup|ulp_run|"
    r"ulp_riscv_run|esp_sleep_enable_touchpad_wakeup|touchSleepWakeUpEnable|esp_sleep_enable_ext0_wakeup|"
    r"esp_sleep_enable_gpio_wakeup|esp_sleep_enable_uart_wakeup|esp_sleep_enable_wifi_wakeup|"
    r"esp_sleep_enable_bt_wakeup|esp_light_sleep_start|rtc_sleep_enable_ultra_low|"
    r"ESP_SLEEP_ULTRA_LOW_MODE"
)
# Only the SDK's PowerManager.cpp may enter deep sleep or arm a wake source.
SDK_OWNED = re.compile(
    r"\b(esp_deep_sleep_start|esp_sleep_enable_ext1_wakeup|esp_deep_sleep_enable_gpio_wakeup)\s*\("
)
# CrossInk's own sleep sequence; outside the SDK only in the exception branch.
CROSSINK_TAIL = re.compile(r"\b(waitForPowerButtonRelease|armPowerButtonWakeup|esp_sleep_config_gpio_isolate)\s*\(")
SDK_SLEEP_ENTRY = re.compile(r"freeink::PowerManager::deepSleepUntilPowerButton\s*\(")
COMMENT_LINE = re.compile(r"^\s*(//|\*|/\*)")
# The single RTC power-domain call allowed, and what must stand in front of it (see the module docstring).
PD_CONFIG = re.compile(r"\besp_sleep_pd_config\s*\(")
PD_ALLOWED_ARGS = re.compile(r"esp_sleep_pd_config\s*\(\s*ESP_PD_DOMAIN_RTC_PERIPH\s*,\s*ESP_PD_OPTION_ON\s*\)")
WAKE_FN_START = re.compile(r"^void\s+PowerManager::armWakeOnPins\b")
PD_EXEMPTION = re.compile(r"\bpowerExternalPull\b")
DIRECTIVE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$")


def iter_sources(project_dir):
    for root in ROOTS:
        top = os.path.join(project_dir, root)
        for dirpath, dirnames, filenames in os.walk(top):
            dirnames[:] = [d for d in dirnames if d != "test"]
            for name in filenames:
                if name.endswith(EXTENSIONS):
                    path = os.path.join(dirpath, name)
                    yield os.path.relpath(path, project_dir).replace(os.sep, "/"), path


def branch_states(lines):
    """For each line, whether it sits in the #if or the #else branch of EXCEPTION_MACRO."""
    stack = []  # one entry per open conditional: (is EXCEPTION_MACRO, in its #else part)
    states = []
    for line in lines:
        match = DIRECTIVE.match(line)
        if match:
            kind, rest = match.group(1), match.group(2).strip()
            if kind in ("if", "ifdef", "ifndef"):
                stack.append((kind == "if" and rest == EXCEPTION_MACRO, False))
            elif kind in ("elif", "else") and stack:
                stack[-1] = (stack[-1][0], True)
            elif kind == "endif" and stack:
                stack.pop()
        in_if = any(is_macro and not in_else for is_macro, in_else in stack)
        in_else = any(is_macro and in_else for is_macro, in_else in stack)
        states.append((in_if, in_else))
    return states


def check(project_dir):
    problems = []
    sdk_path_calls = 0
    for rel, path in iter_sources(project_dir):
        with open(path, encoding="utf-8", errors="replace") as source:
            lines = source.read().splitlines()
        states = branch_states(lines) if rel == EXCEPTION_FILE else None
        in_wake_fn = False
        exempt = False
        pd_calls = 0
        for number, line in enumerate(lines, 1):
            if rel == SDK_POWER_MANAGER:
                if WAKE_FN_START.match(line):
                    in_wake_fn = True
                elif in_wake_fn and line.startswith("}"):
                    in_wake_fn = False
            if COMMENT_LINE.match(line):
                continue
            where = f"{rel}:{number}: {line.strip()}"
            if in_wake_fn and PD_EXEMPTION.search(line):
                exempt = True
            if PD_CONFIG.search(line):
                if rel != SDK_POWER_MANAGER:
                    problems.append(f"RTC power-domain override (esp_sleep_pd_config) outside {SDK_POWER_MANAGER}: {where}")
                else:
                    pd_calls += 1
                    if not in_wake_fn:
                        problems.append(f"esp_sleep_pd_config outside PowerManager::armWakeOnPins(): {where}")
                    elif not exempt:
                        problems.append(f"esp_sleep_pd_config ahead of the powerExternalPull exemption: {where}")
                    elif not PD_ALLOWED_ARGS.search(line):
                        problems.append(f"esp_sleep_pd_config other than ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON: {where}")
            if FORBIDDEN.search(line):
                problems.append(f"Forbidden sleep or wake API (only the power button may wake the device): {where}")
            in_exception = states is not None and states[number - 1][1]
            if SDK_OWNED.search(line) and rel != SDK_POWER_MANAGER and not in_exception:
                problems.append(f"Deep-sleep entry or wake source outside {SDK_POWER_MANAGER}: {where}")
            if CROSSINK_TAIL.search(line) and not rel.startswith("freeink-sdk/") and not in_exception:
                problems.append(f"CrossInk sleep-tail call outside its non-Silkscreen branch: {where}")
            if states is not None and states[number - 1][0] and SDK_SLEEP_ENTRY.search(line):
                sdk_path_calls += 1
        if rel == SDK_POWER_MANAGER and pd_calls > 1:
            problems.append(f"{rel}: {pd_calls} esp_sleep_pd_config calls, at most one is allowed")
    if sdk_path_calls == 0:
        problems.append(
            f"{EXCEPTION_FILE}: no freeink::PowerManager::deepSleepUntilPowerButton() call in its "
            f"#if {EXCEPTION_MACRO} branch (the Silkscreen sleep path)"
        )
    return problems


def report(problems):
    if problems:
        print("Sleep and wake source guard failed:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print(
        "Sleep and wake sources OK: PowerManager is the only deep-sleep entry and wake-source owner "
        "(CrossInk's own tail stays outside the Silkscreen build)."
    )
    return 0


try:
    Import("env")  # noqa: F821  # type: ignore[name-defined]
except NameError:
    _project_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sys.exit(report(check(_project_dir)))
else:
    if report(check(env.subst("$PROJECT_DIR"))) != 0:  # noqa: F821  # type: ignore[name-defined]
        env.Exit(1)  # noqa: F821  # type: ignore[name-defined]
