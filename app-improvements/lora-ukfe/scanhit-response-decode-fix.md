# lora-ukfe — RespScanHit (0x90) wird ungeprüft geloggt statt decodiert

## Was
Anschließend an den bereits behobenen `UkfeRfRespPayload`-Fund habe ich den zweiten,
im letzten Report vorgemerkten Response-Typ geprüft: `UkfeRfRespScanHit = 0x90`
("args: uint32 freq_or_ch, uint8 rssi", `rf/ukfe_rf.h:81`).

`handle_rf_response()` (`uart_comm.c`) behandelt `UkfeRfRespAck`, `UkfeRfRespStatus`,
`UkfeRfRespRelayed` und (seit dem letzten Fix) `UkfeRfRespPayload` explizit. Alles
andere — inklusive `UkfeRfRespScanHit` — fällt weiterhin in den generischen
`default`-Zweig:

```c
default:
    snprintf(line, sizeof(line), "[RX] cmd=0x%02X ctr=%lu alen=%u",
        m->cmd, (unsigned long)m->counter, m->arg_len);
    break;
```

Anders als beim `RespPayload`-Fund gibt es für `RespScanHit` (noch) keine eigene
Konsumenten-Scene — ich habe per `grep -rn "ScanHit\|0x90"` im gesamten Baum geprüft:
der Wert kommt nur in der Enum-Definition vor, nirgends sonst. Es gibt aktuell also
keinen Zustandsverlust wie bei der Payload-Liste, aber die eigentliche Nutzlast
(Frequenz/Kanal, RSSI) wird nie decodiert — jeder Treffer der fünf Scan-Befehle, die
laut `rf/ukfe_rf.h` `UkfeRfRespScanHit` als Trefferantwort verwenden
(`UkfeRfCmdLoraScan`, `UkfeRfCmdSubghzScan`, `UkfeRfCmdWifiScan`, `UkfeRfCmdBleScan`,
`UkfeRfCmdRidScan`), landet im Log nur als `[RX] cmd=0x90 ctr=... alen=...` — für die
eigentliche Auswertung (welche Frequenz/welcher Kanal, wie stark) wertlos, obwohl die
Werte im Frame bereits vorhanden sind.

## Nutzen
- Macht die bereits übertragene Nutzlast (`freq_or_ch`, `rssi`) im Log tatsächlich
  lesbar, statt sie kommentarlos zu verwerfen — für jeden der fünf Scan-Modi, die
  `RespScanHit` senden.
- Rein additiv: ein neuer `case UkfeRfRespScanHit:` vor dem bestehenden `default:`.
  Alle anderen Zweige, der Frame-Scanner (`rx_thread_fn()`,
  `rf_comm_parse_frame()`) und das Wire-Format bleiben unverändert.
- Bounds-geprüft: `args[]` ist zwar ein festes 40-Byte-Array
  (`UKFE_RF_MAX_ARGS`), ein Frame kann aber mit `arg_len < 5` ankommen (Hub-seitiger
  Bug oder unvollständige Nutzlast). Ohne Prüfung von `arg_len` gegen die für
  `freq_or_ch`+`rssi` nötigen 5 Byte würde der Fix unbelegte/veraltete Restbytes des
  Puffers als echte Werte ausgeben — kein Speicherzugriffsfehler, aber ein neues
  Korrektheitsproblem, das der Fix bewusst vermeidet.
- `freq_or_ch` wird konsistent zum bereits vorhandenen Encoder-Muster
  (`ukfe_rf.c:put_u32()`, z. B. in `ukfe_rf_make_lora_scan()`) als Little-Endian-
  `uint32_t` decodiert; `rssi` konsistent zum bestehenden `RespStatus`-Zweig
  (`(int8_t)m->args[3]`) als vorzeichenbehaftetes Byte.

## Betroffene Dateien
- `uart_comm.c` — `handle_rf_response()`: ein neuer `case UkfeRfRespScanHit:` vor
  dem bestehenden `default:`.

Keine Änderung an `rf/ukfe_rf.c`/`.h` (Protokoll/Framing unverändert) oder an einer
Scene — es gibt (noch) keine dedizierte Scan-Ergebnis-Ansicht, die decodierten
Werte landen wie bei `RespRelayed`/`RespAck` zunächst nur im allgemeinen Log
(`scene_log.c`).

## Patch
```c
case UkfeRfRespScanHit: {
    // args: uint32 freq_or_ch (LE), uint8 rssi -- 5 Byte Mindestlaenge, sonst
    // waeren die decodierten Werte unbelegter/veralteter Inhalt von args[]
    // (festes 40-Byte-Array, kein OOB, aber falsche Werte ohne diesen Check).
    if(m->arg_len >= 5) {
        uint32_t freq_or_ch = (uint32_t)m->args[0] | ((uint32_t)m->args[1] << 8) |
            ((uint32_t)m->args[2] << 16) | ((uint32_t)m->args[3] << 24);
        int8_t rssi = (int8_t)m->args[4];
        snprintf(line, sizeof(line), "[SCAN] freq/ch=%lu rssi=%d",
            (unsigned long)freq_or_ch, rssi);
    } else {
        snprintf(line, sizeof(line), "[SCAN] malformed alen=%u (need >=5)", m->arg_len);
    }
    break;
}
```

Einzige Änderung an `handle_rf_response()`: dieser `case`-Zweig wird vor `default:`
eingefügt (nach dem bereits vorhandenen `case UkfeRfRespPayload:`). Funktionssignatur,
alle anderen Zweige und der Aufrufer (`rx_thread_fn()`) bleiben unverändert.

