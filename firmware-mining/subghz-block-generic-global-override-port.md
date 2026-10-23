# SubGhzBlockGenericGlobal: fehlender Rolling-Code-Counter/Button-Override- und Endless-TX-Mechanismus

## Was

Ich habe `lib/subghz/` zwischen meiner eigenen Basis (`G4MEOVER18/G4MEOVER-FW`, `main`, Commit
`b359f6fc32f282ce7217dd2d48a4e3478771852e`, Momentum `mntm-012`) und Unleashed
(`DarkFlippers/unleashed-firmware`, Standard-Branch, Commit
`f31555e4610a74af609c1cc7c3e9db999499fe1d`, 2026-10-22) auf Protokoll- und Blockebene
gegenübergestellt, um einen echten Lückenfund statt einer Wiederholung eines bereits
mehrfach geprüften Clusters (SubGHz-Rolling-Code-Protokolle, LFRFID, NFC — siehe die bereits
vorhandenen Einträge in diesem Verzeichnis) zu liefern. Der Dateinamen-Abgleich von
`lib/subghz/protocols/*` und `lib/lfrfid/protocols/*` ergab keine neue Protokolldatei — jedes
Rolling-Code-Format, das Unleashed führt, ist bei mir bereits vorhanden (teils über frühere
eigene Portierungen, siehe `jarolift-keeloq-subghz-port.md`, `prastel-42bit-rolling-code-port.md`
u. a.). Der Unterschied liegt tiefer: in `lib/subghz/blocks/generic.h`/`.c` fehlt bei mir eine
komplette, protokollübergreifende Infrastrukturschicht — `SubGhzBlockGenericGlobal`.

Diese Struktur plus vier Funktionen (`subghz_block_generic_global_counter_override_set/get`,
`_button_override_set/get`, `_reset`) ist der Mechanismus, über den die SubGhz-Transmitter-App
(Edit-Menü) bei ~15 Rolling-Code-Protokollen (KeeLoq, FAAC SLH, Secplus v1/v2, Alutech AT-4N,
Beninca ARC, Jarolift, Prastel, Ditec GOL4, Phoenix V2, Nice FloR-S, CAME Atomo, KingGates
Stylo4K, Somfy Keytis/Telis) den Zähler- und Tastenwert eines bereits erfassten Signals vor dem
Senden gezielt überschreiben kann, plus ein `endless_tx`-Flag, das die Encoder-"Yield"-Funktion
anweist, die Wiederholzahl *nicht* herunterzuzählen, solange OK gehalten wird (Dauersenden statt
fester Wiederholanzahl). Beides sind bei mir bislang komplett fehlende Fähigkeiten — nicht
kaputt, sondern schlicht nie portiert, weil sie an keiner einzelnen Protokolldatei hängen,
sondern an einer gemeinsamen Infrastrukturdatei, die beim reinen Dateilisten-Abgleich zwischen
den Protokollverzeichnissen nicht auffällt.

Gefunden habe ich die Lücke beim Prüfen eines weiteren, eigenständigen Kandidaten: der
Unleashed-App **SubGhz Remote** (`DarkFlippers/SubGHz_Remote`, Branch `ufw_main_app`, als
Git-Submodul unter `applications/main/subghz_remote` eingebunden, Commit
`df6334cdde0c88e22ab09ba16b12222cfccf834a`, 2026-10-18) — eine Fernbedienungs-App, die bis zu
fünf vorab erfasste `.sub`-Dateien auf die vier Richtungstasten plus OK legt und darüber schnell
abfeuert, ohne jedes Mal durch das Dateisystem zu navigieren (für ein Pentest-Cockpit mit
mehreren immer wieder benötigten Signalen — z. B. Tor/Garage mehrerer autorisiert getesteter
Objekte — ein klarer Gewinn). Ihr `subrem_tx_start_sub()`/`subrem_tx_stop_sub()`
(`subghz_remote_app_i.c`) nutzt genau `subghz_block_generic_global`/`subghz_custom_btn_*()`, um
beim Senden den ursprünglichen Tastenwert zu überschreiben und den Dauersende-Modus zu steuern.
Beim Abgleich, welche der benutzten Symbole bei mir bereits vorhanden sind, stellte sich heraus:
`subghz_custom_btn_*()` (eigene, unabhängige API, `lib/subghz/blocks/custom_btn.c/.h`) ist bei
mir bereits **byte-identisch** vorhanden — nur `subghz_block_generic_global` fehlt komplett. Die
App selbst (3440 Zeilen über 20 Dateien: Szenen, Views, Map-Datei-Format, eigene
`subghz_txrx`-Kopie) ist ein eigener, deutlich größerer Port und bewusst **nicht** Teil dieser
Sitzung — diese Sitzung liefert nur die hier gefundene Infrastrukturlücke, die eine notwendige
Voraussetzung für einen künftigen SubGhz-Remote-Port ist (siehe "Port-Skizze").

