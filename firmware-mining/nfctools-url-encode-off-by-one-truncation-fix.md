# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: URL-Encoder schneidet Bitcoin-Nachricht/Suchbegriff am Pufferende ab

## Was
Ich habe im NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`, Quelle
`github.com/wakdev/nfctools-fz`, aktueller Stand `f0325d67`) den bislang noch nicht einzeln
betrachteten gemeinsamen URL-Encoder-Helfer `url_encode()` (`core/nfc_tools_ndef.c:529`) geprüft,
den die NDEF-Schreibpfade für Bitcoin-Zahlungslinks und für Web-Suchanfragen gemeinsam nutzen
(`nfc_tools_ndef_build()`, Fälle `NdefTypeBitcoin`/`NdefTypeSearch`). Anlass war der bereits in
`firmware-mining/nfctools-icode-cc-mlen-overflow-fix.md` dokumentierte Fund derselben
Fehlerklasse (Off-by-eins bei einer Größenberechnung) — hier trifft es keine Feldgröße, sondern
direkt die Schleifengrenze des Encoders selbst.

## Fund: Off-by-eins in der Schleifenbedingung von `url_encode()` schneidet den kodierten String am Pufferende ab
```c
static void url_encode(const char* src, char* dest, size_t dest_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for(size_t i = 0; src[i] != '\0' && j + 4 < dest_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~') {
            dest[j++] = (char)c;
        } else {
            dest[j++] = '%';
            dest[j++] = hex[(c >> 4) & 0x0F];
            dest[j++] = hex[c & 0x0F];
        }
    }
    dest[j] = '\0';
}
```
Beide Aufrufstellen dimensionieren den Zielpuffer exakt auf den Worst-Case (jedes Zeichen wird
escaped, 3 Byte + Nullterminator):
```c
// Bitcoin-Nachricht (core/nfc_tools_ndef.c:667-668)
enc_msg = malloc(raw_len * 3 + 1);
url_encode(app->ndef_buf3, enc_msg, raw_len * 3 + 1);

