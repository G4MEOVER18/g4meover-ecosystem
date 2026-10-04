# Nice O-Code: Phase 2 des Ports — Struct/`.sub`-Erweiterung plus Decode-Anzeige

## Was

Fortsetzung von `firmware-mining/nice-o-code-subghz-port.md`: die dort verifizierten, additiven
und bis jetzt unverwendeten Hilfsfunktionen (`subghz_protocol_nice_flor_s_encrypt_ic()`/
`_decrypt_ic()`, `subghz_protocol_nice_o_mask()`, `subghz_protocol_nice_o_get_parcel()`) jetzt
tatsächlich verdrahtet — aber bewusst nur den aus der dortigen "Port-Skizze" risikoärmsten
Ausschnitt: Struct-Erweiterung, `.sub`-I/O für ein neues optionales `IC`-Feld, und die
Decoder-Anzeige (`get_string()`). **Nicht** angefasst: `feed()` (die Live-Zwei-Pakete-Heuristik,
die laut Vorsitzung den bereits produktiven Plain-FloR-S-Empfangspfad verzögern würde) und der
Encoder (`get_upload()`, TX-Erzeugung) — beide bleiben wie dort dokumentiert zurückgestellt.

Ergebnis dieser Sitzung: ein Flor-S/Nice-One-`.sub`-Datei mit einem zusätzlichen `IC`-Hex-Feld
wird jetzt korrekt als O-Code dekodiert und angezeigt (Seriennummer/Zähler/Taste/Installateurcode)
statt wie bisher — ganz ohne diese Erweiterung — mit zufälligem Inhalt als falsches Flor-S
angezeigt zu werden (derselbe Fehlklassifikations-Fund wie in der Vorsitzung, jetzt für den
Anzeigepfad behoben). Eine *live* über Funk empfangene O-Code-Übertragung wird weiterhin nicht
als solche erkannt (das bleibt `feed()`s Aufgabe) — diese Phase deckt ausschließlich das Laden
einer bereits mit bekanntem Installateurcode versehenen Datei ab.

## Nutzen

- Schließt eine konkrete Lücke der Vorsitzung: dort lagen die Krypto-Grundlagen bereits vor,
  aber noch an keiner einzigen Stelle im Baum verdrahtet — tatsächlich nutzbar war noch nichts.
  Jetzt kann ich eine Nice-O-Code-`.sub`-Datei, deren Installateurcode ich auf anderem Weg
  kenne (z. B. am Empfänger selbst abgelesen, oder künftig aus der in der Vorsitzung skizzierten
  Installateurcode-Recovery-App), korrekt öffnen und die Felder ablesen.
- Legt das Struct-/Dateiformat-Fundament, auf dem eine künftige Sitzung die verbleibenden,
  deutlich risikoreicheren Teile (Live-Erkennung in `feed()`, Encoder-Gegenstück für Senden)
  aufbauen kann, ohne diese Grundlage selbst noch einmal anzufassen.
- Vollständig rückwärtskompatibel: Jede bisher bei mir gespeicherte `.sub`-Datei (kein `IC`-Feld)
  nimmt exakt denselben, unveränderten Code-Pfad wie vor diesem Patch — siehe „Risiko" unten.

## Betroffene Dateien

- `lib/subghz/protocols/nice_flor_s.c` — additiv, nur den Decoder betreffend:
  `firmware-mining/nice-o-code-decode-display-port/nice_flor_s_c.patch`
  - **Baut auf dem bereits vorliegenden Patch der Vorsitzung auf** (`firmware-mining/
    nice-o-code-subghz-port/nice_flor_s_c.patch` — fügt die vier Hilfsfunktionen samt
    `NICE_FLOR_S_IC_PLAIN`-Makro ein). Reihenfolge beim Anwenden: zuerst diesen, dann den
    hier vorliegenden. `git apply --check` beider Patches hintereinander in genau dieser
    Reihenfolge gegen eine frische, unabhängige Kopie von `Next-Flip/Momentum-Firmware`
    (Tag `mntm-012`) verifiziert — siehe „Verifikation" unten.
  - Kein Encoder-, kein `public_api.h`-, kein Registry-Eintrag betroffen (identisch zur
    Einschätzung der Vorsitzung: Nice O-Code bleibt eine Variante des bestehenden
    `subghz_protocol_nice_flor_s`-Eintrags, kein neuer Protokolleintrag).

