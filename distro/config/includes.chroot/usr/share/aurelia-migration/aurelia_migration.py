#!/usr/bin/env python3
"""AURELIA Windows migration backend.

The module deliberately uses only the Python standard library at import time.
Optional system tools (rsync, udisksctl, pkexec, pefile, wrestool, pkcon,
flatpak and keepassxc-cli) are detected at runtime. Every operation that moves
user data reports what it actually did; there is no simulated progress.

Windows files are opened read-only. This code never formats, deletes or writes
to the source installation.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Callable, Iterable, Optional

APP_ROOT = Path(__file__).resolve().parent
CATALOG_PATH = APP_ROOT / "app_catalog.json"
DATA_HOME = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share"))
CONFIG_HOME = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
REPORT_PATH = DATA_HOME / "aurelia-migration" / "last-report.json"
NOVAIR_DB = DATA_HOME / "novair" / "novair.db"

Progress = Callable[[str, int, int], None]


def _which(name: str) -> Optional[str]:
    return shutil.which(name)


def run(command: list[str], timeout: int = 60, input_text: Optional[str] = None) -> tuple[int, str, str]:
    if not command or not _which(command[0]):
        return 127, "", f"{command[0] if command else 'command'} is not installed"
    try:
        result = subprocess.run(
            command,
            input=input_text,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
        return result.returncode, result.stdout.strip(), result.stderr.strip()
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 1, "", str(exc)


def _unescape_mount(value: str) -> str:
    return re.sub(r"\\([0-7]{3})", lambda m: chr(int(m.group(1), 8)), value)


def mounted_filesystems() -> list[tuple[str, str]]:
    """Return (device, mountpoint) pairs from /proc/mounts."""
    result: list[tuple[str, str]] = []
    try:
        lines = Path("/proc/mounts").read_text(encoding="utf-8").splitlines()
    except OSError:
        lines = []
    for line in lines:
        parts = line.split()
        if len(parts) >= 2:
            result.append((_unescape_mount(parts[0]), _unescape_mount(parts[1])))
    return result


def is_windows_root(path: Path) -> bool:
    return (path / "Windows" / "System32").is_dir() and (path / "Users").is_dir()


@dataclass
class WindowsInstallation:
    root: str
    label: str
    device: str = ""
    users: list[str] = field(default_factory=list)
    mounted: bool = True
    error: str = ""


@dataclass
class Application:
    name: str
    version: str = ""
    publisher: str = ""
    source_path: str = ""
    icon_path: str = ""
    kind: str = "windows"
    linux_name: str = ""
    linux_kind: str = ""
    linux_id: str = ""
    compatible: bool = False
    manual: bool = True
    alternative: str = ""
    note: str = ""
    selected: bool = False


@dataclass
class MigrationOptions:
    source_root: str = ""
    windows_user: str = ""
    groups: list[str] = field(default_factory=list)
    destination: str = ""
    merge_home: bool = False
    overwrite: bool = False
    install_apps: bool = False
    apps: list[dict] = field(default_factory=list)
    browser_data: bool = True
    browser_profiles: list[str] = field(default_factory=list)
    import_passwords: bool = False
    password_csv: str = ""
    password_vault: str = ""
    password_backend: str = "gpg"
    keepass_database: str = ""
    keepass_password: str = ""
    wallpaper: str = ""
    dry_run: bool = False


def _expand_candidates() -> list[Path]:
    roots: list[Path] = []
    for base in (Path("/media"), Path("/mnt"), Path("/run/media")):
        if not base.is_dir():
            continue
        try:
            for child in base.iterdir():
                roots.append(child)
                if child.is_dir():
                    try:
                        roots.extend(grand for grand in child.iterdir() if grand.is_dir())
                    except OSError:
                        pass
        except OSError:
            pass
    return roots


def discover_installations() -> list[WindowsInstallation]:
    """Find mounted Windows installations and unmounted Windows partitions."""
    found: list[WindowsInstallation] = []
    seen_roots: set[str] = set()
    for device, mountpoint in mounted_filesystems():
        root = Path(mountpoint)
        if is_windows_root(root):
            users = sorted(
                entry.name
                for entry in (root / "Users").iterdir()
                if entry.is_dir() and entry.name.lower() not in {"public", "default", "default user", "all users"}
            ) if (root / "Users").is_dir() else []
            found.append(WindowsInstallation(str(root), f"Windows on {device}", device, users))
            seen_roots.add(str(root.resolve()))
    for root in _expand_candidates():
        try:
            resolved = str(root.resolve())
        except OSError:
            continue
        if resolved in seen_roots or not is_windows_root(root):
            continue
        users = sorted(
            entry.name for entry in (root / "Users").iterdir()
            if entry.is_dir() and entry.name.lower() not in {"public", "default", "default user", "all users"}
        )
        found.append(WindowsInstallation(str(root), f"Windows at {root}", "", users))
        seen_roots.add(resolved)

    code, output, _ = run(
        ["lsblk", "-J", "-o", "NAME,TYPE,FSTYPE,LABEL,SIZE,MOUNTPOINT"], timeout=15
    )
    if code == 0:
        try:
            payload = json.loads(output)
        except ValueError:
            payload = {}
        for device in payload.get("blockdevices", []):
            for child in device.get("children") or []:
                if child.get("fstype") not in {"ntfs", "ntfs3", "fuseblk"}:
                    continue
                if child.get("mountpoint"):
                    continue
                name = child.get("name") or child.get("path") or ""
                path = child.get("path") or f"/dev/{name}"
                if any(item.device == path for item in found):
                    continue
                found.append(WindowsInstallation(
                    root="",
                    label=f"{child.get('label') or 'Windows volume'} ({path})",
                    device=path,
                    mounted=False,
                ))
    return found


def mount_installation(installation: WindowsInstallation) -> WindowsInstallation:
    """Mount a detected NTFS volume read-only through udisks or polkit."""
    if installation.mounted and installation.root:
        return installation
    if not installation.device:
        installation.error = "The Windows volume is not mounted and has no device path."
        return installation
    base = Path("/run/media") / os.environ.get("USER", "aurelia") / "aurelia-migration"
    base.mkdir(parents=True, exist_ok=True)
    target = base / installation.device.replace("/", "_")
    target.mkdir(parents=True, exist_ok=True)
    if _which("udisksctl"):
        code, _out, err = run(["udisksctl", "mount", "-b", installation.device, "-o", "ro"], timeout=60)
        if code == 0:
            code, out, _ = run(["udisksctl", "info", "-b", installation.device], timeout=15)
            for line in out.splitlines():
                if "MountPoints:" in line:
                    candidate = line.split("MountPoints:", 1)[1].strip()
                    if candidate.startswith("/"):
                        installation.root = candidate
                        installation.mounted = True
                        return installation
        installation.error = err or "udisks refused the read-only mount."
    if _which("pkexec") and _which("mount"):
        code, _out, err = run(
            ["pkexec", "mount", "-o", "ro,nosuid,nodev", installation.device, str(target)],
            timeout=90,
        )
        if code == 0:
            installation.root = str(target)
            installation.mounted = True
            return installation
        installation.error = err or "Administrator approval was cancelled."
    return installation


def directory_size(path: Path) -> int:
    total = 0
    try:
        for entry in path.rglob("*"):
            try:
                if entry.is_file() and not entry.is_symlink():
                    total += entry.stat().st_size
            except OSError:
                continue
    except OSError:
        pass
    return total


def user_file_groups(root: Path, user: str) -> list[dict]:
    home = root / "Users" / user
    known = [
        ("Desktop", "Desktop", home / "Desktop"),
        ("Documents", "Documents", home / "Documents"),
        ("Downloads", "Downloads", home / "Downloads"),
        ("Pictures", "Pictures", home / "Pictures"),
        ("Videos", "Videos", home / "Videos"),
        ("Music", "Music", home / "Music"),
    ]
    result = []
    for key, label, path in known:
        if path.is_dir():
            result.append({
                "key": key,
                "label": label,
                "path": str(path),
                "size": directory_size(path),
                "file_count": sum(1 for item in path.rglob("*") if item.is_file()),
            })
    return result


def _start_menu_dirs(root: Path, user: str) -> list[Path]:
    return [
        root / "Users" / user / "AppData" / "Roaming" / "Microsoft" / "Windows" / "Start Menu" / "Programs",
        root / "ProgramData" / "Microsoft" / "Windows" / "Start Menu" / "Programs",
    ]


def _normalise_name(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", " ", value.lower()).strip()


def load_catalog() -> dict:
    try:
        return json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {"apps": []}


_CATALOG: Optional[dict] = None


def match_catalog(name: str) -> dict:
    global _CATALOG
    if _CATALOG is None:
        _CATALOG = load_catalog()
    normal = _normalise_name(name)
    for entry in _CATALOG.get("apps", []):
        for pattern in entry.get("match", []):
            if _normalise_name(pattern) in normal:
                return entry
    return {}


def _pe_details(path: Path) -> tuple[str, str]:
    """Read company/product version when python3-pefile is available."""
    try:
        import pefile  # type: ignore
    except Exception:
        return "", ""
    try:
        pe = pefile.PE(str(path), fast_load=True)
        try:
            pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
            version = ""
            publisher = ""
            if hasattr(pe, "VS_VERSIONINFO"):
                for info in pe.VS_VERSIONINFO:
                    for child in info.entry:
                        if getattr(child, "Key", b"") == b"StringFileInfo":
                            for table in child.entry:
                                for item in table.entry:
                                    key = bytes(item.Key).decode(errors="ignore")
                                    value = bytes(item.Value).decode("utf-16-le", errors="ignore").strip("\x00")
                                    if key == "ProductVersion" and not version:
                                        version = value
                                    if key == "CompanyName" and not publisher:
                                        publisher = value
            return version, publisher
        finally:
            pe.close()
    except Exception:
        return "", ""


def _extract_icon(exe: Path, outdir: Optional[Path] = None) -> str:
    if not _which("wrestool"):
        return ""
    outdir = outdir or (DATA_HOME / "aurelia-migration" / "icons")
    outdir.mkdir(parents=True, exist_ok=True)
    target = outdir / (re.sub(r"[^A-Za-z0-9_.-]+", "_", exe.stem)[:80] + ".ico")
    code, _out, _err = run(["wrestool", "-x", "-t", "14", "-o", str(target), str(exe)], timeout=20)
    return str(target) if code == 0 and target.is_file() and target.stat().st_size > 0 else ""


def scan_applications(root: Path, user: str) -> list[Application]:
    """Scan Start Menu shortcuts and Program Files directories.

    This is a filesystem scan, not a claim that the Windows binaries run on
    AURELIA. Compatibility is resolved separately through the catalog.
    """
    candidates: dict[str, Application] = {}
    for start in _start_menu_dirs(root, user):
        if not start.is_dir():
            continue
        for shortcut in start.rglob("*.lnk"):
            name = shortcut.stem.strip()
            if len(name) < 2 or name.lower() in {"uninstall", "readme", "help"}:
                continue
            candidates.setdefault(_normalise_name(name), Application(name=name, source_path=str(shortcut)))
    program_dirs = [root / "Program Files", root / "Program Files (x86)"]
    for base in program_dirs:
        if not base.is_dir():
            continue
        for child in base.iterdir():
            if not child.is_dir():
                continue
            exe = next((item for item in child.glob("*.exe") if item.is_file()), None)
            name = child.name
            if exe is not None:
                candidates.setdefault(_normalise_name(name), Application(name=name, source_path=str(exe)))
    result = []
    for app in candidates.values():
        version, publisher = _pe_details(Path(app.source_path)) if app.source_path else ("", "")
        app.version = version
        app.publisher = publisher
        app.icon_path = _extract_icon(Path(app.source_path)) if app.source_path.lower().endswith(".exe") else ""
        catalog = match_catalog(app.name)
        if catalog:
            linux = catalog.get("linux") or {}
            app.linux_name = linux.get("label") or linux.get("package") or linux.get("id") or ""
            app.linux_kind = linux.get("type", "")
            app.linux_id = linux.get("package") or linux.get("id") or ""
            app.alternative = catalog.get("alternative", "")
            app.note = catalog.get("note", "")
            app.compatible = bool(app.linux_id) and application_available(app.linux_kind, app.linux_id)
            app.manual = not app.compatible
        result.append(app)
    return sorted(result, key=lambda item: item.name.lower())


def application_available(kind: str, identifier: str) -> bool:
    if not identifier:
        return False
    if kind == "apt":
        if not _which("apt-cache"):
            return False
        code, out, _ = run(["apt-cache", "policy", identifier], timeout=15)
        return code == 0 and "Candidate:" in out and "Candidate: (none)" not in out
    if kind == "flatpak":
        if not _which("flatpak"):
            return False
        code, out, _ = run(["flatpak", "remotes", "--columns=name"], timeout=15)
        return code == 0 and bool(out.strip())
    return False


def install_application(app: Application) -> tuple[bool, str]:
    if not app.compatible:
        return False, "No reviewed AURELIA package is mapped for this application."
    if app.linux_kind == "apt":
        if _which("pkcon"):
            code, out, err = run(["pkcon", "install", "-y", app.linux_id], timeout=1800)
        else:
            code, out, err = run(["pkexec", "apt-get", "install", "-y", app.linux_id], timeout=1800)
        return code == 0, (err or out or f"install {app.linux_id}")
    if app.linux_kind == "flatpak":
        code, out, err = run(["flatpak", "install", "--user", "-y", "flathub", app.linux_id], timeout=3600)
        return code == 0, (err or out or f"flatpak install {app.linux_id}")
    return False, "Unknown package backend."


def _chrome_profiles(home: Path) -> list[tuple[str, str, Path]]:
    result = []
    bases = {
        "Google Chrome": home / "AppData" / "Local" / "Google" / "Chrome" / "User Data",
        "Microsoft Edge": home / "AppData" / "Local" / "Microsoft" / "Edge" / "User Data",
        "Brave": home / "AppData" / "Local" / "BraveSoftware" / "Brave-Browser" / "User Data",
        "Chromium": home / "AppData" / "Local" / "Chromium" / "User Data",
    }
    for browser, base in bases.items():
        if not base.is_dir():
            continue
        for profile in base.iterdir():
            if profile.is_dir() and ((profile / "Bookmarks").is_file() or (profile / "History").is_file()):
                result.append((browser, profile.name, profile))
    return result


def _firefox_profiles(home: Path) -> list[tuple[str, str, Path]]:
    base = home / "AppData" / "Roaming" / "Mozilla" / "Firefox"
    result = []
    if not (base / "Profiles").is_dir():
        return result
    for profile in (base / "Profiles").iterdir():
        if profile.is_dir() and ((profile / "places.sqlite").is_file() or (profile / "bookmarks.html").is_file()):
            result.append(("Mozilla Firefox", profile.name, profile))
    return result


def browser_profiles(root: Path, user: str) -> list[dict]:
    home = root / "Users" / user
    result = []
    for browser, profile, path in _chrome_profiles(home) + _firefox_profiles(home):
        result.append({
            "browser": browser,
            "profile": profile,
            "path": str(path),
            "size": directory_size(path),
        })
    return result


def save_application_inventory(apps: list[Application]) -> Path:
    """Keep a durable inventory of every detected Windows app.

    This is reference material for the user after migration; it is not a claim
    that Windows binaries are installed. It deliberately records compatibility
    and alternatives rather than copying executables.
    """
    target = DATA_HOME / "aurelia-migration" / "application-inventory.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "generated_at": time.time(),
        "applications": [asdict(app) for app in apps],
    }
    target.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    os.chmod(target, 0o600)
    return target


def preserve_browser_preferences(profiles: Iterable[str]) -> list[dict]:
    """Preserve compatible preference files without importing secrets.

    Windows browser profile formats are not interchangeable with Chromium or
    Firefox profiles on Linux. The files are kept as reference so a user can
    review settings and recreate them; no password database or DPAPI blob is
    copied.
    """
    reference = DATA_HOME / "aurelia-migration" / "browser-preferences"
    saved = []
    for raw in profiles:
        profile = Path(raw)
        if not profile.is_dir():
            continue
        for name in ("Preferences", "prefs.js"):
            source = profile / name
            if not source.is_file() or source.stat().st_size > 8 * 1024 * 1024:
                continue
            safe = re.sub(r"[^A-Za-z0-9_.-]+", "_", f"{profile.parent.name}-{profile.name}-{name}")
            reference.mkdir(parents=True, exist_ok=True)
            target = reference / safe
            shutil.copy2(source, target)
            os.chmod(target, 0o600)
            saved.append({"profile": str(profile), "file": str(target)})
    return saved


def chrome_bookmarks(profile: Path) -> list[tuple[str, str]]:
    path = profile / "Bookmarks"
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return []
    found: list[tuple[str, str]] = []

    def walk(node: dict) -> None:
        if node.get("type") == "url" and node.get("url"):
            found.append((str(node["url"]), str(node.get("name") or node["url"])))
        for child in node.get("children") or []:
            if isinstance(child, dict):
                walk(child)

    for root_key in ("bookmark_bar", "other", "synced"):
        root_node = (payload.get("roots") or {}).get(root_key)
        if isinstance(root_node, dict):
            walk(root_node)
    return found


def firefox_bookmarks(profile: Path) -> list[tuple[str, str]]:
    database = profile / "places.sqlite"
    if not database.is_file():
        return []
    temp = Path(tempfile.mkstemp(prefix="aurelia-places-", suffix=".sqlite")[1])
    try:
        shutil.copy2(database, temp)
        connection = sqlite3.connect(temp)
        rows = connection.execute(
            "SELECT b.title, p.url FROM moz_bookmarks b "
            "JOIN moz_places p ON b.fk = p.id WHERE b.type = 1 AND p.url IS NOT NULL"
        ).fetchall()
        connection.close()
        return [(str(url), str(title or url)) for title, url in rows if url]
    except (OSError, sqlite3.Error):
        return []
    finally:
        try:
            temp.unlink()
        except OSError:
            pass


def firefox_history(profile: Path) -> list[tuple[str, str, float]]:
    database = profile / "places.sqlite"
    if not database.is_file():
        return []
    temp = Path(tempfile.mkstemp(prefix="aurelia-places-", suffix=".sqlite")[1])
    try:
        shutil.copy2(database, temp)
        connection = sqlite3.connect(temp)
        rows = connection.execute(
            "SELECT url, title, last_visit_date FROM moz_places "
            "WHERE last_visit_date IS NOT NULL ORDER BY last_visit_date DESC LIMIT 20000"
        ).fetchall()
        connection.close()
        return [(str(url), str(title or url), float(visited or 0) / 1_000_000) for url, title, visited in rows]
    except (OSError, sqlite3.Error):
        return []
    finally:
        try:
            temp.unlink()
        except OSError:
            pass


def import_browser_data(profile: Path, db_path: Path = NOVAIR_DB) -> dict:
    """Import bookmarks and history into NOVAIR's real SQLite store."""
    if (profile / "Bookmarks").is_file():
        bookmarks = chrome_bookmarks(profile)
        history: list[tuple[str, str, float]] = []
        history_db = profile / "History"
        if history_db.is_file():
            temp = Path(tempfile.mkstemp(prefix="aurelia-history-", suffix=".sqlite")[1])
            try:
                shutil.copy2(history_db, temp)
                connection = sqlite3.connect(temp)
                rows = connection.execute(
                    "SELECT url, title, last_visit_time FROM urls "
                    "WHERE last_visit_time > 0 ORDER BY last_visit_time DESC LIMIT 20000"
                ).fetchall()
                connection.close()
                # Chromium timestamps are microseconds since 1601-01-01.
                history = [(str(url), str(title or url), (int(visit) / 1_000_000) - 11644473600)
                           for url, title, visit in rows]
            except (OSError, sqlite3.Error):
                history = []
            finally:
                try:
                    temp.unlink()
                except OSError:
                    pass
    else:
        bookmarks = firefox_bookmarks(profile)
        history = firefox_history(profile)
    db_path.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(db_path)
    try:
        connection.executescript(
            """
            CREATE TABLE IF NOT EXISTS history (
                id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT NOT NULL,
                title TEXT NOT NULL DEFAULT '', visited_at REAL NOT NULL
            );
            CREATE TABLE IF NOT EXISTS bookmarks (
                url TEXT PRIMARY KEY, title TEXT NOT NULL DEFAULT '', created_at REAL NOT NULL
            );
            CREATE INDEX IF NOT EXISTS history_visited ON history(visited_at DESC);
            """
        )
        now = time.time()
        for url, title in bookmarks:
            connection.execute(
                "INSERT OR IGNORE INTO bookmarks(url, title, created_at) VALUES (?, ?, ?)",
                (url, title[:300], now),
            )
        for url, title, visited in history:
            if url.startswith(("http://", "https://")):
                connection.execute(
                    "INSERT INTO history(url, title, visited_at) VALUES (?, ?, ?)",
                    (url, title[:300], visited or now),
                )
        connection.commit()
        count = connection.execute("SELECT COUNT(*) FROM bookmarks").fetchone()[0]
        history_count = connection.execute("SELECT COUNT(*) FROM history").fetchone()[0]
    finally:
        connection.close()
    return {"bookmarks": len(bookmarks), "history": len(history), "total_bookmarks": count, "total_history": history_count}


