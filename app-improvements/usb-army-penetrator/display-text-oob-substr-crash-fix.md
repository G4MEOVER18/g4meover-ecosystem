# usb-army-penetrator — `DISPLAY_TEXT`: ein unvollständiger Ducky-Befehl löst eine ungefangene C++-Ausnahme aus und stürzt das Gerät ab

## Was

`src/Attacks/Ducky/Extensions.h`, `handleDisplayText()` — die Implementierung des
DuckyScript-Befehls `DISPLAY_TEXT <x> <y> <text...>` (zeigt Text auf dem TFT an, über
`extCommands["DISPLAY_TEXT"]` registriert). Die Argumentzerlegung bestimmt die Grenzen von
`x`/`y` und den Start von `text` rein über Längenarithmetik, ohne vorher zu prüfen, ob die
dafür verwendeten `std::string::find()`-Treffer überhaupt existieren:

```cpp
std::string remainingArgs = str.substr(str.find(' ') + 1);

auto xPosStr = remainingArgs.substr(0, remainingArgs.find(' '));
int xPos = asciiOrVariableToInt(xPosStr, variables);

auto yStartPos = xPosStr.length() + 1;
auto yCount = remainingArgs.find(' ', xPosStr.length() + 1) - yStartPos;

auto yPosStr = remainingArgs.substr(yStartPos, yCount);           // (1)
int yPos = asciiOrVariableToInt(yPosStr, variables);

auto text = remainingArgs.substr(xPosStr.length() + yPosStr.length() + 2);  // (2)
```

`std::string::substr(pos, ...)` wirft laut Standard `std::out_of_range`, sobald
`pos > size()` ist. Zwei Aufrufstellen können das auslösen, je nachdem, wie viele
Argumente dem Befehl tatsächlich fehlen:

1. **Fehlt `y` komplett** (z. B. `DISPLAY_TEXT 10` — nur `x`, kein weiteres Token):
   `remainingArgs.find(' ')` für `xPosStr` liefert `npos`, `substr(0, npos)` liefert
   also die gesamte Resthälfte als `xPosStr` (kein Wurf an dieser Stelle). Danach ist
   `yStartPos = xPosStr.length() + 1` größer als `remainingArgs.size()` — Zeile (1)
   wirft sofort.
2. **`y` vorhanden, `text` fehlt** (z. B. `DISPLAY_TEXT 10 20` — kein dritter Token, kein
   abschließendes Leerzeichen): `yPosStr` wird noch korrekt bestimmt (`remainingArgs.find(' ', ...)`
   liefert hier `npos`, aber `substr()` mit `pos <= size()` wirft bei einem reinen Zähl-Argument
   `count` nicht, sondern clamped automatisch), aber
   `xPosStr.length() + yPosStr.length() + 2` liegt dann über `remainingArgs.size()` —
   Zeile (2) wirft.

Weder in `handleDisplayText()` noch in `DuckyPayload::loop()` (`duckyFileParser.Execute(...)`,
`src/Attacks/Ducky/DuckyPayload.cpp`) noch im aufgerufenen Interpreter selbst existiert ein
`try`/`catch`: in der extern eingebundenen Bibliothek
(`lib_deps_core` in `platformio.ini`: `i-am-shodan/DuckScriptInterpreter`, Pin
`23aa3ba87e95f402ccab53beb9ead54602643dd7`) ruft `DuckyInterpreter::Execute()`
(`src/DuckyParse.cpp`, Zeile ~1406) die registrierte Extension-Funktion direkt auf:

```cpp
ret = extCommands.at(command)(line, _constants, _variables) == true ? _lineNumber++ : SCRIPT_ERROR;
```

— kein umgebender `try`/`catch`, in dieser Datei grep-bestätigt null Treffer für
`try`/`catch`. Keine der ESP32-/RP2040-`platformio.ini`-Umgebungen setzt
`-fno-exceptions` (bereits beim vorherigen `std::stoi()`-Fund in `Settings.cpp` geprüft,
hier erneut verifiziert) — eine nicht gefangene `std::out_of_range` läuft also bis
`std::terminate()` durch und reißt die gesamte Firmware ab, nicht nur den
Ducky-Interpreter.

