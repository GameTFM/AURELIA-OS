#!/usr/bin/env python3
"""NOVAIR — the AURELIA OS browser.

NOVAIR is an original browser shell built on Qt WebEngine. The web renderer,
network stack and JavaScript engine are Qt's; the tabs, navigation, new-tab
page, bookmarks, history, download manager, private profile, settings and
permission handling in this file are NOVAIR's own interface.
"""
from __future__ import annotations

import argparse
import html
import json
import os
import re
import sqlite3
import sys
import time
import urllib.parse
from datetime import datetime
from pathlib import Path
from typing import Callable, Optional

from PyQt6.QtCore import QSize, Qt, QUrl
from PyQt6.QtGui import QAction, QDesktopServices, QIcon, QKeySequence
from PyQt6.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QMenu,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QSizePolicy,
    QStatusBar,
    QStyle,
    QTabWidget,
    QToolBar,
    QToolButton,
    QVBoxLayout,
    QWidget,
)
from PyQt6.QtWebEngineCore import (
    QWebEngineDownloadRequest,
    QWebEnginePage,
    QWebEngineProfile,
    QWebEngineSettings,
    QWebEngineUrlRequestInterceptor,
)
from PyQt6.QtWebEngineWidgets import QWebEngineView

APP_NAME = "NOVAIR"
APP_VERSION = "1.0"
ICON_PATH = "/usr/share/icons/hicolor/512x512/apps/novair.png"
TRACKERS = {
    "doubleclick.net",
    "googlesyndication.com",
    "google-analytics.com",
    "googletagmanager.com",
    "facebook.net",
    "scorecardresearch.com",
    "adservice.google.com",
    "amazon-adsystem.com",
    "criteo.com",
    "taboola.com",
}
SEARCH_ENGINES = {
    "DuckDuckGo": ("https://duckduckgo.com/", "q"),
    "Google": ("https://www.google.com/search", "q"),
    "Bing": ("https://www.bing.com/search", "q"),
    "Wikipedia": ("https://en.wikipedia.org/w/index.php", "search"),
}
DEFAULT_SETTINGS = {
    "search_engine": "DuckDuckGo",
    "homepage": "https://duckduckgo.com/",
    "allow_popups": False,
    "block_trackers": True,
    "theme": "midnight",
}


def app_data_dir() -> Path:
    base = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share"))
    path = base / "novair"
    path.mkdir(parents=True, exist_ok=True)
    return path


def config_dir() -> Path:
    base = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    path = base / "novair"
    path.mkdir(parents=True, exist_ok=True)
    return path


class Store:
    """Small, real SQLite store for browser history and bookmarks."""

    def __init__(self) -> None:
        self.conn = sqlite3.connect(app_data_dir() / "novair.db")
        self.conn.row_factory = sqlite3.Row
        self.conn.executescript(
            """
            CREATE TABLE IF NOT EXISTS history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                url TEXT NOT NULL,
                title TEXT NOT NULL DEFAULT '',
                visited_at REAL NOT NULL
            );
            CREATE TABLE IF NOT EXISTS bookmarks (
                url TEXT PRIMARY KEY,
                title TEXT NOT NULL DEFAULT '',
                created_at REAL NOT NULL
            );
            CREATE INDEX IF NOT EXISTS history_visited ON history(visited_at DESC);
            """
        )
        self.conn.commit()
        if not self.conn.execute("SELECT 1 FROM bookmarks LIMIT 1").fetchone():
            now = time.time()
            for url, title in (
                ("https://duckduckgo.com/", "DuckDuckGo"),
                ("https://en.wikipedia.org/", "Wikipedia"),
                ("https://github.com/", "GitHub"),
                ("https://www.youtube.com/", "YouTube"),
                ("https://www.kde.org/", "KDE"),
            ):
                self.conn.execute(
                    "INSERT OR IGNORE INTO bookmarks(url, title, created_at) VALUES (?, ?, ?)",
                    (url, title, now),
                )
            self.conn.commit()

    def add_history(self, url: str, title: str) -> None:
        if not url.startswith(("http://", "https://")):
            return
        self.conn.execute(
            "INSERT INTO history(url, title, visited_at) VALUES (?, ?, ?)",
            (url, title[:300], time.time()),
        )
        self.conn.execute(
            "DELETE FROM history WHERE id NOT IN "
            "(SELECT id FROM history ORDER BY id DESC LIMIT 5000)"
        )
        self.conn.commit()

    def recent(self, limit: int = 8) -> list[sqlite3.Row]:
        return list(
            self.conn.execute(
                "SELECT url, title, MAX(visited_at) AS visited_at FROM history "
                "GROUP BY url ORDER BY visited_at DESC LIMIT ?",
                (limit,),
            )
        )

    def history(self, limit: int = 500) -> list[sqlite3.Row]:
        return list(
            self.conn.execute(
                "SELECT url, title, visited_at FROM history ORDER BY visited_at DESC LIMIT ?",
                (limit,),
            )
        )

    def clear_history(self) -> None:
        self.conn.execute("DELETE FROM history")
        self.conn.commit()

    def bookmarks(self) -> list[sqlite3.Row]:
        return list(
            self.conn.execute(
                "SELECT url, title FROM bookmarks ORDER BY created_at DESC"
            )
        )

    def is_bookmarked(self, url: str) -> bool:
        return bool(
            self.conn.execute("SELECT 1 FROM bookmarks WHERE url = ?", (url,)).fetchone()
        )

    def add_bookmark(self, url: str, title: str) -> None:
        self.conn.execute(
            "INSERT OR REPLACE INTO bookmarks(url, title, created_at) VALUES (?, ?, ?)",
            (url, title or url, time.time()),
        )
        self.conn.commit()

    def remove_bookmark(self, url: str) -> None:
        self.conn.execute("DELETE FROM bookmarks WHERE url = ?", (url,))
        self.conn.commit()

    def close(self) -> None:
        self.conn.close()


def load_settings() -> dict:
    path = config_dir() / "settings.json"
    settings = dict(DEFAULT_SETTINGS)
    try:
        loaded = json.loads(path.read_text(encoding="utf-8"))
        if isinstance(loaded, dict):
            settings.update({key: loaded[key] for key in DEFAULT_SETTINGS if key in loaded})
    except (OSError, ValueError):
        pass
    return settings


def save_settings(settings: dict) -> None:
    path = config_dir() / "settings.json"
    path.write_text(json.dumps(settings, indent=2) + "\n", encoding="utf-8")


def search_url(settings: dict, query: str) -> QUrl:
    engine = settings.get("search_engine", "DuckDuckGo")
    action, field = SEARCH_ENGINES.get(engine, SEARCH_ENGINES["DuckDuckGo"])
    separator = "&" if "?" in action else "?"
    return QUrl(action + separator + urllib.parse.urlencode({field: query}))


