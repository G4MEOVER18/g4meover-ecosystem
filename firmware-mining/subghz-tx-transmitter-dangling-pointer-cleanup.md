# SubGHz TX: `instance->transmitter` bleibt nach fehlgeschlagenem Sendeversuch ein freigegebener, nicht zurückgesetzter Zeiger

## Was

Ausgangspunkt war der Unleashed-`dev`-Changelog-Eintrag zu PR #1104 ("Sending a receive-only
signal from the mobile app or qFlipper no longer crashes the Flipper"): dort wurde
`subghz_transmitter_free(NULL)` (Absturz über `furi_check()`) für Protokolle ohne Encoder
behoben, **und** zusätzlich vermerkt, dass "the transmitter pointer is cleared after every
teardown instead of being left dangling".

Den ersten Teil (Absturz bei `NULL`) geprüft: bei mir (mntm-012,
`applications/main/subghz/helpers/subghz_txrx.c::subghz_txrx_tx_start()`) ist der
NULL-Fall bereits durch ein bestehendes `if(instance->transmitter)` vor jedem
`subghz_transmitter_free()`-Aufruf abgesichert — dieser konkrete Absturz tritt bei mir nicht
auf, mein Codestand ist hier bereits sicher.

Den zweiten Teil ("Zeiger wird nach jedem Teardown geleert") geprüft und tatsächlich fehlend
gefunden: im Fehlerpfad von `subghz_txrx_tx_start()` — erreicht u. a. bei
`SubGhzTxRxStartTxStateErrorOnlyRx` (Transmitter erfolgreich alloziert/deserialisiert, aber
Regionssperre/keine Sendefreigabe, z. B. ausgelöst über den RPC-Pfad aus
`subghz_scene_rpc.c`) — wird `instance->transmitter` freigegeben, aber **nicht** auf `NULL`
gesetzt:

```c
// applications/main/subghz/helpers/subghz_txrx.c (mntm-012, unverändert)
if(ret != SubGhzTxRxStartTxStateOk) {
    if(instance->transmitter) subghz_transmitter_free(instance->transmitter);
    if(instance->txrx_state != SubGhzTxRxStateIDLE) {
        subghz_txrx_idle(instance);
    }
}
```

`instance->transmitter` zeigt danach auf bereits freigegebenen Heap-Speicher, ist aber selbst
weiterhin ungleich `NULL` — jeder spätere Zugriffspfad, der sich (wie der bestehende
`subghz_transmitter_get_protocol_instance()`, der nur `furi_check(instance)` — also
ausschließlich Non-NULL — prüft, nie die tatsächliche Gültigkeit) allein auf
`if(instance->transmitter)` bzw. einen Non-NULL-Zeiger verlässt, würde freigegebenen Speicher
lesen/beschreiben.

**Wichtig, ehrlich eingeordnet:** ich habe den vollständigen aktuellen Aufrufgraphen von
`instance->transmitter` in `subghz_txrx.c` durchsucht (drei Leser:
`subghz_transmitter_stop()`/`subghz_transmitter_free()` in `subghz_txrx_tx_stop()`, geschützt
durch `furi_assert(instance->txrx_state == SubGhzTxRxStateTx)`, sowie
`subghz_txrx_set_raw_file_encoder_worker_callback_end()`) und den einzigen Aufrufer des
letzteren (`subghz_scene_read_raw.c`) geprüft: dieser ruft den Setter nachweislich nur im
Erfolgszweig von `subghz_tx_start()` auf. Über keinen mir bekannten, heute existierenden
Aufrufpfad wird der dangling Zeiger tatsächlich gelesen — dies ist **kein** heute auslösbarer
Absturz/UAF, sondern eine Haltungs-/Konsistenzlücke gegenüber dem bereits vom Upstream-Projekt
selbst als Teil desselben Fixes behobenen Zustand. Das Repro (siehe unten) bildet stattdessen
exakt das Zugriffsmuster nach, das der bestehende `subghz_txrx_set_raw_file_encoder_worker_callback_end()`-Aufrufer
bereits verwendet (Zugriff allein über Non-NULL-Prüfung, keine `txrx_state`-Prüfung), um zu
zeigen, wovor die Portierung tatsächlich schützt — sollte dieser oder ein künftiger Aufrufer
einmal ohne zusätzliche `txrx_state`-Prüfung direkt nach einem fehlgeschlagenen Sendeversuch
aufgerufen werden.

## Nutzen