**Erreichbarkeit:** `DISPLAY_TEXT` ist ein regulärer, von mir selbst registrierter
DuckyScript-Befehl (`addDuckyScriptExtensions()`), erreichbar über jedes ausgeführte
`.ds`-Skript (Datei oder `RUN_PAYLOAD`-Kommandozeile) und über
`DuckyPayload::setPayloadCmdLine()` — ein einziger falsch getippter Aufruf mit zu wenigen
Argumenten (`x`/`y` ohne `text`, oder nur `x` ohne `y`/`text`) reicht aus, auch rein
versehentlich beim Testen eines eigenen Skripts.

Zur Abgrenzung: alle anderen Extension-Handler in derselben Datei, die ebenfalls
`str.substr(str.find(' ') + 1)` zur Argumentextraktion nutzen (`handleDisplayPNG`,
`handleUSBMode`, `handleLED`, `handleESP32Marauder`, `handleAgentRun`, `handleLog`,
`handleMouseMove`, `handleWaitForButtonOrTimeout`, `handleRunPayload`, `handleKeyboardLayout`,
`handleRawHid`), geprüft: dort ist `pos = find(' ') + 1` entweder `0` (wenn `find()` `npos`
liefert, da `std::string::npos + 1` für `size_t` definiert zu `0` überläuft — kein
Leerzeichen gefunden, ganze Zeile wird als Argument übernommen, kein Wurf) oder ein
tatsächlich gefundener, gültiger Index — nur `handleDisplayText()` kombiniert **zwei**
solcher Positionen längenarithmetisch zu einer dritten, die dabei über
`remainingArgs.size()` hinauswachsen kann. Kein weiterer Handler in dieser Datei ist von
derselben Fehlerklasse betroffen.

## Nutzen

Verhindert, dass ein unvollständig aufgerufener, selbst registrierter DuckyScript-Befehl
das gesamte Gerät per unbehandelter C++-Ausnahme zum Absturz/Neustart bringt — exakt
dieselbe Fehlerklasse (ungeprüfte C++-Standardbibliotheksfunktion ohne
`try`/`catch` im gesamten Projekt) wie beim bereits behobenen `std::stoi()`-Fund in
`Settings.cpp` (`settings-int-uint16-stoi-uncaught-exception-crash-fix.md`), hier aber
über `std::string::substr()` statt `std::stoi()` und im Ducky-Interpreter-Pfad statt im
Settings-Pfad. Betrifft direkt den eigenen Einsatzzweck: ein laufender autorisierter
Pentest (BadUSB-Payload mit TFT-Statusanzeige) wird durch einen eigenen Tippfehler im
Skript unterbrochen, ohne dass am Zielsystem etwas schiefgelaufen wäre.

## Betroffene Dateien

- `src/Attacks/Ducky/Extensions.h` — nur `handleDisplayText()`. Keine Änderung an
  `asciiOrVariableToInt()`, `Ducky::replaceAllOccurrences()`, `Devices::TFT` oder einer
  anderen Extension-Funktion nötig.

## Fix (Patch)

Grenzen vor jedem `substr()`-Aufruf explizit prüfen, statt sie aus Längensummen
herzuleiten: `find(' ')`-Treffer auf `npos` prüfen, bevor daraus eine Folgeposition
berechnet wird; bei fehlendem Argument denselben "Bad argument"-Logstil wie
`handleUSBMode`/`handleLED` verwenden und `false` zurückgeben (vom Interpreter als
`SCRIPT_ERROR` behandelt — bricht die Skriptausführung kontrolliert ab, statt das
gesamte Gerät abzureißen).