## Änderungen im Detail

1. **Struct-Erweiterung** (`SubGhzProtocolDecoderNiceFlorS`, nur Decoder): zwei neue Felder
   `uint16_t ic` und `bool o_code`. In `subghz_protocol_decoder_nice_flor_s_alloc()` auf
   `NICE_FLOR_S_IC_PLAIN`/`false` initialisiert.
2. **`subghz_protocol_decoder_nice_flor_s_reset()`**: setzt beide Felder bei jedem Rücksetzen
   der Parser-Zustandsmaschine ebenfalls zurück — genau das in der Vorsitzung unter „Port-Skizze
   Schritt 1" als notwendig dokumentierte Detail, damit eine zuvor geladene O-Code-Datei nicht
   den `o_code`/`ic`-Zustand einer späteren, komplett unabhängigen Live-Aufnahme verfälscht (der
   Flipper erzeugt für Laden und Live-Empfang dieselbe Decoder-Instanz, nicht zwei getrennte).
3. **`subghz_protocol_decoder_nice_flor_s_deserialize()`**: liest nach dem bereits bestehenden
   optionalen `CounterMode`-Feld (gleiches Rewind-Muster) ein neues optionales `IC`-Hex-Feld
   (2 Byte, big-endian — dieselbe Byte-Reihenfolge wie das bestehende 8-Byte-`Key`-Feld). Fehlt
   das Feld (jede bisherige Datei), bleibt `o_code = false`, `ic = NICE_FLOR_S_IC_PLAIN` — exakt
   der Zustand nach `alloc()`.
4. **`subghz_protocol_decoder_nice_flor_s_serialize()`**: schreibt das `IC`-Feld nur, wenn
   `o_code` gesetzt ist (kann aktuell nur durch Deserialisieren einer bereits mit `IC`-Feld
   versehenen Datei passieren — ohne Phase 3/Encoder gibt es noch keinen Weg, wie eine frisch
   empfangene Live-Aufnahme `o_code = true` bekäme).
5. **`subghz_protocol_decoder_nice_flor_s_get_string()`**: neue Verzweigung an die Funktionsspitze
   gesetzt — bei `o_code` wird `subghz_protocol_nice_o_get_parcel()` auf `instance->generic.data`
   angewandt, daraus mit `subghz_protocol_nice_o_mask()` die Maske für genau dieses Paket
   bestimmt, und `instance->ic ^ mask_16` an `subghz_protocol_nice_flor_s_decrypt_ic()` übergeben
   (Ciphertext-Argument: die unteren 44 Bit von `instance->generic.data`, nach demselben Schema
   wie in Schritt 6 der Vorsitzungs-Pipeline bereits verifiziert). `cnt`/`serial`/`btn` werden aus
   dem Ergebnis exakt wie in jener Pipeline rekonstruiert; `btn` kommt — wie beim bestehenden
   Plain-Pfad auch — direkt aus dem unverschlüsselten oberen Rahmenbyte, nicht aus dem
   Chiffretext. Ohne `o_code` bleibt der bestehende Aufruf von
   `subghz_protocol_nice_flor_s_remote_controller()` unverändert; die bestehenden
   `furi_string_cat_printf()`-Aufrufe für `Nice One`/`Nice FloR-S` sind **byte-identisch**
   unverändert, nur um eine zusätzliche, rein additive `IC:%04X`-Zeile ergänzt, wenn `o_code`
   gesetzt ist.

## Warum nicht mehr (Risiko/Abgrenzung)

Bewusst nicht Teil dieser Sitzung, unverändert aus der Vorsitzung zurückgestellt:

- **`feed()`** bleibt komplett unverändert. Jede Live-Aufnahme hat nach dieser Sitzung weiterhin
  `o_code == false` (siehe `reset()` oben) und nimmt exakt den bisherigen Pfad — kein Verhalten
  am produktiven Empfangspfad geändert, auch nicht für ein tatsächlich empfangenes O-Code-Signal
  (das läuft weiterhin wie zuvor als falsches Flor-S durch, siehe „Fund" der Vorsitzung).
- **Encoder** (`subghz_protocol_encoder_nice_flor_s_get_upload()`) unverändert — kein Senden
  einer geladenen O-Code-Datei möglich, nur Anzeigen. Eine über die UI versuchte Sende-Aktion für
  eine solche Datei nimmt weiterhin den Plain-Encoder-Pfad (unverändert, aber kryptografisch für
  O-Code falsch) — das ist kein neues Risiko dieser Sitzung, sondern derselbe vorbestehende
  Zustand wie vor diesem Patch, da der Encoder in keiner der beiden Sitzungen angefasst wurde.
- Installateurcode-Recovery-App weiterhin nicht begonnen.

Das `IC`-Feld selbst ist ausschließlich additiv: eine `.sub`-Datei ohne dieses Feld (jede bisher
existierende) verhält sich nach diesem Patch exakt wie davor, siehe Verifikationsschritt 1 unten
(bitgenau identische Ausgabe von `get_string()`-Äquivalent ALT/NEU bei fehlendem Feld).

## Verifikation

**Patch-Anwendung** (unabhängig von diesem Sitzungs-Workdir nachvollzogen): frische,
unabhängige Kopie von `lib/subghz/protocols/{nice_flor_s.c,nice_flor_s.h,public_api.h}`
(`Next-Flip/Momentum-Firmware`, Tag `mntm-012`, per `raw.githubusercontent.com`) in einem leeren
Git-Repo initialisiert; `firmware-mining/nice-o-code-subghz-port/nice_flor_s_c.patch` sowie
`_public_api_h.patch` angewendet (`git apply --check` vorab grün), committet; anschließend
`firmware-mining/nice-o-code-decode-display-port/nice_flor_s_c.patch` `git apply --check`
**und** real angewendet — beides ohne Fehler. Das Ergebnis per `diff` byte-für-byte gegen die in
dieser Sitzung tatsächlich editierte Arbeitskopie abgeglichen: identisch.

**Host-Repro** (`firmware-mining/repro-nice-o-code-decode-display/repro.c`) transkribiert nicht
nur die Chiffre (wie die Vorsitzung bereits tat), sondern die tatsächlich neue Integrationslogik
selbst — `alloc()`s zwei neue Zeilen, `reset()`s zwei neue Zeilen, das `IC`-Feld-Lesen/Schreiben
aus `deserialize()`/`serialize()`, und exakt die neue `get_string()`-Verzweigung — als eigene,
benannte Funktionen, die 1:1 den im Patch stehenden Anweisungen entsprechen. Fünf Prüfschritte
unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined`:

1. **Rückwärtskompatibilitäts-Regression** (2000 zufällige Plain-Flor-S-Rahmen, `IC`-Feld
   fehlend): `get_string_decode()`-Verzweigung liefert bitgenau dasselbe Ergebnis wie ein
   direkter, unveränderter Aufruf des bestehenden Plain-Pfads — und beide rekonstruieren
   Seriennummer/Zähler/Taste exakt.
2. **O-Code Ende-zu-Ende über die echten Integrationspunkte** (200 000 Iterationen, zufällige
   Seriennummer/Zähler/Taste/IC/Paketindex): synthetischer Über-die-Luft-Rahmen gebaut, über
   `deserialize_ic_field()` (echtes Feld-Vorhandensein) geladen, über `get_string_decode()`
   dekodiert — Seriennummer/Zähler/Taste exakt zurückgewonnen in allen 200 000 Fällen; dieselbe
   IC auch beim Zurückschreiben (`serialize_ic_field()`) reproduziert; ein falscher
   Installateurcode (`ic+1`) liefert nachweislich nicht dieselbe (Seriennummer, Zähler).
3. **`reset()` löscht veralteten O-Code-Zustand** (2000 Iterationen): simuliert exakt das in
   „Warum nicht mehr" benannte Szenario — O-Code-Datei laden, `reset()` (Szenenwechsel zurück
   zum Live-Empfang), dann eine neue, unabhängige Plain-Live-Aufnahme — korrekt dekodiert,
   unabhängig vom zuvor geladenen (jetzt gelöschten) Installateurcode.
4. **Symmetrie des Feld-Fehlens**: `serialize_ic_field()` schreibt das `IC`-Feld nachweislich
   nicht, wenn `o_code` falsch ist (Out-Parameter bleibt unberührt).
5. **Durchgerechnetes Beispiel** (fester Seriennummer-/Zähler-/Tasten-/IC-/Paketindex-Wert) zur
   manuellen Gegenprobe gegen eine von Hand bearbeitete `.sub`-Datei ausgegeben.

**Ergebnis: 1 621 999 Prüfungen, ALLE CHECKS BESTANDEN, keine Compiler-Warnungen, keine
ASan-/UBSan-Meldung bei `-O0` oder `-O1`.**

## Lokaler Testhinweis

1. Beide Patches in der oben genannten Reihenfolge auf eine eigene `mntm-012`-Basis anwenden,
   bauen, flashen.
2. Regressionstest (wichtiger als der neue Teil): jede bereits vorhandene eigene Nice-FloR-S-/
   Nice-One-`.sub`-Aufnahme weiterhin unverändert laden und anzeigen — Anzeige muss exakt wie vor
   dem Patch aussehen (kein `IC:`-Zusatz, identische Sn/Cnt/Btn-Werte).
3. Für den neuen Pfad: eine bestehende eigene Flor-S-`.sub`-Datei kopieren, von Hand ein
   `IC: XX XX` Hex-Feld ergänzen (2 Byte, dieselbe Zeilenform wie das bestehende `Key:`-Feld in
   derselben Datei), mit einem frei gewählten Wert. Da der zugehörige echte Installateurcode
   nicht bekannt ist, zeigt die Anzeige dann zwangsläufig eine falsche (aber nicht abstürzende)
   Seriennummer/Zähler — ausreichend, um zu bestätigen, dass die neue `IC:`-Zeile erscheint und
   kein `furi_check()`-Abbruch/Absturz auftritt. Der eigentliche kryptografische Nachweis läuft
   bereits vollständig über das Host-Repro oben (keine echte O-Code-Aufnahme im Besitz, siehe
   `CONSTRAINTS.md`: keine fremden Kartendaten/Captures in diesem Repo).

## Aufwand/Umfang

Firmware-Feature-Mining, additiver Folge-Patch auf einen bereits dokumentierten, mehrteiligen
Port (Phase 2 von 3 aus der Port-Skizze der Vorsitzung) — klein und sicher im Sinne von
`WORKFLOW.md` (nur Decoder, kein `feed()`, kein Encoder), aber nur ein Teilschritt des
Gesamtvorhabens, nicht dessen Abschluss.

## Offen (für eine künftige Sitzung)

- **Phase 3 (`feed()`-Live-Erkennung)**: die in der Vorsitzung skizzierte Zwei-Pakete-Heuristik,
  die ein unbestätigtes O-Code-Paket vom bereits produktiven Plain-FloR-S-Pfad unterscheidet,
  ohne diesen zu verzögern/zu verändern. Bleibt der riskanteste Teil, da er den bereits
  produktiven Empfangspfad berührt.
- **Encoder-Gegenstück**: `get_upload()` pro Paket mit `encrypt_ic()` plus Maske statt der
  bestehenden einmaligen `encrypt()`, nur wenn `instance->o_code` gesetzt ist — würde das
  Senden einer über diese Sitzung geladenen O-Code-Datei ermöglichen.
- **Installateurcode-Recovery-App**: eigenständiges `.fap`, weiterhin nicht begonnen.

## Sicherheitsrahmen

Ausschließlich Lese-/Anzeigefunktionalität für ein bereits öffentlich dokumentiertes
Consumer-Funkprotokoll (Nice-Torantriebe), zur Verwendung an eigenen/autorisierten Anlagen. Keine
fremden Aufnahmen oder Kartendaten in diesem Repo verwendet oder erzeugt — die
Host-Verifikation läuft ausschließlich mit synthetisch erzeugten Testwerten.

## Quellen

- Vorsitzung: `firmware-mining/nice-o-code-subghz-port.md` (Herkunft der vier Hilfsfunktionen,
  vollständige Protokoll-/Masken-Herleitung, Quellenangaben zu Unleashed-Firmware Commit
  `945234a` und den ursprünglichen Nice-FloR-S-Quellen).
- Momentum-Firmware, Tag `mntm-012`: `lib/subghz/protocols/nice_flor_s.c`/`.h` (Ausgangszustand
  dieser Sitzung inklusive der bereits vorgeschlagenen Phase-1-Hilfsfunktionen),
  `lib/flipper_format/flipper_format.h` (Signaturen von `flipper_format_read_hex()`/
  `_insert_or_update_hex()`, exakt wie hier verwendet).
