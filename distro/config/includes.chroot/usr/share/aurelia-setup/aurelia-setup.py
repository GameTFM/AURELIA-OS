#!/usr/bin/env python3
"""AURELIA out-of-box experience.

Seventeen ordered screens, resumable state and a final configuration pass that
uses real KDE configuration files, the system package manager and a narrowly
scoped root helper for the account, hostname, timezone, avatar, recovery
administrator and completion marker. Passwords are handed to the helper through
a mode-0600 temporary file, never through an argument list or persisted state.
"""
from __future__ import annotations

import argparse
import configparser
import getpass
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Optional

try:
    import pwd
except ImportError:  # Windows development host
    pwd = None

from PyQt6.QtCore import QProcess, QSize, Qt, QThread, pyqtSignal
from PyQt6.QtGui import QColor, QFont, QIcon, QPainter, QPixmap
from PyQt6.QtWidgets import (
    QApplication, QButtonGroup, QCheckBox, QComboBox, QFileDialog, QFrame,
    QHBoxLayout, QLabel, QLineEdit, QMainWindow, QMessageBox, QProgressBar,
    QPushButton, QRadioButton, QScrollArea, QStackedWidget, QTextEdit,
    QVBoxLayout, QWidget,
)

APP_NAME = "AURELIA Setup"
APP_VERSION = "2.0"
ICON_PATH = "/usr/share/icons/hicolor/512x512/apps/aurelia-logo.png"
ROOT_HELPER = "/usr/libexec/aurelia/aurelia-oobe-apply"
MARKER_NAME = "aurelia-setup-complete"
STATE_NAME = "aurelia-setup.json"
MIGRATION_APP = "/usr/bin/aurelia-migration"
BACKUP_APP = "/usr/bin/aurelia-backup"

STEPS = [
    "Welcome to AURELIA", "Language", "Country & region", "Keyboard",
    "Network", "Computer name", "Your previous computer",
    "Backup & transfer", "Your account", "Username & password",
    "Profile picture", "Privacy", "Appearance", "Accent colour",
    "Optional applications", "Final configuration", "Welcome to AURELIA",
]
LANGUAGES = [
    ("English (United States)", "en_US.UTF-8"), ("English (United Kingdom)", "en_GB.UTF-8"),
    ("English (India)", "en_IN.UTF-8"), ("Deutsch", "de_DE.UTF-8"),
    ("Français", "fr_FR.UTF-8"), ("Español", "es_ES.UTF-8"),
    ("Português (Brasil)", "pt_BR.UTF-8"), ("日本語", "ja_JP.UTF-8"),
    ("हिन्दी", "hi_IN.UTF-8"), ("中文（简体）", "zh_CN.UTF-8"),
]
COUNTRIES = [
    ("United States", "en_US.UTF-8", "America/New_York"),
    ("United Kingdom", "en_GB.UTF-8", "Europe/London"),
    ("India", "en_IN.UTF-8", "Asia/Kolkata"),
    ("Canada", "en_CA.UTF-8", "America/Toronto"),
    ("Australia", "en_AU.UTF-8", "Australia/Sydney"),
    ("Germany", "de_DE.UTF-8", "Europe/Berlin"),
    ("France", "fr_FR.UTF-8", "Europe/Paris"),
    ("Spain", "es_ES.UTF-8", "Europe/Madrid"),
    ("Japan", "ja_JP.UTF-8", "Asia/Tokyo"),
    ("Brazil", "pt_BR.UTF-8", "America/Sao_Paulo"),
]
KEYBOARDS = [
    ("English (US)", "us"), ("English (UK)", "gb"), ("English (India)", "in"),
    ("Deutsch", "de"), ("Français", "fr"), ("Español", "es"),
    ("日本語", "jp"), ("Português (Brasil)", "br"),
]
ACCENTS = [
    ("Champagne", "214,184,120", "#d6b878"), ("Ocean", "73,183,228", "#49b7e4"),
    ("Sage", "134,201,164", "#86c9a4"), ("Rose", "217,140,160", "#d98ca0"),
]
OPTIONAL_APPS = [
    ("VLC media player", "apt", "vlc", "Video and audio playback for almost every format."),
    ("GIMP", "apt", "gimp", "Photo editing and image composition."),
    ("LibreOffice", "apt", "libreoffice", "Writer, Calc, Impress and the rest of the office suite."),
    ("Thunderbird", "apt", "thunderbird", "Email, calendar and newsgroups."),
    ("Krita", "apt", "krita", "Digital painting and illustration."),
    ("KeePassXC", "apt", "keepassxc", "Local, encrypted password database."),
    ("Spotify", "flatpak", "com.spotify.Client", "Music streaming (installed from Flathub)."),
    ("Steam", "flatpak", "com.valvesoftware.Steam", "Games and the Steam library (Flathub)."),
]
APP_QSS = """
QMainWindow, QWidget#setupRoot { background: #080d13; color: #edf5f8; }
QWidget { font-family: Inter, "Noto Sans", sans-serif; font-size: 13px; color: #dfeaf0; }
QWidget#sidebar { background: #0b141d; border-right: 1px solid #1d2f3c; }
QLabel#brand { color: #f1dfba; font-family: "Playfair Display"; font-size: 27px; letter-spacing: 2px; }
QLabel#brandSub { color: #78909e; font-size: 9px; letter-spacing: 3px; }
QLabel#kicker { color: #d6b878; font-size: 10px; letter-spacing: 2px; font-weight: 700; }
QLabel#pageTitle { color: #f4f7f8; font-family: "Playfair Display"; font-size: 32px; }
QLabel#pageSub { color: #8ea7b4; font-size: 13px; }
QLabel#section { color: #9eb2bd; font-size: 10px; letter-spacing: 1.6px; font-weight: 700; }
QLabel#hint { color: #8ea7b4; font-size: 12px; }
QLabel#review { color: #c7d7de; font-size: 13px; }
QLabel#heroTitle { color: #f6f1e6; font-family: "Playfair Display"; font-size: 42px; }
QLabel#heroSub { color: #9fb4bf; font-size: 14px; }
QLabel#stepItem { color: #6d8491; padding: 3px 0; font-size: 11px; }
QLabel#stepActive { color: #f0d9a8; padding: 3px 0; font-weight: 600; font-size: 11px; }
QLabel#stepDone { color: #b2c7d0; padding: 3px 0; font-size: 11px; }
QLabel#swatch { border-radius: 7px; }
QPushButton { background: #121f2a; color: #e4eef3; border: 1px solid #2a4050; border-radius: 8px; padding: 9px 15px; }
QPushButton:hover { background: #1a2e3d; border-color: #4b778d; }
QPushButton#primary { background: #c9a96a; color: #101820; border: 0; font-weight: 700; padding: 10px 22px; }
QPushButton#primary:hover { background: #e0c184; }
QPushButton:disabled { color: #566b76; border-color: #1b2a34; }
QComboBox, QLineEdit { background: #0b141d; border: 1px solid #263c4a; border-radius: 8px; padding: 9px 11px; color: #e6f0f4; min-height: 20px; }
QComboBox:focus, QLineEdit:focus { border-color: #c9a96a; }
QComboBox QAbstractItemView { background: #0e1922; color: #e6f0f4; selection-background-color: #274253; }
QProgressBar { border: 0; background: #172732; border-radius: 2px; max-height: 4px; }
QProgressBar::chunk { background: #c9a96a; border-radius: 2px; }
QRadioButton, QCheckBox { spacing: 9px; color: #d9e6eb; }
QFrame#card { background: #0c1720; border: 1px solid #1f3543; border-radius: 12px; }
QTextEdit { background: #09121a; border: 1px solid #203541; border-radius: 8px; color: #a9c0cb; font-family: "JetBrains Mono", monospace; font-size: 11px; }
QScrollArea { border: 0; background: transparent; }
"""


