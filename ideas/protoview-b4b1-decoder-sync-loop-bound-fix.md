# ProtoView — B4B1-Dekoder (PT2262/SC5262): Sync-Suche durchläuft nur die Hälfte der deklarierten Muster

## Was

Fortsetzung der [ProtoView](https://github.com/antirez/protoview)-Prüfung (`antirez/protoview`,
BSD-2-Clause, Commit `fd2f6ed`): nach dem API-87-Fund
(`ideas/protoview-digital-signal-tool-app-status.md`) und dem Oregon-2-Logikfund
(`ideas/protoview-oregon2-decoder-logic-fixes.md`) nimmt diese Sitzung den B4B1-Dekoder vor
(`protocols/b4b1.c`, 96 Zeilen) — PT2262/Silian-SC5262-Fernbedienungen (Princeton-Chipsatz-Familie,
433,92 MHz OOK), der dritte der vier nativ unterstützten Protokolle.

**Fund:** Die Sync-Suche in `decode()` deklariert sechs mögliche Präambel-Muster, durchsucht aber
nur die ersten drei.

```c
const char *sync_patterns[6] = {
    "100000000000000000000000000000011101", /* idx0: 30 times gap + one. */
    "100000000000000000000000000000010001", /* idx1: 30 times gap + zero. */
    "1000000000000000000000000000000011101", /* idx2: 31 times gap + one. */
    "1000000000000000000000000000000010001", /* idx3: 31 times gap + zero. */
    "10000000000000000000000000000000011101", /* idx4: 32 times gap + one. */
    "10000000000000000000000000000000010001", /* idx5: 32 times gap + zero. */
};

uint32_t off;
int j;
for (j = 0; j < 3; j++) {                 // <- sollte 6 sein, Array hat 6 Einträge
    off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_patterns[j]);
    if (off != BITMAP_SEEK_NOT_FOUND) break;
}
if (off == BITMAP_SEEK_NOT_FOUND) return false;
```

