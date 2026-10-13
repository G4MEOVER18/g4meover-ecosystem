# LFRFID: Hitag S / ID82xx-Magic-Tag-Werkzeug (`hitags.c`/`.h`) geprüft und portiert

## Was

Aus derselben seit der Doorbell32-Sitzung offenen `unlshd-093`→`dev`-Restliste
(`keyfinder2`/`texkom`/`hitags`, `keyfinder2` bereits portiert) den dort als größten
einzelnen Kandidaten mehrfach zurückgestellten Punkt jetzt aufgegriffen:
`lib/lfrfid/tools/hitags.c`/`.h` (959+77 Zeilen). Gegen den `dev`-Branch erneut geprüft —
weiterhin Commit `af2e764`, kein neuerer Stand, keine Wiederholung.

Das Modul ist ein eigenständiges LFRFID-Werkzeug, kein Protokoll-Decoder: es liest die UID
eines Hitag-S-Familientags und schreibt einen EM4100-Klon auf einen beschreibbaren
ID8268/82xx-"Magic"-Chip — dieselbe Nische wie die bereits bei mir vorhandenen
`tools/t5577.c`/`em4305.c` (EM4100 auf einen T5577/EM4305 klonen), hier für eine dritte,
verbreitete Familie billiger Blanko-Transponder. Der Nutzen ist derselbe wie bei T5577/EM4305:
eine eigene, bereits per LFRFID ausgelesene Zugangskarte auf einen eigenen Blanko-Tag klonen,
um ein physisches Zugangskontrollsystem mit der eigenen Kopie statt dem Original zu testen.

Technisch zwei fast unabhängige Hälften:

1. **UID-Lesen** (`hitags_read_uid()`): closed-loop. Der Flipper-Hüllkurvendetektor kann die
   Tag→Reader-Manchester-Antwort nicht als '0'/'1' trennen (eine '1' wird weggefiltert), also
   wird nur die Kantenfolge der '0'-Zellen interpretiert, die unbekannte Rahmen-Ausrichtung und
   die verlorenen führenden Bits werden iterativ als AC-SEQUENCE-Präfixe an den Tag
   zurückgespielt — nur der Tag, dessen echte UID so beginnt, antwortet. Das macht "hat
   geantwortet" selbst zum Beweis, nicht nur den Antwortinhalt.
2. **Schreiben** (`hitags_write()`): open-loop wie `t5577_write()`/`em4305_write()` — Frames
   werden gesendet, keine Antwort gelesen, jede Seite in eigener stromzyklierter Sitzung
   (UID REQUEST → SELECT → 82xx-Passwort-Unlock → Page-Write), weil der Tag die Sitzung nach
   jedem Schreiben fallen lässt.

Beide Hälften, plus CRC-8 (`poly 0x1D`, Datenblatt-Vektor), Frame-Builder und der
Anticollision-Decoder selbst, sind bereits im eigenen `hitags_selftest()` gegen
Datenblatt-/Proxmark3-Vektoren gepinnt (siehe Host-Repro) — reine Bit-Logik, ruft keine
`furi_hal_rfid_*`-Funktion auf.

## Kompatibilitätsprüfung gegen `mntm-012`

Gegen eine frische, unabhängige Zweitkopie von `G4MEOVER18/G4MEOVER-FW` (Tag-Commit
`b359f6f`, `mntm-012`) geprüft:

- `targets/f7/furi_hal/furi_hal_rfid.h`: jede von `hitags.c` aufgerufene Funktion
  (`furi_hal_rfid_tim_read_start/_pause/_continue/_stop`, `_tim_read_capture_start/_stop`
  inkl. identischer Callback-Signatur `bool level, uint32_t duration, void* context`,
  `furi_hal_rfid_pins_reset`) byteidentisch vorhanden — dasselbe LF-125-kHz-Timer-Interface,
  das `t5577.c`/`em4305.c` bereits nutzen.
- `lib/bit_lib/bit_lib.h`/`.c`: `bit_lib_get_bit`/`_set_bit`/`_copy_bits` byteidentisch
  vorhanden, Semantik (MSB-first) stimmt mit dem im Modul dokumentierten Vertrag überein.
