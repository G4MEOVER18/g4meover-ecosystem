# lora-ukfe — Katalog-Trigger sendet einen zu kurzen Frame (delay_ms fehlt)

## Was
Weiterverfolgt von `command_catalog.h`/den restlichen `scenes/`-Dateien, dem im
letzten `lora-ukfe`-Bericht offen vermerkten Rückstellpunkt. `command_catalog.h`
selbst ist reine Daten (Kategorie→Funktion→`ukfe_rf`-Command-Tabelle), aber ihr
Konsument in `scenes/scene_menu.c`, `send_fn()`, hat einen Fund:

```c
static void send_fn(LораUkfeApp* app, const UkfeFn* fn) {
    UkfeRfMessage m;
    m.cmd = fn->cmd;
    m.arg_len = fn->has_arg ? 1 : 0;
    if(fn->has_arg) m.args[0] = fn->arg;
    ukfe_uart_send_rf(app, &m);
}
```

Für **jeden** Katalog-Eintrag mit `has_arg == true` baut `send_fn()` pauschal
einen Frame mit genau 1 Arg-Byte. Das passt für `UkfeRfCmdBeaconSpam`,
`UkfeRfCmdBleSpam`, `UkfeRfCmdEvilPortal`, `UkfeRfCmdHidPayload`,
`UkfeRfCmdHidDucky`, `UkfeRfCmdJammer` und `UkfeRfCmdPayloadRun` — bei allen
sieben verlangt der Kommentar in `rf/ukfe_rf.h` genau 1 Argument-Byte. Für den
achten `has_arg`-Eintrag, `FN_PAYLOAD[0]` = `{"Trigger [V3]", UkfeRfCmdTrigger,
0, true}`, stimmt das nicht: `rf/ukfe_rf.h:24` dokumentiert
`UkfeRfCmdTrigger = 0x02, // args: uint8 id, uint32 delay_ms (LE)` — 5 Byte,
nicht 1. Wählt man im Menü (Kategorie "Payloads") den Eintrag "Trigger [V3]",
baut `send_fn()` trotzdem nur `arg_len = 1, args[0] = 0` und schickt das über
`ukfe_uart_send_rf()` als vollständig gültigen `ukfe_rf`-Frame (MAC/CRC stimmen,
`ukfe_rf_parse_frame()` nimmt ihn anstandslos an) — nur fehlen die 4
`delay_ms`-Bytes komplett auf der Leitung, still, ohne jede Fehlermeldung. Ein
Empfänger, der dem in `ukfe_rf.h` selbst dokumentierten Format für
`UkfeRfCmdTrigger` folgt, kann `delay_ms` aus diesem Frame gar nicht
rekonstruieren.

Bemerkenswert dabei: die korrekte Baufunktion existiert bereits —
`ukfe_rf_make_trigger()` (`rf/ukfe_rf.c:125`, baut exakt den 5-Byte-Frame) —,
wird aber im gesamten Baum an keiner einzigen Stelle aufgerufen (per
`grep -rn ukfe_rf_make_trigger` verifiziert: nur die eigene Definition und der
Prototyp in `rf/ukfe_rf.h`). Derselbe Musterfund wie schon bei
`rf_comm_counter()` (dokumentiert als "für Anzeige/Persistenz" vorgesehen, aber
nie aufgerufen) und beim `UkfeRfRespPayload`-Dispatch-Fund aus den letzten
beiden Sitzungen: eine bereits fertig geschriebene, korrekte Funktion bleibt
unverdrahtet, während ein generischer Pfad daneben etwas Falsches tut.

## Nutzen
- Der Katalog-Menüpunkt "Trigger [V3]" sendet nach dem Fix einen tatsächlich
  protokollkonformen `UkfeRfCmdTrigger`-Frame (5 statt 1 Arg-Byte), den ein
  Empfänger, der dem in `rf/ukfe_rf.h` dokumentierten Format folgt, korrekt
  auswerten kann.
- Rein additiver Sonderfall: ein `if(fn->cmd == UkfeRfCmdTrigger)`-Zweig vor dem
  bestehenden generischen Pfad in `send_fn()`. Die anderen sieben
  `has_arg`-Einträge sowie alle `has_arg == false`-Einträge (Status, Ping,
  Abort, Reboot, WiFi-/BLE-/Drohnen-/LoRaWAN-Scans usw.) durchlaufen weiterhin
  exakt denselben, unveränderten generischen Pfad.
