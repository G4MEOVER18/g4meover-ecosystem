# ProtoPirate — PSA-Bruteforce: Signal-Info-Refresh im Sub-Decode-Kontext kann bei Fehlschlag ausbleiben

## Was

Ersten Schritt in den bislang zurückgestellten Rückstellpunkt "PSA-Bruteforce-Kernel"
gemacht — bewusst nicht an `protocols/psa_crypto_bf.c`/`psa_bf_core.c` selbst (die dort
verwendeten Krypto-Konstanten sind reverse-engineert und ohne verifizierte
Referenzimplementierung nicht risikofrei zu bewerten, siehe frühere Zurückstellung), sondern
an der reinen Orchestrierungs-/Zustandsautomat-Schicht drumherum:
`scenes/plugins/protopirate_psa_bf_plugin.c` (378 Zeilen, dynamisch geladenes Plugin, das
den Bruteforce-Hintergrund-Thread startet/überwacht) plus `helpers/protopirate_psa_bf_host.c`
und `scenes/protopirate_scene_sub_decode.c` als Aufrufkontext. Reine Zustands-/
Ereignisreihenfolge-Logik, unabhängig von der Kryptokorrektheit vollständig host-seitig
verifizierbar.

Der Bruteforce läuft in einem eigenen Hintergrund-Thread (`psa_brute_force_thread_entry()`
in `psa_crypto_bf.c`): `psa_brute_force_run()` setzt `state->status` auf einen Endwert
(`PSA_BF_STATUS_FOUND`/`_NOT_FOUND`/`_CANCELLED`) und kehrt zurück, **danach** ruft die
Thread-Eintrittsfunktion `state->on_done(state->on_done_ctx)` auf — das ist
`psa_bf_done_cb()`, die ein `ProtoPirateCustomEventPsaBruteforceComplete` an den
View-Dispatcher schickt. Der Endzustand ist also für andere Threads bereits sichtbar
(`volatile uint8_t status`), bevor das zugehörige Event überhaupt verschickt wird.

Parallel dazu läuft im GUI-Thread ein periodischer Tick, der ebenfalls prüft, ob der
Bruteforce fertig ist:

```c
// scenes/protopirate_scene_sub_decode.c, Tick-Handler
if(protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin &&
   app->psa_bf_plugin->is_running(app) &&
   app->psa_bf_plugin->on_scene_event(app, ProtoPiratePsaBfContextSubDecode, event)) {
    return consumed;
}
```

```c
// scenes/plugins/protopirate_psa_bf_plugin.c, plugin_on_scene_event()
if(event.type == SceneManagerEventTypeTick) {
    if(g_bf_thread && g_bf_state) {
        uint8_t bfst = g_bf_state->status;
        if(bfst == PSA_BF_STATUS_IDLE || bfst == PSA_BF_STATUS_RUNNING) {
            show_bf_progress(app);
        } else {
            bf_finish_and_show_result(app, NULL);   // <-- kein Refresh-Aufruf hier
        }
        return true;
    }
    return false;
}
```

Es gibt also zwei voneinander unabhängige Wege, die denselben Statuswechsel bemerken und
`bf_finish_and_show_result()` aufrufen können — den periodischen Tick (der `status` direkt
abfragt, sobald `g_bf_thread` noch gesetzt ist) und den `ProtoPirateCustomEventPsaBruteforceComplete`-
Custom-Event-Handler (der auf das vom Thread verschickte Event wartet). Welcher der beiden
zuerst durch die View-Dispatcher-Nachrichtenschleife verarbeitet wird, ist eine echte
Wettlaufsituation ohne Synchronisation zwischen beiden Pfaden — abhängig davon, ob der
Hintergrund-Thread sein Event bereits verschickt hat, wenn der nächste Tick fällig wird.

Der Sub-Decode-spezifische Refresh-Aufruf sitzt aber **nur** im Custom-Event-Zweig, nicht in
`bf_finish_and_show_result()` selbst:

```c
// scenes/plugins/protopirate_psa_bf_plugin.c
if(event.event == ProtoPirateCustomEventPsaBruteforceComplete) {
    if(g_bf_state) {
        bf_finish_and_show_result(app, NULL);
        if(g_active_ctx == ProtoPiratePsaBfContextSubDecode) {
            g_host_api->subdecode_signal_info_refresh(app);   // nur hier
        }
    }
    return true;
}
```

