# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: FeliCa-Schreib-Timeout zeigt "Failed" ohne jede Ursachenangabe

## Was
Ich habe den NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`, `fap_version`
1.3, Quelle `github.com/wakdev/nfctools-fz`) fortgesetzt und den zuletzt offen gebliebenen
Rückstellpunkt aufgegriffen: `scenes/write/nfc_tools_scene_felica_write.c` (259 Zeilen) und
`scenes/write/nfc_tools_scene_felica_write_input.c` (99 Zeilen) vollständig gelesen — die "FeliCa
Write Block"-Funktion (Schreiben eines 16-Byte-Blocks per WRITE WITHOUT ENCRYPTION, Service
`0x0009`, Blöcke 00–13, passend zu FeliCa Lite/Lite-S).

Eingabevalidierung (`felica_write_input.c`) geprüft: Länge exakt 34 Zeichen erzwungen (2 Dezimal-
+ 32 Hex-Ziffern), Blockbereich 00–13 konsistent mit den eigenen UI-Strings
(`NTS_INPUT_FELICA_BLOCK "Block 0-13:"`, `NTS_ERR_FELICA_BLOCK_RANGE "Block out of range\n(00-13
only)"`) sowie mit dem tatsächlich verwendeten FeliCa-Service-Code — kein Fund (der abweichende
Struct-Feld-Kommentar `felica_write_block; // block number for write (0-27)` in
`include/nfc_tools_i.h:169` ist reine Doku-Ungenauigkeit ohne Funktionsauswirkung, da Eingabe-
Validierung und UI-Strings übereinstimmend 0–13 durchsetzen).

Frame-Aufbau (`nfc_tools_felica_write_cb()`) gegenübergestellt: `FELICA_WRITE_FRAME_SIZE` (32) exakt
nachgerechnet (1 Längen- + 1 Kommando- + 8 IDm- + 1 NumServices- + 2 ServiceCode- + 1 NumBlocks- + 2
BlockListElement- + 16 Datenbytes = 32) gegen die tatsächlich mit `bit_buffer_append_*()`
geschriebene Bytefolge — Summe stimmt exakt, `bit_buffer_alloc(FELICA_WRITE_FRAME_SIZE + 2)` (34
Byte für Frame+CRC) reicht für den anschließenden `felica_crc_append()`-Aufruf — kein
Speicherzugriffsfehler.

## Fund: Präsenz-Timeout ohne jede Fehlerursache — "Failed" mit leerem Meldungstext
`nfc_tools_felica_write_worker()` startet den FeliCa-Poller und wartet **einmalig** auf das
Abschluss-Flag, ohne den Rückgabewert der Wartefunktion zu betrachten
(`scenes/write/nfc_tools_scene_felica_write.c:176–183`):

```c
wctx.nfc_poller = nfc_poller_alloc(app->nfc, NfcProtocolFelica);
nfc_poller_start(wctx.nfc_poller, nfc_tools_felica_write_cb, &wctx);

furi_event_flag_wait(wctx.done, 1u, FuriFlagWaitAny, 8000);

nfc_poller_stop(wctx.nfc_poller);
nfc_poller_free(wctx.nfc_poller);
furi_event_flag_free(wctx.done);

if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

if(wctx.success) {
    notification_message(app->notifications, &sequence_success);
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
} else {
    notification_message(app->notifications, &sequence_error);
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
}
```

`app->info_str` wird ausschließlich innerhalb des Poller-Callbacks (`nfc_tools_felica_write_cb()`)
gesetzt — für jeden der vier dort behandelten Fälle (Erfolg, Write denied, Bad response, Tag-
Aktivierungsfehler), jeweils unmittelbar bevor `furi_event_flag_set(ctx->done, 1u)` das Warten
beendet. Wird innerhalb der 8 Sekunden **kein einziges** FeliCa-Poller-Ereignis ausgelöst — der
naheliegendste Fall: der Nutzer öffnet die Szene und hält gar keine (oder eine zu späte) FeliCa-Karte
an die Antenne —, läuft der Callback nie, `wctx.done` wird nie gesetzt,
`furi_event_flag_wait()` liefert einen Timeout-Status zurück, der aber nirgends abgefragt wird.
`wctx.success` bleibt bei seinem Initialwert `false`, der Code fällt in den `else`-Zweig — zeigt
also "Failed" an, mit dem Inhalt von `app->info_str`, der seit `furi_string_reset(app->info_str)` in
`_on_enter()` nie beschrieben wurde: eine leere Zeichenkette. Der Nutzer bekommt einen leeren
Popup-Text unter der Überschrift "Failed" statt jeder Ursachenangabe.

Das ist genau die Fundklasse, die im NFC-Tools-Cluster bereits mehrfach dokumentiert ist (CC-Read-
"Tag too small"-Fehlmeldung, Remove-Password-Reihenfolgen-Widerspruch): eine dem tatsächlichen
Zustand nicht entsprechende Rückmeldung — hier als vollständiges Fehlen einer Ursache statt als
falsche Ursache.

