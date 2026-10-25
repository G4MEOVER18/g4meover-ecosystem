# SubGhzBlockGenericGlobal: fehlender Rolling-Code-Counter/Button-Override- und Endless-TX-Mechanismus

**Update (Phase 5):** drittes Nachzieh-Protokoll (Somfy Telis, `somfy_telis`) angeschlossen —
siehe "Phase 5: Somfy Telis" sowie "Verifikation Phase 5" unten. Dafür gezielt die sechs bis
dahin noch nicht geprüften echten Kandidaten (Alutech AT-4N, Phoenix V2, CAME Atomo, KingGates
Stylo4K, Somfy Keytis, Somfy Telis) per frischem Doppel-Clone (eigene Basis plus Unleashed)
gegeneinander diffgeprüft: Somfy Telis hatte mit 231 Diff-Zeilen den kleinsten und saubersten
Gesamtdiff alle sechs Kandidaten (Phoenix V2 als nächstkleinster, aber mit einem zusätzlichen,
KeeLoq-Modus-7-artigen `cnt_need_override`/`bypass`-Mechanismus strukturell aufwändiger;
Alutech AT-4N/CAME Atomo/KingGates Stylo4K/Somfy Keytis alle über 400 Diff-Zeilen) und folgt
exakt demselben OFEX-Zähler-Muster, das bei Secplus v1 in Phase 4 bereits erfolgreich
angeschlossen wurde. Damit sind von den ursprünglich gezählten ~15 Protokollen weiterhin vier
(Beninca ARC, Jarolift, Prastel, Ditec GOL4) ohne eigenständige Protokolldatei (siehe
"Update (Phase 4)" unten, per erneutem `find`/`ls` gegen den frischen Phase-5-Checkout
bestätigt unverändert), es bleiben 11 echte Kandidaten. Davon sind KeeLoq (Phase 2), Secplus v1
(Phase 4) und jetzt Somfy Telis (Phase 5) angeschlossen — macht 3 von 11, 8 offen (FAAC SLH,
Secplus v2, Alutech AT-4N, Phoenix V2, Nice FloR-S, CAME Atomo, KingGates Stylo4K, Somfy
Keytis). Princeton (Phase 3) kommt als zusätzlicher, nicht in der ursprünglichen Liste
gezählter Fund hinzu — macht insgesamt 4 angeschlossene Protokolle. Siehe "Port-Skizze" für
den aktualisierten Diff-Größen-Überblick der verbleibenden 8.

**Update (Phase 4):** zweites Nachzieh-Protokoll (Security+ 1.0, `secplus_v1`) angeschlossen —
siehe "Phase 4: Security+ 1.0" sowie "Verifikation Phase 4" unten. Außerdem die bereits in
Phase 3 festgestellte, im letzten Arbeits-Log noch als offen vermerkte Beobachtung mit einem
komplett frischen `G4MEOVER-FW`-Clone erneut bestätigt: die vier in der ursprünglichen
~15-Protokoll-Liste genannten Dateien `beninca_arc.c`, `jarolift.c`, `prastel.c`,
`ditec_gol4.c` existieren bei mir weiterhin nicht als eigenständige Protokolldateien in
`lib/subghz/protocols/` (`find`/`ls` gegen den vollständigen Checkout, kein Treffer für alle
vier Dateinamen), sondern bislang nur als separate, eigene Port-Vorschläge in diesem
Repository dokumentiert (`jarolift-keeloq-subghz-port.md`, `beninca-arc-aes-subghz-port.md`,
`prastel-42bit-rolling-code-port.md` — `ditec_gol4` hat noch keinen eigenen Port-Vorschlag).
Der hier beschriebene Override-Anschluss betrifft sie also
erst, sobald/falls diese vier Protokoll-Ports tatsächlich in die eigene Firmware übernommen
wurden — bis dahin bleiben von der ursprünglich gezählten Liste nur 11 der ~15 Protokolle, die
bei mir heute tatsächlich als eigenständige Datei existieren und für den Override-Anschluss
infrage kommen. Davon sind KeeLoq (Phase 2) und jetzt Secplus v1 (Phase 4) bereits
angeschlossen — macht 2 von 11, 9 offen (FAAC SLH, Secplus v2, Alutech AT-4N, Phoenix V2, Nice
FloR-S, CAME Atomo, KingGates Stylo4K, Somfy Keytis, Somfy Telis). Princeton (Phase 3) kommt
als zusätzlicher, nicht in der ursprünglichen Liste gezählter Fund hinzu — macht insgesamt 3
angeschlossene Protokolle. Siehe "Port-Skizze" für den aktualisierten Diff-Größen-Überblick der
verbleibenden 9.

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

**Jetzt geliefert (Phase 3, erstes Nachzieh-Protokoll Princeton):**
- `firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/princeton.c`
  — meine eigene `princeton.c` (Basis `G4MEOVER18/G4MEOVER-FW`, `b359f6fc32f282ce7217dd2d48a4e3478771852e`,
  per `git ls-remote` zu Sitzungsbeginn erneut als unverändert aktueller `HEAD`-Stand bestätigt),
  additiv um die zwei Anschlussstellen erweitert (siehe "Phase 3: Princeton" oben). Fertige
  Zieldatei zum direkten Ersetzen meiner vorhandenen `lib/subghz/protocols/princeton.c`.
- `firmware-mining/subghz-block-generic-global-override-port/connective-patches/princeton_c.patch`
  — dieselbe Ergänzung als Patch gegen eine unveränderte `G4MEOVER18/G4MEOVER-FW`-Basis.
- `firmware-mining/repro-subghz-block-generic-global-override-port-phase3/` — Host-Repro (siehe
  "Verifikation Phase 3").