def config_home() -> Path:
    path = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    path.mkdir(parents=True, exist_ok=True)
    return path


def marker_path() -> Path:
    return config_home() / MARKER_NAME


def state_path() -> Path:
    return config_home() / STATE_NAME


def logo_pixmap(size: int = 140) -> QPixmap:
    path = Path(ICON_PATH)
    if path.is_file():
        pixmap = QPixmap(str(path))
        if not pixmap.isNull():
            return pixmap.scaled(QSize(size, size), Qt.AspectRatioMode.KeepAspectRatio,
                                 Qt.TransformationMode.SmoothTransformation)
    return QPixmap()


def current_user() -> str:
    return getpass.getuser()


def current_full_name() -> str:
    if pwd is not None:
        try:
            return pwd.getpwnam(current_user()).pw_gecos.split(",")[0] or current_user()
        except KeyError:
            pass
    return current_user()


def run(command: list[str], timeout: int = 30, env: Optional[dict] = None) -> tuple[int, str]:
    if not command or not shutil.which(command[0]):
        return 127, f"{command[0] if command else 'command'} is not installed"
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=timeout,
                                check=False, env=env)
        return result.returncode, (result.stderr or result.stdout or "").strip()
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 1, str(exc)


def read_ini(path: Path) -> configparser.ConfigParser:
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    try:
        parser.read_string(path.read_text(encoding="utf-8"))
    except (OSError, configparser.Error):
        pass
    return parser


def write_ini(path: Path, parser: configparser.ConfigParser) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        parser.write(handle)


def network_summary() -> str:
    if not shutil.which("nmcli"):
        return "NetworkManager is not available on this system."
    code, output = run(["nmcli", "-t", "-f", "STATE", "general"], timeout=8)
    if code != 0:
        return "Network status is unavailable right now."
    state = output.splitlines()[0] if output else "unknown"
    code, wifi = run(["nmcli", "-t", "-f", "ACTIVE,SSID", "device", "wifi"], timeout=8)
    if code == 0:
        for line in wifi.splitlines():
            active, _, ssid = line.partition(":")
            if active == "yes" and ssid:
                return f"Connected to {ssid}  ·  NetworkManager: {state}"
    return f"No Wi-Fi connection is active.  ·  NetworkManager: {state}"


def open_settings(module: str, parent: Optional[QWidget] = None) -> bool:
    for command in (["systemsettings", module], ["kcmshell6", module]):
        if shutil.which(command[0]):
            try:
                subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                return True
            except OSError:
                pass
    QMessageBox.warning(parent, "Settings not available",
                        "System Settings could not be started. Open it from the application menu later.")
    return False


def default_state() -> dict:
    return {
        "version": 2, "step": 0, "language": LANGUAGES[0][1],
        "region": COUNTRIES[0][1], "country": COUNTRIES[0][0],
        "timezone": COUNTRIES[0][2], "keyboard": KEYBOARDS[0][1],
        "device_name": socket.gethostname(), "migration_choice": "new",
        "backup_choice": "later", "full_name": current_full_name(),
        "login_name": current_user(), "theme": "dark", "accent": ACCENTS[0][0],
        "picture": "", "privacy_location": False, "privacy_crash": False,
        "optional_apps": [], "password_set": False, "recovery_admin": False,
    }


