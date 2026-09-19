# KeeLoq Monarch — Zweiten Diskriminator-Wert erkennen (Port aus Unleashed)

## Was
Ich habe gegen meine Basis mntm-012 (`lib/subghz/protocols/keeloq.c`) den Unleashed-Commit
`37e7db36f` ("sub hertz", `dev`-Branch, 18.09.2026, API 88.11) geprüft. Er behandelt zwei
KeeLoq-Hersteller neu: eine Korrektur für den bereits vorhandenen Hersteller **Monarch** und
einen komplett neuen Hersteller **KEY**. Ich habe nur den Monarch-Teil geprüft und portiert —
Details und Begründung für den zurückgestellten KEY-Teil siehe „Nicht mitgeliefert" unten.

Meine Basis behandelt Monarch als „Normal Learning ohne Seriennummer im Hop, fester
Diskriminator" — sowohl beim Empfangen (`subghz_protocol_keeloq_check_remote_controller_selector()`)
als auch beim Neuerzeugen (`subghz_protocol_keeloq_gen_data()`) wird der Diskriminator
hartkodiert als `0x100` angenommen. Beim Empfangen gibt es dafür sogar keinen eigenen
Prüfzweig: Monarch fällt in der `KEELOQ_LEARNING_NORMAL`-Fallunterscheidung in den generischen
`else`-Zweig (`subghz_protocol_keeloq_check_decrypt()`), der eigentlich für Hersteller mit
einer echten Seriennummer im Hop gedacht ist und dort `(hop >> 16) & 0xFF` gegen `end_serial`
(oder gegen `0`) vergleicht.

Das funktioniert für Monarch-Handsender mit Diskriminator `0x100` nur zufällig: `0x100 & 0xFF`
ergibt `0x00`, trifft also den `== 0`-Rückfallzweig, unabhängig vom tatsächlichen `end_serial`.
Unleashed hat jetzt einen zweiten, real vorkommenden Diskriminator-Wert identifiziert
(`0x280`, laut ihrem CHANGELOG beim Dorcas 22366CLN / D-22366CLN), dessen niedrigstes Byte
`0x80` ist — das trifft weder den `== 0`-Fall noch (außer durch Zufall) `end_serial`, und wird
deshalb bei mir aktuell **nicht als Monarch erkannt**, sondern als „Unknown" verworfen, obwohl
der Schlüssel im Grunde passen würde. Zusätzlich wird der tatsächlich empfangene
Diskriminator-Wert nirgends gespeichert: `gen_data()` (genutzt fürs Neusenden/Replay mit
geändertem Zähler) nimmt immer `0x100` an, unabhängig davon, was tatsächlich empfangen wurde —
selbst ein 0x100-Handsender würde bei einer künftigen dritten Diskriminator-Variante falsch
neu kodiert.

Unleashed löst beides, indem der erkannte Diskriminator in `generic.seed` gemerkt wird (analog
zu BFTs Secure-Learning-Seed) und sowohl der Empfangs- als auch der Neuerzeugungs-Pfad beide
bekannten Werte kennen.