**Jetzt geliefert (Phase 4, zweites Nachzieh-Protokoll Security+ 1.0):**
- `firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/secplus_v1.c`
  — meine eigene `secplus_v1.c` (Basis `G4MEOVER18/G4MEOVER-FW`, `main`, per frischem
  `git clone` zu Sitzungsbeginn als unverändert aktueller `HEAD`-Stand bestätigt), additiv um
  die zwei Anschlussstellen erweitert (siehe "Phase 4: Security+ 1.0" unten). Fertige
  Zieldatei zum direkten Ersetzen meiner vorhandenen `lib/subghz/protocols/secplus_v1.c`.
- `firmware-mining/subghz-block-generic-global-override-port/connective-patches/secplus_v1_c.patch`
  — dieselbe Ergänzung als Patch gegen eine unveränderte `G4MEOVER18/G4MEOVER-FW`-Basis.
- `firmware-mining/repro-subghz-block-generic-global-override-port-phase4/` — Host-Repro
  (siehe "Verifikation Phase 4").

**Jetzt geliefert (Phase 5, drittes Nachzieh-Protokoll Somfy Telis):**
- `firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/somfy_telis.c`
  — meine eigene `somfy_telis.c` (Basis `G4MEOVER18/G4MEOVER-FW`, `main`, per frischem
  `git clone` zu Sitzungsbeginn als unverändert aktueller `HEAD`-Stand bestätigt), additiv um
  die zwei Anschlussstellen erweitert (siehe "Phase 5: Somfy Telis" unten). Fertige Zieldatei
  zum direkten Ersetzen meiner vorhandenen `lib/subghz/protocols/somfy_telis.c`.
- `firmware-mining/subghz-block-generic-global-override-port/connective-patches/somfy_telis_c.patch`
  — dieselbe Ergänzung als Patch gegen eine unveränderte `G4MEOVER18/G4MEOVER-FW`-Basis.
- `firmware-mining/repro-subghz-block-generic-global-override-port-phase5/` — Host-Repro
  (siehe "Verifikation Phase 5").

**Für die weitere Nachzieh-Liste (nicht Teil dieser Sitzung, siehe "Port-Skizze"):** von den
ursprünglich ~15 gezählten Rolling-Code-Protokollen existieren vier (Beninca ARC, Jarolift,
Prastel, Ditec GOL4) bei mir noch gar nicht als eigenständige Protokolldatei (siehe
"Update (Phase 4)" oben) — es bleiben 8 echte Kandidaten: FAAC SLH, Secplus v2, Alutech AT-4N,
Phoenix V2, Nice FloR-S, CAME Atomo, KingGates Stylo4K, Somfy Keytis. Pro Datei derselbe
kleine, lokal begrenzte Satz Zeilen wie jetzt bei KeeLoq/Princeton/Secplus v1/Somfy Telis,
sofern sich der Gesamtdiff (teilweise durch Unleasheds separaten `common.c`-Umbau mitverursacht)
klein genug von Hand auftrennen lässt — bei Somfy Telis (231 Diff-Zeilen, siehe "Phase 5") hat
das wie bei Secplus v1 (231 Zeilen) und Princeton (214 Zeilen) zuverlässig funktioniert. Von
den verbliebenen 8 ist Phoenix V2 mit 324 Diff-Zeilen der nächstkleinste, hat aber einen
zusätzlichen `cnt_need_override`/`bypass`-Mechanismus (ähnlich KeeLoqs Modus-7-Pfad in Phase 2)
— hier ist derselbe sorgfältige Einzelabgleich wie bei KeeLoq nötig, kein blinder Dateiersatz.
Bei Protokollen mit deutlich größerem Gesamtdiff (FAAC SLH: 712 Zeilen, Secplus v2: 1133, Nice
FloR-S: 1284, Alutech AT-4N: 404, CAME Atomo: 403, KingGates Stylo4K: 496, Somfy Keytis: 405
u. a.) ist ebenfalls derselbe sorgfältige Einzelabgleich wie bei KeeLoq in Phase 2 nötig, kein
blinder Dateiersatz — noch nicht versucht. Keine dieser Dateien ist in dieser Sitzung
angefasst.

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

### Phase 3: Princeton (erstes Protokoll der Nachzieh-Liste)

**Was:** Princeton ist bei Unleashed **kein** Mitglied der in "Was" oben gezählten ~15
Rolling-Code-Protokolle — es ist ein reines Festcode-Protokoll
(`.type = SubGhzProtocolTypeStatic`, bei mir wie bei Unleashed unverändert), hat also keinen
Zähler und nutzt dementsprechend ausschließlich die Tasten-Override-Hälfte des Mechanismus,
nie die Zähler-Hälfte. Beim Diff meiner eigenen `princeton.c` gegen Unleasheds aktuellen
Standard-Branch (Commit `e611e31583c29b6576eda27afb41e020d10265ce`, 2026-10-23) zeigte sich:
Unleasheds separater `common.c`-Encoder-/Decoder-Basis-Umbau betrifft zwar auch `princeton.c`
(neue `#include "common.h"`, `SUBGHZ_ASSERT_*_LAYOUT`-Makros, `alloc`/`free` über
`subghz_protocol_*_common_alloc/_free` statt eigenem `malloc`/`free`) — anders als bei FAAC SLH
(712 Diff-Zeilen), Secplus v2 (1133), Nice FloR-S (1284) u. a. ist der Gesamtdiff mit 214 Zeilen
aber klein genug, dass sich die zwei tatsächlich override-relevanten Anschlussstellen auch so von
Hand zuverlässig heraustrennen lassen, ohne den Umbau selbst zu übernehmen (der bei mir nicht
existiert und außerhalb dieser Port-Skizze liegt). Meine eigene `princeton.c` hat exakt dieselbe
Vier-Fall-Tastencode-
Struktur (`subghz_protocol_princeton_get_btn_code()`, unverändert, eigener Code) und dieselbe
Zwei-Zweig-Anzeigeaufteilung (8-Bit-Codes `0x30`/`0xC0`/`0xF3`/`0xFC` vs. alle anderen 4-Bit-Codes)
wie Unleasheds aktuelle Version — kein struktureller Diff wie bei KeeLoq in Phase 2 nötig, die
beiden Anschlusspunkte lassen sich 1:1 übernehmen.

