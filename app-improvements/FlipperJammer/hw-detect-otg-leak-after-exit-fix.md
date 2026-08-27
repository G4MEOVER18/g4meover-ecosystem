# FlipperJammer — OTG-5V bleibt nach Hardware-Erkennung dauerhaft an

## Was
Ich habe `jammer_hw_detect()` (`jammer_app.c`) geprüft — der bisher als "Tiefenaudit
offen" vorgemerkte Rest meiner eigenen App neben den bereits durchgesehenen Modulen
(EM4100-Encoder-Fix, RollJam-/RollLab-Puffer-Reset, Jammer-Doppel-Stop in RollForge).
Fund: der 5V-OTG-Boost bleibt in einem meiner selbst dokumentierten Standardfälle für
den Rest der App-Sitzung — und auch nach dem Beenden der App — dauerhaft eingeschaltet.

Vier Stellen im Code schalten OTG selbstständig ein/aus und tragen dafür je ein
eigenes, lokales "hab ich selbst eingeschaltet"-Flag:

- `nrf24_jam.c` — `s_otg_by_jam`
- `ble_jam.c` — `s_otg_by_ble`
- `subghz_jam.c` — `s_otg_by_jam` (eigener Dateiscope)
- `scenes/scene_nrf24_diag.c` — `s_otg_by_diag`

Alle vier folgen demselben Muster: OTG nur dann selbst abschalten, wenn man es auch
selbst eingeschaltet hat (`nrf24_jam.c`, Beispiel):
```c
static void nrf24_jam_power_on(void) {
    if(!furi_hal_power_is_otg_enabled()) {
        furi_hal_power_enable_otg();
        furi_delay_ms(20);
        s_otg_by_jam = true;
    }
}
static void nrf24_jam_power_off(void) {
    if(s_otg_by_jam && furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
        s_otg_by_jam = false;
    }
}
```

`jammer_hw_detect()` (`jammer_app.c`, läuft einmalig beim App-Start) schaltet OTG
ebenfalls ein/aus, aber **ohne** ein Flag zu setzen, das über das Ende der Funktion
hinaus lebt — die lokale Variable `otg_was_on`/`otg2_was_on` ist nur innerhalb der
Funktion sichtbar:
```c
// --- NRF24 pruefen ---
// Versuch 1: ohne OTG (3.3V von GPIO-VCC)
app->hw_nrf24 = nrf24_check_connected();

// Versuch 2: mit OTG-Strom (manche Multiboards brauchen 5V)
if(!app->hw_nrf24) {
    bool otg2_was_on = furi_hal_power_is_otg_enabled();
    if(!otg2_was_on) {
        furi_hal_power_enable_otg();
        furi_delay_ms(30);
    }
    app->hw_nrf24 = nrf24_check_connected();
    if(!app->hw_nrf24 && !otg2_was_on) {
        furi_hal_power_disable_otg();
    }
}
```
Der Abschalt-Zweig greift nur, wenn die Erkennung **fehlschlägt**
(`!app->hw_nrf24`). Genau der Fall, den der eigene Kommentar als Zielszenario nennt
— ein NRF24-Multiboard, das den 5V-Boost tatsächlich braucht ("E01C-ML01SP4 und
PA+LNA Module brauchen mehr als die 3.3V vom GPIO-Pin", derselbe Kommentar auch in
`nrf24_jam.c`) — lässt `app->hw_nrf24 = true` werden, der Abschalt-Zweig wird
übersprungen, und OTG bleibt an. Denselben Effekt hat der bereits im Code selbst so
benannte CC1101-Fall ("OTG nur aktiv lassen wenn CC1101 gefunden") im ersten Teil
derselben Funktion.

In beiden Fällen verlässt `jammer_hw_detect()` die Funktion mit OTG an, ohne dass
irgendwo im App-Zustand festgehalten wird, dass **die Erkennung selbst** es
eingeschaltet gelassen hat. Startet man danach NRF24- oder BLE-Jamming, sehen
`nrf24_jam_power_on()`/`ble_jam_start()` OTG bereits an, beanspruchen es deshalb laut
eigenem "nur abschalten was ich selbst eingeschaltet habe"-Muster nicht als eigenes
(`s_otg_by_jam`/`s_otg_by_ble` bleiben `false`) — folglich schaltet auch
`nrf24_jam_power_off()`/`ble_jam_stop()` beim Stoppen nichts ab. Und
`jammer_app_free()` enthält überhaupt keinen OTG-Aufräumcode. Ich habe den gesamten
Baum nach `enable_otg`/`disable_otg`/`is_otg_enabled` durchsucht — außer den vier
oben genannten Stellen und `jammer_hw_detect()` selbst gibt es keine weitere
Stelle, die OTG je wieder abschalten könnte.

