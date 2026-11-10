#!/usr/bin/env python3
"""AURELIA Backup & Restore backend.

Restic supplies content-addressed, incremental and encrypted snapshots. This
module owns the product configuration, password handling, scheduling, Windows
File History inspection and restore verification. It never pretends a backup
exists: operations read restic's JSON output and report failures verbatim.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import secrets
import shutil
import subprocess
import sys
import tempfile
import time
from contextlib import contextmanager
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Callable, Iterable, Optional

CONFIG_HOME = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
DATA_HOME = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share"))
CONFIG_PATH = CONFIG_HOME / "aurelia-backup" / "config.json"
PASSWORD_PATH = CONFIG_HOME / "aurelia-backup" / "repo.key"
STATE_PATH = CONFIG_HOME / "aurelia-backup" / "state.json"
SYSTEMD_USER = CONFIG_HOME / "systemd" / "user"
Progress = Callable[[str, int, int], None]


def _which(name: str) -> Optional[str]:
    return shutil.which(name)


def run(command: list[str], timeout: int = 120, input_text: Optional[str] = None) -> tuple[int, str, str]:
    if not command or not _which(command[0]):
        return 127, "", f"{command[0] if command else 'command'} is not installed"
    try:
        result = subprocess.run(
            command, input=input_text, capture_output=True, text=True,
            timeout=timeout, check=False,
        )
        return result.returncode, result.stdout.strip(), result.stderr.strip()
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 1, "", str(exc)


def default_config() -> dict:
    home = str(Path.home())
    return {
        "version": 1,
        "repository": "",
        "password_source": "file",
        "paths": [home],
        "exclude": [
            "**/.cache", "**/Cache", "**/node_modules", "**/.local/share/Trash",
            "**/Steam/steamapps", "**/.var/app/*/cache",
        ],
        "schedule": {"enabled": False, "calendar": "daily", "time": "02:00"},
        "created_at": 0,
        "last_backup": 0,
        "last_status": "",
    }


def load_config(path: Path = CONFIG_PATH) -> dict:
    config = default_config()
    try:
        loaded = json.loads(path.read_text(encoding="utf-8"))
        if isinstance(loaded, dict):
            config.update(loaded)
    except (OSError, ValueError):
        pass
    return config


def save_config(config: dict, path: Path = CONFIG_PATH) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(".tmp")
    temp.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    os.chmod(temp, 0o600)
    temp.replace(path)


def set_state(**changes) -> None:
    state = {}
    try:
        state = json.loads(STATE_PATH.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        pass
    state.update(changes)
    STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
    temp = STATE_PATH.with_suffix(".tmp")
    temp.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    os.chmod(temp, 0o600)
    temp.replace(STATE_PATH)


def _secret_tool_available() -> bool:
    return _which("secret-tool") is not None


def store_password(password: str, repository: str, config: dict) -> Path:
    """Store the restic password in the keyring when possible, else 0600 file."""
    if _secret_tool_available():
        code, _out, _err = run(
            ["secret-tool", "store", "--label", "AURELIA Backup", "service", "aurelia-backup",
             "repository", repository],
            timeout=30, input_text=password,
        )
        if code == 0:
            config["password_source"] = "keyring"
            return PASSWORD_PATH
    PASSWORD_PATH.parent.mkdir(parents=True, exist_ok=True)
    PASSWORD_PATH.write_text(password + "\n", encoding="utf-8")
    os.chmod(PASSWORD_PATH, 0o600)
    config["password_source"] = "file"
    return PASSWORD_PATH


def get_password(config: dict) -> str:
    repository = config.get("repository", "")
    if config.get("password_source") == "keyring" and _secret_tool_available():
        code, out, _err = run(
            ["secret-tool", "lookup", "service", "aurelia-backup", "repository", repository], timeout=20
        )
        if code == 0 and out:
            return out.splitlines()[0]
    try:
        return PASSWORD_PATH.read_text(encoding="utf-8").strip()
    except OSError:
        return ""


def create_password(config: dict) -> str:
    password = secrets.token_urlsafe(32)
    store_password(password, config.get("repository", ""), config)
    return password


@contextmanager
def _password_file(password: str):
    handle = tempfile.NamedTemporaryFile("w", prefix="aurelia-restic-", suffix=".key", delete=False)
    try:
        handle.write(password + "\n")
        handle.close()
        os.chmod(handle.name, 0o600)
        yield handle.name
    finally:
        try:
            os.unlink(handle.name)
        except OSError:
            pass


def normalise_repository(repository: str) -> str:
    value = repository.strip()
    if not value:
        return ""
    if ":" in value.split("/", 1)[0] and not value.startswith("/"):
        return value
    return str(Path(value).expanduser().resolve())


def repository_ready(config: dict) -> bool:
    password = get_password(config)
    if not password or not config.get("repository"):
        return False
    with _password_file(password) as password_file:
        code, _out, _err = run(
            ["restic", "-r", normalise_repository(config["repository"]), "--password-file", password_file, "cat", "config"],
            timeout=60,
        )
    return code == 0


def initialise_repository(config: dict) -> tuple[bool, str]:
    repository = normalise_repository(config.get("repository", ""))
    if not repository:
        return False, "Choose a backup destination first."
    password = get_password(config) or create_password(config)
    if repository.startswith("/"):
        Path(repository).mkdir(parents=True, exist_ok=True)
    with _password_file(password) as password_file:
        code, out, err = run(
            ["restic", "-r", repository, "--password-file", password_file, "init"], timeout=180
        )
    return code == 0, (out or err or "restic init")


def _restic(config: dict, arguments: list[str], timeout: int = 3600) -> tuple[int, str, str]:
    password = get_password(config)
    if not password:
        return 1, "", "The backup password is missing."
    repository = normalise_repository(config.get("repository", ""))
    if not repository:
        return 1, "", "The backup destination is not configured."
    with _password_file(password) as password_file:
        return run(
            ["restic", "-r", repository, "--password-file", password_file] + arguments,
            timeout=timeout,
        )


@dataclass
class BackupSelection:
    profile: bool = True
    documents: bool = True
    pictures: bool = True
    videos: bool = True
    music: bool = True
    desktop: bool = True
    application_configs: bool = True
    custom_paths: list[str] = field(default_factory=list)

    def paths(self) -> list[str]:
        home = Path.home()
        result = []
        mapping = {
            "documents": home / "Documents", "pictures": home / "Pictures",
            "videos": home / "Videos", "music": home / "Music", "desktop": home / "Desktop",
        }
        for key, path in mapping.items():
            if getattr(self, key) and path.is_dir():
                result.append(str(path))
        if self.application_configs:
            for path in (home / ".config", home / ".local" / "share" / "novair",
                         home / ".config" / "aurelia-setup.json"):
                if path.exists():
                    result.append(str(path))
        if self.profile:
            result = [str(home)] + result
        result.extend(self.custom_paths)
        return sorted({str(Path(item).expanduser()) for item in result})


def backup(config: dict, selection: Optional[BackupSelection] = None,
           progress: Optional[Progress] = None) -> dict:
    progress = progress or (lambda _message, _done, _total: None)
    selection = selection or BackupSelection()
    if not _which("restic"):
        return {"ok": False, "error": "restic is not installed."}
    repository = normalise_repository(config.get("repository", ""))
    if not repository:
        return {"ok": False, "error": "Choose a backup destination first."}
    if not get_password(config):
        return {"ok": False, "error": "The backup password is missing."}
    if not repository_ready(config):
        ok, detail = initialise_repository(config)
        if not ok:
            return {"ok": False, "error": f"Could not initialise the repository: {detail}"}
    paths = selection.paths()
    if not paths:
        return {"ok": False, "error": "Select at least one folder to back up."}
    arguments = ["backup", "--json", "--tag", "aurelia", "--host", os.uname().nodename if hasattr(os, "uname") else os.environ.get("COMPUTERNAME", "aurelia")]
    for pattern in config.get("exclude", []):
        arguments += ["--exclude", pattern]
    arguments += paths
    code, out, err = _restic(config, arguments, timeout=24 * 3600)
    files = 0
    bytes_done = 0
    status = ""
    for line in out.splitlines():
        try:
            event = json.loads(line)
        except ValueError:
            continue
        message_type = event.get("message_type")
        if message_type == "status":
            files = int(event.get("files_done", files))
            bytes_done = int(event.get("bytes_done", bytes_done))
            progress(f"Backing up {files} files", bytes_done, int(event.get("total_bytes", 0) or 0))
        elif message_type == "summary":
            status = event.get("snapshot_id", "")
            files = int(event.get("files_new", files))
    if code != 0:
        set_state(last_status="failed", last_error=err or "restic backup failed")
        return {"ok": False, "error": err or "restic backup failed"}
    config["last_backup"] = time.time()
    config["last_status"] = "success"
    save_config(config)
    set_state(last_status="success", last_backup=config["last_backup"], snapshot=status)
    return {"ok": True, "snapshot": status, "files": files, "bytes": bytes_done}


def snapshots(config: dict) -> list[dict]:
    if not _which("restic") or not repository_ready(config):
        return []
    code, out, err = _restic(config, ["snapshots", "--json"], timeout=120)
    if code != 0:
        return []
    try:
        payload = json.loads(out)
    except ValueError:
        return []
    return sorted(payload, key=lambda item: item.get("time", ""), reverse=True)


def check_repository(config: dict, progress: Optional[Progress] = None) -> dict:
    progress = progress or (lambda _message, _done, _total: None)
    progress("Checking repository integrity", 0, 1)
    code, out, err = _restic(config, ["check"], timeout=24 * 3600)
    progress("Repository check finished", 1, 1)
    return {"ok": code == 0, "detail": out or err}


def restore(config: dict, snapshot: str, target: str, includes: Optional[list[str]] = None,
            progress: Optional[Progress] = None) -> dict:
    progress = progress or (lambda _message, _done, _total: None)
    destination = Path(target).expanduser()
    destination.mkdir(parents=True, exist_ok=True)
    if any(destination.iterdir()):
        return {"ok": False, "error": "The restore destination is not empty. Choose a new folder so existing files are not overwritten."}
    arguments = ["restore", snapshot, "--target", str(destination)]
    for include in includes or []:
        arguments += ["--include", include]
    progress("Restoring snapshot", 0, 1)
    code, out, err = _restic(config, arguments, timeout=24 * 3600)
    progress("Restore finished", 1, 1)
    if code != 0:
        return {"ok": False, "error": err or "restic restore failed"}
    return {"ok": True, "target": str(destination), "detail": out}


def write_schedule(config: dict) -> tuple[bool, str]:
    schedule = config.get("schedule", {})
    if not schedule.get("enabled"):
        code, out, err = run(["systemctl", "--user", "disable", "--now", "aurelia-backup.timer"], timeout=30)
        return True, out or err or "Scheduled backups disabled."
    calendar = schedule.get("calendar", "daily")
    time_value = schedule.get("time", "02:00")
    if calendar == "hourly":
        on_calendar = "hourly"
    elif calendar == "weekly":
        on_calendar = f"weekly {time_value}"
    else:
        on_calendar = f"daily {time_value}"
    SYSTEMD_USER.mkdir(parents=True, exist_ok=True)
    service = """[Unit]
