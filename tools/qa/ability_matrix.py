#!/usr/bin/env python3
"""
Build and run RogueMelee's host-driven borrowed-special compatibility matrix.

The QA DOL exposes a small RAM mailbox. This script finds that mailbox inside
Dolphin, asks the game to rebuild a clean fighter scene for one
recipient/source/slot/ground-or-air case, watches a heartbeat, and immediately
records the result. If Dolphin exits or emulation hangs, the active case is
recorded and the runner restarts Dolphin and continues.

Results:
  qa-results/ability_matrix.csv
  qa-results/ability_compatibility.json
  qa-results/ability_summary.md
  qa-results/crashes/*.txt
"""

from __future__ import annotations

import argparse
import csv
import ctypes
from ctypes import wintypes
import datetime as dt
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time
from collections import Counter
from typing import Dict, Iterable, List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[2]
QA_DIR = ROOT / "build" / "qa"
RESULTS = ROOT / "qa-results"
CSV_PATH = RESULTS / "ability_matrix.csv"
JSON_PATH = RESULTS / "ability_compatibility.json"
SUMMARY_PATH = RESULTS / "ability_summary.md"
CRASH_DIR = RESULTS / "crashes"
QA_ISO = QA_DIR / "RogueMelee-AbilityQA.iso"

MAGIC = b"ROGUE_QA_MATRIX1"
MAILBOX_VERSION = 1

# Mailbox offsets. Keep synchronized with mod/rogue_debug.c.
OFF_VERSION = 16
OFF_COMMAND = 20
OFF_STATUS = 24
OFF_HEARTBEAT = 28
OFF_RECIPIENT = 32
OFF_SOURCE = 36
OFF_SLOT = 40
OFF_VARIANT = 44
OFF_ERROR = 48
OFF_ACTIVE_COMMAND = 52
OFF_NOTE = 56
MAILBOX_BYTES = 120

QA_WAITING = 0
QA_PREPARING = 1
QA_RUNNING = 2
QA_PASS = 3
QA_SOFT_FAIL = 4
QA_INVALID = 5
QA_NOT_APPLICABLE = 6

CHARACTERS: List[Tuple[int, str]] = [
    (0, "Captain Falcon"),
    (1, "Donkey Kong"),
    (2, "Fox"),
    (3, "Mr. Game & Watch"),
    (4, "Kirby"),
    (5, "Bowser"),
    (6, "Link"),
    (7, "Luigi"),
    (8, "Mario"),
    (9, "Marth"),
    (10, "Mewtwo"),
    (11, "Ness"),
    (12, "Peach"),
    (13, "Pikachu"),
    (14, "Ice Climbers"),
    (15, "Jigglypuff"),
    (16, "Samus"),
    (17, "Yoshi"),
    (18, "Zelda"),
    (19, "Sheik"),
    (20, "Falco"),
    (21, "Young Link"),
    (22, "Dr. Mario"),
    (23, "Roy"),
    (24, "Pichu"),
    (25, "Ganondorf"),
]

SLOTS = [
    (0, "Neutral-B"),
    (1, "Side-B"),
    (2, "Up-B"),
    (3, "Down-B"),
]