Ich liefere additiv: die vollständige `SubGhzBlockGenericGlobal`-Struktur plus die vier
Funktionen aus Unleashed, eingefügt in meine eigenen `generic.h`/`.c`, **ohne** meine eigene,
unabhängige Erweiterung dieser Dateien anzutasten (siehe "Risiko" — meine Basis hat hier bereits
eine eigene, bei Unleashed nicht vorhandene Funktion). Der Mechanismus selbst wird in dieser
Sitzung bewusst **nicht** an ein einzelnes Protokoll angeschlossen (kein Protokoll ruft die neuen
Funktionen auf) — das bleibt Phase 2.

## Nutzen

- **Rolling-Code-Zähler-/Tasten-Editor:** Unleasheds SubGhz-Transmitter-App nutzt diesen
  Mechanismus für ein Edit-Menü ("Counter"/"Button"), mit dem man bei einem bereits erfassten
  Rolling-Code-Signal (eigenes, autorisiert getestetes Gerät) den Zähler- oder Tastenwert vor dem
  erneuten Senden gezielt setzt, statt nur den unveränderten Originalwert erneut abzufeuern. Für
  autorisierte Tests von Rolling-Code-Resync-Fenstern (wie tolerant akzeptiert ein Empfänger einen
  Zähler-Sprung?) oder zum gezielten Reproduzieren eines bestimmten, bereits bekannten
  Zählerstands ist das ein direktes Test-Primitiv, das mir bislang fehlt.
- **Dauersenden (`endless_tx`):** Ersetzt "N feste Wiederholungen" durch "senden solange OK
  gehalten wird" — bei den eigenen Garagentor-/Torantrieb-Protokollen (Hörmann, Marantec, FAAC,
  Nice u. a., bei mir alle bereits vorhanden) oft die näher am Originalverhalten liegende
  Bedienung, und die direkte Voraussetzung für jede Fernbedienungs-App mit "Taste halten = senden"
  (wie SubGhz Remote).
- Minimal-invasiv: reine Ergänzung, kein bestehender Aufrufer betroffen (siehe "Risiko") — die
  vier neuen Funktionen werden von nichts in meiner Basis aufgerufen, bis ich sie in Phase 2 an
  ein Protokoll anschließe.
- Low-Preis: das Kernstück ist rein additiv, zwei kurze Patches (+50/+51 Zeilen) plus ein
  Dreizeiler-Hook — kein mehrteiliger Umbau wie Texkom, keine neue `furi_hal`-Schicht.

## Betroffene Dateien

**Jetzt geliefert:**
- `firmware-mining/subghz-block-generic-global-override-port/lib/subghz/blocks/generic.h` —
  meine eigene `generic.h`, additiv erweitert um `SubGhzBlockGenericGlobal` (Typedef, Struct,
  `extern`-Deklaration) plus die fünf Funktionsdeklarationen
  (`_counter_override_set/get`, `_button_override_set/get`, `_reset`). Fertige Zieldatei zum
  direkten Ersetzen meiner vorhandenen `lib/subghz/blocks/generic.h`.
- `firmware-mining/subghz-block-generic-global-override-port/lib/subghz/blocks/generic.c` —
  dieselbe additive Erweiterung für die Implementierung, direkt nach der `TAG`-Definition
  eingefügt. Fertige Zieldatei zum direkten Ersetzen.
- `firmware-mining/subghz-block-generic-global-override-port/connective-patches/generic_h.patch`
  und `generic_c.patch` — dieselbe Ergänzung als Patch gegen eine unveränderte
  `G4MEOVER18/G4MEOVER-FW`-Basis, für den Fall, dass die eigene lokale Kopie seit
  `b359f6fc32f282ce7217dd2d48a4e3478771852e` bereits eigene Änderungen an diesen Dateien hat (dann
  Patch statt Ersetzen verwenden).
- `firmware-mining/subghz-block-generic-global-override-port/connective-patches/base_c.patch` —
  der einzige Anschlusspunkt an eine dritte, bereits vorhandene Datei:
  `lib/subghz/protocols/base.c`, Funktion `subghz_protocol_decoder_base_get_string()`. Fügt einen
  `#include "../blocks/generic.h"` und einen Aufruf `subghz_block_generic_global_reset(0);` direkt
  nach den beiden `furi_check()`-Zeilen ein — exakt Unleasheds eigene Anbindung, byte-für-byte
  übernommen (siehe "Verifikation"). Das ist der einzige Ort, an dem der neue Mechanismus in
  dieser Sitzung überhaupt "live" geschaltet wird: jedes Mal, wenn ein Decoder seinen
  Anzeige-Text erzeugt (z. B. beim Anzeigen eines empfangenen/geladenen Signals), wird der globale
  Zustand zurückgesetzt (außer `endless_tx`) — noch ohne dass irgendein Protokoll die
  Override-Werte liest oder setzt.
