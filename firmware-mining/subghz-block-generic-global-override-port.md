# SubGhzBlockGenericGlobal: fehlender Rolling-Code-Counter/Button-Override- und Endless-TX-Mechanismus

**Update (Phase 8):** sechstes Nachzieh-Protokoll (Alutech AT-4N, `alutech_at_4n`) angeschlossen —
siehe "Phase 8: Alutech AT-4N" sowie "Verifikation Phase 8" unten. Von den verbliebenen sechs
Kandidaten (FAAC SLH: 712 Diff-Zeilen, Secplus v2: 1133, Alutech AT-4N: 404, Nice FloR-S: 1284,
KingGates Stylo4K: 496, Somfy Keytis: 405) hatte Alutech AT-4N den kleinsten Gesamtdiff und war
damit an der Reihe. Wie bei CAME Atomo (Phase 7) ist der `cnt_need_override`/`bypass`-Mechanismus
hier **kein** Fremdfeature: meine eigene `alutech_at_4n.c` hat bereits ein eigenständiges,
dreistufiges `alutech_at4n_counter_mode`-Feature (Modus 0 Standard/OFEX, Modus 1 feste
Wertetabelle `0000`/`0001`/`FFFE`/`FFFF`, Modus 2 Zähler `0x0000`..`0x0005` mit Wrap) — eine Stufe
weniger als CAME Atomo (kein Freeze-Modus), sonst identisch zu Unleasheds eigenem. Da die Modi 1/2
den Override nie konsultieren, übernimmt Phase 8 auch hier Unleasheds `bypass`-Flag. Anders als bei
CAME Atomo prüft der Bypass-Aufbau in `gen_data()` dabei nur `cnt_need_override`, nicht
`btn_need_override` — der Tasten-Override-Aufruf (separat in `get_upload()`, vor dem Aufruf von
`gen_data()`) setzt `bypass` bei Erfolg selbst direkt, wodurch ein reiner Tasten-Edit in Modus 1/2
trotzdem noch denselben Effekt auf den folgenden Zähler-Durchlauf hat (siehe "Phase 8: Alutech
AT-4N" und Szenario E in "Verifikation Phase 8"). Von den ursprünglich gezählten ~15 Protokollen
bleiben weiterhin vier (Beninca ARC, Jarolift, Prastel, Ditec GOL4) ohne eigenständige
Protokolldatei (unverändert seit "Update (Phase 4)"), es bleiben 11 echte Kandidaten. Davon sind
KeeLoq (Phase 2), Secplus v1 (Phase 4), Somfy Telis (Phase 5), Phoenix V2 (Phase 6), CAME Atomo
(Phase 7) und jetzt Alutech AT-4N (Phase 8) angeschlossen — macht 6 von 11, 5 offen (FAAC SLH,
Secplus v2, Nice FloR-S, KingGates Stylo4K, Somfy Keytis). Princeton (Phase 3) bleibt der
zusätzliche, nicht in der ursprünglichen Liste gezählte Fund — macht insgesamt 7 angeschlossene
Protokolle.

**Update (Phase 7):** fünftes Nachzieh-Protokoll (CAME Atomo, `came_atomo`) angeschlossen — siehe
"Phase 7: CAME Atomo" sowie "Verifikation Phase 7" unten. Von den verbliebenen sieben
Kandidaten (FAAC SLH: 712 Diff-Zeilen, Secplus v2: 1133, Alutech AT-4N: 404, Nice FloR-S: 1284,
CAME Atomo: 403, KingGates Stylo4K: 496, Somfy Keytis: 405) hatte CAME Atomo den kleinsten
Gesamtdiff und war damit an der Reihe — anders als bei Phoenix V2 (Phase 6) ist der
`cnt_need_override`/`bypass`-Mechanismus hier aber *kein* Fremdfeature, das übersprungen werden
konnte: meine eigene `came_atomo.c` hat bereits ein eigenständiges, vierstufiges
`came_atomo_counter_mode`-Feature (Modus 0 Standard/OFEX, Modus 1/2 feste Wertetabellen, Modus 3
Zähler-Freeze) — identisch zu Unleasheds eigenem. Da die Modi 1-3 den Override nie konsultieren,
übernimmt Phase 7 auch Unleasheds `bypass`-Flag, das den Standard-/OFEX-Zweig für einen Durchlauf
erzwingt, sobald ein Override anhängt, unabhängig vom eingestellten Modus — sonst würde ein
Zähler- oder Tasten-Edit in Modus 1-3 stillschweigend ignoriert. Von den ursprünglich gezählten
~15 Protokollen bleiben weiterhin vier (Beninca ARC, Jarolift, Prastel, Ditec GOL4) ohne
eigenständige Protokolldatei (unverändert seit "Update (Phase 4)"), es bleiben 11 echte
Kandidaten. Davon sind KeeLoq (Phase 2), Secplus v1 (Phase 4), Somfy Telis (Phase 5), Phoenix V2
(Phase 6) und jetzt CAME Atomo (Phase 7) angeschlossen — macht 5 von 11, 6 offen (FAAC SLH,
Secplus v2, Alutech AT-4N, Nice FloR-S, KingGates Stylo4K, Somfy Keytis). Princeton (Phase 3)
bleibt der zusätzliche, nicht in der ursprünglichen Liste gezählte Fund — macht insgesamt 6
angeschlossene Protokolle.

**Update (Phase 6):** viertes Nachzieh-Protokoll (Phoenix V2, `phoenix_v2`) angeschlossen — siehe
"Phase 6: Phoenix V2" sowie "Verifikation Phase 6" unten. Von den sechs in Phase 5 verglichenen,
bis dahin noch nicht einzeln geprüften Kandidaten (Alutech AT-4N: 404 Diff-Zeilen, Phoenix V2:
324, CAME Atomo: 403, KingGates Stylo4K: 496, Somfy Keytis: 405, Somfy Telis: 231 — Somfy Telis
bereits in Phase 5 angeschlossen) war Phoenix V2 der nächstkleinste Gesamtdiff; dort bereits als
Vorbehalt vermerkt war ein zusätzlicher `cnt_need_override`/`bypass`-Mechanismus (ähnlich KeeLoqs
Modus-7-Pfad aus Phase 2). Beim tatsächlichen Diff gegen Unleashed zeigte sich: dieser
Mechanismus gehört zu einem bei Unleashed zusätzlich vorhandenen, eigenständigen
"CounterMode"-Feature (ein per `.sub`-Dateifeld wählbares, drittes Zähler-Schema), das mit dem
hier portierten Override-Mechanismus nur über das `bypass`-Flag verzahnt ist, selbst aber eine
eigene, deutlich größere Erweiterung ist — meine eigene `phoenix_v2.c` kennt kein "CounterMode"
und läuft, wie vor Unleasheds eigener Einführung dieses Features, ausschließlich über die
bestehende Standard-/OFEX-Verzweigung. Dieselbe Begründung wie bei KeeLoqs Modus 7 in Phase 2: ein
Feature, das mein eigener Code strukturell nicht kennt, wird nicht blind mitportiert — siehe
"Phase 6: Phoenix V2" für die zwei tatsächlich übernommenen, override-relevanten
Anschlussstellen. Damit sind von den ursprünglich gezählten ~15 Protokollen weiterhin vier
(Beninca ARC, Jarolift, Prastel, Ditec GOL4) ohne eigenständige Protokolldatei (unverändert seit
"Update (Phase 4)"), es bleiben 11 echte Kandidaten. Davon sind KeeLoq (Phase 2), Secplus v1
(Phase 4), Somfy Telis (Phase 5) und jetzt Phoenix V2 (Phase 6) angeschlossen — macht 4 von 11, 7
offen (FAAC SLH, Secplus v2, Alutech AT-4N, Nice FloR-S, CAME Atomo, KingGates Stylo4K, Somfy
Keytis). Princeton (Phase 3) bleibt der zusätzliche, nicht in der ursprünglichen Liste gezählte
Fund — macht insgesamt 5 angeschlossene Protokolle.

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

**Jetzt geliefert (Phase 6, viertes Nachzieh-Protokoll Phoenix V2):**
- `firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/phoenix_v2.c`
  — meine eigene `phoenix_v2.c` (Basis `G4MEOVER18/G4MEOVER-FW`, `main`, per frischem
  `git clone` zu Sitzungsbeginn als unverändert aktueller `HEAD`-Stand bestätigt), additiv um
  die zwei Anschlussstellen erweitert (siehe "Phase 6: Phoenix V2" unten). Fertige Zieldatei
  zum direkten Ersetzen meiner vorhandenen `lib/subghz/protocols/phoenix_v2.c`.
- `firmware-mining/subghz-block-generic-global-override-port/connective-patches/phoenix_v2_c.patch`
  — dieselbe Ergänzung als Patch gegen eine unveränderte `G4MEOVER18/G4MEOVER-FW`-Basis.
- `firmware-mining/repro-subghz-block-generic-global-override-port-phase6/` — Host-Repro
  (siehe "Verifikation Phase 6").

**Jetzt geliefert (Phase 7, fünftes Nachzieh-Protokoll CAME Atomo):**
- `firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/came_atomo.c`
  — meine eigene `came_atomo.c` (Basis `G4MEOVER18/G4MEOVER-FW`, `main`, per frischem
  `git clone` zu Sitzungsbeginn als unverändert aktueller `HEAD`-Stand bestätigt), additiv um
  die drei Anschlussstellen erweitert (siehe "Phase 7: CAME Atomo" unten). Fertige Zieldatei
  zum direkten Ersetzen meiner vorhandenen `lib/subghz/protocols/came_atomo.c`.
- `firmware-mining/subghz-block-generic-global-override-port/connective-patches/came_atomo_c.patch`
  — dieselbe Ergänzung als Patch gegen eine unveränderte `G4MEOVER18/G4MEOVER-FW`-Basis.
- `firmware-mining/repro-subghz-block-generic-global-override-port-phase7/` — Host-Repro
  (siehe "Verifikation Phase 7").

**Jetzt geliefert (Phase 8, sechstes Nachzieh-Protokoll Alutech AT-4N):**
- `firmware-mining/subghz-block-generic-global-override-port/lib/subghz/protocols/alutech_at_4n.c`
  — meine eigene `alutech_at_4n.c` (Basis `G4MEOVER18/G4MEOVER-FW`, `main`, per frischem
  `git clone` zu Sitzungsbeginn als unverändert aktueller `HEAD`-Stand bestätigt), additiv um
  die drei Anschlussstellen erweitert (siehe "Phase 8: Alutech AT-4N" unten). Fertige Zieldatei
  zum direkten Ersetzen meiner vorhandenen `lib/subghz/protocols/alutech_at_4n.c`.
- `firmware-mining/subghz-block-generic-global-override-port/connective-patches/alutech_at_4n_c.patch`
  — dieselbe Ergänzung als Patch gegen eine unveränderte `G4MEOVER18/G4MEOVER-FW`-Basis.
- `firmware-mining/repro-subghz-block-generic-global-override-port-phase8/` — Host-Repro
  (siehe "Verifikation Phase 8").

**Für die weitere Nachzieh-Liste (nicht Teil dieser Sitzung, siehe "Port-Skizze"):** von den
ursprünglich ~15 gezählten Rolling-Code-Protokollen existieren vier (Beninca ARC, Jarolift,
Prastel, Ditec GOL4) bei mir noch gar nicht als eigenständige Protokolldatei (siehe
"Update (Phase 4)" oben) — es bleiben, nach dem jetzt angeschlossenen Alutech AT-4N (Phase 8), 5
echte Kandidaten: FAAC SLH, Secplus v2, Nice FloR-S, KingGates Stylo4K, Somfy Keytis. Pro Datei
derselbe kleine, lokal begrenzte Satz Zeilen wie jetzt bei KeeLoq/Princeton/Secplus v1/Somfy
Telis/Phoenix V2/CAME Atomo/Alutech AT-4N, sofern sich der Gesamtdiff (teilweise durch Unleasheds
separaten `common.c`-Umbau mitverursacht) klein genug von Hand auftrennen lässt — bei Somfy Telis
(231 Diff-Zeilen, siehe "Phase 5"), Phoenix V2 (324 Zeilen, siehe "Phase 6"), CAME Atomo
(403 Zeilen, siehe "Phase 7") und Alutech AT-4N (404 Zeilen, siehe "Phase 8") hat das wie bei
Secplus v1 (231 Zeilen) und Princeton (214 Zeilen) zuverlässig funktioniert, obwohl Phoenix V2
zusätzlich Unleasheds eigenständiges "CounterMode"-Feature im Diff hatte (siehe
"Update (Phase 6)" — bewusst nicht mitportiert), CAME Atomo zusätzlich Unleasheds separaten
`common.c`-Umbau sowie einen neuen sechsten/siebten Tastencode samt eines vierten
Header-Timing-Zweigs im Diff hatte (siehe "Update (Phase 7)" — beide bewusst nicht mitportiert)
und Alutech AT-4N zusätzlich Unleasheds separaten `common.c`-Umbau sowie eine auf ein
6-Byte-Puffer-Array umgestellte Rainbow-Table-Ladefunktion im Diff hatte (siehe
"Update (Phase 8)" bzw. "Phase 8: Alutech AT-4N" — beide bewusst nicht mitportiert; wie bei CAME
Atomo war der `bypass`-Mechanismus bei Alutech AT-4N aber kein Fremdfeature und wurde
mitportiert). Bei Protokollen mit deutlich größerem Gesamtdiff (FAAC SLH: 712 Zeilen, Secplus v2:
1133, Nice FloR-S: 1284, KingGates Stylo4K: 496, Somfy Keytis: 405 u. a.) ist derselbe
sorgfältige Einzelabgleich wie bei KeeLoq in Phase 2 nötig, kein blinder Dateiersatz — noch nicht
versucht. Keine dieser Dateien ist in dieser Sitzung angefasst.

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

### Phase 6: Phoenix V2 (viertes Protokoll der Nachzieh-Liste)

**Was:** `phoenix_v2` ist bei Unleashed Mitglied der ursprünglich gezählten Rolling-Code-
Protokolle und nutzt, wie Secplus v1 (Phase 4) und Somfy Telis (Phase 5), dieselbe OFEX
(Overflow-Experimental)-Sentinel-Logik für `furi_hal_subghz_get_rolling_counter_mult()`
(`-0x7FFFFFFF`), mit demselben 16-Bit-Zählerbereich und denselben Wrap-/Sprung-Formeln wie
Somfy Telis (Standard-Modus: Addition mit Wrap auf 0 bei Überschreiten von `0xFFFF`;
OFEX-Modus: Sprung auf `0xFFFE` statt einfachem Inkrement, außer bei den beiden Randwerten `0`
und `0xFFFE` selbst). Beim Diff meiner eigenen `phoenix_v2.c` gegen Unleasheds aktuellen
Standard-Branch (siehe "Quelle") zeigte sich derselbe Gesamtdiff-Umfang wie in Phase 5
angekündigt: 324 Zeilen, davon ein erster, hier nicht relevanter Block aus Unleasheds separatem
`common.c`-Encoder-/Decoder-Basis-Umbau (`#include "common.h"`, `SUBGHZ_ASSERT_*_LAYOUT`,
`alloc`/`free` über `subghz_protocol_*_common_alloc/_free`, eigene
`subghz_protocol_encoder_common_stop/_yield` statt eigenem Encoder-Loop). Zusätzlich, anders als
bei Princeton/Secplus v1/Somfy Telis, ein zweiter, hier ebenfalls nicht übernommener Block:
Unleasheds eigenes, zusätzliches "CounterMode"-Feature — ein per `.sub`-Dateifeld (`CounterMode`)
wählbares drittes Zähler-Schema (Modus 2: fester Bereich `0x0000`–`0x0004`) plus eine
Modul-globale `v2_phoenix_counter_mode`-Variable und eine `bypass`-Variable, die den
Standard-/OFEX-Zweig nur noch für `v2_phoenix_counter_mode == 0` durchläuft. Dieses Feature ist
bei mir strukturell nicht vorhanden (meine eigene `phoenix_v2.c` kennt kein `CounterMode`-Feld
und lief schon vor Unleasheds eigener Einführung dieses Features ausschließlich über die hier
gezeigte Standard-/OFEX-Verzweigung) und hängt mit dem hier portierten Override-Mechanismus nur
über das `bypass`-Flag zusammen (`if(subghz_block_generic_global.cnt_need_override) bypass =
true;`) — dieselbe Art Seitenfeature wie KeeLoqs Modus-7-Lernsequenz in Phase 2, bewusst nicht
mitportiert, da mein Code dafür keine Entsprechung hat. Nach Abzug beider Blöcke bleiben exakt
dieselben zwei override-relevanten Anschlussstellen wie bei Somfy Telis übrig, strukturell 1:1
übertragbar, da meine eigene `phoenix_v2.c` vor diesen beiden Unleashed-Umbauten strukturell
identisch zu Unleasheds eigener Vor-Umbau-Version ist.

Ich liefere additiv, genau zwei Anschlussstellen in meiner eigenen `phoenix_v2.c`:
1. `subghz_protocol_encoder_phoenix_v2_get_upload()`: direkt nach der bestehenden
   `btn = subghz_protocol_phoenix_v2_get_btn_code();`-Zeile (meine eigene, unveränderte, durch
   ein eventuell gesetztes Custom-Button eigenständig überschriebene Tastencode-Zuweisung) ein
   `subghz_block_generic_global_button_override_get(&btn)`-Aufruf, der den endgültigen
   Tastenwert vor der anschließenden Zähler-Logik und Datenrekonstruktion überschreibt —
   dieselbe Reihenfolge wie bei KeeLoqs/Princetons/Somfy Telis' Encoder-Anschluss. Direkt danach
   bekommt die bestehende `if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) { ...
   } else { /* OFEX */ }`-Verzweigung einen neuen, vorangestellten Zweig:
   `if(subghz_block_generic_global_counter_override_get(&instance->generic.cnt)) { /* Override
   direkt verwendet */ } else if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF)
   { ... } else { /* OFEX, unverändert */ }`. Bei anhängendem Override wird weder der
   Standard-Wrap-Pfad noch der OFEX-Sprung-Pfad erreicht — beide bleiben für den unveränderten
   Fall exakt wie vorher (identische Struktur zu Phase 5, da Phoenix V2 denselben
   16-Bit-Wrap-/Sprung-Formelsatz wie Somfy Telis verwendet).
2. `subghz_protocol_decoder_phoenix_v2_get_string()`: `cnt_is_available`/`cnt_length_bit`/
   `current_cnt` sowie `btn_is_available`/`current_btn`/`btn_length_bit` unbedingt direkt nach
   dem bestehenden `check_remote_controller()`-Aufruf gesetzt — exakt Unleasheds eigene
   Anbindung, byte-für-byte übernommen (siehe "Verifikation Phase 6"). `btn_is_available` ist
   dabei `true`: Phoenix V2s Tastenwert (`instance->generic.btn`, gesetzt in
   `check_remote_controller()` aus dem entschlüsselten Datenfeld) ist unabhängig editierbar,
   dieselbe Festlegung, die Unleashed selbst trifft, wie bei KeeLoq/Princeton/Somfy Telis.

### Phase 7: CAME Atomo (fünftes Protokoll der Nachzieh-Liste)

**Was:** `came_atomo` ist bei Unleashed ebenfalls Mitglied der ursprünglich gezählten
Rolling-Code-Protokolle. Beim Diff meiner eigenen `came_atomo.c` gegen Unleasheds aktuellen
Standard-Branch (siehe "Quelle") zeigte sich der in Phase 6 angekündigte Gesamtdiff-Umfang von
403 Zeilen, davon wie bei Phoenix V2 ein erster, hier nicht relevanter Block aus Unleasheds
separatem `common.c`-Encoder-/Decoder-Basis-Umbau (`#include "common.h"`,
`SUBGHZ_ASSERT_*_LAYOUT`, `alloc`/`free` über `subghz_protocol_*_common_alloc/_free`,
`get_hash_data`/`serialize` über die `common`-Infrastruktur statt eigener Funktionen) sowie ein
zweiter, hier ebenfalls nicht übernommener Block: ein zusätzlicher sechster/siebter Tastencode
(`0x5`/`0x6`, Wire-Nibble `0x0C`/`0x0E`) samt eines neuen vierten Header-Timing-Zweigs
(19k-µs-Variante für ein weiteres Lesegerätemodell, `TOPD4REN`) und einer neuen
`SUBGHZ_CUSTOM_BTN_RIGHT`-Tabelle für die Custom-Taste "Rechts" — beides eigenständige
Protokoll-/Hardware-Erweiterungen, die meine eigene `came_atomo.c` (vier Tasten, zwei bekannte
Header-Timings) nicht kennt und für die der Override-Anschluss keine Voraussetzung ist. Anders
als bei Phoenix V2 ist der dritte Block — Unleasheds `cnt_need_override`/`bypass`-Mechanismus —
hier **kein** Fremdfeature: meine eigene `came_atomo.c` hat bereits ein eigenständiges,
vierstufiges `came_atomo_counter_mode`-Feld (Modus 0: Standard/OFEX wie bei den anderen
Nachzieh-Protokollen; Modus 1: feste Wertetabelle `0000`/`0001`/`FFFE`/`FFFF`; Modus 2: feste
Wertetabelle `0x807B`/`0x807C`/`0x007B`/`0x007C`; Modus 3: Zähler-Freeze, kein Inkrement) —
identisch zu Unleasheds eigenem (per Diff bestätigt: alle vier Modus-Zweige sind bei beiden
Seiten byte-identisch, keine Abweichung außer im Modus-0-Zweig). Da die Modi 1-3 den Override
nie konsultieren (ihre Wertetabellen hängen nur vom aktuellen Zählerstand ab, nicht von
`subghz_block_generic_global`), würde ein Zähler- oder Tasten-Edit in diesen drei Modi
stillschweigend wirkungslos bleiben, wenn nur Modus 0 den Override-Mechanismus bekäme — exakt
dasselbe Problem, das Unleasheds eigenes `bypass`-Flag löst, und anders als bei Phoenix V2s
"CounterMode" (Phase 6, dort bewusst nicht mitportiert, weil mein Code dafür keine Entsprechung
hatte) hat mein Code hier eine 1:1-Entsprechung zu dem, was `bypass` schützt — das Flag wird
daher mitportiert.

Ich liefere additiv, genau drei Anschlussstellen in meiner eigenen `came_atomo.c`:
1. Eine neue Modul-globale `static bool bypass = false;`-Variable direkt nach der bestehenden
   `came_atomo_counter_mode`-Variable (exakt Unleasheds eigene Deklaration, byte-für-byte
   übernommen).
2. `subghz_protocol_encoder_came_atomo_get_upload()`: direkt vor der bestehenden
   `if(came_atomo_counter_mode == 0) { ... }`-Verzweigung zwei neue Zeilen, die `bypass` setzen,
   sobald ein Zähler- oder Tasten-Override anhängt (`if(subghz_block_generic_global.cnt_need_override
   || subghz_block_generic_global.btn_need_override) bypass = true;`); die Bedingung der
   Verzweigung selbst wird um `|| bypass` erweitert (`if(came_atomo_counter_mode == 0 ||
   bypass)`), ebenso die innere OFEX-Bedingung (`if(furi_hal_subghz_get_rolling_counter_mult()
   != -0x7FFFFFFF || bypass)`), direkt gefolgt von `bypass = false;` und einem
   `subghz_block_generic_global_counter_override_get(&instance->generic.cnt)`-Aufruf, der bei
   Erfolg den bisherigen Standard-Inkrement-Block überspringt — die Modi 1/2/3 selbst bleiben
   textidentisch unverändert (sie laufen nur dann nicht, wenn `bypass` sie für einen Durchlauf
   überspringt). Dieselbe Reihenfolge und dieselben Bedingungen wie in Unleasheds eigenem Diff,
   byte-für-byte übernommen.
3. Dieselbe Funktion, unmittelbar vor dem bestehenden Kommentar `//Send header` (nach der
   bestehenden Tastencode-Zuordnungstabelle `if(btn == 0x1) { btn = 0x0; } else if(...) ...`): ein
   `subghz_block_generic_global_button_override_get(&btn)`-Aufruf. Anders als bei
   KeeLoq/Princeton/Secplus v1/Somfy Telis/Phoenix V2 sitzt dieser Aufruf hier **nach**, nicht
   vor der bestehenden Tastencode-Zuordnung — er überschreibt also den bereits auf den
   gesendeten Wire-Nibble-Wert (`0x0`/`0x2`/`0x4`/`0x6`) abgebildeten Code, nicht die rohe,
   benutzerfreundliche Tasten-Id (1-4). Das ist exakt Unleasheds eigene Platzierung, nicht meine
   Entscheidung — siehe "Risiko" für die daraus folgende Einschränkung am Edit-Menü.
4. `subghz_protocol_decoder_came_atomo_get_string()`: `cnt_is_available`/`cnt_length_bit`/
   `current_cnt` sowie `btn_is_available`/`current_btn`/`btn_length_bit` unbedingt direkt nach
   dem bestehenden `subghz_protocol_came_atomo_remote_controller()`-Aufruf gesetzt — exakt
   Unleasheds eigene Anbindung, byte-für-byte übernommen (siehe "Verifikation Phase 7").
   `btn_is_available` ist dabei `true`, `current_btn` ist jedoch `instance->generic.btn` — die
   benutzerfreundliche, dekodierte Tasten-Id (1-4), **nicht** der Wire-Nibble-Wert, den Punkt 3
   überschreibt (siehe "Risiko").

### Phase 8: Alutech AT-4N (sechstes Protokoll der Nachzieh-Liste)

**Was:** `alutech_at_4n` ist bei Unleashed ebenfalls Mitglied der ursprünglich gezählten
Rolling-Code-Protokolle. Beim Diff meiner eigenen `alutech_at_4n.c` gegen Unleasheds aktuellen
Standard-Branch (siehe "Quelle") zeigte sich der in "Update (Phase 7)" angekündigte
Gesamtdiff-Umfang von 404 Zeilen, davon wie bei Phoenix V2/CAME Atomo ein erster, hier nicht
relevanter Block aus Unleasheds separatem `common.c`-Encoder-/Decoder-Basis-Umbau
(`#include "common.h"`, `SUBGHZ_ASSERT_*_LAYOUT`, `alloc`/`free` über
`subghz_protocol_*_common_alloc/_free`, `get_hash_data`/`stop`/`yield` über die
`common`-Infrastruktur statt eigener Funktionen) sowie ein zweiter, hier ebenfalls nicht
übernommener Block: Unleashed liest die sechs Rainbow-Table-Werte nicht mehr direkt aus der
Keystore-Datei (`subghz_protocol_alutech_at_4n_get_magic_data_in_file()`, ein
`subghz_keystore_raw_get_data()`-Aufruf pro Wert), sondern lädt die vollen 32 Byte einmal in
einen RAM-Puffer und liest daraus (`subghz_protocol_alutech_at_4n_get_magic_data_from_buffer()`)
— eine reine Performance-/Zugriffsmuster-Änderung an der Rainbow-Table-Logik, die mit dem
Override-Mechanismus nichts zu tun hat und die meine eigene, weiterhin dateibasierte Lese-Logik
unverändert lässt. Der dritte Block — Unleasheds `cnt_need_override`/`bypass`-Mechanismus — ist
wie bei CAME Atomo (Phase 7) **kein** Fremdfeature: meine eigene `alutech_at_4n.c` hat bereits
ein eigenständiges, dreistufiges `alutech_at4n_counter_mode`-Feld (Modus 0: Standard/OFEX wie bei
den anderen Nachzieh-Protokollen; Modus 1: feste Wertetabelle `0000`/`0001`/`FFFE`/`FFFF`
— byte-identisch zu CAME Atomos eigenem Modus 1; Modus 2: Zähler `0x0000`..`0x0005` mit Wrap
zurück auf `0x0000`) — identisch zu Unleasheds eigenem (per Diff bestätigt: beide Modus-Zweige
sind bei beiden Seiten byte-identisch, keine Abweichung außer im Modus-0-Zweig). Eine Stufe
weniger als CAME Atomo (kein Modus-3-Freeze-Äquivalent) — dafür aber auch kein neues
"CounterMode"-Fremdfeature wie bei Phoenix V2 zu entscheiden. Da die Modi 1/2 den Override nie
konsultieren, würde ein Zähler- oder Tasten-Edit in diesen beiden Modi stillschweigend
wirkungslos bleiben, wenn nur Modus 0 den Override-Mechanismus bekäme — exakt dasselbe Problem,
das Unleasheds eigenes `bypass`-Flag löst, und das Flag wird daher mitportiert.

Eine Besonderheit gegenüber CAME Atomo: dort prüft die Bypass-Aufbau-Zeile in der Zähler-Funktion
beide Flags (`cnt_need_override || btn_need_override`). Bei Alutech AT-4N prüft die
entsprechende Zeile in `subghz_protocol_alutech_at_4n_gen_data()` nur `cnt_need_override` — das
ist exakt Unleasheds eigener Diff, keine eigene Vereinfachung. Der Grund, warum ein reiner
Tasten-Override trotzdem denselben Effekt hat: der Tasten-Override-Aufruf sitzt in
`subghz_protocol_encoder_alutech_at_4n_get_upload()`, direkt nach der bestehenden
`btn = subghz_protocol_alutech_at_4n_get_btn_code();`-Zeile — und damit zeitlich **vor** dem
Aufruf von `gen_data()` in derselben Funktion. Setzt der Tasten-Override-Aufruf `bypass` bei
Erfolg selbst auf `true`, ist die Variable bereits gesetzt, wenn `gen_data()` anschließend
aufgerufen wird, unabhängig davon, ob zusätzlich ein Zähler-Override anhängt oder nicht (siehe
Szenario E in "Verifikation Phase 8").

Ich liefere additiv, genau drei Anschlussstellen in meiner eigenen `alutech_at_4n.c`:
1. Eine neue Modul-globale `static bool bypass = false;`-Variable direkt nach der bestehenden
   `alutech_at4n_counter_mode`-Variable (exakt Unleasheds eigene Deklaration, byte-für-byte
   übernommen).
2. `subghz_protocol_alutech_at_4n_gen_data()`: direkt vor der bestehenden
   `if(alutech_at4n_counter_mode == 0) { ... }`-Verzweigung zwei neue Zeilen, die `bypass`
   setzen, sobald ein Zähler-Override anhängt (`if(subghz_block_generic_global.cnt_need_override)
   bypass = true;`); die Bedingung der Verzweigung selbst wird um `|| bypass` erweitert
   (`if((alutech_at4n_counter_mode == 0) || bypass)`), ebenso die innere OFEX-Bedingung
   (`if((furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) || bypass)`), direkt gefolgt
   von `bypass = false;` und einem
   `subghz_block_generic_global_counter_override_get(&instance->generic.cnt)`-Aufruf, der bei
   Erfolg den bisherigen Standard-Inkrement-Block überspringt — die Modi 1/2 selbst bleiben
   textidentisch unverändert (sie laufen nur dann nicht, wenn `bypass` sie für einen Durchlauf
   überspringt). Dieselbe Reihenfolge und dieselben Bedingungen wie in Unleasheds eigenem Diff,
   byte-für-byte übernommen.
3. `subghz_protocol_encoder_alutech_at_4n_get_upload()`: direkt nach der bestehenden
   `btn = subghz_protocol_alutech_at_4n_get_btn_code();`-Zeile ein eigenständiger
   `subghz_block_generic_global_button_override_get(&btn)`-Aufruf, der bei Erfolg `bypass` auf
   `true` setzt und einen `FURI_LOG_D()`-Log schreibt — anders als bei CAME Atomo (dort nach einer
   bestehenden Tastencode-Zuordnungstabelle) gibt es hier keine zusätzliche Wire-Nibble-Abbildung
   zu beachten: der Override wirkt direkt auf den bereits über die Custom-Button-Logik
   aufgelösten Tastencode (`0x11`/`0x22`/`0xFF`/`0x44`/`0x33`), bevor dieser Wert an `gen_data()`
   übergeben wird. Exakt Unleasheds eigene Platzierung.
4. `subghz_protocol_decoder_alutech_at_4n_get_string()`: `cnt_is_available`/`cnt_length_bit`/
   `current_cnt` sowie `btn_is_available`/`current_btn`/`btn_length_bit` unbedingt direkt nach
   der bestehenden `code_found_lo`-Berechnung gesetzt (`btn_length_bit` ist hier `8`, ein volles
   Byte, nicht wie bei CAME Atomo ein 4-Bit-Nibble, da die fünf realen Tastencodes dieses
   Protokolls volle Bytewerte sind) — exakt Unleasheds eigene Anbindung, byte-für-byte
   übernommen (siehe "Verifikation Phase 8"). `current_btn` ist `instance->generic.btn` — der
   zuletzt dekodierte Tastencode der Empfängerseite, nicht der von Punkt 3 überschriebene
   Encoder-Parameter (gleiche Unterscheidung wie bei CAME Atomo, siehe "Risiko").

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
- **CAME Atomo (Phase 7): Anzeige- und Edit-Wert für die Taste sind nicht derselbe Wertebereich
  — von Unleashed so übernommen, nicht selbst eingeführt:** `get_string()` zeigt `current_btn`
  als die dekodierte, benutzerfreundliche Tasten-Id (`instance->generic.btn`, Werte 1-4), aber
  der Override-Aufruf im Encoder sitzt *nach* der bestehenden Zuordnungstabelle und überschreibt
  damit direkt den bereits auf den Wire-Nibble-Wert abgebildeten Code (`0x0`/`0x2`/`0x4`/`0x6`
  statt 1-4). Ein künftiges Edit-Menü, das den angezeigten Wert (1-4) unverändert als neuen
  Override-Wert zurückschreibt, würde am Sender einen falschen, nicht zur Tabelle passenden
  Nibble-Wert erzeugen (z. B. Override-Wert `3` statt des korrekten `0x4` für die dritte Taste).
  Das ist exakt Unleasheds eigenes, unverändert übernommenes Verhalten (siehe "Phase 7:
  CAME Atomo", Punkt 3) — kein durch diesen Port eingeführter Fehler, aber eine Falle für die
  lokale Edit-Menü-Anbindung: der Override-Wert muss dort in Wire-Nibble-Kodierung
  (`0x0`/`0x2`/`0x4`/`0x6`) eingegeben/umgerechnet werden, nicht als 1-4. Im Host-Repro
  (Szenario A) explizit mit einem Wire-Nibble-Wert geprüft, nicht mit einer Tasten-Id.
- **Alutech AT-4N (Phase 8): kein Pendant zur CAME-Atomo-Falle oben — Anzeige- und Edit-Wert
  sind hier derselbe Wertebereich:** `get_string()` zeigt `current_btn` als
  `instance->generic.btn`, denselben vollen Bytewert (`0x11`/`0x22`/`0xFF`/`0x44`/`0x33`), den
  auch der Override-Aufruf im Encoder überschreibt — anders als bei CAME Atomo gibt es hier
  keine zusätzliche Wire-Nibble-Abbildung zwischen Anzeige und Override-Zielwert, weil dieses
  Protokoll keine entsprechende Zuordnungstabelle hat. Ein künftiges Edit-Menü kann den
  angezeigten Wert hier unverändert als neuen Override-Wert zurückschreiben, ohne Umrechnung.
  Per Diff gegen Unleasheds eigenen, unveränderten Code bestätigt (siehe "Phase 8: Alutech
  AT-4N", Punkt 3/4) — kein eigener Fund, sondern eine bewusst dokumentierte Abgrenzung zur
  Phase-7-Falle, damit eine künftige Edit-Menü-Anbindung nicht fälschlich dieselbe Umrechnung
  für beide Protokolle annimmt.

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

### Verifikation Phase 6 (Phoenix V2)

- **Patch additiv, byte-identisch:** `connective-patches/phoenix_v2_c.patch` per `patch -p1
  --dry-run` sowie real per `patch -p1` gegen zwei frische, unabhängige Zweitkopien meiner
  eigenen `phoenix_v2.c` (`G4MEOVER18/G4MEOVER-FW`, `main`, Commit
  `b359f6fc32f282ce7217dd2d48a4e3478771852e`, per frischem `git clone` zu Sitzungsbeginn
  bestätigt unverändert aktueller `HEAD`-Stand) angewendet — beide Ergebnisse per `md5sum`
  byte-identisch zueinander und zur mitgelieferten Zieldatei bestätigt. `git diff --stat` nach
  dem Anwenden: **20 Zeilen Hinzufügung, 1 Löschung** (dieselbe Art Zeilenverschiebung wie bei
  Secplus v1/Somfy Telis: die bestehende
  `if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {`-Zeile wird durch die um den
  neuen Override-Zweig erweiterte Zeile
  `} else if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {` ersetzt — keine
  inhaltliche Löschung, reine Verschiebung in dieselbe Bedingung).
- **Funktionsverhalten
  (`firmware-mining/repro-subghz-block-generic-global-override-port-phase6/`):** `repro.c`
  verlinkt gegen die tatsächliche, unveränderte Phase-1-`generic.c`; die zwei neuen
  Anschlussstellen selbst sind per `sed`-Zeilenbereich byte-identisch aus der mitgelieferten,
  bereits gepatchten `phoenix_v2.c` in eigene `.inc`-Fragmente extrahiert und in minimalen
  Wrapper-Funktionen eingebunden (Zeilenbereiche in den Kommentaren von `repro.c`, mechanisch
  reproduzierbar: `sed 256,280p` für den Tasten-Override- und den Zähler-Override-Zweig plus
  die zwei unveränderten Geschwister-Zweige, `sed 602,610p` für den `get_string()`-Push-Block).
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
     danach ein Override-Set/Get-Durchlauf im `get_upload()`-Fragment unter Wiederverwendung
     dieser Längen — kein UB, korrekt maskiertes Ergebnis.
  - Gebaut und ausgeführt mit `gcc -std=c17 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
    -fno-sanitize-recover=undefined`: **25 Prüfungen bestanden**, identischer Output beider
    Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde,
    `g_unexpected_stub_calls` bleibt bei 0.
  - Build-Befehl (aus `firmware-mining/repro-subghz-block-generic-global-override-port-phase6/`):
    `gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined
    -I ../subghz-block-generic-global-override-port -I mocks/lib/subghz/blocks -I mocks
    ../subghz-block-generic-global-override-port/lib/subghz/blocks/generic.c stubs.c repro.c -o repro`

### Verifikation Phase 7 (CAME Atomo)

- **Patch additiv, byte-identisch:** `connective-patches/came_atomo_c.patch` per `patch -p1
  --dry-run` sowie real per `patch -p1` gegen zwei frische, unabhängige Zweitkopien meiner
  eigenen `came_atomo.c` (`G4MEOVER18/G4MEOVER-FW`, `main`, Commit
  `b359f6fc32f282ce7217dd2d48a4e3478771852e`, per frischem `git clone` zu Sitzungsbeginn geholt,
  unverändert zu Phase 1-6) angewendet — beide Ergebnisse per `md5sum` byte-identisch zueinander
  und zur mitgelieferten Zieldatei bestätigt. `git diff --stat` nach dem Anwenden: **38 Zeilen
  Hinzufügung, 6 Löschungen** (dieselbe Art Zeilenverschiebung wie bei Secplus v1/Somfy
  Telis/Phoenix V2, hier etwas größer, weil der ganze bestehende Standard-Block eine Ebene
  tiefer in einen neuen `if(!subghz_block_generic_global_counter_override_get(...))`-Zweig
  rutscht: die bestehenden sechs Zeilen `if(came_atomo_counter_mode == 0) {` /
  `if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF) {` /
  `if((instance->generic.cnt + ...) > 0xFFFF) {` / `instance->generic.cnt = 0;` / `} else {` /
  `instance->generic.cnt += ...;` werden durch ihre um `|| bypass` bzw. die neue
  Override-Abfrage erweiterten/eingerückten Gegenstücke ersetzt — keine inhaltliche Löschung,
  reine Erweiterung derselben sechs Bedingungen/Zuweisungen).
- **Funktionsverhalten
  (`firmware-mining/repro-subghz-block-generic-global-override-port-phase7/`):** `repro.c`
  verlinkt gegen die tatsächliche, unveränderte Phase-1-`generic.c`; die drei neuen
  Anschlussstellen sind per `sed`-Zeilenbereich byte-identisch aus der mitgelieferten, bereits
  gepatchten `came_atomo.c` in eigene `.inc`-Fragmente extrahiert und in minimalen
  Wrapper-Funktionen eingebunden (Zeilenbereiche in den Kommentaren von `repro.c`, mechanisch
  reproduzierbar: `sed 197,249p` für den `bypass`-Aufbau plus die vollständige, unveränderte
  Modus-0-bis-3-Kette, `sed 258,271p` für die Tastencode-Zuordnung plus den Tasten-Override,
  `sed 847,855p` für den `get_string()`-Push-Block). Anders als bei Phase 3-6 liegen Tasten- und
  Zähler-Override hier nicht im selben Codeblock — deshalb zwei getrennte Fragmente statt eines
  kombinierten. Geprüft (35 Prüfungen):
  1. Tasten-Override verbraucht sich einmalig, maskiert auf das reale `btn_length_bit` (4) —
     ein außerhalb des 4-Bit-Bereichs liegender Override-Wert (`0x1F`) belegt dabei zugleich die
     Masken-Formel für diese konkrete Protokoll-Bitbreite (`0x1F & 0xF == 0xF`). Geprüft mit
     einem bereits auf den Wire-Nibble-Wert abgebildeten Eingabewert (`0x4`), nicht mit einer
     rohen Tasten-Id — siehe "Risiko" zur Platzierung nach der Zuordnungstabelle.
  2. Ohne anhängenden Tasten-Override bleibt die bestehende Zuordnungstabelle für alle vier
     realen Tastencodes unverändert (`0x1→0x0`, `0x2→0x2`, `0x3→0x4`, `0x4→0x6`, Regression).
  3. Zähler-Override in Modus 0 verwendet den überschriebenen, maskierten Wert verbatim,
     überspringt den Standard-Inkrement-Block vollständig und setzt `bypass` nach Verbrauch
     wieder auf `false` — ein außerhalb des 16-Bit-Bereichs liegender Override-Wert (`0x1ABCD`)
     belegt dabei zugleich die Masken-Formel für den realen 16-Bit-Zähler dieses Protokolls
     (`0x1ABCD & 0xFFFF == 0xABCD`). Override verbraucht sich einmalig, der nächste Aufruf ohne
     neuen Override läuft wieder über den Standard-Inkrement.
  4. Zähler-Override in Modus 0 mit gleichzeitig aktivem OFEX-Sentinel
     (`furi_hal_subghz_get_rolling_counter_mult() == -0x7FFFFFFF`) überspringt auch den
     OFEX-Sprung-Pfad vollständig — Beleg, dass der neue Zweig strikt vor der OFEX-/Standard-
     Verzweigung läuft, nicht nur vor deren Standard-Hälfte.
  5. **Der eigentliche Zweck von `bypass` für dieses Protokoll:** Zähler-Override mit
     `came_atomo_counter_mode` auf 1, 2 bzw. 3 gesetzt — ohne `bypass` würden die jeweiligen
     festen Wertetabellen (Modus 1/2) bzw. das Freeze-Verhalten (Modus 3) laufen und den Override
     vollständig ignorieren; mit `bypass` erzwingt die erweiterte Bedingung
     `came_atomo_counter_mode == 0 || bypass` stattdessen den Standard-Zweig, der Override setzt
     sich in allen drei Fällen durch. Für jeden der drei Modi einzeln geprüft.
  6. Ohne anhängenden Override laufen die Modus-1-, Modus-2- und Modus-3-Zweige unverändert
     weiter (Regression) — `bypass` bleibt `false`, wenn kein Override anhängt.
  7. Ohne anhängenden Override läuft die bestehende Standard-Wrap-Logik (Modus 0) unverändert —
     sowohl der einfache Inkrement-Fall als auch der Wrap-auf-0-Fall bei Überschreiten von
     `0xFFFF`.
  8. Ohne anhängenden Override läuft die bestehende OFEX-Logik unverändert — alle drei real
     vorhandenen Verzweigungen: Wrap-auf-0 bei `cnt == 0xFFFF`, Sprung-auf-`0xFFFE` für jeden
     sonstigen Wert `>= 1`, sowie die beiden vom Sprung bewusst ausgenommenen
     Rand-Inkrement-Fälle `cnt == 0` und `cnt == 0xFFFE`.
  9. `get_string()`s Push-Block setzt `cnt_is_available` unbedingt `true`/`cnt_length_bit`
     unbedingt `16`/`current_cnt` korrekt sowie `btn_is_available` unbedingt `true`/
     `current_btn`/`btn_length_bit` unbedingt `4` korrekt, für zwei unterschiedliche
     Zähler-/Tastenwerte (inkl. des Randfalls beide `0`).
  10. Die in Phase 1 dokumentierte Reihenfolge-Abhängigkeit (`cnt_length_bit`/`btn_length_bit`
      vor dem ersten `_override_get()`-Aufruf) für die realen Bitbreiten dieses Protokolls
      (16/4) explizit durchexerziert: `get_string()`-Fragment zuerst ausgeführt (setzt beide
      Längen), danach ein Override-Set/Get-Durchlauf über beide `get_upload()`-Fragmente in der
      realen Aufrufreihenfolge (Zähler-Block vor Tasten-Block) unter Wiederverwendung dieser
      Längen — kein UB, korrekt maskiertes Ergebnis.
  - Gebaut und ausgeführt mit `gcc -std=c17 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
    -fno-sanitize-recover=undefined`: **35 Prüfungen bestanden**, identischer Output beider
    Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde,
    `g_unexpected_stub_calls` bleibt bei 0.
  - Build-Befehl (aus `firmware-mining/repro-subghz-block-generic-global-override-port-phase7/`):
    `gcc -std=c17 -Wall -Wextra -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined
    -I ../subghz-block-generic-global-override-port -I mocks/lib/subghz/blocks -I mocks
    ../subghz-block-generic-global-override-port/lib/subghz/blocks/generic.c stubs.c repro.c -o repro`

### Verifikation Phase 8 (Alutech AT-4N)

- **Patch additiv, byte-identisch:** `connective-patches/alutech_at_4n_c.patch` per `patch -p1
  --dry-run` sowie real per `patch -p1` gegen zwei frische, unabhängige Zweitkopien meiner
  eigenen `alutech_at_4n.c` (`G4MEOVER18/G4MEOVER-FW`, `main`, Commit
  `b359f6fc32f282ce7217dd2d48a4e3478771852e`, per frischem `git clone` zu Sitzungsbeginn geholt,
  unverändert zu Phase 1-7) angewendet — beide Ergebnisse per `md5sum` byte-identisch zueinander
  und zur mitgelieferten Zieldatei bestätigt. `git diff --stat` nach dem Anwenden: **39 Zeilen
  Hinzufügung, 6 Löschungen** (dieselbe Art Zeilenverschiebung wie bei CAME Atomo: der bestehende
  Standard-Block rutscht eine Ebene tiefer in einen neuen
  `if(!subghz_block_generic_global_counter_override_get(...))`-Zweig — keine inhaltliche
  Löschung, reine Erweiterung derselben Bedingungen/Zuweisungen).
- **Funktionsverhalten
  (`firmware-mining/repro-subghz-block-generic-global-override-port-phase8/`):** `repro.c`
  verlinkt gegen die tatsächliche, unveränderte Phase-1-`generic.c`; die drei neuen
  Anschlussstellen sind per `sed`-Zeilenbereich byte-identisch aus der mitgelieferten, bereits
  gepatchten `alutech_at_4n.c` in eigene `.inc`-Fragmente extrahiert und in minimalen
  Wrapper-Funktionen eingebunden (Zeilenbereiche in den Kommentaren von `repro.c`, mechanisch
  reproduzierbar: `sed 285,332p` für den `bypass`-Aufbau plus die vollständige, unveränderte
  Modus-0-bis-2-Kette, `sed 399,403p` für den eigenständigen Tasten-Override-Aufruf,
  `sed 898,906p` für den `get_string()`-Push-Block). Wie bei Phase 7 liegen Tasten- und
  Zähler-Override hier nicht im selben Codeblock — deshalb zwei getrennte Fragmente statt eines
  kombinierten; anders als bei Phase 7 hat der Tasten-Override hier aber keine vorgelagerte
  Wire-Nibble-Zuordnungstabelle zu reproduzieren, da er auf dem bereits von der
  Custom-Button-Logik aufgelösten Tastencode wirkt. Geprüft (39 Prüfungen):
  1. Tasten-Override verbraucht sich einmalig, voller Bytewert (`btn_length_bit` ist 8, ein
     volles Byte, nicht wie bei CAME Atomo ein 4-Bit-Nibble — die Masken-Formel ist hier eine
     Identitätsmaske über den gesamten `uint8_t`-Bereich, es gibt daher keinen außerhalb des
     Bereichs liegenden Wert, mit dem sich eine Kürzung zusätzlich belegen ließe: der
     Override-Setter selbst nimmt bereits `uint8_t` an). Zusätzlich geprüft: ein erfolgreicher
     Tasten-Override setzt `bypass` auf `true` — anders als bei CAME Atomo, wo nur der
     Zähler-Bypass-Aufbau dies direkt tut (siehe "Phase 8: Alutech AT-4N").
  2. Ohne anhängenden Tasten-Override bleibt der bestehende, von der Custom-Button-Logik
     aufgelöste Tastencode für alle fünf realen Tastencodes unverändert (`0x11`, `0x22`, `0xFF`,
     `0x44`, `0x33`, Regression), `bypass` bleibt dabei `false`.
  3. Zähler-Override in Modus 0 verwendet den überschriebenen, maskierten Wert verbatim,
     überspringt den Standard-Inkrement-Block vollständig und setzt `bypass` nach Verbrauch
     wieder auf `false` — ein außerhalb des 16-Bit-Bereichs liegender Override-Wert (`0x1ABCD`)
     belegt dabei zugleich die Masken-Formel für den realen 16-Bit-Zähler dieses Protokolls
     (`0x1ABCD & 0xFFFF == 0xABCD`). Override verbraucht sich einmalig, der nächste Aufruf ohne
     neuen Override läuft wieder über den Standard-Inkrement.
  4. Zähler-Override in Modus 0 mit gleichzeitig aktivem OFEX-Sentinel
     (`furi_hal_subghz_get_rolling_counter_mult() == -0x7FFFFFFF`) überspringt auch den
     OFEX-Sprung-Pfad vollständig — Beleg, dass der neue Zweig strikt vor der OFEX-/Standard-
     Verzweigung läuft, nicht nur vor deren Standard-Hälfte.
  5. **Der eigentliche Zweck von `bypass` für dieses Protokoll, inklusive der protokollspezifischen
     Besonderheit:** zwei Teilfälle. Erstens ein reiner Zähler-Override mit
     `alutech_at4n_counter_mode` auf 2 — ohne `bypass` würde Modus 2s feste Inkrement-/Wrap-Tabelle
     laufen und den Override vollständig ignorieren; mit `bypass` (hier über
     `cnt_need_override` direkt gesetzt) erzwingt die erweiterte Bedingung
     `(alutech_at4n_counter_mode == 0) || bypass` stattdessen den Standard-Zweig. Zweitens — die
     fragmentübergreifende Besonderheit gegenüber CAME Atomo — ein reiner Tasten-Override
     (kein Zähler-Override) mit `alutech_at4n_counter_mode` auf 1: der Tasten-Fragment-Aufruf
     setzt `bypass` bei Erfolg selbst, und genau dieser bereits gesetzte Zustand erzwingt den
     Standard-Zweig im unmittelbar darauf folgenden Zähler-Fragment-Aufruf — exakt die reale
     Aufrufreihenfolge in `get_upload()` (Tasten-Override vor `gen_data()`). Ohne diesen
     Seiteneffekt bliebe ein reiner Tasten-Edit in Modus 1/2 wirkungslos für den Zähler, obwohl
     `cnt_need_override` gar nicht gesetzt ist.
  6. Ohne anhängenden Override laufen die Modus-1- und Modus-2-Zweige unverändert weiter
     (Regression) — `bypass` bleibt `false`, wenn kein Override anhängt (weder Zähler- noch
     Tasten-seitig).
  7. Ohne anhängenden Override läuft die bestehende Standard-Wrap-Logik (Modus 0) unverändert —
     sowohl der einfache Inkrement-Fall als auch der Wrap-auf-0-Fall bei Überschreiten von
     `0xFFFF`.
  8. Ohne anhängenden Override läuft die bestehende OFEX-Logik unverändert — alle drei real
     vorhandenen Verzweigungen: Wrap-auf-0 bei `cnt == 0xFFFF`, Sprung-auf-`0xFFFE` für jeden
     sonstigen Wert `>= 1`, sowie die beiden vom Sprung bewusst ausgenommenen
     Rand-Inkrement-Fälle `cnt == 0` und `cnt == 0xFFFE`.
  9. `get_string()`s Push-Block setzt `cnt_is_available` unbedingt `true`/`cnt_length_bit`
     unbedingt `16`/`current_cnt` korrekt sowie `btn_is_available` unbedingt `true`/
     `current_btn`/`btn_length_bit` unbedingt `8` korrekt, für zwei unterschiedliche
     Zähler-/Tastenwerte (inkl. des Randfalls beide `0`).
  10. Die in Phase 1 dokumentierte Reihenfolge-Abhängigkeit (`cnt_length_bit`/`btn_length_bit`
      vor dem ersten `_override_get()`-Aufruf) für die realen Bitbreiten dieses Protokolls
      (16/8) explizit durchexerziert: `get_string()`-Fragment zuerst ausgeführt (setzt beide
      Längen), danach ein Override-Set/Get-Durchlauf über beide `get_upload()`/`gen_data()`-
      Fragmente in der realen Aufrufreihenfolge (Tasten-Fragment vor Zähler-Fragment) unter
      Wiederverwendung dieser Längen — kein UB, korrekt maskiertes Ergebnis.
  - Gebaut und ausgeführt mit `gcc -std=c17 -Wall -Wextra -O0`/`-O1 -fsanitize=address,undefined
    -fno-sanitize-recover=undefined`: **39 Prüfungen bestanden**, identischer Output beider
    Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde,
    `g_unexpected_stub_calls` bleibt bei 0.
  - Build-Befehl (aus `firmware-mining/repro-subghz-block-generic-global-override-port-phase8/`):
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
   Override nur als vorangestellter Zweig davor eingefügt. Weiterhin offen nach Phase 5: die
   übrigen 8 der tatsächlich existierenden Nachzieh-Kandidaten (FAAC SLH, Secplus v2,
   Alutech AT-4N, Phoenix V2, Nice FloR-S, CAME Atomo, KingGates Stylo4K, Somfy Keytis).
6. **Phase 6 (geliefert, diese Sitzung — Phoenix V2):** viertes Nachzieh-Protokoll angeschlossen,
   drittes mit tatsächlicher Zähler-Hälfte. Gesamtdiff mit 324 Zeilen wie in Phase 5 angekündigt
   der nächstkleinste der sechs verglichenen Kandidaten, enthielt zusätzlich zu Unleasheds
   separatem `common.c`-Umbau ein eigenständiges, hier bewusst nicht mitportiertes
   "CounterMode"-Feature (siehe "Update (Phase 6)" und "Phase 6: Phoenix V2") — nach Abzug
   beider Blöcke dasselbe OFEX-Sentinel-Muster mit identischem 16-Bit-Zählerbereich und
   identischen Wrap-/Sprung-Formeln wie Somfy Telis; die bestehende Standard-/OFEX-Logik bleibt
   dabei vollständig erhalten, der Override nur als vorangestellter Zweig davor eingefügt.
   Weiterhin offen: die übrigen 7 der tatsächlich existierenden Nachzieh-Kandidaten (FAAC SLH,
   Secplus v2, Alutech AT-4N, Nice FloR-S, CAME Atomo, KingGates Stylo4K, Somfy Keytis —
   gleiches Muster wie KeeLoq/Secplus v1/Somfy Telis/Phoenix V2, siehe "Betroffene Dateien" —
   pro Protokoll eigener Diff gegen Unleashed nötig, da bei KeeLoq und Phoenix V2 bereits
   erhebliche strukturelle Abweichungen bzw. zusätzliche Seitenfeatures gegenüber der aktuellen
   Unleashed-Version bestanden, und bei den auf `common.c` umgebauten Protokollen wie FAAC SLH
   ein deutlich größerer Diff als bei KeeLoq/Princeton/Secplus v1/Somfy Telis/Phoenix V2 zu
   erwarten ist), dann `endless_tx` an die eigene SubGhz-Transmitter-App anschließen (Hold-OK =
   Dauersenden statt fester Wiederholzahl).
7. **Phase 7 (geliefert, diese Sitzung — CAME Atomo):** fünftes Nachzieh-Protokoll angeschlossen,
   viertes mit tatsächlicher Zähler-Hälfte. Gesamtdiff mit 403 Zeilen wie in Phase 6 angekündigt
   der kleinste der sieben verglichenen Kandidaten. Anders als bei Phoenix V2 ist der
   `cnt_need_override`/`bypass`-Mechanismus hier mitportiert, da mein eigenes
   `came_atomo_counter_mode`-Feature (Modi 0-3) 1:1 Unleasheds eigenem entspricht und die Modi
   1-3 den Override sonst stillschweigend ignorieren würden (siehe "Phase 7: CAME Atomo"); die
   bestehenden vier Modus-Zweige selbst bleiben dabei vollständig unverändert. Zusätzlich, hier
   bewusst nicht mitportiert: Unleasheds separater `common.c`-Umbau sowie ein neuer sechster/
   siebter Tastencode samt eines vierten Header-Timing-Zweigs für ein weiteres Lesegerätemodell.
   Weiterhin offen: die übrigen 6 der tatsächlich existierenden Nachzieh-Kandidaten (FAAC SLH,
   Secplus v2, Alutech AT-4N, Nice FloR-S, KingGates Stylo4K, Somfy Keytis — gleiches Muster wie
   KeeLoq/Secplus v1/Somfy Telis/Phoenix V2/CAME Atomo, siehe "Betroffene Dateien" — pro
   Protokoll eigener Diff gegen Unleashed nötig), dann `endless_tx` an die eigene
   SubGhz-Transmitter-App anschließen (Hold-OK = Dauersenden statt fester Wiederholzahl).
8. **SubGhz Remote App-Port (separater, deutlich größerer Port, weiterhin nicht Teil dieser
   Port-Skizze):** erst ab hier wird ein Port der App **SubGhz Remote** (siehe "Was") sinnvoll —
   ihr `subrem_tx_start_sub()` braucht exakt den hier gelieferten Mechanismus, um den Tastenwert
   vor dem Senden zu überschreiben. Verdient eine eigene Port-Vorschlag-Datei mit eigener
   Port-Skizze (Map-Datei-Format, Szenen/Views, die 684-Zeilen-`subghz_txrx`-Kopie der App gegen
   meine eigene, unabhängig weiterentwickelte
   `applications/main/subghz/helpers/subghz_txrx.c` abgleichen — bei einem ersten Blick bereits
   mehrere funktionale Unterschiede sichtbar, u. a. der OTG-Einschaltpfad: die App nutzt einen
   begrenzten `furi_hal_power_enable_otg()`-Retry mit 5 Versuchen, meine eigene Version einen
   einmaligen Aufruf über den `Power`-Service).
9. Bauen nach Phase 1: `ufbt` (bzw. `fbt firmware_all`) — reine Ergänzung, sollte ohne
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
   Rolling-Code-Empfänger (z. B. Somfy-Antrieb). Nach Phase 6 zusätzlich mit Phoenix V2 testbar:
   dieselbe Zähler-/Tasten-Prüfung wie bei Somfy Telis, am eigenen, autorisiert getesteten
   Rolling-Code-Empfänger. Nach Phase 7 zusätzlich mit CAME Atomo testbar: dieselbe
   Zähler-/Tasten-Prüfung wie bei Phoenix V2, zusätzlich für alle vier `CounterMode`-Einstellungen
   (0-3) am eigenen, autorisiert getesteten CAME-Atomo-Empfänger durchspielen — der Override muss
   in jedem der vier Modi durchschlagen, nicht nur in Modus 0 (siehe "Phase 7: CAME Atomo",
   `bypass`). Der Tasten-Override-Wert ist dabei in Wire-Nibble-Kodierung
   (`0x0`/`0x2`/`0x4`/`0x6`) einzugeben, nicht als 1-4 (siehe "Risiko"). Nach Phase 8 zusätzlich
   mit Alutech AT-4N testbar: dieselbe Zähler-/Tasten-Prüfung wie bei CAME Atomo, zusätzlich für
   alle drei `CounterMode`-Einstellungen (0-2) am eigenen, autorisiert getesteten
   Alutech-AT-4N-Empfänger durchspielen — der Override muss in jedem der drei Modi durchschlagen,
   nicht nur in Modus 0 (siehe "Phase 8: Alutech AT-4N", `bypass`), inklusive des Falls, dass nur
   ein Tasten- ohne Zähler-Override gesetzt ist (Modus 1/2 müssen auch dann auf den Standard-Zweig
   umspringen). Der Tasten-Override-Wert ist hier als voller, bereits aufgelöster Bytewert
   einzugeben (`0x11`/`0x22`/`0xFF`/`0x44`/`0x33`), nicht als Wire-Nibble oder Tasten-Id 1-4.

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
- Für Phase 7 per frischem Doppel-Clone zu Sitzungsbeginn erneut bestätigt: eigene Basis
  (`G4MEOVER18/G4MEOVER-FW`, `main`) weiterhin unverändert auf Commit
  `b359f6fc32f282ce7217dd2d48a4e3478771852e`; Unleashed, Standard-Branch, auf Commit
  `447fae5b133a211e5fb2da9e4e72956f8652e892` (2026-10-29, seit dem in Phase 6 verwendeten Stand
  `29c3dccda9c54bd06c23ffd45b33d77efaa18e72` nur ein reiner Submodul-Tag-Bump ohne inhaltliche
  Änderung an `came_atomo.c`/`generic.c`/`.h`, per `diff` gegeneinander bestätigt).
- Für Phase 8 per frischem Doppel-Clone zu Sitzungsbeginn erneut bestätigt: eigene Basis
  (`G4MEOVER18/G4MEOVER-FW`, `main`) weiterhin unverändert auf Commit
  `b359f6fc32f282ce7217dd2d48a4e3478771852e`; Unleashed, Standard-Branch, auf Commit
  `2cbad71c92e7482e7752f5ecb570c18b2d1a2c75` (2026-10-29): `lib/subghz/protocols/alutech_at_4n.c`
  zum vollständigen Diff gegen meine eigene Basis herangezogen, die drei Override-Anschlusspunkte
  (siehe "Phase 8: Alutech AT-4N") daraus übernommen; der übrige, hier nicht übernommene Teil des
  Diffs besteht aus zwei getrennten Blöcken — Unleasheds separatem `common.c`-Encoder-/
  Decoder-Basis-Umbau (wie bei Princeton/Secplus v1/Somfy Telis/Phoenix V2/CAME Atomo) sowie der
  auf einen 32-Byte-RAM-Puffer umgestellten Rainbow-Table-Ladefunktion (siehe
  "Phase 8: Alutech AT-4N") — beide hängen mit diesem Port nicht zusammen und werden nicht
  übernommen.
- (Phase 8) `G4MEOVER18/G4MEOVER-FW`, `main`, Commit `b359f6fc32f282ce7217dd2d48a4e3478771852e`
  (per frischem `git clone` zu Sitzungsbeginn Phase 8 erneut als unverändert aktueller
  `HEAD`-Stand bestätigt, identisch zu Phase 1-7): eigene Basis für
  `lib/subghz/protocols/alutech_at_4n.c`, Ausgangspunkt für
  `connective-patches/alutech_at_4n_c.patch`.
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
- (Phase 6) Unleashed-Firmware, Standard-Branch, Commit
  `29c3dccda9c54bd06c23ffd45b33d77efaa18e72` (2026-10-28, per frischem `git clone` zu
  Sitzungsbeginn geholt): `lib/subghz/protocols/phoenix_v2.c` zum vollständigen Diff gegen meine
  eigene Basis herangezogen, die zwei Override-Anschlusspunkte (siehe "Phase 6: Phoenix V2")
  daraus übernommen; der übrige, hier nicht übernommene Teil des Diffs besteht aus zwei
  getrennten Blöcken — Unleasheds separatem `common.c`-Encoder-/Decoder-Basis-Umbau (wie bei
  Princeton/Secplus v1/Somfy Telis) sowie Unleasheds eigenständigem "CounterMode"-Feature (siehe
  "Update (Phase 6)") — beide hängen mit diesem Port nicht zusammen und werden nicht
  übernommen.
- (Phase 6) `G4MEOVER18/G4MEOVER-FW`, `main`, Commit `b359f6fc32f282ce7217dd2d48a4e3478771852e`
  (per frischem `git clone` zu Sitzungsbeginn Phase 6 erneut als unverändert aktueller
  `HEAD`-Stand bestätigt, identisch zu Phase 1-5): eigene Basis für
  `lib/subghz/protocols/phoenix_v2.c`, Ausgangspunkt für `connective-patches/phoenix_v2_c.patch`.
