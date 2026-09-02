# lora-ukfe — Payload-Liste bleibt nach der Binaer-RX-Migration dauerhaft leer (RespPayload unbehandelt)

## Was
Ich habe den aktuellen Stand von `lora-ukfe` (`0dba410a`, "uart_comm/rf_comm: Flipper-RX
auf Binaer-Frame-Scanner") geprueft, insbesondere die neue `handle_rf_response()` in
`uart_comm.c`, die den bisherigen JSON-Zeilenparser auf der Empfangsseite ersetzt hat.

`handle_rf_response()` behandelt drei Response-Typen explizit:

```c
switch(m->cmd) {
case UkfeRfRespAck:      ...
case UkfeRfRespStatus:   ...
case UkfeRfRespRelayed:  ...
default:
    snprintf(line, sizeof(line), "[RX] cmd=0x%02X ctr=%lu alen=%u",
        m->cmd, (unsigned long)m->counter, m->arg_len);
    break;
}
```

`rf/ukfe_rf.h` definiert daneben aber auch `UkfeRfRespPayload = 0x84` ("args: uint8 idx,
char name[]") und `UkfeRfRespScanHit = 0x90` — beide fallen ungeprueft in den generischen
`default`-Zweig: reines Loggen, kein Zustandsupdate.

Das ist fuer `UkfeRfRespPayload` kein Stilproblem, sondern eine echte Funktionsluecke.
Vorher (JSON-Aera) lief die Payload-Liste ueber `json_parse.c`:

```c
static void handle_payload_list(LораUkfeApp* app, const char* json) {
    ...
    app->payloads[app->payload_count].id = id;
    strlcpy(app->payloads[app->payload_count].name, name, 48);
    app->payload_count++;
    ...
    view_dispatcher_send_custom_event(app->view_dispatcher, 0x12);
}
```

— das fuellte `app->payloads[]`/`app->payload_count` und stiess per Custom-Event `0x12`
den Neuaufbau der Submenue-Liste in `scene_payload_list.c` an. Ich habe per `grep -rn
ukfe_json_handle` im gesamten Baum geprueft: die Funktion ist nur noch an zwei Stellen
vorhanden — ihre eigene Definition in `json_parse.c` und die Deklaration in
`lora_ukfe.h`. Es gibt **keinen** Aufrufer mehr; die RX-Thread-Funktion in `uart_comm.c`
ruft ausschliesslich `rf_comm_parse_frame()` + `handle_rf_response()` auf. Der komplette
JSON-Empfangspfad — inklusive der einzigen Stelle, die `app->payloads[]`/
`app->payload_count` befuellt hat und Event `0x12` sendet — ist damit toter Code.

Ergebnis: `scene_payload_list_on_enter()` (`scenes/scene_payload_list.c`) ruft weiterhin
`ukfe_uart_send_payload_list(app)` auf und wartet auf Event `0x12`, um `build_list()`
erneut aufzurufen. Egal wie viele `UkfeRfRespPayload`-Frames der Hub tatsaechlich sendet
— sie werden alle im `default`-Zweig geloggt und verworfen. `app->payload_count` bleibt
fuer immer `0`, die Ansicht zeigt dauerhaft `"(keine — Liste laden)"`.

## Nutzen
- Behebt eine vollstaendige Funktionsluecke (kein Randfall): die Payload-Liste-Ansicht
  ist seit der Binaer-RX-Migration fuer jeden Nutzer und jede Sitzung unbenutzbar,
  unabhaengig vom Hub-Zustand.
- Der Fix ist rein additiv: ein neuer `case UkfeRfRespPayload:` in der bereits
  vorhandenen `switch`-Anweisung. `UkfeRfRespAck`/`Status`/`Relayed`, der `default`-Zweig
  fuer alles andere sowie der komplette Frame-Scanner (`rx_thread_fn()`,
  `rf_comm_parse_frame()`) bleiben unveraendert.
- Baut exakt das Datenmodell nach, das `scene_payload_list.c` bereits erwartet
  (`app->payloads[idx].id`/`.name`, `app->payload_count`, Custom-Event `0x12`) — keine
  Aenderung an der Scene selbst noetig.
- `idx` wird sowohl gegen `UKFE_MAX_PAYLOADS` als auch die Namenslaenge gegen die
  tatsaechliche Feldkapazitaet von `UkfePayloadEntry.name[48]` geprueft, bevor
  geschrieben wird — `args[0]` ist ein vom Hub kontrolliertes `uint8_t` (0..255,
  `UKFE_MAX_PAYLOADS` ist nur 32), ein ungeprueftes `app->payloads[idx]` waere ein
  Off-by-Design-Speicherzugriffsfehler bei jedem Hub-Firmwarestand, der eine hoehere
  Payload-Anzahl oder einen abweichenden Index sendet als die Flipper-Seite vorsieht.

## Betroffene Dateien
- `uart_comm.c` — `handle_rf_response()`: ein neuer `case UkfeRfRespPayload:` vor dem
  bestehenden `default:`.

Keine Aenderung an `rf/ukfe_rf.c`/`.h` (Protokoll/Framing unveraendert), `scenes/
scene_payload_list.c` (Konsument des bereits vorhandenen Datenmodells) oder `json_parse.c`
(bleibt toter Code — siehe "Offen" unten).

## Patch
```c
case UkfeRfRespPayload: {
    // args: uint8 idx, char name[] (Rest von arg_len) -- idx ist hub-kontrolliert
    // (uint8_t, 0..255), UKFE_MAX_PAYLOADS ist nur 32: ohne Bounds-Check waere das
    // ein Speicherzugriffsfehler auf app->payloads[] bei jedem idx >= 32.
    uint8_t idx = m->arg_len > 0 ? m->args[0] : 0xFFu;
    if(m->arg_len > 0 && idx < UKFE_MAX_PAYLOADS) {
        size_t name_len = m->arg_len > 1 ? (size_t)(m->arg_len - 1) : 0;
        size_t name_cap = sizeof(app->payloads[idx].name) - 1;
        if(name_len > name_cap) name_len = name_cap;
        app->payloads[idx].id = idx;
        memcpy(app->payloads[idx].name, &m->args[1], name_len);
        app->payloads[idx].name[name_len] = '\0';
        if(idx >= app->payload_count) app->payload_count = idx + 1;
        snprintf(line, sizeof(line), "[PAYLOAD] idx=%u %s", idx, app->payloads[idx].name);
        view_dispatcher_send_custom_event(app->view_dispatcher, 0x12);
    } else {
        snprintf(line, sizeof(line), "[PAYLOAD] invalid idx=%u (max %u)",
            idx, UKFE_MAX_PAYLOADS - 1);
    }
    break;
}
```

Einzige Aenderung an `handle_rf_response()`: dieser `case`-Zweig wird vor `default:`
eingefuegt. Die Funktionssignatur, alle anderen Zweige und der Aufrufer
(`rx_thread_fn()`) bleiben unveraendert.

## Testhinweis (ohne Hardware)
Reine Zustands-/Dispatch-Logik (kein UART-, Funk- oder GUI-Zugriff), host-kompilierbar
nachvollziehbar. Reproduktionsprogramm
(`gcc -O0 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro`) in
`repro-payload-list-response-dispatch/repro.c`, das `handle_rf_response()` in ALT- und
NEU-Fassung 1:1 nachbaut (nur der Ausschnitt fuer Ack/Status/Relayed/default bzw. den
neuen `UkfeRfRespPayload`-Case) und vier Szenarien prueft:

1. Drei reguläre `RespPayload`-Frames (idx 0..2) gegen ALT: `payload_count` bleibt `0`,
   Event-Zaehler bleibt `0` — die dokumentierte Funktionsluecke.
2. Dieselben drei Frames gegen NEU: `payload_count` wird `3`, alle drei Namen korrekt
   uebernommen, Event dreimal gesendet.
3. Randfall `idx = UKFE_MAX_PAYLOADS - 1` (letzter gueltiger Slot) mit einem 39-Byte-Namen
   (`UKFE_RF_MAX_ARGS - 1`, der laut Protokoll maximal moegliche Nutzlastanteil) — passt
   vollstaendig in `name[48]`, keine Kuerzung, `payload_count` erreicht `UKFE_MAX_PAYLOADS`.
4. Kartenkontrollierter `idx = 200` (aus dem gueltigen `uint8_t`-Wertebereich von `args[0]`,
   aber ausserhalb von `UKFE_MAX_PAYLOADS`): wird sicher verworfen, `payloads[]` bleibt
   unveraendert, kein Schreibzugriff ausserhalb des Arrays — unter ASan/UBSan verifiziert.

Erwartete Ausgabe: Szenario 1 zeigt die ALT-Luecke (`payload_count` bleibt 0 trotz
empfangener Frames), Szenarien 2-4 bestaetigen NEU (korrekte Uebernahme, korrekter
Randfall, sichere Ablehnung eines ausserhalb liegenden Index).

Funktionaler Test danach lokal auf dem Geraet: Payload-Liste-Szene oeffnen (sendet
`{"cmd":"payload_list"}`), Hub-Antwort abwarten — vor dem Fix bleibt die Liste dauerhaft
bei `"(keine — Liste laden)"` unabhaengig von der Hub-Antwort, danach erscheinen die vom
Hub gemeldeten Eintraege und lassen sich wie gewohnt per `payload_callback()` ausloesen.

## Risiko
- Sehr niedrig: rein additiver `case`-Zweig in einer bereits vorhandenen `switch`-
  Anweisung, keine Aenderung an bestehenden Zweigen, am Frame-Scanner oder am
  Wire-Format.
- Beide Bounds-Pruefungen (Index gegen `UKFE_MAX_PAYLOADS`, Namenslaenge gegen die
  Feldkapazitaet) sind reine Zusatzabsicherung — reduzieren ein Risiko, fuehren keines
  neu ein.
- Kein zusaetzlicher Speicher-, Storage- oder Netzwerkzugriff.

## Offen (bewusst nicht mitbehoben)
- `UkfeRfRespScanHit = 0x90` faellt weiterhin in den `default`-Zweig — betrifft eine
  andere, noch nicht identifizierte Konsumenten-Scene und ist nicht Teil dieses Funds;
  bei Bedarf eigener Prüfpunkt.
- `json_parse.c`/`ukfe_json_handle()` bleiben nach diesem Fix weiterhin toter Code
  (kein Aufrufer im Baum) — Entfernen oder Reaktivieren ist eine separate Entscheidung
  und aendert nichts an der hier behobenen Funktionsluecke.
- Ob die sendende Seite (`ukfe_uart_send_payload_list()` in `uart_comm.c`, sendet nach
  wie vor die JSON-Zeile `{"cmd":"payload_list"}`) mit dem seit der Migration binaer
  antwortenden Hub ueberhaupt noch zusammenpasst, kann ich aus diesem Repo allein nicht
  verifizieren — dafuer fehlt mir der Hub-/V4-Hub-Quellstand. Dieser Fix behebt in jedem
  Fall die RX-seitige Luecke: sobald der Hub (aus welchem Anlass auch immer)
  `UkfeRfRespPayload`-Frames sendet, werden sie jetzt korrekt verarbeitet statt verworfen.

## Quelle
- `G4MEOVER18/lora-ukfe` (main, `0dba410a`): `uart_comm.c` (`handle_rf_response()`,
  `rx_thread_fn()`), `rf/ukfe_rf.h` (`UkfeRfRespPayload`/`UkfeRfRespScanHit`-Definition),
  `scenes/scene_payload_list.c` (Konsument von `app->payloads[]`/`payload_count`/Event
  `0x12`), `lora_ukfe.h` (`UkfePayloadEntry`, `UKFE_MAX_PAYLOADS`,
  `ukfe_json_handle()`-Deklaration), `json_parse.c` (`handle_payload_list()`, alter,
  jetzt aufruferloser Referenzcode fuer das erwartete Zustandsmodell).
