# NFC FeliCa-Emulation: Session haengt nach abgelehnter Anfrage bis zum naechsten Field-Off/On — Unleashed-dev vs. mntm-012

## Was
Aus dem aktuellen Unleashed-`dev`-Stand (Commit `3fb1d517`, 2026-10-13, PR #4465 "NFC: Fix
FeliCa command handling") einen der zehn darin gebündelten Teilfixe isoliert aufgegriffen: die
erste Teilüberschrift des PRs, "Fix FeliCa emulation hanging after rejected commands". Die
übrigen neun Teilfixe desselben PRs (Standard-FeliCa-Multisystem-Emulation: Service-/
Block-List-Validierung, atomare Block-Writes, PMm-Dump-Schutz, IDm-basiertes System-Switching
usw.) bauen durchgehend auf einer `FelicaSystem`/`FelicaService`-Infrastruktur auf, die in
meiner `mntm-012`-Basis (gegen `G4MEOVER18/G4MEOVER-FW`, Commit `b359f6f`, Tag `v2.0.0`
geprüft) schlicht nicht existiert — mein `felica_listener.c` entspricht einem deutlich
älteren, einfacheren Entwicklungsstand der FeliCa-Standard-Emulation (ein einzelnes,
festes Blockset statt mehrerer Systeme/Services). Diese neun Teilfixe sind bei mir daher
nicht portierbar, ohne vorher die gesamte Multisystem-Emulation selbst nachzuziehen —
eigener, deutlich größerer Rückstellpunkt (siehe "Weiterhin offen" unten).

Der erste Teilfix dagegen betrifft ausschließlich `felica_listener_run()`
(`NfcEventTypeRxEnd`-Zweig), eine Funktion, die bei mir **byte-identisch** zum
Unleashed-`dev`-Stand unmittelbar vor PR #4465 ist (gegen eine frische, unabhängige
`DarkFlippers/unleashed-firmware`-Zweitkopie, Commit `3fb1d517^`, abgeglichen) — ebenso die
dort aufgerufenen Hilfsfunktionen `felica_listener_check_idm()`,
`felica_listener_process_system_code()` und `felica_listener_process_request()` sowie das
`FelicaError`-Enum in `felica.h`. Der Fix ist bei mir 1:1 anwendbar, unabhängig von der
fehlenden Multisystem-Infrastruktur.

**Fund:** `felica_listener_run()` behandelt den `NfcEventTypeRxEnd`-Fall über ein
`do { ... } while(false)`-Konstrukt mit mehreren `break`-Ausstiegen. `command` ist vorab fest
auf `NfcCommandContinue` gesetzt und wird nur in **einem einzigen** Pfad (Polling mit einem
System-Code, den `felica_listener_process_system_code()` als `FelicaErrorFeatureUnsupported`
zurückweist) explizit auf `NfcCommandReset` gesetzt. Alle anderen Ablehnungspfade —

- falsche CRC,
- falsche Request-Länge (`felica_listener_check_block_list_size()` schlägt fehl),
- eine "Hardware Polling command leaking through"-Anfrage,
- eine falsche IDm (`felica_listener_check_idm()` schlägt fehl — die Anfrage gilt einer
  anderen/zurückgesetzten Karte),
- ein Fehler beim eigentlichen Frame-Exchange in `felica_listener_process_request()`
  (z. B. Feld-Abbruch mitten in der TX, Timeout) —

verlassen das `do/while` einfach per `break`, ohne `command` zu verändern. Da in keinem dieser
Fälle tatsächlich ein Response gesendet wurde (bei CRC-/Längenfehler wird die Anfrage gar nicht
erst geparst; bei den übrigen Fällen entscheidet der Code bewusst, nicht zu antworten), bleibt
der NFC-Stack auf `NfcCommandContinue` stehen, statt die Listener-Session neu zu bewaffnen.
In der Praxis bedeutet das: nach jeder abgelehnten Anfrage — und das ist kein Rand-, sondern
der Normalfall bei jedem Reader, der zuerst sondiert/pollt, bevor er eine Karte "kennt" — bleibt
die Emulation stehen, bis ein externer Field-Off/-On-Zyklus (`NfcEventTypeFieldOff`, ruft
`felica_listener_reset()`) sie wieder reaktiviert. Ein Reader, der nach einer abgelehnten
Anfrage erneut pollt, ohne das Feld dazwischen abzuschalten (z. B. mehrere Polling-Zyklen mit
unterschiedlichen System-Codes in derselben Sitzung, oder ein Reader, der nach einer falschen
IDm sofort nachfragt, statt das Feld neu aufzubauen), bekommt auf keine weitere Anfrage mehr
eine Antwort — die emulierte Karte wirkt für den Rest der Sitzung "tot", obwohl Hard-/Software
intakt sind.

## Nutzen
- Schließt einen Verfügbarkeits-Bug in der eigenen FeliCa-Kartenemulation (Transit-/
  Zugangskarten-Kontext, ergänzt die bereits dokumentierten FeliCa-Poller-Funde
  `nfc-felica-list-system-uninitialized-response-fix.md` auf der Emulationsseite), der bei
  jedem Reader auftreten kann, der mehr als eine Anfrage pro Feld-Zyklus stellt — kein
  präpariertes Verhalten des Readers nötig, nur mehr als eine (abgelehnte) Anfrage.
