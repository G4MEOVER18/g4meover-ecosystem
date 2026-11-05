# NFC: zwei `NfcProtocol[]`-`memcpy()`-Stellen kopieren die Elementzahl statt der Bytezahl — Unleashed vs. mntm-012

Quelle des Fundes: beim Texkom-Phase-5-Port (letzte Sitzung) als Nebenfund notiert, bislang nicht
bearbeitet. Zwei unabhängige `memcpy()`-Aufrufe im NFC-Scanner bzw. in der Auswahlliste der
erkannten Protokolle kopieren jeweils `count`/`filtered_protocols_num` **Bytes** statt
`count * sizeof(NfcProtocol)` **Bytes** — `NfcProtocol` ist ein gewöhnliches `enum` ohne
expliziten Basistyp, auf meinem Zielsystem (STM32WB55, Cortex-M4) also 4 Byte groß, nicht 1.

Gegen Tag `mntm-012` geprüft: beide Stellen sind bei mir unverändert im alten, zu kurzen Zustand
vorhanden. Unleashed-`dev` hat beide bereits korrigiert — getrennt, in zwei inhaltlich
unabhängigen Commits.

## Was

**1. `lib/nfc/nfc_scanner.c`, `nfc_scanner_filter_detected_protocols()`:**

```c
// bei mir (mntm-012) so:
instance->detected_protocols_num = filtered_protocols_num;
memcpy(instance->detected_protocols, filtered_protocols, filtered_protocols_num);
```

`filtered_protocols` und `instance->detected_protocols` sind beide `NfcProtocol[NfcProtocolNum]`
(gleich große Arrays, kein Bounds-Problem) — aber `filtered_protocols_num` ist eine **Element**-
Anzahl, kein Byte-Offset. Die Kopie trifft also nur die ersten `filtered_protocols_num` **Bytes**
des Zielarrays, nicht die ersten `filtered_protocols_num` **Elemente**.

**2. `applications/main/nfc/helpers/nfc_detected_protocols.c`, `nfc_detected_protocols_set()`:**

```c
// bei mir (mntm-012) so:
memcpy(instance->protocols_detected, types, count);
instance->protocols_detected_num = count;
```

Identisches Muster, andere Stelle. Aufgerufen aus `nfc_scene_detect.c`
(`nfc_scene_detect_scan_callback()`) direkt mit dem Ergebnis des Scanners
(`event.data.protocols`/`event.data.protocol_num`) — betrifft also denselben Protokoll-Erkennungs-
Pfad wie Fund 1, nur einen Schritt weiter in der Auswahlliste, die am Ende der NFC-Lesesitzung dem
Nutzer zur Protokollwahl angezeigt wird.