# Source-character special names mirror mod/rogue_ability_registry.c.
ABILITIES: Dict[str, List[str]] = {
    "Captain Falcon": ["Falcon Punch", "Raptor Boost", "Falcon Dive", "Falcon Kick"],
    "Donkey Kong": ["Giant Punch", "Headbutt", "Spinning Kong", "Hand Slap"],
    "Fox": ["Fox Blaster", "Fox Illusion", "Fire Fox", "Fox Reflector"],
    "Mr. Game & Watch": ["Chef", "Judgment", "Fire", "Oil Panic"],
    "Kirby": ["Inhale", "Hammer", "Final Cutter", "Stone"],
    "Bowser": ["Fire Breath", "Koopa Klaw", "Whirling Fortress", "Bowser Bomb"],
    "Link": ["Bow", "Boomerang", "Spin Attack", "Bomb"],
    "Luigi": ["Luigi Fireball", "Green Missile", "Luigi Jump Punch", "Luigi Cyclone"],
    "Mario": ["Fireball", "Cape", "Super Jump Punch", "Mario Tornado"],
    "Marth": ["Shield Breaker", "Dancing Blade", "Dolphin Slash", "Marth Counter"],
    "Mewtwo": ["Shadow Ball", "Confusion", "Teleport", "Disable"],
    "Ness": ["PK Flash", "PK Fire", "PK Thunder", "PSI Magnet"],
    "Peach": ["Toad", "Peach Bomber", "Peach Parasol", "Vegetable"],
    "Pikachu": ["Thunder Jolt", "Skull Bash", "Quick Attack", "Thunder"],
    "Ice Climbers": ["Ice Shot", "Squall Hammer", "Belay", "Blizzard"],
    "Jigglypuff": ["Rollout", "Pound", "Sing", "Rest"],
    "Samus": ["Charge Shot", "Missile", "Screw Attack", "Morph Ball Bomb"],
    "Yoshi": ["Egg Lay", "Egg Roll", "Egg Throw", "Yoshi Bomb"],
    "Zelda": ["Nayru's Love", "Din's Fire", "Farore's Wind", "Transform to Sheik"],
    "Sheik": ["Needle Storm", "Chain", "Vanish", "Transform to Zelda"],
    "Falco": ["Falco Blaster", "Falco Phantasm", "Fire Bird", "Falco Reflector"],
    "Young Link": ["Fire Bow", "Young Link Boomerang", "Young Link Spin Attack", "Young Link Bomb"],
    "Dr. Mario": ["Megavitamins", "Super Sheet", "Dr. Mario Jump Punch", "Dr. Tornado"],
    "Roy": ["Flare Blade", "Double-Edge Dance", "Blazer", "Roy Counter"],
    "Pichu": ["Pichu Thunder Jolt", "Pichu Skull Bash", "Agility", "Pichu Thunder"],
    "Ganondorf": ["Warlock Punch", "Gerudo Dragon", "Dark Dive", "Wizard's Foot"],
}

FIELDS = [
    "recipient_id", "recipient", "source_id", "source", "slot_id", "slot",
    "ability", "variant", "status", "detail", "seconds", "exit_code", "timestamp",
]


def normalize(text: str) -> str:
    return re.sub(r"[^a-z0-9]", "", text.lower())


CHAR_BY_ID = dict(CHARACTERS)
CHAR_BY_NAME = {normalize(name): (cid, name) for cid, name in CHARACTERS}
SLOT_BY_NAME = {normalize(name): (sid, name) for sid, name in SLOTS}
SLOT_BY_NAME.update({
    "neutral": SLOTS[0], "n": SLOTS[0],
    "side": SLOTS[1], "s": SLOTS[1],
    "up": SLOTS[2], "u": SLOTS[2],
    "down": SLOTS[3], "d": SLOTS[3],
})


def parse_character(value: Optional[str]) -> Optional[Tuple[int, str]]:
    if value is None:
        return None
    key = normalize(value)
    if key in CHAR_BY_NAME:
        return CHAR_BY_NAME[key]
    matches = [item for name, item in CHAR_BY_NAME.items() if key and key in name]
    if len(matches) == 1:
        return matches[0]
    raise argparse.ArgumentTypeError(f"Unknown/ambiguous character: {value}")


def parse_slot(value: Optional[str]) -> Optional[Tuple[int, str]]:
    if value is None:
        return None
    key = normalize(value)
    if key in SLOT_BY_NAME:
        return SLOT_BY_NAME[key]
    raise argparse.ArgumentTypeError(f"Unknown slot: {value}")


def run(cmd: List[str], *, env=None) -> None:
    print("+", subprocess.list2cmdline(cmd))
    subprocess.run(cmd, cwd=ROOT, env=env, check=True)


