# Flipper TV Remote (jmanion0139) — Status

## Quelle
- `jmanion0139/flipper_tv_remote`, GitHub, GPL-3.0, Tag `v1.1`, Commit `9ce9d3e`.
- Eigenständige Infrarot-App (`fap_category = "Infrared"`), kein RogueMaster-Bündel-Fund —
  erste Kategorie "Infrarot (Fernbedienung)" in diesem Katalog, Gegenstück zur bisher nur
  über Gegenüberwachungs-Apps (Nyx/Sobe) abgedeckten IR-Erkennungsseite.
- 2390 Zeilen über sechs Quelldateien (`flipper_tv_remote.c/.h`,
  `views/tv_remote_learn.c/.h`, `views/tv_remote_remote.c/.h`), vollständig gelesen.

## Was
Lernt bis zu 12 IR-Signale (Power, Vol+/-, Ch+/-, Up/Down/Left/Right, OK, Back, Home) von
einer beliebigen TV-Fernbedienung und legt sie als benannte `.ir`-Datei im Standard-Flipper-
IR-Format unter `/ext/infrared/` ab (kompatibel zur eigenen Infrarot-App). Die eigentliche
Fernbedienungs-Ansicht bildet ein konzentrisches Kreislayout nach, das D-Pad/OK/Back jeweils
mit Kurzdruck- und Halte-Aktion belegt, zusätzlich Doppeltipp auf Back für Power — deckt die
zwölf häufigsten Tasten auf einem einzigen Bildschirm ab, ohne Menüwechsel während der
Bedienung. Mehrere benannte Fernbedienungen parallel speicherbar, Lösch-/Überschreibfunktion,
Orientierungs-Umschaltung (Hoch-/Querformat), Einstellungen (Tasten-Tausch, Halten-Wiederholung
pro Achse, Halten-Sendemodus Einzelimpuls/Dauerhaft) persistent über `saved_struct`.

## API-87-Status
✅ Verifiziert gegen einen frischen, blobfreien Sparse-Checkout von `G4MEOVER18/G4MEOVER-FW`
(Tag `v2.0.0`, Commit `b359f6f`, `lib/infrared`, `lib/flipper_format`, `lib/toolbox`): alle
genutzten Symbole (`infrared_worker_rx/tx_*`, `infrared_worker_set_raw_signal`/
`_set_decoded_signal`, `infrared_worker_get_raw_signal`/`_get_decoded_signal`,
`infrared_get_protocol_name`/`_by_name`, `INFRARED_COMMON_CARRIER_FREQUENCY`/
`_DUTY_CYCLE`, `flipper_format_file_alloc`/`_buffered_file_alloc`,
`flipper_format_write_uint32`/`_float`/`_hex`/`_string_cstr`/`_comment_cstr`,
`flipper_format_read_uint32`/`_float`/`_hex`/`_string`, `flipper_format_get_value_count`,
`saved_struct_save`/`_load`) byteidentisch vorhanden. Reine GUI-/Infrarot-/Storage-APIs ohne
Erweiterungsport- oder sonstige HAL-Abhängigkeit — kein Kompatibilitätsrisiko, kein Zusatzmodul
nötig (nutzt ausschließlich den eingebauten IR-Blaster/-Empfänger).

## Fund
`tv_remote_ir_burst()` (`views/tv_remote_remote.c`) wird für jeden "Einzelimpuls"-Versand
genutzt: Back, Power (Doppeltipp), sowie jede Kurzdruck- und jede Nicht-Dauerhaft-Halten-Aktion
des D-Pads. Sie ruft `tv_remote_tx_start(app, button_index)` und danach, falls `app->tx_active`
gesetzt ist, nach der Sendedauer `tv_remote_tx_stop(app)`. `tv_remote_tx_start()` selbst bricht
aber sofort ab, wenn `app->worker_active` bereits gesetzt ist — unabhängig davon, für welche
Taste. Ist die Einstellung "Hold TX: Continuous" aktiv und hält der Nutzer gerade eine D-Pad-Hold-
Aktion (z. B. Up gehalten → Dauer-Sendung von Vol_up), bleibt `worker_active`/`tx_active` über
die gesamte Haltedauer gesetzt. Tippt der Nutzer währenddessen Back (einmal oder als erster Tipp
eines Doppeltipps) bzw. löst der Doppeltipp-Timer Power aus, läuft `tv_remote_ir_burst()` für
Back/Power: `tv_remote_tx_start()` bricht wegen `worker_active == true` sofort ab (sendet nichts,
`remote_selected` bleibt auf der ursprünglichen Hold-Taste stehen), der anschließende
`if(app->tx_active)`-Zweig greift aber trotzdem — er sieht das Flag der ALTEN, noch laufenden
Dauer-Sendung und beendet diese vorzeitig. Ergebnis: das Halten wird abgebrochen, ohne dass
jemals ein Back- oder Power-Signal gesendet wurde — und das, obwohl die UI (`tv_remote_remote_
custom_event_callback`/Input-Callback) den Ring für Back/Power bereits als "aktiv" einfärbt, der
Nutzer also sieht, dass ein Befehl ausgelöst wurde. Betrifft jede reale Bediensituation, in der
während einer laufenden Dauer-Halten-Sendung Back (Zurück/Power) gedrückt wird — mit
"Hold TX: Continuous" ist das kein Rand-, sondern ein naheliegender Bedienfall (z. B. Lautstärke
hochhalten, dann zum Senderwechsel/Abschalten zu Back/Power wechseln, ohne vorher loszulassen).

