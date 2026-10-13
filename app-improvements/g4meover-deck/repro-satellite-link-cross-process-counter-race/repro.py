#!/usr/bin/env python3
"""Host-Repro: g4meover-deck SatelliteLink ohne Datei-Lock ueber Prozessgrenzen
(satellite_link_before.py, 1:1 Stand nach dem bereits vorgeschlagenen
satellite-link-counter-saved-after-send-fix) vs. MIT Cross-Prozess-Lock
(satellite_link_after.py, dieser Patch).

Nutzt die echte, unveraenderte `ukfe_rf.py` (TX-Frame-Bau) und denselben
Satelliten-Mock wie im vorigen Repro (`counter <= last_counter -> verwerfen`,
identisch in jeder Geraete-Firmware). `serial.py` bildet nur die von
satellite_link.py benutzte pyserial-Oberflaeche nach, keine echte UART-Abhaengigkeit.

Modelliert: `deck_daemon` (ein SatelliteLink-Objekt, lebt ueber viele send()-Aufrufe)
UND ein einzelner `deck-ctl sat ...`-Aufruf (ein FRISCH konstruiertes SatelliteLink-
Objekt, laedt den Counter nur EINMAL bei __init__) teilen sich dieselbe counter_file —
exakt wie in deck_daemon.py (ein langlaufender _link) und deck_ctl.py (make_link() pro
Aufruf) tatsaechlich der Fall ist.

    $ python3 repro.py
"""
from __future__ import annotations

import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import satellite_link_before  # noqa: E402  (kein Cross-Prozess-Lock)
import satellite_link_after   # noqa: E402  (mit Cross-Prozess-Lock, dieser Patch)
import serial  # noqa: E402  (Mock, siehe serial.py)
import ukfe_rf  # noqa: E402


class Satellite:
    """Bildet exakt `if (counter <= last_counter) reject; else last_counter = counter;`
    nach (siehe z. B. heltec-ukfe-rx/src/main.cpp, ukfe_rf_parse_frame())."""

    def __init__(self) -> None:
        self.last_counter = 0
        self.rejected = 0
        self.accepted = 0

    def on_frame(self, frame: bytes) -> None:
        counter = int.from_bytes(frame[3:7], "little")
        if counter <= self.last_counter:
            self.rejected += 1
            return
        self.last_counter = counter
        self.accepted += 1


def _make_link(module, counter_file: str, sat: Satellite):
    link = module.SatelliteLink(port="MOCK", baud=115200, counter_file=counter_file)
    link._serial = serial.Serial("MOCK", 115200)
    link._serial.on_write = sat.on_frame
    return link


def run_scenario(label: str, module) -> int:
    """Gibt zurueck, wie viele der zehn `deck-ctl`-Nachlauf-Aufrufe (je ein frisch
    konstruiertes SatelliteLink) vom Satelliten verworfen wurden."""
    print(f"--- {label} ---")
    with tempfile.TemporaryDirectory() as tmp:
        counter_file = os.path.join(tmp, "counter")
        with open(counter_file, "w") as f:
            f.write("1000")  # deterministischer Startwert statt time.time()
        sat = Satellite()

        # deck_daemon startet, laedt counter_file=1000.
        daemon_link = _make_link(module, counter_file, sat)

        # Ein deck-ctl-Prozess startet GLEICHZEITIG (liest counter_file noch im
        # selben Zustand=1000, bevor der Daemon weitergezaehlt hat) -- braucht dann
        # aber (Argparse, Config laden, ...) eine Weile, bis er tatsaechlich sendet.
        ctl_link = _make_link(module, counter_file, sat)

        # Waehrenddessen bedient der Daemon fuenf eingehende HTTP-Requests.
        for _ in range(5):
            daemon_link.send(ukfe_rf.CMD_STATUS)
        print(f"Daemon-Burst: Satellit zuletzt akzeptiert Counter {sat.last_counter} "
              f"({sat.accepted} angenommen, {sat.rejected} verworfen)")

        # Jetzt feuert der laengst gestartete deck-ctl-Prozess seinen EINEN Befehl ab --
        # sein SatelliteLink kennt den Daemon-Burst nicht (eigene In-Memory-Kopie).
        ctl_link.send(ukfe_rf.CMD_STATUS)
        ctl_rejected = sat.rejected > 0
        print(f"deck-ctl-Aufruf: {'VERWORFEN' if ctl_rejected else 'angenommen'} "
              f"(Satellit zuletzt akzeptiert Counter {sat.last_counter}, "
              f"counter_file jetzt={open(counter_file).read().strip()})")

        # Zehn WEITERE, je frisch gestartete deck-ctl-Aufrufe im Anschluss (die counter_file
        # koennte durch den vorigen Aufruf bereits unter den tatsaechlichen Satelliten-Stand
        # zurueckgefallen sein -- dann verwirft der Satellit jeden weiteren Aufruf, bis der
        # auf der Platte gespeicherte Counter den wahren Stand wieder eingeholt hat).
        followup_rejected = 0
        for _ in range(10):
            link = _make_link(module, counter_file, sat)
            before = sat.rejected
            link.send(ukfe_rf.CMD_STATUS)
            if sat.rejected > before:
                followup_rejected += 1
        print(f"10 Nachlauf-deck-ctl-Aufrufe (je frisch gestartet): "
              f"{followup_rejected}/10 verworfen")
        return followup_rejected


def main() -> int:
    before = run_scenario(
        "satellite_link_before.py (Save-vor-Send, aber KEIN Cross-Prozess-Lock)",
        satellite_link_before)
    print()
    after = run_scenario(
        "satellite_link_after.py (mit Cross-Prozess-Lock, dieser Patch)",
        satellite_link_after)
    print()
    ok = True
    if before == 0:
        print("UNERWARTET: satellite_link_before.py hat keinen Nachlauf-Aufruf verworfen")
        ok = False
    else:
        print(f"BUG bestaetigt: satellite_link_before.py verwirft {before}/10 "
              "Nachlauf-deck-ctl-Aufruefe, weil die counter_file durch den "
              "unsynchronisierten deck-ctl-Zwischenaufruf unter den wahren, dem "
              "Satelliten bereits bekannten Stand zurueckgefallen ist.")
    if after != 0:
        print(f"FEHLER: satellite_link_after.py haette 0/10 verwerfen muessen, waren {after}")
        ok = False
    else:
        print("FIX bestaetigt: satellite_link_after.py verwirft 0/10 Nachlauf-Aufrufe -- "
              "jeder send() liest die counter_file unter dem Cross-Prozess-Lock neu und "
              "kann daher nie hinter den wahren, bereits vom Satelliten akzeptierten "
              "Stand zurueckfallen.")
    print()
    print("Alle Szenarien bestaetigt" if ok else "FEHLGESCHLAGEN")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
