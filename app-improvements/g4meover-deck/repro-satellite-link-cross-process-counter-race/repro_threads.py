#!/usr/bin/env python3
"""Zusatz-Check (nicht Teil des gelieferten Repros): echte gleichzeitige Threads statt
der deterministischen Sequenz aus repro.py, um auszuschliessen, dass der Fix nur im
sequentiellen Fall funktioniert. Zwei unabhaengige SatelliteLink-Instanzen (eigene
Locks, wie zwei echte Prozesse) feuern parallel je 50 send()-Aufrufe auf dieselbe
counter_file; der Satelliten-Mock zaehlt verworfene Frames.

    $ python3 repro_threads.py
"""
from __future__ import annotations

import os
import sys
import tempfile
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import satellite_link_before  # noqa: E402
import satellite_link_after  # noqa: E402
import serial  # noqa: E402
import ukfe_rf  # noqa: E402


class Satellite:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.last_counter = 0
        self.rejected = 0
        self.accepted = 0

    def on_frame(self, frame: bytes) -> None:
        counter = int.from_bytes(frame[3:7], "little")
        with self.lock:
            if counter <= self.last_counter:
                self.rejected += 1
            else:
                self.last_counter = counter
                self.accepted += 1


def run(module, n_per_thread=50) -> tuple[int, int]:
    with tempfile.TemporaryDirectory() as tmp:
        counter_file = os.path.join(tmp, "counter")
        with open(counter_file, "w") as f:
            f.write("1000")
        sat = Satellite()

        def worker():
            link = module.SatelliteLink(port="MOCK", baud=115200, counter_file=counter_file)
            link._serial = serial.Serial("MOCK", 115200)
            link._serial.on_write = sat.on_frame
            for _ in range(n_per_thread):
                link.send(ukfe_rf.CMD_STATUS)

        threads = [threading.Thread(target=worker) for _ in range(4)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        return sat.accepted, sat.rejected


for label, module in (("before (kein Lock)", satellite_link_before),
                      ("after (Cross-Prozess-Lock)", satellite_link_after)):
    accepted, rejected = run(module)
    print(f"{label}: {accepted} angenommen, {rejected} verworfen (von 200 gesendet)")
