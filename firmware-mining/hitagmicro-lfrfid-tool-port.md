# LFRFID: ID82xx/Hitag-micro-Magic-Chip-Schreiber (`hitagmicro.c`/`.h`) geprüft und portiert

## Was

Aus derselben seit der Doorbell32-Sitzung offenen `unlshd-093`→`dev`-Restliste
(`keyfinder2`/`texkom`/`hitags`/`hitagmicro`, `keyfinder2` und `hitags` bereits portiert) den
in `hitags-lfrfid-magic-tag-port.md` als eigenen Rückstellpunkt benannten zweiten Kandidaten
derselben BPLM-Timing-Familie jetzt aufgegriffen: `lib/lfrfid/tools/hitagmicro.c`/`.h`
(327+57 Zeilen). Gegen den `dev`-Branch erneut geprüft — jetzt Commit `f62964c` (zuletzt
`af2e764`; der einzige Commit seither, der die beiden Dateien berührt, ist ein reiner
`fmt`-Lauf, Dateigröße unverändert bei 327+57 Zeilen — keine inhaltliche Änderung, keine
Wiederholung).

Das Modul ist, wie `hitags.c`, ein eigenständiges LFRFID-Werkzeug statt eines
Protokoll-Decoders, deckt aber eine andere, dritte Familie billiger "Magic"-Klon-Chips ab:
ID8265/ID8210 (China) bzw. H5/H5.5 (Russland) — am Chip selbst nicht zu unterscheiden außer
über ein festes, variantenspezifisches `LOGIN`-Passwort, deshalb eine einzige
`hitagmicro_write()`-Funktion für alle drei. Gleiche Nische und gleicher Nutzen wie bei den
bereits vorhandenen `tools/t5577.c`/`em4305.c`/`hitags.c`: eine eigene, bereits per LFRFID
ausgelesene EM4100-Zugangskarte auf einen eigenen Blanko-Tag klonen, um ein physisches
Zugangskontrollsystem mit der eigenen Kopie statt dem Original zu testen.

Technisch deutlich einfacher als `hitags.c`, weil **ausschließlich Schreiben, rein
open-loop** (wie `t5577_write()`/`em4305_write()`): kein UID-Lesen, keine
Anticollision-Logik, keine RX/Manchester-Dekodierung — der Flipper sendet nur, liest nie
zurück. Jeder der drei EM4100-Datenblöcke (`block0`/`block1`/`config`) wird in einer eigenen
stromzyklierten Sitzung geschrieben (UID-REQUEST → SYSINFO → Config lesen → LOGIN mit dem
variantenspezifischen Passwort → WRITE, zweimal gesendet, weil der 8265 laut Modulkommentar
nur unregelmäßig committet), danach sechs zusätzliche Stromzyklen, damit der Chip seine
TTF-Emission nach dem letzten Schreibvorgang sauber neu einrastet. `config` wird **zuletzt**
geschrieben — derselbe Grund wie bei `hitags.c`/Proxmark3: eine unterbrochene Sitzung soll den
Tag nie "umkonfiguriert, aber datenlos" zurücklassen.

Reine Bit-Logik ohne eigenen Selbsttest (anders als `hitags.c`, das sein eigenes
`hitags_selftest()` mitbringt) — CRC-16 (poly `0x1021`, init `0`, refin=false, refout=true,
zitiert aus Proxmark3s `common/crc16.c`), vier Frame-Builder und die
BPLM-Feldlücken-Modulation (`T_LOW`/`T_0`/`T_1`/`T_CODE_VIOLATION`, dieselbe Timing-Familie
wie `hitags.c`, aber reader→tag-Richtung statt dessen Manchester-Antwortdekodierung) — siehe
Host-Repro für die unabhängige Gegenprobe.

## Kompatibilitätsprüfung gegen `mntm-012`

Gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW` (Tag-Commit
`b359f6f`, `mntm-012`) geprüft:

- `targets/f7/furi_hal/furi_hal_rfid.h`: jede von `hitagmicro.c` aufgerufene Funktion
  (`furi_hal_rfid_tim_read_start/_pause/_continue/_stop`, `furi_hal_rfid_pin_pull_release`,
  `furi_hal_rfid_pins_reset`) byteidentisch vorhanden — dieselbe LF-125-kHz-Timer-Schnittstelle,
  die auch `hitags.c`/`t5577.c`/`em4305.c` bereits nutzen. Anders als `hitags.c` ruft
  `hitagmicro.c` nie `furi_hal_rfid_tim_read_capture_start/_stop` auf (rein open-loop, kein
  RX) — konsistent mit der oben beschriebenen Nur-Schreib-Natur des Moduls.
- `lib/bit_lib/bit_lib.h`/`.c`: `bit_lib_set_bit`/`bit_lib_get_bit`/`bit_lib_reverse_16_fast`
  byteidentisch vorhanden, Semantik (MSB-first-Bitpositionen, `reverse_16_fast` spiegelt alle
  16 Bit) stimmt mit dem im Modul dokumentierten Vertrag überein.
- `lib/lfrfid/tools/`: `hitagmicro.c`/`.h` fehlen (erwartet), `hitags.c`/`.h` liegen bereits
  als eigener, noch nicht angewendeter Port-Vorschlag vor (keine Namens- oder Symbolkollision:
  komplett getrennte `hitagmicro_*`- vs. `hitags_*`-Präfixe).
- `lib/lfrfid/SConscript`: dieselbe `libenv.GlobRecursive("*.c*")`-Erfassung wie bei
  `hitags.c` — kein Build-Datei-Eintrag nötig, reines Ablegen reicht.
- **Dieselbe Integrationslücke wie bei `hitags.c`**: `lib/lfrfid/lfrfid_worker.h` kennt in
  `mntm-012`s Basis nur ein einzelnes Zielprotokoll pro Schreibvorgang, keine
  "schreibe EM4100 aber als ID82xx/Hitag-micro-Klon"-Variante. Das Werkzeugmodul selbst ist
  100 % kompatibel; die Integrationsschicht (generisches Mehrfachziel-Dispatch oder ein
  schlanker eigener Pfad, siehe Port-Skizze) fehlt weiterhin komplett und ist nicht Teil
  dieser Sitzung — identisch zum bereits für `hitags.c` dokumentierten Rückstellpunkt, jetzt
  für beide Werkzeuge gemeinsam zu lösen (siehe unten).

Kein Port-Hindernis für `hitagmicro.c`/`.h` selbst — siehe Host-Repro.

## Betroffene Dateien

- `lib/lfrfid/tools/hitagmicro.c` — neu, **unverändert** aus dem Unleashed-`dev`-Quelltext
  übernommen (Patch: `hitagmicro-lfrfid-tool-port/hitagmicro_c.patch`)
- `lib/lfrfid/tools/hitagmicro.h` — neu, unverändert übernommen (Patch:
  `hitagmicro-lfrfid-tool-port/hitagmicro_h.patch`)

Beide Patches gegen zwei unabhängige, frische Zweitkopien von `G4MEOVER18/G4MEOVER-FW`
(`b359f6f`) mit `git apply --check` verifiziert, real angewendet (einmal in der
dokumentierten Reihenfolge `.h` dann `.c`, einmal umgekehrt `.c` dann `.h` — beide
Reihenfolgen konfliktfrei, da die beiden Dateien sich nicht überlappen), Ergebnis per
`diff -r` abgeglichen — identisch, und zusätzlich per `md5sum` gegen den unveränderten
Unleashed-Quelltext abgeglichen — identisch. Keine bestehende Datei wird verändert, rein
additiv.

**Nicht Teil dieses Patches** (siehe Port-Skizze): jede Integration, die das Werkzeug
tatsächlich nutzbar macht — dieselbe `lfrfid_worker.h`/`.c`/`lfrfid_worker_modes.c`/
`protocol_em4100.c`-Lücke wie bei `hitags.c`, plus eine UI-Auswahl zwischen allen inzwischen
drei Ziel-Chip-Familien (T5577/EM4305, Hitag S, ID82xx/Hitag micro). Ohne das bleibt
`hitagmicro.c` baubar, aber von keiner App aus erreichbar.

## Host-Repro

`hitagmicro.c` bringt, anders als `hitags.c`, keinen eigenen Selbsttest mit. Trotzdem wird
hier die **tatsächliche, unveränderte** `hitagmicro_write()` aufgerufen statt ihre Logik in
ein `repro.c` zu transkribieren: `furi_delay_us()` wird im Host-Shim nicht verworfen, sondern
aufzeichnend implementiert (`host_shim.c`). Jedes vom Modul gesendete Bit läuft vor jeder
anderen Aktion durch genau einen `furi_delay_us(96|160|288|64)`-Aufruf (Feld-Lücke, dann
Feld-An-Zeit für eine '0', eine '1' oder die SOF-Codeverletzung) — das Abspielen der
aufgezeichneten Dauer-Sequenz rekonstruiert deshalb exakt den Bitstrom, den die reale,
unveränderte Frame-Logik berechnet hat, ganz ohne eine der modul-internen `static`-Funktionen
direkt aufzurufen und ohne jede Hardware.

Die so rekonstruierten Frames werden gegen einen unabhängig geschriebenen zweiten
Frame-Builder plus CRC-16 in `repro_main.c` geprüft (gleicher dokumentierter Algorithmus —
Poly `0x1021`, Init `0`, refin=false, refout=true, `flags(5)+cmd(6)[+page(8)][+data(32)]+
crc(16)` — aber strukturell anders implementiert: Bit-für-Bit statt `hitagmicro_crc16()`s
Byte-Realignment-Trick) — dieselbe Art unabhängiger Gegenprobe, mit der auch andere
Protokoll-Ports in diesem Repo einen transkribierten Decoder gegen eine zweite, unabhängige
Implementierung absichern.

Geprüfte Eigenschaften, alle aus dem tatsächlichen Aufruf von `hitagmicro_write()` für drei
Testläufe (je eine der drei Chip-Varianten, mit unterschiedlichen, nicht-trivialen
EM4100-Nutzdaten, damit ein Byte-/Bit-Reihenfolge-Fehler sich nicht hinter einem entarteten
All-Null-Testvektor verstecken könnte):

- **9 Feld-Ein-/9 Feld-Aus-Zyklen** (3 Schreib-Sitzungen + 6 Latch-Zyklen) — bestätigt die im
  Modulkommentar behauptete Zyklenanzahl aus dem tatsächlichen Kontrollfluss, nicht nur aus
  dem Kommentar.
- **Exakt 18 gesendete Frames** je Lauf (3 Sitzungen × [READ UID, SYSINFO, READ Config,
  LOGIN, WRITE, WRITE-Wiederholung]), jedes einzelne bitgenau gegen den unabhängigen
  Referenz-Builder verglichen (CRC-16 inklusive).
- **Schreibreihenfolge block0 → block1 → config** direkt aus den 18 rekonstruierten Frames
  bestätigt (nicht nur aus dem Quelltext abgelesen) — belegt die im Modulkommentar behauptete
  "Config zuletzt"-Eigenschaft empirisch.
- **Varianten-Tabelle**: alle drei Passwörter (`00000000`/`9AC4999C`/`496B0E59`) und
  Anzeigenamen (`"8265"`/`"8210"`/`"H5.5"`) sowie der explizite Ungültig-Fall
  (`HitagMicroVariantCount` → `NULL`/`"Unknown"`) geprüft.
- **CRC-16-Randfall**: `bitlength == 0` — die eigene bitweise Referenzimplementierung liefert
  hierfür ohne Sonderfall denselben Wert (`0`) wie `hitagmicro_crc16()`s expliziter
  Sonderfall-Rücksprung — ein zusätzlicher kleiner Beleg, dass beide Implementierungen
  tatsächlich denselben Algorithmus berechnen.

**Differenzielle Gegenprobe (Mutationstest, nicht Teil des eigentlichen Repro-Laufs, nur zur
Absicherung dieses Host-Repros selbst)**: eine Kopie von `hitagmicro.c` mit vertauschtem
CRC-Polynom (`0x1021` → `0x8005`) ließ 57 der Prüfungen fehlschlagen (jede der 18 Frames über
alle drei Varianten); eine zweite Kopie mit vertauschter Schreibreihenfolge
(`config, block0, block1` statt `block0, block1, config`) ließ gezielt den
Reihenfolge-Check und die dadurch verschobenen Frame-Vergleiche fehlschlagen (21 Prüfungen) —
beide Mutationen wurden wie erwartet erkannt, keine falsch-grüne Prüfung.

Unter `gcc -std=c17 -Wall -Wextra -Wpedantic -fsanitize=address,undefined
-fno-sanitize-recover=undefined` bei `-O0`/`-O1`: `ALLE CHECKS BESTANDEN` in beiden Fällen,
identischer Output, keine ASan-/UBSan-Meldungen. Verbleibende Compiler-Warnungen (ein
unbenutzter Parameter in `hitagmicro_log_frame()`, der nur im Release-Logging-Zweig entsteht,
sowie eine vorbestehende Warnung in der mitkopierten `bit_lib.c`) liegen außerhalb des von
diesem Repro geprüften Pfads und sind keine eigenen Funde — dieselbe Art harmloser Warnungen
wie bereits beim `hitags.c`-Repro dokumentiert.

Bau (aus `firmware-mining/repro-hitagmicro-lfrfid-tool-port/`):

```
gcc -std=c17 -Wall -Wextra -Wpedantic -I. -Imocks -fsanitize=address,undefined \
    -fno-sanitize-recover=undefined -O0 \
    hitagmicro.c lib/bit_lib/bit_lib.c host_shim.c repro_main.c -o repro && ./repro