## Nutzen
- Ein real vorkommender Monarch-Handsender-Typ (Dorcas 22366CLN, Diskriminator `0x280`), den ich
  aktuell komplett verwerfe („Unknown"), wird korrekt als Monarch erkannt und angezeigt.
- Der beim Empfang tatsächlich gesehene Diskriminator wird für das Neusenden/Replay
  wiederverwendet statt hartkodiert `0x100` anzunehmen — ein empfangenes `0x280`-Signal
  erzeugt nach dieser Änderung beim Neukodieren wieder ein `0x280`-Signal, nicht fälschlich
  `0x100`.
- Der Diskriminator übersteht jetzt auch das Speichern/Laden einer `.sub`-Datei (wie schon bei
  BFT), statt beim Neuladen auf den `0x100`-Standardwert zurückzufallen.
- Rein additiv am bestehenden Monarch-Zweig: kein neues Protokoll, keine neue Datei, keine
  Änderung an anderen Herstellern (Centurion, BFT, Dea_Mio, …) — deren jeweilige Zweige bleiben
  unberührt.
- Autorisierter Pentest-/Analysekontext wie bei jedem anderen KeeLoq-Hersteller in meiner Basis
  bereits (eigene bzw. vertraglich freigegebene Toranlagen/Garagenantriebe).

## Betroffene Dateien (lokal)
- `lib/subghz/protocols/keeloq.c` — Unified-Diff beiliegend:
  `firmware-mining/keeloq-monarch-discriminator-fix/keeloq_c.patch`. Vier zusammengehörige
  Stellen in derselben Datei:
  1. Zwei neue Konstanten `KEELOQ_MONARCH_DISCRIMINATOR_A` (`0x100`) und `_B` (`0x280`) nach dem
     `TAG`-Define.
  2. `subghz_protocol_keeloq_gen_data()`, Monarch-Zweig: liest jetzt `instance->generic.seed`
     (auf 12 Bit maskiert, wie der Diskriminator im Hop kodiert ist) und fällt auf `_A` (`0x100`)
     zurück, falls der Wert keiner der beiden bekannten Konstanten entspricht (deckt sowohl „noch
     nie empfangen" als auch einen dritten, unbekannten Wert ab).
  3. Neue Funktion `subghz_protocol_keeloq_check_decrypt_monarch()` direkt nach der bestehenden
     `subghz_protocol_keeloq_check_decrypt_centurion()` — exakt demselben Aufbau folgend
     (Tasten-Nibble-Vergleich plus Diskriminator-Vergleich, hier gegen beide bekannten Werte statt
     gegen einen einzelnen festen Wert), setzt bei Erfolg zusätzlich `instance->seed` auf den
     erkannten Diskriminator.
  4. `subghz_protocol_keeloq_check_remote_controller_selector()`, `KEELOQ_LEARNING_NORMAL`-Fall:
     neuer `else if`-Zweig für `"Monarch"` (parallel zum bestehenden `"Centurion"`-Zweig), ruft die
     neue Prüffunktion statt in den generischen `else`-Zweig zu fallen.
  5. `subghz_protocol_decoder_keeloq_serialize()`: die bisher BFT-exklusive
     Seed-Speicherbedingung (`if(strcmp(instance->manufacture_name, "BFT") == 0)`) um
     `|| (strcmp(instance->manufacture_name, "Monarch") == 0)` erweitert, damit der erkannte
     Diskriminator wie bei BFT als `Seed`-Feld in die `.sub`-Datei geschrieben und beim Neuladen
     wieder eingelesen wird.

Kein Registry-Eintrag nötig (Monarch existiert bei mir bereits als Hersteller), keine neue Datei,
keine Änderung an `protocol_items.c/.h`.

## Nicht mitgeliefert: neuer Hersteller „KEY"
Unleasheds Commit führt zusätzlich einen komplett neuen KeeLoq-Hersteller `"KEY"` ein (fester
Diskriminator `0x2FF`, Simple Learning) samt eigenem `check_decrypt_key()`-Prüfzweig und einem
neuen „Advanced Add Manually"-Menüpunkt („KL: KEY 433MHz"). Das habe ich bewusst nicht mit
übernommen: ob ich für den Hersteller-Namen `"KEY"` überhaupt einen passenden Schlüssel in meiner
eigenen `keeloq_mfcodes`-Ressource besitze, kann ich von hier aus nicht prüfen — diese Datei wird
verschlüsselt separat vom Firmware-Quellcode ausgeliefert und liegt in diesem Repo nicht vor
(derselbe Grund, aus dem frühere Firmware-Mining-Sitzungen z. B. beim Superrollo/GW60-Port den
zugehörigen `keeloq_mfcodes`-Ressourcen-Diff bewusst nicht übernommen haben). Ohne passenden
Schlüssel wäre der neue Prüfzweig totes Gerüst; mit einem falsch angenommenen Schlüssel bestünde
das Risiko, Klartext-Schlüsselmaterial falsch zu handhaben. Für eine künftige Sitzung vorgemerkt:
erst lokal prüfen, ob `"KEY"` in der eigenen Ressource vorkommt, dann ggf. denselben Prüfzweig
plus die zugehörige „Advanced Add Manually"-UI (neuer `GenKeeloqSeed`-Menüpunkt, `SetTypeKEY433`,
Änderungen an `subghz_gen_info.c`/`subghz_scene_set_type.c`/`subghz_custom_event.h`) separat
portieren. Aus demselben Grund habe ich auch die „Advanced Add Manually"-Erweiterung für Monarch
selbst (neuer Menüpunkt, der nach dem Diskriminator statt nach einem generischen „Seed" fragt,
`GenKeeloqSeed`/`subghz_scene_set_seed.c`) nicht mit übernommen — das ist reine UI für das manuelle
Erzeugen eines Signals und für die hier gelieferte Empfangs-/Replay-Korrektur nicht notwendig,
lässt sich aber bei Bedarf separat nachziehen (mein bisheriger „Monarch"-Menüpunkt erzeugt
weiterhin `0x100`, wie zuvor).

