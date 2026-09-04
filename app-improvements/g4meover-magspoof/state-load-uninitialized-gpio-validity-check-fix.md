# g4meover-magspoof — uninitialisierter GPIO-Validity-Check in `mag_state_load()`

## Was
Meine MagSpoof-App basiert auf `zacharyweiss/magspoof_flipper`. `mag_state_load()`
(`mag_state.c`) lädt die persistenten Einstellungen (GPIO-Pinbelegung, `allow_uart`,
`n_repeats`, `repeat_mode`) aus `MAG_STATE_PATH` in eine lokale Struktur:

```c
bool mag_state_load(MagState* out_state) {
    MagState state;

    bool loaded_from_file = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_file_exists(storage, MAG_STATE_PATH)) {
        FlipperFormat* file = flipper_format_file_alloc(storage);
        do {
            ...
            loaded_from_file = true;
        } while(0);
        flipper_format_free(file);
    }
    furi_record_close(RECORD_STORAGE);

    // If file's GPIO config is invalid (pins overlap)
    // Reset to defaults
    if(!mag_state_gpio_is_valid(&state)) {
        mag_state_gpio_reset(&state);
    }

    if(!loaded_from_file) {
        mag_state_gpio_reset(&state);
        state.allow_uart = MAG_STATE_DEFAULT_ALLOW_UART;
        state.n_repeats = MAG_STATE_DEFAULT_N_REPEATS;
        state.repeat_mode = MAG_STATE_DEFAULT_REPEAT_MODE;
    }

    memcpy(out_state, &state, sizeof(state));
    return loaded_from_file;
}
```

`state` ist eine lokale Struktur **ohne Initialisierer**. Sobald `storage_file_exists()` bei mir
`false` liefert (allererster App-Start auf einer frischen SD-Karte, oder nachdem ich
`MAG_STATE_PATH` gelöscht habe) — oder der `do{...}while(0)`-Block vor der ersten Feldzuweisung
abbricht (`flipper_format_file_open_existing()`/`flipper_format_read_header()` schlägt fehl,
z. B. bei einer beschädigten oder von einer älteren App-Version stammenden Zustandsdatei mit
falschem Header) — wird der komplette Block, der `state.pin_input`/`pin_output`/`pin_enable`
setzt, übersprungen. `mag_state_gpio_is_valid(&state)` liest die drei Felder direkt danach aber
**unbedingt**, bevor irgendeine Initialisierung stattgefunden hat — undefiniertes Verhalten
(Lesen unbestimmter Stack-Werte), unabhängig vom Dateizustand.

Ich habe das mit einer eigenständigen, hostseitigen Nachbildung von `mag_state_gpio_is_valid()`/
`mag_state_gpio_reset()` und einer vereinfachten `mag_state_load()` (Storage-/FlipperFormat-Calls
durch ein `file_exists`-Flag ersetzt, das exakt denselben Zweig wie ein fehlendes
`MAG_STATE_PATH` auslöst) unter Valgrind Memcheck bestätigt: drei
„Conditional jump or move depends on uninitialised value(s)"-Funde, alle in
`mag_state_gpio_is_valid()`, aufgerufen aus `mag_state_load_alt()` — siehe Testhinweis.

**Aktuelle Auswirkung eingegrenzt:** Im selben Zweig ist `loaded_from_file` immer noch `false`,
daher läuft direkt danach unbedingt der `if(!loaded_from_file) { mag_state_gpio_reset(&state);
... }`-Block und überschreibt `pin_input`/`pin_output`/`pin_enable` erneut mit den korrekten
Defaults — der an `out_state` zurückgegebene Endzustand ist in der aktuellen Funktionsstruktur
also in jedem von mir geprüften Fall korrekt. Das habe ich mit dem Host-Repro ebenfalls bestätigt
(identischer `pin_input`/`pin_output`/`pin_enable`-Endwert vor und nach dem Fix). Es bleibt aber
echtes undefiniertes Verhalten (von Valgrind als Fehler markiert, `--error-exitcode` schlägt an),
das nur deshalb folgenlos bleibt, weil zwei Zeilen weiter unten ohnehin ein vollständiger Reset
erzwungen wird — jede künftige Änderung an dieser Reihenfolge (z. B. die beiden `if`-Blöcke
vertauschen oder den zweiten Reset an eine engere Bedingung knüpfen) würde die aktuell
unbeobachtbare UB in einen echten Bug mit unbestimmter GPIO-Pinbelegung verwandeln.

## Nutzen
- Beseitigt einen von Valgrind bestätigten Zugriff auf uninitialisierten Speicher beim
  allerersten App-Start (frische SD-Karte) sowie bei jeder beschädigten/veralteten
  `mag_state.txt` — der häufigste bzw. einfachste Weg, diesen Codepfad überhaupt zu erreichen.
