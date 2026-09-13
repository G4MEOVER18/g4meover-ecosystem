# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: unbeobachtete zweite Wait-Rückgabe in vier Geschwisterfunktionen führt zu leerer Fehlermeldung

## Was
Ich habe den in der letzten NFC-Tools-Sitzung (FeliCa-Write-Timeout, siehe
`firmware-mining/nfctools-felica-write-no-tag-timeout-blank-failure.md`) vorgemerkten Nebenbefund
aufgegriffen: die dort beschriebene Fundklasse — ein `furi_event_flag_wait()`-Rückgabewert, der
nirgends geprüft wird, wodurch ein Timeout stillschweigend als Erfolg=false mit leerem
Fehlertext durchfällt — taucht im selben Cluster (`applications/external/nfctools/`, Autor
`wakdev`, `fap_version` 1.3, Quelle `github.com/wakdev/nfctools-fz`) noch an vier weiteren
Stellen auf:

- `scenes/other/nfc_tools_scene_set_password_write.c` — `nfc_tools_set_password_worker()`
- `scenes/other/nfc_tools_scene_remove_password_write.c` — `nfc_tools_remove_password_worker()`
- `scenes/other/nfc_tools_scene_nfc_commands_run.c` — `nfc_commands_worker()`
- `scenes/other/nfc_tools_scene_lock_tag.c` — `nfc_tools_lock_tag_worker()`

Alle vier Dateien vollständig gelesen (zusammen ~1180 Zeilen), inkl. der jeweils zugehörigen
Poller-Callbacks.

## Fund: zweiter, unbeobachteter Wait nach bereits bestätigter Tag-Präsenz
Alle vier Worker folgen demselben zweistufigen Muster: zuerst eine `NfcScanner`-Erkennungsphase
mit **korrekt geprüftem** Timeout (setzt bei Ausbleiben `NTS_ERR_NO_TAG`), danach eine zweite,
poller-getriebene RF-Sitzung (Auth/Schreiben bzw. Rohkommando), die ihr Ergebnis über einen
lokalen, nullinitialisierten Kontext (`success`-Flag + `err[80]`-Puffer) und ein
`FuriEventFlag* done` zurückmeldet. In allen vier Fällen wird der Rückgabewert dieses zweiten
Waits verworfen:

```
scenes/other/nfc_tools_scene_set_password_write.c:202:    furi_event_flag_wait(write_ctx.done, 1u, FuriFlagWaitAny, 8000);
scenes/other/nfc_tools_scene_remove_password_write.c:205:    furi_event_flag_wait(write_ctx.done, 1u, FuriFlagWaitAny, 8000);
scenes/other/nfc_tools_scene_nfc_commands_run.c:256:    furi_event_flag_wait(cmd_ctx.done, 1u, FuriFlagWaitAny, 8000);
scenes/other/nfc_tools_scene_lock_tag.c:179:    furi_event_flag_wait(lock_ctx.done, 1u, FuriFlagWaitAny, 8000);
```

Muster (hier am Beispiel `lock_tag.c:172–197`, strukturell identisch in den anderen drei):

```c
LockTagCtx lock_ctx = {};                 // success=false, err[0]='\0'
lock_ctx.app  = app;
lock_ctx.done = furi_event_flag_alloc();

NfcPoller* poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
nfc_poller_start_ex(poller, nfc_tools_lock_tag_cb, &lock_ctx);

furi_event_flag_wait(lock_ctx.done, 1u, FuriFlagWaitAny, 8000);   // Rueckgabewert verworfen

nfc_poller_stop(poller);
nfc_poller_free(poller);
furi_event_flag_free(lock_ctx.done);

if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

if(lock_ctx.success) {
    ...
} else {
    notification_message(app->notifications, &sequence_error);
    furi_string_set(app->info_str, lock_ctx.err);   // leer, wenn der Callback nie lief
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
}
```