- Rein additiv: eine lokale `FelicaError error`-Variable ersetzt die bisherige
  Fallunterscheidung `if(error == FelicaErrorFeatureUnsupported)`. Der bisherige Sonderfall
  (System-Code nicht unterstützt → Reset) bleibt über denselben Mechanismus erhalten,
  ergänzt um alle bisher unbehandelten Ablehnungspfade. Der Erfolgsfall (Antwort erfolgreich
  gesendet, `error == FelicaErrorNone`) ist unverändert `NfcCommandContinue`.
- Ein neuer Enum-Wert `FelicaErrorIgnore` (identisch zum Namen/Zweck aus Unleasheds PR
  übernommen) markiert die beiden bewusst stillen Ablehnungspfade (Hardware-Polling-Leak,
  falsche IDm) von echten Protokollfehlern — nur zur Lesbarkeit, keine Verhaltensänderung
  gegenüber den Protokollfehler-Fällen (beide lösen am Ende denselben Reset aus).
- Kein API-Bruch: `FelicaError` ist ein internes Enum von `lib/nfc/protocols/felica/felica.h`,
  neue Werte am Ende sind rückwärtskompatibel; `felica_listener_run()`s Signatur und
  aufrufende Stellen (`nfc_listener_felica.run`) bleiben unverändert.

## Betroffene Dateien (lokal)
- `lib/nfc/protocols/felica/felica.h`: ein neuer Enum-Wert `FelicaErrorIgnore` am Ende von
  `FelicaError` (Patch: `felica_h.patch`).
- `lib/nfc/protocols/felica/felica_listener.c`, Funktion `felica_listener_run()`
  (`NfcEventTypeRxEnd`-Zweig): lokale `FelicaError error`-Variable eingeführt, auf jedem
  Ablehnungspfad gesetzt, finale Entscheidung `command = NfcCommandReset` von
  `error == FelicaErrorFeatureUnsupported` auf `error != FelicaErrorNone` verallgemeinert
  (Patch: `felica_listener_c.patch`).

## Risiko
- Niedrig: keine neue Logik in den eigentlichen Protokoll-Handlern
  (`felica_listener_process_request()`, `felica_listener_process_system_code()`,
  `felica_listener_check_idm()` bleiben unverändert) — nur die bereits vorhandene
  Fehlerinformation wird jetzt konsequent bis zur finalen `command`-Entscheidung
  durchgereicht, statt an einzelnen Stellen verworfen zu werden.
- Der bisherige einzige Reset-Fall (System-Code nicht unterstützt) verhält sich exakt wie
  vorher, da `FelicaErrorFeatureUnsupported != FelicaErrorNone` weiterhin wahr ist.
- Der Erfolgspfad (gültige Anfrage, Antwort erfolgreich gesendet) ist unverändert, da
  `felica_listener_process_request()`/`felica_listener_process_system_code()` dort
  weiterhin `FelicaErrorNone` liefern.
- Vor dem Übernehmen `felica_listener_run()` sowie die Signaturen von
  `felica_listener_check_idm()`/`felica_listener_process_system_code()`/
  `felica_listener_process_request()` in der eigenen `mntm-012`-Basis gegenlesen — bei mir
  identisch zum zitierten Stand (siehe Port-Skizze). Hat Momentum die Datei inzwischen in
  Richtung der Multisystem-Emulation weiterentwickelt, ist der Fix entsprechend anzupassen
  statt blind zu übernehmen.
- Der Build (`fbt`/`ufbt`) deckt jede Signatur-/Enum-Abweichung als Compile-Fehler auf.
- Funktionale Prüfung ohne Hardware nur eingeschränkt möglich (siehe Testhinweis): der
  Kontrollfluss ist host-seitig nachgestellt und verifiziert, der tatsächliche
  Radio-/Timing-Effekt (Session haengt wirklich bis Field-Off) braucht einen echten Reader
  und eine eigene, emulierte FeliCa-Karte.

## Port-Skizze
1. `lib/nfc/protocols/felica/felica.h` öffnen, gegenlesen, ob das `FelicaError`-Enum weiterhin
   mit `FelicaErrorFeatureUnsupported` endet (wie oben zitiert) — falls ja, `felica_h.patch`
   anwenden (neuer Wert `FelicaErrorIgnore` am Ende).
2. `lib/nfc/protocols/felica/felica_listener.c::felica_listener_run()` gegenlesen, ob der
   `NfcEventTypeRxEnd`-Zweig weiterhin wie oben zitiert aussieht (insbesondere die fünf
   `break`-Ausstiege und die bisherige `if(error == FelicaErrorFeatureUnsupported)`-Prüfung).
   Falls ja, `felica_listener_c.patch` anwenden.
3. Host-seitiges Kontrollfluss-Repro (`repro.c`, siehe Testhinweis) mit
   `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined` bauen und laufen lassen — bereits
   von mir lokal ausgeführt.
