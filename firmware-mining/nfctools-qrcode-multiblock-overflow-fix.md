# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: QR-Code-Hilfsmodul erzeugt ab Version 6 einen nicht scannbaren QR-Code

## Was

Ich habe im NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`, Quelle
`github.com/wakdev/nfctools-fz`, fap_version 1.3, aktueller Stand über
`RogueMaster/flipperzero-firmware-wPlugins` Branch `420`, Commit `e06fcb96`) den laut
`ideas/pentest-app-katalog.md` zuletzt offenen Rest des Clusters durchgesehen: die fünf
Lese-Szenen (`scenes/read/*.c`), Haupt-/Sonstiges-Menü, die Tastatur-View sowie die
MD5-/QR-Code-Hilfsmodule. Haupt-/Sonstiges-Menü (`nfc_tools_scene_main_menu.c`,
`nfc_tools_scene_other_menu.c`, `nfc_tools_scene_about.c`) sind reines Boilerplate ohne
Fund. Die Tastatur-View (`views/keyboard/nfc_tools_keyboard.c`, 589 Zeilen) und das
MD5-Modul (`helpers/md5/nfc_tools_md5.c`, Standard-RFC-1321-Implementierung) habe ich
vollständig gelesen und gegen ihre jeweiligen Aufrufer geprüft — beide korrekt (Puffergrenzen,
Rundung, Padding-Berechnung, Bit-Länge in Little-Endian 64-Bit — alles stimmt, kein Fund).
Die fünf Lese-Szenen sind einfache Anzeige-/Navigations-Wrapper um bereits vorhandene
Felder (`app->info_str`, `app->ndef_records[]`) — ebenfalls kein Fund.

Ein Fund im QR-Code-Hilfsmodul (`helpers/qrcode/nfc_tools_qrcodegen.c`, 471 Zeilen, ein
eigenständiger, minimaler QR-Encoder für Byte-Modus/ECC-Level-L): die Versionsauswahl in
`nfc_tools_qr_encode()` behandelt **jede** Version 1 bis 7 als einen einzigen
Reed-Solomon-Block (`qr_data_cw[]`/`qr_ecc_cw[]`/`qr_total_cw[]`, je ein Eintrag pro
Version, eine `rs_remainder()`-Berechnung über den gesamten Codewort-Bereich). Das ist für
Version 1–5 (ECC-Level L) korrekt — der echte QR-Standard verwendet dort tatsächlich nur
einen Block. Ab Version 6 teilt der Standard die Daten jedoch in **zwei verschachtelte**
Reed-Solomon-Blöcke auf: Version 6 = 2×(86 Gesamt-, 68 Daten-Codewörter), Version 7 =
2×(98 Gesamt-, 78 Daten-Codewörter) — unabhängig gegen die Referenzbibliothek `segno`
(`segno.consts.ECC[6][1]`/`ECC[7][1]`, Python, separat installiert) gegengeprüft, nicht nur
gegen eigene Berechnung aus dem ISO/IEC-18004:2015-Standardtext. Für Version 6/7 berechnet
die bestehende Funktion also sowohl die ECC-Codewörter falsch (ein einzelner RS-Block über
die volle Kapazität statt zwei unabhängiger, miteinander verschachtelter Blöcke) als auch
die Codewort-Reihenfolge beim Platzieren in die Matrix (`place_data()` bekommt den
unveränderten, nicht verschachtelten Strom) — das Ergebnis ist eine augenscheinlich
vollständige QR-Grafik (`nfc_tools_qr_encode()` gibt `true` zurück, `has_qr`/`valid` werden
gesetzt, die Grafik wird angezeigt), die aber **strukturell ungültig** ist und von keinem
echten Scanner gelesen werden kann.

Empirisch nachgewiesen (nicht nur theoretisch aus dem Standardtext hergeleitet): Ich habe
die unveränderte Datei in ein eigenständiges Host-Programm kompiliert, das für jede
Textlänge 1–174 Zeichen eine QR-Grafik erzeugt, als PGM-Bild rendert und mit `zbarimg`
(ZBar, ein unabhängiger, weit verbreiteter Open-Source-Barcode-Scanner — nicht mein eigener
Decoder) zurückliest:

```
len=100 version=5 size=37 modules -> OK   (zbarimg liest den Originaltext korrekt zurück)
len=107 version=6 size=41 modules -> FAIL (zbarimg: "scanned 0 barcode symbols")
len=174 version=7 size=45 modules -> FAIL (zbarimg: "scanned 0 barcode symbols")
```

Jede getestete Länge ab 107 Zeichen (= die kleinste Länge, die Version 6 statt 5 erzwingt)
produziert eine Grafik, die zbarimg nicht als QR-Code lesen kann; jede Länge bis 106 Zeichen
(Version 1–5) wird korrekt zurückgelesen. Der Datenfeldwert `value[128]` einer NDEF-Record
(die einzige Quelle für den QR-Inhalt, siehe `nfc_tools_scene_ndef_qr_code.c`) kann bis zu
127 Zeichen lang sein — ein alltäglicher Fall (z. B. eine URL mit Tracking-Parametern oder
ein längerer Freitext) überschreitet die 106-Zeichen-Grenze regelmäßig und löst den Fund aus.
Dies ist kein Speicherfehler auf dem Flipper selbst (alle Pufferzugriffe bleiben innerhalb
ihrer statisch allozierten Größen, auch für Version 6/7 — der Fehler ist rein in der
RS-Codewort-Berechnung/-Anordnung), sondern ein Korrektheitsfehler: Die App zeigt dem Nutzer
eine augenscheinlich funktionierende QR-Grafik, die beim eigentlichen Verwendungszweck (Scan
durch ein drittes Gerät/einen Passanten) kommentarlos fehlschlägt.

## Nutzen

- Verhindert, dass "QR anzeigen" bei längeren NDEF-Werten (≥107 Zeichen) eine augenscheinlich
  gültige, aber unscannbare Grafik zeigt — ein Nutzer, der einem Dritten einen per Flipper
  erzeugten QR-Code zum Scannen vorhält, bekommt stattdessen ehrlich "QR unavailable"
  angezeigt und weiß, dass er den Wert stattdessen manuell übertragen muss, statt auf einen
  stillen Fehlschlag am Zielgerät zu stoßen.
- Minimal-invasiv: eine Versionsschleifengrenze (`<= 7` → `<= 5`) plus Kommentar-Korrektur,
  keine neue Logik, kein Eingriff in die (für Version 1–5 bereits korrekte) Bitstream-/
  Matrix-/Masken-Berechnung. Eine vollständige Mehrblock-RS-Implementierung mit
  Codewort-Verschachtelung (der eigentlich "richtige" Fix, um Version 6/7 wieder nutzbar zu
  machen) ist ein deutlich größerer, eigenständiger Port-Schritt und bewusst nicht Teil
  dieses kleinen, abgeschlossenen Beitrags (siehe "Offen" unten).
- Für Version 1–5 (Länge 1–106 Zeichen) byte-identisches Verhalten zum bisherigen Code —
  siehe Host-Repro Szenario 1.

## Betroffene Dateien

- `helpers/qrcode/nfc_tools_qrcodegen.c` — einzige geänderte Datei. Versionsschleife in
  `nfc_tools_qr_encode()` von `for(int v = 1; v <= 7; v++)` auf `for(int v = 1; v <= 5; v++)`
  begrenzt; Datei-Kopfkommentar entsprechend korrigiert (dokumentiert jetzt explizit die
  Zwei-Block-Struktur ab Version 6 als Grund für die Grenze).

Keine Änderung an `nfc_tools_qrcodegen.h` (der Puffer `QRCODEGEN_BUF_MAX` bleibt unverändert
groß genug, auch wenn er jetzt nie mehr für Version 6/7 befüllt wird — keine
Puffergrößenänderung nötig, kein Risiko durch Überdimensionierung), an
`nfc_tools_scene_ndef_qr_code.c` (zeigt bei `valid == false` bereits unverändert
"QR unavailable" an — dieser Pfad existierte bereits und wird jetzt für längere Werte
korrekt statt gar nicht erreicht) oder an den bereits dokumentierten Funden in
`core/nfc_tools_ndef.c` (unabhängig, unberührt).

## Risiko

- Sehr gering. Die Änderung entfernt ausschließlich zwei Werte (6, 7) aus der bereits
  vorhandenen Versions-Suchschleife — für alle Eingaben, die vorher Version 1–5 ergaben,
  ist das Verhalten exakt unverändert (siehe Host-Repro Szenario 1: byte-identische
  Matrizen für Länge 1–106). Für Eingaben, die vorher Version 6/7 ergeben hätten, ändert
  sich das Verhalten von "zeigt eine kaputte Grafik" zu "zeigt ehrlich `QR unavailable`" —
  der aufrufende Code (`nfc_tools_scene_ndef_qr_code.c`) behandelt `valid == false` bereits
  korrekt (siehe `nfc_tools_qr_draw_cb()`, Zweig `if(!m->valid)`), dieser Pfad ist also kein
  Neuland, sondern bereits vorhandener, genutzter Code.
- Kein Pufferrisiko: `QRCODEGEN_BUF_MAX` (408 Byte, aus dem Header-Makro für Version 10
  berechnet) bleibt groß genug für Version 5 (benötigt nur 255 Byte) und wird durch die
  engere Versionsgrenze nicht kleiner — es wird lediglich nie mehr bis Version 7 befüllt.
- Rückt keine funktionale Erwartung zurecht, die vorher *korrekt* erfüllt war: Version 6/7
  waren vor diesem Fix bereits faktisch nutzlos (jede erzeugte Grafik war unscannbar) — der
  Fix entfernt keine funktionierende Fähigkeit, er benennt nur ehrlich, dass sie nie
  funktioniert hat.

## Testhinweis (ohne Hardware)

Reine Codewort-/Bitstream-Logik, vollständig ohne Flipper/NFC-Feld nachstellbar — und
diesmal zusätzlich mit einem echten, unabhängigen Scanner statt nur eigener Logik
gegengeprüft: `repro-nfctools-qrcode-multiblock-overflow/repro.c` bindet
`nfc_tools_qrcodegen_alt.c` (unverändert aus der geprüften Quelle) und
`nfc_tools_qrcodegen_new.c` (identisch zum angewendeten Patch) als zwei **unveränderte,
vollständige Kopien** der echten Datei ein (keine Transkription) — über
Kommandozeilen-Makros (`-Dnfc_tools_qr_encode=nfc_tools_qr_encode_alt` usw.) umbenannt, damit
beide in einem Prozess linken. Für den eigentlichen Scan-Nachweis rendert das Repro jede
erzeugte QR-Matrix als PGM-Bild und ruft darauf per `popen()` das Kommandozeilenwerkzeug
`zbarimg` (ZBar-Projekt, unabhängiger Barcode-Scanner, auf dieser Maschine separat
installiert) auf — fehlt `zbarimg`, laufen die rein strukturellen Teile trotzdem
(`ok_new == false` für Version 6/7 lässt sich auch ohne Scanner nachweisen).

Vier Szenarien:
1. **Regression (Version 1–5, Länge 1–106)**: ALT und NEU liefern für jede Länge
   byte-identische QR-Matrizen (`memcmp` über den vollen `QRCODEGEN_BUF_MAX`-Puffer).
2. **Fund (Version 6/7, Länge 107/110/130/150/174)**: ALT meldet weiterhin `encode=true` —
   die gerenderte Grafik liefert bei `zbarimg` aber in allen fünf Stichproben **keinen**
   Treffer zurück (`scanned 0 barcode symbols`). NEU lehnt dieselben Längen mit `false` ab.
3. **Grenzfall**: Länge 106 bleibt bei NEU Version 5, `encode=true`, von `zbarimg` korrekt
   zurückgelesen; Länge 107 fällt bei NEU auf `encode=false`.
4. **Randfall leerer Text**: ALT und NEU lehnen beide unverändert ab.

```
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined \
    -Dnfc_tools_qr_encode=nfc_tools_qr_encode_alt -Dnfc_tools_qr_size=nfc_tools_qr_size_alt \
    -Dnfc_tools_qr_module=nfc_tools_qr_module_alt -c nfc_tools_qrcodegen_alt.c -o qrcodegen_alt.o
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined \
    -Dnfc_tools_qr_encode=nfc_tools_qr_encode_new -Dnfc_tools_qr_size=nfc_tools_qr_size_new \
    -Dnfc_tools_qr_module=nfc_tools_qr_module_new -c nfc_tools_qrcodegen_new.c -o qrcodegen_new.o
$ gcc -std=c11 -O0 -Wall -Wextra -fsanitize=address,undefined \
    repro.c qrcodegen_alt.o qrcodegen_new.o -o repro -lm
$ ./repro
OK:   Regression: ALT und NEU liefern fuer Laenge 1..106 (Version 1-5) byte-identische QR-Matrizen.
  Laenge 107: ALT meldet encode=true, Version=6, zbarimg liest zurueck=nein (Fund bestaetigt)
  Laenge 110: ALT meldet encode=true, Version=6, zbarimg liest zurueck=nein (Fund bestaetigt)
  Laenge 130: ALT meldet encode=true, Version=6, zbarimg liest zurueck=nein (Fund bestaetigt)
  Laenge 150: ALT meldet encode=true, Version=6, zbarimg liest zurueck=nein (Fund bestaetigt)
  Laenge 174: ALT meldet encode=true, Version=7, zbarimg liest zurueck=nein (Fund bestaetigt)
OK:   NEU lehnt alle Laengen 107/110/130/150/174 (Version 6/7) mit false ab ("QR unavailable") statt eine Grafik zu zeigen.
OK:   Fund reproduziert: mindestens eine von ALT als "true" gemeldete Version-6/7-QR-Grafik liest bei einem echten, unabhaengigen Scanner (zbarimg) NICHT auf den Originaltext zurueck.
OK:   Grenzfall: Laenge 106 bleibt bei NEU Version 5, encode=true.
OK:   Grenzfall: Laenge 106 (NEU) ist bei zbarimg scannbar.
OK:   Grenzfall: Laenge 107 faellt bei NEU korrekt auf encode=false.
OK:   Randfall leerer Text: ALT und NEU lehnen beide ab (unveraendert).

7/7 Szenarien bestanden.
```
Identisch bei `-O1` (zweiter Durchlauf, dieselben sieben "OK"-Zeilen). Keine ASan-/
UBSan-Meldungen, 7/7 Szenarien bestanden, keine Compiler-Warnungen bei `-Wall -Wextra`.

Funktionaler Test danach lokal auf dem Flipper: einen NDEF-Datensatz mit einem Wert von
mindestens 107 Zeichen lesen (oder einen "Write"-Vorgang mit entsprechend langem Text, dann
zurücklesen) und im Record-Detail "QR"-Button drücken. Vorher: eine augenscheinlich fertige,
aber mit dem Handy nicht scannbare QR-Grafik. Nachher: "QR unavailable" statt der kaputten
Grafik. Für Werte bis 106 Zeichen bleibt der bisherige, funktionierende QR-Code unverändert.

## Port-Skizze / Patch

Patch (`git apply --check` vorab gegen eine frische, unabhängige Kopie von
`RogueMaster/flipperzero-firmware-wPlugins` Branch `420` verifiziert, real angewendet und
byte-für-byte mit der Arbeitskopie abgeglichen) in
`firmware-mining/nfctools-qrcode-multiblock-overflow-fix/nfc_tools_qrcodegen_c.patch`.

Kernstück (Versionsschleife in `nfc_tools_qr_encode()`):
```c
    int version = 0;
-   for(int v = 1; v <= 7; v++) {
+   for(int v = 1; v <= 5; v++) {
        if((size_t)qr_data_cw[v] >= textLen + 2) {
            version = v;
            break;
        }
    }
```

## Offen

- Eine echte Mehrblock-Reed-Solomon-Implementierung mit Codewort-Verschachtelung würde
  Version 6/7 wieder nutzbar machen (Werte bis ca. 166 statt 106 Zeichen) — das ist ein
  eigenständiger, deutlich größerer Port-Schritt (zwei RS-Blöcke pro Version getrennt
  berechnen, deren Codewörter byteweise verschachtelt statt sequenziell in die Matrix
  schreiben, plus Versions-Informationsfeld ab Version 7 — ein eigenes 2×18-Bit-BCH-Feld
  nahe den Eckmustern, das diese Datei für Version 7 bisher ebenfalls nicht zeichnet, aber
  wegen dieses Fixes jetzt nie erreicht wird) und bewusst nicht Teil dieses kleinen Beitrags.
- Bei dieser Gelegenheit zusätzlich festgestellt (nicht Teil des Fixes, da ohne
  Funktionsauswirkung): Der Header-Kommentar zu `QRCODEGEN_BUF_MAX` behauptet
  "196 bytes (version 10 worst case)", das tatsächliche Makro `QRCODEGEN_BUF_LEN(10)`
  berechnet aber 408 Byte (`(57*57+7)/8+1`). Der Puffer ist dadurch nicht zu klein (er wird
  korrekt über das Makro berechnet, nicht über die falsche Kommentarzahl) — reine
  Dokumentationsungenauigkeit, nicht behoben, um diesen Patch auf die eine
  sicherheits-/korrektheitsrelevante Zeile zu beschränken.

## Quelle

- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Commit `e06fcb96`, dieselbe
  Quelle wie alle bisherigen NFC-Tools-Funde dieser Sitzungsreihe):
  `applications/external/nfctools/`, `fap_author` "wakdev", Original-Quelle
  `github.com/wakdev/nfctools-fz`, `fap_version` 1.3.
- ISO/IEC 18004:2015 (QR Code Spezifikation), Tabelle 9 (Error-Correction-Characteristics für
  Byte-Modus) und Annex E (Alignment-Pattern-Positionen).
- Referenzwerte für die Zwei-Block-Struktur ab Version 6 (ECC-Level L) unabhängig gegen die
  Python-Bibliothek `segno` gegengeprüft (`segno.consts.ECC[6][1]` → `num_blocks=2,
  num_total=86, num_data=68`; `ECC[7][1]` → `num_blocks=2, num_total=98, num_data=78`).
- Scan-Nachweis über `zbarimg` (ZBar-Projekt), ein unabhängiges, von mir nicht selbst
  geschriebenes Scanner-Werkzeug — nicht meine eigene Decoder-Logik.
- API-87-Einordnung: Der Fix ändert ausschließlich eine Schleifengrenze und einen Kommentar
  in reiner, firmwareunabhängiger Host-Logik (kein Flipper-API-Aufruf in dieser Datei
  überhaupt) — keine neue API-Fläche, kein firmwareversionsabhängiges
  Kompatibilitätsrisiko gegenüber meinem `mntm-012`-Stand.
- Eigener Fund: `helpers/qrcode/nfc_tools_qrcodegen.c`, `nfc_tools_qr_encode()` —
  identifiziert beim systematischen Durchgehen des zuletzt als offen vermerkten
  Cluster-Rests (`ideas/pentest-app-katalog.md`: "fünf Lese-Szenen, Haupt-/Sonstiges-Menü,
  Tastatur-View, MD5-/QR-Code-Hilfsmodule"). Damit ist der NFC-Tools-Cluster (`wakdev`,
  RogueMaster-Bündel) jetzt vollständig quelltextlich durchgesehen.
