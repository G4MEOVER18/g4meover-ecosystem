# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: fehlende Protokollprüfung in "NFC Commands"

## Was
Ich habe den NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`, `fap_version` 1.3,
Quelle `github.com/wakdev/nfctools-fz`) fortgesetzt und diesmal die "NFC Commands"-Funktion
(rohe APDU-Befehle an eine erkannte Karte senden) vollständig gelesen:
`scenes/other/nfc_tools_scene_nfc_commands_input.c` (41 Zeilen) und
`scenes/other/nfc_tools_scene_nfc_commands_run.c` (322 Zeilen). Zum Quervergleich habe ich
zusätzlich die drei strukturell ähnlichsten Geschwister-Szenen erneut vollständig gelesen:
`scenes/other/nfc_tools_scene_set_password_write.c` (283 Zeilen),
`scenes/other/nfc_tools_scene_remove_password_write.c` (278 Zeilen) und
`scenes/other/nfc_tools_scene_format_tag.c` (173 Zeilen), sowie die zugehörigen Input-Szenen
(`nfc_tools_scene_set_password_input.c`, `nfc_tools_scene_remove_password_input.c`, je unter 50
Zeilen).

## Fund: "NFC Commands" prüft das erkannte Protokoll nicht, bevor es einen Low-Level-Poller startet
Alle vier hier gelesenen "schreibenden" Funktionen der App folgen demselben Muster: ein
`NfcScanner` erkennt zunächst `app->detected_protocol`, danach wird genau dieses Protokoll
geprüft, bevor irgendein protokollspezifischer Poller gestartet wird:

```c
// nfc_tools_scene_set_password_write.c / _remove_password_write.c
if(app->detected_protocol != NfcProtocolMfUltralight) {
    // "Incompatible tag\n(%s)\nOnly NTAG21x" -- Abbruch
}
```
```c
// nfc_tools_scene_format_tag.c
if(app->detected_protocol == NfcProtocolMfUltralight) { ... }
else if(... == NfcProtocolIso15693_3 || ... == NfcProtocolSlix) { ... }
else if(... == NfcProtocolMfClassic) { ... }
else if(... == NfcProtocolMfDesfire) { "Formatting not supported" }
else { "Unsupported tag:\n%s" } // expliziter Fallback
```

"NFC Commands" (`scenes/other/nfc_tools_scene_nfc_commands_run.c:145`) macht das nicht. Die
Routing-Entscheidung, welcher der beiden Low-Level-Poller (ISO14443-3A oder ISO14443-4A) den vom
Nutzer per Hex-Eingabe zusammengestellten Rohbefehl bekommt, ist ein einziger Bool-Helfer ohne
`else`-Zweig für "unbekannt/inkompatibel":

```c
static bool protocol_is_iso4a(NfcProtocol p) {
    return p == NfcProtocolIso14443_4a || p == NfcProtocolMfDesfire || p == NfcProtocolMfPlus;
}
...
bool is4a = protocol_is_iso4a(app->detected_protocol);
NfcProtocol poller_proto = is4a ? NfcProtocolIso14443_4a : NfcProtocolIso14443_3a;

