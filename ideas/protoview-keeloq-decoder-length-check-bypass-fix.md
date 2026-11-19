# ProtoView — KeeLoq-Dekoder: Längen-Check ist ein No-Op, Nachrichten können aus Speicher jenseits des tatsächlich empfangenen Signals zusammengesetzt werden

## Was

Fortsetzung der [ProtoView](https://github.com/antirez/protoview)-Prüfung (`antirez/protoview`,
BSD-2-Clause, Commit `fd2f6ed`): nach dem API-87-Fund
(`ideas/protoview-digital-signal-tool-app-status.md`), dem Oregon-2-Logikfund
(`ideas/protoview-oregon2-decoder-logic-fixes.md`) und dem B4B1-Sync-Fund
(`ideas/protoview-b4b1-decoder-sync-loop-bound-fix.md`) nimmt diese Sitzung den KeeLoq-Dekoder vor
(`protocols/keeloq.c`, 128 Zeilen) — Microchip-HCS200/300/301-Rolling-Code-Fernbedienungen, der
vierte der vier nativ unterstützten Protokolle.

**Fund:** Der Längen-Vorab-Check in `decode()` ist algebraisch wirkungslos.

```c
const char *sync_pattern = "101010101010101010101010" "0000";
uint8_t sync_len = 24+4;
if (numbits-sync_len+sync_len < 3*66) return false;   // <- "-sync_len" und "+sync_len" heben sich auf
uint32_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
```

`numbits-sync_len+sync_len` ist für jeden Wert von `numbits` und `sync_len` identisch zu `numbits`
selbst (Subtraktion und Addition desselben Werts heben sich auf, auch ohne Betrachtung von
Unsigned-Wraparound) — die Zeile prüft also faktisch nur `if (numbits < 3*66) return false;` und
ignoriert `sync_len` komplett, obwohl sie es sichtbar verwendet.

Zum Vergleich: jeder der sieben übrigen Dekoder in derselben Codebasis, die genau dasselbe Problem
lösen müssen (Sync-Länge von der Gesamtbit-Anzahl abziehen, um zu prüfen, ob für die Nutzdaten noch
genug *tatsächlich empfangene* Bits übrig sind), verwendet dieselbe Formel korrekt:

| Datei | Zeile |
|---|---|
| `protocols/oregon2.c` | `if (numbits < 32) return false;` (kein Sync-Abzug nötig, da der spätere Offset separat geprüft wird) |
| `protocols/pvchat.c` | `if (numbits-sync_len < 8*4) return false;` |
| `protocols/tpms/citroen.c` | `if (numbits-sync_len < 8*10) return false;` |
| `protocols/tpms/ford.c` | `if (numbits-sync_len < 8*8) return false;` |
| `protocols/tpms/renault.c` | `if (numbits-12 < 9*8) return false;` |
| `protocols/tpms/schrader_eg53ma4.c` | `if (numbits-sync_len+8 < 8*10) return false;` |
| `protocols/tpms/toyota.c` | `if (numbits-6 < 64*2) return false;` |

Alle sieben folgen demselben Muster `numbits - <sync-bedingter Abzug> < <Nutzdaten-Bits>`. Nur
`keeloq.c` addiert `sync_len` nach dem Abziehen wieder hinzu und hebt die eigene Prüfung damit auf.

Per `git log --follow -p -- protocols/keeloq.c` ist die fehlerhafte Zeile unverändert seit dem
allerersten Commit, der die Datei einführt (`ea48c7f`, "Microchip HCS series, Keeloq protocol.",
2023-01-15) vorhanden — kein später eingeschlichener Regressions-Bug, sondern ein Tippfehler von
Anfang an, der nie aufgefallen ist, weil er sich nicht als Build- oder Absturzfehler äußert.

## Nutzen

Vierter und letzter der vier nativ unterstützten Protokoll-Decoder logikgeprüft (nach
API-Kompatibilitätsfund, Oregon-2-Fund und B4B1-Fund). Anders als die bisherigen drei Funde
(fehlende Decoder-Kompatibilität bzw. falsch dekodierte/übersehene aber *eigene* Bits) betrifft
dieser Fund die Vertrauenswürdigkeit des Ergebnisses selbst: ohne Fix kann der KeeLoq-Dekoder eine
vollständige, plausibel aussehende 66-Bit-Nachricht (verschlüsselter Rolling Code, 28-Bit-Remote-ID,
Tasten-Bits, Batterie-Flag) melden, obwohl ein Teil dieser Bits nachweislich nicht aus dem gerade
empfangenen Funksignal stammt, sondern aus Speicherinhalt jenseits der tatsächlich abgetasteten
Bit-Anzahl. Für die eigene passive Recon-Palette bedeutet das: ein zu kurz empfangener/abgeschnittener
KeeLoq-Sendevorgang kann als scheinbar vollständige, in sich konsistente Nachricht angezeigt werden
— mit Feldwerten, die teilweise von einem früheren, unverwandten Dekodiervorgang stammen können,
statt dass ProtoView den unvollständigen Empfang als solchen erkennt und verwirft.

