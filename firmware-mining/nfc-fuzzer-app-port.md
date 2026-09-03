# NFC Fuzzer — externe App aus RogueMaster-Bündel (Reader-/Tag-Protokollfuzzer)

## Was
Ich habe aus dem seit mehreren Sitzungen offenen Rückstellpunkt "restlicher NFC-Werkzeug-Cluster
aus dem RogueMaster-Bündel" (`ideas/pentest-app-katalog.md`) als nächsten Kandidaten **NFC
Fuzzer** geprüft (`applications/external/nfc_fuzzer/`, laut `application.fam`
`fap_author="PentestSuite & barkandbite"`, `fap_weburl` `barkandbite/flipper_suite` — dieselbe
Quelle wie mein bereits verifizierter UART-Sniff-/Rogue-AP-Detector-/H10301-Writer-Fund).

Die App bietet 11 Fuzz-Profile (UID, ATQA/SAK, Frame, NTAG, ISO15693, Reader Commands, MIFARE
Auth, MIFARE Read/Write, RATS/ATS, NFC-B PUPI, FeliCa IDm) über 4 Mutationsstrategien
(Sequential/Random/Bitflip/Boundary), wahlweise im Listener-Modus (Flipper emuliert eine Karte,
Ziel ist ein Reader) oder Poller-Modus (Flipper ist Reader, Ziel eine Karte). 3404 Zeilen über 6
C-Quelldateien: `nfc_fuzzer.c` (UI/App-Lifecycle, 814 Zeilen), `nfc_fuzzer_profiles.c`
(Testfall-Generatoren pro Profil/Strategie, reine Logik ohne Hardwarezugriff, 1549 Zeilen),
`nfc_fuzzer_worker.c` (Worker-Thread, treibt NFC-HAL für Listener-/Poller-Betrieb, 695 Zeilen).
Ergebnisse (Anomalien: Timeout/Unexpected-Response/Timing-Anomalie) werden live im Display
gezeigt und persistent nach `/ext/nfc_fuzzer/logs/<timestamp>.log` protokolliert.