NfcPoller* poller = nfc_poller_alloc(app->nfc, poller_proto);
nfc_poller_start(poller, is4a ? nfc_commands_iso4a_cb : nfc_commands_iso3a_cb, &cmd_ctx);
```

Zwischen der Protokollerkennung (`nfc_commands_scan_callback`) und dieser Weiche gibt es **keine**
Zeile, die `app->detected_protocol` gegen eine erlaubte Liste prüft — anders als bei den drei oben
zitierten Geschwister-Funktionen. `protocol_is_iso4a()` fängt außerdem nur 3 der insgesamt 6
tatsächlichen ISO14443-4A-Familienmitglieder ab. Der eigene Protokollbaum der Firmware
(`lib/nfc/protocols/nfc_protocol.c`, Kommentar-Diagramm am Dateianfang, `nfc_protocol_nodes[]`)
listet als direkte Kinder von `NfcProtocolIso14443_4a`: `MfPlus`, `MfDesfire`, `Ntag4xx`,
`Type4Tag`, `Emv` — fünf Kinder, nicht zwei. `protocol_is_iso4a()` kennt nur `MfDesfire` und
`MfPlus`; `Ntag4xx` (NTAG424 DNA und vergleichbare Tags), `Type4Tag` und `Emv`
(kontaktlose Bankkarten) fehlen komplett und fallen damit in den `else`-Zweig, obwohl sie
protokollseitig exakt so sehr "ISO14443-4A" sind wie DESFire/MfPlus.

Daraus ergeben sich zwei getrennte Probleme, sobald ein Nutzer "NFC Commands" auf eine Karte
außerhalb der drei geprüften Typen anwendet (laut Kopfkommentar der Datei ausdrücklich als
Funktionsumfang gedacht: *"Route to ISO 14443-4A (DESFire, MfPlus, ISO 14443-4A) or ISO 14443-3A
(MfUltralight, MfClassic, ISO 14443-3A)"*):

1. **Echte ISO14443-4A-Karten werden am falschen Poller vorbeigeschickt.** Ein NTAG424-DNA-Tag,
   ein generisch als "Type 4 Tag" erkannter Datenträger oder eine EMV-fähige Bankkarte — allesamt
   plausible Ziele für einen autorisierten NFC-Pentest mit einem selbst zusammengestellten
   APDU-Befehl — werden statt über den korrekten ISO14443-4A-Poller über den ISO14443-3A-Poller
   angesprochen. Der eingegebene Hex-Befehl (von der App selbst als "APDU" bezeichnet, siehe
   `NTS_INPUT_APDU`) erreicht die Karte damit gar nicht im vom Nutzer erwarteten Protokollrahmen.
2. **Protokollfremde Karten erhalten keine klare Fehlermeldung.** Felica-, ISO15693-3-/SLIX-,
   ST25TB- und ISO14443-3B-/4B-Tags sind weder ISO14443-3A- noch ISO14443-4A-kompatibel — die
   drei Geschwister-Funktionen würden hier sofort mit "Incompatible tag"/"Unsupported tag: %s"
   abbrechen. "NFC Commands" versucht stattdessen kommentarlos den ISO14443-3A-Poller, was in der
   Praxis lediglich zu einem generischen "ISO 14443-3A error" statt einer aussagekräftigen
   Fehlermeldung führt (kein mir bekannter Absturzpfad, da beide Poller-Callbacks den
   `Ready`-Status vor jedem Senden prüfen — siehe "Zusätzlich geprüft" unten) — aber eben ein
   deutlicher Rückschritt gegenüber dem sonst in der App durchgängigen Muster.

## Host-Repro
Eigenständiges Host-Repro (kein Flipper-/NFC-Poller-Bezug), das `protocol_is_iso4a()` 1:1 aus dem
Original übernimmt (ALT) und gegen eine Routing-Funktion vergleicht, die dieselbe
`nfc_protocol_has_parent()`-Logik nutzt, die die Firmware selbst dafür bereitstellt (NEU), über
alle 15 real existierenden `NfcProtocol`-Werte aus `lib/nfc/protocols/nfc_protocol.h`:

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

Tatsächliche Ausgabe (lokal ausgeführt, gekürzt auf die abweichenden Zeilen):
```
protocol       | OLD (as-shipped)     | NEW (real protocol-tree family)
--------------------------------------------------------------------------------
Iso14443_3b    | 3A poller (sent)     | REJECTED ("Incompatible tag")   <-- differs
Iso14443_4b    | 3A poller (sent)     | REJECTED ("Incompatible tag")   <-- differs
Iso15693_3     | 3A poller (sent)     | REJECTED ("Incompatible tag")   <-- differs
Felica         | 3A poller (sent)     | REJECTED ("Incompatible tag")   <-- differs
Slix           | 3A poller (sent)     | REJECTED ("Incompatible tag")   <-- differs
St25tb         | 3A poller (sent)     | REJECTED ("Incompatible tag")   <-- differs
Ntag4xx        | 3A poller (sent)     | 4A poller (sent)   <-- differs
Type4Tag       | 3A poller (sent)     | 4A poller (sent)   <-- differs
Emv            | 3A poller (sent)     | 4A poller (sent)   <-- differs