Ich liefere additiv, genau zwei Anschlussstellen in meiner eigenen `princeton.c`:
1. `subghz_protocol_encoder_princeton_get_upload()`: direkt nach der bestehenden
   `instance->generic.btn = subghz_protocol_princeton_get_btn_code();`-Zeile (meine eigene,
   unveränderte Tastencode-Zuweisung) ein `subghz_block_generic_global_button_override_get(
   &instance->generic.btn)`-Aufruf, der den endgültigen Tastenwert **vor** der anschließenden
   Datenrekonstruktion (die drei `if`/`else if`/`else`-Zweige, die je nach Tastenwert Seriennummer
   und Taste zu `instance->generic.data` zusammensetzen) überschreibt — dieselbe Reihenfolge wie
   bei KeeLoqs Encoder-Anschluss in Phase 2.
2. `subghz_protocol_decoder_princeton_get_string()`: `btn_is_available`/`current_btn` unbedingt
   am Funktionsanfang gesetzt (beide Zweige senden denselben Tastenwert, nur mit unterschiedlicher
   Bitlänge), `btn_length_bit` pro Zweig passend zur bestehenden Anzeigeaufteilung (8 für die vier
   8-Bit-Codes, 4 für alle anderen) — exakt Unleasheds eigene Anbindung, byte-für-byte übernommen
   (siehe "Verifikation Phase 3").

### Phase 4: Security+ 1.0 (zweites Protokoll der Nachzieh-Liste)

**Was:** `secplus_v1` ist bei Unleashed Mitglied der ursprünglich gezählten Rolling-Code-
Protokolle — anders als KeeLoq und Princeton nutzt es aber keine Modus-Tabelle für den Zähler,
sondern eine eigene, mehrstufige "OFEX" (Overflow-Experimental)-Logik mit einem zweiten
Sentinel-Wert (`-0x7FFFFFFF`) für `furi_hal_subghz_get_rolling_counter_mult()`. Beim Diff
meiner eigenen `secplus_v1.c` gegen Unleasheds aktuellen Standard-Branch zeigte sich dieselbe
Situation wie bei Princeton: Unleasheds separater `common.c`-Encoder-/Decoder-Basis-Umbau
betrifft auch diese Datei (neue `#include "common.h"`, `SUBGHZ_ASSERT_*_LAYOUT`, `alloc`/`free`
über `subghz_protocol_*_common_alloc/_free`, eigene `subghz_protocol_encoder_common_stop/_yield`
statt eigenem Encoder-Loop), aber der Gesamtdiff ist mit 231 Zeilen (ähnlich Princetons
214) klein genug, die zwei tatsächlich override-relevanten Anschlussstellen von Hand
herauszutrennen, ohne den `common.c`-Umbau selbst zu übernehmen. Unleasheds eigener Override-
Anschluss ersetzt dabei einen großen Teil der alten Zähler-Logik komplett (die bei Unleashed
inzwischen entfernten `multicntr`-Berechnung und die Sonderfallbehandlung für `rolling == 0`);
meine eigene `secplus_v1.c` hat diesen Umbau nicht mitgemacht — sie ist strukturell noch die
"alte" Logik, die Unleashed vor seinem eigenen Refactor ebenfalls hatte. Ein blinder
Dateiersatz hätte wie bei KeeLoq in Phase 2 die eigene, unveränderte `multicntr`-/OFEX-Logik
ersatzlos gelöscht. Stattdessen, wie bei KeeLoq, die Override-Abfrage als zusätzlicher,
vorangestellter Zweig einer bestehenden `if`/`else if`-Kette eingefügt, ohne die beiden
bestehenden Zweige (Standard-Inkrement, OFEX-Modus) inhaltlich anzutasten:

1. `subghz_protocol_secplus_v1_encode()`: die bestehende `if(furi_hal_subghz_get_
   rolling_counter_mult() != -0x7FFFFFFF) { ... } else { /* OFEX */ }`-Verzweigung bleibt
   vollständig erhalten, bekommt aber einen neuen, vorangestellten Zweig:
   `if(subghz_block_generic_global_counter_override_get(&rolling)) { /* Override direkt
   verwendet */ } else if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) { ... }
   else { /* OFEX, unverändert */ }`. Bei anhängendem Override wird weder der Standard-
   Inkrement-Pfad noch der OFEX-Pfad erreicht — beide bleiben für den unveränderten Fall exakt
   wie vorher.
2. `subghz_protocol_decoder_secplus_v1_get_string()`: `cnt_is_available`/`cnt_length_bit`/
   `current_cnt` sowie `btn_is_available`/`current_btn`/`btn_length_bit` unbedingt direkt nach
   der bestehenden `fixed`/`cnt`/`btn`-Berechnung gesetzt — exakt Unleasheds eigene Anbindung,
   byte-für-byte übernommen (siehe "Verifikation Phase 4"). `btn_is_available` ist dabei `false`:
   Secplus v1s Tastenwert ist aus dem Festcode-Teil (`fixed % 3`) abgeleitet, nicht unabhängig
   editierbar — dieselbe Festlegung, die Unleashed selbst trifft (kein eigener Fund, einfach
   übernommen).

### Phase 5: Somfy Telis (drittes Protokoll der Nachzieh-Liste)