def build_qa_iso(args) -> None:
    if args.no_build:
        if not QA_ISO.is_file():
            raise SystemExit(f"--no-build requested but QA ISO is missing: {QA_ISO}")
        return

    QA_DIR.mkdir(parents=True, exist_ok=True)

    build_cmd = [sys.executable, str(ROOT / "tools" / "build.py"), "--qa-abilities"]
    original = ROOT / "orig" / "GALE01" / "sys" / "main.dol"
    assets = ROOT / "build" / "game" / "files"
    if not original.is_file() or not assets.is_dir():
        build_cmd += ["--image", str(args.iso), "--dolphin", str(args.dolphin)]
        print("Initial game extraction is missing; the QA runner will create it now.")

    run(build_cmd)

    env = os.environ.copy()
    env["ROGUEMELEE_VERSION"] = "qa-abilities"
    run([sys.executable, str(ROOT / "tools" / "package" / "build_mod_zip.py")], env=env)

    apply_mod = ROOT / "build" / "package" / "Apply Mod.exe"
    if not apply_mod.is_file():
        raise SystemExit("Packaging did not create build/package/Apply Mod.exe")

    if QA_ISO.exists():
        QA_ISO.unlink()
    run([str(apply_mod), str(args.iso), str(QA_ISO)])
    print(f"QA image ready: {QA_ISO}")


# ----- Windows process memory -------------------------------------------------

if os.name == "nt":
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    PROCESS_VM_OPERATION = 0x0008
    PROCESS_VM_READ = 0x0010
    PROCESS_VM_WRITE = 0x0020
    PROCESS_QUERY_INFORMATION = 0x0400

    MEM_COMMIT = 0x1000
    PAGE_GUARD = 0x100
    PAGE_NOACCESS = 0x01
    WRITABLE_PROTECT = {0x04, 0x08, 0x40, 0x80}

    class MEMORY_BASIC_INFORMATION(ctypes.Structure):
        _fields_ = [
            ("BaseAddress", ctypes.c_void_p),
            ("AllocationBase", ctypes.c_void_p),
            ("AllocationProtect", wintypes.DWORD),
            ("PartitionId", wintypes.WORD),
            ("RegionSize", ctypes.c_size_t),
            ("State", wintypes.DWORD),
            ("Protect", wintypes.DWORD),
            ("Type", wintypes.DWORD),
        ]

    kernel32.OpenProcess.argtypes = [
        wintypes.DWORD, wintypes.BOOL, wintypes.DWORD
    ]
    kernel32.OpenProcess.restype = wintypes.HANDLE

    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    kernel32.VirtualQueryEx.argtypes = [
        wintypes.HANDLE, ctypes.c_void_p,
        ctypes.POINTER(MEMORY_BASIC_INFORMATION), ctypes.c_size_t
    ]
    kernel32.VirtualQueryEx.restype = ctypes.c_size_t

    kernel32.ReadProcessMemory.argtypes = [
        wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
        ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)
    ]
    kernel32.ReadProcessMemory.restype = wintypes.BOOL

    kernel32.WriteProcessMemory.argtypes = [
        wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
        ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)
    ]
    kernel32.WriteProcessMemory.restype = wintypes.BOOL