```diff
--- a/src/Attacks/Ducky/Extensions.h
+++ b/src/Attacks/Ducky/Extensions.h
@@ -94,16 +94,28 @@
 static int handleDisplayText(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
 {
     std::string remainingArgs = str.substr(str.find(' ') + 1);
 
-    auto xPosStr = remainingArgs.substr(0, remainingArgs.find(' '));
+    const auto xEnd = remainingArgs.find(' ');
+    if (xEnd == std::string::npos)
+    {
+        Debug::Log.error(LOG_DUCKY, "DISPLAY_TEXT needs x, y and text arguments");
+        return false;
+    }
+
+    auto xPosStr = remainingArgs.substr(0, xEnd);
     int xPos = asciiOrVariableToInt(xPosStr, variables);
 
-    auto yStartPos = xPosStr.length() + 1;
-    auto yCount = remainingArgs.find(' ', xPosStr.length() + 1) - yStartPos;
+    const auto yStartPos = xEnd + 1;
+    const auto yEnd = remainingArgs.find(' ', yStartPos);
+    if (yEnd == std::string::npos)
+    {
+        Debug::Log.error(LOG_DUCKY, "DISPLAY_TEXT needs x, y and text arguments");
+        return false;
+    }
 
-    auto yPosStr = remainingArgs.substr(yStartPos, yCount);
+    auto yPosStr = remainingArgs.substr(yStartPos, yEnd - yStartPos);
     int yPos = asciiOrVariableToInt(yPosStr, variables);
 
-    auto text = remainingArgs.substr(xPosStr.length() + yPosStr.length() + 2);
+    auto text = remainingArgs.substr(yEnd + 1);
 
     for (const auto& pair : variables)
     {
```

Für jeden wohlgeformten Aufruf (`x`, `y`, `text` alle vorhanden, `text` darf intern
beliebig viele Leerzeichen enthalten) ist `yEnd + 1` rechnerisch identisch zu
`xPosStr.length() + yPosStr.length() + 2` — reine Ersetzung der Positionsberechnung
durch bereits vorhandene, tatsächlich gefundene Indizes, kein Verhaltensunterschied im
Erfolgsfall. Vollständiger, per `git apply --check` gegen eine frische, unabhängige
Zweitkopie von `G4MEOVER18/usb-army-penetrator` (`main`, Commit `411b153`) verifizierter
und real angewendeter Patch in `display-text-oob-substr-crash-fix/extensions_h.patch`.

## Testhinweis (ohne Hardware)

Host-kompilierbares Repro in `display-text-oob-substr-crash-fix/repro.cpp` — die reine
Argumentparsing-Logik aus `handleDisplayText()` 1:1 übernommen (`Devices::TFT`,
`Debug::Log`, `Ducky::replaceAllOccurrences()` sind für die Positionsbestimmung selbst
nicht relevant und daher nicht nachgebildet), `asciiOrVariableToInt()` unverändert
mitkopiert, kein Arduino/ESP32-Header nötig:

```
g++ -std=c++17 -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.cpp && ./repro
g++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.cpp && ./repro
```

Lokal ausgeführt (beide Optimierungsstufen identisch, keine Compiler-Warnungen):

```
=== ALT (vor Fix): handleDisplayText() Argumentparsing ===
[ALT] 'DISPLAY_TEXT 10' -> WIRFT std::out_of_range (Absturz)
[ALT] 'DISPLAY_TEXT 10 20' -> WIRFT std::out_of_range (Absturz)
[ALT] 'DISPLAY_TEXT 10 20 Hello World' -> x=10 y=20 text="Hello World"
[ALT] 'DISPLAY_TEXT 10 20 ' (trailing space) -> x=10 y=20 text=""

=== NEU (nach Fix) ===
[NEU] 'DISPLAY_TEXT 10' -> kein Wurf, ret=0
[NEU] 'DISPLAY_TEXT 10 20' -> kein Wurf, ret=0
[NEU] 'DISPLAY_TEXT 10 20 Hello World' -> x=10 y=20 text="Hello World"
[NEU] 'DISPLAY_TEXT 10 20 ' (trailing space) -> x=10 y=20 text=""
[NEU] 'DISPLAY_TEXT $X $Y Status: OK' -> x=5 y=7 text="Status: OK"

11 Pruefungen, 0 fehlgeschlagen.
ALLE CHECKS BESTANDEN
```