**Was:** `somfy_telis` ist bei Unleashed Mitglied der ursprünglich gezählten Rolling-Code-
Protokolle und nutzt, wie Secplus v1 in Phase 4, dieselbe OFEX (Overflow-Experimental)-
Sentinel-Logik für `furi_hal_subghz_get_rolling_counter_mult()` (`-0x7FFFFFFF`), allerdings mit
einem eigenen, deutlich kleineren 16-Bit-Zählerbereich (statt Secplus v1s 32-Bit) und eigenen
Wrap-/Sprung-Formeln (Standard-Modus: Addition mit Wrap auf 0 bei Überschreiten von `0xFFFF`;
OFEX-Modus: Sprung auf `0xFFFE` statt einfachem Inkrement, außer bei den beiden Randwerten `0`
und `0xFFFE` selbst). Beim Diff meiner eigenen `somfy_telis.c` gegen Unleasheds aktuellen
Standard-Branch (siehe "Quelle") zeigte sich dieselbe Situation wie bei Princeton und
Secplus v1: Unleasheds separater `common.c`-Encoder-/Decoder-Basis-Umbau betrifft auch diese
Datei (neue `#include "common.h"`, `SUBGHZ_ASSERT_*_LAYOUT`, `alloc`/`free` über
`subghz_protocol_*_common_alloc/_free`, eigene `subghz_protocol_encoder_common_stop/_yield`
statt eigenem Encoder-Loop, `subghz_protocol_decoder_common_get_hash_data/_serialize` statt
eigenen Funktionen), aber der Gesamtdiff ist mit 231 Zeilen (identisch zu Secplus v1, nur knapp
über Princetons 214) klein genug, die zwei tatsächlich override-relevanten Anschlussstellen von
Hand herauszutrennen, ohne den `common.c`-Umbau selbst zu übernehmen. Von den sechs bis dahin
noch nicht geprüften echten Kandidaten (Alutech AT-4N: 404 Zeilen, Phoenix V2: 324, CAME Atomo:
403, KingGates Stylo4K: 496, Somfy Keytis: 405, Somfy Telis: 231) ist Somfy Telis damit klar
der kleinste und zugleich strukturell sauberste — Phoenix V2 als nächstkleinster hat
zusätzlich einen eigenen `cnt_need_override`/`bypass`-Mechanismus (ähnlich KeeLoqs
Modus-7-Pfad aus Phase 2), der einen sorgfältigeren, noch nicht versuchten Einzelabgleich
erfordert. Unleasheds eigener Override-Anschluss verschiebt dabei die `else if`-Bedingung der
bestehenden Standard-/OFEX-Verzweigung lediglich um eine neue, vorangestellte
Override-Bedingung nach unten (siehe "Verifikation Phase 5" für den exakten
`git diff --stat`-Beleg, dass das keine inhaltliche Löschung ist) — meine eigene
`somfy_telis.c` hat diesen Umbau nicht mitgemacht, ist aber strukturell identisch zu
Unleasheds eigener Vor-Override-Version. Ein blinder Dateiersatz hätte wie bei KeeLoq/
Secplus v1 keine eigene Erweiterung gelöscht (meine `somfy_telis.c` hat keine unabhängige
Erweiterung dieser Funktion), wäre aber trotzdem der falsche Weg gewesen, weil er den
kompletten, hier nicht benötigten `common.c`-Umbau mitgezogen hätte. Stattdessen, wie bei
Secplus v1, die Override-Abfrage als zusätzlicher, vorangestellter Zweig einer bestehenden
`if`/`else if`/`else`-Kette eingefügt, ohne die beiden bestehenden Zweige (Standard-Wrap,
OFEX-Sprung) inhaltlich anzutasten:

1. `subghz_protocol_somfy_telis_gen_data()`: direkt nach der bestehenden
   `btn = subghz_protocol_somfy_telis_get_btn_code();`-Zeile (meine eigene, unveränderte
   Tastencode-Zuweisung) ein `subghz_block_generic_global_button_override_get(&btn)`-Aufruf,
   der den endgültigen Tastenwert vor der anschließenden Zähler-Logik und Frame-Konstruktion
   überschreibt — dieselbe Reihenfolge wie bei KeeLoqs/Princetons Encoder-Anschluss. Direkt
   danach bekommt die bestehende `if(furi_hal_subghz_get_rolling_counter_mult() !=
   -0x7FFFFFFF) { ... } else { /* OFEX */ }`-Verzweigung einen neuen, vorangestellten Zweig:
   `if(subghz_block_generic_global_counter_override_get(&instance->generic.cnt)) { /* Override
   direkt verwendet */ } else if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF)
   { ... } else { /* OFEX, unverändert */ }`. Bei anhängendem Override wird weder der
   Standard-Wrap-Pfad noch der OFEX-Sprung-Pfad erreicht — beide bleiben für den unveränderten
   Fall exakt wie vorher.
2. `subghz_protocol_decoder_somfy_telis_get_string()`: `cnt_is_available`/`cnt_length_bit`/
   `current_cnt` sowie `btn_is_available`/`current_btn`/`btn_length_bit` unbedingt direkt nach
   dem bestehenden `check_remote_controller()`-Aufruf gesetzt — exakt Unleasheds eigene
   Anbindung, byte-für-byte übernommen (siehe "Verifikation Phase 5"). `btn_is_available` ist
   dabei `true`: anders als Secplus v1s abgeleiteter Tastenwert ist Somfy Telis' Tastenwert
   (`instance->generic.btn`, gesetzt in `check_remote_controller()`) unabhängig editierbar,
   dieselbe Festlegung, die Unleashed selbst trifft. Der bestehende `furi_string_cat_printf()`-
   Aufruf selbst (Anzeigeformat `"Btn:%s\r\n"`) bleibt unverändert — Unleashed hat diese Zeile
   bei sich zusätzlich auf `"Btn:%X - %s\r\n"` umgestellt, das ist aber eine reine
   Anzeige-Kosmetik ohne Bezug zum Override-Mechanismus und kein Teil der zwei tatsächlich
   override-relevanten Anschlussstellen, deshalb hier nicht übernommen.

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

### Verifikation Phase 3 (Princeton)

- **Patch additiv, byte-identisch:** `connective-patches/princeton_c.patch` per `git apply
  --check` sowie real per `git apply` gegen zwei frische, unabhängige Zweitkopien meiner eigenen
  `princeton.c` (`G4MEOVER18/G4MEOVER-FW`, `b359f6fc32f282ce7217dd2d48a4e3478771852e`) angewendet
  — beide Ergebnisse per `md5sum` byte-identisch zueinander und zur mitgelieferten Zieldatei
  bestätigt. `git diff --stat` nach dem Anwenden: **11 Zeilen Hinzufügung, 0 Löschungen**.