- `firmware-mining/repro-subghz-block-generic-global-override-port/` — Host-Repro (siehe
  "Verifikation").

**Jetzt geliefert (Phase 2, KeeLoq-Pilotprotokoll):**
- `firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/keeloq.c` —
  meine eigene `keeloq.c` (Basis `G4MEOVER18/G4MEOVER-FW`, `b359f6fc32f282ce7217dd2d48a4e3478771852e`,
  dieselbe wie Phase 1), additiv um die drei Anschlussstellen erweitert (siehe "Was" unten).
  Fertige Zieldatei zum direkten Ersetzen meiner vorhandenen `lib/subghz/protocols/keeloq.c`.
- `firmware-mining/subghz-block-generic-global-override-port/connective-patches/keeloq_c.patch`
  — dieselbe Ergänzung als Patch gegen eine unveränderte `G4MEOVER18/G4MEOVER-FW`-Basis.
- `firmware-mining/repro-subghz-block-generic-global-override-port-phase2/` — Host-Repro (siehe
  "Verifikation").

**Für Phase 3 (nicht Teil dieser Sitzung, siehe "Port-Skizze"):** die übrigen ~14 der oben
genannten Protokolldateien, die den Mechanismus bei Unleashed bereits nutzen — pro Datei
derselbe kleine, lokal begrenzte Satz Zeilen wie jetzt bei KeeLoq. Keine dieser Dateien ist in
dieser Sitzung angefasst.

### Phase 2: KeeLoq als Pilotprotokoll

**Was:** meine eigene `keeloq.c` ist **keine** 1:1-Kopie von Unleasheds aktueller `dev`-Version
(Commit `97a0174368527e7307cf6af231aa2f4e41c26f96`, zum Vergleich frisch geholt) — der volle Diff
ist über 500 Zeilen groß und umfasst u. a. Unleasheds inzwischen eingeführte
`common.c`-Encoder-/Decoder-Basisfunktionen, eine Monarch-Diskriminator-Konstante, zusätzliche
Hersteller-Sonderfälle (`Pecinin`, erweiterte `klq_last_custom_btn`-Tabelle) und einen
Modus-7-Lern-Sequenz-Pfad mit einer eigenen `bypass`-Variable, die bei mir schlicht nicht
existieren (mein `keeloq_counter_mode` kennt nur die Modi 0–6, kein Modus 7). Ein blinder
Dateiersatz durch Unleasheds Version — wie in Phase 1 für `generic.c` zunächst erwogen und dann
verworfen — hätte hier denselben Fehler wiederholt, nur gravierender (verlorene eigene
Zählermodi 1–6, verlorene `Pecinin`-Sonderbehandlung). Stattdessen: dieselben drei
Anschlusspunkte, die Unleashed für KeeLoq nutzt, an der jeweils strukturell passenden Stelle
**meiner eigenen** `gen_data()`/`get_upload()`/`get_string()` eingefügt, ohne sonst etwas an der
umgebenden Logik zu ändern:
1. `subghz_protocol_keeloq_gen_data()`, Zähler-Erhöhungsblock (`if(counter_up && prog_mode ==
   PROG_MODE_OFF)`): `subghz_block_generic_global_counter_override_get(&instance->generic.cnt)`
   als zusätzlicher, vorangestellter Zweig der bestehenden `if/else if`-Kette über die Modi 0–6 —
   bei anhängendem Override wird der neue Wert direkt verwendet, sonst läuft exakt die bisherige
   Modus-Logik unverändert weiter. Da mein Code kein Modus 7 kennt, entfällt Unleasheds
   `bypass`-Mechanismus ersatzlos — es gibt hier keinen Lern-Sequenz-Pfad, den er schützen müsste.
2. `subghz_protocol_encoder_keeloq_get_upload()`: direkt nach der bestehenden
   `btn = subghz_protocol_keeloq_get_btn_code(klq_last_custom_btn);`-Zeile (meine eigene,
   programmiermodus-abhängige Tastencode-Zuweisung, bleibt unverändert) ein
   `subghz_block_generic_global_button_override_get(&btn)`-Aufruf, der den endgültigen
   Tastenwert vor der Signalerzeugung überschreibt.