def default_destination(merge_home: bool = False) -> Path:
    return Path.home() if merge_home else Path.home() / "Windows Migration"


def _copy_one(source: Path, target: Path, overwrite: bool, progress: Progress, state: dict) -> None:
    if source.is_dir():
        target.mkdir(parents=True, exist_ok=True)
        for item in sorted(source.rglob("*")):
            if item.is_symlink() or not item.is_file():
                continue
            relative = item.relative_to(source)
            destination = target / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.exists() and not overwrite:
                state["skipped"] += 1
                progress(f"Skipped existing {relative}", state["copied_bytes"], state["total_bytes"])
                continue
            shutil.copy2(item, destination)
            state["copied_bytes"] += item.stat().st_size
            state["copied"] += 1
            progress(f"Copied {relative}", state["copied_bytes"], state["total_bytes"])
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists() and not overwrite:
            state["skipped"] += 1
            return
        shutil.copy2(source, target)
        state["copied_bytes"] += source.stat().st_size
        state["copied"] += 1


def copy_file_groups(
    groups: Iterable[dict],
    destination: Path,
    merge_home: bool = False,
    overwrite: bool = False,
    progress: Optional[Progress] = None,
) -> dict:
    progress = progress or (lambda _message, _done, _total: None)
    selected = [group for group in groups if group.get("selected", True)]
    total = sum(int(group.get("size", 0)) for group in selected)
    state = {"copied": 0, "skipped": 0, "copied_bytes": 0, "total_bytes": total, "errors": []}
    home_map = {
        "Desktop": "Desktop", "Documents": "Documents", "Downloads": "Downloads",
        "Pictures": "Pictures", "Videos": "Videos", "Music": "Music",
    }
    for group in selected:
        source = Path(group.get("path", ""))
        if not source.is_dir():
            state["errors"].append(f"Missing source folder: {source}")
            continue
        relative = home_map.get(group.get("key", ""), source.name)
        target = (destination / relative) if merge_home else (destination / relative)
        try:
            _copy_one(source, target, overwrite, progress, state)
        except OSError as exc:
            state["errors"].append(f"{source}: {exc}")
    return state