## Fix
Additiv, in `tv_remote_ir_burst()`: bevor der eigentliche Start versucht wird, eine bereits für
eine ANDERE Taste laufende Übertragung explizit über das ohnehin vorhandene `tv_remote_tx_stop()`
beenden (`if(app->worker_active && app->remote_selected != button_index) tv_remote_tx_stop(app);`).
Damit startet die nachfolgende `tv_remote_tx_start()` garantiert sauber und sendet tatsächlich das
angeforderte Signal; eine zufällig bereits für dieselbe Taste laufende Sendung (in der Praxis nicht
erreichbar, da Back/Power nie Hold-Ziel sind) bleibt unverändert durch den bestehenden
`worker_active`-Guard in `tv_remote_tx_start()` abgefangen. Patch gegen zwei frische, unabhängige
Zweitkopien mit `git apply --check` verifiziert, real angewendet, byte-für-byte abgeglichen —
identisch (siehe `flipper-tv-remote-back-power-burst-during-continuous-tx-fix/
views_tv_remote_remote_c.patch`).

## Host-Repro
`repro-flipper-tv-remote-back-power-burst-during-continuous-tx/repro.c`: `tv_remote_tx_start()`/
`tv_remote_tx_stop()`/`tv_remote_ir_burst()` 1:1 aus dem Quelltext übernommen (IR-Hardware-Aufrufe
durch einen Sende-Log-Zähler ersetzt, reine Zustandsmaschine ohne Hardwarebezug). Vier Szenarien:
Regression (Back-Burst ohne laufende Sendung sendet korrekt), der eigentliche Fund (Back-Tipp
während laufender Dauer-Halten-Sendung von Up), derselbe Fund für Power per Doppeltipp, sowie ein
Äquivalenztest für den (praktisch unerreichbaren) Fall, dass Burst-Ziel und laufende Sendung
dieselbe Taste wären — dort verhalten sich ALT und NEU identisch. Unter
`gcc -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`: ALT sendet in den beiden Fund-Szenarien nachweislich 0 Back-/Power-Signale
(`sent_count` bleibt bei 1, zeigt weiterhin die alte Hold-Taste), NEU sendet in beiden Fällen
korrekt das angeforderte Signal (`sent_count` erhöht sich, letzter Eintrag ist Back/Power) —
ALLE CHECKS BESTANDEN in beiden Stufen, keine Compiler-/Sanitizer-Warnungen.

## Testhinweis (lokal)
Patch gegen die eigene Kopie von `flipper_tv_remote` anwenden, mit `ufbt` bauen. Funktionstest:
Settings → "Hold TX: Continuous" aktivieren, eine gelernte Fernbedienung öffnen, Up (oder eine
andere D-Pad-Richtung) gedrückt HALTEN (löst die Dauer-Sendung der Hold-Aktion aus, z. B. Vol_up),
währenddessen kurz auf Back tippen. Vor dem Fix: die Dauer-Sendung bricht ab, aber am per IR-
Empfänger eines zweiten Geräts (oder am Zielgerät selbst) ist kein Back-Befehl sichtbar. Nach dem
Fix: Back wird tatsächlich gesendet. Mit "Hold TX: Single" (Standardeinstellung) nicht reproduzierbar,
da dort jede Halte-Aktion ohnehin sofort wieder stoppt, bevor ein Konflikt entstehen kann.

## Relevanz
Erste eigene IR-Fernbedienungs-App im Katalog (Gegenstück zur bisherigen reinen IR-Erkennung bei
Nyx/Sobe) — nützlich als generisches Werkzeug im Rahmen autorisierter Tests (z. B. Steuerung von
Vorführ-/Testgeräten ohne Originalfernbedienung), keine Angriffsfunktion. Erzeugt und liest
ausschließlich das Standard-`.ir`-Dateiformat, keine Zusatzhardware nötig.
