# ProtoView — Oregon2-Dekoder: zwei Logikfehler in der Nibble-Verarbeitung

## Was

Fortsetzung der [ProtoView](https://github.com/antirez/protoview)-Prüfung
(`antirez/protoview`, BSD-2-Clause, Commit `fd2f6ed`): nach dem bereits dokumentierten
API-87-Kompatibilitätsfund (`ideas/protoview-digital-signal-tool-app-status.md`) war
diese Prüfung ausschließlich auf API-Kompatibilität beschränkt — die vier nativ
unterstützten Protokoll-Decoder selbst (TPMS, KeeLoq, Oregon 2, PT2262/SC5262) standen
seitdem als eigener Rückstellpunkt offen. Diese Sitzung nimmt den kleinsten der vier
(`protocols/oregon2.c`, 70 Zeilen) zeilenweise auf eigene Logikfehler vor — unabhängig
vom bereits erledigten Firmware-API-Fund, der eine andere Datei betrifft.

Zwei voneinander unabhängige Fehler in `decode()`s Nibble-Verarbeitungsschleife:

1. **`deviceid[2]` wird nie initialisiert, aber über `|=` gelesen, bevor je etwas
   hineingeschrieben wurde.** Direkt oberhalb werden `temp[3]`/`hum[2]` korrekt mit
   `= {0}` initialisiert — nur `uint8_t deviceid[2];` fehlt dieser Initialisierer. Die
   Schleife baut jedes Byte aus zwei Nibbln per Oder-Verknüpfung zusammen
   (`deviceid[0] |= nib[0]<<4` für die obere, `deviceid[0] |= nib[0]` für die untere
   Nibble, analog `deviceid[1]`) — ein Muster, das voraussetzt, dass das Ziel vorher auf
   0 steht. Ohne Initialisierer liest der allererste dieser beiden Zugriffe pro Byte
   unbestimmten Stack-Inhalt, bevor überhaupt geschrieben wird.
2. **`temp[]`/`hum[]` werden mit dem rohen BCD-Ziffernwert (0–9) aus `nib[0]` belegt,
   aber in der Auswerteformel ein zweites Mal wie ein ASCII-Zeichen behandelt.** `nib[0]`
   ist bereits der numerische Ziffernwert (z. B. `3` für die Ziffer "3"), kein
   ASCII-Zeichen `'3'` (0x33) — `temp[0] = nib[0];`/`hum[0] = nib[0];` usw. übernehmen
   diesen Wert unverändert. Die anschließende Formel `(temp[0]-'0')*10 + …` zieht aber
   zusätzlich `'0'` (48) ab, als wäre `temp[0]` ein ASCII-Zeichen, das erst in einen
   Zahlenwert umgerechnet werden müsste. Das verschiebt jede einzelne Dezimalziffer um
   -48 — bei jedem gültigen Signal, nicht nur in einem Randfall.

## Nutzen

Zwei der vier noch unbegutachteten Protokoll-Decoder-Dateien betreffen nicht nur
API-Kompatibilität, sondern eigene Logikfehler von ProtoView selbst — dieser Fund
dokumentiert und behebt beide für den kleinsten/einfachsten der vier (`Oregon2`),
reduziert den offenen Rückstellpunkt entsprechend und liefert das Vorgehen (Nibble für
Nibble gegen eine unabhängig berechnete Ground Truth prüfen) als Vorlage für die
verbleibenden drei (TPMS-Cluster, KeeLoq, PT2262/SC5262).

## Betroffene Dateien

- `protocols/oregon2.c` (`antirez/protoview`, Funktion `decode()`) — beide Fixe.

## Fund im Detail

### 1. Unbenutzter `deviceid[2]`-Initialisierer

```c
char temp[3] = {0}, hum[2] = {0};
uint8_t deviceid[2];                     // <- kein Initialisierer
for (int j = 0; j < 64; j += 4) {
    ...
    switch(j/4) {
    case 1: deviceid[0] |= nib[0]; break;        // erster Zugriff auf deviceid[0]
    case 0: deviceid[0] |= nib[0] << 4; break;    // bei j=0 zuerst ausgeführt
    case 3: deviceid[1] |= nib[0]; break;
    case 2: deviceid[1] |= nib[0] << 4; break;
    ...
```

Die Schleife läuft `j = 0, 4, 8, …`, `switch(j/4)` trifft also zuerst `case 0`
(`j/4==0`) — der allererste Zugriff auf `deviceid[0]` ist bereits ein Lese-Modifizier-
Schreib-Zugriff (`|=`) auf nie initialisierten Speicher. Das vom Gerät angezeigte
"Sensor ID"-Feld enthält dadurch zusätzliche, vom Stack-Vorzustand abhängige Bits —
nicht reproduzierbar, je nach vorherigem Aufrufkontext ein anderer Wert für dieselbe
Karte/denselben Sensor.

### 2. Doppelt angewandter ASCII-Offset bei Temperatur/Feuchte

```c
case 10: temp[0] = nib[0]; break;   // nib[0] ist der rohe Ziffernwert 0..9
case 9:  temp[1] = nib[0]; break;
case 8:  temp[2] = nib[0]; break;
case 13: hum[0] = nib[0]; break;
case 12: hum[1] = nib[0]; break;
...
float tempval = ((temp[0]-'0')*10) + (temp[1]-'0') + ((float)(temp[2]-'0')*0.1);
int humval = (hum[0]-'0')*10 + (hum[1]-'0');
```

`nib[0]` ist die direkt aus vier Bits berechnete Ziffer (0–15, für gültige BCD-Daten
0–9) — kein ASCII-Zeichen. `temp[]`/`hum[]` übernehmen diesen Wert unverändert. Die
Formel behandelt `temp[0]` aber so, als müsste erst ein ASCII-Zeichen in seinen
Zahlenwert umgerechnet werden (Standard-Idiom `ziffer - '0'`), und zieht zusätzlich 48
ab. Für eine reale Anzeige von 23,4 °C/55 % rF liefert die Formel stattdessen
-509,4 °C/-473 % rF (siehe Host-Repro unten) — bei jedem einzelnen gültigen Signal, kein
Rand- oder Sonderfall. Praktisch macht das die beiden einzigen numerischen
Nutzdatenfelder des Decoders (Temperatur, Feuchte) unbrauchbar; "Sensor ID" bleibt durch
Fund 1 zusätzlich unzuverlässig.

Beide Fehler sind unabhängig voneinander entstanden (unterschiedliche Variablen,
unterschiedliche Codestellen) und wurden in derselben Datei im selben Durchlauf
gefunden.

## Fix

Minimal-invasiv, zwei Einzelstellen in `protocols/oregon2.c`, kein Verhaltensunterschied
für `raw[]`, keine Änderung der Bit-Extraktionslogik selbst:

1. `uint8_t deviceid[2];` → `uint8_t deviceid[2] = {0};` — Fund 1, Initialisierer
   ergänzt, analog zu `temp[3] = {0}, hum[2] = {0}` in derselben Zeile.
2. `float tempval = ((temp[0]-'0')*10) + (temp[1]-'0') + ((float)(temp[2]-'0')*0.1);`
   → `float tempval = (temp[0]*10) + temp[1] + ((float)temp[2]*0.1);`, ebenso
   `int humval = (hum[0]-'0')*10 + (hum[1]-'0');` → `int humval = hum[0]*10 + hum[1];`
   — Fund 2, der doppelt angewandte ASCII-Offset entfällt, `temp[]`/`hum[]` werden als
   das verwendet, was sie bereits enthalten (roher Ziffernwert).

Patch in `ideas/protoview-oregon2-decoder-logic-fixes/oregon2_c.patch`. Gegen zwei
frische, unabhängige Zweitkopien von `antirez/protoview`
(`fd2f6edd43cb813c37a7d912c7dd88f9732d298f`) per `git apply --check`/`git apply` und per
`patch -p1` verifiziert, Ergebnisdatei in beiden Fällen laut `md5sum` byte-identisch zur
lokal vorbereiteten Zieldatei.

## Host-Repro

`ideas/repro-protoview-oregon2-decoder-logic-fixes/`: Der Nibble-Verarbeitungsblock aus
`decode()` (die `for`-Schleife samt `switch` und die beiden Auswerteformeln) wörtlich in
zwei Varianten extrahiert (`oregon2_extract_ALT.c`/`_NEU.c`), `bitmap_get()`/
`bitmap_set()` wörtlich aus `signal.c` übernommen (`mocks.c`). Eine eigene
`set_nibble()`-Hilfsfunktion baut aus vorgegebenen Ziffern/Byte-Werten ein
`buffer[8]`-Bitmuster, exakt an den Nibble-Indizes, die der echte `switch(j/4)`
tatsächlich ausliest (0/1 → `deviceid[0]`, 2/3 → `deviceid[1]`, 10/9/8 →
`temp[0..2]`, 13/12 → `hum[0..1]`) — unabhängig von Decoder-internen Annahmen, als
eigene Ground Truth.

- `gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined`
  bei `-O0`/`-O1`, drei Szenarien (23,4 °C/55 %; 0,0 °C/0 % als Grenzfall;
  9,9 °C/99 % als maximale BCD-Ziffern): `21/21 Prüfungen bestanden`, `ALL CHECKS
  PASSED` bei beiden Optimierungsstufen, keine Compiler-Warnungen in `oregon2_extract_NEU.c`.
  In `oregon2_extract_ALT.c` meldet GCC bei `-O1` von sich aus
  `-Wmaybe-uninitialized` exakt an den beiden `deviceid[]`-Zugriffsstellen — eine
  unabhängige Zweitbestätigung von Fund 1 durch den Compiler selbst. NEU liefert in
  allen drei Szenarien exakt die vorgegebene reale Temperatur/Feuchte/Sensor-ID; ALT
  liefert exakt den unabhängig vorausberechneten, falschen Wert der doppelt
  ASCII-versetzten Formel (z. B. -509,4/-473 für das erste Szenario) und weicht in
  jedem Szenario vom realen Wert ab.
- Für Fund 1 separat mit Valgrind Memcheck (`--track-origins=yes`) statt ASan geprüft,
  da ein unbenutzter Initialisierer kein Speicherfehler, sondern ein
  Uninitialized-Value-Fehler ist: Memcheck meldet bei ALT exakt "Uninitialised value was
  created by a stack allocation at oregon2_extract_ALT.c:14" (die
  `uint8_t deviceid[2];`-Deklaration) und zusätzlich einen konkreten falschen
  Ergebniswert (`deviceid=0x0400` statt der für ein Nullpuffer-Szenario korrekten
  `0x0000`); ein zweiter, eigenständiger NEU-only-Lauf ist vollständig
  Valgrind-clean (`ERROR SUMMARY: 0 errors`). `build.sh` führt alle vier Läufe aus und
  bricht bei jeder Abweichung vom erwarteten Resultat ab.