- Nutzt die bereits vorhandene, bislang unbenutzte `ukfe_rf_make_trigger()`
  statt eine zweite, parallele 5-Byte-Bau-Logik einzuführen — keine neue
  Kodierlogik, nur die richtige, schon vorhandene Funktion an der richtigen
  Stelle aufgerufen.
- `delay_ms = 0` (sofort auslösen) im Fix ist dieselbe Konvention wie beim
  bereits produktiv genutzten `ukfe_uart_send_trigger(app, id, 0)` in
  `scenes/scene_payload_list.c`s `payload_callback()` — der Katalog-Menüpunkt
  bekommt dadurch dasselbe Sofort-Auslösen-Verhalten wie der bestehende
  Payload-Listen-Pfad, nur eben binär über `ukfe_rf` statt über die
  JSON-Kommandozeile.

## Betroffene Dateien
- `scenes/scene_menu.c` — `send_fn()`: ein neuer `if`-Zweig vor dem bestehenden
  generischen Pfad.

Keine Änderung an `command_catalog.h` (der Katalog-Eintrag selbst — Name, Cmd,
`arg`, `has_arg` — bleibt unverändert, das `arg`-Feld liefert weiterhin die
`id`), an `rf/ukfe_rf.c`/`.h` (nutzt nur die dort bereits vorhandene
`ukfe_rf_make_trigger()`) oder an einer anderen Scene.

## Patch
```c
static void send_fn(LораUkfeApp* app, const UkfeFn* fn) {
    UkfeRfMessage m;
    if(fn->cmd == UkfeRfCmdTrigger) {
        // UkfeRfCmdTrigger (rf/ukfe_rf.h:24) braucht 5 Byte (uint8 id, uint32
        // delay_ms LE), nicht das generische 1-Byte-Schema unten -- baut den
        // Frame mit der dafuer bereits vorhandenen, bislang ungenutzten
        // ukfe_rf_make_trigger() (rf/ukfe_rf.c:125). delay_ms=0 = sofort
        // ausloesen, analog zum bereits genutzten
        // ukfe_uart_send_trigger(app, id, 0) in scene_payload_list.c.
        ukfe_rf_make_trigger(&m, fn->arg, 0);
    } else {
        m.cmd = fn->cmd;
        m.arg_len = fn->has_arg ? 1 : 0;
        if(fn->has_arg) m.args[0] = fn->arg;
    }
    ukfe_uart_send_rf(app, &m);
}
```

Einzige Änderung an `send_fn()`: der bisherige Funktionskörper wandert
unverändert in den `else`-Zweig, davor steht der neue `UkfeRfCmdTrigger`-Sonderfall.
Aufrufer (`menu_callback()`) und Signatur bleiben unverändert.

## Testhinweis (ohne Hardware)
Reine Framing-/Katalog-Logik (kein UART-, Funk- oder GUI-Zugriff), host-
kompilierbar nachvollziehbar. Anders als bei den bisherigen `lora-ukfe`-Funden
nutzt das Reproduktionsprogramm diesmal nicht nur eine 1:1-Nachbildung, sondern
den **echten, unveränderten** `rf/ukfe_rf.c`/`.h` (Commit `0dba410a`, verbatim
in dieses Verzeichnis kopiert — reine Protokoll-/Framing-Logik ohne
Flipper-/`furi`-Abhängigkeit, daher direkt host-kompilierbar), inklusive dem
bereits vorhandenen `ukfe_rf_make_trigger()`, echtem `ukfe_rf_build_frame()`/
`ukfe_rf_parse_frame()` mit MAC+CRC über ein Test-Secret:

```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined ukfe_rf.c repro.c -o repro && ./repro
```

in `repro-catalog-trigger-arg-format-mismatch/`. Vier Szenarien:

1. **BUG (ALT)**: `send_fn_old()` (1:1 der aktuelle `send_fn()`) baut für den
   Katalog-Eintrag `{"Trigger [V3]", UkfeRfCmdTrigger, 0, true}` einen Frame
   mit `arg_len = 1`. Über den echten `ukfe_rf_build_frame()` erzeugt das einen
   16-Byte-Frame; `ukfe_rf_parse_frame()` akzeptiert ihn anstandslos (MAC/CRC
   korrekt) — der Fehler ist auf reiner Protokollebene unsichtbar.
