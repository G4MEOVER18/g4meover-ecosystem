# MIFARE Plus: ATS-Nibble-Matching + SAK-Security-Level-Korrektur in der Karten-Erkennung

## Was

`firmware-mining/mfplus-phase2-sl3-struct-serialization.md` hatte im Abschnitt "Nicht Teil dieses
Patches" eine bei Unleashed inzwischen erweiterte Erkennungslogik in
`mf_plus_get_type_from_version()`/`mf_plus_get_type_from_iso4()` (`lib/nfc/protocols/mf_plus/
mf_plus_i.c`) als eigenständigen, zurückgestellten Fund vermerkt: eine SAK-0x20-Probe zur
Unterscheidung von Security Level 0 und Security Level 3 sowie eine Umstellung des
ATS-Produkt-Abgleichs von Volltreffer auf Nibble-Matching. Diese Sitzung greift genau diesen Punkt
auf.

Beim Nachvollziehen der echten Unleashed-Änderung (`dev`, Commit
`945234a76701d556346f131c88f29f0feeec4d52`, 2026-09-10, API 88.9) gegen meine eigene
`mntm-012`-Basis (Tag-Commit `e1784e7418d8b074e971983ceb6fef0f37e52ae4`) stellt sich heraus, dass
die eigentliche SL0-vs-SL3-Unterscheidung selbst **kein reiner Klassifizierungsschritt auf bereits
vorhandenen Daten ist**, wie zunächst angenommen, sondern einen neuen, aktiven Poller-Befehl
braucht (`mf_plus_poller_probe_security_level()` in `mf_plus_poller.c`, aufgerufen aus
`mf_plus_poller_resolve_sak20_security_level()`): eine zusätzliche APDU an die Karte, bevor
`mf_plus_get_type_from_version()`/`_get_type_from_iso4()` überhaupt aufgerufen werden. Beide
Funktionen bekommen dafür einen neuen dritten Parameter (`probed_security_level`), dessen einzige
Quelle dieser neue Poller-Befehl ist. Das ist echtes Karten-I/O und Poller-Zustandsautomat-Gebiet —
also derselbe hardware-blockierte Bereich wie Phase 3-6 aus `firmware-mining/
mfplus-mfultralight-aes-auth-port.md`, nicht die reine Nachverarbeitung bereits vorhandener
SAK-/ATS-/ATQA-Bytes, die ursprünglich vermutet wurde.