Rein additive, risikofreie Konsistenz-Haltung: gleicht meinen Code an das bereits vom
Upstream-Projekt selbst umgesetzte Muster an ("Zeiger nach jedem Teardown leeren"), das an der
Schwesterstelle direkt darunter (`subghz_txrx_tx_stop()`) bereits so gehandhabt wird
(`subghz_transmitter_free(instance->transmitter); instance->transmitter = NULL;` — dort aber
mit einem `furi_assert(instance->txrx_state == SubGhzTxRxStateTx)` davor). Der Fehlerpfad in
`subghz_txrx_tx_start()` ist die einzige Stelle in dieser Datei, die einen Free ohne
anschließendes Nullsetzen durchführt — nach dem Fix ist das Verhalten an allen drei Free-Stellen
einheitlich. Schützt jeden künftigen Aufrufer, der (wie der bereits bestehende
`subghz_txrx_set_raw_file_encoder_worker_callback_end()`) `instance->transmitter` ohne
zusätzliche `txrx_state`-Prüfung liest, vor einem Use-after-Free.

## Betroffene Dateien

- `applications/main/subghz/helpers/subghz_txrx.c` — `subghz_txrx_tx_start()`, Fehlerpfad
  (eine Zeile ergänzt zu drei Zeilen, keine andere Stelle betroffen).

## Risiko

Minimal. Rein additiv (eine zusätzliche Zuweisung `instance->transmitter = NULL;` direkt nach
dem bereits bestehenden `subghz_transmitter_free()`-Aufruf im selben `if`-Zweig), ändert keine
bestehende Funktionssignatur, keinen Kontrollfluss, keine andere Codezeile. Identisch zum
Muster, das dieselbe Datei an der Schwesterstelle `subghz_txrx_tx_stop()` bereits verwendet.

## Port-Skizze / Patch

Unified-Diff in
`firmware-mining/subghz-tx-transmitter-dangling-pointer-cleanup/subghz_txrx_c.patch` — `git
apply --check` vorab gegen einen frischen `mntm-012`-Klon verifiziert, real angewendet und
byte-für-byte mit der beabsichtigten Fassung abgeglichen:

```diff
         if(ret != SubGhzTxRxStartTxStateOk) {
-            if(instance->transmitter) subghz_transmitter_free(instance->transmitter);
+            if(instance->transmitter) {
+                subghz_transmitter_free(instance->transmitter);
+                instance->transmitter = NULL;
+            }
             if(instance->txrx_state != SubGhzTxRxStateIDLE) {
                 subghz_txrx_idle(instance);
             }
         }
```

## Host-Repro

`firmware-mining/repro-subghz-tx-transmitter-dangling-pointer-cleanup/repro.c` — bildet
`struct SubGhzTransmitter`, `subghz_transmitter_free()`,
`subghz_transmitter_get_protocol_instance()` sowie den Fehlerpfad aus
`subghz_txrx_tx_start()` 1:1 nach, dazu einen nachgebauten Aufrufer, der exakt das
Zugriffsmuster von `subghz_txrx_set_raw_file_encoder_worker_callback_end()` verwendet (Zugriff
allein über Non-NULL-Prüfung). Je ein isolierter Kindprozess (`fork()`/`waitpid()`) für
ALT/NEU, da der UAF-Fall unter ASan den Prozess hart beendet.

```
gcc -std=c11 -O0/-O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
```

Ergebnis (beide Optimierungsstufen): **ALT** stürzt zuverlässig mit
`AddressSanitizer: heap-use-after-free` ab (Lesezugriff auf den bereits freigegebenen
`protocol_instance`-Block), **NEU** beendet sich sauber (Zugriff wird durch die
`NULL`-Prüfung kontrolliert übersprungen) — **ALLE CHECKS BESTANDEN**, keine
Compiler-Warnungen.

## Quellen

- Unleashed-Firmware (`DarkFlippers/unleashed-firmware`, `dev`), `CHANGELOG.md`, Eintrag
  "SubGhz: Sending a receive-only signal from the mobile app or qFlipper no longer crashes
  the Flipper" (PR #1104, by @MNeroba).
- Eigener Stand: `Next-Flip/Momentum-Firmware`, Tag `mntm-012`,
  `applications/main/subghz/helpers/subghz_txrx.c`,
  `applications/main/subghz/scenes/subghz_scene_rpc.c`,
  `applications/main/subghz/scenes/subghz_scene_read_raw.c`, `lib/subghz/transmitter.c` (alle
  per `raw.githubusercontent.com` sowie per flachem `git clone --branch mntm-012` direkt
  bezogen).