- `lib/lfrfid/tools/`: Verzeichnis existiert, `hitags.c`/`.h` fehlen (erwartet). Kein
  `common.c`-Äquivalent für Tools nötig — jedes Tool ist bereits für sich eigenständig
  (anders als die SubGHz-Protokolle).
- `lib/lfrfid/SConscript`: `libenv.GlobRecursive("*.c*")` erfasst neue Dateien in
  `lib/lfrfid/tools/` automatisch — kein Build-Datei-Eintrag nötig, reines Ablegen reicht.
- **Lücke, die den eigentlichen Portierungsaufwand ausmacht**: `lib/lfrfid/lfrfid_worker.h`
  kennt nur `lfrfid_worker_write_start(worker, protocol, callback, context)` — ein einzelnes
  Protokoll, keine Variante "schreibe Protokoll X aber als Hitag-S-Klon". Der
  `dev`-Branch-Stand, aus dem `hitags.c` stammt, hat dafür ein inzwischen vorhandenes
  `LFRFIDWriteType`/`LFRFIDWriteRequest`-Dispatch-System (sichtbar im Unit-Test
  `test_lfrfid_protocol_em_write_hitags`, das `protocol_dict_get_write_data(dict,
  LFRFIDProtocolEM4100, &via_hitags)` mit `.write_type = LFRFIDWriteTypeHitagS` aufruft), das
  in `mntm-012`s Basis schlicht nicht existiert. Das Werkzeugmodul selbst ist also 100%
  kompatibel; die Scheune um die Tür — generische Mehrfachziel-Schreib-Infrastruktur plus
  UI-Auswahl "worauf klonen" — fehlt komplett und ist nicht Teil dieser Sitzung.

Kein Port-Hindernis für `hitags.c`/`.h` selbst — siehe Host-Repro.

## Betroffene Dateien

- `lib/lfrfid/tools/hitags.c` — neu, **unverändert** aus dem Unleashed-`dev`-Quelltext
  übernommen (Patch: `hitags-lfrfid-magic-tag-port/hitags_c.patch`)
- `lib/lfrfid/tools/hitags.h` — neu, unverändert übernommen (Patch:
  `hitags-lfrfid-magic-tag-port/hitags_h.patch`)

Beide Patches gegen drei unabhängige, frische Zweitkopien von `G4MEOVER18/G4MEOVER-FW`
(`b359f6f`) mit `git apply --check` verifiziert, real angewendet, Ergebnis per `diff -r`
paarweise byte-für-byte abgeglichen — identisch. Keine bestehende Datei wird verändert, rein
additiv.

**Nicht Teil dieses Patches** (siehe Port-Skizze Phase 2): jede Integration, die das Werkzeug
tatsächlich nutzbar macht — `lfrfid_worker.h`/`.c`, `lfrfid_worker_modes.c`,
`protocol_em4100.c`/`.h` (Schreib-Pfad), eine Scene/UI zur Auswahl "Ziel-Tag-Familie". Ohne
das bleibt `hitags.c` zwar baubar und sein Selbsttest grün, aber von keiner App aus
erreichbar.

## Host-Repro

Anders als bei den bisherigen SubGHz-Ports (Logik verbatim in ein eigenständiges `repro.c`
transkribiert) hier die **tatsächliche, unveränderte** `hitags.c` selbst gegen schlanke
Platzhalter für `furi.h`/`furi_hal_rfid.h`/`core/check.h` kompiliert und die echte, eigene
`hitags_selftest()` des Moduls ausgeführt — das deckt genau den reinen Logik-Anteil ab
(CRC-8, alle vier Frame-Builder, der Anticollision-Decoder inkl. TTF-Verwechslungsschutz,
der Präfix-Abgleich), den `hitags_selftest()` laut eigenem Kommentar auch auf dem Gerät für
den Unleashed-Unit-Test-Lauf prüft, und braucht dafür keine echte Hardware:

- `hitags.c`/`.h`: Datei-für-Datei-Kopie des Originals, keine einzige Zeile verändert.
- `lib/bit_lib/bit_lib.c`/`.h`: Datei-für-Datei-Kopie aus der eigenen `G4MEOVER-FW`-Zweitkopie
  (`b359f6f`) — reale Semantik statt Nachbau.
