# lora-ukfe — json_parse.c ist vollstaendig toter Code (Entfernung), drei Event-Typen ohne Binaer-Aequivalent dokumentiert

## Was
Ich habe den in den letzten beiden `payload-list-response-dispatch-fix.md`/
`scanhit-response-decode-fix.md`-Funden jeweils unter "Offen" zurueckgestellten Punkt
aufgegriffen: "`json_parse.c`/`ukfe_json_handle()` bleiben toter Code — Entfernen oder
Reaktivieren ist eine separate Entscheidung." Ich habe diese Entscheidung jetzt getroffen
und vorbereitet.

Gegen den aktuellen Stand (`0dba410a`, "uart_comm/rf_comm: Flipper-RX auf
Binaer-Frame-Scanner") per `grep -rn "json_parse\|ukfe_json_handle"` ueber den
gesamten Baum geprueft: die Funktion `ukfe_json_handle()` kommt nur an zwei Stellen vor —
ihrer eigenen Definition in `json_parse.c:132` und der Deklaration in `lora_ukfe.h:107-108`.
Kein Aufrufer. Der RX-Thread (`uart_comm.c:rx_thread_fn()`) ruft seit der Migration
ausschliesslich `rf_comm_parse_frame()` + `handle_rf_response()` auf; der komplette
JSON-Zeilenpfad wird nie mehr erreicht — nicht nur `handle_payload_list()` (bereits
identifiziert), sondern alle fuenf Event-Handler in der Datei
(`handle_status`/`handle_trigger_ack`/`handle_payload_list`/`handle_lora_rx`/
`handle_wifi_ap`/`handle_error`) plus die beiden JSON-Hilfsfunktionen (`json_str()`/
`json_int()`).

Beim Durchgehen aller fuenf Event-Typen gegen das neue Binaerprotokoll (`rf/ukfe_rf.h`,
`UkfeRfResp`-Enum) zeigt sich ein Unterschied, den ich fuer kuenftige Sitzungen fest-
halte: zwei der fuenf Events haben inzwischen ein korrektes (`status` -> `UkfeRfRespStatus`,
bereits in `handle_rf_response()` umgesetzt) bzw. ein nachgeruestetes
(`payload_list` -> `UkfeRfRespPayload`, Fix von vor zwei Sitzungen) Binaer-Aequivalent.
Fuer die restlichen drei (`lora_rx` — RSSI + Rohdaten-String; `wifi_ap` — SSID/RSSI/Kanal;
`error` — freier Fehlertext) gibt es im aktuellen `UkfeRfResp`-Enum **keinen** passenden
Code:

```
grep -n "wifi_ap\|lora_rx\|RespError\|RespWifiAp\|RespLoraRx" rf/ukfe_rf.h
-> kein Treffer
```

Das ist kein "noch nicht verdrahtet"-Fund wie bei `UkfeRfRespPayload`/`UkfeRfRespScanHit`
(dort existierte der Enum-Wert bereits, nur der `case`-Zweig fehlte) — hier fehlt der
Enum-Wert selbst. Eine Flipper-seitige Nachruestung allein wuerde nichts bewirken, solange
die Hub-Firmware (separates Repo, `heltec-v4-hub`/`usb-army-penetrator`) keinen
entsprechenden Response-Code sendet; das waere eine Protokollerweiterung mit
Gegenstueck auf der anderen Seite, kein reiner Flipper-Logikfix. Ich dokumentiere das
hier bewusst nur als offenen Punkt (siehe "Offen"), statt unilateral neue Wire-Codes zu
erfinden, die ohne abgestimmte Hub-Gegenseite wirkungslos blieben.

## Nutzen
- `json_parse.c` (146 Zeilen) wird von `ufbt`/`fbt` unveraendert mitkompiliert (keine
  `sources=`-Einschraenkung in `application.fam`, Default-Glob erfasst jede `.c`-Datei im
  App-Verzeichnis) — reiner Flash-Platzverbrauch fuer Code, der nie ausgefuehrt wird.
- Vermeidet Verwechslungsgefahr: solange die Datei im Baum liegt, sieht es so aus, als
  wuerde `error`/`lora_rx`/`wifi_ap` noch irgendwo verarbeitet. Tatsaechlich landen alle
  drei nirgends mehr — weder im (toten) JSON-Pfad noch im (fehlenden) Binaer-Pfad. Eine
  zukuenftige Aenderung, die versehentlich wieder einen Aufruf von `ukfe_json_handle()`
  einbaut, wuerde zwei parallele, inkonsistente Zustandsmodelle fuer `app->status`/
  `app->payloads[]` nebeneinander aktiv machen (derselbe Fehlerklasse, die die letzten
  beiden Sitzungen fuer `payload_list`/`ScanHit` einzeln beheben mussten).
- Rein subtraktiv: keine Verhaltensaenderung fuer `status`/`trigger_ack`/`payload_list`
  (laufen bereits ausschliesslich ueber `handle_rf_response()`), keine weitere
  Funktionsluecke fuer `lora_rx`/`wifi_ap`/`error` (die bestand bereits vor dieser
  Aenderung, seit `0dba410a` — hier nur erstmals vollstaendig benannt statt implizit in
  totem Code "versteckt").

## Betroffene Dateien
- `json_parse.c` — vollstaendig entfernt.
- `lora_ukfe.h` — die zweizeilige Deklaration (`// json_parse.c` / `void
  ukfe_json_handle(...)`) entfernt.

Keine Aenderung an `uart_comm.c`, `rf/ukfe_rf.c`/`.h`, `command_catalog.h` oder den
Scenes — alle bereits in frueheren Sitzungen vollstaendig geprueft.

## Patch
`json-parse-dead-code-removal/json_parse_dead_code_removal.patch` (Unified-Diff: löscht
`json_parse.c`, entfernt die zwei Deklarationszeilen aus `lora_ukfe.h`). Gegen zwei
frische, unabhaengige Zweitkopien der unveraenderten `G4MEOVER18/lora-ukfe`-Basis
(`0dba410a`) mit `git apply --check` verifiziert, real angewendet, Ergebnis per `md5sum`
gegen die hier gepruefte Arbeitskopie abgeglichen — identisch. Anschliessend per
`grep -rn "json_parse\|ukfe_json_handle"` ueber den gesamten gepatchten Baum (`.c`/`.h`/
`.fam`) bestaetigt: keine verbleibende Referenz.

## Testhinweis (ohne Hardware)
Reine Entfernung ungenutzten Codes — nichts an Laufzeitlogik zu reproduzieren, da keine
Funktion mehr existiert, die jemals aufgerufen wurde. Verifikation ist statisch:
1. `git apply --check` gegen zwei unabhaengige frische Kopien — bestanden.
2. `grep -rn "ukfe_json_handle\|json_parse"` nach dem Anwenden — keine Treffer mehr,
   weder im App-Code noch in `application.fam`.
3. Lokal zusaetzlich sinnvoll: `ufbt build` (oder `fbt fap_lora_ukfe`) nach dem Patch
   ausfuehren — da `ukfe_json_handle()` ohnehin nirgends aufgerufen wurde, kann das
   Entfernen keinen Compile- oder Link-Fehler verursachen; ein erfolgreicher Build
   bestaetigt das nur zusaetzlich.
4. Funktionaler Test auf dem Geraet: App wie gewohnt starten, Status/Payload-Liste/Menue
   bedienen — identisches Verhalten vor/nach dem Patch, da der entfernte Code vorher nie
   ausgefuehrt wurde.

## Risiko
- Keines im eigentlichen Sinn: der entfernte Code hatte keinen Aufrufer, sein Entfernen
  kann keinen bestehenden Ablauf aendern.
- Einziges denkbares Risiko waere ein Werkzeug/Skript ausserhalb dieses Repos, das
  `ukfe_json_handle()` extern referenziert (z. B. ein Test-Harness) — per `grep` im
  gesamten geklonten Baum ausgeschlossen; so etwas existiert hier nicht.

## Offen (bewusst nicht mitbehoben)
- Protokollerweiterung fuer `lora_rx`/`wifi_ap`/`error`: drei aus der JSON-Aera bekannte
  Faehigkeiten (LoRa-RX-Rohdatenzeile, WiFi-AP-Listeneintrag mit SSID, freier
  Fehlertext) haben seit `0dba410a` keinen Weg mehr zum Flipper-Log — weder ueber den
  (toten) JSON-Pfad noch ueber das (dafuer nicht vorgesehene) Binaerprotokoll. Ein Fix
  braeuchte sowohl einen neuen `UkfeRfResp`-Code in `rf/ukfe_rf.h` (z. B. freie Slots
  `0x83`/`0x85`/`0x86`) als auch das passende Senden-Gegenstueck auf der Hub-Firmware
  (separates Repo) — bewusst als eigener, groesserer, repo-uebergreifender
  Rueckstellpunkt vermerkt statt hier unilateral ein Wire-Format zu erfinden, das ohne
  Hub-Gegenseite wirkungslos bliebe.
- `UkfeRfRespAck` deckt laut Kommentar in `rf/ukfe_rf.h:77` "args: uint8 orig_cmd,
  uint8 result" ab, `handle_rf_response()` liest aber `args[0]` als `res` und loggt kein
  `orig_cmd` — reine Logging-Praezisionsfrage, kein Zustands- oder Sicherheitsfund, nicht
  Teil dieser Sitzung.
- `README.md` ist bereits vor diesem Fund durchgehend auf dem alten JSON-Protokoll-Stand
  (Architekturdiagramm mit `json_parse`-Block, "UART JSON-Protokoll"-Abschnitt mit
  `wifi_deauth`/`evil_portal`/`evil_portal_stop`-Kommandos, die es in `uart_comm.c` so
  nicht gibt, Dateistruktur ohne `scene_mode_menu.c`/`rf/`) — unabhaengig von diesem Fund
  und deutlich groesser im Umfang als eine Zeile `json_parse.c` nachzuziehen. Bewusst
  nicht mitgefixt: eine vollstaendige README-Aktualisierung (Architektur, Protokoll-
  Beispiele, Dateistruktur) ist ein eigener, groesserer Dokumentations-Rueckstellpunkt.

## Quelle
- `G4MEOVER18/lora-ukfe` (main, `0dba410a`): `json_parse.c` (vollstaendig, alle fuenf
  Event-Handler), `lora_ukfe.h` (Deklaration), `uart_comm.c` (`rx_thread_fn()`,
  `handle_rf_response()` — Gegenprobe fuer "kein Aufrufer"), `rf/ukfe_rf.h`
  (`UkfeRfResp`-Enum — Gegenprobe fuer "kein Binaer-Aequivalent"), `application.fam`
  (keine `sources=`-Einschraenkung, bestaetigt Default-Glob-Kompilierung).
