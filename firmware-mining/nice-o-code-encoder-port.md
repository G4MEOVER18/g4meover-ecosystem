# Nice O-Code: Phase 3 des Ports — Encoder-Gegenstück (Senden)

## Was

Fortsetzung von `firmware-mining/nice-o-code-decode-display-port.md` (Phase 2: Decoder-Struct,
`.sub`-I/O, Anzeige) und `firmware-mining/nice-o-code-subghz-port.md` (Phase 1: Krypto-Grundlagen).
Diese Sitzung liefert den in Phase 1 unter „Port-Skizze Schritt 5" benannten Encoder-Teil: eine
bereits mit einem `IC`-Feld versehene `.sub`-Datei lässt sich jetzt tatsächlich **senden**, nicht
nur anzeigen. Bewusst **nicht** angefasst: `feed()` (Phase 2/3 der ursprünglichen Skizze, die
Zwei-Pakete-Live-Erkennung am produktiven Empfangspfad) und die Installateurcode-Recovery-App —
beide bleiben wie in den Vorsitzungen dokumentiert zurückgestellt. Diese Sitzung berührt
ausschließlich den Encoder (Senden einer bereits geladenen Datei); der Empfangspfad (`feed()`,
Decoder) ist von diesem Patch nicht betroffen und unverändert.

Ergebnis: eine Flor-S-`.sub`-Datei mit einem von Hand (oder von einer künftigen Recovery-App)
ergänzten `IC`-Hex-Feld lässt sich jetzt laden und senden — der Encoder erzeugt für jedes der 16
pro Tastendruck übertragenen Pakete eine eigene, paket-abhängig maskierte Verschlüsselung statt
(kryptografisch falsch) denselben Klartext-Ciphertext 16-mal zu wiederholen.

## Nutzen

- Schließt die zweite der beiden in Phase 2 unter „Warum nicht mehr" benannten Lücken: „Eine über
  die UI versuchte Sende-Aktion für eine [O-Code-]Datei nimmt weiterhin den Plain-Encoder-Pfad
  (unverändert, aber kryptografisch für O-Code falsch)" — genau das behebt dieser Patch.
- Zusammen mit Phase 2 ergibt sich jetzt ein vollständiger Lese-/Schreib-Zyklus für eine bereits
  bekannte Installateurcode: laden → anzeigen (Phase 2) → senden (diese Sitzung) → eine
  gegenständliche Empfangsstelle (z. B. ein zweiter eigener Flipper) kann das gesendete Signal mit
  dem bereits in Phase 2 verifizierten Decoder-Pfad wieder korrekt als O-Code dekodieren.
- Bleibt vollständig additiv und durch `instance->o_code` (Default `false`) abgeschirmt: jede
  bisher gespeicherte Datei (kein `IC`-Feld) sendet exakt wie vor diesem Patch, bitidentisch.

## Betroffene Dateien

- `lib/subghz/protocols/nice_flor_s.c` — additiv, nur den Encoder betreffend:
  `firmware-mining/nice-o-code-encoder-port/nice_flor_s_c.patch`
  - **Baut auf dem bereits vorliegenden Phase-1-Patch auf** (liefert
    `subghz_protocol_nice_flor_s_encrypt_ic()`/`subghz_protocol_nice_o_mask()`, hier
    wiederverwendet). **Unabhängig von Phase 2**: beide ändern denselben Quelltext, aber
    disjunkte Funktionen/Structs (Phase 2 nur `SubGhzProtocolDecoderNiceFlorS` und die
    Decoder-Funktionen, diese Sitzung nur `SubGhzProtocolEncoderNiceFlorS` und die
    Encoder-Funktionen) — `git apply --check` aller drei Patches in beiden Reihenfolgen
    (P1→P2→P3 und P1→P3, ohne P2) gegen eine frische, unabhängige Kopie verifiziert, siehe
    „Verifikation" unten.
  - Kein `public_api.h`-, kein Registry-Eintrag betroffen (identisch zur Einschätzung der
    Vorsitzungen).