9 / 15 protocol values are routed differently by OLD vs. NEW.
```
Die restlichen 6 von 15 Werten (`Iso14443_3a`, `Iso14443_4a`, `MfUltralight`, `MfClassic`,
`MfPlus`, `MfDesfire`) sind Regressionsfälle: ALT und NEU routen identisch — die drei bereits
funktionierenden Kernfälle (3A-Basis, 4A-Basis, die beiden bereits erkannten 4A-Kinder) ändern
sich durch den Fix nicht.

## Betroffene Dateien
- `scenes/other/nfc_tools_scene_nfc_commands_run.c` — `protocol_is_iso4a()` (Zeile ~145) und die
  fehlende Protokollprüfung im Worker (`nfc_commands_worker()`, zwischen Phase 1 und Phase 2,
  Zeile ~233–247), einziger Fix-Ort.

## Risiko
Gering. Reine Kontrollfluss-/Klassifikationsänderung vor dem ersten RF-Zugriff, keine geänderte
Datenstruktur, kein Einfluss auf die drei bereits korrekt geroutete Kernfälle (siehe
Regressionsfälle oben). Kein mir bekannter Absturzpfad — beide Poller-Callbacks
(`nfc_commands_iso3a_cb`/`_iso4a_cb`) verlassen sich nicht darauf, dass das Protokoll vorab
geprüft wurde, sondern prüfen selbst `ev->type != ...Ready`, bevor sie senden (siehe "Zusätzlich
geprüft" unten) — der Fund ist eine Funktions-/Konsistenzlücke (falscher Poller bzw. fehlende
Fehlermeldung), keine Speichersicherheitslücke.

## Port-Skizze / Fix
`protocol_is_iso4a()` durch eine Prüfung anhand des echten Protokollbaums ersetzen (die Firmware
stellt `nfc_protocol_has_parent()` in `lib/nfc/protocols/nfc_protocol.h` bereits genau für diesen
Zweck bereit — kein neuer Code in der Firmware nötig, nur eine bessere Nutzung einer vorhandenen
API), und echte Fremdprotokolle explizit mit derselben Fehlermeldung wie die Geschwister-Szenen
abfangen, statt sie kommentarlos an den 3A-Poller weiterzureichen:

```c
static bool protocol_is_iso4a(NfcProtocol p) {
    return p == NfcProtocolIso14443_4a || nfc_protocol_has_parent(p, NfcProtocolIso14443_4a);
}