## Testhinweis (ohne Hardware)
Reine Decodier-/Formatierlogik (kein UART-, Funk- oder GUI-Zugriff), host-
kompilierbar nachvollziehbar. Reproduktionsprogramm
(`gcc -O0 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro`)
in `repro-scanhit-response-decode/repro.c`, das `handle_rf_response()` in ALT- und
NEU-Fassung 1:1 nachbaut (Ack/Status/Relayed/Payload als bereits behandelt
zusammengefasst, `default` sowie den neuen `UkfeRfRespScanHit`-Case im Detail) und
fünf Szenarien prüft:

1. LoRa-Scan-Treffer (868350 kHz, -72 dBm) gegen ALT: Log zeigt nur
   `[RX] cmd=0x90 ctr=0 alen=5`, weder Frequenz noch RSSI sichtbar — die
   dokumentierte Lücke.
2. Derselbe Treffer gegen NEU: Log zeigt exakt
   `[SCAN] freq/ch=868350 rssi=-72`.
3. Randfall `freq_or_ch = UINT32_MAX`, `rssi = INT8_MIN`: korrekt als
   `[SCAN] freq/ch=4294967295 rssi=-128` decodiert (Vorzeichenbehandlung und
   volle `uint32_t`-Breite bestätigt).
4. Zu kurzer Frame (`arg_len = 4`, RSSI-Byte fehlt am eigentlichen Frame, obwohl im
   Puffer noch ein Fremdwert steht): wird als
   `[SCAN] malformed alen=4 (need >=5)` markiert statt den Fremdwert als RSSI
   auszugeben.
5. Leerer Frame (`arg_len = 0`): ebenfalls sicher als unvollständig markiert.

Erwartete Ausgabe: Szenario 1 zeigt die ALT-Lücke (Nutzlast nicht sichtbar),
Szenarien 2-5 bestätigen NEU (korrekte Decodierung, korrekter Randfall, sichere
Behandlung zu kurzer Frames).

Funktionaler Test danach lokal auf dem Gerät: einen der fünf Scan-Befehle auslösen
(z. B. LoRa-Scan über `scene_mode_menu.c`), Log-Ansicht (`scene_log.c`) öffnen —
vor dem Fix erscheinen Treffer nur als `[RX] cmd=0x90 ...` ohne Nutzlast, danach mit
decodierter Frequenz/Kanal und RSSI.

## Risiko
- Sehr niedrig: rein additiver `case`-Zweig in einer bereits vorhandenen `switch`-
  Anweisung, keine Änderung an bestehenden Zweigen, am Frame-Scanner oder am
  Wire-Format.
- Die Bounds-Prüfung (`arg_len >= 5`) ist reine Zusatzabsicherung — reduziert ein
  Korrektheitsrisiko, führt keines neu ein.
- Kein zusätzlicher Speicher-, Storage- oder Netzwerkzugriff; keine neue
  Zustandsvariable in `LораUkfeApp`.

## Offen (bewusst nicht mitbehoben)
- Es gibt weiterhin keine dedizierte Scan-Ergebnis-Ansicht — Treffer landen nur im
  allgemeinen Log, nicht in einer strukturierten, filter-/sortierbaren Liste
  (analog `scene_payload_list.c`). Das wäre eine eigenständige Scene und damit ein
  größerer, separater Beitrag, nicht Teil dieses Funds.
- `freq_or_ch` ist laut Kommentar in `rf/ukfe_rf.h` je nach auslösendem Scan-Befehl
  entweder eine Frequenz (LoRa/SubGHz, vermutlich kHz analog zu
  `ukfe_rf_make_lora_scan()`s `start_khz`/`end_khz`) oder ein Kanalindex
  (WiFi/BLE/RemoteID) — der `RespScanHit`-Frame selbst trägt kein Feld, das den
  auslösenden Scan-Typ benennt, daher gebe ich den Wert bewusst einheitenlos als
  `freq/ch=` aus statt eine Einheit zu unterstellen, die nicht in jedem Fall stimmt.
  Bei Bedarf könnte der Hub künftig den auslösenden Befehl mitschicken (Protokoll-
  Erweiterung, nicht Teil dieses Fixes).
- Wie bereits beim `RespPayload`-Fund vermerkt: `ukfe_uart_send_lora_scan()` und
  `ukfe_uart_send_wifi_scan()` (`uart_comm.c`) senden weiterhin JSON-Zeilen
  (`{"cmd":"lora_scan",...}` / `{"cmd":"wifi_scan"}`), obwohl der Hub seit der
  Migration binär antwortet — ob das mit dem aktuellen Hub-Stand überhaupt noch
  zusammenpasst, kann ich aus diesem Repo allein nicht verifizieren (fehlender
  Hub-/V4-Hub-Quellstand). Unabhängig davon behebt dieser Fix in jedem Fall die
  RX-seitige Decodierlücke, sobald `RespScanHit`-Frames eintreffen.

## Quelle
- `G4MEOVER18/lora-ukfe` (main, `0dba410a`): `uart_comm.c` (`handle_rf_response()`),
  `rf/ukfe_rf.h` (`UkfeRfRespScanHit`-Definition, Scan-Befehle 0x10/0x12/0x20/0x30/
  0x2B), `rf/ukfe_rf.c` (`put_u32()`, `ukfe_rf_make_lora_scan()` als Referenz für
  die Little-Endian-Kodierung).
