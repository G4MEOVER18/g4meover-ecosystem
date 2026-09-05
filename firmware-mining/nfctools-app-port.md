# NFC Tools — externe App aus RogueMaster-Bündel (NDEF-Lese-/Schreib-Werkzeug)

## Was
Ich habe den letzten offenen Kandidaten aus dem "restlichen NFC-Werkzeug-Cluster" aufgegriffen
(siehe `firmware-mining/nfc-comparator-app-port.md`): **NFC Tools**
(`applications/external/nfctools/`, Autor `wakdev`, `fap_version` 1.3, Quelle
`github.com/wakdev/nfctools-fz`). Mit rund 11 200 Zeilen über 55 Dateien ist das der größte
verbleibende Kandidat des Clusters — in dieser Sitzung habe ich bewusst nur den NDEF-Kern
(`core/nfc_tools_ndef.c`, 1080 Zeilen: Parser, strukturierter Parser, Encoder, alle Record-Typ-
Decoder) vollständig gelesen und geprüft, statt die gesamte App oberflächlich zu überfliegen.
Die restlichen ~10 100 Zeilen (Mifare-Classic-/DESFire-/ICODE-/NTAG-Lesepfade in
`core/nfc_tools_mfc.c`/`_desfire.c`/`_icode.c`/`_ntag.c`, ~35 Schreib-Szenen für
Kontakt/Bitcoin/WLAN/Social/etc., Sektor-Analyse, Passwort-Entfernung, Tag-Formatierung/-Sperre)
bleiben für künftige Sitzungen offen.

Nur die eingebaute Antenne über die Standard-`NfcDevice`-/Poller-APIs, `targets=["f7"]`, keine
Zusatzhardware — dasselbe Muster wie bei Mifare Fuzzer/NFC Fuzzer/NFC Comparator, die ich bereits
gegen `mntm-012` verifiziert habe. Die App liest NDEF-Nachrichten von Type-2-Tags (Mifare
Ultralight/NTAG) und kann strukturierte NDEF-Datensätze (URL, Text, WLAN-Zugangsdaten, vCard,
Bitcoin-URI, SMS/Mail/Anruf-Links, Social-Media-Profile, Custom-MIME/-URI) sowohl lesen/anzeigen
als auch neu auf einen Tag schreiben. Damit ergänzt sie meinen Bestand um eine mir fehlende
Kategorie: NDEF-Payload-Erstellung/-Interpretation, nützlich um zu prüfen, was ein fremder
NFC-Tag (Plakat, Visitenkarte, Gerät mit NFC-Pairing) tatsächlich auslösen würde, bevor man ihn
mit einem eigenen Lesegerät antippt, bzw. um eigene Tags für autorisierte Tests zu beschreiben.

## Fund: `strstr()` auf nicht terminiertem Tag-Payload in der vCard-Zusammenfassung (OOB-Read)
`ndef_fill_record()` (`core/nfc_tools_ndef.c:250`) füllt für jeden geparsten NDEF-Datensatz ein
`NfcToolsNdefRecord` (Rohdaten, Zusammenfassungstext, Struktur-Feld `value`) für die
Detail-/Listenansicht. Im `tnf == 0x02 && type == "text/vcard"`-Zweig (Zeile 302–317) wird das
Namensfeld so extrahiert:

```c
const char* fn = strstr((const char*)payload, "FN:");
if(fn) {
    const char* end = fn + 3;
    size_t max = payload_len - (size_t)(fn - (const char*)payload) - 3;
    ...
}
```

`payload` ist dabei ein reiner Zeiger in einen Heap-Puffer, den die Aufrufer exakt auf die
gelesene Tag-Datenlänge allozieren (`malloc(data_len)` in
`scenes/read/nfc_tools_scene_read_tag.c:345`, `malloc(total)` in `core/nfc_tools_mfc.c:196`,
ebenso in `core/nfc_tools_desfire.c:1035`) — ohne angehängtes Nullbyte. `strstr()` kennt aber
keine Längengrenze: enthält der vCard-Payload eines präparierten Tags kein `"FN:"` innerhalb
seiner eigenen `payload_len` Bytes, liest die Funktion über das Ende der Allokation hinaus
weiter, bis sie zufällig ein Nullbyte oder ein `"FN:"`-Muster im angrenzenden Speicher trifft.

Findet sie ein solches Muster **außerhalb** von `payload_len`, unterläuft zusätzlich die
Längenberechnung: `fn - payload >= payload_len`, also wird `max = payload_len - (fn-payload) - 3`
als `size_t` negativ und damit riesig — die anschließende Kopierschleife liest dann bis zu
`sizeof(rec->value)-1` Bytes aus beliebigem angrenzendem Speicher in `rec->value`, das direkt in
der Tag-Detailansicht angezeigt wird. Auf dem eingebetteten Ziel (STM32WB55, keine
MMU/Seitenschutz) bedeutet das je nach Speicherlayout entweder einen Absturz (`furi_check`/Hard
Fault beim Verlassen des gültigen Heaps) oder eine Informationslücke: Bytes aus einer
benachbarten Heap-Allokation (z. B. Teile eines anderen gerade aktiven NFC-Datensatzes oder
internen App-Zustands) werden als vermeintlicher vCard-Name angezeigt.