class ProcessMemory:
    def __init__(self, pid: int):
        if os.name != "nt":
            raise RuntimeError("The ability matrix host currently requires Windows.")
        access = (
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
            PROCESS_VM_WRITE | PROCESS_VM_OPERATION
        )
        self.handle = kernel32.OpenProcess(access, False, pid)
        if not self.handle:
            raise OSError(ctypes.get_last_error(), "OpenProcess failed")
        self.base: Optional[int] = None

    def close(self):
        if getattr(self, "handle", None):
            kernel32.CloseHandle(self.handle)
            self.handle = None

    def read(self, address: int, size: int) -> bytes:
        buf = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        ok = kernel32.ReadProcessMemory(
            self.handle, ctypes.c_void_p(address), buf, size, ctypes.byref(got)
        )
        if not ok or got.value != size:
            raise OSError(ctypes.get_last_error(), "ReadProcessMemory failed")
        return buf.raw

    def write(self, address: int, data: bytes) -> None:
        buf = ctypes.create_string_buffer(data)
        wrote = ctypes.c_size_t()
        ok = kernel32.WriteProcessMemory(
            self.handle, ctypes.c_void_p(address), buf, len(data),
            ctypes.byref(wrote)
        )
        if not ok or wrote.value != len(data):
            raise OSError(ctypes.get_last_error(), "WriteProcessMemory failed")

    def locate_mailbox(self, timeout: float = 45.0) -> int:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            found = self._scan_once()
            if found is not None:
                self.base = found
                return found
            time.sleep(0.5)
        raise TimeoutError(
            "Could not locate ROGUE_QA_MATRIX1 in Dolphin RAM. "
            "Confirm this is the QA build and that emulation reached Rogue Mode."
        )

    def _scan_once(self) -> Optional[int]:
        address = 0
        mbi = MEMORY_BASIC_INFORMATION()
        mbi_size = ctypes.sizeof(mbi)
        chunk_size = 2 * 1024 * 1024
        overlap = len(MAGIC) - 1

        while True:
            result = kernel32.VirtualQueryEx(
                self.handle, ctypes.c_void_p(address),
                ctypes.byref(mbi), mbi_size
            )
            if not result:
                break

            base = int(mbi.BaseAddress or 0)
            size = int(mbi.RegionSize or 0)
            protect = int(mbi.Protect)
            next_address = base + max(size, 0x1000)

            writable = (protect & 0xFF) in WRITABLE_PROTECT
            readable = not (protect & PAGE_GUARD) and protect != PAGE_NOACCESS
            if (
                mbi.State == MEM_COMMIT and writable and readable and
                0x10000 <= size <= 512 * 1024 * 1024
            ):
                pos = 0
                carry = b""
                while pos < size:
                    count = min(chunk_size, size - pos)
                    try:
                        data = self.read(base + pos, count)
                    except OSError:
                        break
                    haystack = carry + data
                    idx = haystack.find(MAGIC)
                    if idx >= 0:
                        candidate = base + pos - len(carry) + idx
                        try:
                            version = struct.unpack(">I", self.read(
                                candidate + OFF_VERSION, 4
                            ))[0]
                            if version == MAILBOX_VERSION:
                                return candidate
                        except OSError:
                            pass
                    carry = data[-overlap:] if len(data) >= overlap else data
                    pos += count

            if next_address <= address:
                break
            address = next_address

        return None

    def _addr(self, offset: int) -> int:
        if self.base is None:
            raise RuntimeError("Mailbox is not located.")
        return self.base + offset

    def read_u32(self, offset: int) -> int:
        return struct.unpack(">I", self.read(self._addr(offset), 4))[0]

    def read_s32(self, offset: int) -> int:
        return struct.unpack(">i", self.read(self._addr(offset), 4))[0]

    def write_u32(self, offset: int, value: int) -> None:
        self.write(self._addr(offset), struct.pack(">I", value & 0xFFFFFFFF))

    def write_s32(self, offset: int, value: int) -> None:
        self.write(self._addr(offset), struct.pack(">i", value))

    def read_note(self) -> str:
        raw = self.read(self._addr(OFF_NOTE), 64)
        return raw.split(b"\0", 1)[0].decode("ascii", "replace")


def launch_dolphin(dolphin: Path) -> subprocess.Popen:
    cmd = [str(dolphin), "-b", "-e", str(QA_ISO)]
    print("+", subprocess.list2cmdline(cmd))
    return subprocess.Popen(cmd, cwd=dolphin.parent)


def stop_process(proc: Optional[subprocess.Popen]) -> None:
    if not proc:
        return
    if proc.poll() is None:
        try:
            proc.terminate()
            proc.wait(timeout=5)
        except Exception:
            try:
                proc.kill()
            except Exception:
                pass


def connect(proc: subprocess.Popen) -> ProcessMemory:
    memory = ProcessMemory(proc.pid)
    print(f"Locating QA mailbox in Dolphin PID {proc.pid}...")
    address = memory.locate_mailbox()
    print(f"QA mailbox found at host address 0x{address:X}")
    return memory


def write_case(memory: ProcessMemory, sequence: int, case) -> None:
    recipient_id, _, source_id, _, slot_id, _, _, variant = case
    memory.write_s32(OFF_RECIPIENT, recipient_id)
    memory.write_s32(OFF_SOURCE, source_id)
    memory.write_s32(OFF_SLOT, slot_id)
    memory.write_s32(OFF_VARIANT, 0 if variant == "ground" else 1)
    # Publish command last.
    memory.write_u32(OFF_COMMAND, sequence)