`bf_finish_and_show_result()` gibt `g_bf_state` bei `NOT_FOUND`/`CANCELLED` sofort frei und
setzt den Zeiger auf `NULL` — genau die beiden Fälle, in denen kein "Gefunden"-Ergebnis zum
späteren Verwerfen aufgehoben werden muss (der `FOUND`-Fall hält `g_bf_state` bewusst am
Leben, bis der Nutzer das Ergebnis quittiert). Gewinnt jetzt der Tick das Wettrennen (er ruft
`bf_finish_and_show_result()` zuerst auf, weil `g_bf_thread` zu diesem Zeitpunkt noch gesetzt
ist), wird `g_bf_state` bei `NOT_FOUND`/`CANCELLED` bereits dort freigegeben. Wenn danach das
inzwischen vom Worker-Thread verschickte `ProtoPirateCustomEventPsaBruteforceComplete`
verarbeitet wird, greift dessen `if(g_bf_state)`-Wächter nicht mehr (`g_bf_state == NULL`) —
der komplette Block inklusive des `subdecode_signal_info_refresh()`-Aufrufs wird
übersprungen. `protopirate_subdecode_psa_bf_complete_refresh()`
(`scenes/protopirate_scene_sub_decode.c`) ist aber die einzige Stelle, die den
Sub-Decode-Zustandsautomaten (`ctx->state`) nach einem Bruteforce-Versuch zurück auf
`DecodeStateShowSignalInfo` setzt und ein `ProtoPirateCustomEventSubDecodeUpdate` zum
Neuaufbau der Empfänger-Statuszeile (Frequenz/Modulation/Verlaufsstatistik über
`protopirate_scene_sub_decode_update_receiver_statusbar()`) anstößt — bleibt dieser Aufruf
aus, bleibt der Sub-Decode-Kontext im zuvor aktiven Zustand hängen, ohne dass die
Statuszeile den soeben abgeschlossenen (erfolglosen) Bruteforce-Versuch nachträgt, bis eine
andere Aktion den Zustand erneut anstößt.

Das eigentliche Bruteforce-Ergebnis-Widget selbst ("Not found"/"Cancelled") wird davon nicht
berührt — das zeigt bereits der Tick-ausgelöste `bf_finish_and_show_result()`-Aufruf korrekt
an (`show_bf_result()` läuft unabhängig vom Refresh-Aufruf). Betroffen ist ausschließlich der
Sub-Decode-Szenenzustand/die Empfänger-Statuszeile, die für den Rest der Sitzung auf dem
Stand vor dem Bruteforce-Versuch hängen bleibt, bis eine andere Aktion (z. B. erneutes
Öffnen der Signal-Info) den Zustand von sich aus neu synchronisiert.

Beim erfolgreichen Fall (`FOUND`) tritt das Problem nicht auf: `g_bf_state` bleibt dort in
jedem Fall am Leben, der spätere Custom-Event-Aufruf findet `g_bf_state` also immer noch vor
und feuert den Refresh trotzdem (dafür doppelt, siehe "Risiko" — bereits heute so, keine
Regression durch den Fix).

## Nutzen

Schließt eine durch reine Ereignis-Ankunftsreihenfolge ausgelöste Zustandsinkonsistenz im
einzigen bislang unangetasteten Teil des PSA-Bruteforce-Rückstellpunkts, ohne die als riskant
eingestufte Kryptografie selbst anzufassen. Macht das Verhalten nach einem
`NOT_FOUND`/`CANCELLED`-Bruteforce-Versuch im Sub-Decode-Kontext deterministisch statt von
Thread-Timing abhängig.

## Betroffene Dateien

- `scenes/plugins/protopirate_psa_bf_plugin.c` — `bf_finish_and_show_result()`,
  `plugin_on_scene_event()` (Custom-Event-Zweig `ProtoPirateCustomEventPsaBruteforceComplete`).
  Einziger Fix-Ort.

## Patch