Description=AURELIA scheduled backup
After=graphical-session.target

[Service]
Type=oneshot
ExecStart=/usr/bin/aurelia-backup --scheduled
Nice=10
IOSchedulingClass=idle
"""
    timer = """[Unit]
Description=AURELIA backup schedule

[Timer]
OnCalendar={calendar}
Persistent=true
RandomizedDelaySec=10m

[Install]
WantedBy=timers.target
""".format(calendar=on_calendar)
    (SYSTEMD_USER / "aurelia-backup.service").write_text(service, encoding="utf-8")
    (SYSTEMD_USER / "aurelia-backup.timer").write_text(timer, encoding="utf-8")
    if _which("systemctl"):
        run(["systemctl", "--user", "daemon-reload"], timeout=30)
        code, out, err = run(["systemctl", "--user", "enable", "--now", "aurelia-backup.timer"], timeout=40)
        if code != 0:
            return False, err or out or "Could not enable the user timer."
        return True, "Scheduled backups enabled."
    return False, "systemd is not available; the timer was written but cannot be enabled here."


def scheduled_backup() -> int:
    config = load_config()
    selection = config.get("selection")
    typed = BackupSelection(**selection) if isinstance(selection, dict) else BackupSelection()
    result = backup(config, typed)
    set_state(last_status="success" if result.get("ok") else "failed", last_error=result.get("error", ""))
    return 0 if result.get("ok") else 1


# ---------------------------------------------------------------------------
# Windows backup import
# ---------------------------------------------------------------------------


def _human_size(value: int) -> str:
    size = float(value)
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if size < 1024 or unit == "TiB":
            return f"{size:.1f} {unit}"
        size /= 1024
    return f"{size:.1f} TiB"


@dataclass
class BackupGroup:
    key: str
    label: str
    path: str
    size: int
    file_count: int
    selected: bool = True
    kind: str = "folder"


def _tree_stats(path: Path) -> tuple[int, int]:
    size = 0
    count = 0
    try:
        for entry in path.rglob("*"):
            try:
                if entry.is_file() and not entry.is_symlink():
                    size += entry.stat().st_size
                    count += 1
            except OSError:
                continue
    except OSError:
        pass
    return size, count


def scan_windows_backup(root: Path) -> dict:
    """Inspect File History, a mounted Windows image, or a folder backup."""
    if not root.is_dir():
        return {"ok": False, "error": "The backup location does not exist.", "groups": [], "kind": ""}
    file_history = root / "FileHistory" if (root / "FileHistory").is_dir() else (
        root if root.name.lower() == "filehistory" else None
    )
    groups: list[BackupGroup] = []
    kind = ""
    if file_history is not None:
        kind = "file-history"
        for user_dir in sorted(item for item in file_history.iterdir() if item.is_dir()):
            if user_dir.name.lower() == "configuration":
                continue
            for candidate in sorted(user_dir.rglob("*")):
                if not candidate.is_dir():
                    continue
                if candidate.name.lower() in {"desktop", "documents", "downloads", "pictures", "videos", "music"}:
                    if any(parent.name.lower() == candidate.name.lower() for parent in candidate.parents if parent != candidate):
                        continue
                    size, count = _tree_stats(candidate)
                    if count:
                        groups.append(BackupGroup(
                            key=f"{user_dir.name}/{candidate.name}",
                            label=f"{user_dir.name} · {candidate.name}",
                            path=str(candidate),
                            size=size,
                            file_count=count,
                        ))
    image_dir = root / "WindowsImageBackup"
    if image_dir.is_dir():
        kind = "windows-image"
        images = list(image_dir.rglob("*.vhdx"))
        return {
            "ok": True,
            "kind": kind,
            "groups": [],
            "images": [str(item) for item in images],
            "message": "A Windows system image was found. Mount its VHDX read-only before restoring files; the image is not silently mounted or modified.",
        }
    if not groups:
        kind = "folder" if kind == "" else kind
        known = {"desktop", "documents", "downloads", "pictures", "videos", "music"}
        candidates = sorted(root.iterdir())
        for candidate in candidates:
            if not candidate.is_dir():
                continue
            if candidate.name.lower() in known or kind != "file-history":
                size, count = _tree_stats(candidate)
                if count:
                    groups.append(BackupGroup(candidate.name, candidate.name, str(candidate), size, count))
    if not groups:
        return {"ok": False, "kind": kind or "unknown", "groups": [], "error": "No supported Windows backup folders were found."}
    return {
        "ok": True,
        "kind": kind or "folder",
        "groups": [asdict(group) for group in groups],
        "total_bytes": sum(group.size for group in groups),
        "message": f"Found {len(groups)} restore group(s), {_human_size(sum(group.size for group in groups))}.",
    }


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def restore_windows_backup(
    groups: Iterable[dict], destination: Path, overwrite: bool = False,
    verify: bool = True, progress: Optional[Progress] = None,
) -> dict:
    progress = progress or (lambda _message, _done, _total: None)
    destination.mkdir(parents=True, exist_ok=True)
    selected = [group for group in groups if group.get("selected", True)]
    total = sum(int(group.get("size", 0)) for group in selected)
    done = 0
    copied = skipped = 0
    errors: list[str] = []
    verified = 0
    for group in selected:
        source_root = Path(str(group.get("path", "")))
        if not source_root.is_dir():
            errors.append(f"Missing source: {source_root}")
            continue
        target_root = destination / str(group.get("label") or source_root.name)
        for source in source_root.rglob("*"):
            if source.is_symlink() or not source.is_file():
                continue
            relative = source.relative_to(source_root)
            target = target_root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists() and not overwrite:
                skipped += 1
                continue
            try:
                shutil.copy2(source, target)
                copied += 1
                done += source.stat().st_size
                if verify:
                    if _sha256(source) == _sha256(target):
                        verified += 1
                    else:
                        errors.append(f"Hash mismatch: {relative}")
                progress(f"Restored {relative}", done, total)
            except OSError as exc:
                errors.append(f"{source}: {exc}")
    return {
        "ok": not errors,
        "copied": copied,
        "skipped": skipped,
        "verified": verified,
        "errors": errors,
        "destination": str(destination),
    }


def _contained_files(root: Path) -> dict[str, int]:
    return {str(path.relative_to(root)): path.stat().st_size for path in root.rglob("*") if path.is_file()}


def self_test() -> list[str]:
    failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="aurelia-backup-") as temporary:
        base = Path(temporary)
        history = base / "FileHistory" / "Ada"
        documents = history / "C" / "Users" / "Ada" / "Documents"
        documents.mkdir(parents=True)
        (documents / "letter.txt").write_text("AURELIA backup test\n", encoding="utf-8")
        scan = scan_windows_backup(base)
        if not scan.get("ok") or scan.get("kind") != "file-history":
            failures.append("File History was not detected")
        elif not scan.get("groups"):
            failures.append("File History groups were empty")
        else:
            destination = base / "restore"
            result = restore_windows_backup(scan["groups"], destination, progress=lambda *_args: None)
            if not result["ok"] or result["copied"] < 1 or result["verified"] < 1:
                failures.append("File History restore or verification failed")
        config = default_config()
        config["repository"] = str(base / "repo")
        config["password_source"] = "file"
        save_config(config, base / "config.json")
        loaded = load_config(base / "config.json")
        if loaded["repository"] != config["repository"]:
            failures.append("backup config round-trip failed")
        selection = BackupSelection(
            profile=False, documents=True, pictures=False, videos=False, music=False,
            desktop=False, application_configs=False, custom_paths=[str(documents)],
        )
        selected = selection.paths()
        if not selected or str(documents) not in selected:
            failures.append("backup selection did not include the test folder")
        if not hasattr(BackupSelection(), "custom_paths"):
            failures.append("backup selection lacks custom paths")
    return failures


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="AURELIA Backup & Restore")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--scheduled", action="store_true")
    parser.add_argument("--snapshots", action="store_true")
    parser.add_argument("--backup", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        failures = self_test()
        if failures:
            print("FAILED: " + "; ".join(failures))
            return 1
        print("AURELIA backup self-test: PASS")
        return 0
    if args.scheduled:
        return scheduled_backup()
    config = load_config()
    if args.snapshots:
        print(json.dumps(snapshots(config), indent=2))
        return 0
    if args.backup:
        result = backup(config, BackupSelection())
        print(json.dumps(result, indent=2))
        return 0 if result.get("ok") else 1
    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
