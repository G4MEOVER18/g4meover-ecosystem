# RollLab — beansprucht fremd bereits eingeschaltete OTG-5V-Versorgung fälschlich als eigenes Eigentum

## Was
Ich habe die standalone `RollLab`-App (eigenes Repo, nicht zu verwechseln mit dem
gleichnamigen Modul innerhalb von `RollForge`) auf denselben OTG-Ownership-Fund
geprüft, den ich zuvor in `RollForge/rollforge_rf.c` gefunden und behoben habe
(`otg-power-false-ownership-claim-fix.md`). `rolllab_rf.c` (eigenständiges Repo,
eigene Datei, kein gemeinsamer Code mit `RollForge`) enthält exakt dieselbe
Ownership-Buchhaltung für das externe CC1101-Modul — 1:1 dasselbe Muster, nur
`rf_power_on/off` → `rl_power_on/off` umbenannt:

```c
static bool s_otg_by_app = false;

static void rl_power_on(void) {
    uint8_t attempts = 0;
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }
    if(furi_hal_power_is_otg_enabled()) s_otg_by_app = true;
}

static void rl_power_off(void) {
    if(s_otg_by_app && furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
        s_otg_by_app = false;
    }
}
```

`rl_power_on()` setzt `s_otg_by_app = true` allein danach, **ob** die
OTG-Versorgung am Ende der Funktion an ist — nicht danach, **ob ich selbst**
sie eingeschaltet habe. War die 5V-Schiene bereits vorher an, überspringt die
`while`-Schleife jeden `furi_hal_power_enable_otg()`-Aufruf (die
Abbruchbedingung `!furi_hal_power_is_otg_enabled()` ist von Anfang an falsch),
trotzdem beansprucht die abschließende Prüfung fälschlich das Eigentum.

`furi_hal_power_enable_otg()`/`_disable_otg()`/`_is_otg_enabled()`
(`targets/f7/furi_hal/furi_hal_power.c`, gegen den eigenen `mntm-012`-Stand
verifiziert, identisch zum bereits bei der RollForge-Sitzung geprüften Stand)
sind reine Wrapper um den BQ25896-Laderegler-Chip — **ein einziger globaler
Hardwarezustand**, nicht app-eigen. Genau dieselbe Schiene schaltet auch das
Power-Menü ("5V on GPIO") sowie jede andere Erweiterungsplatinen-App
(Marauder/GPS/UART-Bridges/RollForge etc.).

`rlab_rf_init()` ruft `rl_power_on()` **unbedingt** bei jedem App-Start auf,
unabhängig davon, ob überhaupt ein externes CC1101 angeschlossen ist:

```c
rl_power_on();
const SubGhzDevice* ext_dev = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
if(ext_dev && subghz_devices_is_connect(ext_dev)) {
    subghz_devices_begin(ext_dev);
    app->device = ext_dev;
} else {
    rl_power_off();          // <-- greift SOFORT beim App-Start
    app->device = int_dev;
}
```

Konkretes Szenario: Ich habe "5V on GPIO" im Power-Menü dauerhaft aktiviert
(z. B. für ein permanent angeschlossenes GPS- oder UART-Board), starte
RollLab **ohne** ein externes CC1101-Modul angeschlossen zu haben.
`rl_power_on()` sieht die Schiene bereits an, beansprucht fälschlich
Eigentum; da kein EXT-Board antwortet, läuft sofort der `else`-Zweig und
`rl_power_off()` schaltet die von mir manuell im Power-Menü eingeschaltete
5V-Versorgung **augenblicklich beim bloßen Starten von RollLab** wieder ab —
noch bevor ich irgendeine RollLab-Funktion benutzt habe, ohne jede
Fehlermeldung. Dasselbe passiert beim regulären App-Ende (`rlab_rf_deinit()`
→ `rl_power_off()` unbedingt am Ende), falls das externe Board erst später
erkannt wurde: die App reißt beim Beenden eine 5V-Versorgung mit, die gar
nicht ihr gehörte.