Besonders auffällig im Vergleich zu den übrigen Poller-/Schreib-Workern derselben App: **jeder**
andere Schreibpfad, der einen NFC-Tag über einen Poller anspricht, führt vorher eine eigene
`NfcScanner`-Erkennungsphase mit explizit abgefragtem Timeout durch und meldet dabei konsistent
dieselbe, bereits vorhandene Konstante `NTS_ERR_NO_TAG` ("No tag detected") — belegt an fünf
Stellen im selben Repo:

```
scenes/write/nfc_tools_scene_write_scan.c:76:      if(flags == (uint32_t)FuriFlagErrorTimeout) {
scenes/write/nfc_tools_scene_write_scan.c:79:          furi_string_set(app->info_str, NTS_ERR_NO_TAG);
scenes/other/nfc_tools_scene_format_tag.c:36-38:     … dieselbe Prüfung …
scenes/other/nfc_tools_scene_lock_tag.c:151-153:      … dieselbe Prüfung …
scenes/other/nfc_tools_scene_nfc_commands_run.c:229-230: … dieselbe Prüfung …
scenes/other/nfc_tools_scene_remove_password_write.c:159-161: … dieselbe Prüfung …
scenes/other/nfc_tools_scene_set_password_write.c:159-161: … dieselbe Prüfung …
```

`nfc_tools_scene_felica_write.c` ist der einzige Schreibpfad im gesamten Cluster, der **keine**
vorgeschaltete Erkennungsphase besitzt — der einmalige 8-Sekunden-Poller-Wait ist die einzige
Präsenzprüfung der gesamten Funktion, und genau dessen Timeout wird stillschweigend verschluckt.
(Anmerkung, nicht Teil dieses Fundes: die genannten Geschwisterfunktionen prüfen ihrerseits den
Rückgabewert ihres *zweiten*, poller-internen Waits — analog zu `wctx.done` hier — ebenfalls nicht;
dort ist das Zeitfenster für einen echten Timeout aber deutlich kleiner, da die vorgeschaltete
Scanner-Phase die Tag-Präsenz bereits bestätigt hat. Bei FeliCa Write fehlt diese Vorprüfung
komplett, wodurch der unbeobachtete Timeout zum Normalfall bei jeder verpassten/zu späten
Kartenannäherung wird — für eine künftige Sitzung als eigener, kleinerer Rückstellpunkt vorgemerkt.)

## Host-Repro
Eigenständiges Zustandsmodell des Wartens auf das Poller-Ergebnis (kein NFC-/Flipper-Bezug), das für
fünf Szenarien (kein Tag innerhalb 8 s, Erfolg, Write denied, Bad response, Aktivierungsfehler)
vergleicht, was das Popup anzeigt — einmal mit ignoriertem Wait-Rückgabewert (ALT, wie ausgeliefert)
und einmal mit demselben Timeout-Muster wie die übrigen Schreib-Worker (NEU):

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

Tatsächliche Ausgabe (lokal ausgeführt):
```
== ALT (wait() return value discarded) ==
ALT no FeliCa tag presented within 8s (timeout)   | header=Failed body=""  <-- BLANK BODY
ALT tag presented, write succeeds                 | header=OK     body="Block S06 written\nOK"
ALT tag presented, write denied (SF1/SF2 set)     | header=Failed body="Write denied\nBlock S06\nSF1=01 SF2=A6"
ALT tag presented, malformed/short response       | header=Failed body="Bad response\nlen=4 code=00"
ALT tag presented, FeliCa activation failed       | header=Failed body="Tag error\nCould not\nactivate FeliCa"

== NEU (timeout sets NTS_ERR_NO_TAG) ==
NEU no FeliCa tag presented within 8s (timeout)   | header=Failed body="No tag detected"
NEU tag presented, write succeeds                 | header=OK     body="Block S06 written\nOK"
NEU tag presented, write denied (SF1/SF2 set)     | header=Failed body="Write denied\nBlock S06\nSF1=01 SF2=A6"
NEU tag presented, malformed/short response       | header=Failed body="Bad response\nlen=4 code=00"
NEU tag presented, FeliCa activation failed       | header=Failed body="Tag error\nCould not\nactivate FeliCa"

== Regression check (all non-timeout scenarios identical ALT vs NEU) ==
tag presented, write succeeds                 | identical
tag presented, write denied (SF1/SF2 set)     | identical
tag presented, malformed/short response       | identical
tag presented, FeliCa activation failed       | identical

Bestaetigt: ALT zeigt in 1 von 5 Faellen eine leere Fehlermeldung, NEU in 0 von 5. Regressionsabweichungen: 0.
```
Alle vier Fälle, in denen der Callback tatsächlich läuft (Erfolg, Denied, Bad Response,
Aktivierungsfehler), sind für ALT und NEU identisch — der Fix ändert nichts an bereits korrektem
Verhalten, nur am bislang unbehandelten Timeout-Fall.