3. `subghz_protocol_decoder_keeloq_get_string()`: `btn_is_available`/`current_btn`/
   `btn_length_bit` unbedingt am Funktionsanfang gesetzt (alle drei Zweige senden dasselbe
   4-Bit-Tastenfeld); `cnt_is_available`/`cnt_length_bit`/`current_cnt` pro Zweig passend zu
   meiner eigenen (von Unleashed abweichenden) Drei-Zweig-Aufteilung: `BFT` (Zähler im
   Klartext übertragen, editierbar), `Unknown` (reines Replay ohne Zähler, nicht editierbar),
   alle anderen bekannten Hersteller (verschlüsselter Zähler, editierbar) — AN-Motors/HCS101
   fallen bei mir strukturell unter "alle anderen", da meine `gen_data()` sie anders als
   Unleashed weiterhin mit echtem Zähler statt Pseudo-Zähler kodiert.

## Risiko

- **Wichtigster eigener Fund dieser Sitzung — kein blindes Ersetzen möglich:** Ich habe zuerst
  angenommen, meine `generic.c`/`.h` seien eine reine Untermenge von Unleasheds Version (so wie
  bei den meisten bereits geprüften Protokolldateien) und einen direkten Dateiersatz vorbereitet.
  Der vollständige Diff (nicht nur die ersten Zeilen) zeigt das Gegenteil: meine Basis hat eine
  **eigene, bei Unleashed nicht vorhandene** Erweiterung — `subghz_block_generic_serialize_common()`
  plus `Lat`/`Lon`-Geotagging-Felder in `SubGhzRadioPreset` (gefüttert über eine eigene
  NMEA-GPS-Anbindung, `applications/main/subghz/helpers/subghz_gps.c`/`minmea.c`). Diese
  Hilfsfunktion wird von vier bereits vorhandenen Protokolldateien aufgerufen
  (`pcsg_generic.c`, `tpms_generic.c`, `ws_generic.c`, `bin_raw.c`) — ein blinder Ersatz durch
  Unleasheds `generic.c` hätte das GPS-Tagging und alle vier Aufrufer kaputt gemacht. Deshalb ist
  die gelieferte Datei kein Dateiersatz aus Unleashed, sondern eine **additive Zusammenführung**:
  Unleasheds vier Override-Funktionen plus Struct sind eingefügt, meine eigene
  `serialize_common()`/Lat-Lon-Erweiterung bleibt unverändert (siehe "Verifikation" für den
  Beleg, dass beide Teile nebeneinander bestehen).
- **Undefiniertes Verhalten bei `length_bit == 0` (dokumentiert, nach heutigem Code nicht
  erreichbar):** Die Masken-Formel in `_counter_override_get()` —
  `0xFFFFFFFF >> (32 - cnt_length_bit)` — ist für `cnt_length_bit == 0` ein Schiebe-Shift um 32
  Bit auf einem 32-Bit-Typ, laut C-Standard undefiniertes Verhalten. Per UBSan am eigenen
  Host-Repro bestätigt (siehe "Verifikation", `ub_check.c`): der Aufruf crasht mit "shift exponent
  32 is too large". Jeder der ~15 echten Aufrufer bei Unleashed setzt `cnt_length_bit`/
  `btn_length_bit` jedoch unbedingt am Ende seiner `get_string()`-Funktion, bevor die
  `_override_get()`-Aufrufe im Encoder (auf einem späteren Sendeweg) überhaupt erreicht werden
  können — nach heutigem Code also kein erreichbarer Pfad, aber ein implizites, nirgends
  dokumentiertes Vertragsverhältnis zwischen "Länge setzen" und "Override abfragen". Für Phase 2
  heißt das: jedes neu angeschlossene Protokoll muss `cnt_length_bit`/`btn_length_bit` *vor* dem
  ersten `_override_get()`-Aufruf setzen — sonst dieselbe UB. Sollte als Kommentar/Prüfung bei
  jeder Phase-2-Anbindung mitgeführt werden.
- **`base.c`-Hook ist der einzige Ort, an dem sich das Verhalten *jetzt schon* minimal ändert:**
  `subghz_block_generic_global_reset(0)` läuft neu bei jedem `get_string()`-Aufruf. Da kein
  Protokoll die Override-Felder vor Phase 2 liest oder setzt, ist der Reset reine Nullung von
  Feldern, die sonst nie gelesen werden — faktisch wirkungslos, aber es ist die einzige Zeile
  dieser Sitzung, die in einem bereits aktiven Aufrufpfad (statt in bislang unbenutztem Code)
  landet. Lokal nach dem Anwenden einmal `ufbt`/`fbt firmware_all` bauen und ein beliebiges
  SubGhz-Signal anzeigen lassen, um das auszuschließen.