## Testhinweis (lokal)

Patch gegen die eigene `antirez/protoview`-Kopie anwenden, dann `./fbt launch_app
APPSRC=protoview` innerhalb des eigenen `G4MEOVER-FW`-Baums (siehe bereits
dokumentierten `furi_hal_subghz_load_preset()`-Fix in
`ideas/protoview-load-preset-removed-api-fix/`, ohne den die App auf meiner Firmware gar
nicht erst startet). Funktionstest: ein eigenes Oregon-Scientific-Thermo-/Hygrometer
(433,92 MHz, Protokoll v2.1) empfangen und "Sensor ID"/"Temperature"/"Humidity" mit dem
tatsächlichen Anzeigewert des Sensors selbst vergleichen — vor dem Fix muss die
angezeigte Temperatur/Feuchte grob um 48 je Ziffernstelle danebenliegen (z. B. deutlich
negative Temperatur/Feuchte statt eines plausiblen Wertes), nach dem Fix muss sie
übereinstimmen. Die Sensor-ID muss über mehrere Empfänge desselben Sensors hinweg
konstant bleiben (vor dem Fix potenziell bei jedem Start/jeder erneuten Dekodierung
unterschiedlich, je nach Stack-Vorzustand).

## Risiko

Rein additiv/korrigierend an zwei bestehenden Zeilen, keine Änderung der
Bit-Extraktions- oder Signal-Erkennungslogik, kein neuer Sende-Pfad. Reine
Empfangs-/Anzeigefunktion für fremde Umweltsensordaten (Temperatur/Feuchte/Geräte-ID),
kein Eingriff in Ziel-Hardware — fällt wie der Rest von ProtoView unter reine passive
Signalanalyse.

## Offen

Die übrigen drei noch nicht logikgeprüften Decoder (TPMS Renault/Toyota/Schrader/
Citroen/Ford, KeeLoq, PT2262/SC5262) sowie die Bit-Edit-/Keyboard-Ansicht selbst bleiben
als Rückstellpunkt für eine künftige Sitzung offen.

## Quelle

`antirez/protoview`, GitHub, BSD-2-Clause, Commit
`fd2f6edd43cb813c37a7d912c7dd88f9732d298f` (2025-05-05), Datei `protocols/oregon2.c`.