```diff
 static void bf_finish_and_show_result(void* app, ButtonCallback result_callback) {
     if(!g_bf_state) return;

     PsaBfState* s = g_bf_state;
     uint8_t status = s->status;

     if(g_bf_thread) {
         furi_thread_join(g_bf_thread);
         furi_thread_free(g_bf_thread);
         g_bf_thread = NULL;
     }

     if(status == PSA_BF_STATUS_FOUND) {
         apply_success_to_history(app, s);
         if(g_active_ctx == ProtoPiratePsaBfContextSubDecode) {
             g_host_api->notification_success(app);
         }
         ButtonCallback ok_cb = result_callback;
         if(!ok_cb && g_active_ctx == ProtoPiratePsaBfContextReceiverInfo) {
             ok_cb = bf_result_ok_callback;
         }
         show_bf_result(app, status, ok_cb);
     } else {
         free(g_bf_state);
         g_bf_state = NULL;
         show_bf_result(app, status, NULL);
     }
+
+    if(g_active_ctx == ProtoPiratePsaBfContextSubDecode) {
+        g_host_api->subdecode_signal_info_refresh(app);
+    }
 }
```

```diff
     if(event.event == ProtoPirateCustomEventPsaBruteforceComplete) {
         if(g_bf_state) {
             bf_finish_and_show_result(app, NULL);
-            if(g_active_ctx == ProtoPiratePsaBfContextSubDecode) {
-                g_host_api->subdecode_signal_info_refresh(app);
-            }
         }
         return true;
     }
```

Verlagert den Refresh-Aufruf von der bislang einzigen, nicht immer erreichten Aufrufstelle
(Custom-Event-Zweig) in `bf_finish_and_show_result()` selbst — die Funktion, die *tatsächlich*
in jedem Fall ausgeführt wird, unabhängig davon, ob der Tick- oder der Custom-Event-Pfad das
Wettrennen gewinnt. Keine neue API, keine geänderte Signatur, keine neuen Felder.

## Risiko

**Sehr gering.** Reine Verschiebung eines bereits vorhandenen, bedingten Aufrufs an eine
Stelle, die garantiert (statt nur manchmal) erreicht wird, sobald `g_bf_state` zu Beginn des
jeweiligen Aufrufs noch existiert. Keine Änderung an der Kryptografie, keine neue
Speicherverwaltung. Einzige beobachtbare Verhaltensänderung gegenüber dem Status quo: im
`FOUND`-Fall, wenn der Tick das Wettrennen gewinnt, feuert der Refresh jetzt zweimal
(einmal aus dem Tick-ausgelösten `bf_finish_and_show_result()`, einmal aus dem später
verarbeiteten Custom-Event, das `g_bf_state` dort weiterhin vorfindet, weil es im
`FOUND`-Fall bewusst nicht freigegeben wird) — das ist bereits heute so (der Custom-Event-Zweig
ruft den Refresh im `FOUND`-Fall so oder so auf, unabhängig vom Fix), also keine Regression.
`protopirate_subdecode_psa_bf_complete_refresh()` selbst ist idempotent (setzt denselben
Zustand, verschickt dasselbe Update-Event), ein doppelter Aufruf hat keinen sichtbaren Effekt
außer einem zusätzlichen, redundanten Statuszeilen-Neuaufbau.

## Testhinweis

Rein logisch verifizierbar, keine Hardware nötig (siehe Host-Repro unten). Wer zusätzlich am
Gerät prüfen will: im Sub-Decode-Modus eine `.sub`-Datei mit einem PSA-128-Bit-Signal ohne
bekannte Seriennummer laden (Bruteforce-Vorbedingung), Bruteforce auslösen und **absichtlich
scheitern lassen** (z. B. per Zurück-Taste während des Laufs abbrechen, oder ein Signal mit
absichtlich falschem Schlüssel verwenden, sodass keine der beiden Bruteforce-Stufen
anschlägt) — vor dem Fix bleibt die Empfänger-Statuszeile teilweise auf dem Vorzustand hängen
(abhängig vom Thread-Timing, nicht bei jedem Versuch reproduzierbar); nach dem Fix wird sie
nach jedem Versuch zuverlässig aktualisiert. Da die auslösende Ereignisreihenfolge vom
Thread-Timing abhängt, ist der Host-Repro (der beide Reihenfolgen explizit erzwingt) der
zuverlässigere Nachweis.

Host-Repro: `repro-psa-bf-subdecode-refresh-race/repro.c`, baut die Zustandsautomatik von
`bf_finish_and_show_result()`/`plugin_on_scene_event()` (Tick- und Custom-Event-Zweig) 1:1
nach, unter `gcc -O1 -Wall -Wextra -fsanitize=address,undefined` gegen alle
2 (Ergebnis: `FOUND`/`NOT_FOUND`/`CANCELLED`, 3 Werte) × 2 (Kontext: `ReceiverInfo`/
`SubDecode`) × 2 (Ereignisreihenfolge: Tick zuerst/Custom-Event zuerst) = 12 Fälle geprüft:

```
[ALT] BUG confirmed: ctx=SubDecode status=NOT_FOUND order=TickFirst   -> refresh_calls=0 (expected >=1)
[ALT] BUG confirmed: ctx=SubDecode status=CANCELLED order=TickFirst   -> refresh_calls=0 (expected >=1)

--- summary over 12 (outcome x context x order) cases ---
ALT: SubDecode cases missing the refresh call: 2 (expected: 2 -- NOT_FOUND and CANCELLED, both only when Tick wins)
ALT: ReceiverInfo regression violations (refresh leaked into wrong context): 0 (expected: 0)
NEW: SubDecode cases still missing the refresh call: 0 (expected: 0)
NEW: ReceiverInfo regression violations: 0 (expected: 0)

OK: ALT reproduces the skipped-refresh bug in exactly the predicted 2 cases, NEW fixes all of them without leaking the SubDecode-only refresh into ReceiverInfo, no double-free/leak in any of the 12 scenarios.
```

ALT (aktueller Code) verliert den Refresh exakt in den beiden vorhergesagten Fällen
(`NOT_FOUND`/`CANCELLED` im Sub-Decode-Kontext, wenn der Tick das Wettrennen gewinnt); NEU
(Fix) liefert den Refresh in allen 6 Sub-Decode-Fällen zuverlässig, ohne ihn im
`ReceiverInfo`-Kontext auszulösen (0 Regressionsverstöße in beiden Varianten). Zusätzlich in
allen 24 simulierten Durchläufen (ALT+NEU) auf Doppel-Freigabe/Speicherleck des
`PsaBfState` geprüft — keiner der beiden Pfade zeigt eine Abweichung.

## Zusätzlich geprüft, kein Fund

- `helpers/protopirate_psa_bf_host.c` (193 Zeilen, Plugin-Lade-/Entlade-Infrastruktur:
  `protopirate_psa_bf_plugin_ensure_loaded()`, `_unload_if_idle()`, `_context_release()`)
  vollständig gelesen — durchgehend gegen `NULL` abgesichert (Resolver-/Manager-Allokation,
  Plugin-Einstiegspunkt-Prüfung `plugin->set_host_api && plugin->needs_bruteforce &&
  plugin->on_scene_event` vor jeder Nutzung), sauberer Abbau in umgekehrter
  Allokationsreihenfolge bei jedem Fehlerzweig, kein Fund.
- `protopirate_psa_bf_plugin.c` im Übrigen (Back-Event-Behandlung, Cancel-Handling,
  `start_bruteforce()`, `item_needs_bruteforce_from_ff()`): `result_callback`-Parameter von
  `bf_finish_and_show_result()` wird an allen drei tatsächlichen Aufrufstellen im gesamten
  Quellbestand ausschließlich mit `NULL` aufgerufen — dadurch bleibt die
  Erfolgs-Bildschirm-Schaltfläche im `ReceiverInfo`-Kontext bei jedem Aufruf identisch (über
  die vorhandene `!ok_cb && ctx == ReceiverInfo`-Ersatzlogik), unabhängig davon, welcher der
  beiden Pfade zuerst auffährt — anfänglich vermuteter zweiter Fund ("Schaltfläche
  verschwindet nach Doppelaufruf") beim Nachrechnen als nicht zutreffend verworfen, kein
  Fund.
- `protocols/psa.c` (1443 Zeilen, eigentlicher PSA-Protokoll-Decoder/-Encoder, nicht Teil des
  Bruteforce-Kernels) sowie `protocols/psa_crypto.c` (95 Zeilen, TEA-Puffer-Auf-/Abbau) in
  dieser Sitzung nicht geprüft — für eine künftige Sitzung offen.

## Quelle

Eigene Quelltextprüfung von `G4MEOVER18/ProtoPirate` (eigener Fork/Stand, `e3d9da5`):
`scenes/plugins/protopirate_psa_bf_plugin.c`, `helpers/protopirate_psa_bf_host.c`,
`scenes/protopirate_scene_sub_decode.c`, `protocols/psa_bf_core.c`,
`protocols/psa_crypto_bf.c`, `protocols/psa_bf_types.h` — reiner Logikfund im eigenen Code,
kein externer Quellenvergleich nötig.
