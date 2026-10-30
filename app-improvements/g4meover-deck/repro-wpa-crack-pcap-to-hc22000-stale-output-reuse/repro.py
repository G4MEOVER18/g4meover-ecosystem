#!/usr/bin/env python3
"""Host-Repro (ohne hcxtools/hashcat/Hardware): pcap_to_hc22000() ALT vs. NEU.

hcxpcapngtool haengt laut eigenem Maintainer an eine bestehende -o-Zieldatei an,
statt sie zu ueberschreiben (siehe .md "Was"). ALT ruft das Tool nie mit einem
vorab bereinigten out_path auf und prueft auch keinen returncode -- ein zweiter
Lauf auf denselben Default-out_path (z.B. zwei "capture"-Sitzungen mit demselben
Standard-Dateinamen handshake.pcap/.hc22000) liefert dadurch eine Mischung aus
altem und neuem Stand, bzw. bei echtem Tool-Fehlschlag faelschlich den alten
Erfolg. Ein winziges Fake-"hcxpcapngtool"-Skript auf PATH simuliert exakt dieses
dokumentierte Verhalten -- kein echtes hcxtools/hashcat noetig.

transmitiert ALT/NEU als eigenstaendige Funktionspaare (identische Logik zur
jeweiligen deck/wpa_crack.py-Fassung), ruft aber denselben parse_hc22000() aus
dem echten, unveraenderten Modul auf.
"""
from __future__ import annotations
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, "/tmp/deck1/deck")  # fuer parse_hc22000() -- echtes, unveraendertes Modul
import wpa_crack as wc  # noqa: E402

checks = 0
failures = []


def check(name: str, cond: bool):
    global checks
    checks += 1
    if not cond:
        failures.append(name)
    print(f"[{'OK' if cond else 'FAIL'}] {name}")


FAKE_TOOL = """#!/usr/bin/env python3
import sys
import os
args = sys.argv[1:]
out_path = args[args.index("-o") + 1]
mode = os.environ.get("FAKE_HCX_MODE", "ok")
if mode == "fail_before_open":
    sys.exit(1)
if mode == "ok_truncate":
    with open(out_path, "w") as f:
        f.write(os.environ["FAKE_HCX_LINE"] + "\\n")
    sys.exit(0)
if mode == "ok_append":
    with open(out_path, "a") as f:
        f.write(os.environ["FAKE_HCX_LINE"] + "\\n")
    sys.exit(0)
sys.exit(2)
"""


def pcap_to_hc22000_alt(pcap: str, out_path: str | None = None) -> dict:
    """ALT: exakte Logik der ungepatchten deck/wpa_crack.py::pcap_to_hc22000()."""
    if not shutil.which("hcxpcapngtool"):
        return {"error": "hcxpcapngtool nicht installiert", "networks": [], "count": 0}
    if not os.path.isfile(pcap):
        return {"error": f"Datei nicht gefunden: {pcap}", "networks": [], "count": 0}
    out_path = out_path or (os.path.splitext(pcap)[0] + ".hc22000")
    try:
        subprocess.run(["hcxpcapngtool", "-o", out_path, pcap],
                       capture_output=True, text=True, timeout=120)
    except Exception as e:  # noqa: BLE001
        return {"error": f"hcxpcapngtool-Fehler: {e}", "networks": [], "count": 0}
    if not os.path.isfile(out_path) or os.path.getsize(out_path) == 0:
        return {"error": "keine Handshakes/PMKID im .pcap gefunden", "networks": [],
                "count": 0, "hc22000": out_path}
    nets = wc.parse_hc22000(open(out_path, encoding="utf-8", errors="replace").read())
    return {"hc22000": out_path, "networks": nets, "count": len(nets), "error": ""}


def pcap_to_hc22000_neu(pcap: str, out_path: str | None = None) -> dict:
    """NEU: exakte Logik der gepatchten deck/wpa_crack.py::pcap_to_hc22000()."""
    if not shutil.which("hcxpcapngtool"):
        return {"error": "hcxpcapngtool nicht installiert", "networks": [], "count": 0}
    if not os.path.isfile(pcap):
        return {"error": f"Datei nicht gefunden: {pcap}", "networks": [], "count": 0}
    out_path = out_path or (os.path.splitext(pcap)[0] + ".hc22000")
    try:
        if os.path.isfile(out_path):
            os.remove(out_path)
    except OSError as e:
        return {"error": f"alte {out_path} laesst sich nicht entfernen: {e}", "networks": [], "count": 0}
    try:
        proc = subprocess.run(["hcxpcapngtool", "-o", out_path, pcap],
                              capture_output=True, text=True, timeout=120)
    except Exception as e:  # noqa: BLE001
        return {"error": f"hcxpcapngtool-Fehler: {e}", "networks": [], "count": 0}
    if proc.returncode != 0:
        return {"error": f"hcxpcapngtool-Fehler (rc={proc.returncode}): "
                          f"{(proc.stderr or proc.stdout or '').strip()}",
                "networks": [], "count": 0}
    if not os.path.isfile(out_path) or os.path.getsize(out_path) == 0:
        return {"error": "keine Handshakes/PMKID im .pcap gefunden", "networks": [],
                "count": 0, "hc22000": out_path}
    nets = wc.parse_hc22000(open(out_path, encoding="utf-8", errors="replace").read())
    return {"hc22000": out_path, "networks": nets, "count": len(nets), "error": ""}


OLD_LINE = "WPA*02*aaaa*a1b2c3d4e5f6*001122334455*4f4c445f4e4554*0000"   # OLD_NET
NEW_LINE = "WPA*02*bbbb*665544332211*aabbccddeeff*4e45575f4e4554*0001"  # NEW_NET