def parse_password_csv(path: Path) -> list[dict]:
    import csv
    rows = []
    with path.open(encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        required = {"url", "username", "password"}
        if not reader.fieldnames or not required.issubset({name.lower() for name in reader.fieldnames}):
            raise ValueError("Password CSV must contain url, username and password columns.")
        for row in reader:
            normal = {key.lower(): value for key, value in row.items()}
            if normal.get("url") and normal.get("username") and normal.get("password"):
                rows.append({
                    "url": normal["url"],
                    "username": normal["username"],
                    "password": normal["password"],
                    "name": normal.get("name", ""),
                })
    return rows


def store_encrypted_passwords(csv_path: Path, vault_path: Path, passphrase: str) -> tuple[bool, str]:
    if not passphrase or len(passphrase) < 8:
        return False, "Choose a vault passphrase of at least 8 characters."
    if not _which("gpg"):
        return False, "GnuPG is required to create an encrypted password vault."
    vault_path.parent.mkdir(parents=True, exist_ok=True)
    code, _out, err = run(
        ["gpg", "--batch", "--yes", "--pinentry-mode", "loopback", "--passphrase-fd", "0",
         "--symmetric", "--cipher-algo", "AES256", "--output", str(vault_path), str(csv_path)],
        timeout=120,
        input_text=passphrase + "\n",
    )
    if code != 0:
        return False, err or "GnuPG could not create the vault."
    os.chmod(vault_path, 0o600)
    return True, str(vault_path)


def import_passwords_keepassxc(csv_path: Path, database: Path, password: str) -> tuple[bool, str]:
    if not _which("keepassxc-cli"):
        return False, "KeePassXC CLI is not installed."
    if database.exists():
        return False, "The selected KeePassXC database already exists; choose a new database."
    rows = parse_password_csv(csv_path)
    if not rows:
        return False, "The CSV contained no complete credentials."
    database.parent.mkdir(parents=True, exist_ok=True)
    code, _out, err = run(["keepassxc-cli", "db-create", "-p", str(database)], timeout=120, input_text=password + "\n" + password + "\n")
    if code != 0:
        return False, err or "KeePassXC could not create the database."
    for row in rows:
        title = row["name"] or row["url"]
        code, _out, err = run(
            ["keepassxc-cli", "add", "-u", row["username"], "-p", str(database), title],
            timeout=60,
            input_text=row["password"] + "\n" + password + "\n",
        )
        if code != 0:
            return False, f"Could not add {title}: {err}"
    return True, str(database)


def migrate(options: MigrationOptions, progress: Optional[Progress] = None) -> dict:
    progress = progress or (lambda _message, _done, _total: None)
    report = {
        "started_at": time.time(),
        "source": options.source_root,
        "windows_user": options.windows_user,
        "dry_run": options.dry_run,
        "files": {},
        "apps": [],
        "browser": [],
        "browser_preferences": [],
        "passwords": {},
        "errors": [],
    }
    root = Path(options.source_root)
    if not is_windows_root(root):
        report["errors"].append("The selected source is not a mounted Windows installation.")
        return report
    if options.dry_run:
        report["files"] = {"planned": options.groups, "destination": options.destination}
        return report
    groups = user_file_groups(root, options.windows_user)
    by_key = {group["key"]: group for group in groups}
    chosen = []
    for group in options.groups:
        key = group.get("key") if isinstance(group, dict) else str(group)
        if key in by_key:
            item = dict(by_key[key])
            item["selected"] = True
            chosen.append(item)
        elif isinstance(group, dict) and group.get("path"):
            chosen.append(group)
    destination = Path(options.destination) if options.destination else default_destination(options.merge_home)
    report["files"] = copy_file_groups(
        chosen, destination, merge_home=options.merge_home,
        overwrite=options.overwrite, progress=progress,
    )
    if options.browser_data:
        for profile in options.browser_profiles:
            path = Path(profile)
            if not path.is_dir():
                continue
            count = import_browser_data(path)
            report["browser"].append({"profile": str(path), **count})
            progress(f"Imported browser data from {path.name}", 0, 0)
        report["browser_preferences"] = preserve_browser_preferences(options.browser_profiles)
    if options.install_apps:
        for raw in options.apps:
            app = Application(**{key: raw[key] for key in Application.__dataclass_fields__ if key in raw})
            if not app.selected and not raw.get("selected"):
                continue
            ok, detail = install_application(app)
            report["apps"].append({"name": app.name, "installed": ok, "detail": detail})
            progress(f"{app.name}: {detail}", 0, 0)
    if options.import_passwords and options.password_csv:
        try:
            if options.password_backend == "keepassxc" and options.keepass_database:
                ok, detail = import_passwords_keepassxc(
                    Path(options.password_csv), Path(options.keepass_database), options.keepass_password
                )
                report["passwords"] = {"backend": "keepassxc", "ok": ok, "detail": detail}
            else:
                vault = Path(options.password_vault) if options.password_vault else (
                    DATA_HOME / "aurelia-migration" / "passwords.gpg"
                )
                ok, detail = store_encrypted_passwords(
                    Path(options.password_csv), vault, options.keepass_password
                )
                report["passwords"] = {"backend": "gpg", "ok": ok, "detail": detail}
        except (OSError, ValueError) as exc:
            report["passwords"] = {"backend": options.password_backend, "ok": False, "detail": str(exc)}
    if options.wallpaper:
        wallpaper = Path(options.wallpaper)
        if wallpaper.is_file():
            target = Path.home() / "Pictures" / "Migrated Wallpapers" / wallpaper.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(wallpaper, target)
            report["wallpaper"] = str(target)
            if _which("plasma-apply-wallpaperimage"):
                code, _detail = run(["plasma-apply-wallpaperimage", str(target)], timeout=30)[:2]
                report["wallpaper_applied"] = code == 0
    report["finished_at"] = time.time()
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    os.chmod(REPORT_PATH, 0o600)
    return report


def self_test() -> list[str]:
    failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="aurelia-migration-") as temporary:
        root = Path(temporary) / "windows"
        user = root / "Users" / "Ada"
        (root / "Windows" / "System32").mkdir(parents=True)
        for name in ("Desktop", "Documents", "Pictures"):
            folder = user / name
            folder.mkdir(parents=True)
            (folder / "note.txt").write_text(f"{name} data\n", encoding="utf-8")
        start = user / "AppData" / "Roaming" / "Microsoft" / "Windows" / "Start Menu" / "Programs"
        start.mkdir(parents=True)
        (start / "VLC media player.lnk").write_bytes(b"")
        chrome = user / "AppData" / "Local" / "Google" / "Chrome" / "User Data" / "Default"
        chrome.mkdir(parents=True)
        (chrome / "Bookmarks").write_text(json.dumps({
            "roots": {"bookmark_bar": {"children": [
                {"type": "url", "name": "AURELIA", "url": "https://example.org/aurelia"}
            ]}}
        }), encoding="utf-8")
        if not is_windows_root(root):
            failures.append("fake Windows root was not detected")
        groups = user_file_groups(root, "Ada")
        if {group["key"] for group in groups} != {"Desktop", "Documents", "Pictures"}:
            failures.append("known user folders were not discovered")
        apps = scan_applications(root, "Ada")
        if not any(app.name == "VLC media player" for app in apps):
            failures.append("Start Menu application was not discovered")
        profiles = browser_profiles(root, "Ada")
        if not any(item["browser"] == "Google Chrome" for item in profiles):
            failures.append("Chrome profile was not discovered")
        bookmarks = chrome_bookmarks(chrome)
        if bookmarks != [("https://example.org/aurelia", "AURELIA")]:
            failures.append("Chrome bookmarks were not parsed")
        db = Path(temporary) / "novair.db"
        counts = import_browser_data(chrome, db)
        if counts["total_bookmarks"] < 1:
            failures.append("NOVAIR bookmark import failed")
        destination = Path(temporary) / "destination"
        result = copy_file_groups(
            [{"key": "Documents", "path": str(user / "Documents"), "size": 10}],
            destination, progress=lambda *_args: None,
        )
        if not (destination / "Documents" / "note.txt").is_file():
            failures.append("file copy did not create the destination file")
        result_again = copy_file_groups(
            [{"key": "Documents", "path": str(user / "Documents"), "size": 10}],
            destination, progress=lambda *_args: None,
        )
        if result_again["skipped"] < 1:
            failures.append("existing files were overwritten without permission")
        if not load_catalog().get("apps"):
            failures.append("application catalog is empty")
    return failures


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="AURELIA Windows migration backend")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--discover", action="store_true")
    parser.add_argument("--scan")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        failures = self_test()
        if failures:
            print("FAILED: " + "; ".join(failures))
            return 1
        print("AURELIA migration self-test: PASS")
        return 0
    if args.discover:
        print(json.dumps([asdict(item) for item in discover_installations()], indent=2))
        return 0
    if args.scan:
        root = Path(args.scan)
        if not is_windows_root(root):
            print("Not a Windows installation.", file=sys.stderr)
            return 1
        payload = {
            "root": str(root),
            "users": [entry.name for entry in (root / "Users").iterdir() if entry.is_dir()],
        }
        print(json.dumps(payload, indent=2))
        return 0
    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