```

## Risiko

- **Timing-kritisch, Interrupts maskiert**: `hitagmicro_send()` hält für die Dauer jeder
  einzelnen Frame-Übertragung `FURI_CRITICAL_ENTER()` (längste Frame: WRITE mit 67 Bit,
  rechnerisch ca. 13–16 ms bei den dokumentierten Bitzeiten) — bewusst in Kauf genommene
  Interrupt-Sperre, derselbe Entwurf wie bei `hitags.c`s `hitags_tx_cold()`.
- **Destruktiv auf dem Ziel-Tag**: `hitagmicro_write()` überschreibt zwei Datenseiten und das
  Config-Wort eines echten ID82xx/Hitag-micro-Tags unwiderruflich.
- **Passwörter sind keine Geheimnisse**: alle drei LOGIN-Passwörter sind dokumentierte
  Familien-Standardwerte jedes Klon-Chips dieser Variante (wie bei Proxmark3s
  `lf em 410x clone --htu`), keine gerätespezifischen Schlüssel — unproblematisch im
  Klartext für dieses Repo.
- **Kein Speicherfehler gefunden**: alle Frame-Puffer sind fest auf 16 Byte dimensioniert,
  die tatsächlich genutzte Bitzahl bleibt für jeden Frametyp weit darunter (höchstens 67 Bit
  bei WRITE/LOGIN); nichts an dieser Prüfung gibt Anlass, von den bisherigen Funden
  abzuweichen — reine Kompatibilitäts- und Logikprüfung, kein eigenständiger
  Sicherheits-/Speicherfund.
- **Ungenutzt ohne Integrationsschicht**: der reine Dateidrop ist für sich allein aus keiner
  App/Scene erreichbar (siehe Lücke oben) — das Risiko dieses Patches selbst ist praktisch
  null (zwei neue, unreferenzierte Dateien), der Nutzen entsteht erst mit der fehlenden
  Integration.

## Port-Skizze

**Diese Sitzung, erledigt**: `hitagmicro.c`/`.h` unverändert nach `lib/lfrfid/tools/`
kopieren (zwei Patches oben), Kompatibilität gegen `furi_hal_rfid.h`/`bit_lib.h` verifiziert,
Host-Repro von `hitagmicro_write()` gegen eine unabhängige Referenzimplementierung grün,
Mutationstest bestätigt, dass das Repro echte Fehler auch tatsächlich erkennen würde. Baubar,
aber ohne Aufrufer — reiner Vorrat für die Integrationsschicht.

**Zurückgestellt, jetzt gemeinsamer Rückstellpunkt mit `hitags.c`**: beide Werkzeuge
(`hitags.c` für Hitag-S/ID8268, `hitagmicro.c` für ID8265/8210/H5.5) warten auf dieselbe
fehlende Integrationsschicht in `mntm-012`s Basis (siehe Abwägung in
`hitags-lfrfid-magic-tag-port.md`, Option a/b). Mit jetzt zwei statt einem Werkzeug, die
dieselbe Lücke füllen würden, wird Option (a) — generisches `LFRFIDWriteType`-Dispatch-System
nachbauen — gegenüber Option (b) — pro Werkzeug ein eigener schlanker Pfad — tendenziell
attraktiver (zwei separate Einzelpfade hätten doppelten UI-/Scene-Code für im Grunde dieselbe
"welchen Blanko-Chip beschreiben"-Auswahl), aber das ist weiterhin eine Abwägung für die
Umsetzungssitzung selbst, nicht Teil dieser Sitzung. Beide Werkzeuge teilen sich keinen Code
untereinander (komplett getrennte Präfixe, getrennte Frame-Formate) — nur die fehlende
Dispatch-Infrastruktur ist gemeinsam.

## Testhinweis (lokal)

1. Beide Patches (`hitagmicro_h.patch`, dann `hitagmicro_c.patch` — Reihenfolge beliebig, da
   konfliktfrei) gegen die eigene `mntm-012`-Basis anwenden, mit `ufbt`/`fbt firmware_all`
   bauen — reiner Compile-Test, da ohne Integrationsschicht noch kein Aufrufer existiert; ein
   Build-Fehler hier wäre ein eigenständiger, noch unbekannter Fund.
2. Host-Repro laufen lassen (Befehl siehe Host-Repro-Abschnitt oben), um die reine Logik ohne
   Hardware nachzuvollziehen.
3. Sobald die Integrationsschicht umgesetzt ist: `hitagmicro_write()` **nur** mit einem
   eigenen, zum Überschreiben bestimmten ID8265/8210/H5.5-Blanko-Tag testen — die Operation
   ist destruktiv für den vorherigen Inhalt des Tags (siehe Risiko). Ohne eigenes
   UID-Lesevermögen (open-loop) lässt sich der Erfolg nur über eine anschließende
   LFRFID-EM4100-Leseoperation (oder einen unabhängigen Proxmark3-Lauf) bestätigen, nicht über
   das Werkzeug selbst.