## Betroffene Dateien

- `protocols/keeloq.c` (`antirez/protoview`, Funktion `decode()`) — eine Zeile.

## Fund im Detail: warum das kein reines Schönheitsproblem ist

Der Aufrufer in `signal.c::decode_signal()` legt für jedes neu erkannte Signal einen frischen
Scratch-Puffer per `malloc(bitmap_size)` an (`bitmap_size` fest 4096 Byte = 32768 Bit, **kein**
`memset`/`calloc`), wandelt das tatsächlich empfangene Signal in `bits`-Bits um
(`convert_signal_to_bits()`, Rückgabewert = `numbits`, typischerweise weit unter 32768) und ruft
dann `Decoders[j]->decode(bitmap, bitmap_size, bits, info)` auf — **`bitmap_size`** (die volle
4096-Byte-Pufferkapazität), nicht `numbits`, wird als Längenschranke an `decode()` weitergegeben und
von dort unverändert an `convert_from_line_code()` durchgereicht. `numbits` selbst dient in
`decode()` ausschließlich als Eingabe für genau die eine (hier fehlerhafte) Vorab-Prüfung und für
die `bitmap_seek_bits()`-Suchgrenze — danach spielt es für `convert_from_line_code()` keine Rolle
mehr. Jeder Bit-Index zwischen `numbits` und `bitmap_size*8`, der innerhalb des gesuchten
Nachrichtenbereichs liegt, besteht also aus Speicher, der nie von `convert_signal_to_bits()`
beschrieben wurde — bei `malloc()` ohne Nullung in der Praxis sehr häufig der unveränderte
Rest-Inhalt des zuvor an genau dieser Adresse freigegebenen vorherigen Aufrufs (derselbe
4096-Byte-`malloc()`/`free()`-Zyklus läuft bei jedem einzelnen erkannten Signal erneut, Adress-Reuse
durch den Allokator ist der Normalfall, nicht der Ausnahmefall).

Mit dem intendierten Check (`numbits-sync_len < 3*66`) wird abgelehnt, sobald nach Abzug der
Sync-Länge weniger als 198 tatsächlich abgetastete Bits übrig sind — das deckt zuverlässig jeden
Fall ab, in dem die 198 für die Dekodierung benötigten Bits teilweise außerhalb des echten Signals
liegen würden. Der tatsächliche (no-op) Check lässt stattdessen jedes `numbits >= 198` durch,
unabhängig davon, wo genau innerhalb von `numbits` der Sync gefunden wird — bei einem Sync nahe dem
Ende des echten Signals (z. B. durch Rauschen vor dem Sync, oder weil der Sendevorgang kurz vor
Ende der Aufzeichnung abgeschnitten wurde) kann der 198-Bit-Lesebereich dadurch bis zu `sync_len`
(28) Bit über `numbits` hinausreichen, ohne dass `decode()` das bemerkt.

## Fix

Eine Zeile, zieht die bestehende Variable `sync_len` korrekt ab statt sie wieder hinzuzufügen —
identisch zum bereits in sieben Geschwister-Dekodern verwendeten Muster:

```c
-    if (numbits-sync_len+sync_len < 3*66) return false;
+    if (numbits-sync_len < 3*66) return false;
```

Patch in `ideas/protoview-keeloq-decoder-length-check-bypass-fix/keeloq_c.patch`. Gegen drei
frische, unabhängige Zweitkopien von `antirez/protoview` (`fd2f6edd43cb813c37a7d912c7dd88f9732d298f`)
per `git apply --check`/`git apply` UND per `patch -p1 --dry-run`/`patch -p1` verifiziert —
Ergebnisdatei in allen drei Kopien und bei beiden Prüfmethoden laut `md5sum` byte-identisch.

## Host-Repro

`ideas/repro-protoview-keeloq-decoder-length-check-bypass/repro.c`: alle von `decode()`
verwendeten Bitmap-Primitiven (`bitmap_set`/`bitmap_get`/`bitmap_match_bits`/`bitmap_seek_bits`/
`convert_from_line_code`/`bitmap_reverse_bytes_bits`) wörtlich aus `signal.c` übernommen, `decode()`
selbst zweimal transkribiert (ALT = unveränderte Original-Zeile, NEU = Fix) — jede andere Zeile
(Sync-Suche, Gap-Zählung, Line-Code-Extraktion, Bit-Reversal) ist zwischen ALT und NEU identisch.
`FURI_LOG_E`/Fieldset-Aufrufe sind entfernt (kein Flipper-Framework auf dem Host); die dekodierten
`raw[9]`-Bytes werden stattdessen direkt zur Prüfung zurückgegeben.

Vier Szenarien, je mit zwei fest gewählten, klar unterscheidbaren 66-Bit-Nachrichten A (echte
Aufnahme) und B (steht stellvertretend für Speicherinhalt eines vorherigen, unverwandten
Dekodiervorgangs — im Normalfall des beschriebenen `malloc`/`free`-Wiederverwendungsmusters
plausibel echter, nicht zufälliger Leftover-Inhalt):

