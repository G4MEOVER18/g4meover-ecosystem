#!/usr/bin/env python3
"""Host-Repro: lorix_link.device_fleet() vor/nach dem Pagination-Fix.

Transkribiert die betroffene Logik aus deck/lorix_link.py (ALT = vor dem Fix,
NEU = nach dem Fix) 1:1 als eigenstaendige Funktionen. Ein MockTTN-Server bildet
nur das von device_fleet() tatsaechlich genutzte Verhalten der TTN-HTTP-API nach
(seitenbasierte Pagination: limit/page-Query-Parameter, X-Total-Count-Header,
Session-Lookup je Geraet) -- ohne echten TTN-Zugang/Netzwerk. Reine Logik, kein
Hardware-/Netzwerkzugriff noetig.
"""
from __future__ import annotations
from urllib.parse import urlparse, parse_qs

APP = "dogytag-v1"


class MockTTN:
    """Simuliert /api/v3/applications/{APP}/devices (paginiert) und
    /api/v3/ns/applications/{APP}/devices/{id} (Session-Lookup je Geraet)."""

    def __init__(self, fleet_size: int, default_limit: int = 20, joined_every: int = 2,
                 send_total_count: bool = True, server_cap: int | None = None):
        self.devices = [
            {"ids": {"device_id": f"tag-{i:03d}", "dev_eui": f"{i:016X}"}}
            for i in range(fleet_size)
        ]
        self.default_limit = default_limit
        self.joined_every = joined_every
        self.send_total_count = send_total_count
        self.server_cap = server_cap  # Server begrenzt jede Seite zusaetzlich hart
        self.calls = 0

    def get(self, path: str):
        """Wie _get() vor dem Fix: (status, body) -- kein Total-Count-Header."""
        st, body, _ = self.get_page(path)
        return st, body

    def get_page(self, path: str):
        """Wie _get_page() nach dem Fix: (status, body, total)."""
        self.calls += 1
        u = urlparse(path)
        qs = parse_qs(u.query)
        if u.path == f"/api/v3/applications/{APP}/devices":
            limit = int(qs.get("limit", [self.default_limit])[0])
            if self.server_cap is not None:
                limit = min(limit, self.server_cap)
            page = max(int(qs.get("page", ["1"])[0]), 1)  # TTS: 0 == erste Seite
            start = (page - 1) * limit
            batch = self.devices[start:start + limit]
            body = {"end_devices": batch}
            total = len(self.devices) if self.send_total_count else None
            return 200, body, total
        if u.path.startswith(f"/api/v3/ns/applications/{APP}/devices/"):
            dev_id = u.path.rsplit("/", 1)[-1]
            idx = int(dev_id.split("-")[1])
            if idx % self.joined_every == 0:
                return 200, {"session": {"dev_addr": f"26{idx:06X}"}}, None
            return 200, {}, None
        return 404, "not found", None


def _session_lookup(srv: MockTTN, end_devices: list[dict]) -> list[dict]:
    out = []
    for d in end_devices:
        dev_id = d.get("ids", {}).get("device_id")
        sst, sbody = srv.get(
            f"/api/v3/ns/applications/{APP}/devices/{dev_id}?field_mask=session.dev_addr")
        dev_addr = None
        if sst == 200 and isinstance(sbody, dict):
            dev_addr = sbody.get("session", {}).get("dev_addr")
        out.append({"id": dev_id, "dev_eui": d.get("ids", {}).get("dev_eui"),
                    "joined": bool(dev_addr), "dev_addr": dev_addr})
    return out


def device_fleet_alt(srv: MockTTN) -> dict:
    """ALT (vor dem Fix): eine einzige, unparametrisierte Anfrage ohne limit/page."""
    st, body = srv.get(f"/api/v3/applications/{APP}/devices?field_mask=ids")
    if st != 200 or not isinstance(body, dict):
        return {"devices": [], "count": 0, "error": f"TTN {st}: {str(body)[:100]}"}
    return {"devices": _session_lookup(srv, body.get("end_devices", [])),
            "count": len(body.get("end_devices", [])), "error": ""}


def device_fleet_neu(srv: MockTTN, limit: int = 50) -> dict:
    """NEU (nach dem Fix): Schleife ueber alle Seiten mit limit/page + X-Total-Count."""
    end_devices: list[dict] = []
    page = 1
    while True:
        st, body, total = srv.get_page(
            f"/api/v3/applications/{APP}/devices?field_mask=ids&limit={limit}&page={page}")
        if st != 200 or not isinstance(body, dict):
            return {"devices": [], "count": 0, "error": f"TTN {st}: {str(body)[:100]}"}
        batch = body.get("end_devices", [])
        end_devices.extend(batch)
        if not batch:
            break
        if total is not None:
            if len(end_devices) >= total:
                break
        elif len(batch) < limit:
            break
        page += 1
    out = _session_lookup(srv, end_devices)
    return {"devices": out, "count": len(out), "error": ""}