`lock_ctx.err`/`write_ctx.err`/`cmd_ctx.err` wird ausschließlich innerhalb des jeweiligen
Poller-Callbacks beschrieben (`nfc_tools_lock_tag_cb()`, `nfc_tools_set_password_write_cb()`,
`nfc_tools_remove_password_write_cb()`, `nfc_commands_iso3a_cb()`/`nfc_commands_iso4a_cb()`),
jeweils unmittelbar bevor `furi_event_flag_set(ctx->done, 1u)` den Wait beendet. Läuft der
Callback innerhalb der 8 Sekunden kein einziges Mal — die Karte wird zwischen erfolgreicher
Scanner-Erkennung (Phase 1) und dem Start der zweiten, poller-getriebenen RF-Sitzung (Phase 2)
wieder von der Antenne genommen, oder die Kopplung bricht genau in diesem Fenster ab —, bleibt
`ctx.success` bei `false` und `ctx.err` bei einem leeren String stehen. Die App zeigt dieselbe
Symptomatik wie beim bereits dokumentierten FeliCa-Write-Fund: Kopfzeile "Failed", Textkörper
komplett leer.

**Unterschied zum FeliCa-Fund:** dort war der poller-getriebene Wait die *einzige* Präsenzprüfung
der gesamten Funktion, wodurch der unbeobachtete Timeout bei jeder ausbleibenden
Kartenannäherung zum Normalfall wurde. Hier hat jede der vier Funktionen bereits eine
vorgeschaltete `NfcScanner`-Phase mit geprüftem Timeout — der Tag ist zum Zeitpunkt des zweiten
Waits also bereits einmal bestätigt erkannt worden. Das Zeitfenster für einen echten Timeout ist
entsprechend kleiner (die Karte muss *nach* erfolgreicher Erkennung, aber *vor* dem ersten
Poller-Ereignis der zweiten Sitzung wieder wegbewegt werden oder die Kopplung verlieren) — real,
aber seltener auslösbar als beim FeliCa-Fall. Betroffen sind vier von sechs Schreib-/
Kommando-Pfaden der App (die übrigen zwei, `write_scan`/`format_tag`, wurden bereits in früheren
Sitzungen einzeln geprüft und sind hiervon nicht betroffen — `format_tag.c` hat keinen zweiten
Wait dieser Art, `write_scan.c`s einziger Wait ist bereits der geprüfte Scanner-Wait selbst).

## Host-Repro
Generisches Zustandsmodell des zweiten Waits (kein NFC-/Flipper-Bezug), gemeinsam für alle vier
Fundstellen — der `success`/`err[80]`-Kontext ist in allen vier Dateien strukturell identisch.
Fünf Szenarien: Tag-Kontaktfehler, inkompatibler Tag-Typ, ein schrittspezifischer Schreib-/
Auth-Fehler, Erfolg, sowie der Fall, dass der Callback innerhalb der 8 s gar nicht läuft (Karte
nach der Scanner-Erkennung verloren) — einmal mit ignoriertem Wait-Rückgabewert (ALT, wie
ausgeliefert) und einmal mit geprüftem Rückgabewert (NEU, siehe Fix unten):

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

Tatsächliche Ausgabe (lokal ausgeführt):
```
== ALT (wait() return value discarded) ==
ALT tag contact error (Ready check fails)              | header=Failed body="Tag contact error"
ALT incompatible tag type                              | header=Failed body="Incompatible tag
Only NTAG213/215/216"
ALT step-specific write/auth error                     | header=Failed body="Config error
page 0x29 (err:3)"
ALT success                                            | header=OK     body="Success"
ALT no callback event within 8s (tag lost after scan)  | header=Failed body=""  <-- BLANK BODY

== NEU (timeout sets NTS_ERR_TAG_CONTACT) ==
NEU tag contact error (Ready check fails)              | header=Failed body="Tag contact error"
NEU incompatible tag type                              | header=Failed body="Incompatible tag
Only NTAG213/215/216"
NEU step-specific write/auth error                     | header=Failed body="Config error
page 0x29 (err:3)"
NEU success                                            | header=OK     body="Success"
NEU no callback event within 8s (tag lost after scan)  | header=Failed body="Tag contact error"

== Regression check (all non-timeout scenarios identical ALT vs NEU) ==
tag contact error (Ready check fails)              | identical
incompatible tag type                              | identical
step-specific write/auth error                     | identical
success                                            | identical

Bestaetigt: ALT 1 von 5 Faellen mit leerer Fehlermeldung, NEU 0 von 5. Regressionsabweichungen: 0.
```
Alle vier Fälle, in denen der Callback tatsächlich läuft, sind für ALT und NEU identisch — der Fix
ändert nichts an bereits korrektem Verhalten, nur am bislang unbehandelten Timeout-Fall.

