# RollForge — beansprucht fremd bereits eingeschaltete OTG-5V-Versorgung fälschlich als eigenes Eigentum

## Was
Ich habe `rollforge_rf.c` (bislang aus der Lifecycle-/Ext-Int-Umschaltungs-Gruppe
nur überflogen, nicht Zeile für Zeile geprüft) jetzt vollständig gelesen,
zusammen mit `rollforge_app.c` und `rollforge_storage.c` aus derselben Gruppe.
Fund liegt in der OTG-Stromversorgungs-Buchhaltung für das externe
CC1101-Modul:

```c
static bool s_otg_by_app = false;

static void rf_power_on(void) {
    uint8_t attempts = 0;
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }
    if(furi_hal_power_is_otg_enabled()) s_otg_by_app = true;
}

static void rf_power_off(void) {
    if(s_otg_by_app && furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
        s_otg_by_app = false;
    }
}
```

`rf_power_on()` setzt `s_otg_by_app = true` allein danach, **ob** die
OTG-Versorgung am Ende der Funktion an ist — nicht danach, **ob ich selbst**
sie eingeschaltet habe. War die 5V-Schiene bereits vorher an, überspringt die
`while`-Schleife jeden `furi_hal_power_enable_otg()`-Aufruf (die
Abbruchbedingung `!furi_hal_power_is_otg_enabled()` ist von Anfang an falsch),
trotzdem beansprucht die abschließende Prüfung fälschlich das Eigentum.

`furi_hal_power_enable_otg()`/`_disable_otg()`/`_is_otg_enabled()`
(`targets/f7/furi_hal/furi_hal_power.c`, gegen den eigenen `mntm-012`-Stand
verifiziert) sind reine Wrapper um den BQ25896-Laderegler-Chip — **ein
einziger globaler Hardwarezustand**, nicht app-eigen. Genau dieselbe Schiene
schaltet auch das Power-Menü ("5V on GPIO") sowie jede andere
Erweiterungsplatinen-App (Marauder/GPS/UART-Bridges etc.) — ein verbreitetes
und in eigenen Sitzungen bereits einmal in der Gegenrichtung gefundenes Muster
(`firmware-mining`-Nachbarfund in `FlipperJammer`:
`hw-detect-otg-leak-after-exit-fix.md`, dort das *Gegenteil* — OTG bleibt nach
Programmende fälschlich an).

`rollforge_rf_init()` ruft `rf_power_on()` **unbedingt** bei jedem App-Start
auf, unabhängig davon, ob überhaupt ein externes CC1101 angeschlossen ist:

```c
rf_power_on();
const SubGhzDevice* ext = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
if(ext && subghz_devices_is_connect(ext)) {
    ...
} else {
    rf_power_off();          // <-- greift SOFORT beim App-Start
    app->device_ext = NULL;
    ...
}
```

Konkretes Szenario: Ich habe "5V on GPIO" im Power-Menü dauerhaft aktiviert
(z. B. für ein permanent angeschlossenes GPS- oder UART-Board), starte
RollForge **ohne** ein externes CC1101-Modul angeschlossen zu haben.
`rf_power_on()` sieht die Schiene bereits an, beansprucht fälschlich
Eigentum; da kein EXT-Board antwortet, läuft sofort der `else`-Zweig und
`rf_power_off()` schaltet die von mir manuell im Power-Menü eingeschaltete
5V-Versorgung **augenblicklich beim bloßen Starten von RollForge** wieder ab
— noch bevor ich irgendeine RollForge-Funktion benutzt habe, ohne jede
Fehlermeldung. Dasselbe passiert beim regulären App-Ende
(`rollforge_rf_deinit()` → `rf_power_off()`), falls das externe Board erst
später erkannt wurde: die App reißt beim Beenden eine 5V-Versorgung mit, die
gar nicht ihr gehörte.

## Fix
Eigentum nur beanspruchen, wenn die Schiene beim Aufruf tatsächlich noch aus
war:

