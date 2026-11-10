#!/usr/bin/env python3
"""AURELIA Windows migration wizard.

The wizard only exposes operations implemented by aurelia_migration: scan a
mounted Windows installation, copy selected folders without overwriting by
default, map applications to reviewed Linux packages, import browser data into
NOVAIR, and optionally create an encrypted password vault.
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Optional

from PyQt6.QtCore import QThread, Qt, pyqtSignal
from PyQt6.QtGui import QIcon
from PyQt6.QtWidgets import (
    QApplication, QCheckBox, QComboBox, QFileDialog, QFrame, QHBoxLayout,
    QLabel, QLineEdit, QMainWindow, QMessageBox, QProgressBar, QPushButton,
    QScrollArea, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget,
)

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import aurelia_migration as backend  # noqa: E402

ICON = "/usr/share/icons/hicolor/512x512/apps/aurelia-logo.png"
QSS = """
QWidget { background: #080d13; color: #dfeaf0; font-family: Inter, sans-serif; font-size: 13px; }
QLabel#title { font-family: "Playfair Display"; font-size: 30px; color: #f6f1e6; }
QLabel#sub { color: #8ea7b4; }
QLabel#section { color: #d6b878; font-size: 10px; font-weight: 700; letter-spacing: 2px; }
QFrame#card { background: #0c1720; border: 1px solid #1f3543; border-radius: 10px; }
QPushButton { background: #121f2a; border: 1px solid #2a4050; border-radius: 7px; padding: 8px 14px; }
QPushButton#primary { background: #c9a96a; color: #101820; border: 0; font-weight: 700; }
QPushButton:disabled { color: #566b76; }
QComboBox, QLineEdit { background: #0b141d; border: 1px solid #263c4a; border-radius: 7px; padding: 8px; }
QCheckBox { spacing: 8px; }
QProgressBar { max-height: 5px; border: 0; background: #172732; }
QProgressBar::chunk { background: #c9a96a; }
QTableWidget { background: #0b141d; gridline-color: #1d2f3c; border: 1px solid #263c4a; }
QHeaderView::section { background: #10202b; color: #9eb2bd; padding: 6px; border: 0; }
"""


def human_size(value: int) -> str:
    size = float(value)
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if size < 1024:
            return f"{size:.1f} {unit}"
        size /= 1024
    return f"{size:.1f} TiB"


class Worker(QThread):
    progress = pyqtSignal(str, int, int)
    finished_ok = pyqtSignal(dict)
    failed = pyqtSignal(str)

    def __init__(self, action: str, payload: dict) -> None:
        super().__init__()
        self.action = action
        self.payload = payload

    def run(self) -> None:
        try:
            if self.action == "scan":
                root = Path(self.payload["root"])
                user = self.payload["user"]
                data = {
                    "groups": backend.user_file_groups(root, user),
                    "apps": [app.__dict__ for app in backend.scan_applications(root, user)],
                    "profiles": backend.browser_profiles(root, user),
                }
                self.finished_ok.emit(data)
            elif self.action == "migrate":
                options = backend.MigrationOptions(**self.payload)
                result = backend.migrate(
                    options,
                    progress=lambda message, done, total: self.progress.emit(message, done, total),
                )
                self.finished_ok.emit(result)
        except Exception as exc:  # noqa: BLE001 - surface the real exception in the UI
            self.failed.emit(str(exc))


class MigrationWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Bring your files and apps to AURELIA")
        self.resize(1100, 780)
        self.setMinimumSize(900, 650)
        if Path(ICON).is_file():
            self.setWindowIcon(QIcon(ICON))
        self.installations: list[backend.WindowsInstallation] = []
        self.apps: list[backend.Application] = []
        self.profiles: list[dict] = []
        self.groups: list[dict] = []
        self.worker: Optional[Worker] = None
        self._build()
        self.refresh_sources()

    def _build(self) -> None:
        root = QWidget()
        self.setCentralWidget(root)
        outer = QVBoxLayout(root)
        outer.setContentsMargins(28, 24, 28, 20)
        header = QLabel("Bring your files and apps to AURELIA")
        header.setObjectName("title")
        outer.addWidget(header)
        sub = QLabel(
            "Move important files, applications and browser data from your previous "
            "Windows computer. The Windows drive is mounted read-only and is never "
            "modified, formatted or deleted."
        )
        sub.setObjectName("sub")
        sub.setWordWrap(True)
        outer.addWidget(sub)
        outer.addSpacing(12)

        source_card = QFrame()
        source_card.setObjectName("card")
        source_layout = QVBoxLayout(source_card)
        source_layout.setContentsMargins(18, 14, 18, 14)
        row = QHBoxLayout()
        row.addWidget(QLabel("Windows installation"))
        self.source_combo = QComboBox()
        self.source_combo.setMinimumWidth(430)
        row.addWidget(self.source_combo, 1)
        self.refresh_button = QPushButton("Scan drives")
        self.refresh_button.clicked.connect(self.refresh_sources)
        row.addWidget(self.refresh_button)
        self.mount_button = QPushButton("Mount read-only")
        self.mount_button.clicked.connect(self.mount_source)
        row.addWidget(self.mount_button)
        source_layout.addLayout(row)
        row2 = QHBoxLayout()
        row2.addWidget(QLabel("Windows user"))
        self.user_combo = QComboBox()
        row2.addWidget(self.user_combo)
        self.scan_button = QPushButton("Scan files and applications")
        self.scan_button.setObjectName("primary")
        self.scan_button.clicked.connect(self.scan_source)
        row2.addWidget(self.scan_button)
        row2.addStretch(1)
        source_layout.addLayout(row2)
        self.source_status = QLabel("Choose a detected Windows installation.")
        self.source_status.setObjectName("sub")
        source_layout.addWidget(self.source_status)
        outer.addWidget(source_card)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        content = QWidget()
        self.content_layout = QVBoxLayout(content)
        self.content_layout.setContentsMargins(0, 10, 0, 10)
        self.content_layout.setSpacing(12)
        scroll.setWidget(content)
        outer.addWidget(scroll, 1)

        self.files_card = self._card("FILES")
        self.files_layout = self.files_card.layout()
        self.content_layout.addWidget(self.files_card)
        self.apps_card = self._card("APPLICATIONS")
        self.apps_layout = self.apps_card.layout()
        self.content_layout.addWidget(self.apps_card)
        self.browser_card = self._card("BROWSER DATA AND SETTINGS")
        self.browser_layout = self.browser_card.layout()
        self.content_layout.addWidget(self.browser_card)
        self.password_card = self._card("PASSWORD IMPORT · EXPLICIT CONSENT")
        self.password_layout = self.password_card.layout()
        self.content_layout.addWidget(self.password_card)
        self.content_layout.addStretch(1)

        destination_row = QHBoxLayout()
        destination_row.addWidget(QLabel("Destination"))
        self.destination_edit = QLineEdit(str(backend.default_destination(False)))
        destination_row.addWidget(self.destination_edit, 1)
        browse = QPushButton("Browse…")
        browse.clicked.connect(self._browse_destination)
        destination_row.addWidget(browse)
        outer.addLayout(destination_row)
        options_row = QHBoxLayout()
        self.merge_check = QCheckBox("Merge into my normal home folders")
        options_row.addWidget(self.merge_check)
        self.overwrite_check = QCheckBox("Overwrite existing files (off by default)")
        options_row.addWidget(self.overwrite_check)
        self.merge_check.toggled.connect(self._merge_changed)
        options_row.addStretch(1)
        outer.addLayout(options_row)
        self.progress = QProgressBar()
        self.progress.setRange(0, 1000)
        self.progress.setTextVisible(False)
        outer.addWidget(self.progress)
        self.status = QLabel("Ready.")
        self.status.setObjectName("sub")
        outer.addWidget(self.status)
        buttons = QHBoxLayout()
        buttons.addStretch(1)
        cancel = QPushButton("Close")
        cancel.clicked.connect(self.close)
        buttons.addWidget(cancel)
        self.run_button = QPushButton("Transfer selected items")
        self.run_button.setObjectName("primary")
        self.run_button.clicked.connect(self.run_migration)
        buttons.addWidget(self.run_button)
        outer.addLayout(buttons)
        self._populate_password_controls()

    def _card(self, title: str) -> QFrame:
        card = QFrame()
        card.setObjectName("card")
        layout = QVBoxLayout(card)
        layout.setContentsMargins(18, 14, 18, 14)
        label = QLabel(title)
        label.setObjectName("section")
        layout.addWidget(label)
        return card

    def _clear_layout(self, layout) -> None:
        while layout.count():
            item = layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()

    def refresh_sources(self) -> None:
        self.source_status.setText("Scanning mounted and unmounted volumes…")
        QApplication.processEvents()
        self.installations = backend.discover_installations()
        self.source_combo.clear()
        for installation in self.installations:
            state = "mounted" if installation.mounted else "not mounted"
            self.source_combo.addItem(f"{installation.label} · {state}", installation)
        if not self.installations:
            self.source_combo.addItem("No Windows installation detected", None)
            self.source_status.setText(
                "Connect the Windows drive or mount it read-only, then choose Scan drives. "
                "AURELIA never modifies the source drive."
            )
        else:
            self.source_status.setText(f"Detected {len(self.installations)} source(s).")
            self._source_changed()
            self.source_combo.currentIndexChanged.connect(self._source_changed)

    def _source_changed(self) -> None:
        installation = self.source_combo.currentData()
        self.user_combo.clear()
        if installation is None:
            return
        if installation.mounted:
            for user in installation.users:
                self.user_combo.addItem(user)
            self.mount_button.setEnabled(False)
        else:
            self.mount_button.setEnabled(True)
            self.user_combo.addItem("Mount the volume to list users")

    def mount_source(self) -> None:
        installation = self.source_combo.currentData()
        if installation is None:
            return
        self.source_status.setText("Requesting a read-only mount…")
        QApplication.processEvents()
        mounted = backend.mount_installation(installation)
        if mounted.mounted:
            self.refresh_sources()
        else:
            QMessageBox.warning(self, "Could not mount", mounted.error or "The volume could not be mounted.")
            self.source_status.setText(mounted.error or "Mount failed.")

    def scan_source(self) -> None:
        installation = self.source_combo.currentData()
        if installation is None or not installation.mounted:
            QMessageBox.warning(self, "Select a source", "Mount a Windows installation first.")
            return
        user = self.user_combo.currentText()
        if not user:
            return
        self.scan_button.setEnabled(False)
        self.status.setText("Scanning Windows files, Start Menu shortcuts and browser profiles…")
        self.worker = Worker("scan", {"root": installation.root, "user": user})
        self.worker.finished_ok.connect(self._scan_done)
        self.worker.failed.connect(self._worker_failed)
        self.worker.start()

    def _scan_done(self, data: dict) -> None:
        self.scan_button.setEnabled(True)
        self.groups = data.get("groups", [])
        self.apps = [backend.Application(**item) for item in data.get("apps", [])]
        self.profiles = data.get("profiles", [])
        backend.save_application_inventory(self.apps)
        self._render_files()
        self._render_apps()
        self._render_browser()
        self.status.setText(
            f"Found {len(self.groups)} file groups, {len(self.apps)} applications and "
            f"{len(self.profiles)} browser profiles."
        )

    def _render_files(self) -> None:
        self._clear_layout(self.files_layout)
        self.file_checks: dict[str, QCheckBox] = {}
        for group in self.groups:
            check = QCheckBox(f"{group['label']} · {human_size(group['size'])} · {group['file_count']} files")
            check.setChecked(True)
            self.file_checks[group["key"]] = check
            self.files_layout.addWidget(check)
        custom = QPushButton("Add another folder from this Windows user…")
        custom.clicked.connect(self._add_custom_folder)
        self.files_layout.addWidget(custom)
        self.custom_folders: list[dict] = []

    def _add_custom_folder(self) -> None:
        installation = self.source_combo.currentData()
        if installation is None:
            return
        path = QFileDialog.getExistingDirectory(self, "Choose a Windows folder", installation.root)
        if path:
            size = backend.directory_size(Path(path))
            group = {"key": path, "label": Path(path).name, "path": path, "size": size,
                     "file_count": sum(1 for item in Path(path).rglob("*") if item.is_file())}
            check = QCheckBox(f"{group['label']} · {human_size(size)} · custom folder")
            check.setChecked(True)
            self.custom_folders.append(group)
            self.file_checks[path] = check
            self.files_layout.insertWidget(self.files_layout.count() - 1, check)

    def _render_apps(self) -> None:
        self._clear_layout(self.apps_layout)
        compatible = [app for app in self.apps if app.compatible]
        manual = [app for app in self.apps if not app.compatible]
        self.table_apps = compatible
        table = QTableWidget(len(compatible), 6)
        table.setHorizontalHeaderLabels(["Transfer", "Application", "Version", "Publisher", "AURELIA version", "Status"])
        table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        table.horizontalHeader().setStretchLastSection(True)
        for row, app in enumerate(compatible):
            check = QTableWidgetItem()
            check.setFlags(check.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            check.setCheckState(Qt.CheckState.Checked)
            table.setItem(row, 0, check)
            name_item = QTableWidgetItem(app.name)
            if app.icon_path and Path(app.icon_path).is_file():
                name_item.setIcon(QIcon(app.icon_path))
            table.setItem(row, 1, name_item)
            table.setItem(row, 2, QTableWidgetItem(app.version or "—"))
            table.setItem(row, 3, QTableWidgetItem(app.publisher or "—"))
            table.setItem(row, 4, QTableWidgetItem(app.linux_name or app.linux_kind))
            table.setItem(row, 5, QTableWidgetItem(f"Compatible · {app.linux_kind}"))
        table.resizeColumnsToContents()
        self.apps_table = table
        self.apps_layout.addWidget(table)
        note = QLabel(
            "Windows executables are never copied as installed Linux applications. "
            "Only reviewed package mappings can be installed, and only after the "
            "checked rows are approved."
        )
        note.setWordWrap(True)
        note.setObjectName("sub")
        self.apps_layout.addWidget(note)
        self.install_apps_check = QCheckBox("Install the checked compatible applications with the AURELIA package manager")
        self.apps_layout.addWidget(self.install_apps_check)

        manual_title = QLabel("APPLICATIONS THAT NEED MANUAL INSTALLATION OR AN ALTERNATIVE")
        manual_title.setObjectName("section")
        self.apps_layout.addWidget(manual_title)
        if manual:
            manual_table = QTableWidget(len(manual), 5)
            manual_table.setHorizontalHeaderLabels(["Application", "Version", "Publisher", "AURELIA alternative", "Note"])
            manual_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
            manual_table.horizontalHeader().setStretchLastSection(True)
            for row, app in enumerate(manual):
                name_item = QTableWidgetItem(app.name)
                if app.icon_path and Path(app.icon_path).is_file():
                    name_item.setIcon(QIcon(app.icon_path))
                manual_table.setItem(row, 0, name_item)
                manual_table.setItem(row, 1, QTableWidgetItem(app.version or "—"))
                manual_table.setItem(row, 2, QTableWidgetItem(app.publisher or "—"))
                manual_table.setItem(row, 3, QTableWidgetItem(app.alternative or "No automatic replacement"))
                manual_table.setItem(row, 4, QTableWidgetItem(app.note or "Preserved in the application inventory for reference."))
            manual_table.resizeColumnsToContents()
            self.apps_layout.addWidget(manual_table)
        else:
            self.apps_layout.addWidget(QLabel("Every detected application has a reviewed AURELIA mapping."))

    def _render_browser(self) -> None:
        self._clear_layout(self.browser_layout)
        self.browser_checks: dict[str, QCheckBox] = {}
        for profile in self.profiles:
            check = QCheckBox(
                f"{profile['browser']} · {profile['profile']} · {human_size(profile['size'])}"
            )
            check.setChecked(True)
            self.browser_checks[profile["path"]] = check
            self.browser_layout.addWidget(check)
        note = QLabel(
            "Bookmarks and history are imported into NOVAIR's real SQLite store. "
            "Chrome/Edge/Firefox preference files are preserved for reference because "
            "their Windows profile formats are not interchangeable with Linux browsers. "
            "Extensions and encrypted password databases are not silently copied."
        )
        note.setWordWrap(True)
        note.setObjectName("sub")
        self.browser_layout.addWidget(note)
        wallpaper_row = QHBoxLayout()
        wallpaper_row.addWidget(QLabel("Wallpaper (optional)"))
        self.wallpaper_edit = QLineEdit()
        self.wallpaper_edit.setPlaceholderText("Choose a wallpaper from the Windows profile")
        wallpaper_row.addWidget(self.wallpaper_edit, 1)
        choose_wallpaper = QPushButton("Choose…")
        choose_wallpaper.clicked.connect(self._choose_wallpaper)
        wallpaper_row.addWidget(choose_wallpaper)
        self.browser_layout.addLayout(wallpaper_row)
        if not self.profiles:
            self.browser_layout.addWidget(QLabel("No supported Chrome, Edge, Brave or Firefox profile was found."))

    def _populate_password_controls(self) -> None:
        self.password_check = QCheckBox(
            "I have a browser password export CSV and explicitly consent to importing it"
        )
        self.password_check.toggled.connect(self._toggle_password)
        self.password_layout.addWidget(self.password_check)
        row = QHBoxLayout()
        self.password_edit = QLineEdit()
        self.password_edit.setPlaceholderText("Path to the CSV export")
        self.password_edit.setEnabled(False)
        row.addWidget(self.password_edit, 1)
        choose = QPushButton("Choose CSV…")
        choose.setEnabled(False)
        self.password_choose = choose
        choose.clicked.connect(self._choose_password_csv)
        row.addWidget(choose)
        self.password_layout.addLayout(row)
        row2 = QHBoxLayout()
        self.password_backend = QComboBox()
        self.password_backend.addItem("Encrypted GnuPG vault", "gpg")
        self.password_backend.addItem("New KeePassXC database", "keepassxc")
        self.password_backend.setEnabled(False)
        row2.addWidget(QLabel("Storage"))
        row2.addWidget(self.password_backend)
        self.password_passphrase = QLineEdit()
        self.password_passphrase.setEchoMode(QLineEdit.EchoMode.Password)
        self.password_passphrase.setPlaceholderText("Vault / database password (at least 8 characters)")
        self.password_passphrase.setEnabled(False)
        row2.addWidget(self.password_passphrase, 1)
        self.password_layout.addLayout(row2)
        warning = QLabel(
            "Passwords are never imported silently. Chrome and Edge encrypt their "
            "password databases with Windows DPAPI, which AURELIA cannot decrypt. "
            "Export a CSV from the browser first, then choose it here. The CSV is "
            "read into an encrypted vault (or a new KeePassXC database) and the "
            "temporary input is not copied into your home directory."
        )
        warning.setWordWrap(True)
        warning.setObjectName("sub")
        self.password_layout.addWidget(warning)

    def _toggle_password(self, enabled: bool) -> None:
        self.password_edit.setEnabled(enabled)
        self.password_choose.setEnabled(enabled)
        self.password_backend.setEnabled(enabled)
        self.password_passphrase.setEnabled(enabled)

    def _choose_password_csv(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Choose browser password CSV", str(Path.home()), "CSV (*.csv)")
        if path:
            self.password_edit.setText(path)

    def _choose_wallpaper(self) -> None:
        installation = self.source_combo.currentData()
        start = installation.root if installation is not None else str(Path.home())
        path, _ = QFileDialog.getOpenFileName(self, "Choose a Windows wallpaper", start,
                                              "Images (*.png *.jpg *.jpeg *.bmp *.webp)")
        if path:
            self.wallpaper_edit.setText(path)

    def _merge_changed(self, enabled: bool) -> None:
        self.destination_edit.setText(str(backend.default_destination(enabled)))

    def _browse_destination(self) -> None:
        path = QFileDialog.getExistingDirectory(self, "Choose migration destination", str(Path.home()))
        if path:
            self.destination_edit.setText(path)

    def _worker_failed(self, message: str) -> None:
        self.scan_button.setEnabled(True)
        self.run_button.setEnabled(True)
        self.status.setText("Operation failed: " + message)
        QMessageBox.warning(self, "AURELIA migration", message)

    def run_migration(self) -> None:
        installation = self.source_combo.currentData()
        if installation is None or not installation.mounted:
            QMessageBox.warning(self, "Select a source", "Mount a Windows installation first.")
            return
        selected_groups = [
            {"key": key, "path": next((group["path"] for group in self.groups if group["key"] == key), key)}
            for key, check in self.file_checks.items() if check.isChecked()
        ]
        selected_apps = []
        for row, app in enumerate(self.table_apps):
            item = self.apps_table.item(row, 0)
            if item is None or item.checkState() != Qt.CheckState.Checked:
                continue
            selected_apps.append({**app.__dict__, "selected": True})
        profiles = [path for path, check in self.browser_checks.items() if check.isChecked()]
        passwords = self.password_check.isChecked()
        if passwords and not self.password_edit.text().strip():
            QMessageBox.warning(self, "Password import", "Choose the CSV export or clear the consent checkbox.")
            return
        if passwords and len(self.password_passphrase.text()) < 8:
            QMessageBox.warning(self, "Password import", "Choose a vault password of at least 8 characters.")
            return
        if self.merge_check.isChecked() and self.overwrite_check.isChecked():
            if QMessageBox.question(
                self, "Overwrite files?",
                "Merging into your home folders with overwrite enabled can replace existing files. Continue?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            ) != QMessageBox.StandardButton.Yes:
                return
        options = {
            "source_root": installation.root,
            "windows_user": self.user_combo.currentText(),
            "groups": selected_groups,
            "destination": self.destination_edit.text().strip(),
            "merge_home": self.merge_check.isChecked(),
            "overwrite": self.overwrite_check.isChecked(),
            "install_apps": self.install_apps_check.isChecked() and bool(selected_apps),
            "apps": selected_apps,
            "browser_data": bool(profiles),
            "browser_profiles": profiles,
            "import_passwords": passwords,
            "password_csv": self.password_edit.text().strip(),
            "password_backend": self.password_backend.currentData(),
            "keepass_database": str(Path(self.destination_edit.text()).expanduser() / "AURELIA-Passwords.kdbx") if self.password_backend.currentData() == "keepassxc" else "",
            "keepass_password": self.password_passphrase.text(),
            "password_vault": str(Path(self.destination_edit.text()).expanduser() / "AURELIA-Passwords.gpg"),
            "wallpaper": self.wallpaper_edit.text().strip(),
        }
        self.run_button.setEnabled(False)
        self.status.setText("Transferring selected data…")
        self.worker = Worker("migrate", options)
        self.worker.progress.connect(self._progress)
        self.worker.finished_ok.connect(self._migration_done)
        self.worker.failed.connect(self._worker_failed)
        self.worker.start()

    def _progress(self, message: str, done: int, total: int) -> None:
        self.status.setText(message)
        if total > 0:
            self.progress.setValue(min(1000, int(done * 1000 / total)))

    def _migration_done(self, report: dict) -> None:
        self.run_button.setEnabled(True)
        errors = report.get("errors", [])
        files = report.get("files", {})
        browser = report.get("browser", [])
        apps = report.get("apps", [])
        summary = (
            f"Copied {files.get('copied', 0)} files ({human_size(files.get('copied_bytes', 0))}), "
            f"skipped {files.get('skipped', 0)} existing files.\n"
            f"Imported browser data for {len(browser)} profile(s).\n"
            f"Application installs requested: {len(apps)}.\n\n"
            f"Report: {backend.REPORT_PATH}"
        )
        if errors:
            summary += "\n\nWarnings:\n" + "\n".join(f"• {error}" for error in errors)
        self.status.setText("Migration finished.")
        QMessageBox.information(self, "Transfer complete", summary)
        self.close()


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="AURELIA Windows migration wizard")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        failures = backend.self_test()
        if failures:
            print("FAILED: " + "; ".join(failures))
            return 1
        print("AURELIA migration UI self-test: PASS")
        return 0
    app = QApplication([sys.argv[0]])
    app.setApplicationName("AURELIA Migration")
    app.setStyleSheet(QSS)
    window = MigrationWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