Ich liefere daher **nur den Teil der Änderung, der tatsächlich ohne neuen Karten-Zugriff
auskommt** und host-prüfbar ist — die eigentliche SL0-Erkennung bleibt offen (siehe "Nicht Teil
dieses Patches"). Konkret vier Korrekturen, alle ausschließlich auf bereits während eines normalen
Polls verfügbaren SAK-/ATQA-/ATS-Bytes:

1. **ATS-Produkt-Abgleich auf Nibble-Matching umgestellt** (neue Hilfsfunktion
   `mf_plus_type_from_ats()`): statt eines Vergleichs gegen vier feste 7-Byte-Referenzblöcke
   (`mf_plus_ats_t1_tk_values[]`) werden nur die tatsächlich diskriminierenden Nibbles geprüft
   (`C1 05 <Typ/Größe> <Gen> <Caps> <CRC16>` — die letzten beiden Bytes variieren je nach
   Konfiguration/Generation). Erkennt dadurch reale MIFARE-Plus-SE-Varianten, die meine
   bisherige Volltreffer-Tabelle ablehnt — laut NXP-TagInfo-Fingerabdrucktabelle (v6.2.0, im
   Unleashed-Quellkommentar zitiert) existieren zwei SE-Fingerabdrücke
   (`C1 05 21 30 00 77 C1` und `C1 05 20 30 00 AB 9B`), meine Tabelle deckt bislang nur die
   AN10833-Variante `..21 30 xx F6 D1` ab.
2. **ATQA-basierter Größen-Fallback** (neue Hilfsfunktion `mf_plus_size_from_atqa()`): wenn im
   GetVersion-Pfad das Storage-Byte nicht `0x16`/`0x18` ist (unbekannte/ältere Revision), wird die
   Speichergröße jetzt aus dem ATQA-Größenbit abgeleitet statt `MfPlusSizeUnknown` zu bleiben.
   Dieselbe Hilfsfunktion ersetzt im ISO4-Pfad (SAK 0x20, Nicht-SE) die bisherige inline
   Nibble-Prüfung `(atqa[0] & 0x0F) == 0x04/0x02`.
3. **SAK-zu-Security-Level-Zuordnung in `mf_plus_get_type_from_version()` korrigiert**: mein
   bisheriger Code ist ein reines Zweiweg-`if/else` (`sak == 0x20 ? SL3 : SL1`) — dabei fällt jede
   EV1/EV2-Karte mit SAK `0x10`/`0x11` (laut AN10833 Security Level 2) fälschlich auf SL1. Der
   neue `switch` unterscheidet `0x20`→SL3 (unverändert), `0x10`/`0x11`→SL2 (Korrektur),
   `0x08`/`0x18`→SL1 (unverändert), und lässt einen nicht erkannten SAK-Wert jetzt explizit auf
   `MfPlusSecurityLevelUnknown` stehen, statt weiter SL1 zu raten.
4. **SAK `0x10`/`0x11` im ISO4-Pfad meldet generisches `MfPlusTypePlus` statt hartkodiertem
   `MfPlusTypeX`**: die bloße SAK-Byte allein belegt nicht, dass es sich um die X-Variante statt S
   oder SE handelt (dieselbe Nibble-Matching-ATS-Prüfung wird an dieser Stelle gar nicht erreicht,
   da SAK `0x10`/`0x11` laut AN10833 immer Security Level 2 ist, in dem der ATS-Produkt-Fingerabdruck
   nicht zuverlässig zur Verfügung steht).

## Nicht Teil dieses Patches

- **Die eigentliche SL0-vs-SL3-Unterscheidung** (`MfPlusProbeResult`,
  `mf_plus_poller_probe_security_level()`, der neue `probed_security_level`-Parameter selbst):
  braucht einen zusätzlichen aktiven Poller-Befehl an die Karte — echtes Karten-I/O, gehört fachlich
  zu Phase 3 (Poller-Zustandsautomat) aus `firmware-mining/mfplus-mfultralight-aes-auth-port.md`
  und bleibt aus demselben Grund wie diese offen: ohne echte Hardware nicht sinnvoll zu testen. SAK
  `0x20` bleibt bei mir deshalb weiterhin unbedingt `MfPlusSecurityLevel3` (wie vor diesem Patch) —
  ich behaupte an keiner Stelle, SL0 erkennen zu können. Der Backlog-Punkt "SL0-vs-SL3-Erkennung"
  ist damit nur zum host-testbaren Teil (ATS-/SAK-Klassifizierung) abgeschlossen, nicht zur
  vollständigen SL0-Erkennung selbst — die bleibt zusammen mit Phase 3-6 vorgemerkt.
- **`mf_plus.h`/`mf_plus_i.h`**: unverändert. Die beiden Funktionssignaturen bleiben bei mir
  zweiargumentig (`iso14443_4a_data`/`iso4_data`, `mf_plus_data`) statt wie bei Unleashed um
  `probed_security_level` erweitert — ohne den Probe-Aufrufer wäre ein dritter Parameter ungenutzt.
- **`mf_plus.c`/Phase-2-SL3-Nutzlast**: unberührt, wie bereits in der Phase-2-Sitzung festgestellt
  — diese Erkennungsänderung betrifft ausschließlich die beiden hier gepatchten Funktionen.

## Nutzen

- Erkennt reale MIFARE-Plus-SE-Karten mit einem der beiden TagInfo-Fingerabdrücke, die meine
  bisherige feste 4-Werte-Tabelle nicht abdeckt, statt sie mit `MfPlusErrorProtocol` abzulehnen.
- Behebt eine reale Fehlklassifizierung: eine EV1/EV2-Karte mit Security Level 2 (SAK `0x10`/
  `0x11`) wird über den GetVersion-Pfad aktuell fälschlich als SL1 gemeldet — mit entsprechend
  falscher Erwartung an Authentifizierung/Secure Messaging bei einem künftigen Poller.
- Vermeidet eine stille Falschangabe: ein nicht erkannter SAK-Wert führte bisher zu einem
  geratenen SL1 statt zu einem ehrlichen "unbekannt".
- Vermeidet eine Typ-Überbehauptung: SAK `0x10`/`0x11` meldete bislang immer "X", obwohl die SAK
  allein das nicht hergibt.
- Erweitert den Größen-Fallback über ATQA für den Fall eines nicht erkannten Storage-Bytes, statt
  `MfPlusSizeUnknown` stehen zu lassen.
- Bleibt klein, additiv und auf eine Datei begrenzt — keine Struct-Änderung, keine
  Poller-Verdrahtung, keine neue Kartenkommunikation.

## Betroffene Dateien

Patch als Unified-Diff gegen meine `mntm-012`-Basis
(`e1784e7418d8b074e971983ceb6fef0f37e52ae4`) in `mfplus-sl0-sl3-detection-port/`:

- `mf_plus_i.c.patch` — ersetzt die feste `mf_plus_ats_t1_tk_values[]`-Tabelle durch die beiden
  neuen statischen Hilfsfunktionen `mf_plus_type_from_ats()`/`mf_plus_size_from_atqa()` und passt
  `mf_plus_get_type_from_version()`/`mf_plus_get_type_from_iso4()` entsprechend an (Signaturen
  unverändert). Keine andere Funktion in der Datei berührt.

Lokale Anwendung: `patch -p0 < mfplus-sl0-sl3-detection-port/mf_plus_i.c.patch` aus
`lib/nfc/protocols/mf_plus/` heraus (`patch --dry-run` gegen eine frische Kopie meiner eigenen
Basisdatei vorab verifiziert, real angewendet und Byte-für-Byte gegen die host-getestete Fassung
abgeglichen — identisch).

## Risiko

- **Niedrig**: reine Ersetzung der Matching-Logik innerhalb zweier bereits bestehender Funktionen,
  keine Struct-/Signaturänderung, keine neue Kartenkommunikation. Der ATS-Längen-Gate
  (`historical_bytes_len == MF_PLUS_T1_TK_VALUE_LEN`) am Funktionsanfang bleibt unverändert
  bestehen — ein zu kurzer/fehlender ATS-Block wird weiterhin sofort mit `MfPlusErrorProtocol`
  abgelehnt, bevor `mf_plus_type_from_ats()` überhaupt aufgerufen wird.
- Der Nibble-Matcher prüft weiterhin explizit `historical_bytes[0..1] == C1 05` und die
  Typ-Nibble, bevor er irgendeinen Typ zurückgibt — eine DESFire-Karte (ebenfalls SAK `0x20`,
  aber mit einem 5-Byte- statt 7-Byte-ATS-Block und ohne `C1 05`-Präfix) wird dadurch weiterhin
  korrekt nicht als MIFARE Plus erkannt (siehe Testgruppe 2 unten).
- Einziges verbleibendes Risiko: SAK `0x20` liefert bei mir weiterhin ausschließlich SL3 — eine
  echte SL0-Karte würde also (wie schon vor diesem Patch) fälschlich als SL3 klassifiziert, bis
  Phase 3 den Probe-Befehl liefert. Das ist keine Regression dieses Patches, sondern der bewusst
  zurückgestellte Rest desselben Funds.

## Host-Repro

`firmware-mining/repro-mfplus-sl0-sl3-detection-port/repro.c`
(`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined`), Ergebnis `ALL CHECKS PASSED`
(101 von 101 Checks, keine ASan-/UBSan-Meldung). Bildet ALT (byte-identisch zu meiner
`mf_plus_i.c`) und NEU (die hier gelieferte Fassung, mit dem entfernten `probed_security_level`-
Parameter) als zwei getrennte Funktionspaare über denselben minimalen Feld-für-Feld-Nachbau von
`Iso14443_3aData`/`Iso14443_4aData`/`MfPlusData`/`SimpleArray` nach; die Eingaben sind öffentliche
NXP-AN10833-/TagInfo-Protokollkonstanten (ATS-Referenzblöcke, ATQA-Werte), keine erfassten
Kartendaten.

Geprüft:

1. **Nibble-Matching findet TagInfo-SE-Varianten, die die alte Volltreffer-Tabelle ablehnt** —
   sowohl direkt bei SAK `0x08` als auch hinter dem SAK-`0x20`-Gate; ALT lehnt beide Varianten mit
   `MfPlusErrorProtocol` ab, NEU erkennt beide korrekt als SE.
2. **Nibble-Matcher lehnt weiterhin Nicht-Plus-ATS ab**: ein synthetischer 7-Byte-Block ohne
   `C1 05`-Präfix wird von ALT und NEU gleichermaßen abgelehnt; ein echter 5-Byte-DESFire-
   ATS-Block scheitert bereits am unveränderten Längen-Gate in beiden Fassungen.
3. **ATQA-Größen-Fallback** (GetVersion-Pfad, nicht erkanntes Storage-Byte): NEU leitet 2K/4K
   korrekt aus dem jeweiligen ATQA-Bit ab, wo ALT bei `MfPlusSizeUnknown` bleibt; bei fehlenden
   ATQA-Größenbits bleiben beide Fassungen `Unknown` (keine Regression); ein erkanntes
   Storage-Byte gewinnt in beiden Fassungen weiterhin gegenüber einem widersprüchlichen ATQA-Wert.
4. **SAK→Security-Level-Korrektur**: SAK `0x10`/`0x11` liefert bei ALT fälschlich SL1, bei NEU
   korrekt SL2; ein nicht erkannter SAK-Wert liefert bei ALT weiterhin geratenes SL1, bei NEU
   `MfPlusSecurityLevelUnknown`; SAK `0x08`/`0x18`/`0x20` und ein Nicht-EV1/2-`hw_type` verhalten
   sich in ALT und NEU identisch (Regression).
5. **SAK `0x10`/`0x11` meldet generisches `MfPlusTypePlus`** statt hartkodiertem `MfPlusTypeX`
   (ISO4-Pfad); das Ergebnis ist nachweislich unabhängig vom tatsächlichen ATS-Inhalt (nur die
   Länge muss stimmen, da der gemeinsame Längen-Gate am Funktionsanfang für alle SAK-Werte gilt);
   Größe/Security-Level bleiben unverändert.
6. **SAK-`0x20`-SL0/SL3-Nichtregression**: für S- und X-Karten liefern ALT und NEU an dieser
   Stelle beide ausschließlich SL3 — an keiner Stelle wird SL0 behauptet, exakt das in "Nicht Teil
   dieses Patches" beschriebene Verhalten.
7. **Volle Regressionsmatrix über die ursprüngliche ALT-Referenztabelle** (alle vier
   Referenzblöcke, SAK `0x08`/`0x18`/`0x20`, beide Größenzweige bei `0x20`): Typ, Größe und
   Security Level sind zwischen ALT und NEU für jeden der zwölf Referenzfälle identisch.

## Testhinweis (lokal)

1. `mf_plus_i.c.patch` in `lib/nfc/protocols/mf_plus/` anwenden, `ufbt` (bzw. `fbt firmware_all`)
   bauen — reine Funktionslogik ohne Struct-/Signaturänderung, sollte ohne Compile-Fehler
   durchlaufen.
2. Host-Repro laufen lassen (Befehl im Dateikopf von `repro.c`) — sollte weiterhin
   `ALL CHECKS PASSED` liefern, bevor überhaupt geflasht wird.
3. Auf dem Gerät: eine bereits vorhandene MIFARE-Plus-S/X-Karte (SAK `0x08`/`0x18`/`0x20`) lesen —
   muss weiterhin identisch zu vor diesem Patch als S bzw. X erkannt werden (Regressionstest der
   beiden bereits durch die alte Tabelle abgedeckten Fingerabdrücke).
4. Falls verfügbar: eine MIFARE-Plus-SE-Karte mit einer der beiden neu unterstützten
   TagInfo-Konfigurationsvarianten lesen — sollte jetzt als SE statt mit "Sak xx but no known
   Mifare Plus type" (Log-Tag `MfPlus`) fehlschlagen.
5. Eine SL2-Karte (SAK `0x10`/`0x11`) über den GetVersion-Pfad lesen, sofern eine solche Karte zur
   Verfügung steht — die gespeicherte `.nfc`-Datei sollte "Security Level: SL2" statt des vor
   diesem Patch fälschlich gemeldeten SL1 zeigen.
6. Die eigentliche SL0-Erkennung bleibt ungetestet, da sie erst mit Phase 3 (Poller-Probe-Befehl)
   überhaupt existiert — siehe "Nicht Teil dieses Patches".

## Quelle

- Unleashed-Firmware, `dev`-Branch, Commit `945234a76701d556346f131c88f29f0feeec4d52`
  (2026-09-10, API 88.9): `lib/nfc/protocols/mf_plus/mf_plus_i.c` (Erkennungslogik),
  `mf_plus_poller.c` (zeigt den nicht mitportierten Probe-Aufrufer,
  `mf_plus_poller_resolve_sak20_security_level()`/`mf_plus_poller_probe_security_level()`),
  `mf_plus_i.h` (neuer `probed_security_level`-Parameter, nicht übernommen). Zum Zeitpunkt dieser
  Sitzung existiert kein neuerer `unlshd-`-Tag als der bereits für Phase 1/2 gepinnte Stand; ich
  bleibe bewusst bei demselben Commit für Konsistenz mit den bereits gelieferten Phasen.
- Momentum-Firmware, Tag `mntm-012` (Commit `e1784e7418d8b074e971983ceb6fef0f37e52ae4`):
  `lib/nfc/protocols/mf_plus/mf_plus_i.c`, Ausgangszustand für den Diff.
- Vorangehende Sitzungen: `firmware-mining/mfplus-mfultralight-aes-auth-port.md` (Phase 1),
  `firmware-mining/mfplus-phase2-sl3-struct-serialization.md` (Phase 2, dort erstmals als offener
  Punkt vermerkt).