Der Quelltext trägt bereits Kommentare wie `/* Issue 2: ... */`, `/* Issue 6: ... */`,
`/* Issue 7: ... */`, `/* Issue 8: ... */` an mehreren Stellen in `nfc_fuzzer.c` (dynamisches
Results-Array statt Fixgröße, Heap- statt Stack-Puffer für Hex-Strings, geklemmte
`snprintf`-Rückgabewerte, Mutex für geteilten Zustand zwischen Worker- und UI-Thread) — die
üblichen Speicherzugriffs-/Nebenläufigkeitsfehler dieses App-Musters sind hier also bereits
behoben. Beim eigenen Volltext-Review über alle drei Quelldateien kein weiterer
Speicherzugriffsfehler gefunden (alle Testfall-Generatoren in `nfc_fuzzer_profiles.c“ gegen
`NFC_FUZZER_MAX_PAYLOAD_LEN = 255` und die jeweiligen `uint8_t`-Längenfelder einzeln
nachgerechnet — kein Overflow).

Stattdessen ein funktionaler Architekturfehler in `nfc_fuzzer_worker.c` gefunden, der **Profil 5
"ISO15693 Fuzzing" komplett wirkungslos macht**:

```c
// nfc_fuzzer_worker.c, nfc_fuzzer_worker_run_listener() — für ALLE Profile außer
// UID/ATQA-SAK/NFC-B/FeliCa (u.a. also auch fuer Iso15693) gemeinsam durchlaufen:

NfcListener* listener = nfc_listener_alloc(nfc, NfcProtocolIso14443_3a, nfc_data);
nfc_listener_start(listener, nfc_fuzzer_listener_callback, &listener_ctx);

for(uint32_t i = 0; i < total && !worker->stop_requested; i++) {
    ...
    /* Prepare TX frame for frame-level profiles */
    if(worker->profile == NfcFuzzerProfileFrame || worker->profile == NfcFuzzerProfileNtag ||
       worker->profile == NfcFuzzerProfileIso15693) {
        bit_buffer_reset(tx_buf);
        bit_buffer_copy_bytes(tx_buf, test_case->data, test_case->data_len);
    }
    ...
    if(worker->profile == NfcFuzzerProfileFrame || worker->profile == NfcFuzzerProfileNtag ||
       worker->profile == NfcFuzzerProfileIso15693) {
        nfc_listener_tx(nfc, tx_buf);       // <- sendet ueber die ISO14443-3A-Session
    }
```

Der `NfcListener` wird für dieses Profil **nie** umgeschaltet: `nfc_listener_alloc()` bekommt an
dieser Stelle im gesamten Datei-Verlauf ausschließlich `NfcProtocolIso14443_3a` übergeben (das
Umallozieren mit neuen Kollisionsauflösungsdaten in Zeile 180–202 existiert nur für die Profile
`Uid` und `AtqaSak`, nicht für `Iso15693`). Die vom Profil-Generator gebauten ISO15693-Antwort-
Bytes (Flags/DSFID/UID nach ISO15693-3, in `nfc_fuzzer_profiles.c` unter `ISO15693 PROFILE`)
werden also über `nfc_listener_tx()` **auf einer laufenden ISO14443-3A-Listener-Session**
abgesetzt. Ich habe per Volltextsuche über den kompletten App-Quellbestand verifiziert: der
String `Iso15693` bzw. `NfcProtocolIso15693_3` taucht **ausschließlich** in
`nfc_fuzzer_profiles.c` (Namensgebung/Kommentare/Testfallgenerator) und im README/PRD auf — an
keiner einzigen Stelle wird `iso15693_3_alloc()`, `iso15693_3_set_uid()` oder
`NfcProtocolIso15693_3` tatsächlich verwendet; kein `#include` für
`nfc/protocols/iso15693_3/*.h` vorhanden (im Unterschied zu den korrekt verdrahteten
`iso14443_3b.h`/`felica.h`-Includes für die Profile NFC-B und FeliCa).

ISO14443-A und ISO15693 sind auf physikalischer Ebene grundverschiedene Protokolle (unterschiedl.
Modulation/Subträgerfrequenz/Bitrate, eigenes Anticollision-Verfahren) — Bytes, die für eine
ISO15693-Inventory-Antwort geformt sind, aber über eine ISO14443-3A-Sendefunktion herausgegeben
werden, können von keinem echten ISO15693-Reader empfangen/decodiert werden. Profil 5 "ISO15693
Fuzzing" kann in der jetzigen Fassung also niemals gegen einen echten ISO15693/Vicinity-Reader
wirken, obwohl README und PRD es genau dafür bewerben ("Targets inventory and read command
parsers in readers that support HF vicinity cards").

## Nutzen
- 10 der 11 Profile (alle außer ISO15693) sind funktional korrekt verdrahtet gegen die jeweils
  passende NFC-HAL-Session (ISO14443-3A-Listener für UID/ATQA-SAK/Frame/NTAG/RATS, ISO14443-3B-
  Listener für NFC-B, FeliCa-Listener für FeliCa, `nfc_poller_trx()` für die vier Poller-Profile)
  — deckt eine bislang komplett fehlende Kategorie in meinem Werkzeugsatz ab: **strukturierte
  Protokoll-/Grenzwert-Fuzzing-Tests gegen Reader und Tags**, ergänzend zu meinen bestehenden
  Analyse- (NFC Sniffer, sobald lokal eingebaut) und Recovery-Werkzeugen (MFKey/Nested).
- Vier Mutationsstrategien (v. a. Boundary/Bitflip) sind reproduzierbar (fester PRNG-Seed
  `"NFCF"` bei `nfc_fuzzer_profile_init()`) — für autorisierte Reader-Audits dokumentierbare,
  wiederholbare Testläufe statt Zufallsfuzzing ohne Nachvollziehbarkeit.
- Reine Fuzz-/Analysefunktion (Listener emuliert Karten mit Grenzwertdaten, Poller sendet
  malformte Kommandos an eigene Testtags) ohne Massenangriffs- oder DoS-Charakter — passt in den
  defensiven/autorisierten Rahmen aus CONSTRAINTS Punkt 5.
- Keine Zusatzhardware nötig, reine `.fap` ohne Firmware-Kernänderung.
- Der gefundene Architekturfehler lässt die anderen 10 Profile unberührt — der Port ist auch
  ohne sofortige Behebung von Profil 5 bereits nutzbar; ich würde das defekte Profil aber vor dem
  produktiven Einsatz entweder reparieren (Port-Skizze) oder im Menü vorerst deutlich als
  "nicht funktionsfähig" kennzeichnen, um keine falsche Ergebnis-Erwartung zu erzeugen (ein
  leerer/Timeout-lastiger Testlauf gegen ISO15693-Reader würde sonst fälschlich als "Reader
  reagiert nicht auf Grenzwerte" statt als "falsches Protokoll verwendet" gedeutet).

## Betroffene Dateien (lokal)
Neu, aus der Quelle übernehmbar (eigenständiger externer App-Ordner):
- `applications_user/nfc_fuzzer/application.fam`
- `applications_user/nfc_fuzzer/nfc_fuzzer.c` / `.h`
- `applications_user/nfc_fuzzer/nfc_fuzzer_profiles.c` / `.h`
- `applications_user/nfc_fuzzer/nfc_fuzzer_worker.c` / `.h` — **mit der unten beschriebenen
  Ergänzung für Profil 5**
- `applications_user/nfc_fuzzer/nfc_fuzzer.png`
- `applications_user/nfc_fuzzer/README.md`

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (reine `.fap`, keine
Firmware-Kernänderung).

## Risiko
- Niedrig-mittel: additiver externer App-Ordner ohne Firmware-Kernänderung. Fehlschlag zeigt
  sich beim Build (`ufbt`/APPCHK) als Compile-Fehler.
- **API-87-Kompatibilität**: gegen den eigenen `mntm-012`-Header-Stand verifiziert (Momentum-
  Firmware-Tag sparse ausgecheckt: `lib/nfc/nfc.h`, `lib/nfc/nfc_listener.h`,
  `lib/nfc/nfc_poller.h`, `lib/toolbox/bit_buffer.h`,
  `lib/nfc/protocols/{iso14443_3a,iso14443_3b,felica,iso15693_3}/*.h`,
  `lib/nfc/protocols/nfc_protocol.h`). Alle von der App tatsächlich verwendeten Signaturen 1:1
  vorhanden: `nfc_alloc`/`nfc_free`, `nfc_listener_alloc`/`_start`/`_stop`/`_free`,
  `nfc_listener_tx`, `nfc_poller_alloc`/`_start`/`_stop`/`_free`, `nfc_poller_trx`,
  `bit_buffer_alloc`/`_free`/`_reset`/`_copy_bytes`/`_write_bytes`/`_get_size_bytes`/`_get_data`,
  `iso14443_3a_alloc`/`_free`, `iso14443_3b_alloc`/`_free`/`_set_uid`, `felica_alloc`/`_free`/
  `_set_uid`, `NfcCommandContinue`/`NfcCommandStop`, `NfcErrorTimeout`/`NfcErrorNone`. Für die
  Port-Skizze zusätzlich benötigt und ebenfalls 1:1 vorhanden: `NfcProtocolIso15693_3`,
  `iso15693_3_alloc`/`_free`/`_set_uid` (`ISO15693_3_UID_SIZE = 8`),
  `Iso15693_3ListenerEventTypeCustomCommand` (aus `iso15693_3_listener.h`).
- `bit_buffer_write_bytes(const BitBuffer* buf, void* dest, size_t bytes)` kopiert laut Header
  vom `BitBuffer` **in** `dest` (nicht umgekehrt) — deckt sich mit der Verwendung in
  `nfc_fuzzer_worker.c` (`bit_buffer_write_bytes(rx_buf, result->response, rx_len)` liest die
  Antwort aus dem Puffer in die Ergebnisstruktur), also kein Bug an dieser Stelle, obwohl der
  Funktionsname beim ersten Lesen missverständlich wirkt.
- **Der gefundene Architekturfehler (Profil 5 "ISO15693 Fuzzing")**: kein Speicherzugriffsfehler,
  sondern ein stiller Funktionsausfall — die App stürzt nicht ab und meldet keinen Fehler,
  produziert aber gegen einen echten ISO15693-Reader ausschließlich Timeout-Ergebnisse, weil die
  gesendeten Bytes auf falscher Protokollebene (ISO14443-3A statt ISO15693-3) herausgehen. Ohne
  Fix vor dem Einsatz zu markieren/auszuklammern, da sonst falsch interpretierbare
  "Reader-robust"-Ergebnisse entstehen.
- Poller-Profile (`Reader Commands`, `MIFARE Auth`, `MIFARE Read/Write`, `RATS/ATS`) rufen
  `nfc_poller_trx()` ausschließlich aus dem Poller-Callback heraus auf (App-Kommentar: "Calling
  it from the worker thread triggers furi_check in SDK 1.4.3") — die Wartelogik im Worker-Thread
  pollt nur noch `ctx.done`, greift selbst nicht auf den Poller zu. Diese Architektur konnte ich
  nur durch Lesen der Header, nicht durch tatsächlichen Build/Flash bestätigen — beim lokalen
  Bauen/Testen gezielt gegen eine eigene MIFARE-Testkarte prüfen, da hier laut App-Kommentar
  bereits einmal ein SDK-Versionsunterschied (offizielle Firmware 1.4.3 vs. eigene
  Momentum-mntm-012-Basis) eine Rolle gespielt haben könnte.
- Fuzzing realer Reader/Terminals außerhalb eigener/autorisierter Systeme fällt unter
  CONSTRAINTS Punkt 5 (nur eigene Geräte, autorisierte Tests, CTF/Lehre) — die App selbst
  schränkt das nicht technisch ein, das bleibt Nutzungsdisziplin.

## Port-Skizze
1. App-Ordner `nfc_fuzzer` (alle unter „Betroffene Dateien" gelisteten Dateien) aus
   `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
   `applications/external/nfc_fuzzer/`) nach `applications_user/nfc_fuzzer/` kopieren.
2. Für Profil 5 in `nfc_fuzzer_worker.c` eine eigene Listener-Schleife nach dem bereits
   etablierten Muster der NFC-B-/FeliCa-Funktionen (`nfc_fuzzer_worker_run_listener_nfcb()` /
   `_felica()`) ergänzen, aber mit **kommandogetriggertem** Senden statt reinem
   UID-Rotieren, weil Profil 5 laut eigener Beschreibung Antworten auf die Inventory-Anfrage
   des Readers fuzzt, nicht die Tag-Identität selbst:

   ```c
   /* neu: nfc_fuzzer_worker_run_listener_iso15693() */
   typedef struct {
       BitBuffer* tx_buf;
       volatile bool cmd_received;
   } Iso15693FuzzCtx;

   static NfcCommand nfc_fuzzer_iso15693_listener_cb(NfcGenericEvent event, void* context) {
       Iso15693FuzzCtx* ctx = context;
       if(!event.event_data) return NfcCommandContinue;
       Iso15693_3ListenerEvent* ev = event.event_data;
       if(ev->type == Iso15693_3ListenerEventTypeCustomCommand) {
           ctx->cmd_received = true;
           nfc_listener_tx(event.instance, ctx->tx_buf);   /* fuzzed inventory response */
       }
       return NfcCommandContinue;
   }

   /* im Testfall-Loop: */
   uint8_t iso15693_uid[ISO15693_3_UID_SIZE] = {0xE0, 0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
   Iso15693_3Data* data = iso15693_3_alloc();
   iso15693_3_set_uid(data, iso15693_uid, sizeof(iso15693_uid));
   NfcListener* listener = nfc_listener_alloc(nfc, NfcProtocolIso15693_3, data);
   Iso15693FuzzCtx ctx = { .tx_buf = tx_buf, .cmd_received = false };
   nfc_listener_start(listener, nfc_fuzzer_iso15693_listener_cb, &ctx);
   /* pro Testfall: tx_buf mit test_case->data befüllen, ctx.cmd_received vor dem Warten
      zurücksetzen, auf ctx.cmd_received oder Timeout warten (Muster wie im bestehenden
      NfcFuzzerListenerCtx-Timeout-Loop), Anomalie wie gehabt auswerten. */
   ```

   `event.instance` ist im Listener-Callback laut `nfc_generic_event.h` je nach `event.protocol`
   die konkrete Protokoll-Instanz — `nfc_listener_tx()` erwartet aber ausdrücklich `Nfc*`
   (`lib/nfc/nfc.h`). Vor dem Einbau daher zunächst anhand eines bereits funktionierenden
   Referenzpfads (z. B. `nfc_fuzzer_listener_callback()` selbst nutzt `nfc_listener_tx(nfc, ...)`
   mit dem außen gehaltenen `Nfc*`, nicht mit `event.instance`) prüfen, ob dieselbe
   außen-gehaltene `Nfc*`-Referenz statt `event.instance` verwendet werden muss — im Zweifel die
   Variante mit der äußeren `nfc`-Variablen wählen, das ist im bestehenden Code für alle anderen
   Profile so gelöst und offensichtlich funktionsfähig.
3. In `nfc_fuzzer_worker_thread()` die Weiche um `NfcFuzzerProfileIso15693` ergänzen (analog zu
   `NfcFuzzerProfileNfcB`/`NfcFuzzerProfileFelica`), damit die neue Funktion statt der
   generischen `nfc_fuzzer_worker_run_listener()` aufgerufen wird; in letzterer im Gegenzug die
   drei `NfcFuzzerProfileIso15693`-Sonderfälle (Zeilen 205–209 und 218–221 in der Quelle)
   entfernen, da nicht mehr erreichbar.
4. `ufbt launch_app APPSRC=applications_user/nfc_fuzzer` bzw.
   `fbt launch_app APPSRC=applications_user/nfc_fuzzer` — deckt Header-/Signatur-Abweichungen
   als Compile-Fehler auf.
5. Funktionstest der reparierten Profil-5-Schleife: eigenen ISO15693/Vicinity-Reader (oder
   zweiten Flipper im ISO15693-Poll-Modus) gegen den emulierten Fuzz-Tag laufen lassen,
   Anomalie-Erkennung (Timeout/Unexpected/Timing) gegen bekannte Reader-Grenzfälle prüfen.
6. Regressionstest der zehn bereits korrekt verdrahteten Profile unverändert nach Poller-/
   Listener-Art (eigene MIFARE-/NTAG-/FeliCa-/ISO14443-3B-Testtags bzw. -Reader), insbesondere
   die vier Poller-Profile gegen den unter Risiko genannten `nfc_poller_trx()`-Aufrufkontext.
7. Bei erfolgreichem Test: als eigenständige Fuzzing-Kategorie neben NFC Sniffer (Analyse) und
   MFKey/Nested (Key-Recovery) führen.

## Testhinweis
Rein logikbasierter Fund (Protokoll-/Architekturfehler, kein Speicherzugriffsfehler) — durch
Volltextsuche über den kompletten App-Quellbestand belegt (kein `Iso15693_3`-Symbol außerhalb
von `nfc_fuzzer_profiles.c` und der Dokumentation) und durch Abgleich der tatsächlich benötigten
gegen die tatsächlich includierten NFC-Protokoll-Header verifiziert. Kein Host-Repro möglich, da
der Fehler ausschließlich im Zusammenspiel mit der echten NFC-HAL/RF-Ebene auftritt; die Analyse
selbst braucht keine Hardware, nur die Firmware-Header und den App-Quelltext, beides lokal
sparse ausgecheckt. Die übrigen 10 Profile beim Volltext-Review gegen `NFC_FUZZER_MAX_PAYLOAD_LEN`
und die jeweiligen `uint8_t`-Längenfelder in `nfc_fuzzer_profiles.c` einzeln nachgerechnet — kein
Overflow gefunden.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/nfc_fuzzer/`.
- Ursprungsquelle laut `application.fam`/`fap_weburl`: `barkandbite/flipper_suite` — dieselbe
  Quelle wie mein bereits verifizierter UART-Sniff-/Rogue-AP-Detector-/H10301-Writer-Fund.
- Eigener Fund: Profil "ISO15693 Fuzzing" sendet in der vorliegenden Fassung ausschließlich über
  eine ISO14443-3A-Listener-Session (`nfc_fuzzer_worker.c`), nie über einen tatsächlichen
  `NfcProtocolIso15693_3`-Listener — Beleg per Volltextsuche über den App-Quellbestand und
  Header-Abgleich gegen den eigenen `mntm-012`-Stand (siehe oben).
- Restlicher NFC-Werkzeug-Cluster aus demselben Rückstellpunkt weiterhin offen: `chameleon`,
  `nfc_relay`, `mifare_fuzzer`, `nfc_rfid_detector`, `nfctools`, `uhf_rfid`, `nfc_canary`,
  `nfc_comparator`, `simultaneous_rfid_reader`.