## Betroffene Dateien
Je ein Fix-Ort pro Datei, alle strukturell identisch:

- `scenes/other/nfc_tools_scene_set_password_write.c` — `nfc_tools_set_password_worker()`,
  Zeile ~202–210.
- `scenes/other/nfc_tools_scene_remove_password_write.c` — `nfc_tools_remove_password_worker()`,
  Zeile ~205–213.
- `scenes/other/nfc_tools_scene_nfc_commands_run.c` — `nfc_commands_worker()`, Zeile ~256–264.
- `scenes/other/nfc_tools_scene_lock_tag.c` — `nfc_tools_lock_tag_worker()`, Zeile ~179–187.

Alle vier nutzen für den Fix die bereits vorhandene Konstante `NTS_ERR_TAG_CONTACT` ("Tag contact
error") aus `include/nfc_tools_strings.h:140` — dieselbe, die die zugehörigen Callbacks selbst
bereits für den strukturell nächstliegenden Fall verwenden (Poller lief, erreichte aber nie
`Iso14443_3aPollerEventTypeReady`/`Iso14443_4aPollerEventTypeReady`). Ein Wait-Timeout, bei dem der
Callback überhaupt nicht lief, ist dieselbe Grundsituation — die zweite RF-Sitzung konnte nicht
abgeschlossen werden —, nur eine Stufe früher. Keine neue API, kein neuer String nötig.

(Bewusst **nicht** mit portiert: `NTS_ERR_NO_TAG` wäre hier semantisch ungenau, da diese Zeichen-
kette in denselben Dateien bereits für den Phase-1-Scanner-Timeout reserviert ist, "kein Tag
gefunden" — im hier behandelten Fall wurde der Tag aber bereits gefunden und dann verloren.)

## Risiko
Gering. Reine Meldungskorrektur nach demselben, im Cluster inzwischen sechsfach etablierten Muster
(Wait-Rückgabewert prüfen, bei Timeout eine passende Fehlerkonstante setzen) — keine neue
Hardware-Interaktion, keine Änderung an Frame-Aufbau, Auth-Ablauf oder Poller-Ansteuerung.
Einziger Verhaltensunterschied: der zuvor leere Popup-Text im (seltenen) Timeout-Fall wird durch
"Tag contact error" ersetzt; alle Fälle, in denen der Poller tatsächlich ein Ereignis liefert,
bleiben unverändert (Host-Repro als Regressionsfall bestätigt für alle vier Dateien, da
strukturell identisch).

## Port-Skizze / Fix
Am Beispiel `lock_tag.c` (die anderen drei Dateien analog, jeweils mit ihrem eigenen
Kontext-Variablennamen: `write_ctx` in `set_password_write.c`/`remove_password_write.c`, `cmd_ctx`
in `nfc_commands_run.c`):

```c
uint32_t wait_result = furi_event_flag_wait(lock_ctx.done, 1u, FuriFlagWaitAny, 8000);

nfc_poller_stop(poller);
nfc_poller_free(poller);
furi_event_flag_free(lock_ctx.done);

if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

if(wait_result != 1u) {
    // Poller callback never fired within the timeout: the tag was detected
    // by the scanner phase but lost (or never reached) before the second RF
    // session completed. Report it like the callback's own Ready-check
    // failure does, instead of falling through with an empty err[].
    strlcpy(lock_ctx.err, NTS_ERR_TAG_CONTACT, sizeof(lock_ctx.err));
}

if(lock_ctx.success) {
    ...
} else {
    notification_message(app->notifications, &sequence_error);
    furi_string_set(app->info_str, lock_ctx.err);
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
}
```

Die `if(wait_result != 1u)`-Zeile greift ausschließlich dann, wenn der Callback nie lief (einzige
Quelle für das `done`-Signal ist `furi_event_flag_set(ctx->done, 1u)`, das in jedem erreichbaren
Codepfad jedes der vier Callbacks vor dessen Rückkehr aufgerufen wird) — sie überschreibt also nie
eine bereits vom Callback gesetzte, echte Fehlermeldung.

Für `nfc_commands_run.c` identisch, nur mit `cmd_ctx.done`/`cmd_ctx.err` statt `lock_ctx.done`/
`lock_ctx.err`; für `set_password_write.c`/`remove_password_write.c` identisch mit
`write_ctx.done`/`write_ctx.err`.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-second-wait-timeout/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware (je Datei einzeln): Fix einbauen
(`ufbt launch_app` gegen `nfctools`), mit einer eigenen NTAG213/215/216-Testkarte (für
`set_password_write`/`remove_password_write`/`lock_tag`) bzw. einer beliebigen unterstützten Karte
(für `nfc_commands_run`) die jeweilige Funktion wählen. Regressionsfall: Karte durchgehend stabil
an der Antenne halten — Erfolgs- bzw. reale Fehlermeldung muss wie vor dem Fix erscheinen.
Fehlerfall: Karte kurz nach dem Verlassen des Scan-Popups (nach der Erkennungsphase, vor Abschluss
der zweiten RF-Sitzung) wieder von der Antenne nehmen — vor dem Fix erscheint "Failed" mit leerem
Text, nach dem Fix "Failed\nTag contact error". Da das Zeitfenster klein ist, ggf. mehrere Versuche
mit gezieltem Timing nötig; alternativ probeweise die 8000-ms-Konstante in einer lokalen Testkopie
temporär verkürzen, um den Fall zuverlässiger zu treffen (nicht Teil des eigentlichen Fixes).

## Zusätzlich geprüft, kein Fund
- `set_password_write.c`/`remove_password_write.c`: NTAG21x-Seitenadress-Tabelle
  (`ntag21x_get_pages()`), MD5-Berechnung über `app->ndef_buf1`, Auth-/Schreibreihenfolge — beide
  Dateien bereits in einer früheren Sitzung (Remove-Password-Reihenfolgen-Fund) detailliert
  geprüft, hier nur die zweite Wait-Stelle neu untersucht.
- `nfc_commands_run.c`: `hex_parse()`-Grenzprüfung (`count >= max_out`), `format_result()`-
  Pufferzugriffe (`rsp_len` stets ≤ `sizeof(ctx->rsp)` = 258, vor jedem Zugriff geklemmt),
  `protocol_is_iso4a()`-Routing — bereits in einer früheren Sitzung
  (`firmware-mining/nfctools-commands-missing-protocol-guard.md`) detailliert geprüft, hier nur
  die zweite Wait-Stelle neu untersucht.
- `lock_tag.c`: Bytefolge der statischen/dynamischen Lock-Bits gegen die NTAG21x-Datenblatt-Tabelle
  (Kommentarblock Zeile 8–21) nachgerechnet, `read_data.page[0].data[0..1]`-Zugriff durch
  vorherige erfolgreiche `mf_ultralight_poller_read_page()`-Rückgabe abgesichert — kein
  Speicherzugriffsfehler.

## Quelle
`github.com/wakdev/nfctools-fz`, Branch `main` (per `git clone --depth 1`), Dateien
`scenes/other/nfc_tools_scene_set_password_write.c`,
`scenes/other/nfc_tools_scene_remove_password_write.c`,
`scenes/other/nfc_tools_scene_nfc_commands_run.c`, `scenes/other/nfc_tools_scene_lock_tag.c`,
`include/nfc_tools_strings.h`.