## Verifikation (Host, ohne Hardware)
`firmware-mining/repro-keeloq-monarch-discriminator-fix/repro.c` — die vier geänderten
Funktionsteile (generischer Alt-Check, neue Monarch-Prüffunktion, Alt/Neu-`gen_data`-Zweig,
Alt/Neu-Serialize-Bedingung) wörtlich aus dem Patch übernommen, ohne die eigentliche
KeeLoq-Blockchiffre nachzubilden — der Fund sitzt vollständig in der Bit-Interpretation nach dem
Entschlüsseln (`decrypt`-Wert: Bit 28-31 Taste, Bit 16-27 Diskriminator/Seriennummer, Bit 0-15
Zähler), nicht in der Chiffre selbst. Unter `gcc -std=c11 -O1 -Wall -Wextra
-fsanitize=address,undefined`, 5391 von 5391 Prüfungen bestanden, keine ASan-/UBSan-Meldungen:

1. **Erkennungs-Sweep** über alle 16 Tastenwerte, mehrere Zähler- und `end_serial`-Werte für
   beide echten Diskriminatoren (`0x100`, `0x280`): Alt erkennt `0x100` ausschließlich über den
   eigentlich unabhängigen „Low-Byte-ist-0"-Rückfallzweig (in allen 1792 Fällen, unabhängig von
   `end_serial` — reiner Zufallstreffer), verwirft `0x280` dagegen in allen 1792 Fällen. Neu
   erkennt beide Werte in allen 1792 Fällen korrekt und unabhängig von `end_serial`, und
   speichert den erkannten Wert in `seed`.
2. **Neukodierungs-Rundlauf**: nach Empfang eines `0x280`-Signals erzeugt Alt beim Neusenden
   fälschlich `0x100` (passt nicht zum sendenden Handsender), Neu reproduziert korrekt `0x280`.
3. **Speichern/Laden-Rundlauf**: Alts Seed-Speicherbedingung lässt Monarchs Diskriminator beim
   Speichern einer `.sub`-Datei fallen — nach dem Neuladen fällt die Neukodierung auf `0x100`
   zurück, unabhängig vom ursprünglich empfangenen Wert. Neu speichert und lädt den Diskriminator
   wie bei BFT korrekt.
4. **Regression**: BFTs eigene Speicherbedingung bleibt durch die Erweiterung unverändert wahr;
   ein noch nie gesetzter oder ein unbekannter `seed`-Wert fällt in `gen_data()` weiterhin auf
   `0x100` zurück (identisch zum bisherigen, hartkodierten Alt-Verhalten) — deckt sowohl den Fall
   „noch kein Monarch-Signal empfangen" als auch einen dritten, noch unbekannten
   Diskriminator-Wert ab.

Eine Verifikation gegen einen echten Monarch-Handsender-Mitschnitt (insbesondere einen mit
Diskriminator `0x280`) liegt mir nicht vor und ist nur lokal an Hardware möglich.

