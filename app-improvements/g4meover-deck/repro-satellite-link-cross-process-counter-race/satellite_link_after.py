#!/usr/bin/env python3
"""G4MEOVER Deck — Satelliten-Link.

Sendet signierte ukfe_rf-Befehle ueber die Pi-GPIO-UART an den WROOM-Relay
(-> ESP-NOW an LilyGo/Heltec) bzw. direkt an einen Satelliten. Der Rolling-Counter
wird auf Platte persistiert (ueberlebt Reboots) — wichtig fuers Anti-Replay, da die
Satelliten nur monoton steigende Counter akzeptieren.
"""
from __future__ import annotations
import os
import sys
import threading
import time

import serial  # pyserial

import ukfe_rf

if sys.platform == "win32":
    import msvcrt
else:
    import fcntl


class _CounterFileLock:
    """Exklusiver Dateilock UEBER PROZESSGRENZEN auf der counter_file selbst
    (POSIX flock / Windows msvcrt-Byte-Lock, beides stdlib). self._lock (threading.Lock)
    schuetzt nur mehrere Threads INNERHALB eines Prozesses (z.B. deck_daemons
    ThreadingHTTPServer) — ein zusaetzlicher, kurzlebiger deck-ctl-Prozess hat sein
    eigenes Lock-Objekt und wuerde sonst dieselbe counter_file ungeschuetzt
    lesen+erhoehen+schreiben."""

    def __init__(self, counter_file: str):
        self._lock_path = counter_file + ".lock"
        self._fh = None

    def __enter__(self) -> "_CounterFileLock":
        lock_dir = os.path.dirname(self._lock_path)
        if lock_dir:
            os.makedirs(lock_dir, exist_ok=True)
        self._fh = open(self._lock_path, "a+b")
        if sys.platform == "win32":
            if os.path.getsize(self._lock_path) == 0:
                self._fh.write(b"\0")
                self._fh.flush()
            self._fh.seek(0)
            msvcrt.locking(self._fh.fileno(), msvcrt.LK_LOCK, 1)
        else:
            fcntl.flock(self._fh.fileno(), fcntl.LOCK_EX)
        return self

    def __exit__(self, *exc_info) -> None:
        assert self._fh is not None
        try:
            if sys.platform == "win32":
                self._fh.seek(0)
                msvcrt.locking(self._fh.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(self._fh.fileno(), fcntl.LOCK_UN)
        finally:
            self._fh.close()
            self._fh = None


class SatelliteLink:
    def __init__(self, port: str = "/dev/serial0", baud: int = 115200,
                 secret: bytes = ukfe_rf.DEFAULT_SECRET,
                 counter_file: str = "/var/lib/g4meover-deck/counter"):
        self.port = port
        self.baud = baud
        self.secret = secret
        self.counter_file = counter_file
        self._lock = threading.Lock()
        self._serial: serial.Serial | None = None
        self._counter = self._load_counter()

    # ---- Counter-Persistenz (Anti-Replay ueber Reboots UND -Prozesse) ----
    def _load_counter(self) -> int:
        try:
            with open(self.counter_file) as f:
                return int(f.read().strip() or "0")
        except (OSError, ValueError):
            # Kein persistenter Counter -> zeit-basiert starten. So liegt der erste
            # Counter garantiert ueber jedem frueheren Wert (auch nach Neuinstallation),
            # ohne dass die Satelliten den Frame als Replay verwerfen.
            return int(time.time())

    def _save_counter(self) -> None:
        try:
            os.makedirs(os.path.dirname(self.counter_file), exist_ok=True)
            tmp = self.counter_file + ".tmp"
            with open(tmp, "w") as f:
                f.write(str(self._counter))
                f.flush()
                os.fsync(f.fileno())
            os.replace(tmp, self.counter_file)  # atomar
        except OSError:
            pass

    # ---- Verbindung ----
    def open(self) -> None:
        if self._serial is None:
            self._serial = serial.Serial(self.port, self.baud, timeout=0.5)

    def close(self) -> None:
        if self._serial is not None:
            self._serial.close()
            self._serial = None

    # ---- Senden ----
    def send(self, cmd: int, args: bytes = b"") -> int:
        """Baut Frame mit naechstem Counter und sendet ihn. Gibt den Counter zurueck."""
        with self._lock:
            self.open()
            with _CounterFileLock(self.counter_file):
                # Platte ist die eigentliche Quelle der Wahrheit ueber Prozessgrenzen:
                # ein langlaufender deck_daemon haelt self._counter nur im RAM und
                # wuerde ohne diesen Re-Read sonst denselben (oder einen niedrigeren)
                # Wert benutzen wie ein zwischenzeitlich gestarteter deck-ctl-Prozess
                # (oder umgekehrt) — beide schreiben dieselbe counter_file.
                self._counter = max(self._counter, self._load_counter()) + 1
                frame = ukfe_rf.build_frame(self.secret, self._counter, cmd, args)
                # Zuerst persistieren, dann senden: stuerzt der Prozess (Kill/Stromausfall)
                # zwischen diesen beiden Schritten ab, bleibt hoechstens ein ungenutzter
                # Counter-Wert liegen (unkritisch, siehe _load_counter()) statt dass der
                # naechste Prozessstart mit einem Counter weitermacht, den ein Satellit schon
                # als "gesendet" akzeptiert hat und seinerseits als Replay verwirft.
                self._save_counter()
                assert self._serial is not None
                self._serial.write(frame)
                self._serial.flush()
            return self._counter

    # ---- Bequeme Befehle ----
    def status(self) -> int:
        return self.send(ukfe_rf.CMD_STATUS)

    def trigger(self, payload_id: int, delay_ms: int = 0) -> int:
        return self.send(ukfe_rf.CMD_TRIGGER,
                         bytes([payload_id & 0xFF]) + int(delay_ms).to_bytes(4, "little"))

    def wifi_deauth(self, bssid: bytes, channel: int) -> int:
        if len(bssid) != 6:
            raise ValueError("bssid muss 6 Byte sein")
        return self.send(ukfe_rf.CMD_WIFI_DEAUTH, bssid + bytes([channel & 0xFF]))

    def evil_portal(self, portal_id: int = 0) -> int:
        return self.send(ukfe_rf.CMD_EVIL_PORTAL, bytes([portal_id & 0xFF]))

    def beacon_spam(self, mode: int = 0) -> int:
        return self.send(ukfe_rf.CMD_BEACON_SPAM, bytes([mode & 0xFF]))

    def abort(self) -> int:
        return self.send(ukfe_rf.CMD_ABORT)

    def hid_ducky(self, script_id: int) -> int:
        """Loest ein am Satelliten eingebautes DuckyScript per id aus (0x51)."""
        return self.send(ukfe_rf.CMD_HID_DUCKY, bytes([script_id & 0xFF]))

    def hid_stream(self, script: str, chunk_delay_s: float = 0.05) -> int:
        """Zerlegt ein beliebig langes DuckyScript in 0x52-Chunks (max 39 Nutzbytes/Frame,
        1 Byte fuer flags) und streamt sie ueber den Hub an den Satelliten. flags: Bit0=first
        (Puffer leeren), Bit1=last (tippen). Gibt die Anzahl gesendeter Frames zurueck."""
        data = script.encode("utf-8", "replace")
        per = ukfe_rf.MAX_ARGS - 1  # 1 Byte geht fuer flags drauf
        chunks = [data[i:i + per] for i in range(0, len(data), per)] or [b""]
        for idx, chunk in enumerate(chunks):
            flags = (0x01 if idx == 0 else 0x00) | (0x02 if idx == len(chunks) - 1 else 0x00)
            self.send(ukfe_rf.CMD_HID_STREAM, bytes([flags]) + chunk)
            if idx < len(chunks) - 1 and chunk_delay_s:
                time.sleep(chunk_delay_s)  # Satellit muss jeden Frame verarbeiten
        return len(chunks)