Klarstellung zur bereits bei der RollForge-Sitzung getroffenen Aussage "die
RollForge/RollJam/RollLab/Jammer-Module ... sind von diesem Fix nicht
berührt": das bezog sich auf die **innerhalb von RollForge zusammengefassten
Module** (`module_rolllab.c` etc., die alle `rollforge_rf.c` mit-nutzen und
`app->device_ext` nach der Init-Phase nicht mehr anfassen) — nicht auf dieses
**eigenständige** `RollLab`-Repo mit eigener `rolllab_rf.c`. Beide Codebasen
sind unabhängig voneinander entstanden und teilen keinen Code; der Fund war
dort entsprechend nicht mit abgedeckt.

## Fix
Eigentum nur beanspruchen, wenn die Schiene beim Aufruf tatsächlich noch aus
war — identische Fix-Form wie bei `RollForge/rollforge_rf.c`:

```c
static void rl_power_on(void) {
    if(furi_hal_power_is_otg_enabled()) {
        // Schon von aussen an (Settings > Power "5V on GPIO" oder eine
        // andere App/ein anderes Modul) -- nicht meine Zustaendigkeit.
        return;
    }
    uint8_t attempts = 0;
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }
    if(furi_hal_power_is_otg_enabled()) s_otg_by_app = true;
}
```

`rl_power_off()` bleibt unverändert — die Ownership-Prüfung dort war bereits
korrekt, nur die Flag-Setzung in `rl_power_on()` war falsch.

## Betroffene Dateien
- `rolllab_rf.c` (`rl_power_on`) — einzige Änderung, vier neue Zeilen.

## Testhinweis (ohne Hardware)
Reine Zustands-/Ownership-Buchhaltung, host-nachstellbar ohne echten
BQ25896/CC1101-Treiber: `repro-otg-false-ownership/repro.c` (1:1 aus dem
bereits verifizierten RollForge-Repro übernommen, nur die Funktionsnamen
angepasst) mockt `furi_hal_power_is_otg_enabled/enable_otg/disable_otg` über
eine einzelne globale `bool` und prüft dieselben drei Fälle:

```
$ gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
$ ./repro
Szenario 1 (OTG vorher extern an, kein EXT gefunden):
  ALT: Ownership faelschlich beansprucht=1, OTG danach an=0
  NEU: Ownership beansprucht=0,            OTG danach an=1
Szenario 2 (OTG vorher aus, echtes EXT-Board):
  ALT: an nach power_on=1, aus nach deinit=0
  NEU: an nach power_on=1, aus nach deinit=0
Szenario 3 (OTG bleibt trotz Versuchen aus):
  ALT: Ownership beansprucht=0
  NEU: Ownership beansprucht=0

Alle Faelle wie erwartet.
```

- Szenario 1 (der Fund): ALT beansprucht fälschlich Eigentum und schaltet die
  fremd eingeschaltete Versorgung sofort wieder ab; NEU lässt sie unangetastet.
- Szenario 2 (Regression, Kernfunktion): ein echtes, neu angeschlossenes
  EXT-Board wird weiterhin korrekt mit Strom versorgt und beim Beenden
  wieder abgeschaltet — unverändert zu ALT.
- Szenario 3 (Regression, Fehlerfall): schlägt das Einschalten trotz aller
  Versuche fehl (Hardwarefehler), beansprucht weder ALT noch NEU Eigentum —
  unverändert.

Unified-Diff (`otg-power-false-ownership-claim-fix/rolllab_rf_c.patch`) gegen
die unveränderte `G4MEOVER18/RollLab`-Basis (`4ea80481`, main):
`git apply --check` vorab gegen eine frische Kopie verifiziert, real
angewendet und byte-für-byte gegen die separat bearbeitete Fassung abgeglichen
— identisch.

Funktionaler Test danach lokal auf dem Flipper: im Power-Menü "5V on GPIO"
manuell aktivieren, RollLab **ohne** angeschlossenes externes CC1101-Modul
starten und sofort wieder verlassen, danach im Power-Menü prüfen, ob "5V on
GPIO" weiterhin aktiv ist (vor dem Fix: aus; nach dem Fix: unverändert an).
Gegenprobe mit tatsächlich angeschlossenem externem CC1101-Modul: Verhalten
bleibt in beiden Fassungen identisch (Strom an bei Erkennung, ab beim
Beenden).

