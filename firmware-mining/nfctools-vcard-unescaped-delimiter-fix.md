# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: vCard-Schreibpfad escaped Trennzeichen nicht (RFC 6350)

## Was

Ich habe im NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`, Quelle
`github.com/wakdev/nfctools-fz`, fap_version 1.3, aktueller Stand über
`RogueMaster/flipperzero-firmware-wPlugins` Branch `420`, Commit `f0cd3f0d`) den bislang noch
nicht einzeln geprüften Rest der "Write"-Szenen durchgesehen — laut `ideas/pentest-app-katalog.md`
der letzte offene Teil dieses Clusters ("Restlicher Umfang (~35 Schreib-Szenen ...) offen").
Alle ~29 verbleibenden einfachen Texteingabe-Szenen (`scenes/write/nfc_tools_scene_write_*.c`:
Bitcoin-Adresse/-Betrag/-Nachricht, Bluetooth, Contact-Felder, Custom-URI/-MIME/-Data, Facetime,
Location, Mail-Felder, Phone, Search, SMS-Felder, Social, Text, Unit-Link, Url, WiFi-SSID/-Pass)
sowie der komplette gemeinsame Schreibpfad `nfc_tools_ndef_build()` (`core/nfc_tools_ndef.c`,
alle 17 `NdefType`-Fälle) wurden gelesen. Die Szenen selbst sind durchgängig dasselbe
30–40-Zeilen-Muster (Keyboard-Eingabe in einen `app->ndef_bufN`-Puffer mit
`sizeof(...)`-Begrenzung, teils zusätzlich per `keyboard_set_max_length()` enger begrenzt — SSID
32, Passwort 64 — passend zu den in `nfc_tools_ndef_build()` bereits vorhandenen WSC-Klammerungen)
und bieten keine weitere Angriffsfläche; `nfc_tools_ndef_build()` selbst dimensioniert jede
`payload`-Allokation exakt auf die vorher gemessene Länge (kein Puffer-/Längenfeld-Fund über die
bereits dokumentierten `url_encode()`-Stellen hinaus).

Ein Fund blieb: `case NdefTypeContact:` baut die geschriebene vCard-3.0-Datei mit rohem `%s` aus
sechs frei eingebbaren Textfeldern (Name, Firma, Adresse, Telefon, E-Mail, URL) zusammen, ohne die
nach RFC 6350 §3.4 für vCard-TEXT-Werte vorgeschriebene Escape-Behandlung von `\`, `;`, `,` und
eingebetteten Zeilenumbrüchen:

```c
case NdefTypeContact: {
    tnf = 0x02;
    type = "text/vcard";

    FuriString* vc = furi_string_alloc();
    furi_string_cat_str(vc, "BEGIN:VCARD\r\nVERSION:3.0\r\n");
    furi_string_cat_printf(vc, "FN:%s\r\n", app->ndef_buf1);
    furi_string_cat_printf(vc, "N:;%s;;;\r\n", app->ndef_buf1);
    if(app->ndef_buf2[0]) furi_string_cat_printf(vc, "ORG:%s\r\n", app->ndef_buf2);
    if(app->ndef_buf3[0]) furi_string_cat_printf(vc, "ADR:;;%s;;;;\r\n", app->ndef_buf3);
    ...
```

`N` und `ADR` sind selbst durch Semikolon getrennte Teilfelder (`N:` = `family;given;additional;
prefix;suffix`, fünf Teile; `ADR:` = `POBox;ExtAddr;Street;Locality;Region;PostalCode;Country`,
sieben Teile). Ein eingegebener Name wie `Doe;Jane` oder eine Firma wie `Acme; Inc.` enthält genau
das Zeichen, das diese Teilfelder trennt — das `;` landet nicht als Textinhalt, sondern als
*echtes* Trennzeichen in der geschriebenen Datei. Jedes vCard-lesende Gerät (das eigentliche Ziel
einer per Flipper geschriebenen Visitenkarten-NFC-Tag) zerlegt die Zeile dann falsch: aus
`N:;Doe;Jane;;;` liest ein Standard-Parser `family=""`, `given="Doe"`, `additional="Jane"` statt
des gewollten einzelnen Namens — der Name wird auf dem lesenden Gerät sichtbar verstümmelt
angezeigt (z. B. als getrenntes Vor-/Nachname-Paar statt der eingegebenen Zeichenfolge). Bei
`ADR:` verschiebt ein `;` in der Adresse (z. B. "123 Main St; Suite 4") analog die nachfolgenden
Orts-/Regions-/Postleitzahl-/Land-Teilfelder um eine Position. Komma und Backslash sind nach
RFC 6350 ebenso Escape-pflichtig (Komma trennt Listwerte, Backslash leitet die Escape-Sequenz
selbst ein); ein unescaptes `\` vor einem Zeichen, das der Parser als Escape-Start missdeutet,
kann je nach Parser-Implementierung ebenfalls zu Fehlinterpretation führen.

Dies ist kein Speicherfehler auf dem Flipper selbst (die `payload_len`-Berechnung misst die fertig
zusammengesetzte `FuriString` erst **nach** dem Zusammenbau — bei unverändertem Puffer-/
Allokationscode bleibt das korrekt, unabhängig vom Inhalt), sondern ein Korrektheitsfehler: die
erzeugte Datei ist kein normgerechtes vCard, und die Verstümmelung tritt beim **Lesen durch ein
drittes Gerät** auf — also genau im eigentlichen Verwendungszweck dieser Schreibfunktion.

## Nutzen

- Macht "Write Contact" für Namen/Firmen/Adressen, die eines der vier RFC-6350-Sonderzeichen
  (`;`, `,`, `\`, eingebetteter Zeilenumbruch) enthalten, erstmals normgerecht — bisher wurde die
  geschriebene Visitenkarte auf jedem lesenden Smartphone/Kontakt-App sichtbar falsch zerlegt
  angezeigt, ohne dass der Flipper selbst das erkennen oder anzeigen konnte (der Fehler zeigt sich
  ausschließlich auf dem lesenden Gerät).
- Rein additiv: eine neue statische Hilfsfunktion `vcard_escape()` (nach demselben Baustein-Muster
  wie das bereits vorhandene `url_encode()` direkt darüber in derselben Datei) plus deren Aufruf
  an sechs bestehenden Stellen innerhalb des einen `case NdefTypeContact:`-Blocks. Keine andere
  `NdefType`-Verzweigung, keine Szenen-Datei, keine Puffergröße (`NFC_TOOLS_NDEF_BUF*_SIZE`) ändert
  sich.
- Ohne Sonderzeichen (der bisher einzige in den Reports dieser Sitzungsreihe getestete Fall)
  bleibt die erzeugte Datei byte-identisch zum bisherigen Verhalten — siehe Host-Repro-Szenario 1.

## Betroffene Dateien

- `core/nfc_tools_ndef.c` — einzige geänderte Datei. Neue Funktion `vcard_escape()` direkt nach
  `url_encode()` (Zeile 557 im geprüften Stand); `case NdefTypeContact:` in
  `nfc_tools_ndef_build()` escaped jetzt alle sechs Felder (`ndef_buf1`…`ndef_buf6`) vor dem
  Einbetten in die `FuriString`.

Keine Änderung an `scenes/write/nfc_tools_scene_write_contact_*.c` (sechs Dateien, reine
Keyboard-Eingabe-Wrapper, unverändert korrekt), an den Puffergrößen-Makros, an den übrigen 16
`NdefType`-Fällen, oder am bereits separat dokumentierten `url_encode()`-Fund
(`firmware-mining/nfctools-url-encode-off-by-one-truncation-fix.md`, unabhängige Funktion,
unberührt).

## Risiko

- Sehr gering. `vcard_escape()` ist bei Eingaben ohne die vier Sonderzeichen ein reiner
  Durchreich-Pfad (jedes Zeichen landet unverändert im Ziel) — identisches Verhalten zu vorher.
  Die Pufferdimensionierung (`strlen(src) * 2 + 1`, worst case: jedes Zeichen wird escaped, zwei
  Ausgabebytes je Eingabebyte, plus Nullterminator) ist exakt für den ungünstigsten Fall bemessen,
  dieselbe Herangehensweise wie bei den bereits vorhandenen `malloc(kw_raw_len * 3 + 1)`-Stellen
  im Bitcoin-/Such-URL-Encoder derselben Datei.
- Jede der sechs neuen `malloc()`-Allokationen wird im selben Codepfad vor dem nächsten
  `furi_string_cat_printf()`-Aufruf wieder `free()`t — kein Leck, auch nicht bei den optionalen,
  nur bedingt durchlaufenen Feldern (Org/Adresse/Telefon/E-Mail/URL), da `malloc()`/`free()` jeweils
  innerhalb desselben `if(...)`-Zweigs stehen.
- `payload_len = furi_string_size(vc)` (unverändert) misst weiterhin die tatsächliche, fertig
  zusammengebaute Zeichenkette — unabhängig vom escapten Inhalt korrekt, keine Diskrepanz zwischen
  gemeldeter und tatsächlicher Länge.

## Testhinweis (ohne Hardware)

Reine Zeichenketten-Logik, vollständig ohne Flipper/NFC-Feld nachstellbar —
`repro-nfctools-vcard-unescaped-delimiter/repro.c` transkribiert `vcard_escape()` 1:1 aus dem
angewendeten Patch sowie den `case NdefTypeContact:`-Block in ALT- (unescaped) und NEU-Fassung
(mit `vcard_escape()`), jeweils mit einem simplen wachsenden Host-Puffer statt `FuriString`
(gleiches Ergebnis, kein Flipper-Header nötig). Ein eigener RFC-6350-konformer Teilfeld-Splitter
(`vcard_split_prop()`, zeilenanker-sicher gegen die `FN:`/`N:`-Präfix-Kollision) zerlegt `N:`/`ADR:`
genauso, wie es ein echter vCard-Parser täte, und macht die Verstümmelung sichtbar.

Neun Szenarien:
1. **Regression ohne Sonderzeichen**: ALT und NEU erzeugen byte-identische vCard-Ausgabe; `N:`
   zerlegt in die standardmäßigen 5 Teilfelder, `given="John Doe"` unverändert.
2. **Name mit `;`** ("Doe;Jane"): ALT zerlegt `N:` in 6 statt 5 Teilfelder (`given="Doe"`,
   `additional="Jane"`, Name verstümmelt) — NEU erhält ihn als einziges `given`-Feld, 5 Teilfelder
   wie vom Standard vorgesehen.
3. **Name mit `,` und `\`** ("O'Brien, Patrick \\Pat\\"): NEU liefert ihn nach dem Rundlauf
   (Escape → Parse → Unescape) unverändert zurück.
4. **Adresse mit `;`** ("123 Main St; Suite 4"): ALT erzeugt 8 statt 7 `ADR:`-Teilfelder (das
   nachfolgende Orts-Feld wird mit " Suite 4" überschrieben) — NEU hält exakt 7 Teilfelder, die
   Straße bleibt unversehrt in ihrem eigenen Slot.
5. **Firma mit `;`** ("Acme; Inc."): ALT schreibt sie unescaped (`ORG:Acme; Inc.`), NEU escaped
   (`ORG:Acme\; Inc.`).
6. **Leere optionale Felder** (Org/Adresse/Telefon/E-Mail/URL) bleiben unverändert ausgelassen.
7. **Unveränderte Sentinel-Werte** ("+" bei Telefon, "https://" bei URL, von den jeweiligen
   Eingabe-Szenen als Vorbelegung gesetzt) werden weiterhin unterdrückt statt als Feld geschrieben.
8. **Telefon/E-Mail/URL mit `;`**: ebenfalls escaped (Symmetrie über alle sechs Felder, nicht nur
   Name/Firma/Adresse).
9. **Randfall leerer Name** (in der echten App durch `keyboard_set_minimum_length(1)` in
   `nfc_tools_scene_write_contact_name.c` ausgeschlossen, hier defensiv mitgeprüft): ALT und NEU
   bleiben byte-identisch.

```
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
$ ./repro
OK:   Regression ohne Sonderzeichen: ALT und NEU erzeugen byte-identisches vCard, N: parst zu 5 Feldern family="" given="John Doe" additional/prefix/suffix="".
OK:   Name mit ';' ("Doe;Jane"): ALT zerlegt N: in 6 statt 5 Feldern (given="Doe" additional="Jane", Name verstuemmelt) -- NEU erhaelt den Namen unversehrt als einziges given-Feld, 5 Felder wie vom Standard vorgesehen.
OK:   Name mit ',' und '\' ("O'Brien, Patrick \Pat\"): NEU liefert ihn nach dem Rundlauf unveraendert zurueck, weiterhin 5 N:-Felder.
OK:   Adresse mit ';' ("123 Main St; Suite 4"): ALT erzeugt 8 statt 7 ADR-Teilfelder (Ort-Feld wird mit " Suite 4" ueberschrieben) -- NEU haelt exakt 7 Teilfelder, Strasse unversehrt in ihrem eigenen Slot.
OK:   Firma mit ';' ("Acme; Inc."): ALT schreibt sie unescaped, NEU escaped sie als "Acme\; Inc.".
OK:   Leere optionale Felder (Org/Adresse/Telefon/E-Mail/URL) bleiben unveraendert ausgelassen.
OK:   Unveraenderte Sentinel-Werte ("+" bei Telefon, "https://" bei URL) werden weiterhin unterdrueckt.
OK:   Telefon/E-Mail/URL mit ';' werden ebenfalls escaped (Symmetrie, RFC 6350).
OK:   Randfall leerer Name (in der echten App durch minimum_length=1 ausgeschlossen): ALT und NEU bleiben byte-identisch.
Alle Szenarien bestaetigt (exit code 0)
```
Identisch bei `-O1` (zweiter Lauf, dieselben neun "OK"-Zeilen). Keine ASan-/UBSan-Meldungen,
9/9 Szenarien bestanden, keine Compiler-Warnungen bei `-Wall -Wextra`.

Funktionaler Test danach lokal auf dem Flipper: "Write Contact" mit einem Namen oder einer Firma
wählen, die ein `;` oder `,` enthält (z. B. Name "Doe, Jane" oder Firma "Acme; Inc."), auf einen
eigenen beschreibbaren NFC-Tag schreiben, anschließend mit einem echten Smartphone antippen und
prüfen, dass Name/Firma in der Kontakt-Vorschau unverstümmelt als ein zusammenhängendes Feld
erscheinen (vorher: Name/Firma sichtbar an der `;`/`,`-Stelle aufgeteilt bzw. in ein falsches
vCard-Feld verschoben).

## Port-Skizze / Patch

Patch (`git apply --check` vorab gegen eine frische, unabhängige Kopie von
`RogueMaster/flipperzero-firmware-wPlugins` Branch `420` verifiziert, real angewendet und
byte-für-byte mit der Arbeitskopie abgeglichen) in
`firmware-mining/nfctools-vcard-unescaped-delimiter-fix/nfc_tools_ndef_c.patch`.

Kernstück, neue Hilfsfunktion (direkt nach `url_encode()`):
```c
static void vcard_escape(const char* src, char* dest, size_t dest_size) {
    size_t j = 0;
    for(size_t i = 0; src[i] != '\0' && j + 3 < dest_size; i++) {
        char c = src[i];
        if(c == '\\' || c == ';' || c == ',') {
            dest[j++] = '\\';
            dest[j++] = c;
        } else if(c == '\n') {
            dest[j++] = '\\';
            dest[j++] = 'n';
        } else if(c == '\r') {
            continue;
        } else {
            dest[j++] = c;
        }
    }
    dest[j] = '\0';
}
```
Aufruf an jeder der sechs Stellen in `case NdefTypeContact:`, z. B. für den Namen:
```c
size_t name_esc_size = strlen(app->ndef_buf1) * 2 + 1;
char* name_esc = malloc(name_esc_size);
vcard_escape(app->ndef_buf1, name_esc, name_esc_size);
furi_string_cat_printf(vc, "FN:%s\r\n", name_esc);
furi_string_cat_printf(vc, "N:;%s;;;\r\n", name_esc);
free(name_esc);
```
Analog für Firma (`ORG:`), Adresse (`ADR:;;...;;;;`), Telefon (`TEL:`, innerhalb der bestehenden
"+"-Sentinel-Prüfung), E-Mail (`EMAIL:`) und URL (`URL:`, innerhalb der bestehenden
"https://"-Sentinel-Prüfung) — vollständiger Diff im Patch.

## Quelle

- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Commit `f0cd3f0d`, dieselbe Quelle
  wie alle bisherigen NFC-Tools-Funde dieser Sitzungsreihe): `applications/external/nfctools/`,
  `fap_author` "wakdev", Original-Quelle `github.com/wakdev/nfctools-fz`, `fap_version` 1.3.
- RFC 6350 (vCard Format Specification), §3.4 "Property Value Escaping".
- API-87-Einordnung: Der Fix nutzt ausschließlich bereits in dieser Datei vorhandene,
  firmwareunabhängige Bausteine (`furi_string_alloc/_cat_str/_cat_printf/_size/_get_cstr/_free`,
  `malloc`/`free`, `strlen`/`strcmp`) — keine neue API-Fläche, kein firmwareversionsabhängiges
  Kompatibilitätsrisiko gegenüber meinem `mntm-012`-Stand.
- Eigener Fund: `core/nfc_tools_ndef.c`, `nfc_tools_ndef_build()`, `case NdefTypeContact:` —
  identifiziert beim systematischen Durchgehen des zuvor als "Restlicher Umfang (~35 Schreib-
  Szenen) offen" vermerkten Teils des Clusters (`ideas/pentest-app-katalog.md`). Alle 17
  `NdefType`-Fälle sowie alle ~29 einfachen Eingabe-Szenen wurden dabei gelesen; dies ist der
  einzige über die bereits dokumentierten Funde hinausgehende Treffer — der Rest des
  Schreibpfads (Puffergrößen, Sentinel-Werte, Feld-Rücksetzung beim Typwechsel über
  `write_bufs_reset()` in `scenes/nfc_tools_scene_write_menu.c`) ist konsistent und korrekt
  dimensioniert.