Einzige zufällige Ausnahme: `subghz_jam.c` beansprucht OTG bereits, sobald es beim
eigenen Start **irgendwie** an vorfindet (nicht nur wenn es selbst einschaltet):
```c
while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
    furi_hal_power_enable_otg();
    ...
}
if(furi_hal_power_is_otg_enabled()) s_otg_by_jam = true;
```
Läuft in derselben Sitzung noch ein SubGHz-Jam-Start/Stop-Zyklus, schaltet dieser
das von `jammer_hw_detect()` hinterlassene OTG zufällig mit ab. Wer aber — wie im
Anwendungsfall des NRF24-5V-Boost-Kommentars naheliegend — nur die NRF24- oder
BLE-Funktionen nutzt und die App danach verlässt, behält den 5V-Boost dauerhaft
aktiv: über das Ende der App-Sitzung hinaus, bis zum nächsten Neustart des Geräts
oder bis eine andere App zufällig `disable_otg()` aufruft.

## Nutzen
- Schließt einen Batterie-Drain: der 5V-Boost-Wandler bleibt nach dem Schließen der
  App aktiv eingeschaltet, obwohl kein Jamming mehr läuft — betrifft direkt den vom
  Code selbst als Standardfall benannten 5V-NRF24-Multiboard-Fall sowie den
  externen-CC1101-Fall, also die beiden häufigsten realen Hardware-Konfigurationen
  für diese App.
- Der GPIO-5V-Pin bleibt dauerhaft energisiert, auch nachdem der Nutzer die App
  verlassen hat — unerwartet für alles, was danach an denselben Pin angeschlossen
  wird (5V ohne aktive Absicht des Nutzers).
- Reine Zustands-/Ownership-Logik in `jammer_app.c`, keine Änderung an den vier
  bestehenden, bereits korrekten Modul-eigenen OTG-Funktionen nötig.

## Betroffene Dateien
- `jammer_app.h` — `JammerApp`: ein neues Feld `hw_detect_left_otg_on` (bool).
- `jammer_app.c` — `jammer_hw_detect()`: merkt sich am Ende der Funktion, ob OTG
  jetzt an ist und beim Eintritt der Funktion noch aus war. `jammer_app_free()`:
  schaltet OTG anhand dieses Flags beim Beenden der App ab, falls es dann noch an
  ist.

## Port-Skizze / Patch

### `jammer_app.h` — `JammerApp`-Struct
```c
    /* Hardware-Erkennung (gesetzt beim Start durch jammer_hw_detect) */
    bool hw_ext_cc1101; // externer CC1101 via SubGHz-Treiber erkannt
    bool hw_nrf24;      // NRF24-Modul via GPIO-SPI erkannt
    bool hw_detect_left_otg_on; // jammer_hw_detect() hat OTG an gelassen, war beim
                                 // Funktionseintritt aus -> beim App-Ende abschalten
```

### `jammer_app.c` — `jammer_hw_detect()`
```c
void jammer_hw_detect(JammerApp* app) {
    bool otg_off_at_entry = !furi_hal_power_is_otg_enabled();

    // --- Ext CC1101 pruefen (mit kurzer OTG-Power) ---
    bool otg_was_on = furi_hal_power_is_otg_enabled();
    ... // unveraendert
    if(app->hw_ext_cc1101) {
        app->subghz_external = true;
    }

    // --- NRF24 pruefen ---
    ... // unveraendert (Versuch 1 + Versuch 2)

    // NEU: merken, ob ich selbst OTG an gelassen habe (fuer den Aufraeum-Schritt
    // in jammer_app_free() -- keine der vier Modul-eigenen OTG-Funktionen
    // (nrf24_jam.c/ble_jam.c/subghz_jam.c/scene_nrf24_diag.c) weiss davon, da sie
    // OTG bereits eingeschaltet vorfinden und es deshalb nicht als eigenes
    // beanspruchen).
    app->hw_detect_left_otg_on = otg_off_at_entry && furi_hal_power_is_otg_enabled();
}
```
Einzige Änderung an der bestehenden Erkennungslogik: die zusätzliche Zeile
`otg_off_at_entry` am Anfang und die Flag-Zuweisung am Ende. Beide bereits
vorhandenen Erkennungs-Blöcke (CC1101, NRF24 Versuch 1+2) bleiben unverändert.

### `jammer_app.c` — `jammer_app_free()`
```c
void jammer_app_free(JammerApp* app) {
    furi_assert(app);

    /* Stop timer if running */
    if(app->running) {
        furi_timer_stop(app->run_timer);
        app->running = false;
    }
    furi_timer_free(app->run_timer);

    // NEU: von jammer_hw_detect() hinterlassenes OTG abschalten, falls es sonst
    // niemand (z. B. ein zwischenzeitlicher SubGHz-Jam-Stop) bereits getan hat.
    if(app->hw_detect_left_otg_on && furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
    }

    /* Remove views */
    ... // unveraendert
```

Bewusst **kein** Eingriff in `nrf24_jam.c`, `ble_jam.c`, `subghz_jam.c` oder
`scene_nrf24_diag.c` — deren "nur abschalten was ich selbst eingeschaltet habe"-Logik
ist für sich genommen bereits korrekt; die Lücke lag ausschließlich darin, dass die
einmalige Erkennung beim App-Start keinen eigenen, bis zum App-Ende sichtbaren
Eigentümer-Nachweis hinterlassen hat.