- **Funktionsverhalten
  (`firmware-mining/repro-subghz-block-generic-global-override-port-phase3/`):** `repro.c`
  verlinkt gegen die tatsächliche, unveränderte Phase-1-`generic.c`; die zwei neuen
  Anschlussstellen selbst sind per `sed`-Zeilenbereich byte-identisch aus der mitgelieferten,
  bereits gepatchten `princeton.c` in eigene `.inc`-Fragmente extrahiert und in minimalen
  Wrapper-Funktionen eingebunden (Zeilenbereiche in den Kommentaren von `repro.c`, mechanisch
  reproduzierbar: `sed 264,280p`/`602,604p`/`608p`/`627p`). Geprüft (37 Prüfungen):
  1. Override greift in allen drei Datenrekonstruktions-Zweigen (8-Bit ohne Maske `0x30`/`0xC0`,
     8-Bit mit Maskierung auf 4 Bit `0xF3`/`0xFC`, reiner 4-Bit-Zweig) und überschreibt dabei
     einen bewusst aus einem jeweils anderen Zweig gewählten Vorwert — Beleg, dass der Override,
     nicht der ursprüngliche `get_btn_code()`-Wert, über den Rekonstruktions-Zweig entscheidet.
  2. Override ist "einmal verbraucht": ein zweiter Aufruf direkt danach ohne neuen `_set()`
     verwendet den (simulierten) nächsten `get_btn_code()`-Vorwert unverändert.
  3. Ohne anhängenden Override läuft die bestehende Formel für alle neun real vorkommenden
     Tastencodes (`0x30`/`0xC0`/`0xF3`/`0xFC`/`0x1`/`0x2`/`0x4`/`0x8`/`0xF`) exakt wie vor dem
     Patch (Regression).
  4. `get_string()`s zwei Anzeige-Zweige setzen `btn_is_available`/`current_btn`/`btn_length_bit`
     korrekt (8 im ersten, 4 im zweiten Zweig) — der reale Aufrufpfad, der die in Phase 1
     dokumentierte Reihenfolge-Abhängigkeit (Bitlänge vor dem `_override_get()`-Aufruf) erfüllt;
     da Princeton nur die Tasten-, nie die Zähler-Hälfte des Mechanismus nutzt und `btn_length_bit`
     hier nie 0 wird, ist die in Phase 1 als UB dokumentierte Schiebe-Falle für dieses Protokoll
     grundsätzlich unerreichbar (keine neue Prüfung nötig, nur zur Vollständigkeit vermerkt).
  - Gebaut und ausgeführt mit `gcc -std=c17 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
    -fno-sanitize-recover=undefined`: **37 Prüfungen bestanden**, identischer Output beider
    Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde,
    `g_unexpected_stub_calls` bleibt bei 0.
  - Build-Befehl (aus `firmware-mining/repro-subghz-block-generic-global-override-port-phase3/`):
    `gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined
    -I ../subghz-block-generic-global-override-port -I mocks/lib/subghz/blocks -I mocks
    ../subghz-block-generic-global-override-port/lib/subghz/blocks/generic.c stubs.c repro.c -o repro`

### Verifikation Phase 4 (Security+ 1.0)

- **Patch additiv, byte-identisch:** `connective-patches/secplus_v1_c.patch` per `git apply
  --check` sowie real per `git apply` gegen zwei frische, unabhängige Zweitkopien meiner
  eigenen `secplus_v1.c` (`G4MEOVER18/G4MEOVER-FW`, `main`, per frischem `git clone` zu
  Sitzungsbeginn geholt) angewendet — beide Ergebnisse per `md5sum` byte-identisch zueinander
  und zur mitgelieferten Zieldatei bestätigt (zusätzlich per `git apply` in einer dritten
  Zweitkopie bestätigt). `git diff --stat` nach dem Anwenden: **14 Zeilen Hinzufügung,
  1 Löschung** (die eine Löschung ist die bestehende `if(furi_hal_subghz_get_rolling_counter_
  mult() != -0x7FFFFFFF) {`-Zeile, die durch die um den neuen Override-Zweig erweiterte Zeile
  `} else if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {` ersetzt wird — keine
  inhaltliche Löschung, reine Zeilenverschiebung in dieselbe Bedingung).
- **Funktionsverhalten
  (`firmware-mining/repro-subghz-block-generic-global-override-port-phase4/`):** `repro.c`
  verlinkt gegen die tatsächliche, unveränderte Phase-1-`generic.c`; die zwei neuen
  Anschlussstellen selbst sind per `sed`-Zeilenbereich byte-identisch aus der mitgelieferten,
  bereits gepatchten `secplus_v1.c` in eigene `.inc`-Fragmente extrahiert und in minimalen
  Wrapper-Funktionen eingebunden (Zeilenbereiche in den Kommentaren von `repro.c`, mechanisch
  reproduzierbar: `sed 228,274p` für den Zähler-Override-Zweig plus die unveränderten
  Geschwister-Zweige, `sed 613,621p` für den `get_string()`-Push-Block). Geprüft (18 Prüfungen):
  1. Zähler-Override verwendet den überschriebenen Wert verbatim und überspringt dabei sowohl
     den Standard-Inkrement-Pfad als auch den OFEX-Pfad vollständig — auch dann, wenn
     `furi_hal_subghz_get_rolling_counter_mult()` gleichzeitig auf den OFEX-Sentinel-Wert
     gesetzt ist (Beleg, dass der neue Zweig strikt vor der gesamten bestehenden `if`/`else
     if`-Kette läuft, nicht nur vor deren Standard-Hälfte). Override verbraucht sich einmalig.
  2. Ohne anhängenden Override läuft die bestehende Standard-Inkrement-Logik unverändert (die
     `rolling == 0`-Sonderbehandlung mit dem kleinen `multicntr`-Zweig sowie der normale
     `rolling += multicntr`-Zweig).
  3. Ohne anhängenden Override läuft die bestehende OFEX-Logik unverändert — sowohl der
     einfache `rolling++`-Fall unterhalb der `0xE6000000`-Schwelle als auch der
     Sprung-auf-`0xFFFFFFFE`-Fall bei/oberhalb der Schwelle (Regression gegen beide
     dokumentierten Verzweigungen, nicht nur den einfachen Inkrement-Fall).
  4. `get_string()`s Push-Block setzt `cnt_is_available` unbedingt `true`/`cnt_length_bit`
     unbedingt `32`/`current_cnt` korrekt sowie `btn_is_available` unbedingt `false`/
     `current_btn`/`btn_length_bit` korrekt, für zwei unterschiedliche Zähler-/Tastenwerte
     (inkl. des Randfalls beide `0`).
  - Gebaut und ausgeführt mit `gcc -std=c17 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
    -fno-sanitize-recover=undefined`: **18 Prüfungen bestanden**, identischer Output beider
    Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde,
    `g_unexpected_stub_calls` bleibt bei 0.
  - Build-Befehl (aus `firmware-mining/repro-subghz-block-generic-global-override-port-phase4/`):
    `gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined
    -I ../subghz-block-generic-global-override-port -I mocks/lib/subghz/blocks -I mocks
    ../subghz-block-generic-global-override-port/lib/subghz/blocks/generic.c stubs.c repro.c -o repro`

