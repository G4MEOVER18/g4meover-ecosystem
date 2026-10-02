# FAAC-SLH: Genius-Fernbedienungen über ein optionales Manufacture-Dateifeld

## Was
Ich habe den in `faac-slh-decode-manufacturer-last-match-fix.md` als Folgeschritt zurückgestellten
eigentlichen Genius-Port jetzt umgesetzt: Unleasheds `dev`-Commit `178a99e5` („subghz: make genius
remotes work", @xMasterX) fügt ein optionales `Manufacture`-Textfeld in die `.sub`-Datei ein, das
beim Empfangen/Senden eines `FAAC SLH`-Rahmens gezielt gegen den Namen im Keystore matcht, statt
wie bisher blind über den Lerntyp zu gehen. Dadurch werden Genius-Fernbedienungen (KILO TX2/4 JLC,
Amigo u. a.), die denselben Rahmen wie FAAC-Empfänger verwenden, aber einen eigenen
Keystore-Herstellerschlüssel brauchen, korrekt unterscheidbar.

Der Commit ändert bei Unleashed fünf Dateien. Vier davon habe ich strukturell in meiner eigenen
`mntm-012`-Basis wiedergefunden, an anderen Pfaden (kein eigenes `plugins/add_manually/`-
Verzeichnis):
- `lib/subghz/protocols/faac_slh.c` — selber Pfad, der eigentliche Protokoll-Code.
- `applications/main/subghz/helpers/subghz_custom_event.h` (bei Unleashed ebenfalls unter
  `helpers/`) — `SetType`-Enum für die „Add Manually"-Menüs.
- `applications/main/subghz/plugins/add_manually/subghz_gen_info.c` bei Unleashed →
  bei mir `applications/main/subghz/helpers/subghz_gen_info.c`.
- `applications/main/subghz/plugins/add_manually/subghz_scene_set_type.c` bei Unleashed →
  bei mir `applications/main/subghz/scenes/subghz_scene_set_type.c`.
- `applications/main/subghz/plugins/add_manually/subghz_txrx_create_protocol_key.c` bei Unleashed →
  bei mir `applications/main/subghz/helpers/subghz_txrx_create_protocol_key.c`.

(Die fünfte vom Commit geänderte Datei, `applications/main/subghz_remote` (ein Submodul-Pointer),
sowie die reine Dokumentationsänderung `documentation/SubGHzSupportedSystems.md` sind nicht Teil
dieses Ports.)

Alle vier betroffenen Dateien habe ich gegen einen frischen Klon meiner eigenen
`G4MEOVER18/G4MEOVER-FW`-Basis geprüft: An allen vier Stellen liegt noch exakt der Vor-Genius-Stand
vor (`faac_slh.c` byte-identisch zum in `faac-slh-decode-manufacturer-last-match-fix.md`
dokumentierten Stand inklusive des dort gefundenen, hier mitbehobenen Auswahl-Bugs; die drei
„Add Manually"-Dateien byte-identisch zu Unleasheds eigenem Vor-Commit-Stand). `faac_slh.h` ist bei
mir bereits weiter als Unleasheds damaliger Stand (`subghz_protocol_encoder/decoder_faac_slh_free()`
existieren bei mir schon als eigene Funktionen statt der generischen `_common_free()`) — der Teil
des Commits, der genau das nachzieht, ist bei mir bereits erledigt und entfällt.

### Der mitbehobene Auswahl-Bug
`subghz_protocol_faac_slh_gen_data()` (Senden) und `subghz_protocol_faac_slh_check_remote_controller()`
(Empfangen/Anzeigen/erneutes Senden) iterieren beide unverändert über
`*subghz_keystore_get_data(keystore)`. Der Sendezweig bricht beim ersten Namenstreffer korrekt ab;
der Empfangszweig (der bereits in `faac-slh-decode-manufacturer-last-match-fix.md` dokumentierte,
unabhängig gefundene Bug) hat kein äußeres `break` und matcht zusätzlich gar nicht gegen einen
gewünschten Namen — bei mehr als einem `KEELOQ_LEARNING_FAAC`-Eintrag im Keystore gewinnt
ausschließlich der letzte in Keystore-Reihenfolge. Der neue Helfer
`subghz_protocol_faac_slh_get_key()` ersetzt beide Schleifen durch eine einzige, namensbasierte
Suche (sofortiger Rückgabe beim Treffer, siehe Port-Skizze) und behebt den Bug dabei strukturell
mit — **dieser Port ersetzt damit den dort vorgeschlagenen minimalen Fix, beide Patches dürfen
nicht gemeinsam angewendet werden** (siehe Risiko).

## Nutzen
- Schaltet Genius-Fernbedienungen (KILO TX2/4 JLC, Amigo u. a.) für Empfang, Anzeige und erneutes
  Senden frei, sofern mein Keystore einen entsprechenden Eintrag enthält oder künftig bekommt
  (siehe Risiko zur Unsicherheit darüber) — bislang würde jede Genius-Erzeugung/-Erkennung auf dem
  FAAC-Schlüssel landen oder (nach dem reinen Auswahl-Fix allein) auf dem jeweils ersten
  `KEELOQ_LEARNING_FAAC`-Eintrag, nie gezielt auf Genius.
- Macht die Schlüsselauswahl erstmals wirklich **verifizierbar** statt nur deterministisch: Mit dem
  `Manufacture`-Dateifeld weiß die Firmware, welcher Hersteller gemeint ist, statt nur „der erste
  Eintrag" zu raten (die Einschränkung aus `faac-slh-decode-manufacturer-last-match-fix.md`, dass
  echte Mehrdeutigkeits-Auflösung ohne dieses Feld nicht möglich ist, entfällt für neu
  gespeicherte/erzeugte Dateien).
- Zwei neue „Add Manually"-Menüeinträge („Genius SLH 433MHz", „Genius SLH 868MHz"), symmetrisch zu
  den bestehenden FAAC-SLH-Einträgen, in beiden Untermenüs (einfach generieren und das erweiterte
  Formular mit eigenen Serial-/Button-/Counter-/Seed-Werten) nutzbar, ohne dass die
  Menü-/Generierungslogik selbst (`subghz_scene_set_type.c`) irgendeine Sonderbehandlung für den
  neuen `GenFaacSLH`-Fall braucht — sie hängt bereits vollständig generisch an `gen_info.faac_slh.manuf`.
- Rückwärtskompatibel: Jede vor diesem Port geschriebene `.sub`-Datei hat kein `Manufacture`-Feld,
  `flipper_format_read_string()` schlägt dann fehl, der Code fällt auf den bisherigen
  `FAAC_SLH`-Namen zurück — exakt das bisherige Verhalten (durch Host-Repro Szenario 1 bestätigt).

## Betroffene Dateien (lokal)
- `lib/subghz/protocols/faac_slh.c` — Kern des Ports:
  - `struct SubGhzProtocolDecoderFaacSLH`/`struct SubGhzProtocolEncoderFaacSLH`: neues Feld
    `FuriString* manufacture_from_file`.
  - Neuer statischer Helfer `subghz_protocol_faac_slh_get_key()` (Keystore, gewünschter Name →
    Schlüssel + tatsächlich benutzter Name; leerer/fehlender Wunsch fällt auf `"FAAC_SLH"` zurück,
    ein nicht gefundener Wunschname fällt ebenfalls auf einen vorhandenen `FAAC_SLH`-Eintrag
    zurück statt auf Schlüssel 0).
  - `subghz_protocol_faac_slh_gen_data()` (Senden): bisherige Namens-`strcmp`-Schleife durch einen
    `get_key()`-Aufruf ersetzt (nur Umbau, Verhalten für einen vorhandenen Treffer identisch).
  - `subghz_protocol_faac_slh_check_remote_controller()` (Empfangen/Anzeigen/Serialisieren): neuer
    Parameter `manufacture_from_file`, Ersatz der fehlerhaften Schleife durch `get_key()` — behebt
    den in `faac-slh-decode-manufacturer-last-match-fix.md` dokumentierten Bug strukturell mit.
  - Alle vier Aufrufer der Funktion (`encoder_deserialize`, `decoder_serialize`,
    `decoder_deserialize`, `decoder_get_string`) lesen vorab optional das `Manufacture`-Feld aus
    der Datei (Rewind davor/danach, wie bei den bereits vorhandenen `Seed`/`AllowZeroSeed`-Feldern)
    und reichen es durch; `decoder_serialize` schreibt den Namen zurück in die Datei, aber nur wenn
    er bereits aus der Datei kam (kein Raten bei frisch empfangenen Signalen, siehe Risiko).
  - `decoder_get_string()`: zusätzliche `display_name`-Auswahl — zeigt bei einem Nicht-FAAC_SLH-
    Treffer (z. B. „Genius") den Herstellernamen statt des Protokollnamens „Faac SLH".
  - `encoder_alloc`/`decoder_alloc`: `manufacture_from_file` wird allokiert; `encoder_free`/
    `decoder_free` (bei mir bereits eigenständige Funktionen, siehe oben) geben es wieder frei.
- `applications/main/subghz/helpers/subghz_custom_event.h` — zwei neue `SetType`-Enum-Werte
  (`SetTypeGeniusSLH_433`, `SetTypeGeniusSLH_868`) direkt nach den bestehenden FAAC-SLH-Einträgen.
- `applications/main/subghz/helpers/subghz_gen_info.c` — zwei neue `case`-Zweige in
  `subghz_scene_set_type_fill_generation_infos()`, identisch zu den FAAC-SLH-Zweigen bis auf
  `.faac_slh.manuf = "Genius"`.
- `applications/main/subghz/scenes/subghz_scene_set_type.c` — zwei neue Einträge im
  `submenu_names[]`-Array („Genius SLH 433MHz"/„Genius SLH 868MHz").
- `applications/main/subghz/helpers/subghz_txrx_create_protocol_key.c` —
  `subghz_txrx_gen_faac_slh_protocol()`: eine fehlende Zeile ergänzt, die den übergebenen
  `manufacture_name` als `Manufacture`-Feld in die neu erzeugte Datei schreibt (ohne diese Zeile
  würde eine über „Add Manually" erzeugte Genius-Datei beim nächsten Laden wieder als FAAC_SLH
  interpretiert).

## Risiko
- **Nicht gemeinsam mit `faac-slh-decode-manufacturer-last-match-fix/faac_slh_c.patch` anwenden.**
  Beide Patches ändern dieselbe Schleife in `check_remote_controller()` auf unterschiedliche Weise
  (`git apply --check` des älteren, kleineren Fixes schlägt nach Anwendung dieses Ports
  erwartungsgemäß fehl, da der Kontext — die alte Schleife — nicht mehr existiert; lokal verifiziert).
  Dieser Port ist der vollständigere Ersatz für den dortigen Fund.
- Ob mein eigener Keystore (`applications/main/subghz/resources/subghz/assets/keeloq_mfcodes`,
  verschlüsselt) bereits einen „Genius"-Eintrag vom Typ `KEELOQ_LEARNING_FAAC` enthält, kann ich
  weiterhin nicht aus dem Quelltext feststellen (siehe bereits in
  `faac-slh-decode-manufacturer-last-match-fix.md` benannte Einschränkung). Fehlt der Eintrag,
  bleiben die beiden neuen Menüpunkte nutzbar, aber wirkungslos: `get_key()` fällt dann im
  `wanted == "Genius"`-Fall sauber auf den vorhandenen `FAAC_SLH`-Schlüssel zurück (Host-Repro
  Szenario 4) — kein Absturz, keine falsche Herstellerzuordnung, nur kein tatsächlicher
  Genius-Schlüssel. Lokal zu klären (z. B. Debug-Ausgabe der geladenen Keystore-Namen beim
  Firmware-Start).
- `encoder_deserialize()`/`decoder_serialize()`/`decoder_deserialize()` nutzen `flipper_format_rewind()`
  je zweimal zusätzlich (vor und nach dem optionalen `Manufacture`-Lesen) — exakt das bereits im
  Code etablierte Muster für optionale Felder (siehe `Seed`/`AllowZeroSeed` in derselben Funktion),
  kein neues Fehlerverhalten.
- `decoder_serialize()` schreibt das `Manufacture`-Feld bewusst nur zurück, wenn es bereits aus der
  Quelldatei stammte (`!furi_string_empty(instance->manufacture_from_file)`), nicht wenn es aus dem
  gerade erst entschlüsselten Signal „erraten" wurde — ein live empfangenes FAAC-Signal ohne
  vorheriges `Manufacture`-Feld bleibt also unverändert als namenloses FAAC_SLH gespeichert, auch
  wenn der Empfangscode zufällig auf einen Genius-Schlüssel getroffen wäre. Das entspricht exakt
  Unleasheds eigener Begründung im Originalcode (kein Verifikationsmechanismus im Rahmen selbst,
  siehe bereits in `faac-slh-decode-manufacturer-last-match-fix.md` erläutert) und ist bewusst so
  übernommen.
- Host-Repro deckt ausschließlich die Auswahl-/Fallback-Logik von `get_key()` sowie die
  `display_name`-Auswahl ab (acht Szenarien, 21 Einzelprüfungen), nicht die eigentliche
  KeeLoq-FAAC-Kryptofunktion (bereits in früheren Sitzungen unabhängig geprüft), nicht das
  `FlipperFormat`-Dateiformat selbst (`flipper_format_read_string`/`_rewind`/`_write_string_cstr`
  sind etablierte, an anderer Stelle im selben Code bereits genutzte APIs) und nicht das Laden der
  echten, verschlüsselten Keystore-Datei.
- Patches gegen `git apply --check` vorab gegen eine frische, unabhängige Kopie meiner eigenen
  `G4MEOVER18/G4MEOVER-FW`-Basis verifiziert, real angewendet und byte-für-byte mit der
  beabsichtigten Endfassung abgeglichen — nicht gegen die ESP32-/STM32WB55-Toolchain kompiliert.

## Port-Skizze
1. Reihenfolge beim Anwenden unerheblich (die fünf Patches betreffen getrennte Dateien bzw.,
   im Fall von `faac_slh.c`, nicht überlappende Stellen derselben Datei), aber alle fünf gehören
   zusammen — Teilanwendung lässt Menüeinträge ohne wirksamen Protokoll-Code zurück oder umgekehrt:
   - `faac-slh-genius-manufacture-field-port/subghz_custom_event_h.patch`
   - `faac-slh-genius-manufacture-field-port/subghz_gen_info_c.patch`
   - `faac-slh-genius-manufacture-field-port/subghz_scene_set_type_c.patch`
   - `faac-slh-genius-manufacture-field-port/subghz_txrx_create_protocol_key_c.patch`
   - `faac-slh-genius-manufacture-field-port/faac_slh_c.patch`
2. **Falls `faac-slh-decode-manufacturer-last-match-fix/faac_slh_c.patch` bereits angewendet
   wurde:** vor diesem Port wieder rückgängig machen (`git apply -R`) oder stattdessen direkt von
   einer unveränderten `faac_slh.c` ausgehen — siehe Risiko.
3. Build: `ufbt` (bzw. `fbt firmware_all`). Neue `FuriString`-Nutzung, `flipper_format_read_string`/
   `_write_string_cstr` sind im restlichen SubGHz-Code bereits etabliert, kein neuer Include nötig.
4. Funktionstest lokal, dreiteilig:
   - **Regression (ohne echten Genius-Eintrag testbar):** ein vorhandenes `FAAC_SLH`-Capture ohne
     `Manufacture`-Feld einlesen/erneut senden — Anzeige, Zähler und Herstellername müssen exakt
     wie vor dem Port bleiben.
   - **Add Manually:** „Genius SLH 433MHz" bzw. 868MHz im Add-Manually-Menü wählen, Datei
     generieren — die erzeugte `.sub`-Datei muss ein `Manufacture: Genius`-Feld enthalten
     (per Dateibrowser/USB-Export prüfbar), erneutes Laden dieser Datei muss „Genius" statt
     „Faac SLH" anzeigen.
   - **Echter Genius-Empfang:** nur möglich, falls mein Keystore tatsächlich einen
     `KEELOQ_LEARNING_FAAC`-Eintrag „Genius" enthält (siehe Risiko) — eine echte Genius-
     Fernbedienung aufzeichnen, Anzeige muss „Genius" statt „Faac SLH" zeigen, erneutes Senden
     muss am Zielempfänger funktionieren.
5. Host-Repro deckt bereits vorab ab: `firmware-mining/repro-faac-slh-genius-manufacture-field/repro.c`,
   unter `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined` bei `-O0` und `-O1` ausgeführt,
   21 Einzelprüfungen, ALLE CHECKS BESTANDEN, keine ASan-/UBSan-Meldungen, keine
   Compiler-Warnungen.

## Quelle
- `DarkFlippers/unleashed-firmware`, `dev`-Branch, Commit `178a99e5d6acecf32e6831aa8ca8b7daf08b6ae2`
  (@xMasterX): „subghz: make genius remotes work".
- `Next-Flip/Momentum-Firmware`, Tag `mntm-012`, sowie meine eigene `G4MEOVER18/G4MEOVER-FW`-Basis
  (identisch an allen fünf betroffenen Dateien) als Vergleichs-/Patch-Basis.
- `faac-slh-decode-manufacturer-last-match-fix.md` — der hier mitbehobene, unabhängig gefundene
  Auswahl-Bug und der dort bereits als Folgeschritt benannte Rückstellpunkt für diesen Port.