Sieben Prüfungen zum Fund: ALT wirft für beide unvollständigen Befehle (Fund bestätigt,
je ein Scenario für jede der zwei Wurfstellen); zwei ALT-Regressionsfälle (wohlgeformter
Aufruf mit mehrteiligem Text, expliziter Leer-Text über trailing Space) bleiben korrekt.
NEU: beide unvollständigen Befehle werfen nicht mehr und werden kontrolliert mit
`ret=false` abgelehnt; drei NEU-Regressionsfälle (wohlgeformter Aufruf, Leer-Text über
trailing Space, Variablen statt Literalen für `x`/`y`) liefern identische Werte wie ALT.
Unter `-O0` und `-O1`: identischer Output, keine ASan-/UBSan-Meldungen.

Funktionaler Test danach lokal (benötigt PlatformIO-Build, hier nicht nachbildbar): ein
`.ds`-Skript mit `DISPLAY_TEXT 10 20` (ohne Textargument) ausführen — vor dem Fix startet
das Gerät neu (Serial-Monitor zeigt den C++-Abort/Backtrace,
`monitor_filters = esp32_exception_decoder` ist in `platformio.ini` bereits aktiv), nach
dem Fix erscheint die Log-Zeile "DISPLAY_TEXT needs x, y and text arguments" und das
Skript bricht kontrolliert ab. Danach denselben Test mit einem vollständigen Aufruf
wiederholen (z. B. `DISPLAY_TEXT 10 20 Hello`) — Textanzeige funktioniert weiterhin wie
vorher.

## Risiko

- Niedrig: rein additiv (zwei neue, frühe Rückkehrpunkte mit Logging, sonst nur
  Umbenennung bereits vorhandener Positionsvariablen). Für jeden bisher gültigen Aufruf
  (Host-Repro-Regressionsfälle) ist das Verhalten identisch zu vorher.
- Ändert den Rückgabewert für den bisher kaputten Fall von "Absturz" auf `false`
  (`SCRIPT_ERROR`) — ein Verhaltensunterschied, aber ausschließlich für einen Fall, der
  zuvor ohnehin nicht funktionierte (Geräteabsturz statt Textanzeige).
- Keine Änderung an `Devices::TFT`, am Wire-Format eines anderen Befehls oder an der
  Registrierungstabelle `extCommands` selbst.

## Quelle

- `G4MEOVER18/usb-army-penetrator` (main, Commit `411b153`): `src/Attacks/Ducky/Extensions.h`
  (`handleDisplayText()`, `asciiOrVariableToInt()`, `addDuckyScriptExtensions()`),
  `src/Attacks/Ducky/DuckyPayload.cpp` (`DuckyPayload::loop()`, `duckyFileParser.Execute()`),
  `platformio.ini` (`lib_deps_core`: `i-am-shodan/DuckScriptInterpreter`, Pin
  `23aa3ba87e95f402ccab53beb9ead54602643dd7`; alle Build-Umgebungen gegen
  `-fno-exceptions` geprüft — nirgends gesetzt).
- `i-am-shodan/DuckScriptInterpreter` (Commit `23aa3ba`): `src/DuckyParse.cpp`,
  `DuckyInterpreter::Execute()` (Zeile ~1406, direkter Aufruf der Extension-Funktion ohne
  `try`/`catch`; Datei insgesamt grep-geprüft, keine Treffer für `try`/`catch`).
- Eigener vorheriger Fund in diesem Repo (dieselbe Fehlerklasse, andere Aufrufstelle):
  `app-improvements/usb-army-penetrator/settings-int-uint16-stoi-uncaught-exception-crash-fix.md`.