### Verifikation Phase 5 (Somfy Telis)

- **Patch additiv, byte-identisch:** `connective-patches/somfy_telis_c.patch` per `git apply
  --check` sowie real per `git apply` gegen drei frische, unabhängige Zweitkopien meiner
  eigenen `somfy_telis.c` (`G4MEOVER18/G4MEOVER-FW`, `main`, per frischem `git clone` zu
  Sitzungsbeginn geholt) angewendet — alle drei Ergebnisse per `md5sum` byte-identisch
  zueinander und zur mitgelieferten Zieldatei bestätigt. `git diff --stat` nach dem Anwenden:
  **20 Zeilen Hinzufügung, 1 Löschung** (die eine Löschung ist dieselbe Art Zeilenverschiebung
  wie bei Secplus v1 in Phase 4: die bestehende
  `if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {`-Zeile wird durch die um den
  neuen Override-Zweig erweiterte Zeile
  `} else if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {` ersetzt — keine
  inhaltliche Löschung, reine Verschiebung in dieselbe Bedingung).
- **Funktionsverhalten
  (`firmware-mining/repro-subghz-block-generic-global-override-port-phase5/`):** `repro.c`
  verlinkt gegen die tatsächliche, unveränderte Phase-1-`generic.c`; die zwei neuen
  Anschlussstellen selbst sind per `sed`-Zeilenbereich byte-identisch aus der mitgelieferten,
  bereits gepatchten `somfy_telis.c` in eigene `.inc`-Fragmente extrahiert und in minimalen
  Wrapper-Funktionen eingebunden (Zeilenbereiche in den Kommentaren von `repro.c`, mechanisch
  reproduzierbar: `sed 129,152p` für den Tasten-Override- und den Zähler-Override-Zweig plus
  die zwei unveränderten Geschwister-Zweige, `sed 762,770p` für den `get_string()`-Push-Block).
  Geprüft (25 Prüfungen):
  1. Tasten-Override verbraucht sich einmalig, maskiert auf das reale `btn_length_bit` (4) —
     ein außerhalb des 4-Bit-Bereichs liegender Override-Wert (`0x1F`) belegt dabei zugleich die
     Masken-Formel für diese konkrete Protokoll-Bitbreite (`0x1F & 0xF == 0xF`).
  2. Ohne anhängenden Tasten-Override bleibt der übergebene, vorbestehende
     `get_btn_code()`-Wert unverändert (Regression).
  3. Zähler-Override verwendet den überschriebenen, maskierten Wert verbatim und überspringt
     dabei sowohl den Standard-Wrap-Pfad als auch den OFEX-Sprung-Pfad vollständig — auch dann,
     wenn `furi_hal_subghz_get_rolling_counter_mult()` gleichzeitig auf den OFEX-Sentinel-Wert
     gesetzt ist (Beleg, dass der neue Zweig strikt vor der gesamten bestehenden `if`/`else
     if`-Kette läuft, nicht nur vor deren Standard-Hälfte) — ein außerhalb des 16-Bit-Bereichs
     liegender Override-Wert (`0x1ABCD`) belegt dabei zugleich die Masken-Formel für den realen
     16-Bit-Zähler dieses Protokolls (`0x1ABCD & 0xFFFF == 0xABCD`). Override verbraucht sich
     einmalig.
  4. Ohne anhängenden Override läuft die bestehende Standard-Wrap-Logik unverändert — sowohl
     der einfache Inkrement-Fall als auch der Wrap-auf-0-Fall bei Überschreiten von `0xFFFF`.
  5. Ohne anhängenden Override läuft die bestehende OFEX-Logik unverändert — alle drei real
     vorhandenen Verzweigungen: Wrap-auf-0 bei `cnt == 0xFFFF`, Sprung-auf-`0xFFFE` für jeden
     sonstigen Wert `>= 1`, sowie die beiden vom Sprung bewusst ausgenommenen
     Rand-Inkrement-Fälle `cnt == 0` und `cnt == 0xFFFE` (Regression gegen alle vier
     dokumentierten OFEX-Teilfälle, nicht nur den Sprung-Fall).
  6. `get_string()`s Push-Block setzt `cnt_is_available` unbedingt `true`/`cnt_length_bit`
     unbedingt `16`/`current_cnt` korrekt sowie `btn_is_available` unbedingt `true`/
     `current_btn`/`btn_length_bit` unbedingt `4` korrekt, für zwei unterschiedliche
     Zähler-/Tastenwerte (inkl. des Randfalls beide `0`).
  7. Die in Phase 1 dokumentierte Reihenfolge-Abhängigkeit (`cnt_length_bit`/`btn_length_bit`
     vor dem ersten `_override_get()`-Aufruf) für die realen Bitbreiten dieses Protokolls (16/4)
     explizit durchexerziert: `get_string()`-Fragment zuerst ausgeführt (setzt beide Längen),
     danach ein Override-Set/Get-Durchlauf im `gen_data()`-Fragment unter Wiederverwendung
     dieser Längen — kein UB, korrekt maskiertes Ergebnis.
  - Gebaut und ausgeführt mit `gcc -std=c17 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
    -fno-sanitize-recover=undefined`: **25 Prüfungen bestanden**, identischer Output beider
    Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde,
    `g_unexpected_stub_calls` bleibt bei 0.
  - Build-Befehl (aus `firmware-mining/repro-subghz-block-generic-global-override-port-phase5/`):
    `gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined
    -I ../subghz-block-generic-global-override-port -I mocks/lib/subghz/blocks -I mocks
    ../subghz-block-generic-global-override-port/lib/subghz/blocks/generic.c stubs.c repro.c -o repro`

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
3. **Phase 3 (geliefert, vorherige Sitzung — Princeton):** erstes Nachzieh-Protokoll
   angeschlossen, nur die Tasten-Hälfte des Mechanismus (Princeton ist Festcode, kein Zähler).
   Gesamtdiff mit 214 Zeilen klein genug, um die zwei override-relevanten Anschlussstellen von
   Hand aus dem (auch bei Princeton vorhandenen, aber hier nicht übernommenen)
   `common.c`-Umbau-Diff herauszutrennen.