- **Phase 2 (Protokoll-Anbindung) ist nicht Teil dieser Sitzung und separat zu bewerten:** sobald
  ein Protokoll `_override_get()` tatsächlich aufruft, ändert sich sein Sendeverhalten abhängig
  vom UI-Zustand — das ist der eigentliche Feature-Zugewinn, aber auch der Punkt, an dem ein
  Fehler beim Anschluss (z. B. `cnt_length_bit` falsch gesetzt) ein falsches Signal erzeugen
  könnte. Jede Phase-2-Anbindung sollte denselben Verifikationsstil bekommen wie dieser
  Infrastruktur-Port (Host-Repro mit den tatsächlichen Protokoll-Bitlängen).

## Verifikation (auf dem Host, ohne Hardware)

- **Herkunft/Unverändertheit der übernommenen Logik:** Die vier Override-Funktionen plus Struct
  in `generic.h`/`.c` sind Zeile für Zeile aus Unleasheds `lib/subghz/blocks/generic.c`/`.h`
  übernommen (Commit `f31555e4610a74af609c1cc7c3e9db999499fe1d`) — per drei unabhängigen,
  zeitlich getrennten `git clone`-Durchläufen gegen `DarkFlippers/unleashed-firmware` geholt
  (einer davon über eine komplett neue Arbeitskopie außerhalb des ursprünglichen Klons), alle drei
  per `md5sum` byte-identisch zueinander bestätigt (`7e948057f1fbdece0db40aa95980542c` für
  `generic.c`, `065ffa4f4c8d42baaace96753a9970e8` für `generic.h`).
- **Additiv, nicht destruktiv:** `connective-patches/generic_h.patch`, `generic_c.patch` und
  `base_c.patch` wurden einzeln und zusammen per `git apply --check` sowie real per `git apply`
  gegen eine frische, unveränderte `G4MEOVER18/G4MEOVER-FW`-Arbeitskopie
  (`b359f6fc32f282ce7217dd2d48a4e3478771852e`) angewendet: alle drei greifen, zusammen ergeben sie
  **104 Zeilen Hinzufügung, 0 Löschungen** (`git diff --stat` nach dem Anwenden). Die eigene,
  unabhängige `subghz_block_generic_serialize_common()`/Lat-Lon-Erweiterung bleibt dabei
  byte-für-byte unverändert (per gezieltem Diff vor/nach dem Patch auf genau diesen
  Funktionskörper bestätigt).
- **Funktionsverhalten (`firmware-mining/repro-subghz-block-generic-global-override-port/`):**
  `repro.c` kompiliert die tatsächliche, nach dem Merge entstandene `generic.c` (dieselbe Datei,
  die oben als fertige Zieldatei mitgeliefert wird — inklusive der eigenen Lat-Lon-Erweiterung,
  die dabei mitkompiliert, aber nie aufgerufen wird) unverändert mit, zusammen mit minimalen
  Mocks für ihre fremden Abhängigkeiten (`FuriString`, `FlipperFormat`, `Stream` — `mocks/`,
  `stubs.c`). Geprüft (101 Prüfungen):
  1. Override ist "einmal verbraucht": nach `_set()` liefert der nächste `_get()` `true` und den
     Override-Wert; ein zweiter `_get()` ohne neuen `_set()` liefert `false` und lässt die
     Ausgabevariable unverändert — sowohl für Counter als auch für Button.
  2. Die Masken-Formel ist für jeden tatsächlich bei Unleashed verwendeten Bitlängen-Wert
     geprüft (`cnt_length_bit`: 8/16/20/28/32 — KeeLoq, Secplus v1, FAAC SLH, Secplus v2,
     Beninca ARC; `btn_length_bit`: 1/2/3/4/8 — Princeton, CAME, KeeLoq/Ditec GOL4 u. a.,
     Alutech AT-4N — per `grep` gegen `lib/subghz/protocols/*.c` erhoben), jeweils mit einem
     Wert, der den vollen Bitbereich ausreizt (`0xFFFFFFFF`/`0xFF`) und einem, der bereits
     innerhalb der Länge liegt (Maske darf dann nicht verändern).
  3. `_reset()` nullt jedes Feld außer `endless_tx`, das in beide Richtungen (`true`→`true`,
     `false`→`false`) über den Reset hinweg erhalten bleibt — die eine im Quelltext kommentierte
     Ausnahme.
  4. Direkt nach einem frischen Reset meldet `_get()` für Counter wie Button korrekt "kein
     Override anhängig" — der reale Aufrufpfad über den neuen `base.c`-Hook.
  - Gebaut und ausgeführt mit `gcc -std=c17 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
    -fno-sanitize-recover=undefined`: **101 Prüfungen bestanden**, identischer Output beider
    Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde. Der Zähler
    `g_unexpected_stub_calls` (jeder Aufruf in die gemockten `FuriString`/`FlipperFormat`/
    `Stream`-Pfade) bleibt über den gesamten Testlauf bei 0 — bestätigt, dass ausschließlich die
    vier reinen Override-/Reset-Funktionen ausgeübt wurden, nicht die (weiterhin
    gemockte, weil für diese Sitzung irrelevante) Serialisierung.
  - Build-Befehl (aus `firmware-mining/repro-subghz-block-generic-global-override-port/`):
    `gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined
    -I ../subghz-block-generic-global-override-port -I mocks/lib/subghz/blocks -I mocks
    ../subghz-block-generic-global-override-port/lib/subghz/blocks/generic.c stubs.c repro.c -o repro`