// Suchbegriff (core/nfc_tools_ndef.c:707-708)
encoded = malloc(kw_raw_len * 3 + 1);
url_encode(app->ndef_buf1, encoded, kw_raw_len * 3 + 1);
```
Die Schleifenbedingung `j + 4 < dest_size` verlangt vor jedem Zeichen ein Byte mehr Reserve, als
für "bis zu 3 Byte schreiben + Nullterminator setzen" tatsächlich nötig ist (richtig wäre
`j + 4 <= dest_size`, äquivalent `j + 3 < dest_size`). Bei einem exakt worst-case-dimensionierten
Puffer — wie an beiden Aufrufstellen — bricht die Schleife dadurch **einen Schreibvorgang zu
früh** ab, sobald der bereits geschriebene Anteil `j` nahe genug an `dest_size` heranreicht,
unabhängig davon, ob das nächste Zeichen selbst escaped werden müsste oder nicht. Das
funktionsseitig gesetzte `dest[j] = '\0'` danach maskiert den Abbruch vollständig — es gibt keinen
Fehlerpfad, keine Rückmeldung, der String wird einfach kürzer als eingegeben.

Konkrete Auswirkung (siehe Host-Repro): jeder Bitcoin-Zahlungslink (`bitcoin:<adresse>?message=...`)
oder jede Web-Such-URL, deren `message`-/Suchbegriff-Feld auch nur teilweise aus Zeichen außerhalb
`[A-Za-z0-9._~-]` besteht (Leerzeichen, `!`, `&`, `:`, Satzzeichen — praktisch jeder normale
Freitext), verliert stillschweigend ihr Ende. Im Extremfall (Eingabe besteht nur aus einem
einzelnen Zeichen, escaped oder nicht) wird das Feld komplett zu einem leeren String — der
geschriebene NDEF-Tag bleibt syntaktisch gültig, transportiert aber eine verstümmelte
Bitcoin-Nachricht bzw. einen verstümmelten/leeren Suchbegriff, ohne dass "NFC Tools" beim
Schreiben oder beim eigenen Rücklesen etwas davon bemerkt.

## Host-Repro
Eigenständiges Host-Repro (kein Flipper-/NFC-Bezug), `url_encode()` byteidentisch aus
`core/nfc_tools_ndef.c` übernommen (ALT) neben der Ein-Zeichen-Korrektur (NEU), beide gegen einen
unabhängig neu geschriebenen Referenz-Prozent-Encoder verglichen:
```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```
Tatsächliche Ausgabe (lokal ausgeführt):
```
[regression: empty            ] input=               ref=                     ALT=                     (match)  NEU=                     (match)
[regression: alnum only       ] input=SatoshiNakamoto123 ref=SatoshiNakamoto123   ALT=SatoshiNakamoto123   (match)  NEU=SatoshiNakamoto123   (match)
[regression: single alnum     ] input=A              ref=A                    ALT=                     (TRUNCATED)  NEU=A                    (match)
[BUG: single escaped char     ] input=!              ref=%21                  ALT=                     (TRUNCATED)  NEU=%21                  (match)
[BUG: two escaped chars       ] input=!!             ref=%21%21               ALT=%21                  (TRUNCATED)  NEU=%21%21               (match)
[BUG: escaped + alnum tail    ] input=!A             ref=%21A                 ALT=%21                  (TRUNCATED)  NEU=%21A                 (match)
[BUG: bitcoin message         ] input=for coffee!    ref=for%20coffee%21      ALT=for%20coffee%21      (match)  NEU=for%20coffee%21      (match)
[BUG: search keyword          ] input=C&A shop       ref=C%26A%20shop         ALT=C%26A%20shop         (match)  NEU=C%26A%20shop         (match)
[BUG: all escaped, longer     ] input=!!!!!          ref=%21%21%21%21%21      ALT=%21%21%21%21         (TRUNCATED)  NEU=%21%21%21%21%21      (match)
```
"regression: alnum only" (kein Zeichen escaped, deutlich unter der Worst-Case-Pufferreserve) und
"BUG: bitcoin message"/"BUG: search keyword" (realistische, längere Eingaben mit gemischtem
Escaping) bestätigen: solange genug Pufferreserve vorhanden bleibt, sind ALT und NEU
identisch — der Fix ändert am üblichen Fall nichts. Die drei kurzen/eng dimensionierten Fälle
zeigen den Fund: ALT verliert das letzte Zeichen (`A`, `!`) komplett auf leer, verliert bei `!!`
und `!A` ein Zeichen, und verliert bei `!!!!!` das letzte escaped-Zeichen — NEU liefert in allen
neun Fällen exakt die Referenzkodierung.

## Betroffene Dateien
- `core/nfc_tools_ndef.c` — `url_encode()` (Zeile 529-545), einzige Fix-Stelle; wirkt sich über
  die beiden bestehenden Aufrufstellen auf `NdefTypeBitcoin` (`?message=`-Feld, Zeile 667-668) und
  `NdefTypeSearch` (Suchbegriff, Zeile 707-708) aus. Keine Signatur-/Aufrufstellenänderung nötig.

## Risiko
Sehr gering. Reiner Datenkorrektheitsfehler, keine Speichersicherheitsfrage: `enc_msg_len`/
`kw_enc_len` werden nach dem Encode-Aufruf jeweils per `strlen()` auf dem tatsächlich
geschriebenen (ggf. verkürzten) Ergebnis neu bestimmt, bevor die restliche `payload_len`-Berechnung
und der abschließende `memcpy` darauf aufbauen — die Payload-Größe und ihr Inhalt bleiben
untereinander konsistent, nur inhaltlich verstümmelt. Der Fix ändert eine einzelne Vergleichsoperation
(`<` → `<=`) in einer bereits vorhandenen Bedingung, keine neue Fallunterscheidung, keine geänderte
Pufferdimensionierung an den Aufrufstellen nötig.

## Port-Skizze / Fix
```c
static void url_encode(const char* src, char* dest, size_t dest_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for(size_t i = 0; src[i] != '\0' && j + 4 <= dest_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~') {
            dest[j++] = (char)c;
        } else {
            dest[j++] = '%';
            dest[j++] = hex[(c >> 4) & 0x0F];
            dest[j++] = hex[c & 0x0F];
        }
    }
    dest[j] = '\0';
}
```
Einziger Unterschied zum Original: `j + 4 < dest_size` → `j + 4 <= dest_size`. Herleitung: vor
jedem Zeichen sind im schlechtesten Fall 3 Byte Nutzdaten plus 1 Byte für den späteren
Nullterminator nötig, also `j + 3 + 1 <= dest_size`. Die bestehende Bedingung verlangt ein Byte
mehr Reserve als das (`j + 4 < dest_size` ⇔ `j + 5 <= dest_size`) und bricht dadurch beim
worst-case-dimensionierten Puffer der beiden Aufrufstellen vorzeitig ab.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-url-encode-off-by-one/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: "NFC Tools" → Write → Bitcoin, eine Adresse und
als Nachricht einen kurzen Text mit Leerzeichen/Satzzeichen eingeben (z. B. "for coffee!"), auf
einen eigenen leeren NDEF-Tag schreiben, danach mit einem unabhängigen NDEF-Leser (zweiter
Flipper/Smartphone-NFC-App) den geschriebenen `bitcoin:`-URI auslesen und mit der Eingabe
vergleichen. Vor dem Fix: das Nachrichtenfeld ist am Ende abgeschnitten oder bei kurzen
Eingaben komplett leer, ohne dass "NFC Tools" beim Schreiben einen Fehler meldet. Nach dem
Fix: der `message=`-Wert entspricht exakt der Prozent-Kodierung der Eingabe. Gleiches Vorgehen
für Write → Search mit einem Suchbegriff, der Leerzeichen/Sonderzeichen enthält.

## Zusätzlich geprüft, kein Fund
- Alle übrigen Zweige von `nfc_tools_ndef_build()` (`NdefTypeUnitLink`, `NdefTypeUrl`/
  `NdefTypeCustomUri`, `NdefTypeText`, `NdefTypeWifi` — bereits in einer vorherigen Sitzung
  gegengeprüft —, `NdefTypeContact`, `NdefTypeLocation`, `NdefTypeSocial`, `NdefTypeCustomData`,
  `NdefTypeBluetooth`, `NdefTypeSms`, `NdefTypeFacetime`) nutzen `url_encode()` nicht und bauen
  ihre Payload-Längen direkt aus `strlen()` der jeweiligen Eingabepuffer — kein Fund dieser
  Fehlerklasse.
- Die ~34 Schreib-Szenen unter `scenes/write/` (Tastatur-Eingabe-Wrapper für die einzelnen
  `ndef_bufN`-Felder, inkl. Kontakt/Bitcoin/WLAN/Social/Location/Custom/Search/SMS/FaceTime)
  vollständig gelesen: reine `keyboard_set_result_callback()`-Wrapper ohne eigene Kodier- oder
  Längenlogik, kein Fund. `core/nfc_tools_search.c`/`core/nfc_tools_social.c` (statische
  Präfix-/Suffix-Tabellen) ebenfalls ohne Fund.

## Quelle
- `github.com/wakdev/nfctools-fz`, Commit `f0325d67` (Stand dieser Sitzung, direkt vom
  Original-Repo geklont statt aus dem RogueMaster-Bündel, wie bereits in der vorherigen
  Schreib-Szenen-Sitzung).
- API-87-Einordnung: appeigene String-/Pufferlogik ohne Firmware-API-Bezug — kein
  firmwareversionsabhängiges Kompatibilitätsrisiko.
- Eigener Fund: `core/nfc_tools_ndef.c`, `url_encode()`, mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-nfctools-url-encode-off-by-one/repro.c`).
- Damit ist der zuvor offene Rückstellpunkt "~35 Schreib-Szenen für Kontakt/Bitcoin/WLAN/Social/
  etc." des NFC-Tools-Clusters abgeschlossen (Schreib-Szenen selbst ohne Fund, gemeinsamer
  URL-Encoder mit Fund). Restlicher Cluster-Umfang: nur noch `chameleon`/`nfc_relay`
  (Fremdgerät/Zweitgerät, außerhalb reiner Quelltextprüfung) offen.