Bemerkenswert: dieselbe Datei enthält direkt daneben `ndef_decode_vcard()` (Zeile 148, genutzt
für die reine Textanzeige über `nfc_tools_ndef_parse_type2_tag()`), die exakt dasselbe Problem
— Felder aus einem nicht terminierten Payload extrahieren — bereits korrekt mit einer auf `len`
begrenzten Byte-für-Byte-Suche löst (`while((size_t)(p - src) < len) ...`). Der `strstr()`-Aufruf
im strukturierten Zweig (`ndef_fill_record()`) ist die einzige Stelle im Datei, die auf das
sicherere, bereits vorhandene Muster nicht zurückgreift — ein Nacharbeitungsfehler, keine
bewusste Designentscheidung.

Auslösbar durch jeden Type-2-Tag (Ultralight/NTAG) mit einer NDEF-Nachricht, deren TNF/Typ als
`text/vcard` deklariert ist und dessen Payload kein `"FN:"` enthält — ein Angreifer kann einen
solchen Tag beliebig präparieren und einer Zielperson zum Scannen anbieten (klassisches
NFC-Tag-Drop-Szenario), ohne dass ein gültiges vCard-Format eingehalten werden muss.

## Host-Repro
Eigenständiges Host-Repro (kein Flipper/keine NFC-Abhängigkeit), das den `text/vcard`-Zweig auf
seine Kontrolllogik reduziert (ALT = `strstr()`-Variante 1:1 wie im Original, NEU = auf
`payload_len` begrenzte Suche nach demselben Muster wie in `ndef_decode_vcard()`):

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

Tatsächliche Ausgabe (lokal ausgeführt):
```
case 1 (well-formed FN:) -- NEU: found=1 value="Jane Doe"
case 1 (well-formed FN:) -- ALT: found=1 value="Jane Doe"
case 2 (no FN:, unterminated payload) -- NEU: found=0 value="" (expected: not found, no OOB access)
case 2 (no FN:, unterminated payload) -- ALT: ==================...
==ERROR: AddressSanitizer: heap-buffer-overflow ... READ of size 38
0x504000000035 is located 0 bytes after 37-byte region [0x504000000010,0x504000000035)
...
==ABORTING
```

Fall 1 (Regressionscheck, echtes `"FN:Jane Doe"` vollständig innerhalb von `payload_len`): ALT
und NEU liefern identisches, korrektes Ergebnis — der Fix ändert am normalen Anwendungsfall
nichts. Fall 2 (kein `"FN:"`, Payload exakt `malloc(payload_len)` groß, kein Füllbyte): NEU
erkennt korrekt "nicht gefunden" ohne Speicherzugriff außerhalb des Puffers; ALT liest exakt am
Allokationsende vorbei — ASan meldet das deterministisch als `heap-buffer-overflow` genau an der
erwarteten Adresse (0 Bytes nach der 37-Byte-Region). Auf echter Hardware ohne ASan wäre das
je nach folgendem Speicherinhalt ein Absturz oder ein Fehlanzeige-/Leak-Fall wie oben
beschrieben, statt eines kontrollierten Abbruchs.

## Betroffene Dateien
- `core/nfc_tools_ndef.c` — `ndef_fill_record()`, `text/vcard`-Zweig (Zeile ~302–317 im
  Sparse-Checkout-Stand dieser Sitzung), Fix wie unten.

## Risiko
Gering. Reine Ersetzung eines unbegrenzten `strstr()`-Aufrufs durch eine auf `payload_len`
begrenzte Byte-für-Byte-Suche — exakt das Muster, das `ndef_decode_vcard()` in derselben Datei
für dasselbe Problem bereits verwendet, keine neue Abhängigkeit, keine geänderte
Puffergröße/-struktur. Verhalten bei einem wohlgeformten Payload (Regressionsfall im Repro)
bleibt unverändert.

## Port-Skizze / Fix
1. In `ndef_fill_record()`, `text/vcard`-Zweig: die Zeile
   `const char* fn = strstr((const char*)payload, "FN:");` durch eine lokale, auf `payload_len`
   begrenzte Suche ersetzen:
   ```c
   const char* src = (const char*)payload;
   const char* fn = NULL;
   for(size_t i = 0; i + 3 <= payload_len; i++) {
       if(memcmp(src + i, "FN:", 3) == 0) {
           fn = src + i;
           break;
       }
   }
   ```
2. Die nachfolgende Extraktion (`end = fn + 3; max = payload_len - (fn - src) - 3; ...`) bleibt
   unverändert — durch die Begrenzung in Schritt 1 kann `fn` jetzt nur noch innerhalb von
   `[payload, payload + payload_len - 3]` liegen, wodurch `max` nicht mehr unterlaufen kann.
3. `(const char*)payload` in der Vergleichsbasis auf die bereits vorhandene lokale `src`-Variable
   umstellen (kein neuer Cast nötig).