def wait_case(
    proc: subprocess.Popen,
    memory: ProcessMemory,
    sequence: int,
    *,
    timeout: float,
    heartbeat_timeout: float,
) -> Tuple[str, str, str]:
    start = time.monotonic()
    last_heartbeat = None
    heartbeat_changed = start
    last_note = ""
    last_status = ""

    while True:
        if proc.poll() is not None:
            return "crash", last_note or "Dolphin process exited", str(proc.returncode)

        now = time.monotonic()
        try:
            heartbeat = memory.read_u32(OFF_HEARTBEAT)
            active = memory.read_u32(OFF_ACTIVE_COMMAND)
            status = memory.read_u32(OFF_STATUS)
            note = memory.read_note()
        except OSError as exc:
            if proc.poll() is not None:
                return "crash", last_note or "Dolphin process exited", str(proc.returncode)
            time.sleep(0.05)
            continue

        last_note = note
        last_status = str(status)

        if heartbeat != last_heartbeat:
            last_heartbeat = heartbeat
            heartbeat_changed = now

        if active == sequence:
            if status == QA_PASS:
                return "pass", note or "pass", ""
            if status == QA_SOFT_FAIL:
                return "unavailable", note or "move refused", ""
            if status == QA_INVALID:
                return "invalid", note or "invalid QA request", ""
            if status == QA_NOT_APPLICABLE:
                return "not-applicable", note or "source has no entry point", ""

        if now - heartbeat_changed > heartbeat_timeout:
            return "hang", note or f"heartbeat stopped at status {last_status}", ""

        if now - start > timeout:
            return "timeout", note or f"test exceeded {timeout:.0f}s", ""

        time.sleep(0.05)