## Risiko
- Niedrig: vier eng zusammengehörige, additive Änderungen an genau einem bereits vorhandenen
  Hersteller-Zweig in einer einzigen Datei. Kein anderer Hersteller-Zweig (Centurion, BFT,
  Dea_Mio, generischer Fall, …) wird berührt — durch den host-seitig geprüften
  Regressionsfall für BFT sowie dadurch abgesichert, dass alle Änderungen ausschließlich
  innerhalb `strcmp(..., "Monarch")`-Bedingungen bzw. dem bereits Monarch-spezifischen
  `gen_data()`-Zweig liegen.
- Kein neues Schlüsselmaterial nötig — Monarch ist bei mir bereits ein unterstützter Hersteller,
  diese Änderung korrigiert nur die Bit-Interpretation nach der bestehenden Entschlüsselung.
- Realistischer Fehlerfall bei einem Transkriptionsfehler wäre eine falsche
  Diskriminator-Erkennung/-Anzeige, kein Speicherfehler (alle Operationen sind Bitmasken/-shifts
  auf festen `uint16_t`/`uint32_t`-Werten, keine Puffer, kein Overflow-Potential) — durch den
  vollständigen Sweep über alle Tasten-/Zähler-/`end_serial`-Kombinationen für beide bekannten
  Werte host-seitig ausgeschlossen.
- Build (`fbt`/`ufbt`) deckt jede verbleibende Signaturabweichung sofort als Compile-Fehler auf.
- Funktionale Prüfung nur mit einem echten Monarch-Handsender möglich (lokal, nicht Teil dieses
  Repos) — insbesondere ein Exemplar mit Diskriminator `0x280`, um den eigentlichen Fund zu
  bestätigen; ein `0x100`-Handsender funktionierte schon vorher zufällig.

## Port-Skizze
1. `firmware-mining/keeloq-monarch-discriminator-fix/keeloq_c.patch` gegen die unveränderte
   `lib/subghz/protocols/keeloq.c` anwenden (`patch -p1 < keeloq_c.patch` aus dem Firmware-Root,
   vorab mit `--dry-run` geprüft).
2. Build: `ufbt` (bzw. `fbt firmware_all`) — reine Bitmasken-/Vergleichsänderung an bestehenden
   Funktionssignaturen, sollte ohne weitere Anpassungen durchlaufen.
3. Funktionstest lokal:
   - Regression zuerst: einen vorhandenen Monarch-Mitschnitt (falls vorhanden, Diskriminator
     `0x100`) erneut einlesen — muss weiterhin als „Monarch" erkannt werden, Taste/Zähler
     unverändert.
   - Eigentlicher Fund: einen Monarch-Handsender mit dem zweiten Diskriminator (laut Unleashed
     u. a. Dorcas 22366CLN / D-22366CLN) empfangen — muss jetzt als „Monarch" erkannt werden statt
     „Unknown".
   - Speichern/Laden: das empfangene Signal als `.sub` speichern, Flipper-Ordner neu laden, erneut
     senden (bzw. mit inkrementiertem Zähler neu erzeugen) — der neu gesendete Diskriminator muss
     mit dem ursprünglich empfangenen übereinstimmen.
   - Nur an eigener/vertraglich freigegebener Toranlage bzw. mit eigenem Handsender testen.

## Quelle
- Unleashed-Firmware, `dev`-Branch, Commit `37e7db36f6912c8c4133f2d02c27a1af624dd15b`
  ("sub hertz", 18.09.2026, API 88.11): `lib/subghz/protocols/keeloq.c` (Monarch-Teil),
  `CHANGELOG.md`, `documentation/SubGHzSupportedSystems.md`.
- Momentum-Firmware, Tag `mntm-012`: `lib/subghz/protocols/keeloq.c` (Ausgangszustand — Monarch
  bereits vorhanden, aber mit hartkodiertem `0x100` und ohne eigenen Prüfzweig).