- `mocks/furi.h`, `mocks/furi_hal_rfid.h`, `mocks/core/check.h`: minimale Platzhalter, deren
  Signaturen 1:1 den echten `furi_hal_rfid.h`-Funktionen entsprechen; `hitags_selftest()`
  ruft keine davon auf (sie werden nur fürs Linken gebraucht, `hitags_read_uid()`/
  `hitags_write()` selbst bleiben dadurch ungetestet — siehe Testhinweis für den
  Hardware-Teil).
- `repro_main.c`: ruft die reale `hitags_selftest()` auf, meldet `PASS`/`FAIL`.

Geprüfte Vektoren (alle aus `hitags_selftest()` selbst, nicht von mir ausgewählt): CRC-8 gegen
den Datenblatt-Vektor (Kap. 13.3), UID-REQUEST-Frame, ein reales Proxmark3-SELECT-Trace
(UID `95D3 2408` → 45-Bit-Frame), beide WRITE-PAGE-Frames (Auth-Page und Daten-Page), der
82xx-Passwort-Frame, ein AC-SEQUENCE-Frame, der volle Decoder-Rundlauf gegen eine synthetische
UID-Antwort (`95D3194C`) inkl. Alignment-Grenzfall (ein Slot verschoben darf **nicht**
matchen), zwei Leerfenster-Fälle, ein TTF-Dauerstrom-Fall und ein Sweep über fünf
Halbzellen-Fehlpositionen (Manchester-Gap-Verwechslungsschutz), sowie der
Präfix-Abgleich-Trust-Anchor (positiv für die richtige UID, negativ für eine, die den
Präfix teilt, den Rest aber nicht).

Unter `gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
bei `-O0`/`-O1`: `hitags_selftest()` liefert `NULL` (alle Vektoren bestanden) in beiden
Fällen, identischer Output, keine ASan-/UBSan-Meldungen. Verbleibende Compiler-Warnungen
(eine unbenutzte Variable in `hitags_read_uid()`, eine in der mitkopierten `bit_lib.c`) liegen
beide außerhalb des von `hitags_selftest()` durchlaufenen Codepfads und sind keine eigenen
Funde.

Bau (aus `firmware-mining/repro-hitags-lfrfid-magic-tag-port/`):

```
gcc -std=c17 -Wall -Wextra -I. -Imocks -fsanitize=address,undefined \
    -fno-sanitize-recover=undefined -O0 \
    hitags.c lib/bit_lib/bit_lib.c repro_main.c -o repro && ./repro