1. **Abgeschnittene Aufnahme, Speicherbereich jenseits von `numbits` setzt B's Line-Code fort**
   (`numbits=198`, exakt an ALT's wirkungsloser Schwelle, 55 von 66 Datenbits stammen aus der
   echten Aufnahme A, die restlichen 11 — byteweise ausgerichtet auf `raw[7]`, das Tasten- plus
   4 ID-Bits trägt — aus B): ALT meldet `true`, `decoded==66`, `raw[7]` entspricht exakt B's Byte,
   nicht A's — **Finding reproduziert**: ein vollständig gemeldetes Tasten-/ID-Feld stammt
   nachweislich aus Speicher jenseits des echten Signals. NEU lehnt denselben Puffer korrekt ab.
2. **Gegenprobe:** dieselbe abgeschnittene Aufnahme, aber mit nullgefülltem (statt
   B-line-code-förmigem) Speicher jenseits von `numbits` — ALT lehnt ab, weil Nullen keinen
   gültigen `"110"`/`"100"`-Lauf bilden. Zeigt, dass Szenario 1 kein Zufallstreffer ist, sondern
   gezielt zeigt, *was* den Bug in der Praxis gefährlich macht (RF-förmiger Speicherinhalt durch
   `malloc`/`free`-Wiederverwendung), ohne zu behaupten, der Bug sei nur mit präpariertem
   Speicherinhalt auslösbar.
3. **Regressionsprobe:** vollständige, echte Aufnahme (`numbits=231`, keine Bits jenseits des
   Sync+Nutzdaten-Bereichs angefordert) — ALT und NEU dekodieren beide korrekt und identisch zu A.
4. **Grob zu kurze Aufnahme** (`numbits=40`, weit unter jeder Schwelle) — beide lehnen korrekt ab.

`gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei
`-O0`/`-O1`: `11/11 Prüfungen bestanden`, `ALL CHECKS PASSED`, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

## Testhinweis (lokal)

Patch gegen die eigene `antirez/protoview`-Kopie anwenden (zusätzlich zum bereits dokumentierten
`furi_hal_subghz_load_preset()`-Fix in `ideas/protoview-load-preset-removed-api-fix/`, ohne den die
App auf meiner Firmware gar nicht erst startet), `./fbt launch_app APPSRC=protoview` im eigenen
`G4MEOVER-FW`-Baum. Funktionstest: eine eigene KeeLoq-Fernbedienung (HCS200/300/301, 433,92 MHz OOK)
vollständig und wiederholt empfangen — die dekodierten Feldwerte (`encr`, `id`,
`s[2,1,0,3]`/Tasten, Batterie) müssen bei jedem vollständigen Empfang eines selbst gedrückten
Tasters konsistent zum erwarteten eigenen Remote-ID bleiben (Regressionsprobe, unverändert zum
Verhalten vor dem Fix). Den eigentlichen Fehlerfall (abgeschnittener Empfang) lokal zu erzwingen ist
ohne gezielte Signalbeschneidung schwer reproduzierbar — der Fix ist rein defensiv (schließt eine
Bedingung, die ohne ihn unbemerkt bliebe) und verändert das Verhalten bei vollständigem Empfang
nicht.

## Risiko

Eine Zeile, stellt die bereits vom Autor selbst sichtbar beabsichtigte Prüfung wieder her (dieselbe
Formel wie in sieben Geschwister-Dekodern) — kein neues Muster, keine Änderung der
Dekodierlogik nach erfolgreicher Prüfung, kein neuer Sende-Pfad. Reine Empfangs-/
Dekodierfunktion für fremde KeeLoq-Signale, fällt wie der Rest von ProtoView unter passive
Signalanalyse auf bereits vorhandene Flipper-SubGhz-Empfangsfunktion. Der Fix kann ausschließlich
dazu führen, dass zuvor fälschlich akzeptierte unvollständige Aufnahmen jetzt korrekt abgelehnt
werden — kein Risiko, zuvor korrekt dekodierte vollständige Nachrichten zu verlieren (siehe
Regressionsprobe oben).

## Offen

Damit sind alle vier nativ unterstützten Protokoll-Decoder von ProtoView mindestens einmal
zeilenweise geprüft. Weiterhin offen: die Bit-Edit-/Keyboard-Ansicht selbst (nicht Teil der
Protokoll-Decoder) bleibt als Rückstellpunkt für künftige Sitzungen.

## Quelle

`antirez/protoview`, GitHub, BSD-2-Clause, Commit `fd2f6edd43cb813c37a7d912c7dd88f9732d298f`
(2025-05-05, zugleich aktueller `HEAD` des Repositorys, keine neueren Commits), Datei
`protocols/keeloq.c`. Git-History-Beleg für das Alter des Funds: Commit
`ea48c7f89d1a6429295c06b4d636c116e56f2b3b` ("Microchip HCS series, Keeloq protocol.", 2023-01-15).