## Änderungen im Detail

1. **Struct-Erweiterung** (`SubGhzProtocolEncoderNiceFlorS`): zwei neue Felder `uint16_t ic` und
   `bool o_code`, exakt wie bereits beim Decoder in Phase 2. In
   `subghz_protocol_encoder_nice_flor_s_alloc()` auf `NICE_FLOR_S_IC_PLAIN`/`false` initialisiert.
2. **`subghz_protocol_encoder_nice_flor_s_deserialize()`**: liest nach dem bereits bestehenden
   optionalen `CounterMode`-Feld (gleiches Rewind-Muster wie Phase 2) ein neues optionales
   `IC`-Hex-Feld. Fehlt es (jede bisherige Datei), bleibt `o_code = false` und die Funktion ruft
   wie bisher ausschließlich die bestehende, unveränderte
   `subghz_protocol_nice_flor_s_remote_controller()` auf, um aus dem geladenen `Key`-Feld
   `serial`/`cnt`/`btn` zu rekonstruieren. Ist das Feld vorhanden, kann `remote_controller()`
   nicht verwendet werden — sie kennt nur Plain-Flor-S' Ein-Chiffretext-Schema — stattdessen
   rekonstruiert ein neuer Zweig `serial`/`cnt`/`btn` aus dem geladenen `Key`-Rahmen exakt nach
   dem in Phase 2 bereits verifizierten Schema: Paketindex aus den unverschlüsselten
   P0/P1-Kopfbits (`subghz_protocol_nice_o_get_parcel()`), daraus die Maske
   (`subghz_protocol_nice_o_mask()`), damit `subghz_protocol_nice_flor_s_decrypt_ic()` auf die
   unteren 44 Bit des geladenen Rahmens angewandt. `btn` kommt wie beim bestehenden Plain-Pfad
   auch direkt aus dem unverschlüsselten oberen Rahmenbyte.
3. **`subghz_protocol_encoder_nice_flor_s_get_upload()`**: neue Verzweigung innerhalb der
   bestehenden Schleife über die 16 Wiederholungen (`for(int i = 0; i < 16; i++)`). Bisher wird
   `enc_part` genau einmal vor der Schleife berechnet und in jeder der 16 Iterationen
   unverändert wiederverwendet (Plain-Flor-S: derselbe Chiffretext 16-mal). Bei `o_code` wird
   `enc_part` jetzt pro Iteration neu berechnet: Maske für den aktuellen Paketindex
   (`loops[i]`) bestimmen, Seriennummer und Installateurcode damit XOR-verknüpfen, dann mit
   `subghz_protocol_nice_flor_s_encrypt_ic()` statt der einmaligen
   `subghz_protocol_nice_flor_s_encrypt()` verschlüsseln — exakt die in Phase 1 (Repro-Schritt
   6) bereits verifizierte Pipeline, jetzt in die reale Sende-Schleife eingehängt statt
   synthetisch nachgebaut. Ohne `o_code` bleibt `enc_part` über alle 16 Iterationen unverändert,
   die Schleife erzeugt bitidentisch dieselben 16 Pakete wie vor diesem Patch.

## Warum nicht mehr (Risiko/Abgrenzung)

- **`feed()`** bleibt komplett unverändert — dieser Patch berührt ausschließlich den
  Encoder/Sendepfad, nicht den Zustandsautomaten des Empfangs. Ein tatsächlich über Funk
  empfangenes O-Code-Signal läuft weiterhin unverändert wie in den Vorsitzungen dokumentiert
  (Fehlklassifikation als falsches Flor-S) durch — diese Sitzung ändert daran nichts.
- **Installateurcode-Recovery-App** weiterhin nicht begonnen — ohne sie bleibt der praktische
  Weg zu einer gültigen `IC` entweder eine andere Quelle (z. B. am Empfänger abgelesen) oder
  manuelles Eintragen in eine Kopie einer vorhandenen Flor-S-Datei.