def is_address(text: str) -> bool:
    text = text.strip()
    if not text:
        return False
    if re.match(r"^(https?|file)://", text, re.I):
        return True
    if text.startswith("localhost") or text.startswith("127.0.0.1"):
        return True
    return " " not in text and "." in text


def pretty_url(url: QUrl) -> str:
    if not url.isValid() or url.isEmpty():
        return ""
    if url.host() == "novair.local":
        return ""
    return url.toString()


def human_time(timestamp: float) -> str:
    return datetime.fromtimestamp(timestamp).strftime("%d %b %Y · %H:%M")


def human_bytes(value: int) -> str:
    value = max(0, int(value))
    for unit in ("B", "KB", "MB", "GB"):
        if value < 1024 or unit == "GB":
            return f"{value:.0f} {unit}" if unit == "B" else f"{value:.1f} {unit}"
        value /= 1024
    return f"{value:.1f} GB"


def unique_download_path(directory: Path, name: str) -> Path:
    """Return a non-existing path without silently overwriting a download."""
    safe_name = Path(name or "download").name
    candidate = directory / safe_name
    stem, suffix = Path(safe_name).stem, Path(safe_name).suffix
    counter = 1
    while candidate.exists():
        candidate = directory / f"{stem} ({counter}){suffix}"
        counter += 1
    return candidate