4. **Phase 4 (geliefert, vorherige Sitzung — Security+ 1.0):** zweites Nachzieh-Protokoll
   angeschlossen, erstes Nachzieh-Protokoll mit tatsächlicher Zähler-Hälfte (zusätzlich zur
   Tasten-Hälfte). Gesamtdiff mit 231 Zeilen ähnlich klein wie bei Princeton; die bestehende
   eigene OFEX-/`multicntr`-Logik bleibt dabei (wie KeeLoq in Phase 2) vollständig erhalten,
   der Override nur als vorangestellter Zweig davor eingefügt. Dabei geklärt: vier der
   ursprünglich gezählten ~15 Protokolldateien (Beninca ARC, Jarolift, Prastel, Ditec GOL4)
   existieren bei mir noch nicht als eigene Datei — siehe "Update (Phase 4)" oben.
5. **Phase 5 (geliefert, diese Sitzung — Somfy Telis):** drittes Nachzieh-Protokoll
   angeschlossen, zweites mit tatsächlicher Zähler-Hälfte. Gesamtdiff mit 231 Zeilen identisch
   klein zu Secplus v1, nach gezielter Diff-Prüfung aller sechs bis dahin noch nicht geprüften
   echten Kandidaten der kleinste und strukturell sauberste. Dasselbe OFEX-Sentinel-Muster wie
   bei Secplus v1, aber mit eigenem 16-Bit-Zählerbereich und eigenen Wrap-/Sprung-Formeln; die
   bestehende Standard-/OFEX-Logik bleibt dabei (wie bei Secplus v1) vollständig erhalten, der
   Override nur als vorangestellter Zweig davor eingefügt. Weiterhin offen: die übrigen 8 der
   tatsächlich existierenden Nachzieh-Kandidaten (FAAC SLH, Secplus v2, Alutech AT-4N,
   Phoenix V2, Nice FloR-S, CAME Atomo, KingGates Stylo4K, Somfy Keytis — gleiches Muster wie
   KeeLoq/Secplus v1/Somfy Telis, siehe "Betroffene Dateien" — pro Protokoll eigener Diff gegen
   Unleashed nötig, da bereits bei KeeLoq und jetzt bei Phoenix V2 erhebliche strukturelle
   Abweichungen von der aktuellen Unleashed-Version bestanden bzw. zu erwarten sind, und bei
   den auf `common.c` umgebauten Protokollen wie FAAC SLH ein deutlich größerer Diff als bei
   KeeLoq/Princeton/Secplus v1/Somfy Telis zu erwarten ist), dann `endless_tx` an die eigene
   SubGhz-Transmitter-App anschließen (Hold-OK = Dauersenden statt fester Wiederholzahl).
6. **Phase 6 (separater, deutlich größerer Port, nicht Teil dieser Port-Skizze):** erst ab hier
   wird ein Port der App **SubGhz Remote** (siehe "Was") sinnvoll — ihr `subrem_tx_start_sub()`
   braucht exakt den hier gelieferten Mechanismus, um den Tastenwert vor dem Senden zu
   überschreiben. Verdient eine eigene Port-Vorschlag-Datei mit eigener Port-Skizze (Map-Datei-
   Format, Szenen/Views, die 684-Zeilen-`subghz_txrx`-Kopie der App gegen meine eigene,
   unabhängig weiterentwickelte `applications/main/subghz/helpers/subghz_txrx.c` abgleichen —
   bei einem ersten Blick bereits mehrere funktionale Unterschiede sichtbar, u. a. der
   OTG-Einschaltpfad: die App nutzt einen begrenzten `furi_hal_power_enable_otg()`-Retry mit
   5 Versuchen, meine eigene Version einen einmaligen Aufruf über den `Power`-Service).
