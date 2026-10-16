#!/usr/bin/env python3
"""Host-Repro: wpa_crack.show_cracked() ALT (vor dem Fix) vs. NEU (nach dem Fix).

Transkribiert ausschliesslich die fuer den Fund relevante Schleife aus
show_cracked() 1:1 als eigenstaendige Funktionen (_parse_show_ALT/_parse_show_NEU),
ohne hashcat/hcxpcapngtool aufzurufen -- die Eingabe ist der Text, den
`hashcat -m 22000 <hc22000> --show` auf stdout liefert (eine Zeile je bereits
geknacktem Netz: der vollstaendige hc22000-Hash, gefolgt von ':', gefolgt vom
Klartextpasswort). Die restlichen Funktionen (_have/_fmt_mac) werden unveraendert
aus dem Original importiert, damit kein Verhalten doppelt gepflegt wird.
"""
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wpa_crack_orig import _fmt_mac  # unveraendert, identisch in ALT und NEU


def _parse_show_ALT(out: str) -> list[dict]:
    """1:1-Transkription der show_cracked()-Schleife vor dem Fix."""
    results = []
    for line in out.splitlines():
        f = line.strip().split("*")
        if len(f) >= 6 and f[0] == "WPA":
            pw = line.rsplit(":", 1)[-1] if ":" in line else ""
            try:
                ssid = bytes.fromhex(f[5]).decode("utf-8", "replace")
            except ValueError:
                ssid = f[5]
            results.append({"ssid": ssid, "bssid": _fmt_mac(f[3]), "password": pw})
    return results


def _parse_show_NEU(out: str) -> list[dict]:
    """1:1-Transkription der show_cracked()-Schleife nach dem Fix."""
    results = []
    for line in out.splitlines():
        f = line.strip().split("*")
        if len(f) >= 6 and f[0] == "WPA":
            pw = line.split(":", 1)[1] if ":" in line else ""
            try:
                ssid = bytes.fromhex(f[5]).decode("utf-8", "replace")
            except ValueError:
                ssid = f[5]
            results.append({"ssid": ssid, "bssid": _fmt_mac(f[3]), "password": pw})
    return results


# Ein echtes hc22000-WPA2-EAPOL-Hash-Praefix (Feldaufbau wie von hcxpcapngtool
# erzeugt: WPA*02*hash*mac_ap*mac_sta*essid_hex*...), gefolgt von ':' + Klartext --
# exakt das Format, das `hashcat -m 22000 ... --show` auf stdout schreibt.
HASH_PREFIX = (
    "WPA*02*366b2659a413620b038339bc609d9700*a1b2c3d4e5f6*001122334455*"
    "4c696e6b737973*0103007502010a00000000000000000000018fa31ad403a627d9"
    "f4f68829dbc3fe100000000000000000000000000000000"
)


def _line(password: str) -> str:
    return f"{HASH_PREFIX}:{password}"