- Rein defensiv/robustheitsverbessernd: macht die Funktion unabhängig von der aktuellen
  zufälligen Reihenfolge der beiden Reset-Blöcke, statt sich implizit auf die zweite Prüfung zu
  verlassen.
- Ein-Zeilen-Fix, keine Verhaltensänderung im Erfolgsfall (Datei vorhanden und lesbar).

## Betroffene Dateien (lokal, in meiner MagSpoof-App)
- `mag_state.c` — `mag_state_load()`: die Deklaration `MagState state;`.

Keine Änderung an `mag_state.h`, `mag_state_gpio_is_valid()`, `mag_state_gpio_reset()` oder am
`.txt`-Zustandsdateiformat nötig.

## Patch
```c
bool mag_state_load(MagState* out_state) {
    MagState state = {0};
    /* ... Rest der Funktion unveraendert ... */
```

Einzige Änderung: `MagState state;` → `MagState state = {0};`. Damit sind alle Felder (inklusive
`pin_input`/`pin_output`/`pin_enable`, die als `MagPin`-Enum bei Wert `0` alle auf `MagPinA7`
zeigen, also alle drei gleich sind) beim Aufruf von `mag_state_gpio_is_valid(&state)` definiert;
die Funktion liefert dann deterministisch `false` (drei identische Pins sind laut eigener
Definition ungültig) und `mag_state_gpio_reset(&state)` läuft garantiert statt zufällig.

## Testhinweis (ohne Hardware)
Eigenständiges Reproduktionsprogramm der reinen Zustandslogik (keine Furi-/Flipper-Typen nötig):
`app-improvements/g4meover-magspoof/repro-state-load-uninitialized-gpio-validity/` —
`repro_old.c` (aktueller Stand, `MagState state;`) und `repro_new.c` (mit Fix,
`MagState state = {0};`), beide mit `file_exists=false` (entspricht fehlender/gelöschter
`mag_state.txt`).

```sh
gcc -g -O0 -o repro_old repro_old.c
gcc -g -O0 -o repro_new repro_new.c
valgrind --error-exitcode=99 -q ./repro_old   # 3x "uninitialised value" in mag_state_gpio_is_valid, exit 99
valgrind --error-exitcode=99 -q ./repro_new   # keine Funde, exit 0
```

Bei mir lokal reproduziert: `repro_old` liefert unter Valgrind drei
„Conditional jump or move depends on uninitialised value(s)"-Meldungen mit Stack `by
mag_state_gpio_is_valid → mag_state_load_alt → main` und Exit-Code 99 (Fehler); `repro_new` läuft
mit Exit-Code 0 sauber durch. Beide geben denselben `pin_input`/`pin_output`/`pin_enable`-Endwert
aus (`0/1/2`, entspricht `MagPinA7`/`MagPinA6`/`MagPinA4`) — bestätigt, dass der Fix die aktuell
sichtbare Ausgabe nicht verändert, nur die UB beseitigt.

Funktionaler Test nach dem Patch, lokal auf dem Flipper (kein Sender/Reader nötig):
1. `ufbt` bauen, App starten.
2. Vor dem ersten Start `apps_data/mag/mag_state.txt` (falls vorhanden) löschen, um den
   „file_exists=false"-Pfad zu erzwingen.
3. App starten, in die GPIO-Pin-Einstellungen wechseln: `Input`/`Output`/`Enable` müssen die
   Defaults `A7`/`A6`/`A4` zeigen (wie vor dem Fix, da der zweite Reset-Block dies bereits
   erzwang) — dies bestätigt, dass sich das sichtbare Verhalten nicht ändert.
4. Optional unter `gdb`/mit `-fsanitize=undefined` auf einer host-kompilierten Kopie der
   restlichen `mag_state.c`-Logik (Storage-Aufrufe gestubbt) zusätzlich absichern.

## Risiko
- Sehr niedrig: reine Initialisierung einer lokalen Struktur, keine Struct-/API-Änderung, kein
  Einfluss auf das `.txt`-Zustandsdateiformat oder auf den Erfolgsfall (Datei vorhanden und
  lesbar bleibt byteidentisch im Verhalten).
- Ändert aktuell **keine** beobachtbare Ausgabe (siehe Host-Repro-Vergleich oben) — reiner
  Robustheits-/UB-Fix gegen künftige Regressionen an dieser Funktion.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko.

## Quelle
- `zacharyweiss/magspoof_flipper` (main): `mag_state.c` (`mag_state_load`,
  `mag_state_gpio_is_valid`, `mag_state_gpio_reset`), `helpers/mag_types.h` (`MagState`,
  `MagPin`, `MAG_STATE_DEFAULT_PIN_*`).
- Valgrind Memcheck (`--error-exitcode`, „Conditional jump or move depends on uninitialised
  value(s)") als Nachweiswerkzeug für den Uninitialisiert-Lesefund.
