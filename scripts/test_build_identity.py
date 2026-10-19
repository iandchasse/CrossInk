"""Check that volatile build identity reaches only BuildInfo.cpp."""
from pathlib import Path
import runpy
import configparser
import unittest
import os
import re
import subprocess
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
MODULE = runpy.run_path(str(ROOT / "scripts/git_branch.py"))

class Env(dict):
    def __init__(self, name="default"):
        super().__init__(PROJECT_DIR=str(ROOT), PIOENV=name)
        self.defines = []
        self.middleware = []
    def Append(self, **kwargs): self.defines.extend(kwargs.get("CPPDEFINES", []))
    def Clone(self):
        copy = Env(self["PIOENV"])
        copy.defines = list(self.defines)
        return copy
    def Object(self, node): return (node, self.defines)
    def AddBuildMiddleware(self, callback, pattern): self.middleware.append((callback, pattern))

class BuildIdentityTest(unittest.TestCase):
    def test_download_endpoint_is_explicit_and_validated(self):
        base = "https://crossink-languages.s3.us-east-1.amazonaws.com/testing/abc/"
        function = MODULE["inject_download_endpoints"]
        env = Env()
        with patch.dict(os.environ, {"CROSSINK_ASSET_BASE_URL": base}, clear=True):
            function(env)
        self.assertEqual(len(env.defines), 1)
        self.assertEqual(env.defines[0][0], "ASSET_DOWNLOAD_BASE_URL")
        self.assertIn(base, env.defines[0][1])
        for invalid in ("http://host/", "https://host/no-slash", "https://host/../", "https://user:pass@host/", 'https://host/"bad/'):
            with patch.dict(os.environ, {"CROSSINK_ASSET_BASE_URL": invalid}, clear=True):
                with self.assertRaises(ValueError): function(Env())
        with patch.dict(os.environ, {"CROSSINK_ASSET_BASE_URL": ""}, clear=True):
            empty = Env(); function(empty)
            self.assertEqual(empty.defines, [])

    def test_font_endpoint_is_opt_in_and_validated(self):
        function = MODULE["inject_download_endpoints"]
        with patch.dict(os.environ, {}, clear=True):
            env = Env(); function(env)
            self.assertEqual(env.defines, [])
        for base in ("http://fonts.example.com/", "https://fonts.example.com/custom/"):
            with patch.dict(os.environ, {"CROSSINK_FONT_BASE_URL": base}, clear=True):
                env = Env(); function(env)
                self.assertEqual(len(env.defines), 1)
                self.assertEqual(env.defines[0][0], "FONT_DOWNLOAD_BASE_URL")
                self.assertIn(base, env.defines[0][1])
        for invalid in ("ftp://host/", "https://host/no-slash", "https://host/../",
                        "https://user:pass@host/", 'https://host/"bad/', "https://host/\n"):
            with patch.dict(os.environ, {"CROSSINK_FONT_BASE_URL": invalid}, clear=True):
                with self.assertRaises(ValueError): function(Env())
        with patch.dict(os.environ, {"CROSSINK_ASSET_BASE_URL": "https://assets.example.com/",
                                    "CROSSINK_FONT_BASE_URL": "http://fonts.example.com/"}, clear=True):
            env = Env(); function(env)
            self.assertEqual([item[0] for item in env.defines],
                             ["ASSET_DOWNLOAD_BASE_URL", "FONT_DOWNLOAD_BASE_URL"])

    def test_font_manifest_url_matches_build_configuration_and_format(self):
        header = (ROOT / "src/activities/settings/FontDownloadActivity.h").read_text()
        macros = header[header.index("#define FONTS_MANIFEST_VERSION"):header.index("class FontDownloadActivity")]
        font_header = (ROOT / "lib/EpdFont/SdCardFont.h").read_text()
        version = re.search(r"^#define CPFONT_VERSION ([0-9]+)$", font_header, re.M).group(1)
        manifest_version = re.search(r"^#define FONTS_MANIFEST_VERSION ([0-9]+)$", macros, re.M).group(1)
        for prefix in (None, "http://fonts.example.com/", "https://fonts.example.com/custom/"):
            with self.subTest(prefix=prefix):
                source = f"#define CPFONT_VERSION {version}\n"
                if prefix is not None:
                    source += f'#define FONT_DOWNLOAD_BASE_URL "{prefix}"\n'
                effective_prefix = prefix or "https://github.com/uxjulia/crossink-fonts/releases/download/"
                catalog_base = f"{effective_prefix}sd-fonts-m{manifest_version}-b{version}/"
                expected = catalog_base + "fonts.json"
                source += macros + f'\nstatic_assert(__builtin_strcmp(FONT_MANIFEST_URL, "{expected}") == 0);\n'
                source += f'static_assert(__builtin_strcmp(FONT_DOWNLOAD_CATALOG_BASE_URL, "{catalog_base}") == 0);\n'
                if prefix is None:
                    source += '#ifndef FONT_DOWNLOAD_USE_GITHUB_RELEASES\n#error Missing GitHub fallback\n#endif\n'
                else:
                    source += '#ifdef FONT_DOWNLOAD_USE_GITHUB_RELEASES\n#error Custom host must retain its catalog base URL\n#endif\n'
                subprocess.run(["c++", "-std=c++17", "-x", "c++", "-fsyntax-only", "-"],
                               input=source, text=True, check=True, capture_output=True)

    def test_identity_only_changes_one_compile_environment(self):
        function = MODULE["inject_version"]
        globals_ = function.__globals__
        previous = globals_["get_git_short_sha"]
        self.addCleanup(globals_.__setitem__, "get_git_short_sha", previous)
        for name in ("default", "sticky", "x4-pro", "x4-classic", "silkscreen", "silkscreen-batlog", "silkscreen-368",
                     "silkscreen-368-batlog", "debug", "sticky-debug", "x4-pro-debug", "test", "gh_release_rc",
                     "x4-pro-simulator"):
            captures = []
            for sha in ("abc1234", "def5678"):
                globals_["get_git_short_sha"] = lambda _: sha
                env = Env(name)
                function(env)
                self.assertEqual(len(env.middleware), 1)
                callback, pattern = env.middleware[0]
                self.assertEqual(pattern, "*src/util/BuildInfo.cpp")
                _, scoped = callback(env, "BuildInfo.cpp")
                names = [d[0] for d in env.defines if isinstance(d, tuple)]
                self.assertNotIn("CROSSINK_GIT_SHA", names)
                self.assertNotIn("CROSSINK_GIT_DIRTY", names)
                self.assertNotIn("CROSSINK_VERSION", names)
                self.assertIn("CROSSINK_GIT_SHA", dict(d for d in scoped if isinstance(d, tuple)))
                captures.append((env.defines, scoped))
            self.assertEqual(captures[0][0], captures[1][0])
            self.assertNotEqual(captures[0][1], captures[1][1])

    def test_silkscreen_368_versions_keep_the_battery_log_analyzers_endings(self):
        # The analyzer reads a version ending "-silkscreen-batlog" as CrossInk's (its early-wake limit
        # is 500 ms, not CrossPoint's 300), so the 3.68" variant's log build must still end that way.
        function = MODULE["get_hardware_version"]
        globals_ = function.__globals__
        previous = globals_["get_crossink_version"]
        self.addCleanup(globals_.__setitem__, "get_crossink_version", previous)
        globals_["get_crossink_version"] = lambda _: "1.6.1"
        with patch.dict(os.environ, {"CROSSINK_RELEASE_VERSION": "", "CROSSINK_RC_HASH": ""}):
            self.assertEqual(function(str(ROOT), "silkscreen-368"), "1.6.1-silkscreen-368")
            self.assertEqual(function(str(ROOT), "silkscreen-368-batlog"), "1.6.1-368-silkscreen-batlog")
            self.assertTrue(function(str(ROOT), "silkscreen-batlog").endswith("-silkscreen-batlog"))
            self.assertTrue(function(str(ROOT), "silkscreen-368-batlog").endswith("-silkscreen-batlog"))

    def test_no_firmware_device_type_continues_another_past_a_hyphen(self):
        # OtaUpdater.cpp accepts firmware-<type>.bin and firmware-<type>-<anything>.bin (a release asset is
        # firmware-<type>-v<version>.bin). A device type that continues another one past a hyphen would be
        # offered to that board: firmware-silkscreen-368.bin matches the 4.26" board's type "silkscreen".
        parser = configparser.RawConfigParser(strict=False, interpolation=None)
        parser.read(ROOT / "platformio.ini", encoding="utf-8")
        types = sorted({parser.get(section, "custom_firmware_device_type").strip()
                        for section in parser.sections()
                        if parser.has_option(section, "custom_firmware_device_type")})
        self.assertIn("silkscreen", types)  # the parse found the envs' types
        for first in types:
            for second in types:
                if first != second:
                    self.assertFalse(second.startswith(first + "-"),
                                     f"{second!r} continues {first!r}: its asset would match {first!r}'s")

    def test_support_provenance_excludes_custom_version_and_invalid_sdk_values(self):
        function = MODULE["inject_version"]
        globals_ = function.__globals__
        for name in ("get_crossink_version", "run_git_value"):
            previous = globals_[name]
            self.addCleanup(globals_.__setitem__, name, previous)
        globals_["get_crossink_version"] = lambda _: "1.2.3+PRIVATE_BRANCH_PATH"
        globals_["run_git_value"] = lambda *_: "PRIVATE_SDK_PATH"
        env = Env("x4-pro-simulator")
        function(env)
        _, defines = env.middleware[0][0](env, "BuildInfo.cpp")
        values = dict(d for d in defines if isinstance(d, tuple))
        self.assertIn("unknown", values["CROSSINK_SUPPORT_VERSION"])
        self.assertIn("unknown", values["CROSSINK_SDK_SHA"])
        globals_["get_crossink_version"] = lambda _: "1.2.3"
        globals_["run_git_value"] = lambda *_: "a" * 40
        env = Env("x4-pro-simulator")
        function(env)
        _, defines = env.middleware[0][0](env, "BuildInfo.cpp")
        values = dict(d for d in defines if isinstance(d, tuple))
        self.assertIn("1.2.3", values["CROSSINK_SUPPORT_VERSION"])
        self.assertIn("a" * 40, values["CROSSINK_SDK_SHA"])

    def test_support_version_honors_release_override_and_rc_precedence(self):
        function = MODULE["inject_version"]
        globals_ = function.__globals__
        previous = globals_["get_crossink_version"]
        self.addCleanup(globals_.__setitem__, "get_crossink_version", previous)
        globals_["get_crossink_version"] = lambda _: "1.6.1"
        for release, rc, expected in (("1.6.2", "", "1.6.2"),
                                       ("v1.6.3", "", "1.6.3"),
                                       ("1.6.2", "abc123", "1.6.1"),
                                       ("PRIVATE_VERSION", "", "unknown")):
            with patch.dict(os.environ, {"CROSSINK_RELEASE_VERSION": release, "CROSSINK_RC_HASH": rc}):
                env = Env("default")
                function(env)
                _, defines = env.middleware[0][0](env, "BuildInfo.cpp")
                values = dict(d for d in defines if isinstance(d, tuple))
                self.assertIn(expected, values["CROSSINK_SUPPORT_VERSION"])

if __name__ == "__main__": unittest.main()