static bool protocol_is_iso3a_only(NfcProtocol p) {
    return p == NfcProtocolIso14443_3a || p == NfcProtocolMfUltralight ||
           p == NfcProtocolMfClassic;
}
```

Im Worker, direkt nach Phase 1 (Protokollerkennung), vor Aufbau von `cmd_ctx`/Poller-Start:
```c
if(!protocol_is_iso4a(app->detected_protocol) && !protocol_is_iso3a_only(app->detected_protocol)) {
    notification_message(app->notifications, &sequence_error);
    furi_string_printf(
        app->info_str,
        "Incompatible tag\n(%s)\nOnly ISO14443-3A/4A",
        nfc_device_get_protocol_name(app->detected_protocol));
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
    return 0;
}
```
`#include "../../../lib/nfc/protocols/nfc_protocol.h"` (bzw. der von der App bereits
transitiv über `<nfc/nfc_poller.h>` eingebundene Pfad — in dieser Sitzung nicht am realen
Build-Baum verifiziert, siehe Testhinweis) für `nfc_protocol_has_parent()`.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-commands-missing-protocol-guard/repro.c`, host-kompilierbar,
keine Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: Fix einbauen (`ufbt launch_app` gegen
`nfctools`), danach mit einem NTAG424-DNA-Tag oder einer EMV-fähigen Bankkarte (autorisierter
eigener Test, keine fremde Karte) "NFC Commands" → einen einfachen SELECT-APDU
(`00 A4 04 00 00`) eingeben. Vor dem Fix: Befehl wird über den ISO14443-3A-Poller gesendet
(falscher Protokollrahmen). Nach dem Fix: Befehl wird über den ISO14443-4A-Poller gesendet. Bei
mir zusätzlich mit einem ISO15693-3-/SLIX-Tag (z. B. bereits vorhandener ICODE-Tag aus der
ICODE-Sitzung) gegenprüfbar: Vor dem Fix generisches "ISO 14443-3A error", nach dem Fix klares
"Incompatible tag (Iso15693_3)\nOnly ISO14443-3A/4A".

## Zusätzlich geprüft, kein Fund
- `hex_parse()` (Zeile 26): jeder Byte-Schreibzugriff gegen `max_out` geprüft (`if(count >=
  max_out) return -1;`), gibt bei Überlänge sauber `-1` zurück statt zu kürzen oder zu überlaufen
  — `cmd_bytes[128]` in `nfc_commands_worker()` kann durch die 1024 Byte fassende
  `app->ndef_buf1`-Eingabe (kein `keyboard_set_max_length()`-Aufruf in
  `nfc_tools_scene_nfc_commands_input.c`, das Feld verlässt sich bewusst auf `hex_parse()`s
  eigene Grenze) nicht überlaufen — kein Fund.
- `nfc_commands_iso3a_cb()`/`_iso4a_cb()`: `rsp[258]` exakt an die Poller-API angepasst
  (`bit_buffer_alloc(258)`, "max APDU + 2 SW bytes"), `rsp_len` vor jedem `memcpy()` zusätzlich
  gegen `sizeof(ctx->rsp)` geklammert — kein Fund, unabhängig vom oben beschriebenen
  Routing-Fund.
- `format_result()`: `rsp_len == 0`/`rsp_len >= 2`-Fälle beide behandelt, `cmd`/`rsp`-Schleifen
  über die jeweils übergebene Länge, keine feste Zwischenpuffer — kein Fund.
- Geschwister-Funktionen `nfc_tools_scene_set_password_write.c`,
  `nfc_tools_scene_remove_password_write.c`, `nfc_tools_scene_format_tag.c`: alle drei prüfen
  `app->detected_protocol` korrekt und vollständig vor jedem Poller-Start (siehe "Fund" oben) —
  kein Fund, dienen hier als Vergleichsmaßstab für den beschriebenen Fund in "NFC Commands".
  `ntag21x_get_pages()` (identisch dupliziert in beiden Passwort-Dateien) deckt exakt die drei
  über `mf_ultralight_get_type_by_version()` unterscheidbaren NTAG21x-Varianten ab, `default:
  return false` fängt jeden anderen `MfUltralightType` (inkl. reinem Ultralight/UL-C ohne
  NTAG21x-Seitenlayout) — kein Fund.
- `nfc_tools_scene_lock_tag.c` (257 Zeilen, zum Vergleich erneut mitgelesen): dieselbe
  `detected_protocol != NfcProtocolMfUltralight`-Prüfung, feste 4-Byte-Seitenpuffer für UID-Bytes
  und Lock-Bits, keine kartenabhängige Puffergröße — kein Fund.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout, Stand dieser
  Sitzung, identischer Commit-Stand wie die vorherigen NFC-Tools-Sitzungen):
  `applications/external/nfctools/`, `fap_author` "wakdev", Original-Quelle
  `github.com/wakdev/nfctools-fz`, `fap_version` 1.3.
- Protokollbaum/`nfc_protocol_has_parent()`: `lib/nfc/protocols/nfc_protocol.h` und `.c` aus
  demselben Sparse-Checkout (Firmware-eigene NFC-Infrastruktur, nicht Teil der App selbst) — zur
  Verifikation der tatsächlichen Eltern-Kind-Beziehungen zwischen `NfcProtocol`-Werten
  nachgeladen, nicht nur aus dem Enum-Namen geraten.
- API-87-Einordnung: Der Fund betrifft ausschließlich appeigene Klassifikationslogik
  (`protocol_is_iso4a()`), die bereits vorhandene, stabile Firmware-API `nfc_protocol_has_parent()`
  wird für den Fix lediglich erstmals von dieser App genutzt — kein firmwareversionsabhängiges
  Kompatibilitätsrisiko; `NfcProtocolNtag4xx`/`NfcProtocolType4Tag`/`NfcProtocolEmv` sind
  bestehende, nicht neue Einträge im Protokollbaum.
- Eigener Fund: `scenes/other/nfc_tools_scene_nfc_commands_run.c`, `protocol_is_iso4a()` +
  fehlende Protokollprüfung im Worker, mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-nfctools-commands-missing-protocol-guard/repro.c`).
- Restlicher Umfang des Clusters (~35 Schreib-Szenen für Kontakt/Bitcoin/WLAN/Social/etc.,
  Sektor-Analyse bereits geprüft, Tag-Info-/QR-Code-/NDEF-Detail-Lesepfade) für künftige
  Sitzungen offen; damit ist der Rückstellpunkt "restlicher NFC-Werkzeug-Cluster" weiterhin nicht
  vollständig abgeschlossen.
