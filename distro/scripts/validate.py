#!/usr/bin/env python3
"""Static validation for the AURELIA OS image profile.

This does not replace an Ubuntu 26.04 live-build, Plasma runtime, Calamares
install test, or QEMU boot. It catches broken package names, JSON/YAML/INI
syntax, shell syntax, JavaScript syntax, malformed QML delimiters and missing
branding payloads before a multi-minute ISO build.
"""
from __future__ import annotations

import configparser
import json
import shutil
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
INCLUDE = ROOT / "config" / "includes.chroot"
ERRORS: list[str] = []
CHECKS = 0
LEAN_FORBIDDEN = {
    "libreoffice",
    "libreoffice-writer",
    "libreoffice-calc",
    "kdeconnect",
    "filelight",
    "breeze-gtk-theme",
    "kde-config-gtk-style",
    "plasma-discover-notifier",
    "plasma-discover-backend-snap",
    "xorriso",
    "snapd",
}


def fail(message: str) -> None:
    ERRORS.append(message)


def passed(message: str) -> None:
    global CHECKS
    CHECKS += 1
    print(f"  ok  {message}")


def files(pattern: str = "*") -> list[Path]:
    result = []
    for path in ROOT.rglob(pattern):
        if not path.is_file():
            continue
        rel = path.relative_to(ROOT)
        if rel.parts and rel.parts[0] in {"build", "cache", "chroot", "binary", ".git"}:
            continue
        if any(part in {"build", "cache", "chroot", "binary"} for part in rel.parts):
            continue
        result.append(path)
    return sorted(result)


def check_json() -> None:
    for path in files("*.json"):
        try:
            json.loads(path.read_text(encoding="utf-8"))
            passed(f"JSON {path.relative_to(ROOT)}")
        except Exception as exc:  # noqa: BLE001 - report exact parser error
            fail(f"JSON {path.relative_to(ROOT)}: {exc}")


def check_yaml() -> None:
    for path in files("*.conf") + files("*.desc"):
        rel = path.relative_to(ROOT)
        if "etc/calamares" not in rel.parts:
            continue
        try:
            yaml.safe_load(path.read_text(encoding="utf-8"))
            passed(f"YAML {rel}")
        except Exception as exc:  # noqa: BLE001
            fail(f"YAML {rel}: {exc}")


def check_ini() -> None:
    names = {"kdeglobals", "plasmarc", "kwinrc", "theme.conf"}
    for path in files():
        rel = path.relative_to(ROOT)
        if path.suffix not in {".desktop", ".colors"} and path.name not in names:
            continue
        parser = configparser.ConfigParser(interpolation=None, strict=False)
        try:
            parser.read_string(path.read_text(encoding="utf-8"))
            passed(f"INI {rel}")
        except Exception as exc:  # noqa: BLE001
            fail(f"INI {rel}: {exc}")


