#!/usr/bin/env python3
"""AURELIA first-login setup.

This is a real PyQt6 setup surface. It writes KDE configuration files, can
apply an installed colour scheme, can set the hostname through polkit, can
copy a profile picture, and can open the real KDE account, network and privacy
settings applications. It never displays a control that has no effect.
"""
from __future__ import annotations

import argparse
import configparser
import getpass
import json
import os
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

try:
    import pwd
except ImportError:  # Windows development host
    pwd = None

from PyQt6.QtCore import QSize, Qt, QTimer
from PyQt6.QtGui import QIcon, QPixmap
from PyQt6.QtWidgets import (
    QApplication,
    QButtonGroup,
    QComboBox,
    QFileDialog,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QRadioButton,
    QSizePolicy,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)

APP_NAME = "AURELIA Setup"
APP_VERSION = "1.0"
ICON_PATH = "/usr/share/icons/hicolor/512x512/apps/aurelia-logo.png"
MARKER_NAME = "aurelia-setup-complete"
STATE_NAME = "aurelia-setup.json"

STEPS = [
    "Welcome",
    "Region",
    "Keyboard",
    "Wi-Fi",
    "Device",
    "Account",
    "Sign-in",
    "Picture",
    "Privacy",
    "Appearance",
    "Accent",
    "Review",
    "Ready",
]
LOCALES = [
    ("United States — English (US)", "en_US.UTF-8"),
    ("United Kingdom — English (UK)", "en_GB.UTF-8"),
    ("India — English (India)", "en_IN.UTF-8"),
    ("Canada — English (Canada)", "en_CA.UTF-8"),
    ("Australia — English (Australia)", "en_AU.UTF-8"),
    ("Germany — Deutsch", "de_DE.UTF-8"),
    ("France — Français", "fr_FR.UTF-8"),
    ("Spain — Español", "es_ES.UTF-8"),
    ("Japan — 日本語", "ja_JP.UTF-8"),
    ("Brazil — Português", "pt_BR.UTF-8"),
]
KEYBOARDS = [
    ("English (US)", "us"),
    ("English (UK)", "gb"),
    ("English (India)", "in"),
    ("Deutsch", "de"),
    ("Français", "fr"),
    ("Español", "es"),
    ("日本語", "jp"),
    ("Português (Brasil)", "br"),
]
ACCENTS = [
    ("Champagne", "214,184,120", "#d6b878"),
    ("Ocean", "73,183,228", "#49b7e4"),
    ("Sage", "134,201,164", "#86c9a4"),
    ("Rose", "217,140,160", "#d98ca0"),
]

APP_QSS = """
QMainWindow, QWidget#setupRoot { background: #080d13; color: #edf5f8; }
QWidget { font-family: Inter, "Noto Sans", sans-serif; font-size: 13px; color: #dfeaf0; }
QWidget#sidebar { background: #0b141d; border-right: 1px solid #1d2f3c; }
QLabel#brand { color: #f1dfba; font-family: "Playfair Display"; font-size: 28px;
  letter-spacing: 2px; }
QLabel#brandSub { color: #78909e; font-size: 10px; letter-spacing: 3px; }
QLabel#kicker { color: #d6b878; font-size: 10px; letter-spacing: 2px; font-weight: 700; }
QLabel#pageTitle { color: #f4f7f8; font-family: "Playfair Display"; font-size: 34px; }
QLabel#pageSub { color: #8ea7b4; font-size: 13px; }
QLabel#section { color: #9eb2bd; font-size: 11px; letter-spacing: 1.6px; font-weight: 700; }
QLabel#hint { color: #8ea7b4; font-size: 12px; }
QLabel#review { color: #c7d7de; font-size: 13px; line-height: 1.7; }
QLabel#heroTitle { color: #f6f1e6; font-family: "Playfair Display"; font-size: 44px; }
QLabel#heroSub { color: #9fb4bf; font-size: 14px; }
QLabel#stepItem { color: #6d8491; padding: 5px 0; }
QLabel#stepActive { color: #f0d9a8; padding: 5px 0; font-weight: 600; }
QLabel#stepDone { color: #b2c7d0; padding: 5px 0; }
QLabel#swatch { border-radius: 7px; min-width: 14px; max-width: 14px;
  min-height: 14px; max-height: 14px; }
QPushButton { background: #121f2a; color: #e4eef3; border: 1px solid #2a4050;
  border-radius: 8px; padding: 9px 16px; }
QPushButton:hover { background: #1a2e3d; border-color: #4b778d; }
QPushButton#primary { background: #c9a96a; color: #101820; border: 0;
  font-weight: 700; padding: 10px 22px; }
QPushButton#primary:hover { background: #e0c184; }
QPushButton:disabled { color: #566b76; border-color: #1b2a34; }
QComboBox, QLineEdit { background: #0b141d; border: 1px solid #263c4a;
  border-radius: 8px; padding: 9px 11px; color: #e6f0f4; min-height: 20px; }
QComboBox:focus, QLineEdit:focus { border-color: #c9a96a; }
QComboBox QAbstractItemView { background: #0e1922; color: #e6f0f4;
  selection-background-color: #274253; }
QProgressBar { border: 0; background: #172732; border-radius: 2px; max-height: 4px; }
QProgressBar::chunk { background: #c9a96a; border-radius: 2px; }
QRadioButton { spacing: 9px; color: #d9e6eb; }
QRadioButton::indicator { width: 15px; height: 15px; }
QRadioButton::indicator:unchecked { border: 1px solid #4b6573; border-radius: 8px;
  background: #0b141d; }
QRadioButton::indicator:checked { border: 2px solid #d6b878; border-radius: 8px;
  background: #d6b878; }
QFrame#card { background: #0c1720; border: 1px solid #1f3543; border-radius: 12px; }
"""


