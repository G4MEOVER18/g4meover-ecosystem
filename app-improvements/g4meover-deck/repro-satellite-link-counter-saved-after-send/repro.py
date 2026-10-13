#!/usr/bin/env python3
"""Host-Repro: g4meover-deck SatelliteLink.send() persistiert den Rolling-Counter
ERST NACH dem UART-Write (satellite_link_orig.py, 1:1 Originalstand) statt davor
(satellite_link_fixed.py, mit dem vorgeschlagenen Patch).

Nutzt die echten, unveraenderten Original-Dateien `ukfe_rf.py` (TX-Frame-Bau) und
`satellite_link_orig.py`/`satellite_link_fixed.py` (1:1 aus deck/satellite_link.py
vor/nach dem Patch) -- keine pyserial-/UART-Abhaengigkeit, siehe serial.py in diesem
Verzeichnis. Modelliert nur die Satelliten-seitige Anti-Replay-Pruefung
(`counter <= last_counter -> verwerfen`), die in jeder Geraete-Firmware
(heltec-ukfe-rx/lilygo-ukfe-rx/heltec-v4-hub/usb-army-penetrator) identisch in
`ukfe_rf_parse_frame()` steht.

    $ python3 repro.py
"""
from __future__ import annotations

import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import satellite_link_orig  # noqa: E402  (Original, Counter-Save NACH dem Write)
import satellite_link_fixed  # noqa: E402  (Patch, Counter-Save VOR dem Write)
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


def _make_link(module, counter_file: str, crash_after_write: bool):
    link = module.SatelliteLink(port="MOCK", baud=115200, counter_file=counter_file)
    link._serial = serial.Serial("MOCK", 115200)
    link._serial.crash_after_write = crash_after_write
    return link


def run_scenario(label: str, module) -> bool:
    print(f"--- {label} ---")
    with tempfile.TemporaryDirectory() as tmp:
        counter_file = os.path.join(tmp, "counter")
        sat = Satellite()

        # Sitzung 1: fuenf normale Befehle (kein Crash) -- Satellit folgt brav.
        link1 = _make_link(module, counter_file, crash_after_write=False)
        link1._serial.on_write = sat.on_frame
        for _ in range(5):
            link1.send(ukfe_rf.CMD_STATUS)
        print(f"Sitzung 1: 0 verworfen (erwartet), Satellit zuletzt akzeptiert: "
              f"Counter {sat.last_counter}")
        assert sat.rejected == 0 and sat.accepted == 5

        # Sechster Befehl: UART-Write liefert das Frame aus (Satellit sieht es sofort
        # und akzeptiert es), der Host-Prozess stirbt aber direkt danach (Kill/
        # Stromausfall) -- bei satellite_link_orig.py passiert das NACH write(), aber
        # VOR _save_counter(); bei satellite_link_fixed.py ist _save_counter() an
        # dieser Stelle schon durchgelaufen.
        link1._serial.crash_after_write = True
        try:
            link1.send(ukfe_rf.CMD_STATUS)
            raise AssertionError("Crash haette hier auftreten muessen")
        except serial.Crash:
            pass
        print(f"Crash bei Counter {sat.last_counter} (vom Satelliten bereits akzeptiert)")

        # Neustart: frischer Prozess, laedt den Counter nur von der Platte.
        link2 = _make_link(module, counter_file, crash_after_write=False)
        link2._serial.on_write = sat.on_frame
        rejected_after_restart = 0
        for _ in range(10):
            link2.send(ukfe_rf.CMD_STATUS)
        rejected_after_restart = sat.rejected
        print(f"Nach Neustart: {rejected_after_restart}/10 Status-Befehle vom "
              f"Satelliten verworfen")
        return rejected_after_restart == 0


def main() -> int:
    bug_clean = run_scenario("satellite_link_orig.py (Counter-Save NACH dem Write)",
                              satellite_link_orig)
    print()
    fix_clean = run_scenario("satellite_link_fixed.py (Counter-Save VOR dem Write)",
                              satellite_link_fixed)
    print()
    ok = True
    if bug_clean:
        print("UNERWARTET: satellite_link_orig.py hat keinen Befehl verworfen")
        ok = False
    else:
        print("BUG bestaetigt: satellite_link_orig.py verwirft nach dem simulierten "
              "Absturz den naechsten Befehl als bereits verbraucht (Selbstaussperrung "
              "fuer genau den einen verlorenen Counter-Schritt)")
    if not fix_clean:
        print("FEHLER: satellite_link_fixed.py haette 0/10 verwerfen muessen")
        ok = False
    else:
        print("FIX bestaetigt: satellite_link_fixed.py verwirft nach demselben "
              "simulierten Absturz keinen einzigen Befehl")
    print()
    print("Alle Szenarien bestaetigt" if ok else "FEHLGESCHLAGEN")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
