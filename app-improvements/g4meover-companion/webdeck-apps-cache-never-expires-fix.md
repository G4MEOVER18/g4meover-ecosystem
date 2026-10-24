# g4meover-companion / webdeck — `_apps_cache` wird nie neu befüllt

## Was

`webdeck/webdeck.py` ist der dependency-freie Browser-Dashboard-Server für den
Flipper Zero (startet dauerhaft via `python webdeck.py -p ... --host 0.0.0.0`
auf dem Pi-Deck, läuft typischerweise tage- bis wochenlang durch). Der
`/api/apps`-Endpunkt (App-Landschaft nach Kategorie) ist über `api_apps()`
implementiert:

```python
_apps_cache = None

def api_apps():
    global _apps_cache
    if _apps_cache is not None:
        return _apps_cache
    counts = {}
    with _lock:
        for cat in CATS:
            out = _bridge.storage_list(f"/ext/apps/{cat}")
            n = len(re.findall(r"\[F\].+\.fap", out))
            if n:
                counts[cat] = n
    _apps_cache = {"counts": counts, "total": sum(counts.values())}
    return _apps_cache
```

`_apps_cache` ist ein Modul-globales Feld, das beim allerersten `/api/apps`-
Aufruf einmal befüllt und danach **nie wieder invalidiert** wird — nicht nach
Ablauf einer Zeitspanne, nicht beim nächsten Browser-Reload, nicht beim
nächsten `setInterval`-Tick. Jeder weitere Aufruf während der gesamten
Laufzeit des `webdeck.py`-Prozesses liefert exakt dasselbe `dict` zurück, egal
wie viele Apps in der Zwischenzeit auf dem Flipper installiert, aktualisiert
oder entfernt wurden.

Das unterscheidet sich von der oberflächlich ähnlichen Stelle in
`cockpit/cockpit.py::live_monitor()`, wo `count_apps()` bewusst **einmal pro
`live`-Kommando-Aufruf** (ein kurzlebiger CLI-Prozess, der mit Ctrl-C beendet
wird) als teure Operation dokumentiert ist — dort ist "einmalig" tatsächlich
"einmal pro Sitzung". `webdeck.py` ist dagegen ein Dauerprozess; "einmalig"
bedeutet hier "einmal für immer, bis der Server neu gestartet wird".

## Nutzen des Fixes

Ein TTL (30 s) macht aus dem permanenten Cache einen kurzlebigen Cache: er
spart weiterhin wiederholte `storage list`-Serien-Roundtrips bei schnell
aufeinanderfolgenden `/api/apps`-Aufrufen (z. B. mehrere offene Browser-Tabs),
verhindert aber, dass die App-Landschaft für die gesamte Serverlaufzeit nach
der ersten Installation/Deinstallation einer App dauerhaft falsch angezeigt
wird. 30 s liegt in derselben Größenordnung wie das bestehende
`setInterval(refresh, 5000)` im Frontend und ist kurz genug, dass ein
Seitenreload nach einer eigenen Installation/Deinstallation in der Praxis
den aktuellen Stand zeigt, ohne bei jedem einzelnen Request erneut sechs
`storage list`-Roundtrips über die serielle Verbindung zu erzeugen.

## Betroffene Dateien

- `webdeck/webdeck.py`:
  - `import time` ergänzt.
  - Neue Modul-Felder `_apps_cache_ts = 0.0` und `APPS_CACHE_TTL_S = 30.0`
    neben dem bestehenden `_apps_cache = None`.
  - `api_apps()`: Cache-Treffer jetzt an `(time.monotonic() - _apps_cache_ts)
    < APPS_CACHE_TTL_S` gebunden statt an bloßes `_apps_cache is not None`;
    `_apps_cache_ts` wird beim Neu-Befüllen mitgesetzt.

Bewusst nicht angefasst: `api_status()`, `api_captures()`, `api_sync()` sowie
die identische, aber unabhängige `count_apps()`-Implementierung in
`cockpit/cockpit.py` (dort korrekt als bewusstes Einmal-pro-Sitzung
dokumentiert, kein Fund). Das Frontend (`PAGE`-Konstante, `loadApps()`) bleibt
unverändert — der Fix wirkt rein serverseitig; ein Browser-Reload reicht nach
Ablauf der TTL aus, um den aktuellen Stand zu sehen.

## Patch