def config_home() -> Path:
    path = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    path.mkdir(parents=True, exist_ok=True)
    return path


def marker_path() -> Path:
    return config_home() / MARKER_NAME


def state_path() -> Path:
    return config_home() / STATE_NAME


def logo_pixmap(size: int = 150) -> QPixmap:
    path = Path(ICON_PATH)
    if path.is_file():
        pixmap = QPixmap(str(path))
        if not pixmap.isNull():
            return pixmap.scaled(
                QSize(size, size), Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation,
            )
    return QPixmap()


def current_user() -> str:
    return getpass.getuser()


def current_full_name() -> str:
    if pwd is not None:
        try:
            entry = pwd.getpwnam(current_user())
            return entry.pw_gecos.split(",")[0] or current_user()
        except KeyError:
            pass
    return current_user()


def run_command(command: list[str], timeout: int = 25) -> tuple[int, str]:
    if not command or not shutil.which(command[0]):
        return 127, f"{command[0] if command else 'command'} is not installed"
    try:
        result = subprocess.run(
            command, capture_output=True, text=True, timeout=timeout, check=False
        )
        detail = (result.stderr or result.stdout or "").strip()
        return result.returncode, detail
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
    code, output = run_command(["nmcli", "-t", "-f", "STATE", "general"], timeout=8)
    if code != 0:
        return "Network status is unavailable right now."
    state = output.splitlines()[0] if output else "unknown"
    code, wifi = run_command(
        ["nmcli", "-t", "-f", "ACTIVE,SSID", "device", "wifi"], timeout=8
    )
    if code == 0:
        for line in wifi.splitlines():
            active, _, ssid = line.partition(":")
            if active == "yes" and ssid:
                return f"Connected to {ssid}  ·  NetworkManager: {state}"
    return f"No Wi-Fi connection is active.  ·  NetworkManager: {state}"


def open_kde_settings(module: str, parent: Optional[QWidget] = None) -> bool:
    for command in (["systemsettings", module], ["kcmshell6", module]):
        if shutil.which(command[0]):
            try:
                subprocess.Popen(command, stdout=subprocess.DEVNULL,
                                 stderr=subprocess.DEVNULL)
                return True
            except OSError:
                pass
    QMessageBox.warning(
        parent,
        "Settings not available",
        "The KDE settings application could not be started. You can open "
        "System Settings from the application menu after setup.",
    )
    return False


class SetupWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("AURELIA Setup")
        self.setMinimumSize(900, 620)
        self.resize(1080, 720)
        if Path(ICON_PATH).is_file():
            self.setWindowIcon(QIcon(ICON_PATH))
        self.step = 0
        self.state = {
            "region": LOCALES[0][1],
            "keyboard": KEYBOARDS[0][1],
            "device_name": socket.gethostname(),
            "theme": "dark",
            "accent": ACCENTS[0][0],
            "picture": "",
        }
        self._picture_path = ""
        self._build_ui()
        self._set_step(0)

    def _build_ui(self) -> None:
        root = QWidget()
        root.setObjectName("setupRoot")
        outer = QHBoxLayout(root)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)
        self.setCentralWidget(root)

        sidebar = QWidget()
        sidebar.setObjectName("sidebar")
        sidebar.setFixedWidth(285)
        side = QVBoxLayout(sidebar)
        side.setContentsMargins(30, 30, 28, 26)
        side.setSpacing(3)
        brand = QLabel("AURELIA")
        brand.setObjectName("brand")
        side.addWidget(brand)
        sub = QLabel("OPERATING SYSTEM")
        sub.setObjectName("brandSub")
        side.addWidget(sub)
        side.addSpacing(24)
        self.side_progress = QProgressBar()
        self.side_progress.setRange(0, len(STEPS) - 1)
        self.side_progress.setTextVisible(False)
        side.addWidget(self.side_progress)
        side.addSpacing(20)
        self.step_labels: list[QLabel] = []
        for index, title in enumerate(STEPS):
            label = QLabel(f"{index + 1:02d}   {title}")
            label.setObjectName("stepItem")
            side.addWidget(label)
            self.step_labels.append(label)
        side.addStretch(1)
        footer = QLabel("Your settings are stored locally on this device.")
        footer.setObjectName("hint")
        footer.setWordWrap(True)
        side.addWidget(footer)

        body = QWidget()
        body_layout = QVBoxLayout(body)
        body_layout.setContentsMargins(52, 42, 56, 34)
        body_layout.setSpacing(0)
        self.kicker = QLabel()
        self.kicker.setObjectName("kicker")
        body_layout.addWidget(self.kicker)
        self.page_title = QLabel()
        self.page_title.setObjectName("pageTitle")
        body_layout.addWidget(self.page_title)
        body_layout.addSpacing(6)
        self.page_sub = QLabel()
        self.page_sub.setObjectName("pageSub")
        self.page_sub.setWordWrap(True)
        body_layout.addWidget(self.page_sub)
        body_layout.addSpacing(26)
        self.stack = QStackedWidget()
        body_layout.addWidget(self.stack, 1)
        body_layout.addSpacing(22)
        navigation = QHBoxLayout()
        self.back_button = QPushButton("Back")
        self.back_button.clicked.connect(self._back)
        self.next_button = QPushButton("Continue")
        self.next_button.setObjectName("primary")
        self.next_button.clicked.connect(self._next)
        navigation.addWidget(self.back_button)
        navigation.addStretch(1)
        self.step_note = QLabel()
        self.step_note.setObjectName("hint")
        navigation.addWidget(self.step_note)
        navigation.addSpacing(14)
        navigation.addWidget(self.next_button)
        body_layout.addLayout(navigation)

        outer.addWidget(sidebar)
        outer.addWidget(body, 1)
        self._pages = [
            self._page_welcome,
            self._page_region,
            self._page_keyboard,
            self._page_network,
            self._page_device,
            self._page_account,
            self._page_signin,
            self._page_picture,
            self._page_privacy,
            self._page_appearance,
            self._page_accent,
            self._page_review,
            self._page_ready,
        ]
        for builder in self._pages:
            self.stack.addWidget(builder())

    def _page_widget(self) -> tuple[QWidget, QVBoxLayout]:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(14)
        return page, layout

    def _page_welcome(self) -> QWidget:
        page, layout = self._page_widget()
        logo = QLabel()
        pixmap = logo_pixmap(130)
        if not pixmap.isNull():
            logo.setPixmap(pixmap)
        layout.addWidget(logo, 0, Qt.AlignmentFlag.AlignLeft)
        title = QLabel("Welcome to AURELIA.")
        title.setObjectName("heroTitle")
        layout.addWidget(title)
        sub = QLabel(
            "Let's make this computer yours. The installer has already created "
            "your account and installed the system. These choices set up your "
            "language, keyboard, appearance and first connection."
        )
        sub.setObjectName("heroSub")
        sub.setWordWrap(True)
        layout.addWidget(sub)
        card = QFrame()
        card.setObjectName("card")
        card_layout = QVBoxLayout(card)
        card_layout.setContentsMargins(24, 20, 24, 20)
        card_layout.setSpacing(9)
        for line in (
            "13 guided screens, about two minutes.",
            "No account is sent to a cloud service.",
            "Every setting can be changed later in System Settings.",
        ):
            label = QLabel("—   " + line)
            label.setObjectName("hint")
            card_layout.addWidget(label)
        layout.addWidget(card)
        layout.addStretch(1)
        return page

    def _page_region(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Country and region")
        label.setObjectName("section")
        layout.addWidget(label)
        self.region_combo = QComboBox()
        for name, locale in LOCALES:
            self.region_combo.addItem(name, locale)
        layout.addWidget(self.region_combo)
        hint = QLabel(
            "AURELIA uses this for date, number and language formats. It does "
            "not send your region anywhere."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_keyboard(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Keyboard layout")
        label.setObjectName("section")
        layout.addWidget(label)
        self.keyboard_combo = QComboBox()
        for name, layout_name in KEYBOARDS:
            self.keyboard_combo.addItem(name, layout_name)
        layout.addWidget(self.keyboard_combo)
        hint = QLabel(
            "The layout is written to your KDE keyboard configuration. You can "
            "add more layouts later in System Settings → Input Devices."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_network(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Wi-Fi and network")
        label.setObjectName("section")
        layout.addWidget(label)
        self.network_label = QLabel(network_summary())
        self.network_label.setWordWrap(True)
        self.network_label.setObjectName("hint")
        layout.addWidget(self.network_label)
        row = QHBoxLayout()
        refresh = QPushButton("Refresh status")
        refresh.clicked.connect(self._refresh_network)
        configure = QPushButton("Open Network Settings")
        configure.clicked.connect(self._open_network)
        row.addWidget(refresh)
        row.addWidget(configure)
        row.addStretch(1)
        layout.addLayout(row)
        note = QLabel(
            "You can continue without a connection. A wired connection is used "
            "automatically if one is available."
        )
        note.setObjectName("hint")
        note.setWordWrap(True)
        layout.addWidget(note)
        layout.addStretch(1)
        return page

    def _page_device(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Device name")
        label.setObjectName("section")
        layout.addWidget(label)
        self.device_edit = QLineEdit(socket.gethostname())
        self.device_edit.setPlaceholderText("aurelia-desktop")
        layout.addWidget(self.device_edit)
        hint = QLabel(
            "This is the name other devices see on your local network. Changing "
            "it requires administrator approval; you can keep the current name."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_account(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Your account")
        label.setObjectName("section")
        layout.addWidget(label)
        card = QFrame()
        card.setObjectName("card")
        card_layout = QVBoxLayout(card)
        card_layout.setContentsMargins(24, 20, 24, 20)
        self.account_name = QLabel(f"Signed in as  {current_user()}")
        self.account_name.setStyleSheet("font-size: 16px; color: #f4f7f8;")
        card_layout.addWidget(self.account_name)
        full_name = QLabel(f"Display name  ·  {current_full_name()}")
        full_name.setObjectName("hint")
        card_layout.addWidget(full_name)
        note = QLabel(
            "Calamares created this local administrator account during "
            "installation. It is not a cloud account. Use the button below to "
            "edit the account name or password in KDE Settings."
        )
        note.setObjectName("hint")
        note.setWordWrap(True)
        card_layout.addWidget(note)
        layout.addWidget(card)
        open_account = QPushButton("Open Account Settings")
        open_account.clicked.connect(
            lambda: open_kde_settings("kcm_users", self)
        )
        layout.addWidget(open_account)
        layout.addStretch(1)
        return page

    def _page_signin(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Sign-in")
        label.setObjectName("section")
        layout.addWidget(label)
        title = QLabel(current_user())
        title.setStyleSheet("font-size: 22px; color: #f4f7f8;")
        layout.addWidget(title)
        hint = QLabel(
            "Your password protects this computer and is requested by SDDM at "
            "startup. AURELIA stores a local password hash; it does not send "
            "your password to an online service."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        button = QPushButton("Change Password in Account Settings")
        button.clicked.connect(lambda: open_kde_settings("kcm_users", self))
        layout.addWidget(button)
        layout.addStretch(1)
        return page

    def _page_picture(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Profile picture")
        label.setObjectName("section")
        layout.addWidget(label)
        self.picture_preview = QLabel("No picture selected")
        self.picture_preview.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.picture_preview.setMinimumSize(190, 150)
        self.picture_preview.setStyleSheet(
            "border: 1px dashed #35515f; border-radius: 12px; color: #8ea7b4;"
        )
        layout.addWidget(self.picture_preview, 0, Qt.AlignmentFlag.AlignLeft)
        row = QHBoxLayout()
        choose = QPushButton("Choose Picture…")
        choose.clicked.connect(self._choose_picture)
        clear = QPushButton("Remove")
        clear.clicked.connect(self._clear_picture)
        row.addWidget(choose)
        row.addWidget(clear)
        row.addStretch(1)
        layout.addLayout(row)
        hint = QLabel(
            "The picture is copied to your local account as ~/.face and "
            "~/.face.icon. You can change it later."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_privacy(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Privacy")
        label.setObjectName("section")
        layout.addWidget(label)
        statement = QLabel(
            "AURELIA does not include a telemetry client, advertising ID or "
            "cloud account requirement. Your files, browser history and notes "
            "stay on this computer unless you choose to share them."
        )
        statement.setWordWrap(True)
        layout.addWidget(statement)
        bullet = QLabel(
            "Review KDE's privacy controls for recent files, clipboard and "
            "application permissions. The button opens the real Privacy module."
        )
        bullet.setObjectName("hint")
        bullet.setWordWrap(True)
        layout.addWidget(bullet)
        open_privacy = QPushButton("Open Privacy Settings")
        open_privacy.clicked.connect(lambda: open_kde_settings("kcm_privacy", self))
        layout.addWidget(open_privacy)
        layout.addStretch(1)
        return page

    def _page_appearance(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Appearance")
        label.setObjectName("section")
        layout.addWidget(label)
        self.dark_radio = QRadioButton("Midnight — the AURELIA dark theme")
        self.light_radio = QRadioButton("Ivory — a bright, high-contrast theme")
        group = QButtonGroup(page)
        group.addButton(self.dark_radio)
        group.addButton(self.light_radio)
        self.dark_radio.setChecked(True)
        layout.addWidget(self.dark_radio)
        layout.addWidget(self.light_radio)
        hint = QLabel(
            "This applies a real KDE colour scheme with plasma-apply-colorscheme. "
            "The desktop layout and wallpaper stay the same."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_accent(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Accent colour")
        label.setObjectName("section")
        layout.addWidget(label)
        self.accent_group = QButtonGroup(page)
        for index, (name, rgb, swatch) in enumerate(ACCENTS):
            row = QHBoxLayout()
            radio = QRadioButton(name)
            radio.setProperty("accent_rgb", rgb)
            self.accent_group.addButton(radio, index)
            if index == 0:
                radio.setChecked(True)
            square = QLabel()
            square.setObjectName("swatch")
            square.setStyleSheet(
                f"background: {swatch}; border-radius: 7px;"
                " min-width: 14px; max-width: 14px; min-height: 14px;"
                " max-height: 14px;"
            )
            row.addWidget(square)
            row.addWidget(radio)
            row.addStretch(1)
            layout.addLayout(row)
        hint = QLabel(
            "The accent is stored in KDE's configuration and applied to "
            "selection, focus and highlight colours."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_review(self) -> QWidget:
        page, layout = self._page_widget()
        label = QLabel("Review")
        label.setObjectName("section")
        layout.addWidget(label)
        self.review_label = QLabel()
        self.review_label.setObjectName("review")
        self.review_label.setWordWrap(True)
        layout.addWidget(self.review_label)
        hint = QLabel(
            "Finish applies these choices to your account. You can change all "
            "of them later in System Settings."
        )
        hint.setObjectName("hint")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        layout.addStretch(1)
        return page

    def _page_ready(self) -> QWidget:
        page, layout = self._page_widget()
        title = QLabel("You're ready.")
        title.setObjectName("heroTitle")
        layout.addWidget(title)
        sub = QLabel(
            "Your AURELIA desktop is waiting. Use the launcher in the top bar "
            "or the dock to open Files, NOVAIR, Settings and the rest of the "
            "installed applications."
        )
        sub.setObjectName("heroSub")
        sub.setWordWrap(True)
        layout.addWidget(sub)
        card = QFrame()
        card.setObjectName("card")
        card_layout = QVBoxLayout(card)
        card_layout.setContentsMargins(24, 20, 24, 20)
        for line in (
            "Top bar: launcher, system status and quick settings.",
            "Desktop: large clock, calendar, weather, notes and system monitor.",
            "NOVAIR: the AURELIA browser, one click from the dock.",
        ):
            item = QLabel("—   " + line)
            item.setObjectName("hint")
            card_layout.addWidget(item)
        layout.addWidget(card)
        layout.addStretch(1)
        return page

    def _set_step(self, index: int) -> None:
        self.step = max(0, min(len(STEPS) - 1, index))
        self.stack.setCurrentIndex(self.step)
        self.side_progress.setValue(self.step)
        self.kicker.setText(f"STEP {self.step + 1:02d} OF {len(STEPS):02d}")
        self.page_title.setText(STEPS[self.step])
        self.page_sub.setText(
            {
                0: "A quiet, local-first start for your new computer.",
                1: "Choose the formats your computer should use.",
                2: "Set the keyboard you will type on.",
                3: "Connect to a network, or continue offline.",
                4: "Give this computer a name on your network.",
                5: "Review the local account created by the installer.",
                6: "Your password protects this device.",
                7: "Add a face to your account.",
                8: "Know what stays on this computer.",
                9: "Choose how AURELIA looks.",
                10: "Pick the colour used for focus and selection.",
                11: "One last look before your desktop opens.",
                12: "Setup is complete.",
            }.get(self.step, ""))
        for index, label in enumerate(self.step_labels):
            if index == self.step:
                label.setObjectName("stepActive")
            elif index < self.step:
                label.setObjectName("stepDone")
            else:
                label.setObjectName("stepItem")
            label.style().unpolish(label)
            label.style().polish(label)
        self.back_button.setEnabled(self.step > 0)
        if self.step == 0:
            self.next_button.setText("Begin")
        elif self.step == len(STEPS) - 1:
            self.next_button.setText("Enter AURELIA")
        else:
            self.next_button.setText("Continue")
        self.step_note.setText(f"{self.step + 1} / {len(STEPS)}")

    def _next(self) -> None:
        if self.step == 0:
            self._set_step(1)
            return
        if self.step == len(STEPS) - 1:
            self._finish()
            return
        if self.step == 1:
            self.state["region"] = self.region_combo.currentData()
        elif self.step == 2:
            self.state["keyboard"] = self.keyboard_combo.currentData()
        elif self.step == 4:
            name = self.device_edit.text().strip()
            if not name:
                QMessageBox.warning(self, "Device name", "Enter a device name.")
                return
            if any(ch.isspace() for ch in name):
                QMessageBox.warning(self, "Device name", "Use a name without spaces.")
                return
            self.state["device_name"] = name
        elif self.step == 7:
            self.state["picture"] = self._picture_path
        elif self.step == 9:
            self.state["theme"] = "dark" if self.dark_radio.isChecked() else "light"
        elif self.step == 10:
            button = self.accent_group.checkedButton()
            if button is not None:
                self.state["accent"] = button.text()
        if self.step == 11:
            self._update_review()
        self._set_step(self.step + 1)

    def _back(self) -> None:
        self._set_step(self.step - 1)

    def _refresh_network(self) -> None:
        self.network_label.setText(network_summary())

    def _open_network(self) -> None:
        open_kde_settings("kcm_networkmanagement", self)

    def _choose_picture(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Choose a profile picture",
            str(Path.home()),
            "Images (*.png *.jpg *.jpeg *.webp *.bmp)",
        )
        if not path:
            return
        pixmap = QPixmap(path)
        if pixmap.isNull():
            QMessageBox.warning(self, "Picture", "That image could not be read.")
            return
        self._picture_path = path
        self.picture_preview.setPixmap(
            pixmap.scaled(QSize(180, 140), Qt.AspectRatioMode.KeepAspectRatio,
                          Qt.TransformationMode.SmoothTransformation)
        )
        self.picture_preview.setText("")

    def _clear_picture(self) -> None:
        self._picture_path = ""
        self.picture_preview.clear()
        self.picture_preview.setText("No picture selected")

    def _update_review(self) -> None:
        region = self.region_combo.currentText()
        keyboard = self.keyboard_combo.currentText()
        theme = "Midnight" if self.dark_radio.isChecked() else "Ivory"
        button = self.accent_group.checkedButton()
        accent = button.text() if button is not None else ACCENTS[0][0]
        self.review_label.setText(
            f"<b>Region</b><br>{region}<br><br>"
            f"<b>Keyboard</b><br>{keyboard}<br><br>"
            f"<b>Device name</b><br>{self.device_edit.text().strip() or socket.gethostname()}<br><br>"
            f"<b>Account</b><br>{current_user()}<br><br>"
            f"<b>Appearance</b><br>{theme} · {accent} accent<br><br>"
            f"<b>Profile picture</b><br>{Path(self._picture_path).name if self._picture_path else 'Not set'}"
        )

    def _finish(self) -> None:
        self.next_button.setEnabled(False)
        self.next_button.setText("Applying…")
        QApplication.processEvents()
        errors = self._apply()
        marker = marker_path()
        marker.write_text(
            json.dumps({"completed_at": time.time(), **self.state}, indent=2) + "\n",
            encoding="utf-8",
        )
        if errors:
            QMessageBox.warning(
                self,
                "Some settings need attention",
                "Setup finished, but these actions could not be completed:\n\n"
                + "\n".join(f"• {error}" for error in errors)
                + "\n\nYou can change them later in System Settings.",
            )
        self.close()
        QApplication.quit()

    def _apply(self) -> list[str]:
        errors: list[str] = []
        self.state["region"] = self.region_combo.currentData()
        self.state["keyboard"] = self.keyboard_combo.currentData()
        self.state["device_name"] = self.device_edit.text().strip()
        self.state["theme"] = "dark" if self.dark_radio.isChecked() else "light"
        button = self.accent_group.checkedButton()
        if button is not None:
            self.state["accent"] = button.text()
        state_path().write_text(json.dumps(self.state, indent=2) + "\n", encoding="utf-8")

        locale_path = config_home() / "plasma-localerc"
        parser = read_ini(locale_path)
        if not parser.has_section("Formats"):
            parser.add_section("Formats")
        parser.set("Formats", "LANG", self.state["region"])
        parser.set("Formats", "LC_TIME", self.state["region"])
        write_ini(locale_path, parser)

        keyboard_path = config_home() / "kxkbrc"
        parser = read_ini(keyboard_path)
        if not parser.has_section("Layout"):
            parser.add_section("Layout")
        parser.set("Layout", "Use", "true")
        parser.set("Layout", "LayoutList", self.state["keyboard"])
        write_ini(keyboard_path, parser)

        color_scheme = "Aurelia" if self.state["theme"] == "dark" else "BreezeLight"
        kdeglobals = config_home() / "kdeglobals"
        parser = read_ini(kdeglobals)
        if not parser.has_section("General"):
            parser.add_section("General")
        parser.set("General", "ColorScheme", color_scheme)
        accent_rgb = next(
            (rgb for name, rgb, _swatch in ACCENTS if name == self.state["accent"]),
            ACCENTS[0][1],
        )
        parser.set("General", "AccentColor", accent_rgb)
        write_ini(kdeglobals, parser)
        code, detail = run_command(["plasma-apply-colorscheme", color_scheme])
        if code != 0:
            errors.append(f"Colour scheme: {detail or 'plasma-apply-colorscheme failed'}")

        if self._picture_path:
            try:
                source = Path(self._picture_path)
                target = Path.home() / ".face"
                shutil.copyfile(source, target)
                icon_target = Path.home() / ".face.icon"
                try:
                    if icon_target.exists() or icon_target.is_symlink():
                        icon_target.unlink()
                    icon_target.symlink_to(target)
                except OSError:
                    shutil.copyfile(source, icon_target)
            except OSError as exc:
                errors.append(f"Profile picture: {exc}")

        if self.state["device_name"] and self.state["device_name"] != socket.gethostname():
            code, detail = run_command(
                ["pkexec", "hostnamectl", "set-hostname", self.state["device_name"]],
                timeout=60,
            )
            if code != 0:
                errors.append(f"Device name: {detail or 'administrator approval cancelled'}")

        for command in (
            ["qdbus6", "org.kde.KWin", "/KWin", "reconfigure"],
            ["qdbus6", "org.kde.plasmashell", "/PlasmaShell", "refreshCurrentShell"],
        ):
            if shutil.which(command[0]):
                run_command(command, timeout=8)
        return errors


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="AURELIA first-login setup")
    parser.add_argument("--version", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.version:
        print(APP_VERSION)
        return 0
    if args.self_test:
        print("AURELIA Setup self-test: PyQt6 import OK")
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