2. **FIX (NEU)**: `send_fn_new()` (mit dem `UkfeRfCmdTrigger`-Sonderfall) baut
   für denselben Katalog-Eintrag über `ukfe_rf_make_trigger()` einen Frame mit
   `arg_len = 5` (20 Byte, exakt 4 Byte mehr als ALT); nach `build`+`parse`
   werden `id` und `delay_ms = 0` korrekt aus dem Frame rekonstruiert.
3. **FIX, Rundlauf**: 7 `id`/`delay_ms`-Kombinationen direkt über
   `ukfe_rf_make_trigger()` (inkl. Randfälle `id ∈ {0, 255}`,
   `delay_ms ∈ {0, 1, 250, 5000, UINT32_MAX}`) jeweils über `build_frame()` +
   `parse_frame()` verlustfrei rundgelaufen.
4. **Regression**: ein 1-Byte-Eintrag (`UkfeRfCmdPayloadRun`, `arg=3`) und ein
   0-Byte-Eintrag (`UkfeRfCmdWifiScan`) liefern über `send_fn_old()` und
   `send_fn_new()` byteidentische `UkfeRfMessage`-Felder — der generische Pfad
   ist durch den Fix für alle anderen Katalog-Einträge unverändert.

Unter `gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined`: alle vier
Szenarien bestanden, keine ASan-/UBSan-Meldungen.

Funktionaler Test danach lokal auf dem Gerät: im Menü Kategorie "Payloads" →
"Trigger [V3]" auslösen, auf der Empfängerseite (Hub/Satellit) prüfen, dass ein
vollständiger 5-Byte-`UkfeRfCmdTrigger`-Frame ankommt (`id` und `delay_ms`
beide auswertbar) statt eines um 4 Byte zu kurzen Frames.

## Risiko
- Sehr niedrig: ein gezielter `if`-Sonderfall vor dem unveränderten generischen
  Pfad, keine Änderung an dessen Verhalten für die übrigen sieben
  `has_arg`-Einträge oder die `has_arg == false`-Einträge.
- Nutzt ausschließlich bereits vorhandenen, für genau diesen Zweck
  geschriebenen Code (`ukfe_rf_make_trigger()`) — keine neue Kodierlogik.
- Kein zusätzlicher Speicher-, Storage- oder Netzwerkzugriff; keine neue
  Zustandsvariable in `LораUkfeApp`.

## Offen (bewusst nicht mitbehoben)
- `fn->arg` ist für `FN_PAYLOAD[0]` im Katalog fest auf `0` gesetzt — das Menü
  erlaubt weiterhin keine Eingabe einer beliebigen Payload-`id` oder eines
  `delay_ms` über diesen Pfad (anders als `scene_payload_list.c`, die die reale
  `id` aus der vom Hub gemeldeten Payload-Liste übernimmt). Das wäre eine
  eigene, größere UI-Erweiterung (Zahlen-/Listeneingabe im Katalog-Menü) und
  nicht Teil dieses reinen Wire-Format-Fixes.
- Derselbe Musterfund am direkten Nachbar-Eintrag `FN_PAYLOAD[1]` =
  `{"Payload-Run [V3]", UkfeRfCmdPayloadRun, 0, true}`: `arg` ist ebenfalls
  fest auf `0`, das Wire-Format (1 Byte `idx`) stimmt hier zwar bereits, aber
  der Menüpunkt kann dadurch praktisch nur Payload-Index 0 auslösen — ein
  Bedienungs-/Vollständigkeitsproblem, kein Protokollfehler wie beim
  `Trigger`-Fund, bewusst nicht Teil dieses Fixes.
- `command_catalog.h` selbst bleibt reine, unveränderte Daten — der Fund liegt
  ausschließlich in ihrem Konsumenten (`send_fn()`).

## Quelle
- `G4MEOVER18/lora-ukfe` (main, `0dba410a`): `scenes/scene_menu.c`
  (`send_fn()`), `command_catalog.h` (`FN_PAYLOAD`-Tabelle), `rf/ukfe_rf.h`
  (`UkfeRfCmdTrigger`-Kommentar, `UkfeRfCmdPayloadRun`-Kommentar als Vergleich),
  `rf/ukfe_rf.c` (`ukfe_rf_make_trigger()`, `ukfe_rf_build_frame()`,
  `ukfe_rf_parse_frame()`), `scenes/scene_payload_list.c`
  (`payload_callback()` als Referenz für die `delay_ms=0`-Konvention).
