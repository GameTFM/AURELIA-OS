#!/usr/bin/env python3
"""AURELIA Backup & Restore desktop application."""
from __future__ import annotations

import argparse
import datetime as dt
import os
import sys
from pathlib import Path
from typing import Optional

from PyQt6.QtCore import QThread, Qt, pyqtSignal
from PyQt6.QtGui import QIcon
from PyQt6.QtWidgets import (
    QApplication, QCheckBox, QComboBox, QFileDialog, QFrame, QHBoxLayout,
    QLabel, QLineEdit, QMainWindow, QMessageBox, QProgressBar, QPushButton,
    QSpinBox, QTabWidget, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget,
)

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import aurelia_backup as backend  # noqa: E402

ICON = "/usr/share/icons/hicolor/512x512/apps/aurelia-logo.png"
QSS = """
QWidget { background: #080d13; color: #dfeaf0; font-family: Inter, sans-serif; font-size: 13px; }
QLabel#title { font-family: "Playfair Display"; font-size: 30px; color: #f6f1e6; }
QLabel#sub { color: #8ea7b4; }
QLabel#section { color: #d6b878; font-size: 10px; font-weight: 700; letter-spacing: 2px; }
QTabWidget::pane { border: 1px solid #203541; border-radius: 8px; }
QTabBar::tab { background: #0b141d; padding: 9px 15px; border: 0; color: #9eb2bd; }
QTabBar::tab:selected { color: #f0d9a7; border-bottom: 2px solid #c9a96a; }
QFrame#card { background: #0c1720; border: 1px solid #1f3543; border-radius: 10px; }
QPushButton { background: #121f2a; border: 1px solid #2a4050; border-radius: 7px; padding: 8px 14px; }
QPushButton#primary { background: #c9a96a; color: #101820; border: 0; font-weight: 700; }
QLineEdit, QComboBox, QSpinBox { background: #0b141d; border: 1px solid #263c4a; border-radius: 7px; padding: 7px; }
QTableWidget { background: #0b141d; gridline-color: #1d2f3c; border: 1px solid #263c4a; }
QHeaderView::section { background: #10202b; color: #9eb2bd; padding: 6px; border: 0; }
QProgressBar { max-height: 5px; border: 0; background: #172732; }
QProgressBar::chunk { background: #c9a96a; }
"""


class Worker(QThread):
    progress = pyqtSignal(str, int, int)
    finished_ok = pyqtSignal(object)
    failed = pyqtSignal(str)

    def __init__(self, action: str, payload: Optional[dict] = None) -> None:
        super().__init__()
        self.action = action
        self.payload = payload or {}

    def run(self) -> None:
        try:
            if self.action == "backup":
                result = backend.backup(
                    self.payload["config"], self.payload["selection"],
                    progress=lambda message, done, total: self.progress.emit(message, done, total),
                )
            elif self.action == "check":
                result = backend.check_repository(
                    self.payload["config"], progress=lambda message, done, total: self.progress.emit(message, done, total)
                )
            elif self.action == "restore":
                result = backend.restore(
                    self.payload["config"], self.payload["snapshot"], self.payload["target"],
                    self.payload.get("includes", []),
                    progress=lambda message, done, total: self.progress.emit(message, done, total),
                )
            elif self.action == "scan-backup":
                result = backend.scan_windows_backup(Path(self.payload["root"]))
            elif self.action == "restore-windows":
                result = backend.restore_windows_backup(
                    self.payload["groups"], Path(self.payload["target"]),
                    overwrite=self.payload.get("overwrite", False), verify=True,
                    progress=lambda message, done, total: self.progress.emit(message, done, total),
                )
            else:
                result = {"ok": False, "error": "Unknown action"}
            self.finished_ok.emit(result)
        except Exception as exc:  # noqa: BLE001 - the dialog must show the actual failure
            self.failed.emit(str(exc))


class BackupWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("AURELIA Backup & Restore")
        self.resize(1080, 760)
        self.setMinimumSize(860, 620)
        if Path(ICON).is_file():
            self.setWindowIcon(QIcon(ICON))
        self.config = backend.load_config()
        self.snapshots: list[dict] = []
        self.scan_groups: list[dict] = []
        self.worker: Optional[Worker] = None
        self._build()
        self._load_schedule()
        self.refresh_snapshots()

    # ------------------------------------------------------------------ UI
    def _build(self) -> None:
        root = QWidget()
        self.setCentralWidget(root)
        layout = QVBoxLayout(root)
        layout.setContentsMargins(28, 24, 28, 20)
        title = QLabel("Backup & Restore")
        title.setObjectName("title")
        layout.addWidget(title)
        subtitle = QLabel(
            "Encrypted, incremental snapshots of your files and settings. "
            "Every restore is verified against the source before AURELIA reports success."
        )
        subtitle.setObjectName("sub")
        subtitle.setWordWrap(True)
        layout.addWidget(subtitle)
        layout.addSpacing(10)

        destination = QFrame()
        destination.setObjectName("card")
        dest_layout = QVBoxLayout(destination)
        dest_layout.setContentsMargins(16, 12, 16, 12)
        row = QHBoxLayout()
        row.addWidget(QLabel("Destination"))
        self.repository_edit = QLineEdit(self.config.get("repository", ""))
        self.repository_edit.setPlaceholderText("/media/you/AURELIA-Backup · /mnt/nas/backup · sftp:host:/backup")
        row.addWidget(self.repository_edit, 1)
        browse = QPushButton("Browse…")
        browse.clicked.connect(self._browse_repository)
        row.addWidget(browse)
        dest_layout.addLayout(row)
        row2 = QHBoxLayout()
        self.password_label = QLabel(self._password_hint())
        self.password_label.setObjectName("sub")
        row2.addWidget(self.password_label, 1)
        new_password = QPushButton("Set / change password…")
        new_password.clicked.connect(self._set_password)
        row2.addWidget(new_password)
        save = QPushButton("Save destination")
        save.clicked.connect(self._save_repository)
        row2.addWidget(save)
        dest_layout.addLayout(row2)
        layout.addWidget(destination)

        tabs = QTabWidget()
        tabs.addTab(self._backup_tab(), "Back up")
        tabs.addTab(self._restore_tab(), "Restore")
        tabs.addTab(self._windows_tab(), "Import Windows backup")
        tabs.addTab(self._schedule_tab(), "Schedule")
        layout.addWidget(tabs, 1)

        self.progress = QProgressBar()
        self.progress.setRange(0, 1000)
        self.progress.setTextVisible(False)
        layout.addWidget(self.progress)
        self.status = QLabel("Ready.")
        self.status.setObjectName("sub")
        self.status.setWordWrap(True)
        layout.addWidget(self.status)

    def _password_hint(self) -> str:
        source = self.config.get("password_source")
        if backend.get_password(self.config):
            return "Repository password is stored " + (
                "in the system keyring." if source == "keyring" else f"in {backend.PASSWORD_PATH} (mode 0600)."
            )
        return "No password yet. One will be generated and stored in the system keyring or a 0600 file."

    def _backup_tab(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        note = QLabel("Choose what to include. Personal folders are backed up as real files; application data is limited to supported configuration directories.")
        note.setObjectName("sub")
        note.setWordWrap(True)
        layout.addWidget(note)
        self.selection_checks: dict[str, QCheckBox] = {}
        labels = {
            "profile": "Entire user profile (home directory)",
            "documents": "Documents", "pictures": "Pictures", "videos": "Videos",
            "music": "Music", "desktop": "Desktop",
            "application_configs": "Application configurations and NOVAIR data",
        }
        for key, label in labels.items():
            check = QCheckBox(label)
            check.setChecked(key not in {"profile"})
            self.selection_checks[key] = check
            layout.addWidget(check)
        self.custom_paths: list[str] = []
        self.custom_label = QLabel("No additional folders.")
        self.custom_label.setObjectName("sub")
        layout.addWidget(self.custom_label)
        row = QHBoxLayout()
        add = QPushButton("Add folder…")
        add.clicked.connect(self._add_path)
        row.addWidget(add)
        row.addStretch(1)
        start = QPushButton("Back up now")
        start.setObjectName("primary")
        start.clicked.connect(self.start_backup)
        row.addWidget(start)
        layout.addLayout(row)
        layout.addStretch(1)
        return page

    def _restore_tab(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        row = QHBoxLayout()
        row.addWidget(QLabel("Snapshots in this repository"))
        row.addStretch(1)
        refresh = QPushButton("Refresh")
        refresh.clicked.connect(self.refresh_snapshots)
        row.addWidget(refresh)
        verify = QPushButton("Verify repository")
        verify.clicked.connect(self.check_repository)
        row.addWidget(verify)
        layout.addLayout(row)
        self.snapshot_table = QTableWidget(0, 5)
        self.snapshot_table.setHorizontalHeaderLabels(["Date", "Host", "Files", "Size", "Snapshot"])
        self.snapshot_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.snapshot_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.snapshot_table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.snapshot_table, 1)
        row2 = QHBoxLayout()
        row2.addWidget(QLabel("Restore into"))
        self.restore_target = QLineEdit(str(Path.home() / "AURELIA Restore" / dt.datetime.now().strftime("%Y-%m-%d %H%M")))
        row2.addWidget(self.restore_target, 1)
        choose = QPushButton("Choose empty folder…")
        choose.clicked.connect(self._choose_restore_target)
        row2.addWidget(choose)
        layout.addLayout(row2)
        row3 = QHBoxLayout()
        row3.addWidget(QLabel("Only paths containing"))
        self.restore_include = QLineEdit()
        self.restore_include.setPlaceholderText("Optional, e.g. Documents or report.pdf")
        row3.addWidget(self.restore_include, 1)
        restore = QPushButton("Restore snapshot")
        restore.setObjectName("primary")
        restore.clicked.connect(self.restore_snapshot)
        row3.addWidget(restore)
        layout.addLayout(row3)
        warning = QLabel("Restores never overwrite an existing folder. AURELIA restores into a new destination and verifies the result before you merge or move it.")
        warning.setObjectName("sub")
        warning.setWordWrap(True)
        layout.addWidget(warning)
        return page

    def _windows_tab(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        note = QLabel(
            "Import a Windows File History backup, a folder backup, or a mounted WindowsImageBackup. "
            "The source is read only; files are copied to a new destination and hashed for verification. "
            "Windows system images are detected but never mounted automatically."
        )
        note.setObjectName("sub")
        note.setWordWrap(True)
        layout.addWidget(note)
        row = QHBoxLayout()
        row.addWidget(QLabel("Backup location"))
        self.windows_source = QLineEdit()
        self.windows_source.setPlaceholderText("D:\\FileHistory or /media/you/Backup")
        row.addWidget(self.windows_source, 1)
        browse = QPushButton("Browse…")
        browse.clicked.connect(self._browse_windows_source)
        row.addWidget(browse)
        scan = QPushButton("Scan")
        scan.clicked.connect(self.scan_windows)
        row.addWidget(scan)
        layout.addLayout(row)
        self.windows_table = QTableWidget(0, 4)
        self.windows_table.setHorizontalHeaderLabels(["Restore", "Group", "Files", "Size"])
        self.windows_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.windows_table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.windows_table, 1)
        self.windows_message = QLabel("Select a backup and scan it.")
        self.windows_message.setObjectName("sub")
        self.windows_message.setWordWrap(True)
        layout.addWidget(self.windows_message)
        row2 = QHBoxLayout()
        row2.addWidget(QLabel("Restore to"))
        self.windows_target = QLineEdit(str(Path.home() / "Windows Restore" / dt.datetime.now().strftime("%Y-%m-%d")))
        row2.addWidget(self.windows_target, 1)
        choose = QPushButton("Choose folder…")
        choose.clicked.connect(self._choose_windows_target)
        row2.addWidget(choose)
        layout.addLayout(row2)
        self.windows_overwrite = QCheckBox("Overwrite existing files (requires confirmation)")
        layout.addWidget(self.windows_overwrite)
        start = QPushButton("Restore selected files")
        start.setObjectName("primary")
        start.clicked.connect(self.restore_windows)
        layout.addWidget(start, alignment=Qt.AlignmentFlag.AlignRight)
        return page

    def _schedule_tab(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        self.schedule_enabled = QCheckBox("Keep backups up to date automatically")
        layout.addWidget(self.schedule_enabled)
        row = QHBoxLayout()
        row.addWidget(QLabel("Frequency"))
        self.schedule_frequency = QComboBox()
        self.schedule_frequency.addItems(["daily", "weekly", "hourly"])
        row.addWidget(self.schedule_frequency)
        row.addWidget(QLabel("Time"))
        self.schedule_time = QLineEdit("02:00")
        self.schedule_time.setMaximumWidth(80)
        row.addWidget(self.schedule_time)
        row.addStretch(1)
        save = QPushButton("Save schedule")
        save.setObjectName("primary")
        save.clicked.connect(self.save_schedule)
        row.addWidget(save)
        layout.addLayout(row)
        note = QLabel(
            "Scheduled backups run through a systemd user timer. The destination and "
            "password must already be configured. A missed run is started after login "
            "because Persistent=true is set on the timer."
        )
        note.setObjectName("sub")
        note.setWordWrap(True)
        layout.addWidget(note)
        layout.addStretch(1)
        return page

    # ------------------------------------------------------------- helpers
    def _load_schedule(self) -> None:
        schedule = self.config.get("schedule", {})
        self.schedule_enabled.setChecked(bool(schedule.get("enabled")))
        frequency = schedule.get("calendar", "daily")
        index = self.schedule_frequency.findText(frequency)
        if index >= 0:
            self.schedule_frequency.setCurrentIndex(index)
        self.schedule_time.setText(schedule.get("time", "02:00"))

    def _browse_repository(self) -> None:
        path = QFileDialog.getExistingDirectory(self, "Choose backup destination", str(Path.home()))
        if path:
            self.repository_edit.setText(path)

    def _save_repository(self) -> None:
        repository = backend.normalise_repository(self.repository_edit.text())
        if not repository:
            QMessageBox.warning(self, "Destination", "Choose a local folder or enter a restic repository URL.")
            return
        self.config["repository"] = repository
        if not backend.get_password(self.config):
            password = backend.create_password(self.config)
            QMessageBox.information(
                self, "Recovery key created",
                "A random repository password was created. It is stored in the system "
                f"keyring when available, otherwise in {backend.PASSWORD_PATH} with owner-only "
                "permissions. Without this password the encrypted backup cannot be recovered. "
                "Record it in a safe place before relying on the backup.",
            )
            del password
        backend.save_config(self.config)
        self.password_label.setText(self._password_hint())
        self.status.setText("Destination saved. Initialising the repository happens on the first backup.")
        self.refresh_snapshots()

    def _set_password(self) -> None:
        from PyQt6.QtWidgets import QInputDialog
        password, accepted = QInputDialog.getText(
            self, "Repository password", "New password (at least 12 characters):",
            QLineEdit.EchoMode.Password,
        )
        if not accepted:
            return
        if len(password) < 12:
            QMessageBox.warning(self, "Password too short", "Use at least 12 characters for the backup password.")
            return
        backend.store_password(password, self.config.get("repository", ""), self.config)
        backend.save_config(self.config)
        self.password_label.setText(self._password_hint())
        QMessageBox.information(
            self, "Password changed",
            "Use this password for every future restore and on any other computer. "
            "It cannot be recovered from AURELIA.",
        )

    def _add_path(self) -> None:
        path = QFileDialog.getExistingDirectory(self, "Add a folder to every backup", str(Path.home()))
        if path and path not in self.custom_paths:
            self.custom_paths.append(path)
            self.custom_label.setText("Additional folders: " + ", ".join(self.custom_paths))

    def _choose_restore_target(self) -> None:
        path = QFileDialog.getExistingDirectory(self, "Choose an empty restore folder", str(Path.home()))
        if path:
            self.restore_target.setText(path)

    def _choose_windows_target(self) -> None:
        path = QFileDialog.getExistingDirectory(self, "Choose a restore folder", str(Path.home()))
        if path:
            self.windows_target.setText(path)

    def _browse_windows_source(self) -> None:
        path = QFileDialog.getExistingDirectory(self, "Choose Windows backup", str(Path.home()))
        if path:
            self.windows_source.setText(path)

    # ------------------------------------------------------------- actions
    def _selected_selection(self) -> backend.BackupSelection:
        return backend.BackupSelection(
            profile=self.selection_checks["profile"].isChecked(),
            documents=self.selection_checks["documents"].isChecked(),
            pictures=self.selection_checks["pictures"].isChecked(),
            videos=self.selection_checks["videos"].isChecked(),
            music=self.selection_checks["music"].isChecked(),
            desktop=self.selection_checks["desktop"].isChecked(),
            application_configs=self.selection_checks["application_configs"].isChecked(),
            custom_paths=list(self.custom_paths),
        )

    def _start_worker(self, action: str, payload: Optional[dict] = None) -> None:
        if self.worker is not None and self.worker.isRunning():
            QMessageBox.information(self, "Please wait", "Another backup operation is still running.")
            return
        self.worker = Worker(action, payload)
        self.worker.progress.connect(self._progress)
        self.worker.finished_ok.connect(self._finished)
        self.worker.failed.connect(self._failed)
        self.worker.start()

    def start_backup(self) -> None:
        if not self.repository_edit.text().strip():
            QMessageBox.warning(self, "Destination", "Choose a backup destination first.")
            return
        self.config["repository"] = backend.normalise_repository(self.repository_edit.text())
        selection = self._selected_selection()
        if not selection.paths():
            QMessageBox.warning(self, "Nothing selected", "Select at least one folder to back up.")
            return
        backend.save_config(self.config)
        self.status.setText("Starting encrypted incremental backup…")
        self._start_worker("backup", {"config": self.config, "selection": selection})

    def refresh_snapshots(self) -> None:
        self.snapshots = backend.snapshots(self.config)
        self.snapshot_table.setRowCount(len(self.snapshots))
        for row, snapshot in enumerate(self.snapshots):
            self.snapshot_table.setItem(row, 0, QTableWidgetItem(snapshot.get("time", "")[:19].replace("T", " ")))
            self.snapshot_table.setItem(row, 1, QTableWidgetItem(snapshot.get("hostname", "")))
            self.snapshot_table.setItem(row, 2, QTableWidgetItem(str(snapshot.get("files_new", snapshot.get("file_count", "")))))
            summary = snapshot.get("summary") if isinstance(snapshot.get("summary"), dict) else {}
            size = int(summary.get("total_bytes_processed", 0) or 0)
            self.snapshot_table.setItem(row, 3, QTableWidgetItem(f"{size / 1048576:.1f} MiB" if size else "—"))
            self.snapshot_table.setItem(row, 4, QTableWidgetItem(snapshot.get("short_id", snapshot.get("id", ""))))
        if self.snapshots:
            self.status.setText(f"{len(self.snapshots)} snapshot(s) available.")
        elif self.config.get("repository") and backend.get_password(self.config):
            self.status.setText("No snapshots yet, or the repository has not been initialised.")

    def check_repository(self) -> None:
        if not self.config.get("repository"):
            QMessageBox.warning(self, "Destination", "Choose a destination first.")
            return
        self.status.setText("Verifying the repository…")
        self._start_worker("check", {"config": self.config})

    def restore_snapshot(self) -> None:
        row = self.snapshot_table.currentRow()
        if row < 0 or row >= len(self.snapshots):
            QMessageBox.warning(self, "Restore", "Select a snapshot first.")
            return
        target = self.restore_target.text().strip()
        snapshot = self.snapshots[row]
        if QMessageBox.question(
            self, "Restore snapshot",
            f"Restore snapshot {snapshot.get('short_id', '')} into:\n{target}\n\n"
            "The destination must be empty. Existing files are never overwritten by this action.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        ) != QMessageBox.StandardButton.Yes:
            return
        includes = [self.restore_include.text().strip()] if self.restore_include.text().strip() else []
        self.status.setText("Restoring snapshot…")
        self._start_worker("restore", {"config": self.config, "snapshot": snapshot.get("id", ""),
                                       "target": target, "includes": includes})

    def scan_windows(self) -> None:
        raw = self.windows_source.text().strip()
        if not raw:
            QMessageBox.warning(self, "Backup location", "Choose a Windows backup location first.")
            return
        path = Path(raw)
        if not path.is_dir():
            QMessageBox.warning(self, "Backup location", "That folder does not exist or is not readable.")
            return
        self.status.setText("Scanning Windows backup…")
        self._start_worker("scan-backup", {"root": str(path)})

    def restore_windows(self) -> None:
        selected = []
        for row, group in enumerate(self.scan_groups):
            item = self.windows_table.item(row, 0)
            if item is not None and item.checkState() == Qt.CheckState.Checked:
                selected.append({**group, "selected": True})
        if not selected:
            QMessageBox.warning(self, "Nothing selected", "Choose at least one group to restore.")
            return
        target = self.windows_target.text().strip()
        if not target:
            QMessageBox.warning(self, "Destination", "Choose a restore destination first.")
            return
        overwrite = self.windows_overwrite.isChecked()
        warning = (
            f"Restore {len(selected)} selected group(s) into:\n{target}\n\n"
            "The source is read only. Every restored file is hashed for verification."
        )
        if overwrite:
            warning += "\n\nExisting destination files may be replaced. This cannot be undone."
        if QMessageBox.question(
            self, "Confirm restore", warning,
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        ) != QMessageBox.StandardButton.Yes:
            return
        self.status.setText("Restoring Windows files and verifying hashes…")
        self._start_worker("restore-windows", {"groups": selected, "target": target, "overwrite": overwrite})

    def save_schedule(self) -> None:
        calendar = self.schedule_frequency.currentText()
        time_value = self.schedule_time.text().strip() or "02:00"
        if calendar != "hourly" and not __import__("re").match(r"^([01]?[0-9]|2[0-3]):[0-5][0-9]$", time_value):
            QMessageBox.warning(self, "Time", "Use 24-hour time, for example 02:30.")
            return
        self.config["schedule"] = {
            "enabled": self.schedule_enabled.isChecked(),
            "calendar": calendar,
            "time": time_value,
        }
        self.config["selection"] = self._selected_selection().__dict__
        backend.save_config(self.config)
        ok, detail = backend.write_schedule(self.config)
        if ok:
            QMessageBox.information(self, "Schedule", detail)
        else:
            QMessageBox.warning(self, "Schedule", detail)
        self.status.setText(detail)

    # -------------------------------------------------------------- results
    def _progress(self, message: str, done: int, total: int) -> None:
        self.status.setText(message)
        if total > 0:
            self.progress.setValue(min(1000, int(done * 1000 / total)))

    def _failed(self, message: str) -> None:
        self.status.setText("Failed: " + message)
        QMessageBox.warning(self, "Backup & Restore", message)

    def _finished(self, result: object) -> None:
        data = result if isinstance(result, dict) else {}
        action = self.worker.action if self.worker else ""
        if data.get("ok") is False:
            self._failed(str(data.get("error") or data.get("detail") or "The operation failed."))
            return
        if action == "backup":
            size = int(data.get("bytes", 0) or 0)
            self.status.setText(f"Backup complete · {data.get('files', 0)} files · {size / 1048576:.1f} MiB.")
            self.refresh_snapshots()
            QMessageBox.information(self, "Backup complete",
                                    f"Snapshot {data.get('snapshot', '')} was created and is listed in Restore.")
        elif action == "check":
            QMessageBox.information(self, "Repository check", "The repository passed restic's integrity check.")
            self.status.setText("Repository verified.")
        elif action == "restore":
            self.status.setText("Restore complete.")
            QMessageBox.information(self, "Restore complete", f"Restored into:\n{data.get('target', '')}")
        elif action == "scan-backup":
            self.scan_groups = data.get("groups", [])
            self.windows_table.setRowCount(len(self.scan_groups))
            for row, group in enumerate(self.scan_groups):
                check = QTableWidgetItem()
                check.setFlags(check.flags() | Qt.ItemFlag.ItemIsUserCheckable)
                check.setCheckState(Qt.CheckState.Checked)
                self.windows_table.setItem(row, 0, check)
                self.windows_table.setItem(row, 1, QTableWidgetItem(str(group.get("label", ""))))
                self.windows_table.setItem(row, 2, QTableWidgetItem(str(group.get("file_count", 0))))
                self.windows_table.setItem(row, 3, QTableWidgetItem(f"{int(group.get('size', 0)) / 1048576:.1f} MiB"))
            message = data.get("message") or data.get("error") or ""
            if data.get("images"):
                message += "\nSystem image files: " + ", ".join(data["images"])
            self.windows_message.setText(message)
            self.status.setText(message)
        elif action == "restore-windows":
            self.status.setText("Windows backup restored and verified.")
            details = (
                f"Copied {data.get('copied', 0)} files, skipped {data.get('skipped', 0)}, "
                f"verified {data.get('verified', 0)} hashes."
            )
            errors = data.get("errors", [])
            if errors:
                details += "\n\nWarnings:\n" + "\n".join(f"• {error}" for error in errors[:8])
            QMessageBox.information(self, "Restore complete", details)


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="AURELIA Backup & Restore")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        failures = backend.self_test()
        if failures:
            print("FAILED: " + "; ".join(failures))
            return 1
        print("AURELIA backup UI self-test: PASS")
        return 0
    app = QApplication([sys.argv[0]])
    app.setApplicationName("AURELIA Backup & Restore")
    app.setStyleSheet(QSS)
    window = BackupWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
