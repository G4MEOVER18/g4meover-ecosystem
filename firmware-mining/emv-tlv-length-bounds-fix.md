# NFC EMV: kartenkontrollierte TLV-Längen ungebunden im Poller — Unleashed vs. mntm-012

Quelle: Unleashed-Firmware-Commit `15e208c0ca29f08ea90a5bc30dc3c2c5adc186d9` ("[NFC] Bound
card-controlled TLV lengths in the EMV poller", PR #1048, @Endika, gemerged 31.07.2026), aus dem
`unlshd-091`-Fenster als zweiter, bislang offener Restfund neben dem bereits portierten
Kaskadenstufen-/SAK-Paar (`firmware-mining/iso14443-3a-cascade-uid-overflow.md`,
`firmware-mining/iso14443-3a-sak-cascade-bit-fix.md`) vorgemerkt. Gegen Tag `mntm-012` geprüft:
`lib/nfc/protocols/emv/emv_poller_i.c` ist bis auf zwei kosmetische Unterschiede (dazu unten
mehr) identisch zum hier zitierten Vorher-Stand, `lib/nfc/protocols/emv/emv.h` ist byteidentisch.

## Was

`emv_decode_tlv_tag()` (`lib/nfc/protocols/emv/emv_poller_i.c`) verarbeitet TLV-Werte aus der
Kartenantwort: `tag`/`tlen` kommen aus `emv_parse_tag()`, das die Längen-Bytes direkt aus dem
Empfangspuffer (`buff[i]`, `uint8_t`, max. 255) liest — vollständig kartenkontrolliert, keine
Prüfung gegen die tatsächliche Antwortlänge oder gegen die Größe des jeweiligen Zielfeldes. Bei
mir landet dieser Wert in mehreren Fällen direkt als `memcpy()`-Länge in fest dimensionierte
Puffer:

```c
// lib/nfc/protocols/emv/emv_poller_i.c, emv_decode_tlv_tag() -- bei mir (mntm-012) so vorhanden:
case EMV_TAG_TRACK_1_EQUIV: {
    char track_1_equiv[80];
    memcpy(track_1_equiv, &buff[i], tlen);        // tlen bis 255, Ziel nur 80 Byte
    track_1_equiv[tlen] = '\0';                    // Schreibzugriff zusaetzlich ausserhalb bei tlen>=80
    ...
}
case EMV_TAG_APPL_NAME:
    furi_check(tlen < sizeof(app->application_name)); // furi_check() = Geraeteabsturz, kein Ablehnen
    memcpy(app->application_name, &buff[i], tlen);
    ...
case EMV_TAG_LOG_FMT:
    furi_check(tlen < sizeof(app->log_fmt));           // ebenso: furi_check() statt Ablehnen
    memcpy(app->log_fmt, &buff[i], tlen);
    ...
case EMV_TAG_AID:
    app->aid_len = tlen;                                // gar keine Pruefung, Ziel 16 Byte
    memcpy(app->aid, &buff[i], tlen);
    ...
case EMV_TAG_PAN:
    memcpy(app->pan, &buff[i], tlen);                   // gar keine Pruefung, Ziel 10 Byte
    app->pan_len = tlen;
    ...
```

Zusätzlich zu den direkten `memcpy`-Fällen liest `emv_parse_tag()` selbst `buff[i]` (Tag-Byte),
danach ggf. ein zweites Byte, danach das Längen-Byte, danach ggf. ein Byte für die 2-Byte-Länge —
**ohne an irgendeiner dieser Stellen `i` gegen `len` zu prüfen**. Eine Kartenantwort, die kürzer
ist als der erwartete Header (Timeout mit Teilantwort, absichtlich verkürzte Antwort eines
Angreifer-kontrollierten Emulators), lässt diese Funktion über das Ende von `buff` hinaus lesen.
`emv_decode_response_tlv()`/`emv_decode_tl()` verrechnen `tlen` danach ungeprüft weiter
(`i += tlen`) und rufen `emv_decode_tlv_tag(&buff[i], ...)` mit einem `tlen` auf, das über das
Ende der Kartenantwort hinausreicht — der eigentliche Lesezugriff in den `case`-Zweigen (das
`memcpy`) liest dann ebenfalls über `buff` hinaus.

**Eigener Fund, über den in der PR-Beschreibung zitierten Umfang hinaus:** `emv_trace()` und die
Track-2-ASCII-Konvertierung (`char track_2_equiv[41]`, derselbe unbegrenzte Schreibschleifen-Bug
wie bei Track 1, nur mit 2 Ausgabe-Byte pro Eingabe-Byte statt 1:1) stehen im
Unleashed-Vorher-Stand hinter `#ifndef LOGS_RELEASE_BUILD` und existieren in einem
Release-Build dort **gar nicht**. Bei mir (Momentum, gegen `mntm-012` verifiziert) fehlt genau
dieser Guard — der Block ist bei mir in **jedem** Build aktiv, auch im Release-Build, den ich
tatsächlich flashe. Das macht diesen einen Unterfall für mich strikt schwerwiegender als für
einen Unleashed-Release-Build: `track_2_equiv[41]` kann bei mir mit kartenkontrolliertem
`tlen` bis zu 510 Byte Schreibversuch erhalten (2 ASCII-Zeichen pro Eingabe-Byte), während der
Guard bei Unleashed selbst diesen Codepfad in der Praxis meist stilllegt.

## Nutzen

- Schließt eine echte, kartenkontrollierte Speicherzugriffslücke, die bei **jedem** EMV-Lesevorgang
  (`emv_poller_select_ppse`/`_select_application`/`_get_processing_options`/`_read_sfi_record`/
  `_req_get_data`/`_read_log_entry` — jede einzelne EMV-Poller-Funktion ruft
  `emv_decode_response_tlv()` oder `emv_decode_tl()` auf) durch eine böswillige/manipulierte
  Karte bzw. einen Karten-Emulator ausgelöst werden kann: Stack-Buffer-Overflow mit
  kartenkontrolliertem Inhalt (Track-1/Track-2-Felder), Heap-/Struct-Overflow (PAN, AID,
  Applikationsname, Log-Format, Transaktionshistorie), sowie mehrere `furi_check()`-Abstürze
  (garantierter Geräteneustart statt eines abgelehnten Tags) und OOB-Lesezugriffe bei verkürzten
  Antworten.
- Direkt relevant für meinen eigenen Einsatzzweck: Ich benutze die EMV-App als Pentest-Werkzeug,
  bei dem das Gerät bewusst unbekannte/nicht selbst kontrollierte Karten liest — genau das ist
  das Bedrohungsmodell, das dieser Fix abdeckt (eine präparierte Test-/Red-Team-Karte statt eines
  normalen Bankkarten-Lesevorgangs).
- Ergänzt die bereits portierten NFC-Stack-Fixes (DESFire/MFUL-Overflows, ISO15693,
  ISO14443-3A-Kaskade, FeliCa, LFRFID) um den bislang einzigen noch offenen Fund aus derselben
  Absicherungswelle für kartenkontrollierte Längen — EMV war die letzte in dieser Reihe noch
  ungeprüfte NFC-Protokollschicht.
- Verhält sich für spezifikationskonforme Karten identisch zu vorher (siehe Testhinweis): keine
  Funktionseinbuße im Erfolgsfall, nur zusätzliche Ablehnung/Kürzung bei Werten, die ohnehin
  nicht in die vorgesehenen Felder passen.

## Betroffene Dateien (Portumfang)

- `lib/nfc/protocols/emv/emv.h`
  - `PDOLValue`: neues Feld `uint8_t size;` (wie viele Byte der fest hinterlegte Wert tatsächlich
    hat — die Karte kann in ihrem eigenen PDOL eine andere Länge für dasselbe Tag anfordern).
  - `cardholder_name[24 + 1]` → `cardholder_name[26 + 1]` (EMV Book 3 Annex A: Tag 5F20 hat eine
    "ans"-Länge von 2–26, mein bisheriges Feld war einen Buchstaben zu knapp für den
    spezifikationskonformen Maximalwert).
- `lib/nfc/protocols/emv/emv_poller_i.c`
  - Alle vierzehn `pdol_*`-Konstanten: `size`-Feld ergänzt (Compound-Literal-Erweiterung, kein
    Verhaltensunterschied für sich allein).
  - `emv_trans_writable()` (neue statische Hilfsfunktion): prüft `active_tr < COUNT_OF(trans)`,
    ersetzt den bisherigen `furi_check()` am Ende von `emv_poller_read_log_entry()` durch ein
    kontrolliertes Abbrechen der Sweep-Schleife.
  - `emv_tag_rejected()` (neue statische Hilfsfunktion): einheitliches `FURI_LOG_W` +
    `return false` für jeden abgelehnten Tag, ersetzt die bisherigen `furi_check()`-Abstürze.
  - `emv_decode_tlv_tag()`: pro Tag-Fall eine Längenprüfung vor dem jeweiligen `memcpy`
    (`!=`/`>=`/`>` je nach Feldsemantik, siehe Port-Skizze), inkl. der beiden von mir zusätzlich
    identifizierten Fälle (`track_1_equiv`, `track_2_equiv` — bei mir ohne
    `LOGS_RELEASE_BUILD`-Guard, siehe oben).
  - `emv_parse_tag()`: `i`/`len`-Prüfung vor jedem der bis zu vier sequenziellen Lesezugriffe auf
    `buff[i]`.
  - `emv_decode_tl()`/`emv_decode_response_tlv()`: `i + tlen > len`-Prüfung, bevor der jeweilige
    Wertebereich als gültig behandelt wird.
  - `emv_prepare_pdol()`: `EMV_PDOL_MAX_SIZE`-Konstante (256 − 9, das tatsächliche
    ISO14443-4-Poller-Sendepuffer-Budget abzüglich Protokoll-Overhead) statt `furi_check()` gegen
    `sizeof(dest->data)`; kopiert zusätzlich `MIN(tlen, pdol_values[j]->size)` statt blind `tlen`
    Byte aus dem eigenen, kürzeren `data[]`-Array zu lesen (sonst Lesezugriff über die eigene
    PDOL-Konstante hinaus, wenn die Karte eine größere Länge fürs selbe Tag verlangt, als der
    Terminal-Wert tatsächlich hat).
  - `emv_poller_read_log_entry()`: `furi_check()`-Aufruf durch `emv_trans_writable()`-Prüfung mit
    `break` ersetzt.

## Risiko

- Moderater Umfang (ein Datei-Header plus eine Implementierungsdatei, ca. 15 Stellen), aber jede
  einzelne Änderung ist eine lokal begrenzte Bounds-Prüfung vor einem bestehenden Schreib-/
  Lesezugriff — keine neue Datenstruktur, kein API-Bruch außer der internen `PDOLValue`-
  Felderweiterung (rein additiv, nur intern in dieser Datei verwendet).
- Verhaltensänderung im Fehlerfall: wo vorher `furi_check()` das Gerät neu startete, wird der Tag
  jetzt abgelehnt und der Lesevorgang läuft mit unvollständigen Daten für dieses eine Feld
  weiter (protokolliert per `FURI_LOG_W`). Das ist die gewünschte Härtung, aber ein
  Verhaltensunterschied, den ich beim lokalen Funktionstest bewusst mit einer eigenen Karte
  gegenprüfen sollte (Lesevorgang bricht nicht mehr ab, wo er vorher — kontrolliert per
  `furi_check`-Neustart statt unkontrolliert — abbrach).
- `cardholder_name[24+1]` → `[26+1]`: reine Vergrößerung eines Felds in `EmvApplication`,
  wirkt sich nicht auf `flipper_format`-Dateikompatibilität aus (das Feld wird beim Speichern als
  String geschrieben, nicht mit fester Breite).
- Vor dem Übernehmen `EmvApplication`/`PDOLValue`-Layout und `emv_poller_i.c` erneut gegen meine
  aktuelle mntm-012-Basis gegenlesen (siehe Port-Skizze Schritt 2) — bei der Prüfung für diesen
  Vorschlag war die Datei bis auf die beiden oben genannten kosmetischen `LOGS_RELEASE_BUILD`-
  Unterschiede identisch zum zitierten Vorher-Stand.
- Der Build (`fbt`/`ufbt`) deckt jede Signatur-/Struktur-Abweichung als Compile-Fehler auf.
- Verwandter, bewusst **nicht** in diesen Vorschlag aufgenommener Fund aus demselben
  Absicherungs-Zeitfenster: Commit `2631895419075d7df8d71be7e3d93465fcc6a491` ("[NFC] Bound
  file-controlled lengths in the EMV .nfc loader", PR #1056) behebt dieselbe Fehlerklasse in
  `emv_load()`/`emv_save()` (Datei-kontrollierte statt Karten-kontrollierte Längen beim Laden
  einer gespeicherten `.nfc`-Datei) — eigenständiger Angriffsvektor (präparierte Datei statt
  präparierte Karte), eigene Datei-Stellen, für eine künftige Sitzung vorgemerkt.

## Port-Skizze

1. `lib/nfc/protocols/emv/emv.h` in meiner mntm-012-Basis öffnen: `PDOLValue` um `uint8_t size;`
   ergänzen (zwischen `tag` und `data[]`), `cardholder_name[24 + 1]` auf `cardholder_name[26 + 1]`
   vergrößern.
2. `lib/nfc/protocols/emv/emv_poller_i.c` gegen den oben zitierten Stand gegenlesen — insbesondere
   prüfen, ob die beiden `LOGS_RELEASE_BUILD`-Guards bei mir wie zuletzt geprüft (2026-08-22)
   weiterhin fehlen; falls ein künftiges Momentum-Update sie wieder einführt, ändert das den
   Schweregrad des Track-2-Falls, nicht die Notwendigkeit des Fixes selbst.
3. Alle vierzehn `pdol_*`-Konstanten um das `size`-Feld ergänzen (Wert = Länge des bereits
   vorhandenen `data`-Arrays je Konstante, siehe Upstream-Diff).
4. `emv_trans_writable()` und `emv_tag_rejected()` als neue statische Hilfsfunktionen einfügen.
5. In `emv_decode_tlv_tag()` pro Fall die passende Längenprüfung vor dem `memcpy` einfügen (Muster
   aus dem Upstream-Diff: `!=` bei fester Breite, `>=` wenn ein Byte für `\0` reserviert bleibt,
   `>` wenn das volle Feld nutzbar ist, weil ein separates `*_len` die tatsächlich gültige Länge
   trägt). Bei `EMV_TAG_TRACK_2_DATA`/`_EQUIV`: PAN-Schleife zusätzlich gegen `sizeof(app->pan)`
   und einen Mindest-Rest von 3 Byte (Trenner + 2 Byte Ablaufdatum) begrenzen; ASCII-Konvertierung
   gegen `sizeof(track_2_equiv)` begrenzen (bei mir unbedingt notwendig, siehe Nutzen/Risiko).
6. `emv_parse_tag()`: vor jedem der vier sequenziellen `buff[i]`-Zugriffe `if(i >= len) return
   success;` (mit `success` weiterhin `false`) einfügen.
7. `emv_decode_tl()`/`emv_decode_response_tlv()`: vor der Weiterverarbeitung des dekodierten Tags
   `(uint16_t)i + tlen > len` prüfen und bei Überschreitung abbrechen bzw. den Tag ablehnen.
8. `emv_prepare_pdol()`: `EMV_PDOL_MAX_SIZE`-Makro ergänzen, `furi_check()` durch die
   Kapazitätsprüfung plus `MIN(tlen, pdol_values[j]->size)`-Kopie plus Nullauffüllung des Rests
   ersetzen.
9. `emv_poller_read_log_entry()`: `furi_check()`-Aufruf durch `if(!emv_trans_writable(...)) {
   FURI_LOG_D(...); break; }` ersetzen.
10. Host-Repro (`repro.c`, unten) mit `gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined`
    bauen und laufen lassen, um den Stack-Overflow ALT vs. NEU zu bestätigen — bereits von mir
    lokal ausgeführt, siehe Testhinweis.
11. Build: `ufbt` (bzw. `fbt firmware_all`).
12. Funktionstest lokal: mit einer eigenen EMV-Testkarte (Bankkarte im eigenen Besitz oder
    EMV-Testkarten-Set) einen normalen Lesevorgang mehrfach wiederholen (Ergebnis unverändert:
    PAN/Ablaufdatum/Kartenname wie vorher). Danach, falls verfügbar, mit einem eigenen
    NFC-Emulator/Proxmark eine Antwort mit überlangem `tlen` für z. B. `EMV_TAG_APPL_LABEL`
    zurückspielen und prüfen, dass kein Geräteabsturz/-neustart mehr auftritt, wo er vorher
    (`furi_check`) auftrat.

## Testhinweis (lokal, ohne Hardware für die Kernlogik)

Host-kompilierbares Mini-Repro (`repro-emv-tlv-length-bounds-fix/repro.c`) der beiden aus meiner
Sicht schwerwiegendsten Einzelfälle: `EMV_TAG_TRACK_1_EQUIV` (Ziel `track_1_equiv[80]`) und die
Track-2-ASCII-Konvertierung (Ziel `track_2_equiv[41]`, bei mir ohne `LOGS_RELEASE_BUILD`-Guard).
ALT ist 1:1 die bei mir vorhandene Logik, NEU die gebundene Fassung aus Commit `15e208c0`. Jeder
Testfall läuft isoliert in einem `fork()`-Kindprozess. Gebaut/ausgeführt mit
`gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined`:

```
== Fall A: EMV_TAG_TRACK_1_EQUIV (Ziel: track_1_equiv[80]) ==
  ALT, tlen=37 (realistischer Track-1-Wert): kein Absturz
  NEU, tlen=37 (realistischer Track-1-Wert): kein Absturz
  ALT, tlen=255 (kartenkontrollierter Maximalwert): ABSTURZ (Stack-Buffer-Overflow, von ASan erkannt)
  NEU, tlen=255 (kartenkontrollierter Maximalwert): kein Absturz

== Fall B: EMV_TAG_TRACK_2 ASCII-Konvertierung (Ziel: track_2_equiv[41]) ==
(bei mir/Momentum ohne LOGS_RELEASE_BUILD-Guard in jedem Build aktiv)
  ALT, tlen=19 (realistischer Track-2-Wert): kein Absturz
  NEU, tlen=19 (realistischer Track-2-Wert): kein Absturz
  ALT, tlen=255 (kartenkontrollierter Maximalwert): ABSTURZ (Stack-Buffer-Overflow, von ASan erkannt)
  NEU, tlen=255 (kartenkontrollierter Maximalwert): kein Absturz

OK: beide Faelle bestaetigt -- ALT stuerzt bei kartenkontrolliertem tlen=255 in beiden Faellen
unter ASan ab (Stack-Buffer-Overflow mit kartenkontrolliertem Inhalt), NEU (Laengenpruefung/
abgebrochene Schreibschleife aus Commit 15e208c0) nicht. Fuer realistische, spezifikationskonforme
Werte verhalten sich ALT und NEU identisch (kein Verhaltensunterschied im Erfolgsfall).
```

Bestätigt: die ALT-Fassung überschreibt in beiden Fällen bei kartenkontrolliertem Maximalwert
(`tlen=255`) den jeweiligen Stack-Puffer über seine Grenze hinaus mit kartenkontrolliertem
Inhalt; die NEU-Fassung lehnt den überlangen Wert kontrolliert ab bzw. bricht die
Schreibschleife an der Puffergrenze ab. Für realistische, spezifikationskonforme Kartenwerte
(Track 1 < 79 Byte, Track 2 ≤ 19 Byte) verhalten sich beide Fassungen identisch — kein
Funktionsverlust im Regelfall. Die übrigen ~13 gebundenen Stellen (AID, PAN, Applikationsname,
Log-Format, PDOL, `emv_parse_tag`-Header-Bytes, Transaktionshistorie) folgen demselben
Bounds-Prinzip und wurden am Quellcode gegen den Upstream-Diff geprüft, aber nicht einzeln
host-repliziert (unterschiedliche, aber strukturell gleichartige `memcpy`-Fälle).

## Quelle

- `DarkFlippers/unleashed-firmware`, Commit `15e208c0ca29f08ea90a5bc30dc3c2c5adc186d9` (PR #1048,
  @Endika, gemerged 31.07.2026): „[NFC] Bound card-controlled TLV lengths in the EMV poller" —
  `lib/nfc/protocols/emv/emv.h`, `lib/nfc/protocols/emv/emv_poller_i.c`.
- Momentum-Firmware, Tag `mntm-012`: `lib/nfc/protocols/emv/emv_poller_i.c` (identisch zum
  zitierten Vorher-Stand bis auf zwei entfernte `LOGS_RELEASE_BUILD`-Guards, siehe „Was") und
  `lib/nfc/protocols/emv/emv.h` (byteidentisch) als Vergleichsbasis.
- Weiterhin offen aus demselben `unlshd-091`-Absicherungsfenster: Commit `26318954` (PR #1056,
  „Bound file-controlled lengths in the EMV .nfc loader") — dateikontrollierte statt
  kartenkontrollierte Längen in `emv_load()`/`emv_save()`, für eine künftige Sitzung vorgemerkt.