def main():
    tmp = tempfile.mkdtemp(prefix="wpa_crack_repro_")
    tooldir = os.path.join(tmp, "bin")
    os.makedirs(tooldir)
    tool_path = os.path.join(tooldir, "hcxpcapngtool")
    with open(tool_path, "w") as f:
        f.write(FAKE_TOOL)
    os.chmod(tool_path, 0o755)
    os.environ["PATH"] = tooldir + os.pathsep + os.environ["PATH"]

    # Szenario A: zwei aufeinanderfolgende echte Erfolgslaeufe auf denselben Default-
    # out_path (z.B. zwei "capture"-Sitzungen mit demselben Standardnamen handshake.pcap).
    # Erster Lauf findet OLD_NET, zweiter -- andere Luftaufnahme! -- findet NEW_NET.
    pcap1 = os.path.join(tmp, "handshake.pcap")
    open(pcap1, "wb").write(b"\x00" * 10)

    os.environ["FAKE_HCX_MODE"] = "ok_truncate"
    os.environ["FAKE_HCX_LINE"] = OLD_LINE
    r1_alt = pcap_to_hc22000_alt(pcap1)
    check("Lauf 1 (ALT) findet nur OLD_NET", r1_alt["count"] == 1 and r1_alt["networks"][0]["ssid"] == "OLD_NET")

    os.environ["FAKE_HCX_MODE"] = "ok_append"  # echtes hcxpcapngtool-Verhalten: haengt an
    os.environ["FAKE_HCX_LINE"] = NEW_LINE
    r2_alt = pcap_to_hc22000_alt(pcap1)
    check("Fund A (ALT): Lauf 2 enthaelt noch OLD_NET aus Lauf 1 vermischt",
          r2_alt["count"] == 2 and any(n["ssid"] == "OLD_NET" for n in r2_alt["networks"]))

    # NEU: dieselbe Lauf-1/Lauf-2-Sequenz, aber mit vorab bereinigtem out_path.
    pcap2 = os.path.join(tmp, "handshake2.pcap")
    open(pcap2, "wb").write(b"\x00" * 10)
    out2 = os.path.join(tmp, "handshake2.hc22000")

    os.environ["FAKE_HCX_MODE"] = "ok_truncate"
    os.environ["FAKE_HCX_LINE"] = OLD_LINE
    r1_neu = pcap_to_hc22000_neu(pcap2, out_path=out2)
    check("Lauf 1 (NEU) findet nur OLD_NET", r1_neu["count"] == 1)

    os.environ["FAKE_HCX_MODE"] = "ok_append"
    os.environ["FAKE_HCX_LINE"] = NEW_LINE
    r2_neu = pcap_to_hc22000_neu(pcap2, out_path=out2)
    check("Fix: Lauf 2 (NEU) enthaelt NUR NEW_NET, kein OLD_NET mehr",
          r2_neu["count"] == 1 and r2_neu["networks"][0]["ssid"] == "NEW_NET")

    # Szenario B: Tool-Fehlschlag (z.B. kaputtes/unlesbares .pcap) VOR Oeffnen der
    # Ausgabedatei, wobei aus einem FRUEHEREN, unabhaengigen Lauf noch eine alte,
    # nicht-leere out_path-Datei am selben Pfad herumliegt.
    pcap3 = os.path.join(tmp, "broken.pcap")
    open(pcap3, "wb").write(b"\x00" * 10)
    out3 = os.path.join(tmp, "broken.hc22000")
    with open(out3, "w") as f:  # Alt-Stand von einer FRUEHEREN, unabhaengigen Sitzung
        f.write(OLD_LINE + "\n")

    os.environ["FAKE_HCX_MODE"] = "fail_before_open"  # Tool bricht ab, bevor es out_path oeffnet
    r_alt_fail = pcap_to_hc22000_alt(pcap3, out_path=out3)
    check("Fund B (ALT): Tool-Fehlschlag wird als Erfolg mit dem ALTEN Fund gemeldet (kein error)",
          r_alt_fail.get("error") == "" and r_alt_fail["count"] == 1)

    with open(out3, "w") as f:  # Alt-Stand erneut ablegen fuer den NEU-Lauf
        f.write(OLD_LINE + "\n")
    r_neu_fail = pcap_to_hc22000_neu(pcap3, out_path=out3)
    check("Fix: Tool-Fehlschlag (NEU) liefert einen echten Fehler, count=0, kein Alt-Fund",
          r_neu_fail["count"] == 0 and r_neu_fail["error"] != "" and not os.path.isfile(out3))

    # Regressionsfall: ein einziger, echter Erfolgslauf ohne jede Vorgeschichte --
    # ALT und NEU muessen identisch das eine, echte Netz melden.
    pcap4 = os.path.join(tmp, "fresh.pcap")
    open(pcap4, "wb").write(b"\x00" * 10)
    os.environ["FAKE_HCX_MODE"] = "ok_truncate"
    os.environ["FAKE_HCX_LINE"] = NEW_LINE
    r_alt_fresh = pcap_to_hc22000_alt(pcap4)
    r_neu_fresh = pcap_to_hc22000_neu(pcap4, out_path=os.path.splitext(pcap4)[0] + "-neu.hc22000")
    check("Regression: einzelner frischer Lauf liefert bei ALT und NEU dasselbe Ergebnis",
          r_alt_fresh["count"] == r_neu_fresh["count"] == 1
          and r_alt_fresh["networks"][0]["ssid"] == r_neu_fresh["networks"][0]["ssid"] == "NEW_NET")

    shutil.rmtree(tmp, ignore_errors=True)

    print(f"\n{'ALL CHECKS PASSED' if not failures else 'FAILURES: ' + ', '.join(failures)} "
          f"({checks} total)")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