**Tatsächliche Auswirkung** (per Host-Repro bestätigt, siehe Testhinweis): solange nach der
Filterung/Übernahme nur **ein** Protokoll übrig bleibt, bleibt der Fehler in der Praxis folgenlos
— die höherwertigen drei Bytes des Zielelements sind durch eine frühere, vollständige
Enum-Zuweisung bereits auf 0, die 1-Byte-Teilkopie trifft zufällig trotzdem den richtigen Wert.
Sobald **mehr als ein** Protokoll übrig bleibt (z. B. eine MIFARE-Plus-Karte, die sowohl als
`MfClassic` als auch als `MfPlus` erkannt und nach der Elternteil-Filterung mit zwei
nicht-verwandten Einträgen weitergegeben wird — der von Unleashed in Commit `9ef0b565e` zitierte
Ausgangsfall, PR #1016/#1017), bleiben alle Elemente ab Index 1 vollständig unberührt und zeigen
den *alten* Inhalt des Zielarrays aus einer vorherigen Scan-Phase statt des tatsächlich erkannten
zweiten Protokolls. Kein Speicherfehler (beide Arrays bleiben innerhalb ihrer Grenzen) — ein reiner
Korrektheitsfehler in der Protokollerkennung bei Mehrprotokoll-Karten.

## Nutzen

- Betrifft genau die Fälle, für die der NFC-Scanner die Mehrprotokoll-Filterung überhaupt
  eingebaut hat: eine Karte, die mehrere voneinander unabhängige (nicht in Eltern-Kind-Beziehung
  stehende) Protokolle gleichzeitig anbietet. Ohne den Fix zeigt die App in diesem Fall
  möglicherweise ein falsches zweites (oder weiteres) Protokoll zur Auswahl an, statt des
  tatsächlich erkannten — bei einem eigenen autorisierten Pentest-Gerät mit Mehrprotokoll-
  Unterstützung (z. B. kombinierte MIFARE-Classic/Plus-Testkarten) ein irreführendes statt ein
  falsches/gefährliches Ergebnis, aber dennoch eine fehlerhafte Protokollerkennung.
- Für den weit überwiegenden Regelfall (genau ein erkanntes Protokoll) ändert sich nichts am
  beobachtbaren Verhalten (siehe Testhinweis, Regressionsfall 1) — der Fix korrigiert ausschließlich
  den bislang falsch behandelten Mehrprotokoll-Fall.
- Kleiner, in zwei Dateien klar abgegrenzter Fund direkt im Anschluss an den abgeschlossenen
  Texkom-Phase-5-Port (letzte Sitzung), bei dem er als Nebenfund notiert wurde.

## Betroffene Dateien (Portumfang)

- `lib/nfc/nfc_scanner.c`
  - `nfc_scanner_filter_detected_protocols()`: `memcpy(...)`-Aufruf um
    `* sizeof(NfcProtocol)` ergänzt (eine Zeile, auf drei Zeilen umformatiert wie im
    Unleashed-Original).
- `applications/main/nfc/helpers/nfc_detected_protocols.c`
  - `nfc_detected_protocols_set()`: `memcpy(...)`-Aufruf um `* sizeof(NfcProtocol)` ergänzt (eine
    Zeile).

Keine Änderung an Headern, Signaturen oder Aufrufstellen nötig — beide Fixe sind rein interne
Korrekturen der jeweiligen Funktion.

## Risiko

- Minimalster mögliche Umfang: zwei Implementierungsdateien, je ein geänderter `memcpy()`-Aufruf,
  keine neuen Felder/Symbole, keine API-/Struct-Änderung.
- Verhaltensänderung ausschließlich im bisher fehlerhaften Mehrprotokoll-Fall (siehe Testhinweis,
  Fall 2/3); der Regelfall mit genau einem erkannten Protokoll bleibt unverändert (Regressionsfall
  1), ebenso der Fall ohne erkanntes Protokoll (Regressionsfall 2).
- Der Build (`fbt`/`ufbt`) deckt jede Signaturabweichung als Compile-Fehler auf; `sizeof(NfcProtocol)`
  ist ein triviales, bereits im selben Header definiertes Konstrukt, keine neue Abhängigkeit.
- Beide Fixe sind voneinander unabhängig wirksam (zwei getrennte `memcpy()`-Stellen in zwei
  Dateien) — ein Fix kann unabhängig vom anderen übernommen werden, falls gewünscht.

## Port-Skizze

1. `lib/nfc/nfc_scanner.c` und `applications/main/nfc/helpers/nfc_detected_protocols.c` gegen
   meine mntm-012-Basis gegenlesen — bei der Prüfung für diesen Vorschlag an den betroffenen
   Stellen unverändert zum zitierten Vorher-Stand.
2. Patches anwenden (aus dem Firmware-Wurzelverzeichnis):
   - `patch -p1 < nfc-protocol-array-memcpy-byte-count-fix/nfc_scanner_c.patch`
   - `patch -p1 < nfc-protocol-array-memcpy-byte-count-fix/nfc_detected_protocols_c.patch`
3. Host-Repro (`repro.c`, unten) mit
   `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O0`
   (bzw. `-O1`) bauen und laufen lassen — bereits von mir lokal ausgeführt, siehe Testhinweis.
4. Build: `ufbt` (bzw. `fbt firmware_all`).
5. Funktionstest lokal: eine gewöhnliche Einzelprotokoll-Karte scannen — Ergebnis unverändert zu
   vorher (Protokollanzeige/-auswahl identisch). Danach, falls verfügbar, eine Mehrprotokoll-
   Testkarte (z. B. MIFARE Plus im SL1-Modus, die sowohl `MfClassic`- als auch
   `MfPlus`-Erkennung auslöst) scannen: vor dem Fix zeigt die Protokollauswahl möglicherweise ein
   falsches zweites Protokoll (Altwert aus einer vorherigen Scan-Phase), nach dem Fix das
   tatsächlich erkannte zweite Protokoll.

## Testhinweis (lokal, ohne Hardware — reine Array-/Speicherlogik)

Host-kompilierbares Mini-Repro
(`repro-nfc-protocol-array-memcpy-byte-count-fix/repro.c`) bildet beide `memcpy()`-Stellen
(ALT = bei mir vorhandene Fassung, NEU = gebundene Fassung) mit demselben `NfcProtocol`-Enum
nach. Kein Speicherfehler zu erwarten (beide Arrays bleiben innerhalb ihrer Grenzen) — die Prüfung
erfolgt per Wertevergleich, nicht per Sanitizer-Befund:

```
sizeof(NfcProtocol) = 4

== Fall 1 (Regression): nfc_scanner_filter_detected_protocols(), 1 verbleibendes Protokoll ==
  ALT: Slot 0 zufaellig trotzdem korrekt (hoeherwertige Bytes waren bereits 0): ok
  NEU: Slot 0 zeigt korrekt MfPlus: ok

== Fall 2: nfc_scanner_filter_detected_protocols(), 2 verbleibende Protokolle ==
  ALT: Slot 1 bleibt vollstaendig unberuehrt (alter Wert statt MfDesfire): ok
  NEU: Slot 0/1 zeigen korrekt MfClassic/MfDesfire: ok

== Fall 3: nfc_detected_protocols_set(), 2 erkannte Protokolle ==
  ALT: zweites Protokoll in der Auswahlliste bleibt faelschlich Slix statt MfPlus: ok
  NEU: Auswahlliste zeigt korrekt MfClassic/MfPlus: ok

== Regressionsfall 2: count == 0 (kein Protokoll erkannt) ==
  ALT und NEU verhalten sich bei 0 Protokollen identisch (kein Funktionsverlust): ok

ALL CHECKS PASSED
```

Gebaut/ausgeführt mit
`gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei
`-O0`/`-O1`: identischer Output, keine Compiler-Warnungen, keine Sanitizer-Befunde (erwartungsgemäß,
da kein Speicherfehler vorliegt — nur falsche Werte im Regelfall mit mehr als einem Protokoll).

Fall 1 bestätigt zusätzlich, warum der Fehler bislang unauffällig blieb: im weit überwiegenden
Regelfall (genau ein erkanntes Protokoll) sind die höherwertigen Bytes des Zielelements durch eine
vorherige vollständige Enum-Zuweisung bereits 0, sodass die 1-Byte-Teilkopie zufällig trotzdem den
richtigen Wert ergibt. Erst ab zwei verbleibenden Protokollen (Fall 2/3) wird der Fehler
beobachtbar.

Zwei unabhängige, frische Zweitkopien der betroffenen Dateien aus meiner eigenen mntm-012-Basis
(`git show mntm-012:...`) jeweils per `patch -p1` und per `git apply` gegengeprüft: beide
betroffenen Dateien laut `md5sum` über alle Anwendungswege byte-identisch.

## Quelle

- `Next-Flip/momentum-firmware`, Tag `mntm-012`: `lib/nfc/nfc_scanner.c` (Commit `d92b0a82c`,
  "NFC refactoring (#3050)", 2023-10-24) und
  `applications/main/nfc/helpers/nfc_detected_protocols.c` (Commit `f73d60cba`,
  "[FL-3766] Refactor detected protocols list (#3809)", 2024-08-01) als Vergleichsbasis — beide
  Dateien enthalten die zu kurze `memcpy()`-Kopie bereits seit ihrer jeweiligen Erstfassung.
- `DarkFlippers/unleashed-firmware`, `dev`-Zweig:
  - Commit `9ef0b565ef1b4fb4c0f8d2797c8df73b98cad2bc` ("[NFC]: Read MIFARE Plus 2K (SL1) as full
    32 sectors / 64 keys (#1016)", 2026-07-03) behebt die `nfc_scanner.c`-Stelle als Teilaspekt
    ("Fix a pre-existing NfcProtocol[] memcpy size bug in the scanner that multi-protocol
    (Classic + Plus) cards exercise", schließt #917) — hier nur dieser eine `memcpy()`-Fix
    übernommen, nicht der übrige, deutlich umfangreichere MIFARE-Plus-2K-Sektorenumfang dieses
    Commits.
  - Commit `2b4411f234ac9e798d941410e45ded1121fede40` ("[NFC]: cut the app's resident RAM by 54%
    by moving protocol scenes into their plugins (#1073)", 2026-08-20) behebt die
    `nfc_detected_protocols.c`-Stelle ebenfalls nur als Teilaspekt — hier nur dieser eine
    `memcpy()`-Fix übernommen, nicht der übrige RAM-Umbau dieses Commits.