def new_tab_html(settings: dict, store: Store) -> str:
    engine = settings.get("search_engine", "DuckDuckGo")
    action, field = SEARCH_ENGINES.get(engine, SEARCH_ENGINES["DuckDuckGo"])
    empty = '<p class="empty">Pages you visit will appear here.</p>'
    tiles = []
    for row in store.bookmarks()[:10]:
        url = html.escape(row["url"], quote=True)
        title = html.escape(row["title"] or row["url"])
        tiles.append(
            f'<a class="tile" href="{url}"><span class="tile-letter">'
            f'{title[:1].upper()}</span><span>{title}</span></a>'
        )
    shortcuts = "".join(tiles) or '<p class="empty">Add a bookmark to pin it here.</p>'
    recent_rows = []
    for row in store.recent(7):
        url = html.escape(row["url"], quote=True)
        title = html.escape(row["title"] or row["url"])
        recent_rows.append(
            f'<a class="recent-row" href="{url}"><span>{title}</span>'
            f'<span class="recent-host">{html.escape(QUrl(row["url"]).host())}</span></a>'
        )
    recent = "".join(recent_rows) or empty
    dark = settings.get("theme", "midnight") != "ivory"
    palette = {
        "bg": "#080e14" if dark else "#f5f8fa",
        "panel": "rgba(18, 29, 40, .78)" if dark else "rgba(255, 255, 255, .82)",
        "text": "#edf7fb" if dark else "#102330",
        "muted": "#8fa9b7" if dark else "#5c7481",
        "line": "rgba(124, 196, 226, .17)" if dark else "rgba(21, 76, 102, .14)",
        "field": "rgba(4, 12, 19, .70)" if dark else "#ffffff",
        "shadow": "rgba(0, 0, 0, .34)" if dark else "rgba(30, 75, 102, .12)",
    }
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>NOVAIR</title>
<style>
:root {{ color-scheme: {'dark' if dark else 'light'}; }}
* {{ box-sizing: border-box; }}
body {{ margin: 0; min-height: 100vh; color: {palette['text']}; background:
  radial-gradient(ellipse at 50% -15%, #175a7b 0%, transparent 55%),
  linear-gradient(160deg, #0a1822 0%, {palette['bg']} 58%, #05090d 100%);
  font-family: Inter, "Noto Sans", sans-serif; }}
body.light {{ background: radial-gradient(ellipse at 50% -15%, #c9e9f7 0%,
  transparent 55%), linear-gradient(160deg, #f4fbfe 0%, #e8f2f6 100%); }}
main {{ width: min(1100px, calc(100% - 48px)); margin: 0 auto; padding: 9vh 0 48px; }}
.logo {{ display: flex; align-items: center; gap: 16px; justify-content: center;
  letter-spacing: .42em; font-size: clamp(28px, 5vw, 54px); font-weight: 300;
  color: {palette['text']}; }}
.logo b {{ font-weight: 800; letter-spacing: .08em; }}
.mark {{ width: 44px; height: 44px; border: 2px solid #49b7e4; border-radius: 13px;
  display: grid; place-items: center; color: #8bdcff; font-size: 20px;
  transform: rotate(45deg); box-shadow: 0 0 24px rgba(73,183,228,.28); }}
.mark span {{ transform: rotate(-45deg); }}
.clock {{ text-align: center; margin-top: 22px; color: {palette['muted']};
  font-size: 15px; letter-spacing: .16em; }}
.clock strong {{ color: {palette['text']}; font-size: 30px; font-weight: 300;
  letter-spacing: .08em; margin-right: 14px; }}
.search {{ margin: 34px auto 0; max-width: 780px; display: flex; align-items: center;
  background: {palette['field']}; border: 1px solid {palette['line']};
  border-radius: 18px; padding: 7px 9px 7px 18px; box-shadow: 0 14px 40px {palette['shadow']}; }}
.search label {{ color: #49b7e4; font-size: 21px; padding-right: 8px; }}
.search input {{ flex: 1; min-width: 0; border: 0; outline: 0; background: transparent;
  color: {palette['text']}; font: inherit; font-size: 17px; padding: 12px 8px; }}
.search button {{ border: 0; border-radius: 12px; padding: 12px 20px; background: #1b87b7;
  color: white; font: inherit; font-weight: 600; cursor: pointer; }}
.grid {{ display: grid; grid-template-columns: 1.35fr .85fr; gap: 20px; margin-top: 42px; }}
.card {{ background: {palette['panel']}; border: 1px solid {palette['line']};
  border-radius: 20px; padding: 20px; box-shadow: 0 12px 32px {palette['shadow']};
  backdrop-filter: blur(18px); }}
.card h2 {{ margin: 0 0 16px; font-size: 12px; letter-spacing: .18em;
  text-transform: uppercase; color: {palette['muted']}; font-weight: 600; }}
.tiles {{ display: grid; grid-template-columns: repeat(5, minmax(0, 1fr)); gap: 12px; }}
.tile {{ display: flex; flex-direction: column; gap: 9px; align-items: center;
  color: {palette['text']}; text-decoration: none; font-size: 12px; text-align: center;
  padding: 13px 7px; border-radius: 14px; background: rgba(255,255,255,.035);
  border: 1px solid transparent; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }}
.tile:hover {{ background: rgba(73,183,228,.12); border-color: {palette['line']}; }}
.tile-letter {{ display: grid; place-items: center; width: 40px; height: 40px;
  border-radius: 12px; color: #c9f2ff; background: linear-gradient(145deg,#176f97,#0c3d58);
  font-size: 18px; font-weight: 700; box-shadow: inset 0 1px rgba(255,255,255,.2); }}
.recent-row {{ display: flex; justify-content: space-between; gap: 14px; padding: 11px 0;
  color: {palette['text']}; text-decoration: none; border-bottom: 1px solid {palette['line']}; }}
.recent-row:last-child {{ border-bottom: 0; }}
.recent-row span:first-child {{ overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }}
.recent-host {{ color: {palette['muted']}; font-size: 12px; flex: 0 0 auto; max-width: 38%;
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }}
.empty {{ color: {palette['muted']}; font-size: 13px; margin: 10px 0; }}
footer {{ margin-top: 40px; text-align: center; color: {palette['muted']};
  font-size: 12px; letter-spacing: .06em; }}
@media (max-width: 760px) {{ .grid {{ grid-template-columns: 1fr; }}
  .tiles {{ grid-template-columns: repeat(3, 1fr); }} main {{ padding-top: 6vh; }} }}
</style></head>
<body class="{'light' if not dark else ''}">
<main>
  <div class="logo"><span class="mark"><span>N</span></span><b>NOVAIR</b></div>
  <div class="clock"><strong id="clock">--:--</strong><span id="date"></span></div>
  <form class="search" action="{html.escape(action, quote=True)}" method="get">
    <label for="q">⌕</label>
    <input id="q" name="{html.escape(field, quote=True)}" autocomplete="off"
      placeholder="Search {html.escape(engine)} or enter an address" autofocus>
    <button type="submit">Search</button>
  </form>
  <div class="grid">
    <section class="card"><h2>Favorites</h2><div class="tiles">{shortcuts}</div></section>
    <section class="card"><h2>Recently visited</h2>{recent}</section>
  </div>
  <footer>NOVAIR 1.0 · Qt WebEngine · AURELIA OS</footer>
</main>
<script>
function tick() {{
  const now = new Date();
  document.getElementById('clock').textContent =
    now.toLocaleTimeString([], {{hour: '2-digit', minute: '2-digit'}});
  document.getElementById('date').textContent =
    now.toLocaleDateString([], {{weekday:'long', day:'numeric', month:'long'}});
}}
tick(); setInterval(tick, 1000);
</script></body></html>"""


def stylesheet(theme: str) -> str:
    dark = theme != "ivory"
    bg = "#0a1018" if dark else "#eef5f8"
    panel = "#111b26" if dark else "#ffffff"
    field = "#0b141e" if dark else "#ffffff"
    text = "#eaf5fa" if dark else "#122631"
    muted = "#8aa4b3" if dark else "#5b7380"
    line = "#243746" if dark else "#d3e3ea"
    accent = "#49b7e4"
    hover = "#192b3a" if dark else "#e3f3fa"
    return f"""
QMainWindow, QDialog {{ background: {bg}; color: {text}; }}
QWidget {{ color: {text}; font-family: Inter, "Noto Sans", sans-serif; font-size: 13px; }}
QToolBar {{ background: {panel}; border: 0; border-bottom: 1px solid {line};
  spacing: 5px; padding: 5px 9px; }}
QToolButton {{ background: transparent; border: 0; border-radius: 8px; padding: 7px;
  color: {text}; }}
QToolButton:hover {{ background: {hover}; }}
QToolButton:pressed {{ background: {line}; }}
QLineEdit {{ background: {field}; border: 1px solid {line}; border-radius: 13px;
  padding: 8px 12px; color: {text}; selection-background-color: {accent};
  selection-color: #06111a; }}
QLineEdit:focus {{ border-color: {accent}; }}
QTabWidget::pane {{ border: 0; background: {bg}; }}
QTabBar {{ background: {panel}; }}
QTabBar::tab {{ background: transparent; color: {muted}; padding: 9px 16px;
  border: 0; border-bottom: 2px solid transparent; min-width: 90px; }}
QTabBar::tab:selected {{ color: {text}; border-bottom-color: {accent}; background: {bg}; }}
QTabBar::tab:hover {{ background: {hover}; }}
QProgressBar {{ border: 0; background: {line}; height: 2px; }}
QProgressBar::chunk {{ background: {accent}; }}
QMenuBar {{ background: {panel}; color: {text}; border-bottom: 1px solid {line}; }}
QMenuBar::item {{ background: transparent; padding: 6px 10px; }}
QMenuBar::item:selected {{ background: {hover}; border-radius: 6px; }}
QMenu {{ background: {panel}; color: {text}; border: 1px solid {line}; padding: 5px; }}
QMenu::item {{ padding: 7px 28px 7px 12px; border-radius: 6px; }}
QMenu::item:selected {{ background: {hover}; }}
QStatusBar {{ background: {panel}; color: {muted}; border-top: 1px solid {line}; }}
QListWidget {{ background: {field}; border: 1px solid {line}; border-radius: 10px;
  padding: 5px; }}
QListWidget::item {{ padding: 8px; border-radius: 6px; }}
QListWidget::item:selected {{ background: {accent}; color: #06111a; }}
QPushButton {{ background: {field}; color: {text}; border: 1px solid {line};
  border-radius: 8px; padding: 7px 14px; }}
QPushButton:hover {{ background: {hover}; border-color: {accent}; }}
QComboBox {{ background: {field}; color: {text}; border: 1px solid {line};
  border-radius: 8px; padding: 7px 10px; }}
QCheckBox {{ spacing: 8px; }}
QLabel#wordmark {{ font-weight: 800; letter-spacing: 2px; color: {accent}; font-size: 14px; }}
"""


class TrackerBlocker(QWebEngineUrlRequestInterceptor):
    """Real request interceptor for a small, transparent tracker deny-list."""

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.enabled = True

    def interceptRequest(self, info) -> None:  # noqa: N802 - Qt API
        if not self.enabled:
            return
        host = info.requestUrl().host().lower()
        if any(host == blocked or host.endswith("." + blocked) for blocked in TRACKERS):
            info.block(True)


class SettingsDialog(QDialog):
    def __init__(self, settings: dict, parent=None) -> None:
        super().__init__(parent)
        self.settings = settings
        self.setWindowTitle("NOVAIR Settings")
        self.resize(520, 300)
        form = QFormLayout(self)
        form.setContentsMargins(24, 24, 24, 18)
        form.setSpacing(14)
        form.addRow(QLabel("<b>NOVAIR preferences</b>"))
        self.engine = QComboBox()
        self.engine.addItems(SEARCH_ENGINES.keys())
        self.engine.setCurrentText(settings.get("search_engine", "DuckDuckGo"))
        form.addRow("Search engine", self.engine)
        self.homepage = QLineEdit(settings.get("homepage", "https://duckduckgo.com/"))
        self.homepage.setPlaceholderText("https://example.org")
        form.addRow("Homepage", self.homepage)
        self.theme = QComboBox()
        self.theme.addItems(["midnight", "ivory"])
        self.theme.setCurrentText(settings.get("theme", "midnight"))
        form.addRow("Theme", self.theme)
        self.block_trackers = QCheckBox("Block known advertising and analytics hosts")
        self.block_trackers.setChecked(bool(settings.get("block_trackers", True)))
        form.addRow("Tracking protection", self.block_trackers)
        self.allow_popups = QCheckBox("Allow sites to open pop-up windows")
        self.allow_popups.setChecked(bool(settings.get("allow_popups", False)))
        form.addRow("Pop-ups", self.allow_popups)
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Save
            | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def values(self) -> dict:
        return {
            "search_engine": self.engine.currentText(),
            "homepage": self.homepage.text().strip() or "https://duckduckgo.com/",
            "theme": self.theme.currentText(),
            "block_trackers": self.block_trackers.isChecked(),
            "allow_popups": self.allow_popups.isChecked(),
        }


class HistoryDialog(QDialog):
    def __init__(self, store: Store, open_url: Callable[[str], None], parent=None) -> None:
        super().__init__(parent)
        self.store = store
        self.open_url = open_url
        self.setWindowTitle("NOVAIR History")
        self.resize(760, 500)
        layout = QVBoxLayout(self)
        self.filter = QLineEdit()
        self.filter.setPlaceholderText("Filter history")
        self.filter.textChanged.connect(self.refresh)
        layout.addWidget(self.filter)
        self.list = QListWidget()
        self.list.itemDoubleClicked.connect(self._open)
        layout.addWidget(self.list, 1)
        row = QHBoxLayout()
        clear = QPushButton("Clear history")
        clear.clicked.connect(self._clear)
        close = QPushButton("Close")
        close.clicked.connect(self.accept)
        row.addWidget(clear)
        row.addStretch(1)
        row.addWidget(close)
        layout.addLayout(row)
        self.refresh()

    def refresh(self) -> None:
        query = self.filter.text().lower().strip()
        self.list.clear()
        for row in self.store.history():
            text = f"{row['title'] or row['url']}\n{row['url']} · {human_time(row['visited_at'])}"
            if query and query not in text.lower():
                continue
            item = QListWidgetItem(text)
            item.setData(Qt.ItemDataRole.UserRole, row["url"])
            self.list.addItem(item)

    def _open(self, item: QListWidgetItem) -> None:
        url = item.data(Qt.ItemDataRole.UserRole)
        if url:
            self.open_url(url)
            self.accept()

    def _clear(self) -> None:
        if QMessageBox.question(
            self,
            "Clear history",
            "Remove all NOVAIR browsing history?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        ) == QMessageBox.StandardButton.Yes:
            self.store.clear_history()
            self.refresh()


class BookmarksDialog(QDialog):
    def __init__(self, store: Store, open_url: Callable[[str], None], parent=None) -> None:
        super().__init__(parent)
        self.store = store
        self.open_url = open_url
        self.setWindowTitle("NOVAIR Bookmarks")
        self.resize(760, 480)
        layout = QVBoxLayout(self)
        self.list = QListWidget()
        self.list.itemDoubleClicked.connect(self._open)
        layout.addWidget(self.list, 1)
        row = QHBoxLayout()
        remove = QPushButton("Remove selected")
        remove.clicked.connect(self._remove)
        close = QPushButton("Close")
        close.clicked.connect(self.accept)
        row.addWidget(remove)
        row.addStretch(1)
        row.addWidget(close)
        layout.addLayout(row)
        self.refresh()

    def refresh(self) -> None:
        self.list.clear()
        for row in self.store.bookmarks():
            item = QListWidgetItem(f"{row['title'] or row['url']}\n{row['url']}")
            item.setData(Qt.ItemDataRole.UserRole, row["url"])
            self.list.addItem(item)

    def _open(self, item: QListWidgetItem) -> None:
        url = item.data(Qt.ItemDataRole.UserRole)
        if url:
            self.open_url(url)
            self.accept()

    def _remove(self) -> None:
        item = self.list.currentItem()
        if item:
            self.store.remove_bookmark(item.data(Qt.ItemDataRole.UserRole))
            self.refresh()


class DownloadsDialog(QDialog):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle("NOVAIR Downloads")
        self.resize(700, 420)
        self._rows: dict[int, tuple[QWidget, QProgressBar, QLabel]] = {}
        self._downloads: list[QWebEngineDownloadRequest] = []
        layout = QVBoxLayout(self)
        heading = QLabel("<b>Downloads</b>")
        layout.addWidget(heading)
        self.list = QListWidget()
        layout.addWidget(self.list, 1)
        row = QHBoxLayout()
        folder = QPushButton("Open Downloads folder")
        folder.clicked.connect(self._open_folder)
        close = QPushButton("Close")
        close.clicked.connect(self.accept)
        row.addWidget(folder)
        row.addStretch(1)
        row.addWidget(close)
        layout.addLayout(row)

    def add(self, download: QWebEngineDownloadRequest) -> None:
        self._downloads.append(download)
        item = QListWidgetItem(self.list)
        row = QWidget()
        row_layout = QVBoxLayout(row)
        row_layout.setContentsMargins(2, 4, 2, 4)
        name = QLabel(download.downloadFileName() or "Download")
        name.setStyleSheet("font-weight: 600;")
        progress = QProgressBar()
        progress.setTextVisible(False)
        progress.setMaximumHeight(6)
        state = QLabel("Starting…")
        state.setStyleSheet("color: #8aa4b3; font-size: 11px;")
        row_layout.addWidget(name)
        row_layout.addWidget(progress)
        row_layout.addWidget(state)
        item.setSizeHint(QSize(420, 78))
        self.list.addItem(item)
        self.list.setItemWidget(item, row)
        self._rows[id(download)] = (row, progress, state)
        download.receivedBytesChanged.connect(lambda d=download: self._refresh(d))
        download.totalBytesChanged.connect(lambda d=download: self._refresh(d))
        download.stateChanged.connect(lambda d=download: self._refresh(d))
        self._refresh(download)

    def _refresh(self, download: QWebEngineDownloadRequest) -> None:
        entry = self._rows.get(id(download))
        if not entry:
            return
        _, progress, state = entry
        total = download.totalBytes()
        received = download.receivedBytes()
        if total > 0:
            progress.setRange(0, 100)
            progress.setValue(min(100, int(received * 100 / total)))
            detail = f"{human_bytes(received)} of {human_bytes(total)}"
        else:
            progress.setRange(0, 0)
            detail = human_bytes(received)
        download_state = download.state()
        if download_state == QWebEngineDownloadRequest.DownloadState.DownloadCompleted:
            progress.setRange(0, 100)
            progress.setValue(100)
            state.setText(f"Completed · {human_bytes(received)}")
        elif download_state == QWebEngineDownloadRequest.DownloadState.DownloadCancelled:
            state.setText(f"Cancelled · {detail}")
        elif download_state == QWebEngineDownloadRequest.DownloadState.DownloadInterrupted:
            state.setText(f"Interrupted · {detail}")
        else:
            state.setText(f"Downloading · {detail}")

    def _open_folder(self) -> None:
        folder = Path.home() / "Downloads"
        folder.mkdir(parents=True, exist_ok=True)
        QDesktopServices.openUrl(QUrl.fromLocalFile(str(folder)))


class BrowserPage(QWebEnginePage):
    def __init__(self, profile: QWebEngineProfile, tab: "BrowserTab") -> None:
        super().__init__(profile, tab)
        self.tab = tab
        self.fullScreenRequested.connect(tab.window_ref.handle_fullscreen)
        self.featurePermissionRequested.connect(self._feature_permission)

    def createWindow(self, _window_type):  # noqa: N802 - Qt API
        if not self.tab.window_ref.settings.get("allow_popups", False):
            return None
        tab = self.tab.window_ref.new_tab(private=self.tab.private, select=True)
        return tab.page()

    def _feature_permission(self, origin: QUrl, feature) -> None:
        label = {
            QWebEnginePage.Feature.Geolocation: "location",
            QWebEnginePage.Feature.Notifications: "notifications",
            QWebEnginePage.Feature.MediaAudioCapture: "microphone",
            QWebEnginePage.Feature.MediaVideoCapture: "camera",
            QWebEnginePage.Feature.MediaAudioVideoCapture: "camera and microphone",
        }.get(feature, "a browser permission")
        answer = QMessageBox.question(
            self.view().window() if self.view() else None,
            "Website permission",
            f"Allow {origin.host() or 'this site'} to use {label}?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        policy = (
            QWebEnginePage.PermissionPolicy.PermissionGranted
            if answer == QMessageBox.StandardButton.Yes
            else QWebEnginePage.PermissionPolicy.PermissionDenied
        )
        self.setFeaturePermission(origin, feature, policy)


class BrowserTab(QWebEngineView):
    def __init__(self, window: "MainWindow", private: bool) -> None:
        super().__init__()
        self.window_ref = window
        self.private = private
        self._new_tab = True
        # Latest loadProgress value. The Qt signal cannot be read back, so the
        # window keeps this instead of calling tab.loadProgress().
        self.progress_value = 100
        page = BrowserPage(window.profile_for(private), self)
        self.setPage(page)
        self.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.customContextMenuRequested.connect(self._context_menu)
        self.urlChanged.connect(self._url_changed)
        self.titleChanged.connect(self._title_changed)
        self.iconChanged.connect(self._icon_changed)
        self.loadProgress.connect(self._load_progress)
        self.loadFinished.connect(self._load_finished)
        self._apply_settings()

    def _apply_settings(self) -> None:
        settings = self.settings()
        for attribute in (
            "JavascriptEnabled",
            "JavascriptCanOpenWindows",
            "LocalStorageEnabled",
            "FullScreenSupportEnabled",
            "PdfViewerEnabled",
            "DeveloperExtrasEnabled",
            "ErrorPageEnabled",
            "ScrollAnimatorEnabled",
        ):
            value = getattr(QWebEngineSettings.WebAttribute, attribute, None)
            if value is not None:
                settings.setAttribute(value, True)
        plugins = getattr(QWebEngineSettings.WebAttribute, "PluginsEnabled", None)
        if plugins is not None:
            settings.setAttribute(plugins, False)

    def load_new_tab(self) -> None:
        self._new_tab = True
        self.setHtml(
            new_tab_html(self.window_ref.settings, self.window_ref.store),
            QUrl("https://novair.local/"),
        )

    def _url_changed(self, url: QUrl) -> None:
        if url.host() != "novair.local":
            self._new_tab = False
        self.window_ref.tab_url_changed(self, url)

    def _title_changed(self, title: str) -> None:
        self.window_ref.tab_title_changed(self, title)

    def _icon_changed(self, icon: QIcon) -> None:
        self.window_ref.tab_icon_changed(self, icon)

    def _load_progress(self, value: int) -> None:
        self.progress_value = value
        self.window_ref.tab_progress_changed(self, value)

    def _load_finished(self, ok: bool) -> None:
        self.window_ref.tab_loaded(self, ok)

    def is_new_tab(self) -> bool:
        return self._new_tab or self.url().host() == "novair.local"

    def _context_menu(self, position) -> None:
        menu = QMenu(self)
        back = QAction("Back", self)
        back.setEnabled(self.history().canGoBack())
        back.triggered.connect(self.back)
        menu.addAction(back)
        forward = QAction("Forward", self)
        forward.setEnabled(self.history().canGoForward())
        forward.triggered.connect(self.forward)
        menu.addAction(forward)
        reload_action = QAction("Reload", self)
        reload_action.triggered.connect(self.reload)
        menu.addAction(reload_action)
        menu.addSeparator()
        for label, action_name in (("Copy", "Copy"), ("Paste", "Paste"), ("Select all", "SelectAll")):
            action = self.page().action(getattr(QWebEnginePage.WebAction, action_name))
            action.setText(label)
            menu.addAction(action)
        menu.addSeparator()
        inspect_action = QAction("Inspect page", self)
        inspect_action.triggered.connect(
            lambda: self.page().triggerAction(QWebEnginePage.WebAction.InspectElement)
        )
        menu.addAction(inspect_action)
        menu.exec(self.mapToGlobal(position))

    def closeEvent(self, event) -> None:  # noqa: N802 - Qt API
        self.page().deleteLater()
        super().closeEvent(event)


class MainWindow(QMainWindow):
    def __init__(
        self,
        settings: dict,
        store: Store,
        private: bool = False,
        initial_url: Optional[str] = None,
    ) -> None:
        super().__init__()
        self.settings = settings
        self.store = store
        self.private = private
        self._private_profile: Optional[QWebEngineProfile] = None
        self._downloads: Optional[DownloadsDialog] = None
        self._handled_downloads: set[int] = set()
        self._download_profiles: set[int] = set()
        self._was_maximized = False
        self.private_windows: list["MainWindow"] = []
        self.setWindowTitle("NOVAIR" + (" Private" if private else ""))
        self.resize(1280, 820)
        if Path(ICON_PATH).is_file():
            self.setWindowIcon(QIcon(ICON_PATH))
        self.setStyleSheet(stylesheet(self.settings.get("theme", "midnight")))
        self.profile = self.profile_for(private)
        self._install_tracker_for(self.profile)
        self._build_actions()
        self._build_toolbar()
        self._build_menu()
        self._build_tabs()
        self.setStatusBar(QStatusBar(self))
        self.statusBar().showMessage("NOVAIR ready")
        self.tabs.currentChanged.connect(self._current_changed)
        self.addAction(self.new_tab_action)
        self.addAction(self.close_tab_action)
        self.addAction(self.bookmark_action)
        self.addAction(self.history_action)
        self.addAction(self.downloads_action)
        self.addAction(self.settings_action)
        self.addAction(self.zoom_in_action)
        self.addAction(self.zoom_out_action)
        self.addAction(self.zoom_reset_action)
        if initial_url:
            self.new_tab(initial_url)
        else:
            self.new_tab()

    def profile_for(self, private: bool) -> QWebEngineProfile:
        if not private:
            profile = QWebEngineProfile.defaultProfile()
        elif self._private_profile is None:
            self._private_profile = QWebEngineProfile(self)
            profile = self._private_profile
        else:
            profile = self._private_profile
        profile_key = id(profile)
        if profile_key not in self._download_profiles:
            profile.downloadRequested.connect(self.handle_download)
            self._download_profiles.add(profile_key)
        return profile

    def _make_action(
        self,
        text: str,
        shortcut: Optional[str],
        slot: Callable,
        icon: Optional[QIcon] = None,
        checkable: bool = False,
    ) -> QAction:
        action = QAction(text, self)
        if icon is not None:
            action.setIcon(icon)
        if shortcut:
            action.setShortcut(QKeySequence(shortcut))
        action.setCheckable(checkable)
        action.triggered.connect(slot)
        return action

    def _build_actions(self) -> None:
        style = self.style()
        self.back_action = self._make_action(
            "Back", "Alt+Left", lambda: self._tab_call("back"),
            style.standardIcon(QStyle.StandardPixmap.SP_ArrowBack),
        )
        self.forward_action = self._make_action(
            "Forward", "Alt+Right", lambda: self._tab_call("forward"),
            style.standardIcon(QStyle.StandardPixmap.SP_ArrowForward),
        )
        self.reload_action = self._make_action(
            "Reload", "F5", lambda: self._tab_call("reload"),
            style.standardIcon(QStyle.StandardPixmap.SP_BrowserReload),
        )
        self.home_action = self._make_action(
            "Home", "Alt+Home", self.go_home,
            style.standardIcon(QStyle.StandardPixmap.SP_DirHomeIcon),
        )
        self.new_tab_action = self._make_action("New tab", "Ctrl+T", lambda: self.new_tab())
        self.new_private_action = self._make_action(
            "New private window", "Ctrl+Shift+N", self.open_private_window
        )
        self.close_tab_action = self._make_action("Close tab", "Ctrl+W", self.close_current_tab)
        self.bookmark_action = self._make_action("Bookmark this page", "Ctrl+D", self.toggle_bookmark)
        self.history_action = self._make_action("History", "Ctrl+H", self.show_history)
        self.downloads_action = self._make_action("Downloads", "Ctrl+J", self.show_downloads)
        self.settings_action = self._make_action("Settings", "Ctrl+,", self.show_settings)
        self.zoom_in_action = self._make_action(
            "Zoom in", "Ctrl++", lambda: self._zoom(0.1)
        )
        self.zoom_out_action = self._make_action(
            "Zoom out", "Ctrl+-", lambda: self._zoom(-0.1)
        )
        self.zoom_reset_action = self._make_action(
            "Reset zoom", "Ctrl+0", lambda: self._zoom(None)
        )
        self.quit_action = self._make_action("Quit NOVAIR", "Ctrl+Q", QApplication.quit)
        self.bookmarks_manager_action = self._make_action(
            "Manage bookmarks", None, self.show_bookmarks
        )
        self.clear_history_action = self._make_action(
            "Clear all history", None, self.clear_history
        )
        self.about_action = self._make_action("About NOVAIR", None, self.show_about)

    def _build_toolbar(self) -> None:
        self.toolbar = QToolBar("Navigation", self)
        self.toolbar.setMovable(False)
        self.toolbar.setIconSize(QSize(17, 17))
        self.addToolBar(Qt.ToolBarArea.TopToolBarArea, self.toolbar)
        self.toolbar.addAction(self.back_action)
        self.toolbar.addAction(self.forward_action)
        self.toolbar.addAction(self.reload_action)
        self.toolbar.addAction(self.home_action)
        self.wordmark = QLabel("NOVAIR")
        self.wordmark.setObjectName("wordmark")
        self.wordmark.setToolTip("NOVAIR new tab")
        self.wordmark.setCursor(Qt.CursorShape.PointingHandCursor)
        self.wordmark.mousePressEvent = lambda _event: self.new_tab()
        self.toolbar.addWidget(self.wordmark)
        self.address = QLineEdit()
        self.address.setPlaceholderText("Search with NOVAIR or enter an address")
        self.address.setClearButtonEnabled(True)
        self.address.setMinimumWidth(320)
        self.address.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.address.returnPressed.connect(self.navigate_address)
        self.toolbar.addWidget(self.address)
        self.star_button = QToolButton()
        self.star_button.setText("☆")
        self.star_button.setToolTip("Bookmark this page (Ctrl+D)")
        self.star_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.star_button.clicked.connect(self.toggle_bookmark)
        self.toolbar.addWidget(self.star_button)
        self.menu_button = QToolButton()
        self.menu_button.setText("⋮")
        self.menu_button.setToolTip("NOVAIR menu")
        menu = QMenu(self.menu_button)
        menu.addAction(self.new_tab_action)
        menu.addAction(self.new_private_action)
        menu.addSeparator()
        menu.addAction(self.bookmark_action)
        menu.addAction(self.bookmarks_manager_action)
        menu.addAction(self.history_action)
        menu.addAction(self.downloads_action)
        menu.addSeparator()
        menu.addAction(self.settings_action)
        menu.addAction(self.about_action)
        self.menu_button.setMenu(menu)
        self.menu_button.setPopupMode(QToolButton.ToolButtonPopupMode.InstantPopup)
        self.toolbar.addWidget(self.menu_button)
        self.toolbar.addAction(self.new_tab_action)

    def _build_menu(self) -> None:
        file_menu = self.menuBar().addMenu("&File")
        file_menu.addAction(self.new_tab_action)
        file_menu.addAction(self.new_private_action)
        file_menu.addAction(self.close_tab_action)
        file_menu.addSeparator()
        file_menu.addAction(self.quit_action)
        edit_menu = self.menuBar().addMenu("&Edit")
        edit_menu.addAction(self._make_action("Find on page", "Ctrl+F", self.find_on_page))
        edit_menu.addSeparator()
        edit_menu.addAction(self.zoom_in_action)
        edit_menu.addAction(self.zoom_out_action)
        edit_menu.addAction(self.zoom_reset_action)
        bookmarks_menu = self.menuBar().addMenu("&Bookmarks")
        bookmarks_menu.addAction(self.bookmark_action)
        bookmarks_menu.addAction(self.bookmarks_manager_action)
        self.bookmarks_menu = bookmarks_menu
        bookmarks_menu.aboutToShow.connect(self._populate_bookmarks_menu)
        history_menu = self.menuBar().addMenu("&History")
        history_menu.addAction(self.history_action)
        history_menu.addAction(self.clear_history_action)
        tools_menu = self.menuBar().addMenu("&Tools")
        tools_menu.addAction(self.downloads_action)
        tools_menu.addAction(self.settings_action)
        help_menu = self.menuBar().addMenu("&Help")
        help_menu.addAction(self.about_action)

    def _build_tabs(self) -> None:
        central = QWidget()
        layout = QVBoxLayout(central)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        self.progress = QProgressBar()
        self.progress.setTextVisible(False)
        self.progress.setMaximumHeight(2)
        self.progress.hide()
        layout.addWidget(self.progress)
        self.tabs = QTabWidget()
        self.tabs.setDocumentMode(True)
        self.tabs.setTabsClosable(True)
        self.tabs.setMovable(True)
        self.tabs.setTabBarAutoHide(False)
        self.tabs.tabCloseRequested.connect(self.close_tab)
        self.tabs.setCornerWidget(self._new_tab_button(), Qt.Corner.TopRightCorner)
        layout.addWidget(self.tabs, 1)
        self.setCentralWidget(central)

    def _new_tab_button(self) -> QToolButton:
        button = QToolButton()
        button.setText("+")
        button.setToolTip("New tab (Ctrl+T)")
        button.setAutoRaise(True)
        button.clicked.connect(lambda: self.new_tab())
        return button

    def new_tab(self, url: Optional[str] = None, private: Optional[bool] = None, select: bool = True) -> BrowserTab:
        if private is None:
            private = self.private
        tab = BrowserTab(self, private)
        index = self.tabs.addTab(tab, "New Tab")
        self.tabs.setTabToolTip(index, "NOVAIR")
        if select:
            self.tabs.setCurrentIndex(index)
        if url:
            tab.setUrl(QUrl.fromUserInput(url))
        else:
            tab.load_new_tab()
            if select:
                self.address.setFocus()
        return tab

    def _tab_call(self, method: str) -> None:
        tab = self.current_tab()
        if tab is not None:
            getattr(tab, method)()

    def current_tab(self) -> Optional[BrowserTab]:
        widget = self.tabs.currentWidget() if hasattr(self, "tabs") else None
        return widget if isinstance(widget, BrowserTab) else None

    def _current_changed(self, _index: int) -> None:
        tab = self.current_tab()
        if tab is None:
            self.address.setEnabled(False)
            return
        self.address.setEnabled(True)
        url = tab.url()
        self.address.setText(pretty_url(url))
        self.setWindowTitle(
            ("NOVAIR Private" if self.private else "NOVAIR")
            + (f" — {tab.title()}" if tab.title() else "")
        )
        self._update_star(tab)
        # QWebEngineView.loadProgress is a Qt signal, not a getter. Calling it
        # raised "TypeError: native Qt signal is not callable" and aborted the
        # switch to the tab. BrowserTab caches the latest value instead.
        value = getattr(tab, "progress_value", 100)
        self.progress.setVisible(0 < value < 100)
        self.progress.setValue(value)

    def navigate_address(self) -> None:
        text = self.address.text().strip()
        if not text:
            return
        tab = self.current_tab()
        if tab is None:
            tab = self.new_tab()
        url = QUrl.fromUserInput(text) if is_address(text) else search_url(self.settings, text)
        tab.setUrl(url)
        tab.setFocus()

    def tab_url_changed(self, tab: BrowserTab, url: QUrl) -> None:
        index = self.tabs.indexOf(tab)
        if index >= 0:
            self.tabs.setTabToolTip(index, url.toString())
        if tab is not self.current_tab():
            return
        self.address.setText(pretty_url(url))
        self.address.setCursorPosition(0)
        self._update_star(tab)
        if url.host() != "novair.local":
            self.progress.show()

    def tab_title_changed(self, tab: BrowserTab, title: str) -> None:
        index = self.tabs.indexOf(tab)
        if index >= 0:
            self.tabs.setTabText(index, (title or "New Tab")[:32])
            self.tabs.setTabToolTip(index, title or "New Tab")
        if tab is self.current_tab():
            self.setWindowTitle(
                ("NOVAIR Private" if self.private else "NOVAIR")
                + (f" — {title}" if title else "")
            )

    def tab_icon_changed(self, tab: BrowserTab, icon: QIcon) -> None:
        index = self.tabs.indexOf(tab)
        if index >= 0 and not icon.isNull():
            self.tabs.setTabIcon(index, icon)

    def tab_progress_changed(self, tab: BrowserTab, value: int) -> None:
        if tab is not self.current_tab():
            return
        self.progress.setValue(value)
        self.progress.setVisible(0 < value < 100)
        if value >= 100:
            self.progress.hide()

    def tab_loaded(self, tab: BrowserTab, _ok: bool) -> None:
        if not tab.private:
            url = tab.url()
            if url.scheme() in ("http", "https") and url.host() not in ("", "novair.local"):
                self.store.add_history(url.toString(), tab.title() or url.toString())
        index = self.tabs.indexOf(tab)
        if index >= 0 and tab.is_new_tab():
            self.tabs.setTabText(index, "New Tab")
            if tab is self.current_tab():
                self.address.clear()
        if tab is self.current_tab():
            self.progress.hide()
            self._update_star(tab)

    def _update_star(self, tab: Optional[BrowserTab]) -> None:
        if tab is None or tab.is_new_tab():
            self.star_button.setEnabled(False)
            self.star_button.setText("☆")
            return
        self.star_button.setEnabled(True)
        bookmarked = self.store.is_bookmarked(tab.url().toString())
        self.star_button.setText("★" if bookmarked else "☆")
        self.star_button.setToolTip(
            "Remove bookmark" if bookmarked else "Bookmark this page (Ctrl+D)"
        )

    def toggle_bookmark(self) -> None:
        tab = self.current_tab()
        if tab is None or tab.is_new_tab() or not tab.url().isValid():
            return
        url = tab.url().toString()
        if self.store.is_bookmarked(url):
            self.store.remove_bookmark(url)
            self.statusBar().showMessage("Bookmark removed", 2500)
        else:
            self.store.add_bookmark(url, tab.title() or url)
            self.statusBar().showMessage("Bookmark added", 2500)
        self._update_star(tab)

    def go_home(self) -> None:
        tab = self.current_tab()
        if tab is None:
            tab = self.new_tab()
        homepage = self.settings.get("homepage", "")
        if homepage:
            tab.setUrl(QUrl.fromUserInput(homepage))
        else:
            tab.load_new_tab()

    def close_current_tab(self) -> None:
        if self.tabs.count():
            self.close_tab(self.tabs.currentIndex())

    def close_tab(self, index: int) -> None:
        widget = self.tabs.widget(index)
        self.tabs.removeTab(index)
        if widget is not None:
            widget.deleteLater()
        if self.tabs.count() == 0:
            if self.private:
                self.close()
            else:
                self.new_tab()

    def _populate_bookmarks_menu(self) -> None:
        self.bookmarks_menu.clear()
        self.bookmarks_menu.addAction(self.bookmark_action)
        self.bookmarks_menu.addAction(self.bookmarks_manager_action)
        self.bookmarks_menu.addSeparator()
        bookmarks = self.store.bookmarks()[:30]
        if not bookmarks:
            empty = QAction("No bookmarks yet", self)
            empty.setEnabled(False)
            self.bookmarks_menu.addAction(empty)
        for row in bookmarks:
            title = row["title"] or row["url"]
            action = QAction(title[:50], self)
            action.triggered.connect(lambda _checked=False, url=row["url"]: self.open_url(url))
            self.bookmarks_menu.addAction(action)

    def open_url(self, url: str) -> None:
        tab = self.current_tab()
        if tab is None:
            tab = self.new_tab()
        tab.setUrl(QUrl.fromUserInput(url))

    def show_history(self) -> None:
        HistoryDialog(self.store, self.open_url, self).exec()

    def show_bookmarks(self) -> None:
        BookmarksDialog(self.store, self.open_url, self).exec()

    def clear_history(self) -> None:
        if QMessageBox.question(
            self,
            "Clear history",
            "Remove all NOVAIR browsing history?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        ) == QMessageBox.StandardButton.Yes:
            self.store.clear_history()
            self.statusBar().showMessage("History cleared", 2500)

    def show_downloads(self) -> None:
        if self._downloads is None:
            self._downloads = DownloadsDialog(self)
        self._downloads.show()
        self._downloads.raise_()
        self._downloads.activateWindow()

    def handle_download(self, download: QWebEngineDownloadRequest) -> None:
        key = id(download)
        if key in self._handled_downloads:
            return
        self._handled_downloads.add(key)
        downloads = Path.home() / "Downloads"
        downloads.mkdir(parents=True, exist_ok=True)
        candidate = unique_download_path(downloads, download.downloadFileName() or "download")
        download.setDownloadDirectory(str(downloads))
        download.setDownloadFileName(candidate.name)
        download.accept()
        if self._downloads is None:
            self._downloads = DownloadsDialog(self)
        self._downloads.add(download)
        self.show_downloads()
        self.statusBar().showMessage(f"Downloading {candidate.name}", 4000)

    def show_settings(self) -> None:
        dialog = SettingsDialog(self.settings, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            self.settings.update(dialog.values())
            save_settings(self.settings)
            if self._private_profile is None:
                self._install_tracker_for(QWebEngineProfile.defaultProfile())
            if self._private_profile is not None:
                self._install_tracker_for(self._private_profile)
            self.setStyleSheet(stylesheet(self.settings.get("theme", "midnight")))
            self.statusBar().showMessage("NOVAIR settings saved", 3000)

    def _install_tracker_for(self, profile: QWebEngineProfile) -> None:
        blocker = getattr(self, "_tracker_blocker", None)
        if blocker is None:
            blocker = TrackerBlocker(self)
            self._tracker_blocker = blocker
        blocker.enabled = bool(self.settings.get("block_trackers", True))
        profile.setUrlRequestInterceptor(blocker)

    def find_on_page(self) -> None:
        tab = self.current_tab()
        if tab is None:
            return
        text, ok = _get_text(self, "Find on page", "Find text:")
        if ok and text:
            tab.page().findText(text)

    def _zoom(self, delta: Optional[float]) -> None:
        tab = self.current_tab()
        if tab is None:
            return
        if delta is None:
            tab.setZoomFactor(1.0)
        else:
            tab.setZoomFactor(max(0.35, min(3.0, tab.zoomFactor() + delta)))

    def open_private_window(self) -> None:
        window = MainWindow(self.settings, self.store, private=True)
        self.private_windows.append(window)
        window.destroyed.connect(lambda: self._private_windows_changed(window))
        window.show()
        window.raise_()

    def _private_windows_changed(self, window: "MainWindow") -> None:
        if window in self.private_windows:
            self.private_windows.remove(window)

    def handle_fullscreen(self, request) -> None:
        if request.toggleOn():
            self._was_maximized = self.isMaximized()
            self.showFullScreen()
        else:
            if self._was_maximized:
                self.showMaximized()
            else:
                self.showNormal()
        request.accept()

    def show_about(self) -> None:
        QMessageBox.about(
            self,
            "About NOVAIR",
            "<h2>NOVAIR 1.0</h2>"
            "<p>The AURELIA OS browser.</p>"
            "<p>NOVAIR is an original browser interface built on Qt WebEngine. "
            "Qt WebEngine supplies the Chromium-derived rendering engine; NOVAIR "
            "supplies its tabs, navigation, new-tab page, bookmarks, history, "
            "downloads, private browsing, settings and permission prompts.</p>"
            "<p>Rendering engine: Qt WebEngine. OS: AURELIA.</p>",
        )


class _EmptyStore:
    """Minimal store used by the headless self-test."""

    def bookmarks(self):
        return []

    def recent(self, _limit: int = 8):
        return []


def _get_text(parent: QWidget, title: str, label: str) -> tuple[str, bool]:
    dialog = QDialog(parent)
    dialog.setWindowTitle(title)
    layout = QVBoxLayout(dialog)
    layout.addWidget(QLabel(label))
    edit = QLineEdit()
    layout.addWidget(edit)
    buttons = QDialogButtonBox(
        QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
    )
    buttons.accepted.connect(dialog.accept)
    buttons.rejected.connect(dialog.reject)
    layout.addWidget(buttons)
    edit.setFocus()
    result = dialog.exec()
    return edit.text(), result == QDialog.DialogCode.Accepted


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="NOVAIR, the AURELIA OS browser")
    parser.add_argument("--private", action="store_true", help="open a private window")
    parser.add_argument("--url", help="open this address in a new tab")
    parser.add_argument("--version", action="store_true", help="print the NOVAIR version")
    parser.add_argument("urls", nargs="*", help="addresses to open")
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="verify the Qt WebEngine Python bindings and exit",
    )
    args = parser.parse_args(argv)
    if args.version:
        print(f"NOVAIR {APP_VERSION}")
        return 0
    if args.self_test:
        checks = [
            ("Qt WebEngine bindings import", True),
            ("search URL", search_url(DEFAULT_SETTINGS, "aurelia os").toString().startswith("https://duckduckgo.com/?q=")),
            ("address detection", is_address("example.com") and not is_address("not an address")),
            ("new-tab HTML", "NOVAIR" in new_tab_html(DEFAULT_SETTINGS, _EmptyStore())),
            ("safe download names", unique_download_path(Path("/tmp"), "../../file.zip").name == "file.zip"),
        ]
        failed = [label for label, ok in checks if not ok]
        for label, ok in checks:
            print(f"NOVAIR self-test {label}: {'PASS' if ok else 'FAIL'}")
        return 1 if failed else 0

    QApplication.setAttribute(Qt.ApplicationAttribute.AA_ShareOpenGLContexts, True)
    app = QApplication([sys.argv[0]])
    app.setApplicationName(APP_NAME)
    app.setApplicationVersion(APP_VERSION)
    app.setOrganizationName("AURELIA")
    if Path(ICON_PATH).is_file():
        app.setWindowIcon(QIcon(ICON_PATH))
    settings = load_settings()
    store = Store()
    initial_url = args.url or (args.urls[0] if args.urls else None)
    window = MainWindow(settings, store, private=args.private, initial_url=initial_url)
    window.show()
    if initial_url:
        window.address.setFocus()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