4. Build: `ufbt` (bzw. `fbt firmware_all`).
5. Funktionstest lokal: mit einer eigenen FeliCa-Karte (oder einem zweiten Flipper als Reader)
   die Emulation starten, mehrfach hintereinander pollen/lesen, dabei mindestens einmal eine
   Anfrage erzeugen, die abgelehnt wird (z. B. ein Polling mit einem fremden System-Code, oder
   zwei Leseversuche kurz hintereinander ohne das Feld dazwischen abzuschalten) — vor dem Fix
   bleibt die Karte nach der abgelehnten Anfrage für den Rest der Sitzung stumm, nach dem Fix
   antwortet sie auf die nächste gültige Anfrage wieder normal, ohne dass das Feld
   zwischendurch ab- und wieder angeschaltet werden muss.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Kontrollfluss-Repro (`repro.c`) von `felica_listener_run()`s
`NfcEventTypeRxEnd`-Zweig, ALT (mntm-012-Stand, 1:1 wie oben zitiert) gegen NEU (eigener Fix)
nebeneinander, mit gemockten `felica_crc_check()`/`felica_listener_check_block_list_size()`/
`felica_listener_process_system_code()`/`felica_listener_check_idm()`/
`felica_listener_process_request()` — deckt nur den Kontrollfluss ab (welchen Wert `command`
am Ende annimmt), nicht die FeliCa-Bitlogik selbst (die hängt an Nfc-/BitBuffer-/
SimpleArray-Typen, die hier nicht nachgebaut sind). Acht Szenarien, davon fünf abgelehnte
Anfragen (CRC falsch, Länge falsch, Hardware-Polling-Leak, falsche IDm, Frame-Exchange-Fehler
bei sonst gültiger Anfrage) und drei erfolgreiche (System-Code nicht unterstützt — bereits vor
dem Fix korrekt zurückgesetzt; System-Code erfolgreich beantwortet; Anfrage erfolgreich
verarbeitet). Gebaut/ausgeführt mit
`gcc -std=c17 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`:

```
ALT weicht von NEU ab in 5/8 Faellen.
Davon ALT=Continue bei abgelehnter Anfrage (= der 'Hang'-Bug): 5 Faelle.
NEU weicht vom erwarteten Zielverhalten ab: 0 Faelle.
ERGEBNIS: bestaetigt -- ALT laesst 5 von 8 abgelehnten Anfragen ohne Reset, NEU setzt in
jedem dieser Faelle korrekt zurueck; der Erfolgsfall (Continue) bleibt in beiden Versionen
unveraendert.
```

Identischer Output bei `-O0` und `-O1`, keine Compiler-/Sanitizer-Meldungen. Bestätigt: ALT
lässt in allen fünf Ablehnungsfällen `command` auf `NfcCommandContinue` stehen (der Hang-Bug),
NEU setzt in genau diesen fünf Fällen korrekt auf `NfcCommandReset` zurück, der bereits vorher
korrekte Sonderfall sowie beide Erfolgsfälle bleiben in beiden Versionen identisch
(`NfcCommandReset` bzw. `NfcCommandContinue`).

## Weiterhin offen
- Die übrigen neun Teilfixe aus PR #4465 (Standard-FeliCa-Multisystem-Emulation: Service-/
  Block-List-Validierung inkl. erweiterter Block-Adressierung, atomare Block-Writes,
  PMm-Dump-Schutz während der Emulation, Timeout-Berechnung aus PMm, IDm-basiertes
  System-Switching, neue SF1/SF2-Statuskonstanten) setzen eine `FelicaSystem`/
  `FelicaService`-Multisystem-Infrastruktur voraus, die in meiner `mntm-012`-Basis fehlt —
  eigener, deutlich größerer Rückstellpunkt (erst die Multisystem-Emulation selbst nachziehen,
  dann die neun Fixe darauf aufsetzen). Ähnlich einzuordnen wie der bereits zurückgestellte
  `texkom`-NFC-Protokollfamilien-Kandidat (mehrteilig, eigener Rückstellpunkt).
- Die übrigen seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
  MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
  `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
  Ducky-Handler, Hitag-S-Phase-2-Integrationsschicht) bleiben unverändert offen.

## Quelle
- `DarkFlippers/unleashed-firmware`, `dev`-Branch, PR #4465 (@kormax, gemerged 13.10.2026):
  „NFC: Fix FeliCa command handling" — Teilfix „NFC: Fix FeliCa emulation hanging after
  rejected commands" aus `lib/nfc/protocols/felica/felica_listener.c`/`felica.h`. Commit
  `3fb1d517` (bzw. `3fb1d517^` als Vorher-Stand, gegen eine frische, unabhängige
  Zweitkopie geprüft).
- `G4MEOVER18/G4MEOVER-FW`, Commit `b359f6f` (Tag `v2.0.0`, 2026-08-18) als eigene
  Vergleichsbasis — `felica_listener.c`/`felica.h` byte-identisch zum oben zitierten
  Unleashed-Vorher-Stand in den hier geänderten Funktionen/Enum geprüft.