Per Git-History (`git log -p -- protocols/b4b1.c`, Commit `cc32c5e` "b4b1: more robust message
detection") stand hier ursprünglich ein Drei-Elemente-Array (Sync-Gap 30/31/32 te, jeweils ohne
Berücksichtigung des ersten Datenbits), passend zur damaligen `j < 3`-Schleife. Dieser Commit hat
das Array bewusst auf sechs Einträge erweitert, um für jede der drei Gap-Längen zusätzlich das
jeweils erste Datenbit (0 oder 1) mit in das Sync-Muster aufzunehmen ("more robust message
detection") — die Schleifengrenze wurde dabei nicht mitgezogen. Damit sind die Indizes 3, 4 und 5
(31-te-Gap + Bit 0, 32-te-Gap + Bit 1, 32-te-Gap + Bit 0) toter Code: `bitmap_seek_bits()` wird für
sie nie aufgerufen.

Zum Vergleich: der TPMS-Toyota-Dekoder in derselben Codebasis (`protocols/tpms/toyota.c`) löst
exakt dasselbe Muster — mehrere Präambel-Varianten mit/ohne folgendem Datenbit — korrekt mit einer
NULL-terminierten Musterliste und `for (j = 0; sync[j]; j++)`, probiert also wirklich alle
deklarierten Einträge durch.

## Nutzen

Dritter der vier nativ unterstützten Protokoll-Decoder logikgeprüft (nach API-Kompatibilitätsfund
und Oregon-2-Fund). Der Fund betrifft die Signal-*Erkennung* selbst (nicht nur Dateninterpretation
nach erfolgter Erkennung): ohne Fix erkennt ProtoView ein gültiges PT2262/SC5262-Signal in der
Hälfte der realistischen Fälle gar nicht als Nachricht — die App zeigt dann nur den
Rohpuls-Bildschirm statt des decodierten Dateninhalts ("id"/"button"), obwohl das Signal vollständig
empfangen wurde. Für die eigene passive Recon-Palette bedeutet das: PT2262/SC5262-basierte
433-MHz-Handsender (sehr verbreitet bei Garagentor-/Torantrieb-Fernbedienungen, Bewegungsmeldern,
Steckdosen-Funkschaltern) werden mit ProtoView nur unzuverlässig decodiert, ohne dass das als
Empfangsproblem erkennbar wäre.

## Betroffene Dateien

- `protocols/b4b1.c` (`antirez/protoview`, Funktion `decode()`) — eine Zeile.

## Fund im Detail: warum das kein Rand-, sondern ein Regelfall ist

Der Sync besteht laut `build_message()` aus einem kurzen Puls (`te`) gefolgt von einer 31-`te`-Pause
(`raw_samples_add(samples,true,te); raw_samples_add(samples,false,te*31);`). Die 30/31/32-Variante
deckt die Messtoleranz der Pulslängen-Erkennung ab (ein `te`-Zähleinheiten-Fehler von ±1 durch
Timing-Jitter ist bei dieser Modulationsart normal — exakt der Grund, warum hier überhaupt drei statt
nur einer Gap-Länge geprüft werden). Das erste Datenbit danach ist der höchstwertige Bit der
24-Bit-Nutzlast (ID + Button) und bei beliebigen echten Fernbedienungs-Codes mit etwa 50 %
Wahrscheinlichkeit 0 statt 1. Beide Dimensionen (gemessene Gap-Länge, Wert des ersten Datenbits)
variieren unabhängig vom eigentlichen Inhalt der Nachricht — der Fund trifft im Mittel rund die
Hälfte aller realistischen Empfangsfälle, nicht nur einen konstruierten Grenzfall.

## Fix

Eine Zeile, additiv im Sinn von "aktiviert bereits deklarierten, aber unerreichbaren Code" — keine
neue Logik, kein geändertes Muster:

```c
-    for (j = 0; j < 3; j++) {
+    for (j = 0; j < 6; j++) {
```

Patch in `ideas/protoview-b4b1-decoder-sync-loop-bound-fix/b4b1_c.patch`. Gegen zwei frische,
unabhängige Zweitkopien von `antirez/protoview` (`fd2f6edd43cb813c37a7d912c7dd88f9732d298f`) per
`git apply --check`/`git apply` UND per `patch -p1 --dry-run`/`patch -p1` verifiziert — Ergebnisdatei
in beiden Fällen und bei beiden Prüfmethoden laut `md5sum` byte-identisch zur lokal vorbereiteten
Zieldatei.

## Host-Repro

`ideas/repro-protoview-b4b1-decoder-sync-loop-bound-fix/`: die Sync-Suche aus `decode()` (das
`sync_patterns[]`-Array plus die Such-Schleife, bis einschließlich des
`BITMAP_SEEK_NOT_FOUND`-Checks) wörtlich in zwei Varianten extrahiert
(`b4b1_seek_sync_ALT.c`/`_NEU.c`, ALT = unverändertes `j < 3`, NEU = `j < 6`), gegen eine wörtliche
Kopie der von `decode()` tatsächlich genutzten Bitmap-Primitiven aus `signal.c`
(`bitmap_set`/`bitmap_get`/`bitmap_set_pattern`/`bitmap_match_bits`/`bitmap_seek_bits`, `mocks.c`) —
keine Nachbildung, derselbe Code, den auch die reale Firmware/App nutzt.

`repro_main.c` baut für jedes der sechs deklarierten `sync_patterns[]`-Einträge eine eigene Bitmap,
die exakt dieses eine Muster (mit kleinem Null-Versatz davor, um echtes Vorwärtssuchen statt
Zufallstreffer an Position 0 zu erzwingen) und sonst nichts enthält, und ruft ALT/NEU jeweils darauf
auf:

- ALT findet die Muster mit Index 0–2 (wie erwartet unverändert funktionsfähig — keine Regression),
  meldet für Index 3–5 jeweils "nicht gefunden" — **Finding reproduziert**.
- NEU findet alle sechs Muster, mit korrektem Offset und korrektem Index.
- Gegenprobe: ein komplett nullgefüllter Puffer (kein Sync-Muster enthalten) wird von ALT UND NEU
  abgelehnt — der Fix führt nicht zu Falsch-Positiven. Der bestehende `numbits < 30`-Wächter bleibt
  für beide Varianten unverändert wirksam.

`gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined` bei
`-O0`/`-O1`: `34/34 Prüfungen bestanden`, `ALL CHECKS PASSED`, identischer Output beider
Optimierungsstufen, keine Compiler-Warnungen, keine Sanitizer-Befunde.

## Testhinweis (lokal)

Patch gegen die eigene `antirez/protoview`-Kopie anwenden (zusätzlich zum bereits dokumentierten
`furi_hal_subghz_load_preset()`-Fix in `ideas/protoview-load-preset-removed-api-fix/`, ohne den die
App auf meiner Firmware gar nicht erst startet), `./fbt launch_app APPSRC=protoview` im eigenen
`G4MEOVER-FW`-Baum. Funktionstest: eine eigene PT2262/SC5262-basierte 433-MHz-Fernbedienung
(Garagentor/Torantrieb/Funksteckdose) mehrfach hintereinander senden und mit ProtoView empfangen
(Modulation "OOK 650Khz" oder "OOK 433", je nach Fernbedienung) — vor dem Fix bleibt ein Teil der
Empfänge beim Rohpuls-Bildschirm stehen (kein "PT/SC remote"-Dekodierfeld erscheint), nach dem Fix
muss jeder vollständig empfangene Sendevorgang zuverlässig decodiert werden, unabhängig vom Wert des
gesendeten Codes. Regressionsprobe: die bereits vorher funktionierenden Fälle (Index 0–2) müssen
weiterhin exakt dieselben "id"/"button"-Werte liefern wie vor dem Fix.

## Risiko

Eine Zeile, aktiviert ausschließlich bereits vom Autor selbst deklarierten, aber unerreichbaren Code
— kein neues Muster, keine Änderung der Dekodierlogik nach erfolgter Sync-Erkennung, kein neuer
Sende-Pfad. Reine Empfangs-/Dekodierfunktion für fremde PT2262/SC5262-Signale, fällt wie der Rest von
ProtoView unter passive Signalanalyse auf bereits vorhandene Flipper-SubGhz-Empfangsfunktion.

## Offen

Verbleibende zwei der vier nativ unterstützten Protokoll-Decoder (TPMS-Cluster
Renault/Toyota/Schrader/Citroen/Ford, KeeLoq) sowie die Bit-Edit-/Keyboard-Ansicht selbst bleiben als
Rückstellpunkt für künftige Sitzungen.

## Quelle

`antirez/protoview`, GitHub, BSD-2-Clause, Commit `fd2f6edd43cb813c37a7d912c7dd88f9732d298f`
(2025-05-05, zugleich aktueller `HEAD` des Repositorys, keine neueren Commits), Datei
`protocols/b4b1.c`. Git-History-Beleg für die Entstehung des Fundes: Commit `cc32c5e6fdf25e7a2333bb285715fc412064f89c`
("b4b1: more robust message detection", 2023-01-25).