4. Kein Fix an `ndef_decode_vcard()` nötig — die Funktion begrenzt ihre Feldsuche bereits korrekt
   auf `len` und dient hier als Vorbild für den obigen Fix.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-vcard-strstr-oob-read/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an der echten App: einen eigenen beschreibbaren NTAG-/Ultralight-
Tag mit "NFC Tools" → Write → Custom Data auf TNF `0x02` und Typ `text/vcard` beschreiben, als
Payload einen Text **ohne** `"FN:"`-Feld eintragen (z. B. nur `"ORG:Test\r\n"`), danach denselben
Tag mit "NFC Tools" → Read einlesen und die Detailansicht öffnen — vor dem Fix ist das Verhalten
je nach Heap-Zustand ein Absturz oder eine sichtbar unsinnige/aus fremdem Speicher stammende
Namenszeile, nach dem Fix bleibt das vCard-Namensfeld leer, der Rest der Zusammenfassung
(TNF/Typ) unverändert korrekt.

## Zusätzlich geprüft, kein Fund
- `ndef_parse_message()`/`ndef_parse_message_structured()`/`nfc_tools_ndef_parse_type2_tag()`/
  `_structured()`: alle TLV-/Record-Header-Felder (Flags/TNF, Type-Length, Short-Record- vs.
  4-Byte-Payload-Length, ID-Length) werden vor jedem Zugriff gegen `msg_len`/`data_len` geprüft
  (`if(pos + N > len) break;`), kein Off-by-one bei den Grenzprüfungen selbst gefunden — die
  Payload-Länge kann zwar (im `!sr`-Zweig) als beliebiger `uint32_t`-Wert aus dem Tag stammen,
  wird aber vor dem Weiterrücken von `pos` stets gegen die tatsächliche Restlänge geprüft, ein
  Integer-Overflow von `pos + payload_len` auf 32-Bit-`size_t`-Zielen wäre theoretisch bei
  Payload-Längen nahe `SIZE_MAX` möglich, praktisch aber durch `msg_len`/`data_len` (max. wenige
  KB Tag-Speicher) nicht erreichbar.
- `ndef_decode_uri()`/`ndef_build_uri_value()`, `ndef_decode_text()`/`ndef_build_text_value()`:
  beide Varianten prüfen `len < 1` bzw. `(1 + lang_len) >= len` vor dem Zugriff, die
  `_build_*`-Varianten zusätzlich hart gegen `out_sz` mit korrektem `truncated`-Flag — kein Fund.
  Auffällig sauber gegenüber dem vCard-Zweig, der als einziger Record-Typ auf `strstr()`
  zurückgreift.
- `wifi_find_attr()` (rekursive WPS-TLV-Suche für den WLAN-Zweig): jede Iteration prüft
  `pos + 4 <= len` vor dem Header-Zugriff und `pos + alen > len` vor dem rekursiven Abstieg bzw.
  Fortschreiten — kein OOB-Zugriff, auch nicht bei absichtlich verschachtelten
  `0x100E`-Credential-Containern.
- `nfc_tools_ndef_build()` (Encoder-Seite, Zeile 560ff.): alle `ndef_buf*`-Felder werden über
  `strlen()` gemessen und mit exakt berechneter `payload_len` alloziert; der WLAN-Zweig klammert
  SSID/PSK zusätzlich explizit auf die Protokollgrenzen (32/64 Byte) — verhindert, dass die
  Ein-Byte-WSC-Längenfelder umlaufen können, selbst wenn die Eingabepuffer größer sind.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout, Stand dieser
  Sitzung): `applications/external/nfctools/`, `fap_author` "wakdev", Original-Quelle
  `github.com/wakdev/nfctools-fz`, `fap_version` 1.3.
- API-87-Einordnung: der Fund selbst betrifft ausschließlich Standard-C-String-/Pufferlogik
  (`strstr`/`memcmp`) ohne Firmware-API-Bezug — kein Kompatibilitätsrisiko unabhängig von der
  Firmware-Version. `application.fam` (`targets=["f7"]`, ausschließlich `NfcDevice`-/Poller-
  Standard-APIs für Type-2-Tags) bestätigt dasselbe unauffällige Muster wie bei Mifare
  Fuzzer/NFC Fuzzer/NFC Comparator.
- Eigener Fund: `core/nfc_tools_ndef.c`, `ndef_fill_record()`, mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-nfctools-vcard-strstr-oob-read/repro.c`).
- Restlicher Umfang von `nfctools` (`core/nfc_tools_mfc.c`/`_desfire.c`/`_icode.c`/`_ntag.c`,
  ~35 Schreib-Szenen, Sektor-Analyse, Passwort-Entfernung, Tag-Formatierung/-Sperre — zusammen
  ~10 100 der ~11 200 Zeilen) für eine künftige Sitzung offen; damit ist der Rückstellpunkt
  "restlicher NFC-Werkzeug-Cluster" nicht vollständig abgeschlossen, sondern nur begonnen.