## Betroffene Dateien
- `scenes/write/nfc_tools_scene_felica_write.c` — `nfc_tools_felica_write_worker()`, einziger
  Fix-Ort (Zeile ~179–195). Nutzt die in `include/nfc_tools_strings.h` bereits vorhandene Konstante
  `NTS_ERR_NO_TAG`, keine neue API/kein neuer String nötig.

## Risiko
Gering. Reine Meldungskorrektur nach demselben, im Cluster fünffach bereits etablierten Muster
(Wait-Rückgabewert prüfen, bei Timeout `NTS_ERR_NO_TAG` setzen) — keine neue Hardware-Interaktion,
keine Änderung an Frame-Aufbau oder Poller-Ansteuerung. Einziger Verhaltensunterschied: der zuvor
leere Popup-Text im Timeout-Fall wird durch "No tag detected" ersetzt; alle vier Fälle, in denen der
Poller tatsächlich ein Ereignis liefert, bleiben unverändert (Host-Repro als Regressionsfall
bestätigt).

## Port-Skizze / Fix
Rückgabewert von `furi_event_flag_wait()` festhalten und bei Timeout `info_str` explizit setzen,
bevor der bestehende Erfolg/Fehler-Zweig greift:

```c
uint32_t wait_result = furi_event_flag_wait(wctx.done, 1u, FuriFlagWaitAny, 8000);

nfc_poller_stop(wctx.nfc_poller);
nfc_poller_free(wctx.nfc_poller);
furi_event_flag_free(wctx.done);

if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

if(wait_result != 1u) {
    // Poller callback never fired within the timeout: no FeliCa tag was
    // presented. Report this like every other write path in the app does
    // for the same situation, instead of falling through with an empty
    // info_str.
    furi_string_set(app->info_str, NTS_ERR_NO_TAG);
}

if(wctx.success) {
    notification_message(app->notifications, &sequence_success);
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
} else {
    notification_message(app->notifications, &sequence_error);
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
}
```

Die `if(wait_result != 1u)`-Zeile greift ausschließlich dann, wenn der Callback nie lief (einzige
Quelle für `wctx.done`s Signal ist `furi_event_flag_set(ctx->done, 1u)`, das in jedem erreichbaren
Codepfad des Callbacks vor dessen Rückkehr aufgerufen wird) — sie überschreibt also nie eine bereits
vom Callback gesetzte, echte Fehlermeldung.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-felica-write-no-tag-timeout/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: Fix einbauen (`ufbt launch_app` gegen `nfctools`),
mit einer eigenen FeliCa-Lite-Testkarte "FeliCa Write Block" wählen, gültigen Block+Daten eingeben.
Regressionsfall: Karte sofort und stabil an die Antenne halten — Erfolgsmeldung "Block SXX
written\nOK" muss wie vor dem Fix erscheinen. Fehlerfall: nach der Eingabe bewusst **keine** Karte
an die Antenne halten und die vollen 8 Sekunden abwarten — vor dem Fix erscheint "Failed" mit leerem
Text, nach dem Fix "Failed\nNo tag detected". Zusatzcheck: Karte kurz vor Ablauf der 8 Sekunden noch
anhalten (z. B. bei 6–7 s) — muss weiterhin die reale Callback-Meldung (Erfolg/Denied/Bad
response/Tag error) zeigen, nicht "No tag detected" (Regressionsfall aus dem Host-Repro).

## Zusätzlich geprüft, kein Fund
- `felica_parse_data_bytes()`/`felica_parse_input()`: Indexzugriffe `s[2+i*2]`/`s[2+i*2+1]` für
  i=0..15 liegen vollständig innerhalb der vorab auf exakt 34 Zeichen geprüften Eingabe
  (`app->ndef_buf1`, `NFC_TOOLS_NDEF_BUF1_SIZE` = 1024 Byte) — kein Overrun, weder in der Eingabe-
  noch in der Schreib-Szene.
- `nfc_tools_felica_write_cb()`: Antwortlängenprüfung `rx_len >= 12` vor jedem Zugriff auf
  `rx_data[1]`/`rx_data[10]`/`rx_data[11]`, `bit_buffer_alloc(64)` für die Antwort ausreichend
  bemessen — kein Fund.
- `nfc_tools_felica_write_stop_worker()`/`_on_exit()`: Thread-Join vor Freigabe, `popup_reset()`
  danach — keine Use-after-free-/Doppel-Freigabe-Reihenfolge gefunden.

## Quelle
`github.com/wakdev/nfctools-fz`, Branch `main` (per `git clone --depth 1` sowie einzeln per
`raw.githubusercontent.com` gegengeprüft), Dateien `scenes/write/nfc_tools_scene_felica_write.c`,
`scenes/write/nfc_tools_scene_felica_write_input.c`, `include/nfc_tools_i.h`,
`include/nfc_tools_strings.h`, zum Quervergleich `scenes/write/nfc_tools_scene_write_scan.c`,
`scenes/other/nfc_tools_scene_set_password_write.c`, `_remove_password_write.c`,
`_nfc_commands_run.c`, `_lock_tag.c`.