def read_rows() -> List[dict]:
    if not CSV_PATH.exists():
        return []
    with CSV_PATH.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def append_row(row: dict) -> None:
    RESULTS.mkdir(parents=True, exist_ok=True)
    exists = CSV_PATH.exists()
    with CSV_PATH.open("a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        if not exists:
            writer.writeheader()
        writer.writerow(row)
        f.flush()


def case_key(row_or_case) -> Tuple[str, str, str, str]:
    if isinstance(row_or_case, dict):
        return (
            row_or_case["recipient"],
            row_or_case["source"],
            row_or_case["slot"],
            row_or_case["variant"],
        )
    _, recipient, _, source, _, slot, _, variant = row_or_case
    return recipient, source, slot, variant


def possible_dolphin_logs(dolphin: Path) -> Iterable[Path]:
    yield dolphin.parent / "User" / "Logs" / "dolphin.log"
    appdata = Path(os.environ.get("APPDATA", ""))
    user = Path(os.environ.get("USERPROFILE", ""))
    if appdata:
        yield appdata / "Dolphin Emulator" / "Logs" / "dolphin.log"
        yield appdata / "Slippi Launcher" / "netplay" / "User" / "Logs" / "dolphin.log"
    if user:
        yield user / "Documents" / "Dolphin Emulator" / "Logs" / "dolphin.log"


def log_tail(dolphin: Path, lines: int = 120) -> str:
    for path in possible_dolphin_logs(dolphin):
        try:
            if path.is_file():
                content = path.read_text(errors="replace").splitlines()
                return f"Dolphin log: {path}\n\n" + "\n".join(content[-lines:])
        except Exception:
            continue
    return "No Dolphin log file was found automatically."


def save_crash(case, row: dict, dolphin: Path) -> None:
    CRASH_DIR.mkdir(parents=True, exist_ok=True)
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    _, recipient, _, source, _, slot, ability, variant = case
    safe = normalize(f"{recipient}-{source}-{slot}-{variant}")
    path = CRASH_DIR / f"{stamp}-{safe}.txt"
    text = (
        f"RogueMelee ability QA failure\n"
        f"=============================\n"
        f"Recipient: {recipient}\n"
        f"Source:    {source}\n"
        f"Ability:   {ability}\n"
        f"Slot:      {slot}\n"
        f"Variant:   {variant}\n"
        f"Status:    {row['status']}\n"
        f"Detail:    {row['detail']}\n"
        f"Exit code: {row['exit_code']}\n"
        f"Timestamp: {row['timestamp']}\n\n"
        f"{log_tail(dolphin)}\n"
    )
    path.write_text(text, encoding="utf-8")


def build_cases(args, previous: List[dict]) -> List[tuple]:
    recipients = CHARACTERS
    sources = CHARACTERS
    slots = SLOTS
    variants = ["ground", "air"]

    if args.recipient:
        recipients = [parse_character(args.recipient)]
    if args.source:
        sources = [parse_character(args.source)]
    if args.slot:
        slots = [parse_slot(args.slot)]
    if args.variant != "both":
        variants = [args.variant]

    cases = []
    for rid, rname in recipients:
        for sid, sname in sources:
            for slot_id, slot_name in slots:
                ability = ABILITIES[sname][slot_id]
                for variant in variants:
                    cases.append(
                        (rid, rname, sid, sname, slot_id, slot_name,
                         ability, variant)
                    )

    previous_by_key = {case_key(row): row for row in previous}

    if args.rerun_failures:
        cases = [
            c for c in cases
            if case_key(c) in previous_by_key and
            previous_by_key[case_key(c)]["status"] not in {"pass", "not-applicable"}
        ]
    elif not args.fresh:
        cases = [c for c in cases if case_key(c) not in previous_by_key]

    if args.limit:
        cases = cases[: args.limit]

    return cases


def generate_reports() -> None:
    rows = read_rows()
    RESULTS.mkdir(parents=True, exist_ok=True)

    matrix: Dict[str, dict] = {}
    for row in rows:
        rec = matrix.setdefault(row["recipient"], {})
        src = rec.setdefault(row["source"], {})
        slot = src.setdefault(row["slot"], {})
        slot[row["variant"]] = {
            "status": row["status"],
            "ability": row["ability"],
            "detail": row["detail"],
            "timestamp": row["timestamp"],
        }

    JSON_PATH.write_text(
        json.dumps({
            "generated": dt.datetime.now().isoformat(timespec="seconds"),
            "result_count": len(rows),
            "matrix": matrix,
        }, indent=2),
        encoding="utf-8",
    )

    counts = Counter(row["status"] for row in rows)
    lines = [
        "# RogueMelee Ability Matrix",
        "",
        f"Generated: {dt.datetime.now().isoformat(timespec='seconds')}",
        "",
        f"Total recorded cases: **{len(rows)}**",
        "",
        "| Status | Count |",
        "|---|---:|",
    ]
    for status in ["pass", "not-applicable", "unavailable", "crash", "hang", "timeout", "invalid"]:
        lines.append(f"| {status} | {counts.get(status, 0)} |")

    bad = [r for r in rows if r["status"] not in {"pass", "not-applicable"}]
    lines += ["", "## Cases needing attention", ""]
    if not bad:
        lines.append("None.")
    else:
        lines.append("| Recipient | Ability source | Ability | Variant | Status | Detail |")
        lines.append("|---|---|---|---|---|---|")
        for row in bad[:500]:
            detail = row["detail"].replace("|", "/")
            lines.append(
                f"| {row['recipient']} | {row['source']} | {row['ability']} | "
                f"{row['variant']} | {row['status']} | {detail} |"
            )
        if len(bad) > 500:
            lines.append(f"\n_First 500 of {len(bad)} non-passing cases shown._")

    SUMMARY_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def fresh_results():
    if RESULTS.exists():
        shutil.rmtree(RESULTS)
    RESULTS.mkdir(parents=True, exist_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Systematically test every RogueMelee recipient/source special combination."
    )
    parser.add_argument(
        "--iso", type=Path, required=True,
        help="Clean, unmodified US 1.02 Melee ISO/GCM."
    )
    parser.add_argument(
        "--dolphin", type=Path, required=True,
        help="Dolphin.exe (or Slippi Dolphin.exe) used to launch the QA image."
    )
    parser.add_argument(
        "--variant", choices=["both", "ground", "air"], default="both",
        help="Test ground entries, air entries, or both. Default: both."
    )
    parser.add_argument("--recipient", help="Only test one receiving character.")
    parser.add_argument("--source", help="Only test specials from one source character.")
    parser.add_argument("--slot", help="Only test neutral/side/up/down.")
    parser.add_argument("--limit", type=int, help="Run only the first N selected cases.")
    parser.add_argument(
        "--fresh", action="store_true",
        help="Delete previous qa-results and start a new matrix."
    )
    parser.add_argument(
        "--rerun-failures", action="store_true",
        help="Run only previously recorded non-passing cases."
    )
    parser.add_argument(
        "--no-build", action="store_true",
        help="Reuse build/qa/RogueMelee-AbilityQA.iso."
    )
    parser.add_argument(
        "--timeout", type=float, default=25.0,
        help="Maximum seconds per test case. Default: 25."
    )
    parser.add_argument(
        "--heartbeat-timeout", type=float, default=12.0,
        help="Seconds without game heartbeat before classifying a hang. Default: 12."
    )
    args = parser.parse_args()

    if os.name != "nt":
        parser.error("The automated Dolphin RAM harness currently requires Windows.")
    args.iso = args.iso.resolve()
    args.dolphin = args.dolphin.resolve()
    if not args.iso.is_file():
        parser.error(f"ISO not found: {args.iso}")
    if not args.dolphin.is_file():
        parser.error(f"Dolphin executable not found: {args.dolphin}")
    if args.fresh and args.rerun_failures:
        parser.error("--fresh and --rerun-failures cannot be combined.")

    if args.fresh:
        fresh_results()
    else:
        RESULTS.mkdir(parents=True, exist_ok=True)

    build_qa_iso(args)

    previous = read_rows()
    cases = build_cases(args, previous)
    total_selected = (
        len(CHARACTERS) * len(CHARACTERS) * len(SLOTS) *
        (2 if args.variant == "both" else 1)
    )
    print()
    print(f"Selected cases remaining: {len(cases)}")
    if not args.recipient and not args.source and not args.slot:
        print(f"Full matrix size for this variant mode: {total_selected}")
    print(f"Results: {CSV_PATH}")
    print("The CSV is flushed after every case, so Ctrl+C is safe.")
    print()

    if not cases:
        generate_reports()
        print("Nothing to run; selected cases are already recorded.")
        return 0

    proc: Optional[subprocess.Popen] = None
    memory: Optional[ProcessMemory] = None
    sequence = 1
    completed = 0

    try:
        for case in cases:
            rid, rname, sid, sname, slot_id, slot_name, ability, variant = case

            if proc is None or proc.poll() is not None or memory is None:
                if memory is not None:
                    memory.close()
                    memory = None
                stop_process(proc)
                proc = launch_dolphin(args.dolphin)
                try:
                    memory = connect(proc)
                except Exception:
                    stop_process(proc)
                    raise

            sequence += 1
            print(
                f"[{completed + 1}/{len(cases)}] "
                f"{rname} <- {sname} {slot_name} ({ability}) [{variant}]",
                flush=True,
            )
            write_case(memory, sequence, case)
            started = time.monotonic()
            status, detail, exit_code = wait_case(
                proc, memory, sequence,
                timeout=args.timeout,
                heartbeat_timeout=args.heartbeat_timeout,
            )
            elapsed = time.monotonic() - started

            row = {
                "recipient_id": rid,
                "recipient": rname,
                "source_id": sid,
                "source": sname,
                "slot_id": slot_id,
                "slot": slot_name,
                "ability": ability,
                "variant": variant,
                "status": status,
                "detail": detail,
                "seconds": f"{elapsed:.2f}",
                "exit_code": exit_code,
                "timestamp": dt.datetime.now().isoformat(timespec="seconds"),
            }
            append_row(row)
            completed += 1

            if status != "pass":
                print(f"  -> {status.upper()}: {detail}")
            else:
                print(f"  -> pass ({elapsed:.1f}s)")

            if status in {"crash", "hang", "timeout"}:
                save_crash(case, row, args.dolphin)
                if memory is not None:
                    memory.close()
                    memory = None
                stop_process(proc)
                proc = None
                # Continue with the next case after a clean emulator restart.
                time.sleep(1.0)

            if completed % 20 == 0:
                generate_reports()

    except KeyboardInterrupt:
        print("\nInterrupted. Progress has already been saved.")
    finally:
        if memory is not None:
            memory.close()
        stop_process(proc)
        generate_reports()

    print()
    print(f"Matrix CSV:     {CSV_PATH}")
    print(f"Compatibility:  {JSON_PATH}")
    print(f"Summary:        {SUMMARY_PATH}")
    print(f"Crash reports:  {CRASH_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
