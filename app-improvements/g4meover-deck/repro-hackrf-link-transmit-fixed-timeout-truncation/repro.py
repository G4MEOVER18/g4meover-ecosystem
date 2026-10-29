#!/usr/bin/env python3
"""Host-Repro: hackrf_link.transmit() vor/nach dem Timeout-Fix.

Transkribiert die betroffene Logik aus deck/hackrf_link.py (ALT = vor dem Fix,
NEU = nach dem Fix) 1:1 als eigenstaendige Funktionen. Ein MockProc bildet nur
das von transmit() tatsaechlich genutzte Verhalten von subprocess.run() nach:
wirft subprocess.TimeoutExpired, wenn der uebergebene timeout-Wert kuerzer ist
als die tatsaechlich fuer die Datei benoetigte Sendezeit ("air_seconds" --
Dateigroesse in Byte / 2 Byte pro IQ-Sample / sample_rate), sonst liefert es
genau den konfigurierten returncode. Kein HackRF-/Hardwarezugriff noetig.
"""
from __future__ import annotations
import subprocess


class MockProc:
    """Ersetzt subprocess.run() fuer den einen Aufruf aus transmit()."""

    def __init__(self, air_seconds: float, returncode: int = 0):
        self.air_seconds = air_seconds   # tatsaechlich benoetigte Sendezeit
        self.returncode = returncode
        self.calls: list[float] = []     # mit welchem timeout jeweils aufgerufen wurde

    def run(self, cmd, capture_output=True, text=True, timeout=None):
        self.calls.append(timeout)
        if timeout is not None and timeout < self.air_seconds:
            # hackrf_transfer wird mitten in der Uebertragung gekillt
            raise subprocess.TimeoutExpired(cmd, timeout)
        return subprocess.CompletedProcess(cmd, self.returncode, stdout="", stderr="")


# ---------------------------------------------------------------------------
# ALT (vor dem Fix): fester timeout=120, unabhaengig von Dateigroesse/Sendezeit.
# ---------------------------------------------------------------------------
def transmit_alt(proc: MockProc, size_bytes: int, freq_mhz: float,
                  sample_rate: int = 8_000_000) -> bool:
    r = proc.run(["hackrf_transfer", "-t", "path", "-f", str(int(freq_mhz * 1e6)),
                  "-s", str(sample_rate), "-x", "20"],
                 capture_output=True, text=True, timeout=120)
    return r.returncode == 0


# ---------------------------------------------------------------------------
# NEU (nach dem Fix): timeout aus Dateigroesse/sample_rate abgeleitet,
# TimeoutExpired wird aufgefangen statt durchzuschlagen.
# ---------------------------------------------------------------------------
def transmit_neu(proc: MockProc, size_bytes: int, freq_mhz: float,
                  sample_rate: int = 8_000_000) -> bool:
    air_seconds = size_bytes / 2 / sample_rate   # 8-Bit I/Q = 2 Byte/Sample
    timeout = max(air_seconds + 15, 30)
    try:
        r = proc.run(["hackrf_transfer", "-t", "path", "-f", str(int(freq_mhz * 1e6)),
                      "-s", str(sample_rate), "-x", "20"],
                     capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return False
    return r.returncode == 0


# ---------------------------------------------------------------------------
checks = 0
failed = 0


def check(label: str, cond: bool):
    global checks, failed
    checks += 1
    status = "OK" if cond else "FEHLGESCHLAGEN"
    if not cond:
        failed += 1
    print(f"[{status}] {label}")


DEFAULT_SR = 8_000_000

# Fall 1: kurze Datei (10s Sendezeit) -- ALT und NEU muessen beide erfolgreich senden.
short_bytes = int(10 * DEFAULT_SR * 2)
p_alt_short = MockProc(air_seconds=10)
check("ALT: kurze Datei (10s) sendet erfolgreich",
      transmit_alt(p_alt_short, short_bytes, 433.92) is True)
p_neu_short = MockProc(air_seconds=10)
check("NEU: kurze Datei (10s) sendet erfolgreich (Regressionsfall)",
      transmit_neu(p_neu_short, short_bytes, 433.92) is True)
check("NEU: kurze Datei bekommt timeout >= 30s Mindestwert",
      p_neu_short.calls[-1] == 30)

# Fall 2: GPS-Sim-Replay, 300s Sendezeit (realistischer gps-sdr-sim-Dauerlauf,
# deutlich laenger als die alte fixe 120s-Grenze) -- der eigentliche Fund.
long_bytes = int(300 * DEFAULT_SR * 2)
p_alt_long = MockProc(air_seconds=300)
alt_crashed = False
try:
    transmit_alt(p_alt_long, long_bytes, 1575.42)
except subprocess.TimeoutExpired:
    alt_crashed = True
check("ALT: 300s-Datei (z.B. GPS-Sim-Replay) wird nach 120s abgewuergt und "
      "transmit() stuerzt mit TimeoutExpired ab statt False zurueckzugeben",
      alt_crashed is True)

p_neu_long = MockProc(air_seconds=300)
res_neu_long = transmit_neu(p_neu_long, long_bytes, 1575.42)
check("NEU: 300s-Datei sendet vollstaendig durch (timeout jetzt an die "
      "Dateigroesse angepasst)", res_neu_long is True)
check("NEU: timeout fuer die 300s-Datei liegt bei air_seconds+15 (315s), "
      "nicht an der alten 120s-Grenze",
      p_neu_long.calls[-1] == 315.0)

# Fall 3: Datei, die selbst mit dem neuen, angepassten timeout noch zu knapp
# bemessen waere (hackrf_transfer haengt laenger als air_seconds+15, z.B. durch
# USB-Stottern) -- NEU darf dann nicht abstuerzen, sondern muss False liefern.
p_neu_stall = MockProc(air_seconds=400)   # braucht laenger als die berechneten 315s
res_neu_stall = transmit_neu(p_neu_stall, long_bytes, 1575.42)
check("NEU: haengender Transfer (> berechnetem timeout) liefert False statt "
      "eine Exception durchzulassen", res_neu_stall is False)

# Fall 4: Fehlschlag des Tools selbst (returncode != 0) -- beide Varianten
# muessen das unveraendert als False melden (kein neues Fehlverhalten).
p_alt_fail = MockProc(air_seconds=10, returncode=1)
check("ALT: Tool-Fehlschlag (returncode=1) liefert False",
      transmit_alt(p_alt_fail, short_bytes, 433.92) is False)
p_neu_fail = MockProc(air_seconds=10, returncode=1)
check("NEU: Tool-Fehlschlag (returncode=1) liefert weiterhin False",
      transmit_neu(p_neu_fail, short_bytes, 433.92) is False)

print(f"\n{'ALL CHECKS PASSED' if failed == 0 else f'{failed} CHECK(S) FAILED'} "
      f"({checks} total)")
import sys
sys.exit(1 if failed else 0)
