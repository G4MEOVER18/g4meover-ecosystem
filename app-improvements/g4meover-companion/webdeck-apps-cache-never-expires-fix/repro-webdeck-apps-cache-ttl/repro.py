#!/usr/bin/env python3
"""Host-Repro: webdeck.py api_apps() vor/nach dem TTL-Cache-Fix.

Transkribiert die betroffene Logik aus webdeck/webdeck.py::api_apps() 1:1 als
eigenstaendige ALT-/NEU-Funktionen (reiner Zustandsautomat: Cache-Dict +
Mock-Uhr statt time.monotonic(), MockBridge statt FlipperBridge/pyserial).
Kein Hardware-/Seriell-/Netzwerkzugriff noetig, keine externen Abhaengigkeiten.

ALT: `if _apps_cache is not None: return _apps_cache` -- einmal befuellt, nie
wieder neu abgefragt, solange der webdeck.py-Prozess laeuft.
NEU: zusaetzlich ein Zeitstempel + 30s-TTL (APPS_CACHE_TTL_S) -- nach Ablauf
wird wieder echt beim Flipper nachgefragt.
"""
from __future__ import annotations
import re

CATS = ["Sub-GHz", "NFC", "RFID", "Infrared", "GPIO", "iButton",
        "USB", "Bluetooth", "Games", "Media", "Tools"]

APPS_CACHE_TTL_S = 30.0


class MockBridge:
    """Simuliert FlipperBridge.storage_list(): liefert pro Kategorie eine
    `storage list`-Antwort mit so vielen `[F] *.fap`-Zeilen wie aktuell in
    `fap_counts[cat]` hinterlegt. `fap_counts` ist vom Test jederzeit
    veraenderbar (simuliert App-Install/-Deinstall auf dem echten Geraet)."""

    def __init__(self, fap_counts: dict[str, int]):
        self.fap_counts = fap_counts
        self.calls = 0  # zaehlt echte storage_list()-Aufrufe (= Seriell-Traffic)

    def storage_list(self, path: str) -> str:
        self.calls += 1
        cat = path.rsplit("/", 1)[-1]
        n = self.fap_counts.get(cat, 0)
        lines = [f"\t[F] app{i}.fap {100 + i}b" for i in range(n)]
        return "\r\n".join(lines) if lines else "\tEmpty\r\n"


def _count(bridge: MockBridge) -> dict:
    counts = {}
    for cat in CATS:
        out = bridge.storage_list(f"/ext/apps/{cat}")
        n = len(re.findall(r"\[F\].+\.fap", out))
        if n:
            counts[cat] = n
    return counts


class CacheState:
    def __init__(self):
        self.apps_cache = None
        self.apps_cache_ts = 0.0


def api_apps_old(bridge: MockBridge, state: CacheState, now: float) -> dict:
    """1:1-Transkript der Vor-Fix-Logik (unbedingt-permanenter Cache)."""
    if state.apps_cache is not None:
        return state.apps_cache
    state.apps_cache = {"counts": (c := _count(bridge)), "total": sum(c.values())}
    return state.apps_cache


def api_apps_new(bridge: MockBridge, state: CacheState, now: float) -> dict:
    """1:1-Transkript der Nach-Fix-Logik (TTL-Cache, `now` statt time.monotonic())."""
    if state.apps_cache is not None and (now - state.apps_cache_ts) < APPS_CACHE_TTL_S:
        return state.apps_cache
    c = _count(bridge)
    state.apps_cache = {"counts": c, "total": sum(c.values())}
    state.apps_cache_ts = now
    return state.apps_cache


def check(label, cond):
    status = "OK" if cond else "FEHLER"
    print(f"[{status}] {label}")
    return cond


def main():
    all_ok = True

    # --- ALT: Fund selbst -- Cache bleibt nach einer App-Installation falsch ---
    bridge = MockBridge({"Tools": 3, "Sub-GHz": 2})
    state = CacheState()
    r1 = api_apps_old(bridge, state, now=0.0)
    all_ok &= check("ALT erster Aufruf liefert echten Stand (Total=5)", r1["total"] == 5)
    all_ok &= check("ALT erster Aufruf fragt das Geraet einmal pro Kategorie ab",
                    bridge.calls == len(CATS))

    # User installiert zwei neue Apps auf dem Flipper (realistisch: Stunden spaeter,
    # waehrend der webdeck.py-Server durchlaeuft)
    bridge.fap_counts["Tools"] = 5
    r2 = api_apps_old(bridge, state, now=99999.0)  # auch lange danach
    all_ok &= check(
        "FUND: ALT zeigt nach Install weiterhin den veralteten Stand (Total=5 statt 7)",
        r2["total"] == 5,
    )
    all_ok &= check("ALT fragt beim zweiten Aufruf das Geraet gar nicht erneut ab",
                    bridge.calls == len(CATS))

    # --- NEU: innerhalb der TTL weiterhin gecacht (kein unnoetiger Seriell-Traffic) ---
    bridge2 = MockBridge({"Tools": 3, "Sub-GHz": 2})
    state2 = CacheState()
    r1n = api_apps_new(bridge2, state2, now=1000.0)
    calls_after_first = bridge2.calls
    all_ok &= check("NEU erster Aufruf liefert echten Stand (Total=5)", r1n["total"] == 5)

    bridge2.fap_counts["Tools"] = 5  # Install noch innerhalb der TTL
    r2n = api_apps_new(bridge2, state2, now=1000.0 + APPS_CACHE_TTL_S - 0.001)
    all_ok &= check(
        "NEU liefert knapp vor Ablauf der TTL weiterhin den gecachten Stand (Total=5)",
        r2n["total"] == 5,
    )
    all_ok &= check("NEU fragt innerhalb der TTL das Geraet nicht erneut ab",
                    bridge2.calls == calls_after_first)

    # --- NEU: nach Ablauf der TTL wird echt neu abgefragt -- Fix greift ---
    r3n = api_apps_new(bridge2, state2, now=1000.0 + APPS_CACHE_TTL_S + 0.001)
    all_ok &= check(
        "FIX: NEU zeigt nach Ablauf der TTL den aktuellen Stand (Total=7)",
        r3n["total"] == 7,
    )
    all_ok &= check("NEU fragt nach Ablauf der TTL das Geraet erneut ab (ein Satz)",
                    bridge2.calls == calls_after_first + len(CATS))

    # --- NEU: Regression -- ohne Aenderung am Geraet bleibt der Stand nach Refresh gleich ---
    r4n = api_apps_new(bridge2, state2, now=1000.0 + 2 * APPS_CACHE_TTL_S)
    all_ok &= check(
        "Regression: unveraenderter Geraetestand bleibt nach erneutem Refresh gleich (Total=7)",
        r4n["total"] == 7,
    )

    print()
    print("ALLE PRUEFUNGEN BESTANDEN" if all_ok else "MINDESTENS EINE PRUEFUNG FEHLGESCHLAGEN")
    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