```

## Risiko

- **Timing-kritisch, Interrupts maskiert**: `hitags_tx_cold()` hält für ~4 ms
  `FURI_CRITICAL_ENTER()` (Moduskommentar warnt explizit davor, das Zeitfenster "zu
  verbreitern" statt die Critical Section zu akzeptieren) — Systick-Verlust während dieser
  Zeit ist von den Autoren bewusst in Kauf genommen, nicht versehentlich.
- **Destruktiv auf dem Ziel-Tag**: `hitags_write()` überschreibt zwei Datenseiten eines
  echten Hitag-S-Tags unwiderruflich; das Modul loggt das selbst auf Info-Level, genau
  deshalb, weil Release-Builds Debug-Logs wegkompilieren und sonst keine Spur bliebe.
- **82xx-Passwort ist kein Geheimnis**: `hitags_82xx_password[]` ist der dokumentierte
  Familien-Standardwert jedes ID82xx-Klons (wie bei Proxmark3s `HTSF_82xx`), kein
  gerätespezifischer Schlüssel — entsprechend für dieses Repo unproblematisch im Klartext.
- **Kein Speicherfehler gefunden**: alle Puffer sind fest dimensioniert
  (`HITAGS_FRAME_BYTES`/`HITAGS_MAX_EDGES`), jeder Index wird vor Gebrauch gegen die
  tatsächliche Länge geprüft (`furi_check`, `lead_max`-Clamping); nichts an dieser Prüfung
  gibt Anlass, von den bisherigen Funden abzuweichen — hier liegt kein eigenständiger
  Sicherheits- oder Speicherfund vor, nur eine Kompatibilitäts- und Logikprüfung.
- **Ungenutzt ohne Phase 2**: der reine Dateidrop ist für sich allein aus keiner App/Scene
  erreichbar (siehe Lücke oben) — das Risiko dieses Patches selbst ist praktisch null (zwei
  neue, unreferenzierte Dateien), der Nutzen entsteht erst mit der fehlenden Integration.

## Port-Skizze

**Phase 1 (diese Sitzung, erledigt)**: `hitags.c`/`.h` unverändert nach
`lib/lfrfid/tools/` kopieren (zwei Patches oben), Kompatibilität gegen `furi_hal_rfid.h`/
`bit_lib.h` verifiziert, Host-Repro der eigenen `hitags_selftest()` grün. Baubar, aber ohne
Aufrufer — reiner Vorrat für Phase 2.

**Phase 2 (zurückgestellt, eigener Rückstellpunkt)**: zwei Optionen, vor der Umsetzung
gegeneinander abzuwägen statt die größere blind zu übernehmen:

- *(a) Generische Dispatch-Infrastruktur* wie im `dev`-Branch: `LFRFIDWriteType`-Enum und
  `LFRFIDWriteRequest`-Union in `lfrfid_worker.h` nachbauen, `protocol_em4100.c`s
  Schreib-Pfad um einen `LFRFIDWriteTypeHitagS`-Zweig erweitern (liefert `LFRFIDHitagS` statt
  der normalen T5577/EM4305-Blöcke), `lfrfid_worker_modes.c` um einen Ablauf erweitern, der
  zuerst `hitags_read_uid()` (UID lesen, UI-Feedback "Tag auflegen") und danach
  `hitags_write()` aufruft, UI-Scene zur Auswahl der Ziel-Chip-Familie. Größter Umbau, dafür
  generisch für jedes künftig portierte EM4100-kompatible Protokoll wiederverwendbar.
- *(b) Eigenständiger, schlanker Pfad*: eine einzelne neue Scene/App-Funktion, die direkt
  `hitags_read_uid()`/`hitags_write()` aufruft und die EM4100-Daten selbst in `page4`/`page5`
  packt (die Aufteilung ist in `hitags.h` bereits dokumentiert: Bits 63..32 / 31..0 des
  64-Bit-EM4100-Frames) — kein Umbau der generischen Dict-Infrastruktur, dafür nicht
  wiederverwendbar für andere Protokolle oder spätere `hitagmicro`-artige Ziele.

Für beide Optionen zusätzlich zu klären: `hitagmicro.c`/`.h` (327+57 Zeilen, laut Datenblatt-
Header dieselbe BPLM-Timing-Familie wie `hitags.c`, aber nicht Teil dieser Sitzung) teilt sich
vermutlich Code mit einer künftigen generischen Lösung — vor Phase 2 gegenprüfen, ob (a) oder
(b) das besser trägt.

## Testhinweis (lokal)

1. Beide Patches (`hitags_h.patch`, dann `hitags_c.patch`) gegen die eigene `mntm-012`-Basis
   anwenden, mit `ufbt`/`fbt firmware_all` bauen — reiner Compile-Test, da ohne Phase 2 noch
   kein Aufrufer existiert; ein Build-Fehler hier wäre ein eigenständiger, noch unbekannter
   Fund.
2. Host-Repro laufen lassen (Befehl siehe Host-Repro-Abschnitt oben), um die reine Logik
   ohne Hardware nachzuvollziehen.
3. Sobald Phase 2 umgesetzt ist: `hitags_read_uid()` an einem eigenen ID8268/82xx-Tag am
   Erweiterungsport bzw. der internen LF-Antenne testen (UID muss mit einem unabhängigen
   Proxmark3-`lf hitag hts reader`-Lauf übereinstimmen), danach `hitags_write()` **nur** mit
   einem eigenen, zum Überschreiben bestimmten Blanko-Tag — die Operation ist destruktiv für
   den vorherigen Inhalt des Tags (siehe Risiko).
