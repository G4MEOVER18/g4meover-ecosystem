#!/usr/bin/env python3
"""Host-Repro: FlipperBridge.storage_send() Verifikationslogik, ALT vs. NEU.

Isoliert genau den Vergleichsschritt am Ende von storage_send() (Datei ist zu
diesem Zeitpunkt schon per write_chunk geschrieben, "Ready" wurde bereits
bestaetigt -- es geht nur noch um den md5-Soll/Ist-Vergleich). Kein
pyserial-/Hardwarezugriff noetig: der "Soll"-Wert ist eine reine
Python-hashlib-Berechnung ueber `data`, der "Ist"-Wert wird hier durch eine
Fake-cmd()-Funktion simuliert, die genau das zurueckgibt, was `cmd('storage
md5 "..."')` auf dem echten Geraet liefern wuerde (letztes Wort der Antwort
= Hex-MD5).

ALT und NEU sind 1:1 aus flipperbridge/flipperbridge.py::storage_send()
transkribiert (nur der Verifikationsteil, der Rest -- write_chunk/Ready/Write
-- ist fuer diesen Fund irrelevant und wird hier als bereits erfolgt
angenommen).
"""
import hashlib
import sys


def _md5(data):
    return hashlib.md5(data).hexdigest()


def verify_alt(data, cmd_md5_result):
    """1:1 aus dem Status quo: `got = cmd(...) if data else ""`."""
    want = _md5(data)
    got = cmd_md5_result if data else ""
    return want == got


def verify_neu(data, cmd_md5_result):
    """1:1 aus dem Fix: leere Datei -> nichts zu vergleichen, sofort True."""
    if not data:
        return True
    want = _md5(data)
    got = cmd_md5_result
    return want == got


def main():
    failures = []

    def check(label, got, expected):
        status = "OK" if got == expected else "FAIL"
        if got != expected:
            failures.append(label)
        print(f"[{status}] {label}: got={got!r} expected={expected!r}")

    # 1) Nicht-leere Datei, Geraet liefert korrektes md5 zurueck (Normalfall,
    #    Upload tatsaechlich erfolgreich) -- ALT und NEU muessen beide True liefern.
    data_a = b"hello world"
    cmd_result_a = _md5(data_a)  # Geraet meldet exakt den richtigen Hash zurueck
    check("ALT: nicht-leer, Hash stimmt -> True", verify_alt(data_a, cmd_result_a), True)
    check("NEU: nicht-leer, Hash stimmt -> True", verify_neu(data_a, cmd_result_a), True)

    # 2) Nicht-leere Datei, Geraet liefert FALSCHES md5 zurueck (simulierte
    #    Uebertragungsstoerung) -- ALT und NEU muessen beide False liefern,
    #    der Fix darf die echte Fehlererkennung nicht abschwaechen.
    data_b = b"hello world"
    cmd_result_b = _md5(b"HELLO WORLD")  # anderer Inhalt -> anderer Hash
    check("ALT: nicht-leer, Hash falsch -> False", verify_alt(data_b, cmd_result_b), False)
    check("NEU: nicht-leer, Hash falsch -> False", verify_neu(data_b, cmd_result_b), False)

    # 3) FUND: leere Datei (0 Byte). storage_send() ruft bei leeren Daten gar
    #    kein "storage md5"-Kommando auf (das `if data` in der ALT-Zeile
    #    unterdrueckt genau das), d.h. cmd_md5_result ist in der Praxis
    #    irrelevant -- wird hier trotzdem als "waere ohnehin korrekt"
    #    durchgereicht, um zu zeigen: selbst ein korrektes Ergebnis haette
    #    ALT nicht gerettet.
    data_c = b""
    cmd_result_c = _md5(b"")  # selbst wenn das Geraet "liefe" und korrekt antwortete
    want_c = _md5(data_c)
    print(f"      (want fuer leere Datei ist {want_c!r}, NICHT die leere Zeichenkette)")
    check("ALT: leere Datei -> False (BUG: sollte als Erfolg gelten)", verify_alt(data_c, cmd_result_c), False)
    check("NEU: leere Datei -> True (Fix)", verify_neu(data_c, cmd_result_c), True)

    # 4) Randfall: leere Datei, Geraet liefert (hypothetisch) wirklich eine
    #    leere Antwort auf "storage md5" zurueck -- ALT bleibt dennoch False,
    #    weil `want` nie leer ist; zeigt, dass der Fehler strukturell ist
    #    (asymmetrischer Vergleich), nicht vom tatsaechlichen Geraete-Verhalten
    #    abhaengt.
    check(
        "ALT: leere Datei, cmd liefert '' zurueck -> trotzdem False (BUG)",
        verify_alt(b"", ""),
        False,
    )
    check("NEU: leere Datei, cmd liefert '' zurueck -> True", verify_neu(b"", ""), True)

    print()
    if failures:
        print(f"ERGEBNIS: {len(failures)} unerwartete(s) Ergebnis(se): {failures}")
        sys.exit(1)
    print("ERGEBNIS: ALLE PRUEFUNGEN WIE ERWARTET (ALT reproduziert den Fund, NEU behebt ihn).")
    sys.exit(0)


if __name__ == "__main__":
    main()