- **UB-Beleg (`ub_check.c`):** separates, bewusst vertragsbrüchiges Probe-Programm (ruft
  `_counter_override_get()` auf, ohne vorher `cnt_length_bit` zu setzen) — unter
  `-fsanitize=undefined -fno-sanitize-recover=undefined` bricht der Prozess exakt wie erwartet
  mit `generic.c:23:33: runtime error: shift exponent 32 is too large for 32-bit type
  'unsigned int'` ab (Exit-Code 1). Bestätigt die in "Risiko" beschriebene, nach heutigem Code
  nicht erreichbare, aber für Phase 2 zu beachtende UB-Falle.

### Verifikation Phase 2 (KeeLoq)

- **Patch additiv, byte-identisch:** `connective-patches/keeloq_c.patch` per `git apply --check`
  sowie real per `git apply` gegen zwei frische, unabhängige Zweitkopien meiner eigenen
  `keeloq.c` (`G4MEOVER18/G4MEOVER-FW`, `b359f6fc32f282ce7217dd2d48a4e3478771852e`) angewendet —
  beide Ergebnisse per `md5sum` byte-identisch zueinander und zur mitgelieferten Zieldatei
  bestätigt.
- **Die eigene Vertragslücke aus Phase 1 tatsächlich getroffen, dann korrekt entschärft:** der
  erste Durchlauf des neuen Host-Repros (Szenario A, Override gesetzt, `cnt_length_bit` noch auf
  dem `_reset()`-Standardwert 0) brach exakt mit derselben UB ab, die `ub_check.c` in Phase 1
  demonstriert hatte (`generic.c:23:33: shift exponent 32 is too large`) — der reale Beleg, dass
  das in Phase 1 als "nach heutigem Code nicht erreichbar, aber für Phase 2 zu beachten"
  dokumentierte Vertragsverhältnis beim ersten tatsächlichen Anschluss sofort relevant wird.
  Behoben, indem jedes Testszenario `cnt_length_bit`/`btn_length_bit` zuerst setzt — genau wie es
  mein eigener, jetzt gelieferter Fragment-3-Patch (`get_string()`) vor jedem realen
  `_override_get()`-Aufruf im Encoder auch tut, da `get_string()` beim Anzeigen eines Signals
  immer vor einer eigenen Sendung läuft.