class ApplyWorker(QThread):
    progress = pyqtSignal(str)
    done = pyqtSignal(bool, list)

    def __init__(self, state: dict, password: str, password_confirm: str,
                 recovery_password: str = "") -> None:
        super().__init__()
        self.state = state
        self.password = password
        self.password_confirm = password_confirm
        self.recovery_password = recovery_password

    def _user_configuration(self) -> list[str]:
        errors: list[str] = []
        config = config_home()
        locale = read_ini(config / "plasma-localerc")
        for section, values in {
            "Formats": {"LANG": self.state["language"], "LC_TIME": self.state["region"]},
            "Translations": {"LANGUAGE": self.state["language"].split(".")[0]},
        }.items():
            if not locale.has_section(section):
                locale.add_section(section)
            for key, value in values.items():
                locale.set(section, key, value)
        write_ini(config / "plasma-localerc", locale)

        keyboard = read_ini(config / "kxkbrc")
        if not keyboard.has_section("Layout"):
            keyboard.add_section("Layout")
        keyboard.set("Layout", "Use", "true")
        keyboard.set("Layout", "LayoutList", self.state["keyboard"])
        write_ini(config / "kxkbrc", keyboard)

        kdeglobals = read_ini(config / "kdeglobals")
        if not kdeglobals.has_section("General"):
            kdeglobals.add_section("General")
        scheme = "Aurelia" if self.state["theme"] == "dark" else "BreezeLight"
        kdeglobals.set("General", "ColorScheme", scheme)
        accent = next((rgb for name, rgb, _ in ACCENTS if name == self.state["accent"]), ACCENTS[0][1])
        kdeglobals.set("General", "AccentColor", accent)
        write_ini(config / "kdeglobals", kdeglobals)
        for candidate in (scheme, "BreezeDark" if self.state["theme"] == "dark" else "BreezeLight"):
            code, detail = run(["plasma-apply-colorscheme", candidate], timeout=25)
            if code == 0:
                break
        else:
            errors.append(f"Colour scheme: {detail}")

        location = read_ini(config / "kgeolocationrc")
        if not location.has_section("General"):
            location.add_section("General")
        location.set("General", "Enabled", "true" if self.state["privacy_location"] else "false")
        write_ini(config / "kgeolocationrc", location)

        crash = read_ini(config / "kcrashrc")
        if not crash.has_section("Global"):
            crash.add_section("Global")
        crash.set("Global", "AutoSubmit", "true" if self.state["privacy_crash"] else "false")
        write_ini(config / "kcrashrc", crash)

        session = read_ini(config / "ksmserverrc")
        if not session.has_section("General"):
            session.add_section("General")
        session.set("General", "loginMode", "restorePreviousLogout")
        write_ini(config / "ksmserverrc", session)

        if self.state.get("picture"):
            try:
                source = Path(self.state["picture"])
                target = Path.home() / ".face"
                shutil.copyfile(source, target)
                os.chmod(target, 0o600)
                icon = Path.home() / ".face.icon"
                if icon.exists() or icon.is_symlink():
                    icon.unlink()
                try:
                    icon.symlink_to(target)
                except OSError:
                    shutil.copyfile(source, icon)
            except OSError as exc:
                errors.append(f"Profile picture: {exc}")

        for command in (
            ["qdbus6", "org.kde.KWin", "/KWin", "reconfigure"],
            ["qdbus6", "org.kde.plasmashell", "/PlasmaShell", "refreshCurrentShell"],
        ):
            if shutil.which(command[0]):
                run(command, timeout=8)
        return errors

    def _install_apps(self) -> list[str]:
        errors: list[str] = []
        selected = [item for item in OPTIONAL_APPS if item[0] in self.state.get("optional_apps", [])]
        apt_packages = [item[2] for item in selected if item[1] == "apt"]
        flatpak_ids = [item[2] for item in selected if item[1] == "flatpak"]
        if apt_packages:
            self.progress.emit("Installing selected applications with apt: " + ", ".join(apt_packages))
            env = dict(os.environ, DEBIAN_FRONTEND="noninteractive")
            command = ["pkexec", "apt-get", "install", "-y"] + apt_packages
            if os.geteuid() == 0:
                command = command[1:]
            code, detail = run(command, timeout=1800, env=env)
            if code != 0:
                errors.append("Application installation: " + (detail or "apt-get failed"))
            else:
                self.progress.emit("Installed: " + ", ".join(apt_packages))
        if flatpak_ids:
            if shutil.which("flatpak"):
                run(["flatpak", "remote-add", "--user", "--if-not-exists", "flathub",
                     "https://flathub.org/repo/flathub.flatpakrepo"], timeout=120)
                for app_id in flatpak_ids:
                    self.progress.emit("Installing " + app_id + " from Flathub")
                    code, detail = run(["flatpak", "install", "--user", "-y", "flathub", app_id], timeout=3600)
                    if code != 0:
                        errors.append(f"{app_id}: {detail or 'flatpak install failed'}")
            else:
                errors.append("Flatpak is not installed; the selected Flathub applications were not installed.")
        return errors

    def _root_configuration(self) -> tuple[list[str], bool]:
        payload = {
            "user": current_user(),
            "full_name": self.state["full_name"].strip(),
            "password": self.password,
            "password_confirm": self.password_confirm,
            "hostname": self.state["device_name"].strip(),
            "timezone": self.state["timezone"],
            "theme": self.state["theme"],
            "picture": self.state.get("picture", ""),
            "recovery_admin": self.state.get("recovery_admin", False),
            "recovery_name": self.state.get("recovery_name", ""),
            "recovery_password": self.recovery_password,
            "completed_at": time.time(),
        }
        handle = tempfile.NamedTemporaryFile("w", prefix="aurelia-oobe-", suffix=".json", delete=False)
        try:
            json.dump(payload, handle)
            handle.close()
            os.chmod(handle.name, 0o600)
            self.progress.emit("Applying account, computer name, time zone and boot theme…")
            command = [ROOT_HELPER, "--payload", handle.name]
            if os.geteuid() != 0:
                command = ["pkexec"] + command
            try:
                result = subprocess.run(command, capture_output=True, text=True, timeout=300, check=False)
                code = result.returncode
                stdout = result.stdout.strip()
                detail = result.stderr.strip()
            except (OSError, subprocess.TimeoutExpired) as exc:
                code, stdout, detail = 1, "", str(exc)
        finally:
            try:
                os.unlink(handle.name)
            except OSError:
                pass
        helper_ok = False
        try:
            report = json.loads(stdout.splitlines()[-1]) if stdout else {}
            helper_ok = bool(report.get("ok"))
            if helper_ok:
                for operation in report.get("operations", []):
                    self.progress.emit(operation)
                return [], True
            errors = list(report.get("errors") or [])
            if detail:
                errors.append(detail)
        except ValueError:
            errors = [detail or stdout or "The administrator operation returned no result."]
        if code != 0 and not errors:
            errors = ["Administrator approval was cancelled or the helper failed."]
        return errors, helper_ok

    def run(self) -> None:
        errors = self._user_configuration()
        root_errors, helper_ok = self._root_configuration()
        errors.extend(root_errors)
        if helper_ok:
            errors.extend(self._install_apps())
        self.done.emit(helper_ok, errors)


class SetupWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(APP_NAME)
        self.setMinimumSize(980, 680)
        self.resize(1180, 780)
        if Path(ICON_PATH).is_file():
            self.setWindowIcon(QIcon(ICON_PATH))
        self.step = 0
        self.state = default_state()
        self._load_state()
        self._picture_path = self.state.get("picture", "")
        self._password = ""
        self._password_confirm = ""
        self._recovery_password = ""
        self.completed = False
        self.worker: Optional[ApplyWorker] = None
        self._build_ui()
        resume_step = int(self.state.get("step", 0))
        if resume_step > 14:
            resume_step = 14
        if resume_step > 9 and not self._password:
            resume_step = 9
        self._set_step(max(0, min(16, resume_step)), initial=True)

    # ------------------------------------------------------------- state
    def _load_state(self) -> None:
        try:
            data = json.loads(state_path().read_text(encoding="utf-8"))
            if isinstance(data, dict):
                for key, value in data.items():
                    if key in self.state and key not in {"password_set"}:
                        self.state[key] = value
        except (OSError, ValueError):
            pass

    def _save_state(self) -> None:
        self.state["step"] = self.step
        self.state["password_set"] = bool(self._password)
        self.state["picture"] = self._picture_path
        path = state_path()
        path.parent.mkdir(parents=True, exist_ok=True)
        temp = path.with_suffix(".tmp")
        temp.write_text(json.dumps(self.state, indent=2) + "\n", encoding="utf-8")
        os.chmod(temp, 0o600)
        temp.replace(path)

    # ---------------------------------------------------------------- ui
    def _build_ui(self) -> None:
        root = QWidget()
        root.setObjectName("setupRoot")
        outer = QHBoxLayout(root)
        outer.setContentsMargins(0, 0, 0, 0)
        self.setCentralWidget(root)
        sidebar = QWidget()
        sidebar.setObjectName("sidebar")
        sidebar.setFixedWidth(278)
        side = QVBoxLayout(sidebar)
        side.setContentsMargins(28, 26, 24, 24)
        side.setSpacing(2)
        brand = QLabel("AURELIA")
        brand.setObjectName("brand")
        side.addWidget(brand)
        sub = QLabel("OPERATING SYSTEM")
        sub.setObjectName("brandSub")
        side.addWidget(sub)
        side.addSpacing(18)
        self.side_progress = QProgressBar()
        self.side_progress.setRange(0, len(STEPS))
        self.side_progress.setTextVisible(False)
        side.addWidget(self.side_progress)
        side.addSpacing(14)
        self.step_labels: list[QLabel] = []
        for index, title in enumerate(STEPS):
            label = QLabel(f"{index + 1:02d}   {title}")
            label.setObjectName("stepItem")
            side.addWidget(label)
            self.step_labels.append(label)
        side.addStretch(1)
        footer = QLabel("Setup resumes safely if it is interrupted. Passwords are never saved in the setup state.")
        footer.setObjectName("hint")
        footer.setWordWrap(True)
        side.addWidget(footer)

        body = QWidget()
        body_layout = QVBoxLayout(body)
        body_layout.setContentsMargins(44, 34, 48, 26)
        body_layout.setSpacing(0)
        self.kicker = QLabel()
        self.kicker.setObjectName("kicker")
        body_layout.addWidget(self.kicker)
        self.page_title = QLabel()
        self.page_title.setObjectName("pageTitle")
        body_layout.addWidget(self.page_title)
        body_layout.addSpacing(4)
        self.page_sub = QLabel()
        self.page_sub.setObjectName("pageSub")
        self.page_sub.setWordWrap(True)
        body_layout.addWidget(self.page_sub)
        body_layout.addSpacing(18)
        self.stack = QStackedWidget()
        body_layout.addWidget(self.stack, 1)
        body_layout.addSpacing(16)
        nav = QHBoxLayout()
        self.back_button = QPushButton("Back")
        self.back_button.clicked.connect(self._back)
        self.next_button = QPushButton("Continue")
        self.next_button.setObjectName("primary")
        self.next_button.clicked.connect(self._next)
        nav.addWidget(self.back_button)
        nav.addStretch(1)
        self.step_note = QLabel()
        self.step_note.setObjectName("hint")
        nav.addWidget(self.step_note)
        nav.addSpacing(12)
        nav.addWidget(self.next_button)
        body_layout.addLayout(nav)
        outer.addWidget(sidebar)
        outer.addWidget(body, 1)
        self._pages = [
            self._page_welcome, self._page_language, self._page_region,
            self._page_keyboard, self._page_network, self._page_device,
            self._page_migration, self._page_backup, self._page_account,
            self._page_password, self._page_picture, self._page_privacy,
            self._page_appearance, self._page_accent, self._page_apps,
            self._page_final, self._page_ready,
        ]
        for builder in self._pages:
            self.stack.addWidget(builder())
        self._restore_controls()

    def _page(self) -> tuple[QWidget, QVBoxLayout]:
        container = QWidget()
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 0, 8, 0)
        layout.setSpacing(12)
        scroll.setWidget(page)
        outer = QVBoxLayout(container)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.addWidget(scroll)
        self._last_page_layout = layout
        return container, layout

    def _card(self, layout: QVBoxLayout) -> QVBoxLayout:
        card = QFrame()
        card.setObjectName("card")
        inner = QVBoxLayout(card)
        inner.setContentsMargins(20, 16, 20, 16)
        inner.setSpacing(8)
        layout.addWidget(card)
        return inner

    def _page_welcome(self) -> QWidget:
        page, layout = self._page()
        logo = QLabel()
        pixmap = logo_pixmap(120)
        if not pixmap.isNull():
            logo.setPixmap(pixmap)
        layout.addWidget(logo)
        title = QLabel("Welcome to AURELIA.")
        title.setObjectName("heroTitle")
        layout.addWidget(title)
        sub = QLabel("A calm, local-first computer. Let's bring your files, choose how it looks, and get everything ready.")
        sub.setObjectName("heroSub")
        sub.setWordWrap(True)
        layout.addWidget(sub)
        card = self._card(layout)
        for line in ("17 short screens, about five minutes.", "Bring files and supported apps from Windows.",
                     "Encrypted backups are built in.", "Every choice can be changed later in Settings."):
            label = QLabel("—   " + line)
            label.setObjectName("hint")
            card.addWidget(label)
        layout.addStretch(1)
        return page

    def _page_language(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("DISPLAY LANGUAGE")
        section.setObjectName("section")
        layout.addWidget(section)
        self.language_combo = QComboBox()
        for name, locale in LANGUAGES:
            self.language_combo.addItem(name, locale)
        layout.addWidget(self.language_combo)
        hint = QLabel("The language and translation preference is written to your KDE locale configuration and applies to your next sign-in.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_region(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("COUNTRY, FORMATS AND TIME ZONE")
        section.setObjectName("section")
        layout.addWidget(section)
        self.country_combo = QComboBox()
        for name, locale, timezone in COUNTRIES:
            self.country_combo.addItem(f"{name}  ·  {timezone}", (name, locale, timezone))
        layout.addWidget(self.country_combo)
        hint = QLabel("Sets the regional date and number formats and the system time zone. AURELIA never sends your region anywhere.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_keyboard(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("KEYBOARD LAYOUT")
        section.setObjectName("section")
        layout.addWidget(section)
        self.keyboard_combo = QComboBox()
        for name, code in KEYBOARDS:
            self.keyboard_combo.addItem(name, code)
        layout.addWidget(self.keyboard_combo)
        self.keyboard_test = QLineEdit()
        self.keyboard_test.setPlaceholderText("Type here to test the layout")
        layout.addWidget(self.keyboard_test)
        hint = QLabel("The layout is written to KDE's keyboard configuration. You can add more layouts in System Settings.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_network(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("NETWORK CONNECTION")
        section.setObjectName("section")
        layout.addWidget(section)
        self.network_label = QLabel(network_summary())
        self.network_label.setWordWrap(True)
        layout.addWidget(self.network_label)
        row = QHBoxLayout()
        refresh = QPushButton("Refresh status")
        refresh.clicked.connect(lambda: self.network_label.setText(network_summary()))
        configure = QPushButton("Open Network Settings")
        configure.clicked.connect(lambda: open_settings("kcm_networkmanagement", self))
        row.addWidget(refresh)
        row.addWidget(configure)
        row.addStretch(1)
        layout.addLayout(row)
        hint = QLabel("A connection is recommended for optional apps and browser data, but you can finish setup offline.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_device(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("COMPUTER NAME")
        section.setObjectName("section")
        layout.addWidget(section)
        self.device_edit = QLineEdit(self.state.get("device_name", socket.gethostname()))
        self.device_edit.setPlaceholderText("aurelia-desktop")
        layout.addWidget(self.device_edit)
        hint = QLabel("This is the name other devices see on your local network. Changing it requires administrator approval at the final step.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_migration(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("BRING YOUR FILES AND APPS TO AURELIA")
        section.setObjectName("section")
        layout.addWidget(section)
        statement = QLabel("Move your important files, applications, and settings from your previous Windows computer.")
        statement.setWordWrap(True)
        layout.addWidget(statement)
        self.migration_group = QButtonGroup(page)
        options = [
            ("windows", "Transfer from Windows",
             "Scan a Windows drive or Windows backup. Files are copied; supported apps are mapped to real Linux packages."),
            ("backup", "Restore from an existing backup",
             "Open Backup & Restore and import Windows File History, a folder backup or an AURELIA snapshot."),
            ("new", "Set up as a new computer",
             "Start clean. No Windows drive is mounted, changed or formatted."),
        ]
        for index, (key, title, description) in enumerate(options):
            card = QFrame()
            card.setObjectName("card")
            inner = QVBoxLayout(card)
            inner.setContentsMargins(18, 13, 18, 13)
            radio = QRadioButton(title)
            radio.setProperty("choice", key)
            self.migration_group.addButton(radio, index)
            inner.addWidget(radio)
            detail = QLabel(description)
            detail.setObjectName("hint")
            detail.setWordWrap(True)
            inner.addWidget(detail)
            layout.addWidget(card)
        self.migration_group.button(2).setChecked(True)
        self.migration_group.buttonClicked.connect(self._migration_changed)
        self.migration_note = QLabel("The Windows installation is never deleted, formatted or modified. Mounting is read-only.")
        self.migration_note.setObjectName("hint")
        self.migration_note.setWordWrap(True)
        layout.addWidget(self.migration_note)
        self.open_migration_button = QPushButton("Open Windows Migration…")
        self.open_migration_button.clicked.connect(self._open_migration)
        self.open_migration_button.setEnabled(False)
        layout.addWidget(self.open_migration_button, alignment=Qt.AlignmentFlag.AlignLeft)
        layout.addStretch(1)
        return page

    def _page_backup(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("BACKUP AND FILE TRANSFER")
        section.setObjectName("section")
        layout.addWidget(section)
        statement = QLabel("AURELIA can protect this computer from today. Backups are encrypted incremental snapshots; restores are verified.")
        statement.setWordWrap(True)
        layout.addWidget(statement)
        self.backup_group = QButtonGroup(page)
        for index, (key, text) in enumerate((
            ("now", "Set up backups now"),
            ("later", "I'll set up Backup & Restore later"),
        )):
            radio = QRadioButton(text)
            radio.setProperty("choice", key)
            self.backup_group.addButton(radio, index)
            layout.addWidget(radio)
        self.backup_group.button(0).setChecked(True)
        row = QHBoxLayout()
        open_backup = QPushButton("Open Backup & Restore")
        open_backup.clicked.connect(self._open_backup)
        row.addWidget(open_backup)
        row.addStretch(1)
        layout.addLayout(row)
        hint = QLabel("Backup & Restore supports USB and external drives, network repositories, SFTP and cloud storage supported by restic and rclone. Windows File History can also be imported there.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_account(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("USER ACCOUNT CREATION")
        section.setObjectName("section")
        layout.addWidget(section)
        card = QFrame()
        card.setObjectName("card")
        inner = QVBoxLayout(card)
        inner.setContentsMargins(20, 16, 20, 16)
        self.login_label = QLabel(f"Login name  ·  {current_user()}")
        self.login_label.setStyleSheet("font-size: 17px; color: #f4f7f8;")
        inner.addWidget(self.login_label)
        note = QLabel("This local administrator account was created during installation. It is not a cloud account. The next screen sets its password.")
        note.setObjectName("hint")
        note.setWordWrap(True)
        inner.addWidget(note)
        layout.addWidget(card)
        name_label = QLabel("Your name")
        name_label.setObjectName("section")
        layout.addWidget(name_label)
        self.full_name_edit = QLineEdit(self.state.get("full_name", current_full_name()))
        layout.addWidget(self.full_name_edit)
        hint = QLabel("The display name appears on the lock screen and in the account menu. Multiple accounts can be added later.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        add = QPushButton("Add another account in System Settings…")
        add.clicked.connect(lambda: open_settings("kcm_users", self))
        layout.addWidget(add, alignment=Qt.AlignmentFlag.AlignLeft)
        layout.addStretch(1)
        return page

    def _page_password(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("USERNAME AND PASSWORD")
        section.setObjectName("section")
        layout.addWidget(section)
        username = QLabel(current_user())
        username.setStyleSheet("font-size: 20px; color: #f4f7f8;")
        layout.addWidget(username)
        self.password_edit = QLineEdit()
        self.password_edit.setEchoMode(QLineEdit.EchoMode.Password)
        self.password_edit.setPlaceholderText("New password (at least 8 characters)")
        self.password_edit.textChanged.connect(self._password_strength)
        layout.addWidget(self.password_edit)
        self.password_confirm = QLineEdit()
        self.password_confirm.setEchoMode(QLineEdit.EchoMode.Password)
        self.password_confirm.setPlaceholderText("Confirm password")
        layout.addWidget(self.password_confirm)
        self.password_hint = QLabel("Use at least 8 characters. A longer passphrase is stronger.")
        self.password_hint.setObjectName("hint")
        layout.addWidget(self.password_hint)
        recovery = self._card(layout)
        self.recovery_check = QCheckBox("Create a second administrator account for account recovery (recommended)")
        self.recovery_check.toggled.connect(self._toggle_recovery)
        recovery.addWidget(self.recovery_check)
        self.recovery_name = QLineEdit()
        self.recovery_name.setPlaceholderText("Recovery login name, for example aurelia-recovery")
        self.recovery_name.setEnabled(False)
        recovery.addWidget(self.recovery_name)
        self.recovery_password = QLineEdit()
        self.recovery_password.setEchoMode(QLineEdit.EchoMode.Password)
        self.recovery_password.setPlaceholderText("Recovery administrator password")
        self.recovery_password.setEnabled(False)
        recovery.addWidget(self.recovery_password)
        note = QLabel("Passwords are sent only to the local account helper, hashed by the operating system and never stored in setup state, logs or cloud services.")
        note.setObjectName("hint")
        note.setWordWrap(True)
        layout.addWidget(note)
        layout.addStretch(1)
        return page

    def _page_picture(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("PROFILE PICTURE")
        section.setObjectName("section")
        layout.addWidget(section)
        self.picture_preview = QLabel()
        self.picture_preview.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.picture_preview.setFixedSize(150, 150)
        self.picture_preview.setStyleSheet("border: 1px dashed #35515f; border-radius: 75px; color: #8ea7b4;")
        layout.addWidget(self.picture_preview, alignment=Qt.AlignmentFlag.AlignLeft)
        row = QHBoxLayout()
        choose = QPushButton("Choose picture…")
        choose.clicked.connect(self._choose_picture)
        clear = QPushButton("Use initials")
        clear.clicked.connect(self._clear_picture)
        row.addWidget(choose)
        row.addWidget(clear)
        row.addStretch(1)
        layout.addLayout(row)
        hint = QLabel("The picture is copied to your local account as ~/.face and the AccountsService icon. It is never uploaded.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        self._refresh_picture()
        layout.addStretch(1)
        return page

    def _page_privacy(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("PRIVACY")
        section.setObjectName("section")
        layout.addWidget(section)
        statement = QLabel("AURELIA includes no telemetry client and no advertising ID. Your files, browser history and notes stay on this computer unless you choose to share them.")
        statement.setWordWrap(True)
        layout.addWidget(statement)
        self.location_check = QCheckBox("Allow applications to request my location")
        self.location_check.setChecked(self.state.get("privacy_location", False))
        layout.addWidget(self.location_check)
        self.crash_check = QCheckBox("Submit anonymous crash reports with KDE's crash handler")
        self.crash_check.setChecked(self.state.get("privacy_crash", False))
        layout.addWidget(self.crash_check)
        hint = QLabel("These choices are stored in KDE's own configuration. More permissions can be reviewed in System Settings → Privacy.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        open_privacy = QPushButton("Open Privacy Settings")
        open_privacy.clicked.connect(lambda: open_settings("kcm_privacy", self))
        layout.addWidget(open_privacy, alignment=Qt.AlignmentFlag.AlignLeft)
        layout.addStretch(1)
        return page

    def _page_appearance(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("LIGHT OR DARK APPEARANCE")
        section.setObjectName("section")
        layout.addWidget(section)
        self.dark_radio = QRadioButton("Midnight — AURELIA's deep dark theme")
        self.light_radio = QRadioButton("Ivory — a bright, high-contrast theme")
        group = QButtonGroup(page)
        group.addButton(self.dark_radio)
        group.addButton(self.light_radio)
        layout.addWidget(self.dark_radio)
        layout.addWidget(self.light_radio)
        hint = QLabel("Applies a real KDE colour scheme and selects the matching boot theme. The desktop layout is preserved.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_accent(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("ACCENT COLOUR")
        section.setObjectName("section")
        layout.addWidget(section)
        self.accent_group = QButtonGroup(page)
        for index, (name, rgb, swatch) in enumerate(ACCENTS):
            row = QHBoxLayout()
            square = QLabel()
            square.setFixedSize(16, 16)
            square.setStyleSheet(f"background: {swatch}; border-radius: 8px;")
            radio = QRadioButton(name)
            radio.setProperty("accent_rgb", rgb)
            self.accent_group.addButton(radio, index)
            if index == 0:
                radio.setChecked(True)
            row.addWidget(square)
            row.addWidget(radio)
            row.addStretch(1)
            layout.addLayout(row)
        hint = QLabel("The accent is written to KDE's colour configuration and applied to focus, selection and highlights.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_apps(self) -> QWidget:
        page, layout = self._page()
        section = QLabel("OPTIONAL APPLICATIONS")
        section.setObjectName("section")
        layout.addWidget(section)
        hint = QLabel("AURELIA already includes NOVAIR, Files, Settings, Backup & Restore and the desktop applications. Choose extra apps you want installed now; every installation is reported truthfully.")
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        self.app_checks: dict[str, QCheckBox] = {}
        for name, kind, identifier, description in OPTIONAL_APPS:
            check = QCheckBox(f"{name}   ·   {kind}")
            check.setToolTip(description)
            self.app_checks[name] = check
            layout.addWidget(check)
        layout.addStretch(1)
        return page

    def _page_final(self) -> QWidget:
        page, layout = self._page()
        self.final_title = QLabel("Getting everything ready for you.")
        self.final_title.setObjectName("heroTitle")
        layout.addWidget(self.final_title)
        self.final_sub = QLabel("Applying your account, preferences, apps and boot theme. Nothing is reported as complete until the operation actually succeeds.")
        self.final_sub.setObjectName("heroSub")
        self.final_sub.setWordWrap(True)
        layout.addWidget(self.final_sub)
        self.final_progress = QProgressBar()
        self.final_progress.setRange(0, 0)
        layout.addWidget(self.final_progress)
        self.final_log = QTextEdit()
        self.final_log.setReadOnly(True)
        self.final_log.setMinimumHeight(220)
        layout.addWidget(self.final_log)
        self.retry_button = QPushButton("Retry configuration")
        self.retry_button.clicked.connect(self._start_finalisation)
        self.retry_button.hide()
        layout.addWidget(self.retry_button, alignment=Qt.AlignmentFlag.AlignLeft)
        return page

    def _page_ready(self) -> QWidget:
        page, layout = self._page()
        title = QLabel("Welcome to AURELIA.")
        title.setObjectName("heroTitle")
        layout.addWidget(title)
        sub = QLabel("Your computer is ready. Here is where everything lives.")
        sub.setObjectName("heroSub")
        sub.setWordWrap(True)
        layout.addWidget(sub)
        card = self._card(layout)
        for line in (
            "Start menu — lower-left launcher for every installed application.",
            "Taskbar — pinned apps, running windows, network, sound and power.",
            "Desktop widgets — clock, calendar, weather, notes and system monitor.",
            "NOVAIR — the AURELIA browser, with imported bookmarks and history.",
            "Files — browse your home, mounted drives and migrated folders.",
            "Settings — appearance, accounts, privacy, network and display.",
            "Backup & Restore — encrypted backups and Windows backup import.",
        ):
            item = QLabel("—   " + line)
            item.setObjectName("hint")
            item.setWordWrap(True)
            card.addWidget(item)
        layout.addStretch(1)
        return page

    # ----------------------------------------------------------- controls
    def _restore_controls(self) -> None:
        def set_combo(combo: QComboBox, value) -> None:
            index = combo.findData(value)
            if index >= 0:
                combo.setCurrentIndex(index)

        set_combo(self.language_combo, self.state.get("language"))
        country_value = next((item for item in COUNTRIES if item[0] == self.state.get("country")), COUNTRIES[0])
        set_combo(self.country_combo, country_value)
        set_combo(self.keyboard_combo, self.state.get("keyboard"))
        self.dark_radio.setChecked(self.state.get("theme") != "light")
        self.light_radio.setChecked(self.state.get("theme") == "light")
        for index, (name, _rgb, _swatch) in enumerate(ACCENTS):
            if name == self.state.get("accent"):
                self.accent_group.button(index).setChecked(True)
        choice = self.state.get("migration_choice", "new")
        for button in self.migration_group.buttons():
            if button.property("choice") == choice:
                button.setChecked(True)
        self._migration_changed()
        choice = self.state.get("backup_choice", "now")
        for button in self.backup_group.buttons():
            if button.property("choice") == choice:
                button.setChecked(True)
        self.full_name_edit.setText(self.state.get("full_name", current_full_name()))
        for name, check in self.app_checks.items():
            check.setChecked(name in self.state.get("optional_apps", []))
        self.location_check.setChecked(self.state.get("privacy_location", False))
        self.crash_check.setChecked(self.state.get("privacy_crash", False))

    def _migration_changed(self) -> None:
        button = self.migration_group.checkedButton()
        key = button.property("choice") if button else "new"
        self.open_migration_button.setEnabled(key == "windows")
        if key == "windows":
            self.migration_note.setText("Open the migration wizard to choose files and apps. The Windows volume is mounted read-only and is never modified.")
        elif key == "backup":
            self.migration_note.setText("Use Backup & Restore to scan a Windows File History, folder backup or mounted image before restoring.")
        else:
            self.migration_note.setText("No Windows drive will be touched. You can migrate later from Backup & Restore or the Windows Migration app.")

    def _open_migration(self) -> None:
        if not Path(MIGRATION_APP).is_file():
            QMessageBox.warning(self, "Migration unavailable", "The Windows Migration application is not installed in this image.")
            return
        if not QProcess.startDetached(MIGRATION_APP, []):
            QMessageBox.warning(self, "Migration unavailable", "The Windows Migration application could not be started.")

    def _open_backup(self) -> None:
        if not Path(BACKUP_APP).is_file():
            QMessageBox.warning(self, "Backup unavailable", "Backup & Restore is not installed in this image.")
            return
        if not QProcess.startDetached(BACKUP_APP, []):
            QMessageBox.warning(self, "Backup unavailable", "Backup & Restore could not be started.")

    def _toggle_recovery(self, enabled: bool) -> None:
        self.recovery_name.setEnabled(enabled)
        self.recovery_password.setEnabled(enabled)

    def _password_strength(self, text: str) -> None:
        score = 0
        score += 1 if len(text) >= 8 else 0
        score += 1 if len(text) >= 14 else 0
        score += 1 if re.search(r"[A-Z]", text) and re.search(r"[a-z]", text) else 0
        score += 1 if re.search(r"\d", text) and re.search(r"[^A-Za-z0-9]", text) else 0
        labels = ["Too short", "Fair", "Good", "Strong", "Very strong"]
        self.password_hint.setText(labels[min(score, 4)] + " · Use at least 8 characters; a long passphrase is stronger.")

    def _initials_pixmap(self) -> QPixmap:
        pixmap = QPixmap(150, 150)
        pixmap.fill(Qt.GlobalColor.transparent)
        painter = QPainter(pixmap)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setBrush(QColor("#c9a96a"))
        painter.setPen(Qt.PenStyle.NoPen)
        painter.drawEllipse(0, 0, 150, 150)
        painter.setPen(QColor("#101820"))
        font = QFont("Inter", 44)
        font.setBold(True)
        painter.setFont(font)
        initials = "".join(word[0] for word in current_full_name().split()[:2]).upper() or "A"
        painter.drawText(pixmap.rect(), Qt.AlignmentFlag.AlignCenter, initials)
        painter.end()
        return pixmap

    def _refresh_picture(self) -> None:
        if self._picture_path and Path(self._picture_path).is_file():
            pixmap = QPixmap(self._picture_path)
            if not pixmap.isNull():
                self.picture_preview.setPixmap(pixmap.scaled(150, 150, Qt.AspectRatioMode.KeepAspectRatioByExpanding,
                                                             Qt.TransformationMode.SmoothTransformation))
                return
        self.picture_preview.setPixmap(self._initials_pixmap())

    def _choose_picture(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Choose a profile picture", str(Path.home()),
                                              "Images (*.png *.jpg *.jpeg *.webp *.bmp)")
        if path:
            self._picture_path = path
            self.state["picture"] = path
            self._refresh_picture()

    def _clear_picture(self) -> None:
        self._picture_path = ""
        self.state["picture"] = ""
        self._refresh_picture()

    # ---------------------------------------------------------- navigation
    def _set_step(self, index: int, initial: bool = False) -> None:
        self.step = max(0, min(len(STEPS) - 1, index))
        self.stack.setCurrentIndex(self.step)
        self.side_progress.setValue(self.step + (1 if self.step == len(STEPS) - 1 else 0))
        self.kicker.setText(f"STEP {self.step + 1:02d} OF {len(STEPS):02d}")
        self.page_title.setText(STEPS[self.step])
        descriptions = {
            0: "A quiet, local-first start for your new computer.",
            1: "Choose the language you want AURELIA to use.",
            2: "Set your formats, region and time zone.",
            3: "Choose the keyboard you will type on.",
            4: "Connect to a network, or continue offline.",
            5: "Give this computer a name on your network.",
            6: "Bring your files and apps, or start new.",
            7: "Set up encrypted backups and file transfer.",
            8: "Review the local administrator account.",
            9: "Set the password that protects this computer.",
            10: "Add a face to your account.",
            11: "Choose what stays private.",
            12: "Choose how AURELIA looks.",
            13: "Pick the colour used for focus and selection.",
            14: "Install a few useful applications.",
            15: "Getting everything ready for you.",
            16: "Your desktop is ready.",
        }
        self.page_sub.setText(descriptions.get(self.step, ""))
        for index, label in enumerate(self.step_labels):
            label.setObjectName("stepActive" if index == self.step else ("stepDone" if index < self.step else "stepItem"))
            label.style().unpolish(label)
            label.style().polish(label)
        self.back_button.setEnabled(
            self.step > 0 and not self.completed
            and not (self.step == 15 and self.worker is not None and self.worker.isRunning())
        )
        if self.step == 0:
            self.next_button.setText("Begin")
        elif self.step == 15:
            self.next_button.setText("Continue" if self.completed else "Preparing…")
        elif self.step == 16:
            self.next_button.setText("Enter AURELIA")
        else:
            self.next_button.setText("Continue")
            self.next_button.setEnabled(True)
        if self.step == 15:
            self.next_button.setEnabled(self.completed)
        elif self.step == 16:
            self.next_button.setEnabled(True)
        self.step_note.setText(f"{self.step + 1} / {len(STEPS)}")
        if self.step == 15 and not initial:
            self._start_finalisation()

    def _collect(self) -> bool:
        if self.step == 1:
            self.state["language"] = self.language_combo.currentData()
        elif self.step == 2:
            name, locale, timezone = self.country_combo.currentData()
            self.state.update(country=name, region=locale, timezone=timezone)
        elif self.step == 3:
            self.state["keyboard"] = self.keyboard_combo.currentData()
        elif self.step == 5:
            name = self.device_edit.text().strip()
            if not name or len(name) > 63 or not re.match(r"^[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?$", name):
                QMessageBox.warning(self, "Computer name", "Use 1–63 letters, digits or hyphens; start and end with a letter or digit.")
                return False
            self.state["device_name"] = name
        elif self.step == 6:
            button = self.migration_group.checkedButton()
            self.state["migration_choice"] = button.property("choice") if button else "new"
            if self.state["migration_choice"] == "windows" and not Path(MIGRATION_APP).is_file():
                QMessageBox.warning(self, "Migration unavailable", "The Windows Migration application is missing from this image.")
                return False
        elif self.step == 7:
            button = self.backup_group.checkedButton()
            self.state["backup_choice"] = button.property("choice") if button else "later"
        elif self.step == 8:
            name = self.full_name_edit.text().strip()
            if not name:
                QMessageBox.warning(self, "Your name", "Enter your display name.")
                return False
            self.state["full_name"] = name
        elif self.step == 9:
            if not self._validate_passwords():
                return False
        elif self.step == 11:
            self.state["privacy_location"] = self.location_check.isChecked()
            self.state["privacy_crash"] = self.crash_check.isChecked()
        elif self.step == 12:
            self.state["theme"] = "light" if self.light_radio.isChecked() else "dark"
        elif self.step == 13:
            button = self.accent_group.checkedButton()
            self.state["accent"] = button.text() if button else ACCENTS[0][0]
        elif self.step == 14:
            self.state["optional_apps"] = [name for name, check in self.app_checks.items() if check.isChecked()]
        elif self.step == 10 and not self._picture_path:
            try:
                avatar = config_home() / "aurelia-avatar.png"
                if self._initials_pixmap().save(str(avatar), "PNG"):
                    os.chmod(avatar, 0o600)
                    self._picture_path = str(avatar)
                    self.state["picture"] = str(avatar)
            except OSError:
                pass
        return True

    def _validate_passwords(self) -> bool:
        password = self.password_edit.text()
        confirm = self.password_confirm.text()
        if len(password) < 8 or any(ord(char) < 32 for char in password):
            QMessageBox.warning(self, "Password", "Use at least 8 characters and avoid control characters.")
            return False
        if password != confirm:
            QMessageBox.warning(self, "Password", "The two passwords do not match.")
            return False
        self._password = password
        self._password_confirm = confirm
        self._recovery_password = ""
        recovery = self.recovery_check.isChecked()
        if recovery:
            name = self.recovery_name.text().strip()
            recovery_password = self.recovery_password.text()
            if not re.match(r"^[a-z_][a-z0-9_-]{2,31}$", name):
                QMessageBox.warning(self, "Recovery account", "Use lowercase letters, digits, underscore or hyphen (3–32 characters).")
                return False
            if name == current_user():
                QMessageBox.warning(self, "Recovery account", "Choose a different login name from your main account.")
                return False
            if len(recovery_password) < 8 or any(ord(char) < 32 for char in recovery_password):
                QMessageBox.warning(self, "Recovery account", "Use a recovery password of at least 8 characters without control characters.")
                return False
            self.state["recovery_admin"] = True
            self.state["recovery_name"] = name
            self._recovery_password = recovery_password
        else:
            self.state["recovery_admin"] = False
            self.state["recovery_name"] = ""
        return True

    def _next(self) -> None:
        if self.completed and self.step == 15:
            self._set_step(16)
            self._save_state()
            return
        if self.step == 16:
            self._save_state()
            self.close()
            QApplication.quit()
            return
        if not self._collect():
            return
        self._save_state()
        self._set_step(self.step + 1)

    def _back(self) -> None:
        if self.worker is not None and self.worker.isRunning():
            return
        if self.step == 15:
            return
        self._save_state()
        self._set_step(self.step - 1)

    # ------------------------------------------------------------ finalise
    def _start_finalisation(self) -> None:
        if self.worker is not None and self.worker.isRunning():
            return
        if not self._password:
            QMessageBox.warning(self, "Password required", "Go back to the password screen and choose a password before finishing.")
            self._set_step(9)
            return
        self.completed = False
        self.retry_button.hide()
        self.next_button.setText("Preparing…")
        self.next_button.setEnabled(False)
        self.back_button.setEnabled(False)
        self.final_log.clear()
        self.final_progress.setRange(0, 0)
        self.final_log.append("Starting final configuration. Administrator approval will be requested once.")
        self.worker = ApplyWorker(dict(self.state), self._password, self._password_confirm, self._recovery_password)
        self.worker.progress.connect(self._append_log)
        self.worker.done.connect(self._final_done)
        self.worker.start()

    def _append_log(self, message: str) -> None:
        self.final_log.append(message)

    def _final_done(self, helper_ok: bool, errors: list) -> None:
        self.final_progress.setRange(0, 1)
        self.final_progress.setValue(1)
        if not helper_ok:
            self.final_log.append("\nConfiguration could not be completed. No completion marker was written.")
            for error in errors:
                self.final_log.append("• " + str(error))
            self.retry_button.show()
            self.next_button.setEnabled(False)
            self.back_button.setEnabled(False)
            return
        self._save_state()
        marker = marker_path()
        try:
            marker.write_text(json.dumps({"completed_at": time.time(), **self.state}, indent=2) + "\n", encoding="utf-8")
            os.chmod(marker, 0o600)
        except OSError as exc:
            self.completed = False
            self.final_log.append("\nSystem changes were applied, but the local completion marker could not be written: " + str(exc))
            self.retry_button.show()
            self.next_button.setText("Preparing…")
            self.next_button.setEnabled(False)
            self.back_button.setEnabled(False)
            return
        self.completed = True
        for message in ("Account and password configured.", "Computer name and time zone applied.",
                        "Desktop preferences, privacy choices and session restore applied.",
                        "Boot theme updated for the next start."):
            self.final_log.append(message)
        if errors:
            self.final_log.append("\nSetup finished with these warnings:")
            for error in errors:
                self.final_log.append("• " + str(error))
            self.final_log.append("You can retry optional installations from the application menu or Settings.")
        else:
            self.final_log.append("\nEverything requested was applied successfully.")
        self.final_title.setText("Your AURELIA computer is ready.")
        self.final_sub.setText("Setup is complete. Continue to the welcome screen.")
        self.next_button.setText("Continue")
        self.next_button.setEnabled(True)
        self.back_button.setEnabled(False)
        self._password = ""
        self._password_confirm = ""
        self._recovery_password = ""
        self.password_edit.clear()
        self.password_confirm.clear()
        self.recovery_password.clear()


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="AURELIA out-of-box experience")
    parser.add_argument("--version", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.version:
        print(APP_VERSION)
        return 0
    if args.self_test:
        failures = []
        if len(STEPS) != 17:
            failures.append("OOBE does not have 17 screens")
        for key in ("language", "region", "timezone", "keyboard", "device_name", "migration_choice",
                    "full_name", "theme", "accent", "optional_apps"):
            if key not in default_state():
                failures.append(f"OOBE state lacks {key}")
        if failures:
            print("FAILED: " + "; ".join(failures))
            return 1
        print("AURELIA OOBE self-test: 17 screens, persistent state schema OK")
        return 0
    if marker_path().is_file():
        print("AURELIA setup has already been completed for this user.")
        return 0
    app = QApplication([sys.argv[0]])
    app.setApplicationName(APP_NAME)
    app.setApplicationVersion(APP_VERSION)
    app.setOrganizationName("AURELIA")
    app.setStyleSheet(APP_QSS)
    if Path(ICON_PATH).is_file():
        app.setWindowIcon(QIcon(ICON_PATH))
    window = SetupWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