def main() -> int:
    ok_count = 0
    total_count = 0

    def check(label: str, cond: bool, detail: str = "") -> None:
        nonlocal ok_count, total_count
        total_count += 1
        status = "OK" if cond else "FAIL"
        if cond:
            ok_count += 1
        print(f"[{status}] {label}{(' -- ' + detail) if detail else ''}")

    # 1) Regression: Flotte passt in eine Standard-Seite -- ALT und NEU identisch
    srv = MockTTN(fleet_size=3, default_limit=20)
    alt = device_fleet_alt(srv)
    srv2 = MockTTN(fleet_size=3, default_limit=20)
    neu = device_fleet_neu(srv2)
    check("Regression ohne Fund: ALT==NEU bei kleiner Flotte (3 Geraete)",
          alt["count"] == 3 and neu["count"] == 3 and alt["devices"] == neu["devices"],
          f"ALT={alt['count']} NEU={neu['count']}")

    # 2) Der eigentliche Fund: Flotte > serverseitiger Standard-Seitengroesse (ohne
    #    explizites limit/page liefert die TTN-API nur die erste Seite zurueck)
    srv = MockTTN(fleet_size=47, default_limit=20)
    alt = device_fleet_alt(srv)
    check("Fund ALT: 47 registrierte Geraete, aber nur erste Seite gelesen",
          alt["count"] == 20, f"ALT meldet {alt['count']} von 47 echten Geraeten")

    srv2 = MockTTN(fleet_size=47, default_limit=20)
    neu = device_fleet_neu(srv2)
    check("Fix NEU: alle 47 registrierten Geraete vollstaendig gelesen",
          neu["count"] == 47, f"NEU meldet {neu['count']} von 47")

    # 3) Exakte Seitengrenze: Flotte ist ein ganzes Vielfaches der Seitengroesse --
    #    kein Off-by-one (keine ueberfluessige leere Folgeseite, kein fehlendes Geraet)
    srv = MockTTN(fleet_size=100, default_limit=20, send_total_count=True)
    neu = device_fleet_neu(srv, limit=50)
    check("Fix NEU: exaktes Vielfaches der Seitengroesse (100 Geraete, limit=50)",
          neu["count"] == 100 and srv.calls == 2 + 100,
          f"count={neu['count']} http_calls={srv.calls} (2 Device-Seiten + 100 Session-Lookups)")

    # 4) Server ohne X-Total-Count-Header: Fallback ueber Seitengroesse muss trotzdem
    #    korrekt bis zur letzten (kuerzeren) Seite weiterlesen
    srv = MockTTN(fleet_size=73, default_limit=20, send_total_count=False)
    neu = device_fleet_neu(srv, limit=25)
    check("Fix NEU ohne X-Total-Count-Header: Fallback ueber Seitengroesse",
          neu["count"] == 73, f"NEU meldet {neu['count']} von 73 (kein Total-Count-Header)")

    # 5) Server kappt jede Seite serverseitig unter das angeforderte limit, liefert
    #    aber weiterhin X-Total-Count -- die Schleife darf sich NICHT auf die
    #    Seitengroesse verlassen, sondern muss dem Total-Count folgen (sonst bricht
    #    sie faelschlich zu frueh ab, siehe Korrektur der ersten Fix-Fassung)
    srv = MockTTN(fleet_size=90, default_limit=20, server_cap=10, send_total_count=True)
    neu = device_fleet_neu(srv, limit=50)  # Client fordert 50 an, Server liefert hart max. 10
    check("Fix NEU bei serverseitig gekappter Seitengroesse (angefordert 50, Server liefert 10)",
          neu["count"] == 90, f"NEU meldet {neu['count']} von 90 trotz Server-Kappung auf 10")

    # 6) Leere Flotte -- kein Fehlschlag, kein Endlos-Loop
    srv = MockTTN(fleet_size=0, default_limit=20)
    neu = device_fleet_neu(srv)
    check("Fix NEU bei leerer Flotte: count=0, kein Endlos-Loop",
          neu["count"] == 0 and neu["error"] == "")

    # 7) Aequivalenz ohne Fund: fuer jede Flottengroesse bis zur Standard-Seitengroesse
    #    liefern ALT und NEU dasselbe Ergebnis (keine Verhaltensaenderung im Normalfall)
    equal = True
    for n in range(0, 21):
        a = device_fleet_alt(MockTTN(fleet_size=n, default_limit=20))
        b = device_fleet_neu(MockTTN(fleet_size=n, default_limit=20))
        if a["devices"] != b["devices"] or a["count"] != n or b["count"] != n:
            equal = False
            break
    check("Aequivalenz ALT==NEU fuer Flottengroesse 0..20 (<= Standard-Seite)", equal)

    print(f"{ok_count}/{total_count} Einzelprüfungen bestanden -- "
          + ("ALLE CHECKS BESTANDEN" if ok_count == total_count else "FEHLER VORHANDEN"))
    return 0 if ok_count == total_count else 1


if __name__ == "__main__":
    raise SystemExit(main())