- **Nice One (72 Bit) + O-Code kombiniert**: in dieser Sitzung nachträglich mit einem eigenen
  Host-Repro-Szenario abgedeckt (siehe „Verifikation" unten) — war in der Vorsitzung unter
  „Offen" vermerkt, da die zusätzlichen 20 Bit für Nice One (`data_2`, CRC) unverändert aus dem
  bereits gebildeten `instance->generic.data` abgeleitet werden (`subghz_protocol_nice_one_get_
  data()`), unabhängig davon, ob dessen unterer 44-Bit-Teil aus dem Plain- oder dem IC-Pfad
  stammt. Bestätigt jetzt mit echtem Host-Repro statt nur struktureller Einschätzung.
- Wie in Phase 1/2: die exakte Byte-Reihenfolge/Vorzeichen stammt aus Unleashed, nicht gegen
  eine echte O-Code-Funkaufnahme verifizierbar (keine eigene Aufnahme vorhanden,
  `CONSTRAINTS.md` untersagt ohnehin fremde Kartendaten/Captures in diesem Repo). Die
  Host-Verifikation unten beweist Selbstkonsistenz (ein mit dieser Sitzung gesendetes Signal
  wird vom bereits in Phase 2 verifizierten Decoder-Pfad korrekt zurückgewonnen), nicht
  Übereinstimmung mit dem realen Funkprotokoll.

Additiv, durch `instance->o_code` abgeschirmt, verhaltensneutral bei fehlendem `IC`-Feld.

## Verifikation

**Patch-Anwendung** (unabhängig von diesem Sitzungs-Workdir nachvollzogen): frische,
unabhängige Kopie von `lib/subghz/protocols/nice_flor_s.c` (`Next-Flip/Momentum-Firmware`, Tag
`mntm-012`, per `raw.githubusercontent.com`) in einem leeren Git-Repo initialisiert;
`firmware-mining/nice-o-code-subghz-port/nice_flor_s_c.patch` (Phase 1) angewendet, committet;
anschließend `firmware-mining/nice-o-code-encoder-port/nice_flor_s_c.patch` (diese Sitzung)
`git apply --check` **und** real angewendet — ohne Fehler. Zusätzlich in einer zweiten, separat
aufgesetzten Kopie dieselbe Reihenfolge inklusive Phase 2
(`firmware-mining/nice-o-code-decode-display-port/nice_flor_s_c.patch`) dazwischengeschaltet
(P1 → P2 → P3) geprüft: ebenfalls fehlerfrei, da Phase 2 und Phase 3 disjunkte
Funktionen/Structs ändern. Das Ergebnis beider Kopien per `diff` byte-für-byte abgeglichen:
identisch zur tatsächlich editierten Arbeitskopie dieser Sitzung.

**Host-Repro** (`firmware-mining/repro-nice-o-code-encoder/repro.c`) transkribiert die
tatsächlich neue Encoder-Integrationslogik selbst — `alloc()`s zwei neue Zeilen, das
`IC`-Feld-Lesen samt neuem Entschlüsselungszweig aus `deserialize()`, und exakt die neue
`get_upload()`-Verzweigung — als eigene, benannte Funktionen, die 1:1 den im Patch stehenden
Anweisungen entsprechen. Fünf Prüfschritte (der fünfte in dieser Sitzung ergänzt, siehe unten)
unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined`:

1. **Rückwärtskompatibilitäts-Regression** (2000 zufällige Plain-Flor-S-Dateien, `IC`-Feld
   fehlend): `deserialize()` rekonstruiert `serial`/`cnt`/`btn` exakt aus dem geladenen Rahmen;
   alle 16 von `get_upload()` erzeugten Pakete sind bitgenau identisch zu dem, was der
   unveränderte, ursprüngliche Encoder für dieselben Werte erzeugen würde (direkter Vergleich
   gegen eine zweite, unabhängige Transkription von `subghz_protocol_nice_flor_s_encrypt()`
   plus der bestehenden Paket-Byte-Formel); alle 16 Pakete tragen denselben Chiffretext, wie
   vor diesem Patch.
2. **O-Code Send→Receive über alle 16 Pakete** (200 000 Iterationen, zufällige
   Seriennummer/Zähler/Taste/IC/Paketindex der geladenen Datei): eine Datei mit einem
   synthetischen, nach der Phase-1-Pipeline gebauten `Key`+`IC` geladen, über `deserialize()`
   (echter neuer Entschlüsselungszweig) `serial`/`cnt`/`btn` zurückgewonnen — exakt die
   Ausgangswerte in allen 200 000 Fällen. Die daraus von `get_upload()` erzeugten 16
   Sendepakete **einzeln** mit dem in Phase 2 bereits verifizierten Decoder-Pfad
   (`get_string()`-Äquivalent) dekodiert: **jedes** der 16 Pakete liefert unabhängig
   voneinander wieder exakt `serial`/`cnt`/`btn` — der eigentliche Nachweis, dass ein mit
   dieser Sitzung gesendetes Signal an einer Empfangsstelle korrekt ankäme. Zusätzlich
   bestätigt: die 16 erzeugten Chiffretexte sind tatsächlich nicht alle gleich (O-Code
   unterscheidet sich sichtbar von Plain-Verhalten), und ein falscher Installateurcode beim
   Dekodieren liefert nachweislich nicht dieselbe (Seriennummer, Zähler).
3. **`IC`-Feld-Abwesenheitssymmetrie**: `deserialize()` setzt `o_code` nachweislich nicht,
   wenn kein `IC`-Feld vorhanden ist, unabhängig vom (nicht gelesenen) Pufferinhalt.
4. **Durchgerechnetes Beispiel** (fester Seriennummer-/Zähler-/Tasten-/IC-Wert, Paket 9 von 16)
   zur manuellen Gegenprobe ausgegeben.
5. **Nice One (72 Bit) + O-Code kombiniert** (neu in dieser Sitzung, 50 000 O-Code- plus 2 000
   Plain-Iterationen): transkribiert zusätzlich den unveränderten, von diesem Patch nicht
   berührten Nice-One-Anhang (`subghz_protocol_blocks_get_bit_array()`,
   `subghz_protocol_nice_one_crc()`, `subghz_protocol_nice_one_get_data()`) und wendet ihn auf
   die bereits in Schritt 2 erzeugten 16 Sendepakete an — exakt wie `get_upload()`s reale
   `NICE_ONE_COUNT_BIT`-Verzweigung, die auf einer *Kopie* des bereits gebildeten 52-Bit-Rahmens
   arbeitet, nie auf dem Rahmen selbst. Je Paket vier Prüfungen: (a) der 52-Bit-Rahmen bleibt
   über den bereits in Schritt 2 verifizierten Decoder-Pfad unverändert korrekt dekodierbar,
   unabhängig davon, ob zusätzlich ein Nice-One-Anhang berechnet wird; (b) der Anhang ist
   deterministisch (zweimalige Berechnung desselben Rahmens/Paketindex liefert denselben Wert);
   (c) ein unabhängig aus (Seriennummer, Zähler, Taste, IC, Paketindex) über die Masken-/
   `encrypt_ic()`-Formel neu zusammengesetzter 52-Bit-Rahmen stimmt mit dem von `get_upload()`
   erzeugten exakt überein, und beide liefern denselben Nice-One-Anhang; (d) der Anhang variiert
   über die 16 Pakete (kein konstanter/eingefrorener Wert). Zusätzlich dieselben vier Prüfungen
   (ohne (a), da hier kein IC existiert) auch für den unveränderten Plain-Pfad (Nice One ohne
   O-Code) wiederholt, um zu bestätigen, dass die bereits bestehende Kombination durch diesen
   Patch unangetastet bleibt.

**Ergebnis: 16 783 921 Prüfungen, ALLE CHECKS BESTANDEN, keine Compiler-Warnungen, keine
ASan-/UBSan-Meldung bei `-O0` oder `-O1`.**

## Lokaler Testhinweis

1. Phase 1 und diesen Patch (Reihenfolge: zuerst Phase 1) auf eine eigene `mntm-012`-Basis
   anwenden; optional zusätzlich Phase 2 (beliebige Position relativ zu diesem Patch, siehe
   „Verifikation"), bauen, flashen.
2. Regressionstest (wichtiger als der neue Teil): jede bereits vorhandene eigene
   Nice-FloR-S-/Nice-One-`.sub`-Datei weiterhin unverändert senden können — das gesendete
   Signal muss (mit einem zweiten eigenen Empfänger oder einem Oszilloskop) identisch zum
   Verhalten vor diesem Patch sein.
3. Für den neuen Pfad (benötigt Phase 2 für die Anzeige, ist aber auch ohne sie sendefähig):
   eine bestehende eigene Flor-S-`.sub`-Datei kopieren, ein `IC: XX XX` Hex-Feld ergänzen (2
   Byte, wie in Phase 2 beschrieben) und senden. Da der zugehörige echte Installateurcode
   nicht bekannt ist, entsteht zwangsläufig ein für eine reale Nice-Anlage ungültiges Signal —
   ausreichend, um zu bestätigen, dass kein `furi_check()`-Abbruch/Absturz beim Senden
   auftritt und (mit Phase 2 installiert) ein zweiter eigener Flipper das gesendete Signal als
   O-Code mit genau dem eingetragenen `IC` empfängt und anzeigt. Der eigentliche
   kryptografische Nachweis läuft bereits vollständig über das Host-Repro oben.

## Aufwand/Umfang

Firmware-Feature-Mining, additiver Folge-Patch auf einen bereits dokumentierten, mehrteiligen
Port (Phase 3 von 3 aus der ursprünglichen Port-Skizze, abzüglich `feed()` und der
Recovery-App, die eigenständig zurückgestellt bleiben) — klein und sicher im Sinne von
`WORKFLOW.md` (nur Encoder, kein `feed()`), schließt aber den in den Vorsitzungen offen
gelassenen Sende-Pfad ab.

## Offen (für eine künftige Sitzung)

- **`feed()`-Live-Erkennung**: unverändert der riskanteste, bisher zurückgestellte Teil — berührt
  den bereits produktiven Empfangspfad, siehe Begründung in
  `firmware-mining/nice-o-code-subghz-port.md`.
- **Installateurcode-Recovery-App**: eigenständiges `.fap`, weiterhin nicht begonnen.

## Sicherheitsrahmen

Ausschließlich Sendefunktionalität für ein bereits öffentlich dokumentiertes
Consumer-Funkprotokoll (Nice-Torantriebe), zur Verwendung an eigenen/autorisierten Anlagen und
nur nutzbar, wenn der Installateurcode bereits auf anderem Weg bekannt ist (dieser Patch
berechnet/errät keinen). Keine fremden Aufnahmen oder Kartendaten in diesem Repo verwendet oder
erzeugt — die Host-Verifikation läuft ausschließlich mit synthetisch erzeugten Testwerten.

## Quellen

- Vorsitzungen: `firmware-mining/nice-o-code-subghz-port.md` (Phase 1, Herkunft der
  Hilfsfunktionen und vollständige Protokoll-/Masken-Herleitung),
  `firmware-mining/nice-o-code-decode-display-port.md` (Phase 2, Decoder-Pendant dieses
  Patches, hier als Empfangsstelle für die Host-Verifikation wiederverwendet).
- Momentum-Firmware, Tag `mntm-012`: `lib/subghz/protocols/nice_flor_s.c` (Ausgangszustand
  dieser Sitzung inklusive der bereits vorgeschlagenen Phase-1-Hilfsfunktionen) sowie für
  Schritt 5 zusätzlich frisch per `raw.githubusercontent.com` bezogen und im Detail
  nachvollzogen: der unveränderte Nice-One-Anhang (`subghz_protocol_nice_one_crc()`/
  `subghz_protocol_nice_one_get_data()` in derselben Datei) und `subghz_protocol_blocks_
  get_bit_array()` (`lib/subghz/blocks/encoder.c`).