- **Funktionsverhalten
  (`firmware-mining/repro-subghz-block-generic-global-override-port-phase2/`):** `repro.c`
  kompiliert die tatsächliche, nach dem Patch entstandene `generic.c`-Gegenseite aus Phase 1
  unverändert mit; die drei neuen Anschlussstellen selbst sind nicht als vollständige `keeloq.c`
  mitkompiliert (das zöge `subghz_keystore_i.h`/`custom_btn_i.h`/`m-array.h` nach sich, die dieser
  Patch nicht berührt), sondern per `sed`-Zeilenbereich byte-identisch aus der mitgelieferten,
  bereits gepatchten `keeloq.c` in eigene `.inc`-Fragmente extrahiert und in minimalen
  Wrapper-Funktionen eingebunden (Zeilenbereiche in den Kommentaren von `repro.c`, mechanisch
  reproduzierbar). Geprüft (63 Prüfungen):
  1. Zähler-Override schneidet für jeden Modus 0–6 (inkl. beider OFEX-Zweige von Modus 0) die
     Standard-Erhöhung ab, schreibt exakt den `new_cnt`-Wert und verbraucht sich einmalig.
  2. Ein zweiter Aufruf direkt danach ohne neuen `_set()` führt die normale Modus-0-Erhöhung aus
     (`+1`), statt den verbrauchten Override erneut anzuwenden.
  3. Ohne anhängenden Override laufen alle sieben Modi (0–6) exakt wie vor dem Patch — 14
     Einzelfälle inkl. aller dokumentierten Überlauf-/Wrap-Werte (`0xFFFF`→`0`, `0x8007`→`0x0006`
     usw.).
  4. `counter_up == false` bzw. `prog_mode != PROG_MODE_OFF` verhindern weiterhin jeden
     Zähler-Zugriff, auch bei anhängendem Override (der Override-Aufruf selbst steht innerhalb
     derselben bestehenden Bedingung, nicht davor).
  5. Tasten-Override verbraucht sich einmalig wie der Zähler-Override; ohne anhängenden Override
     bleibt der übergebene Tastenwert unverändert.
  6. `get_string()`s drei Zweige (`BFT`/`Unknown`/alle anderen) setzen `cnt_is_available`
     korrekt `true`/`false`/`true` und befüllen `current_cnt`/`cnt_length_bit` nur, wenn
     verfügbar; `btn_is_available`/`current_btn`/`btn_length_bit` sind in allen drei Zweigen
     identisch gesetzt.
  - Gebaut und ausgeführt mit `gcc -std=c17 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
    -fno-sanitize-recover=undefined`: **63 Prüfungen bestanden**, identischer Output beider
    Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde,
    `g_unexpected_stub_calls` bleibt bei 0.
  - Build-Befehl (aus `firmware-mining/repro-subghz-block-generic-global-override-port-phase2/`):
    `gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined
    -I ../subghz-block-generic-global-override-port -I mocks/lib/subghz/blocks -I mocks
    ../subghz-block-generic-global-override-port/lib/subghz/blocks/generic.c stubs.c repro.c -o repro`
  - Nebenfund beim Einrichten des Builds: derselbe `-I mocks/lib/subghz/blocks`-Suchpfad-Trick wie
    in Phase 1 setzt voraus, dass `mocks/lib/subghz/blocks/` als Verzeichnis tatsächlich existiert
    (die Kompilierung löst `../types.h` relativ zu diesem `-I`-Pfad auf) — Git verfolgt aber keine
    leeren Verzeichnisse. Deshalb liegt in diesem Repro-Verzeichnis zusätzlich eine
    `PATH_ANCHOR.txt`, rein damit das Verzeichnis einen frischen Checkout überlebt; ohne sie würde
    genau derselbe Build-Befehl nach einem frischen `git clone` mit "`fatal error: ../types.h: No
    such file or directory`" fehlschlagen (hier tatsächlich reproduziert). Rein redaktioneller
    Nebenfund für den eigenen Phase-2-Ordner, keine Änderung an Phase 1.

## Port-Skizze

1. **Phase 1 (geliefert, diese Sitzung):** `generic.h`/`generic.c` additiv um
   `SubGhzBlockGenericGlobal` erweitern (Dateien oben zum direkten Ersetzen, oder die drei
   Patches anwenden), `base.c`-Hook setzen. Baut eigenständig (reine Ergänzung, host-seitig
   bestätigt), ändert das Verhalten noch nicht beobachtbar, da nichts die neuen Funktionen
   aufruft außer dem harmlosen Reset-Hook.
2. **Phase 2 (geliefert, diese Sitzung):** KeeLoq als Pilotprotokoll angeschlossen — ein
   `_button_override_get()`-Aufruf im Encoder, ein `_counter_override_get()`-Aufruf im Encoder
   (bei mir in `gen_data()` statt direkt in `get_upload()`, da meine eigene Zählerlogik dort
   sitzt), plus das Setzen von `cnt_is_available`/`cnt_length_bit`/`current_cnt`/
   `btn_is_available`/`btn_length_bit`/`current_btn` am Anfang/in den drei Zweigen von
   `get_string()`. Die in "Risiko" dokumentierte Reihenfolge-Abhängigkeit (`cnt_length_bit` vor
   dem ersten `_override_get()`) wurde dabei am eigenen Host-Repro tatsächlich ausgelöst und
   korrekt eingehalten (siehe "Verifikation Phase 2"). Kein Modus-7/`bypass`-Mechanismus nötig,
   da mein `keeloq_counter_mode` kein Modus 7 kennt.
3. **Phase 3 (künftige Sitzung):** die übrigen ~14 Protokolle nachziehen (gleiches Muster wie
   KeeLoq, siehe "Betroffene Dateien" — pro Protokoll eigener Diff gegen Unleashed nötig, da
   bereits bei KeeLoq erhebliche strukturelle Abweichungen von der aktuellen Unleashed-Version
   bestanden), dann `endless_tx` an die eigene SubGhz-Transmitter-App anschließen (Hold-OK =
   Dauersenden statt fester Wiederholzahl).
4. **Phase 4 (separater, deutlich größerer Port, nicht Teil dieser Port-Skizze):** erst ab hier
   wird ein Port der App **SubGhz Remote** (siehe "Was") sinnvoll — ihr `subrem_tx_start_sub()`
   braucht exakt den hier gelieferten Mechanismus, um den Tastenwert vor dem Senden zu
   überschreiben. Verdient eine eigene Port-Vorschlag-Datei mit eigener Port-Skizze (Map-Datei-
   Format, Szenen/Views, die 684-Zeilen-`subghz_txrx`-Kopie der App gegen meine eigene,
   unabhängig weiterentwickelte `applications/main/subghz/helpers/subghz_txrx.c` abgleichen —
   bei einem ersten Blick bereits mehrere funktionale Unterschiede sichtbar, u. a. der
   OTG-Einschaltpfad: die App nutzt einen begrenzten `furi_hal_power_enable_otg()`-Retry mit
   5 Versuchen, meine eigene Version einen einmaligen Aufruf über den `Power`-Service).
5. Bauen nach Phase 1: `ufbt` (bzw. `fbt firmware_all`) — reine Ergänzung, sollte ohne
   Compile-Fehler durchlaufen. Funktionstest: ein beliebiges SubGhz-Signal anzeigen lassen (löst
   den neuen `base.c`-Hook aus), Verhalten muss unverändert bleiben (reiner Infrastruktur-Schritt,
   noch kein sichtbares Feature). Ab Phase 2 mit Hardware testbar: KeeLoq-Signal erfassen, über
   ein künftiges Edit-Menü den Zähler ändern, erneut senden, am Zielgerät (eigenes, autorisiert
   getestetes Garagentor/Empfänger) beobachten, ob/wie der geänderte Zähler akzeptiert wird.

## Quelle

- Unleashed-Firmware, Standard-Branch, Commit `f31555e4610a74af609c1cc7c3e9db999499fe1d`
  (2026-10-22): `lib/subghz/blocks/generic.c`/`.h`, `lib/subghz/blocks/custom_btn.c`/`.h`
  (zum Abgleich, byte-identisch bei mir vorhanden), `lib/subghz/protocols/base.c`, sowie als
  Fundort/Beleg für die Nutzung des Mechanismus: `keeloq.c`, `faac_slh.c`, `secplus_v1.c`,
  `secplus_v2.c`, `alutech_at_4n.c`, `beninca_arc.c`, `jarolift.c`, `prastel.c`, `ditec_gol4.c`,
  `phoenix_v2.c`, `nice_flor_s.c`, `came_atomo.c`, `kinggates_stylo_4k.c`, `somfy_keytis.c`,
  `somfy_telis.c`, `princeton.c`, `common.c`. Dreifach per unabhängigem `git clone` geholt,
  `generic.c`/`.h` jedes Mal per `md5sum` byte-identisch bestätigt.
- `DarkFlippers/SubGHz_Remote`, Branch `ufw_main_app`, Commit
  `df6334cdde0c88e22ab09ba16b12222cfccf834a` (2026-10-18, als Git-Submodul von
  Unleashed unter `applications/main/subghz_remote` eingebunden, `.gitmodules`): Auslöser dieser
  Recherche, `subghz_remote_app_i.c` (`subrem_tx_start_sub`/`_stop_sub`) als Beleg für den
  tatsächlichen Nutzungsfall des hier portierten Mechanismus bei einer externen App; eigener Port
  dieser App bewusst nicht Teil dieser Sitzung (siehe "Port-Skizze", Phase 4).
- `G4MEOVER18/G4MEOVER-FW`, `main`, Commit `b359f6fc32f282ce7217dd2d48a4e3478771852e`
  (2026-08-18, per `git ls-remote` sowohl zu Sitzungsbeginn Phase 1 als auch erneut zu
  Sitzungsbeginn Phase 2 als unverändert aktueller `HEAD`-Stand bestätigt): eigene Basis für den
  vollständigen Diff von `lib/subghz/blocks/generic.c`/`.h`, `lib/subghz/protocols/base.c` sowie
  (Phase 2) `lib/subghz/protocols/keeloq.c`; Fundort der eigenen, unabhängigen
  `subghz_block_generic_serialize_common()`/Lat-Lon-Erweiterung
  (`applications/main/subghz/helpers/subghz_gps.c`, `minmea.c`, vier Aufrufer
  `pcsg_generic.c`/`tpms_generic.c`/`ws_generic.c`/`bin_raw.c`) sowie Ausgangspunkt für alle
  vier `connective-patches/*.patch`.
- (Phase 2) Unleashed-Firmware, Standard-Branch (`dev`), Commit
  `97a0174368527e7307cf6af231aa2f4e41c26f96` (2026-10-23, per `git ls-remote` zu
  Sitzungsbeginn als aktueller `HEAD`-Stand bestätigt): `lib/subghz/protocols/keeloq.c`/`.h` nur
  zum strukturellen Abgleich geholt (nicht als Portquelle verwendet, siehe "Phase 2: KeeLoq als
  Pilotprotokoll" — meine eigene `keeloq.c` ist seit Phase 1s Commit `f31555e4` bereits so weit
  von Unleasheds aktueller Version abgewichen, dass ein Ersatz/Diff-Port hier nicht sinnvoll war,
  nur die drei Anschlusspunkte selbst).