```c
static void rf_power_on(void) {
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

`rf_power_off()` bleibt unverändert — die Ownership-Prüfung dort war bereits
korrekt, nur die Flag-Setzung in `rf_power_on()` war falsch.

## Testhinweis (ohne Hardware)
Reine Zustands-/Ownership-Buchhaltung, host-nachstellbar ohne echten
BQ25896/CC1101-Treiber: `repro-otg-false-ownership/repro.c` mockt
`furi_hal_power_is_otg_enabled/enable_otg/disable_otg` über eine einzelne
globale `bool` (bildet exakt die reale Eigenschaft nach, dass es sich um
einen einzigen geteilten Hardwarezustand handelt) und prüft drei Fälle:

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

Funktionaler Test danach lokal auf dem Flipper: im Power-Menü "5V on GPIO"
manuell aktivieren, RollForge **ohne** angeschlossenes externes CC1101-Modul
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
- Deckt sich thematisch, aber nicht überlappend, mit dem bereits gefundenen
  FlipperJammer-OTG-Leck (dort: eigene Versorgung wird nach Programmende
  fälschlich NICHT abgeschaltet; hier: fremde Versorgung wird fälschlich
  abgeschaltet) — beide Richtungen sind jetzt für die eigenen Apps geprüft,
  die RollForge/RollJam/RollLab/Jammer-Module selbst fassen `app->device_ext`
  nach der Init-Phase nicht mehr an und sind von diesem Fix nicht berührt.

## Zusätzlich geprüft, kein Fund
- `rollforge_rf_switch()`: OTG-Versorgung wird beim Umschalten INT↔EXT
  bewusst nicht angefasst (bleibt für die gesamte App-Laufzeit wie beim Init
  gesetzt) — korrekt, da `device_ext` nur existiert, wenn die Versorgung
  bereits beim Start stand.
- `rollforge_rf_deinit()`: ruft `rf_power_off()` unbedingt am Ende auf,
  unabhängig von `dev_is_ext` — mit dem Fix korrekt sowohl für "nie
  beansprucht" (No-Op) als auch "selbst eingeschaltet" (schaltet ab).
- `rollforge_save_sig()` (`rollforge_storage.c`): Grenzen/Vorzeichenumwandlung
  der RAW-Puffer-Werte einzeln nachgerechnet (`packed & 0x7FFFFFFF` als
  `int32_t`, Negation bei `!level`), kein Overflow möglich (Maximalwert
  `INT32_MAX`, keine `INT32_MIN`-Negation erreichbar).
- `rollforge_app.c`-Hauptschleife: der Geräte-Guard
  (`if(!app->device && app->active != MOD_MENU) mod_leave(app);`) verhindert
  den Eintritt in ein Modul beim Fehlen von CC1101 nicht sofort, sondern erst
  eine Schleifeniteration später — alle drei `_enter()`-Funktionen
  (`rj_enter`/`rl_enter`/`jm_enter`, gegengeprüft) fassen dabei aber
  ausschließlich Status-/Phasenfelder an, kein Hardwarezugriff; auf echter
  Hardware ist der interne CC1101 zudem immer vorhanden, wodurch
  `app->device` in der Praxis nie `NULL` wird — rein theoretischer Pfad ohne
  Absturz- oder Datenverlustrisiko, kein eigener Fix-Vorschlag.

## Quelle
- `G4MEOVER18/RollForge` (main): `rollforge_rf.c` (`rf_power_on`,
  `rf_power_off`, `rollforge_rf_init`, `rollforge_rf_deinit`).
- `Next-Flip/Momentum-Firmware` (Tag `mntm-012`):
  `targets/f7/furi_hal/furi_hal_power.c` (`furi_hal_power_enable_otg`,
  `_disable_otg`, `_is_otg_enabled` — BQ25896-Wrapper, ein globaler
  Hardwarezustand, gegen den eigenen Header-Stand verifiziert).