`connective-patches/webdeck_py.patch` (git-Diff gegen `webdeck/webdeck.py`,
Commit `ceb068bf278feae4a8180d24330111e0b911301d` von
`G4MEOVER18/g4meover-companion`, `main`). Gegen zwei frische, unabhängige
Zweitkopien des Repos mit `git apply --check` verifiziert, real angewendet,
per `md5sum` byte-für-byte abgeglichen — identisch. Vollständige Zieldatei als
`webdeck_py.target` beigelegt. `python3 -m py_compile` auf die gepatchte Datei
lief ohne Fehler.

## Host-Repro

`repro-webdeck-apps-cache-ttl/repro.py`: transkribiert `api_apps()` 1:1 als
ALT-/NEU-Funktion über ein injizierbares `CacheState`-Objekt und eine
`MockBridge`, deren `storage_list()` die reale `[F] name NNNb`-Zeilenform des
Firmware-CLI-Outputs nachbildet (gegen `applications/services/storage/
storage_cli.c` in `G4MEOVER18/G4MEOVER-FW` verifiziert: `"\t[F] %s %lub\r\n"`
pro Datei). Kein Hardware-/Seriell-/`pyserial`-Zugriff nötig (reine
Modul-Logik, deshalb nicht über `import webdeck` getestet, sondern
transkribiert — `flipperbridge.py` importiert `pyserial`, das auf diesem Host
nicht installiert ist).

10 Prüfungen, `python3 repro.py`:
1. ALT: erster Aufruf liefert korrekten Stand (Total=5), ein `storage_list`
   pro Kategorie.
2. **Fund:** nach einer simulierten App-Installation (Tools 3→5 Apps, auch
   nach einer sehr langen simulierten Zeitspanne) liefert ALT weiterhin
   Total=5 — ohne jede erneute Geräteabfrage.
3. NEU: erster Aufruf liefert korrekten Stand.
4. NEU: kurz vor Ablauf der TTL weiterhin gecacht (Total=5, kein zusätzlicher
   `storage_list`-Aufruf) — Performance-Vorteil des Caches bleibt erhalten.
5. **Fix:** kurz nach Ablauf der TTL liefert NEU den aktuellen Stand
   (Total=7), mit genau einem erneuten Abfrage-Satz (10 Aufrufe = Anzahl
   Kategorien).
6. Regression: ein weiterer Refresh ohne Geräteänderung liefert weiterhin den
   unveränderten aktuellen Stand.

Ergebnis: **ALLE PRÜFUNGEN BESTANDEN**.

## Testhinweis (lokal)

Patch gegen die eigene `webdeck.py`-Kopie anwenden, `webdeck.py -p COM3`
starten, `/api/apps` im Browser/`curl` einmal abrufen (merkt sich den Stand),
dann auf dem Flipper eine App in einer der `CATS`-Kategorien (z. B.
`/ext/apps/Tools`) hinzufügen oder entfernen. Vor dem Fix bleibt `/api/apps`
bis zum Neustart des `webdeck.py`-Prozesses beim alten Stand; nach dem Fix
zeigt ein Abruf nach spätestens 30 s den aktualisierten Stand. Regressionsfall:
mehrere `/api/apps`-Abrufe innerhalb von 30 s ohne Geräteänderung erzeugen
weiterhin nur einen einzigen `storage list`-Durchlauf über die serielle
Verbindung (am seriellen Traffic beobachtbar, z. B. über ein USB-Sniff oder
einen Zähler in `FlipperBridge.cmd()`).

## Risiko

Rein additiv, keine bestehende Rückgabestruktur geändert. Einziges neues
Verhalten: ein zusätzlicher `storage_list`-Aufrufsatz alle 30 s statt eines
einzigen für die gesamte Prozesslaufzeit — bei einem Single-User-Dashboard
mit serialisiertem Zugriff (`_lock`) vernachlässigbare zusätzliche Last auf
der seriellen Verbindung.

## Quelle

`G4MEOVER18/g4meover-companion`, `main`, Commit
`ceb068bf278feae4a8180d24330111e0b911301d` (`webdeck/webdeck.py`).
Firmware-Referenz für das `storage list`-Zeilenformat:
`G4MEOVER18/G4MEOVER-FW`, `main`, Commit
`b359f6fc32f282ce7217dd2d48a4e3478771852e`
(`applications/services/storage/storage_cli.c`).