7. Bauen nach Phase 1: `ufbt` (bzw. `fbt firmware_all`) — reine Ergänzung, sollte ohne
   Compile-Fehler durchlaufen. Funktionstest: ein beliebiges SubGhz-Signal anzeigen lassen (löst
   den neuen `base.c`-Hook aus), Verhalten muss unverändert bleiben (reiner Infrastruktur-Schritt,
   noch kein sichtbares Feature). Ab Phase 2 mit Hardware testbar: KeeLoq-Signal erfassen, über
   ein künftiges Edit-Menü den Zähler ändern, erneut senden, am Zielgerät (eigenes, autorisiert
   getestetes Garagentor/Empfänger) beobachten, ob/wie der geänderte Zähler akzeptiert wird. Nach
   Phase 3 zusätzlich mit Princeton testbar: Festcode-Signal erfassen, über ein künftiges
   Edit-Menü die Taste ändern, erneut senden, am Zielgerät beobachten, ob der geänderte
   Tastenwert erwartungsgemäß erkannt wird. Nach Phase 4 zusätzlich mit Secplus v1 testbar:
   Rolling-Code-Signal erfassen, über ein künftiges Edit-Menü den Zähler ändern, erneut senden,
   am Zielgerät (eigenes, autorisiert getestetes Garagentor/Empfänger) beobachten, ob/wie der
   geänderte Zähler akzeptiert wird. Nach Phase 5 zusätzlich mit Somfy Telis testbar: dieselbe
   Zähler-/Tasten-Prüfung wie bei Secplus v1, am eigenen, autorisiert getesteten
   Rolling-Code-Empfänger (z. B. Somfy-Antrieb).

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
  dieser App bewusst nicht Teil dieser Sitzung (siehe "Port-Skizze", Phase 6).
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
- (Phase 3) Unleashed-Firmware, Standard-Branch, Commit
  `e611e31583c29b6576eda27afb41e020d10265ce` (2026-10-23, per frischem `git clone` zu
  Sitzungsbeginn als aktueller `HEAD`-Stand bestätigt): `lib/subghz/protocols/princeton.c` zum
  Abgleich geholt, die zwei Button-Override-Anschlusspunkte (siehe "Phase 3: Princeton") daraus
  übernommen; der übrige, hier nicht übernommene Teil des Diffs besteht ausschließlich aus
  Unleasheds separatem `common.c`-Encoder-/Decoder-Basis-Umbau (`subghz_protocol_encoder_common_*`,
  `SUBGHZ_ASSERT_*_LAYOUT`), der mit diesem Port nicht zusammenhängt und nicht übernommen wird.
- (Phase 3) `G4MEOVER18/G4MEOVER-FW`, `main`, Commit `b359f6fc32f282ce7217dd2d48a4e3478771852e`
  (per `git ls-remote` zu Sitzungsbeginn Phase 3 erneut als unverändert aktueller `HEAD`-Stand
  bestätigt, identisch zu Phase 1/2): eigene Basis für `lib/subghz/protocols/princeton.c` sowie
  Ausgangspunkt für `connective-patches/princeton_c.patch`.
- (Phase 4) Unleashed-Firmware, Standard-Branch, Commit
  `44ef0b91eafeb59215626cffb2cac9be1fc2f310` (2026-10-24, per frischem `git clone` zu
  Sitzungsbeginn geholt): `lib/subghz/protocols/secplus_v1.c` zum Abgleich geholt, die zwei
  Counter-/Availability-Override-Anschlusspunkte (siehe "Phase 4: Security+ 1.0") daraus
  übernommen; der übrige, hier nicht übernommene Teil des Diffs besteht wie bei Princeton
  ausschließlich aus Unleasheds separatem `common.c`-Encoder-/Decoder-Basis-Umbau, der mit
  diesem Port nicht zusammenhängt und nicht übernommen wird.
- (Phase 4) `G4MEOVER18/G4MEOVER-FW`, `main`, Commit `b359f6fc32f282ce7217dd2d48a4e3478771852e`
  (per frischem `git clone` zu Sitzungsbeginn Phase 4 erneut als unverändert aktueller
  `HEAD`-Stand bestätigt, identisch zu Phase 1/2/3): eigene Basis für
  `lib/subghz/protocols/secplus_v1.c` sowie Ausgangspunkt für
  `connective-patches/secplus_v1_c.patch`; gleichzeitig Fundort/Beleg für die in "Update
  (Phase 4)" dokumentierte Klärung, dass `beninca_arc.c`, `jarolift.c`, `prastel.c`,
  `ditec_gol4.c` hier (noch) nicht existieren (`find lib/subghz/protocols -iname` gegen den
  vollständigen, frisch geklonten Checkout, kein Treffer für alle vier Dateinamen).
- (Phase 5) Unleashed-Firmware, Standard-Branch, Commit
  `0d295f581c1e34cd56e2e590ea85625a64c2b724` (2026-10-24, per frischem `git clone` zu
  Sitzungsbeginn geholt): `lib/subghz/protocols/alutech_at_4n.c`, `phoenix_v2.c`,
  `came_atomo.c`, `kinggates_stylo_4k.c`, `somfy_keytis.c`, `somfy_telis.c` zum Diff-Größen-
  Abgleich aller sechs bis dahin noch nicht geprüften echten Kandidaten geholt (siehe "Phase 5:
  Somfy Telis" für die Diff-Zeilenzahlen); `somfy_telis.c` zusätzlich zum Anschlusspunkt-
  Abgleich herangezogen, die zwei Override-Anschlusspunkte (siehe "Phase 5: Somfy Telis")
  daraus übernommen; der übrige, hier nicht übernommene Teil des Diffs besteht wie bei
  Princeton/Secplus v1 ausschließlich aus Unleasheds separatem `common.c`-Encoder-/Decoder-
  Basis-Umbau, der mit diesem Port nicht zusammenhängt und nicht übernommen wird.
- (Phase 5) `G4MEOVER18/G4MEOVER-FW`, `main`, Commit `b359f6fc32f282ce7217dd2d48a4e3478771852e`
  (per frischem `git clone` zu Sitzungsbeginn Phase 5 erneut als unverändert aktueller
  `HEAD`-Stand bestätigt, identisch zu Phase 1/2/3/4): eigene Basis für die sechs oben
  genannten, bis dahin noch nicht geprüften Kandidat-Dateien sowie für
  `lib/subghz/protocols/somfy_telis.c` im Speziellen, Ausgangspunkt für
  `connective-patches/somfy_telis_c.patch`.