## Testhinweis (ohne Hardware)
Reine Zustands-/Ownership-Logik, host-nachstellbar ohne echten NRF24-/CC1101-Treiber.
`repro-hw-detect-otg-leak/repro.c` bildet die für den Bug relevanten Zustände
(`g_otg` als Modell von `furi_hal_power_is_otg_enabled()`) sowie die unveränderte
`nrf24_jam_power_on()/off()`-Eigentümerlogik nach und durchläuft das im Code selbst
benannte Zielszenario (NRF24-Multiboard braucht 5V-Boost, `hw_detect` erkennt es erst
im zweiten Versuch mit OTG an) bis zum simulierten App-Ende:

```
$ gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
$ ./repro bug    # vorher: kein hw_detect_left_otg_on-Flag, kein Cleanup in jammer_app_free()
Nach hw_detect:      OTG=AN
Nach nrf24_jam_start: OTG=AN (s_otg_by_jam=false)
Nach nrf24_jam_stop:  OTG=AN
Nach App-Ende:        OTG=AN
repro: repro.c:106: main: Assertion `!g_otg && "OTG-5V-Boost bleibt nach Beenden der App aktiv (Batterie-Drain)"' failed.
Aborted (exit code 134)

$ ./repro fix    # nachher: hw_detect_left_otg_on gesetzt, jammer_app_free() schaltet ab
Nach hw_detect:      OTG=AN
Nach nrf24_jam_start: OTG=AN (s_otg_by_jam=false)
Nach nrf24_jam_stop:  OTG=AN
Nach App-Ende:        OTG=AUS
OK: OTG korrekt aus.
exit code: 0
```
"vorher" zeigt in beiden Läufen identisch `s_otg_by_jam=false` beim Jam-Start (die
Modul-eigene Logik erkennt korrekt, dass sie OTG nicht selbst eingeschaltet hat) —
der Unterschied entsteht ausschließlich am simulierten App-Ende, exakt an der Stelle,
die der Patch ergänzt.

Funktionaler Test danach lokal auf dem Flipper: mit angeschlossenem 5V-NRF24-Board
(oder testweise per `furi_hal_power_enable_otg()` vor App-Start simuliert) die App
starten, NRF24-Jamming kurz starten und mit Zurück wieder stoppen, App verlassen —
vor dem Fix bleibt der 5V-Pin messbar (Multimeter) oder am OTG-Status im
Power-Menü/Debug-Log dauerhaft an; nach dem Fix ist er beim Zurückkehren ins
Hauptmenü/Verlassen der App wieder aus. Ebenso mit angeschlossenem externem CC1101
(ohne je eine SubGHz-Jam-Funktion zu starten) reproduzierbar.

## Risiko
- Niedrig: rein additiver Patch (ein neues Bool-Feld, zwei neue Zeilen in
  `jammer_hw_detect()`, ein neuer Cleanup-Block in `jammer_app_free()`). Keine
  bestehende Zeile wird geändert oder gelöscht.
- Kein Eingriff in die vier bestehenden Modul-eigenen OTG-Funktionen — deren
  Verhalten während einer laufenden Jam-Sitzung bleibt exakt wie zuvor.
- Einzig denkbare Nebenwirkung: falls `jammer_hw_detect()` OTG absichtlich für einen
  externen CC1101 an gelassen hat und der Nutzer die App verlässt, ohne je SubGHz-Jam
  genutzt zu haben, schaltet der Patch den 5V-Pin jetzt beim Verlassen ab, statt ihn
  (bisher ungewollt) dauerhaft anzulassen — das ist der eigentliche Zweck des Fixes,
  kein Nebeneffekt. Der Pin lässt sich beim nächsten App-Start jederzeit erneut
  automatisch aktivieren.
- Deckt nur die App-Ende-Aufräumlücke ab; die separate, bereits im Backlog vermerkte
  Frage der `s_otg_by_jam`-Ownership-Inkonsistenz zwischen `subghz_jam.c`
  ("beanspruche OTG sobald es an ist") und `nrf24_jam.c`/`ble_jam.c`
  ("beanspruche nur was ich selbst eingeschaltet habe") bleibt bestehen, führt aber
  für sich genommen zu keinem Leck mehr, sobald dieser Patch liegt (der Cleanup in
  `jammer_app_free()` fängt den Rest ab).

## Quelle
- `G4MEOVER18/FlipperJammer` (main): `jammer_app.c` (`jammer_hw_detect`,
  `jammer_app_free`), `jammer_app.h` (`JammerApp`), zum Abgleich der bestehenden
  Eigentümer-Konvention `modules/nrf24_jam.c`, `modules/ble_jam.c`,
  `modules/subghz_jam.c`, `scenes/scene_nrf24_diag.c`.