def main() -> int:
    checks = 0
    failed = 0

    def check(name: str, cond: bool, detail: str = "") -> None:
        nonlocal checks, failed
        checks += 1
        if cond:
            print(f"[OK] {name}")
        else:
            failed += 1
            print(f"[FAIL] {name} {detail}")

    # (1) Regression: Passwort ohne ':' -- ALT und NEU liefern identisch das
    #     volle, richtige Passwort.
    simple = _line("Sommer2026")
    alt = _parse_show_ALT(simple)
    neu = _parse_show_NEU(simple)
    check("Regression ALT: Passwort ohne ':' korrekt", alt[0]["password"] == "Sommer2026",
          f"-> {alt}")
    check("Regression NEU: Passwort ohne ':' korrekt", neu[0]["password"] == "Sommer2026",
          f"-> {neu}")

    # (2) Fund: Passwort MIT einem ':' -- ALT trunkiert, NEU liefert das volle
    #     Passwort.
    colon_pw = "pass:word"
    one_colon = _line(colon_pw)
    alt = _parse_show_ALT(one_colon)
    neu = _parse_show_NEU(one_colon)
    check("Fund ALT: Passwort mit einem ':' wird trunkiert (Bug reproduziert)",
          alt[0]["password"] == "word", f"-> erhalten: {alt[0]['password']!r}")
    check("Fix NEU: Passwort mit einem ':' bleibt vollstaendig",
          neu[0]["password"] == colon_pw, f"-> erhalten: {neu[0]['password']!r}")

    # (3) Fund verschaerft: Passwort mit MEHREREN ':' -- ALT verwirft noch mehr
    #     vom echten Passwort, NEU bleibt korrekt.
    multi_colon_pw = "a:b:c:super:geheim"
    multi = _line(multi_colon_pw)
    alt = _parse_show_ALT(multi)
    neu = _parse_show_NEU(multi)
    check("Fund ALT: Passwort mit mehreren ':' stark trunkiert",
          alt[0]["password"] == "geheim", f"-> erhalten: {alt[0]['password']!r}")
    check("Fix NEU: Passwort mit mehreren ':' vollstaendig",
          neu[0]["password"] == multi_colon_pw, f"-> erhalten: {neu[0]['password']!r}")

    # (4) Mehrzeilige --show-Ausgabe (zwei bereits geknackte Netze, eines mit
    #     ':' im Passwort) -- beide Zeilen muessen unabhaengig korrekt
    #     zugeordnet werden.
    multi_line_out = "\n".join([_line("einfach123"), _line("Wi:Fi:2026!")])
    neu = _parse_show_NEU(multi_line_out)
    check("Fix NEU: zwei Zeilen, nur zweite mit ':' im Passwort -- beide korrekt",
          len(neu) == 2 and neu[0]["password"] == "einfach123"
          and neu[1]["password"] == "Wi:Fi:2026!", f"-> {neu}")

    # (5) ssid/bssid-Extraktion unveraendert durch den Fix (nur die
    #     Passwort-Spalte ist betroffen) -- bssid/ssid muessen fuer ALT und NEU
    #     bei jeder Zeile identisch bleiben.
    for pw in ("Sommer2026", colon_pw, multi_colon_pw):
        ln = _line(pw)
        a = _parse_show_ALT(ln)[0]
        n = _parse_show_NEU(ln)[0]
        check(f"ssid/bssid unveraendert fuer Passwort {pw!r}",
              a["ssid"] == n["ssid"] == "Linksys" and a["bssid"] == n["bssid"] == "A1:B2:C3:D4:E5:F6",
              f"-> ALT={a}, NEU={n}")

    # (6) Zeile ohne ':' (noch nicht geknackt, sollte in der Praxis von hashcat
    #     --show gar nicht ausgegeben werden) -- beide liefern leeres Passwort,
    #     kein Crash.
    no_colon = HASH_PREFIX  # kein ':' am Ende
    alt = _parse_show_ALT(no_colon)
    neu = _parse_show_NEU(no_colon)
    check("Randfall ohne ':': ALT liefert leeres Passwort statt Crash",
          alt[0]["password"] == "", f"-> {alt}")
    check("Randfall ohne ':': NEU liefert leeres Passwort statt Crash",
          neu[0]["password"] == "", f"-> {neu}")

    # Differenzielle Gegenprobe: eine mutierte NEU-Variante (zurueck auf rsplit)
    # muss die Faelle (2)/(3) wieder reproduzieren -- Repro erkennt das.
    def _parse_show_MUTANT(out: str) -> list[dict]:
        results = []
        for line in out.splitlines():
            f = line.strip().split("*")
            if len(f) >= 6 and f[0] == "WPA":
                pw = line.rsplit(":", 1)[-1] if ":" in line else ""
                results.append({"password": pw})
        return results

    mutant = _parse_show_MUTANT(one_colon)
    check("Mutationstest: mutierte NEU (rsplit) reproduziert den Bug wieder (Repro ist scharf)",
          mutant[0]["password"] == "word", f"-> {mutant}")

    print(f"{checks - failed}/{checks} Einzelprüfungen bestanden -- "
          f"{'ALLE CHECKS BESTANDEN' if failed == 0 else 'FEHLER AUFGETRETEN'}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
