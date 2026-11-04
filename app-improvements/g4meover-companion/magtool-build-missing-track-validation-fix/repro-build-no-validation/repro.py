#!/usr/bin/env python3
"""Host-Repro: magtool `build` schreibt ALT ungeprueft, NEU validiert vor dem Schreiben.

Importiert ausschliesslich die unveraenderten Bausteine `build_track1`,
`build_track2`, `validate_track1`, `validate_track2` aus der echten, unmodifizierten
`magtool.py` (Commit ceb068bf278feae4a8180d24330111e0b911301d) -- diese Funktionen
aendert der Fix nicht. ALT/NEU transkribieren 1:1 den fraglichen Ausschnitt aus
`cmd_build()` vor/nach dem Patch; kein argparse-/Hardwarezugriff noetig.

Aufruf: python3 repro.py  (erwartet magtool.py im selben Repo-Checkout, Pfad unten anpassbar)
"""
import importlib.util
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
# Pfad zur unveraenderten magtool.py im lokalen Checkout von g4meover-companion.
MAGTOOL_PY = os.environ.get(
    "MAGTOOL_PY", os.path.join(HERE, "..", "..", "..", "..", "..", "g4meover-companion", "magtool", "magtool.py")
)

spec = importlib.util.spec_from_file_location("magtool_orig", MAGTOOL_PY)
magtool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(magtool)


def alt_cmd_build(pan, name, expiry, service, track3=""):
    """ALT: 1:1 aus cmd_build() vor dem Fix -- schreibt ungeprueft."""
    t1 = magtool.build_track1(pan, name, expiry, service)
    t2 = magtool.build_track2(pan, expiry, service)
    t3 = track3 or ""
    content = (
        f"{magtool.MAG_HEADER}\n{magtool.MAG_VERSION}\n"
        f"# Mag device track data\n"
        f"Track 1: {t1}\n"
        f"Track 2: {t2}\n"
        f"Track 3: {t3}\n"
    )
    out = tempfile.NamedTemporaryFile(mode="w", suffix=".mag", delete=False, newline="\n")
    out.write(content)
    out.close()
    return True, out.name, t1, t2


def neu_cmd_build(pan, name, expiry, service, track3=""):
    """NEU: 1:1 aus cmd_build() nach dem Fix -- validiert vor dem Schreiben."""
    t1 = magtool.build_track1(pan, name, expiry, service)
    t2 = magtool.build_track2(pan, expiry, service)
    t3 = track3 or ""
    errs = {1: magtool.validate_track1(t1), 2: magtool.validate_track2(t2)}
    if t3:
        errs[3] = magtool.validate_track2(t3, 107)
    bad = {n: es for n, es in errs.items() if es}
    if bad:
        return False, None, t1, t2
    content = (
        f"{magtool.MAG_HEADER}\n{magtool.MAG_VERSION}\n"
        f"# Mag device track data\n"
        f"Track 1: {t1}\n"
        f"Track 2: {t2}\n"
        f"Track 3: {t3}\n"
    )
    out = tempfile.NamedTemporaryFile(mode="w", suffix=".mag", delete=False, newline="\n")
    out.write(content)
    out.close()
    return True, out.name, t1, t2


CASES = [
    ("normaler Name (kein Sonderzeichen)",
     dict(pan="4111111111111111", name="SMITH/JOHN", expiry="2512", service="101"),
     True),
    ("Fund: Caret im --name (dritter Feldtrenner in Track 1)",
     dict(pan="4111111111111111", name="SMITH^JOHN", expiry="2512", service="101"),
     False),
    ("Fund: nicht in T1_CHARSET erlaubtes Zeichen im Namen (Umlaut bleibt nach .upper())",
     dict(pan="4111111111111111", name="MÜLLER", expiry="2512", service="101"),
     False),
    ("Fund: ungueltiger --track3 (Sentinels/Zeichensatz/Feldtrenner falsch)",
     dict(pan="4111111111111111", name="SMITH/JOHN", expiry="2512", service="101",
          track3="ABC^123"),
     False),
    ("Regression: gueltiger, nicht-leerer --track3 bleibt erlaubt",
     dict(pan="4111111111111111", name="SMITH/JOHN", expiry="2512", service="101",
          track3=";9999999999=0000?"),
     True),
]

passed = 0
failed = 0
for label, kwargs, should_be_valid in CASES:
    alt_ok, alt_path, alt_t1, alt_t2 = alt_cmd_build(**kwargs)
    neu_ok, neu_path, neu_t1, neu_t2 = neu_cmd_build(**kwargs)

    # ALT schreibt in JEDEM Fall (das ist genau der Fund) -- track1/track2 muessen
    # zwischen ALT und NEU identisch aufgebaut sein (reine Zusatzpruefung, keine
    # Aenderung an build_track1()/build_track2() selbst).
    build_identical = (alt_t1 == neu_t1) and (alt_t2 == neu_t2)
    alt_always_writes = alt_ok is True
    neu_matches_expectation = neu_ok == should_be_valid

    ok = build_identical and alt_always_writes and neu_matches_expectation
    status = "OK" if ok else "FEHLGESCHLAGEN"
    print(f"[{status}] {label}")
    print(f"    Track1={alt_t1!r}")
    print(f"    ALT: schreibt={alt_ok}  NEU: schreibt={neu_ok} (erwartet gueltig={should_be_valid})")
    if alt_path:
        os.unlink(alt_path)
    if neu_path:
        os.unlink(neu_path)
    if ok:
        passed += 1
    else:
        failed += 1

print(f"\n{passed}/{passed + failed} Faelle bestanden.")
sys.exit(0 if failed == 0 else 1)