## Risiko
- Niedrig: die Änderung fügt nur eine frühzeitige Rückkehr hinzu, wenn die
  Versorgung bereits an ist — kein neuer Zustand, keine neue Abhängigkeit.
- Betrifft ausschließlich die Ownership-Buchhaltung für den externen
  CC1101-Pfad; der interne CC1101 (immer vorhanden, keine OTG-Versorgung
  nötig) ist von dieser Datei an keiner Stelle betroffen.
- `rlab_rf_deinit()` ruft `rl_power_off()` unbedingt am Ende auf — mit dem
  Fix korrekt sowohl für "nie beansprucht" (No-Op) als auch "selbst
  eingeschaltet" (schaltet ab), keine Verhaltensänderung an dieser Stelle.

## Zusätzlich geprüft, kein Fund
- `rlab_rf_init()`: `subghz_devices_begin(int_dev)` wird vor der
  Ext-Erkennung unbedingt aufgerufen; gegen `Next-Flip/Momentum-Firmware`
  (Tag `mntm-012`, `lib/subghz/devices/cc1101_int/cc1101_int_interconnect.c`)
  verifiziert: `.begin = NULL` für das interne Device, der Aufruf ist damit
  ein dokumentierter No-Op (kein Ressourcenleck, keine doppelte
  Initialisierung, wenn danach zusätzlich `ext_dev` begonnen wird).
- `rlab_app.c`-Hauptschleife (`RLAB_ADVANCE`-Zustand): der OK-Druck
  ("Done" → `RLAB_PROBE_READY`) ist gegen die laufende Capture-Restart-Logik
  bereits per `if(app->state == RLAB_ADVANCE && !app->abort)`-Prüfung
  abgesichert; der nachfolgende `RLAB_PROBE_READY`-Fall stoppt eine noch
  aktive Capture zusätzlich selbst ab (`if(app->rf_mode == RLAB_RF_CAPTURING)
  rlab_capture_work_stop(app);`) — kein Race wie bei den bereits behobenen
  RollForge-/RollJam-Funden gefunden.
- `rlab_analyze()`: die Silence-Terminator-Ausschluss-Logik aus meinem
  eigenen, früheren Fund (`analyze-silence-terminator-stats-skew-fix.md`) ist
  bereits im aktuellen Repo-Stand übernommen — nicht erneut gefunden, nur
  gegengeprüft.
- `rlab_replay_tx_cb()`/`rlab_rx_ref_cb()`/`rlab_rx_work_cb()`: Puffer-Bounds
  (`< RLAB_REF_BUF_SIZE`/`< RLAB_WORK_BUF_SIZE`) vor jedem Schreibzugriff
  einzeln geprüft, kein Overflow möglich.

## Quelle
- `G4MEOVER18/RollLab` (main, Commit `4ea80481`): `rolllab_rf.c`
  (`rl_power_on`, `rl_power_off`, `rlab_rf_init`, `rlab_rf_deinit`).
- Eigener vorheriger Fund in diesem Repo:
  `app-improvements/RollForge/otg-power-false-ownership-claim-fix.md`
  (identisches Muster in `rollforge_rf.c`, dort bereits behoben — dieser
  Fund überträgt denselben Fix auf das unabhängige `RollLab`-Repo).
- `Next-Flip/Momentum-Firmware` (Tag `mntm-012`):
  `targets/f7/furi_hal/furi_hal_power.c` (`furi_hal_power_enable_otg`,
  `_disable_otg`, `_is_otg_enabled` — BQ25896-Wrapper, ein globaler
  Hardwarezustand), `lib/subghz/devices/cc1101_int/cc1101_int_interconnect.c`
  (`.begin = NULL`-Bestätigung für den zusätzlich geprüften No-Op-Punkt).