def check_shell() -> None:
    shell_paths = []
    for candidate in [ROOT / "auto" / "config", ROOT / "auto" / "build", ROOT / "auto" / "clean"]:
        if candidate.exists():
            shell_paths.append(candidate)
    shell_paths += list((ROOT / "scripts").glob("*.sh"))
    shell_paths += list((ROOT / "config" / "hooks").rglob("*.hook.chroot"))
    shell_paths += [
        INCLUDE / "usr/libexec/aurelia/aurelia-flatpak-setup",
        INCLUDE / "usr/libexec/aurelia/aurelia-first-login",
        INCLUDE / "usr/libexec/aurelia/aurelia-installer",
    ]
    for path in sorted(set(shell_paths)):
        if not path.exists():
            fail(f"shell script missing: {path.relative_to(ROOT)}")
            continue
        result = subprocess.run(
            ["bash", "-n", str(path)],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode:
            fail(f"shell {path.relative_to(ROOT)}: {result.stderr.strip()}")
        else:
            passed(f"shell {path.relative_to(ROOT)}")


def check_python() -> None:
    for path in files("*.py"):
        try:
            compile(path.read_text(encoding="utf-8"), str(path), "exec")
            passed(f"Python {path.relative_to(ROOT)}")
        except Exception as exc:  # noqa: BLE001
            fail(f"Python {path.relative_to(ROOT)}: {exc}")


def check_js() -> None:
    node = shutil.which("node")
    layouts = list(INCLUDE.rglob("org.kde.plasma.desktop-layout.js"))
    if not layouts:
        fail("no Plasma layout.js found")
        return
    if not node:
        fail("Node.js is required to check layout.js")
        return
    for path in layouts:
        result = subprocess.run(
            [node, "--check", str(path)],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode:
            fail(f"JavaScript {path.relative_to(ROOT)}: {result.stderr.strip()}")
        else:
            passed(f"JavaScript {path.relative_to(ROOT)}")


def strip_qml_strings_and_comments(source: str) -> str:
    out: list[str] = []
    i = 0
    state = "code"
    quote = ""
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if state == "code":
            if ch == "/" and nxt == "/":
                state = "line-comment"
                i += 2
                continue
            if ch == "/" and nxt == "*":
                state = "block-comment"
                i += 2
                continue
            if ch in "\"'":
                state = "string"
                quote = ch
                i += 1
                continue
            out.append(ch)
            i += 1
        elif state == "line-comment":
            if ch == "\n":
                state = "code"
                out.append(ch)
            i += 1
        elif state == "block-comment":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 2
            else:
                i += 1
        else:  # string
            if ch == "\\":
                i += 2
            elif ch == quote:
                state = "code"
                i += 1
            else:
                i += 1
    return "".join(out)


def check_qml() -> None:
    pairs = {")": "(", "]": "[", "}": "{"}
    for path in files("*.qml"):
        source = strip_qml_strings_and_comments(path.read_text(encoding="utf-8"))
        stack: list[str] = []
        problem = ""
        for ch in source:
            if ch in "([{":
                stack.append(ch)
            elif ch in ")]}":
                if not stack or stack[-1] != pairs[ch]:
                    problem = f"unmatched {ch}"
                    break
                stack.pop()
        if not problem and stack:
            problem = f"unclosed {stack[-1]}"
        if problem:
            fail(f"QML {path.relative_to(ROOT)}: {problem}")
        else:
            passed(f"QML delimiters {path.relative_to(ROOT)}")


def check_required_payload() -> None:
    required = [
        "usr/share/plasma/look-and-feel/org.aurelia.solstice/metadata.json",
        "usr/share/plasma/look-and-feel/org.aurelia.solstice/contents/defaults",
        "usr/share/plasma/look-and-feel/org.aurelia.solstice/contents/layouts/org.kde.plasma.desktop-layout.js",
        "usr/share/plasma/plasmoids/org.aurelia.clock/metadata.json",
        "usr/share/plasma/plasmoids/org.aurelia.clock/contents/ui/main.qml",
        "usr/share/plasma/plasmoids/org.aurelia.calendar/metadata.json",
        "usr/share/plasma/plasmoids/org.aurelia.calendar/contents/ui/main.qml",
        "usr/share/color-schemes/Aurelia.colors",
        "usr/share/wallpapers/AureliaSolstice/contents/images/3840x2160.png",
        "usr/share/wallpapers/AureliaIvory/contents/images/3840x2160.png",
        "usr/share/sddm/themes/aurelia/Main.qml",
        "usr/share/sddm/themes/aurelia/background.png",
        "usr/share/fonts/truetype/aurelia/PlayfairDisplay.ttf",
        "usr/share/fonts/truetype/aurelia/OFL.txt",
        "usr/share/applications/install-aurelia.desktop",
        "usr/libexec/aurelia/aurelia-first-login",
        "usr/libexec/aurelia/aurelia-installer",
        "etc/calamares/settings.conf",
        "etc/calamares/branding/aurelia/branding.desc",
        "etc/calamares/modules/unpackfsc.conf",
        "etc/calamares/modules/bootloader.conf",
        "etc/calamares/modules/mount.conf",
        "etc/default/grub",
    ]
    for rel in required:
        path = INCLUDE / rel
        if path.is_file() and path.stat().st_size > 0:
            passed(f"payload {rel}")
        else:
            fail(f"missing or empty payload: {rel}")

    for rel in ["auto/config", "auto/build", "auto/clean"]:
        path = ROOT / rel
        if path.is_file():
            passed(f"live-build entrypoint {rel}")
        else:
            fail(f"missing live-build entrypoint: {rel}")


def check_branding_and_installer() -> None:
    branding = yaml.safe_load(
        (INCLUDE / "etc/calamares/branding/aurelia/branding.desc").read_text(encoding="utf-8")
    )
    if not isinstance(branding.get("style"), dict):
        fail("Calamares branding.desc must contain a style map")
    else:
        passed("Calamares 3.3 style map")
    if branding.get("componentName") != "aurelia":
        fail("Calamares componentName must be aurelia")
    else:
        passed("Calamares componentName")

    settings = yaml.safe_load(
        (INCLUDE / "etc/calamares/settings.conf").read_text(encoding="utf-8")
    )
    sequence = settings.get("sequence", [])
    exec_steps = []
    for phase in sequence:
        if isinstance(phase, dict) and "exec" in phase:
            exec_steps.extend(phase["exec"])
    for required in ["partition", "mount", "unpackfsc", "bootloader", "umount"]:
        if required not in exec_steps:
            fail(f"Calamares sequence is missing {required}")
        else:
            passed(f"Calamares sequence {required}")
    if "unpackfs" in exec_steps:
        fail("Calamares sequence must use unpackfsc, not unpackfs")
    if settings.get("hide-back-and-next-during-exec") is None:
        fail("Calamares 3.3 requires hide-back-and-next-during-exec")
    if settings.get("quit-at-end") is None:
        fail("Calamares 3.3 requires quit-at-end")

    unpack = yaml.safe_load(
        (INCLUDE / "etc/calamares/modules/unpackfsc.conf").read_text(encoding="utf-8")
    )
    if unpack.get("source") != "/cdrom/casper/filesystem.squashfs":
        fail("unpackfsc source must be /cdrom/casper/filesystem.squashfs")
    if unpack.get("sourcefs") != "squashfs":
        fail("unpackfsc sourcefs must be squashfs")
    if unpack.get("destination") != "/":
        fail('unpackfsc destination must be "/" (not empty)')
    else:
        passed("unpackfsc flat configuration")

    layout = (
        INCLUDE / "usr/share/plasma/look-and-feel/org.aurelia.solstice/contents/layouts/org.kde.plasma.desktop-layout.js"
    ).read_text(encoding="utf-8")
    for token in ["org.aurelia.clock", "org.aurelia.calendar", "org.kde.plasma.notes", "org.kde.plasma.mediacontroller", "org.kde.plasma.icontasks", "org.kde.plasma.kickoff"]:
        if token not in layout:
            fail(f"Plasma layout missing {token}")
        else:
            passed(f"Plasma layout token {token}")
    if "\"favorites\"" not in layout:
        fail("Plasma layout must configure Kickoff favorites")
    if "install-aurelia.desktop" in layout:
        fail("installer must not be pinned for installed users")


def check_images() -> None:
    try:
        from PIL import Image
    except Exception as exc:  # noqa: BLE001
        fail(f"Pillow unavailable, cannot verify images: {exc}")
        return
    expected = {
        "usr/share/wallpapers/AureliaSolstice/contents/images/3840x2160.png": (3840, 2160),
        "usr/share/wallpapers/AureliaIvory/contents/images/3840x2160.png": (3840, 2160),
        "usr/share/sddm/themes/aurelia/background.png": (1920, 1080),
        "etc/calamares/branding/aurelia/background.png": (1920, 1080),
        "boot/grub/themes/aurelia/background.png": (1920, 1080),
    }
    for rel, size in expected.items():
        path = INCLUDE / rel
        try:
            with Image.open(path) as image:
                actual = image.size
            if actual != size:
                fail(f"image {rel} is {actual}, expected {size}")
            else:
                passed(f"image {rel} {actual[0]}x{actual[1]}")
        except Exception as exc:  # noqa: BLE001
            fail(f"image {rel}: {exc}")


def package_names() -> list[str]:
    names: list[str] = []
    for path in sorted((ROOT / "config" / "package-lists").glob("*.list.chroot")):
        for line in path.read_text(encoding="utf-8").splitlines():
            entry = line.split("#", 1)[0].strip()
            if entry:
                names.append(entry.split()[0])
    return names


def check_lean_policy() -> None:
    names = package_names()
    blocked = sorted(set(names) & LEAN_FORBIDDEN)
    if blocked:
        fail(f"lean policy forbids: {', '.join(blocked)}")
    else:
        passed("lean package policy has no optional bloat packages")

    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        fail(f"duplicate package entries: {', '.join(duplicates)}")
    else:
        passed("package lists contain no duplicate entries")

    config = (ROOT / "auto" / "config").read_text(encoding="utf-8")
    if "--apt-recommends false" not in config:
        fail("auto/config must disable APT recommends")
    else:
        passed("APT recommends are disabled")


def check_packages() -> None:
    index = ROOT / "build" / "package-index" / "available.txt"
    if not index.exists():
        print("  --  package index not present; package-name check skipped")
        return
    available = {line.strip() for line in index.read_text(encoding="utf-8").splitlines() if line.strip()}
    requested: set[str] = set()
    for path in (ROOT / "config" / "package-lists").glob("*.list.chroot"):
        for line in path.read_text(encoding="utf-8").splitlines():
            name = line.split("#", 1)[0].strip()
            if name:
                requested.add(name.split()[0])
    missing = sorted(requested - available)
    if missing:
        fail(f"package(s) absent from resolute archive: {', '.join(missing)}")
    else:
        passed(f"all {len(requested)} package names exist in resolute archive")


def main() -> int:
    print("AURELIA static validation")
    check_json()
    check_yaml()
    check_ini()
    check_shell()
    check_python()
    check_js()
    check_qml()
    check_required_payload()
    check_branding_and_installer()
    check_images()
    check_lean_policy()
    check_packages()

    print()
    if ERRORS:
        print(f"FAILED: {len(ERRORS)} problem(s), {CHECKS} checks passed")
        for error in ERRORS:
            print(f"  - {error}")
        return 1
    print(f"PASS: {CHECKS} static checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
