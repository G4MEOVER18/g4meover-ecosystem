# usb-army-penetrator — RAW_HID: ungeprüfter `std::stoi()` im Ducky-Handler stürzt das Gerät ab

## Was

`src/Attacks/Ducky/Extensions.h`, Funktion `handleRawHid()` (Handler für den DuckyScript-Befehl
`RAW_HID`). Für das Hex-Argument wird nach dem `"0x"`-Präfix-Check ungeprüft `std::stoi()`
aufgerufen:

```cpp
if (hidStr.rfind("0x", 0) == 0)
{
    auto tempInt = std::stoi(hidStr, nullptr, 16);
    if (tempInt <= static_cast<int>(UINT8_MAX) && tempInt >= 0)
    { ... }
    else { Debug::Log.error(LOG_DUCKY, "Invalid value"); }
}
```

Das ist dieselbe Schwachstellenklasse wie der bereits behobene `settings-int-uint16-stoi-
uncaught-exception-crash-fix` (`src/Utilities/Settings.cpp`): `std::stoi()` wirft eine
C++-Ausnahme, wenn der numerische Wert außerhalb des `int`-Bereichs liegt
(`std::out_of_range`). Weder in dieser Funktion noch irgendwo sonst im Projekt existiert ein
`try`/`catch` (per `grep -rn "catch\s*(\|try\s*{" src/` im gesamten `src/`-Baum bestätigt — null
Treffer, unverändert seit dem Settings.cpp-Fund). Das `platformio.ini` setzt für keine Umgebung
`-fno-exceptions`, eine nicht gefangene Ausnahme läuft also bis `std::terminate()` durch und
reißt das gesamte Firmware-Image ab, nicht nur den Ducky-Interpreter.

**Auslöser:** jeder Hex-Wert mit `"0x"`-Präfix, bei dem direkt eine gültige Hex-Ziffer folgt
(damit die `strtol`-Präfixregel greift) und dessen Zahlenwert ≥ `0x80000000` (über `INT_MAX`)
liegt — z. B. `RAW_HID 0x80000000` oder `RAW_HID 0xFFFFFFFF`. `std::stoi()` prüft den
`int`-Zielbereich zusätzlich zur eigentlichen `strtol`-Konvertierung und wirft
`std::out_of_range`, sobald der Wert diesen Bereich verlässt — unabhängig davon, ob die
Zwischendarstellung (`long`) selbst noch passt. `RAW_HID` ist über jedes `.ds`-Payload
erreichbar (eigenes Skript, aus `RUN_PAYLOAD` nachgeladenes Skript, oder ein von
`AGENT_RUN`/`ESP32M` indirekt beeinflusster Ablauf) — ein einzelner Tippfehler oder ein absichtlich
präpariertes Skript reicht aus, um das Gerät mitten im Lauf abstürzen zu lassen.

Beim Review selbst geprüft und bewusst **nicht** als weiteren Fund behauptet — dieselbe bereits
beim Settings.cpp-Fund dokumentierte `strtol`-Präfixregel gilt auch hier: `"0x"` allein oder ein
ungültiger Hex-Rest (`"0xZZZZ"`) wird nicht als Präfix erkannt, `std::stoi()` parst dann nur die
führende `"0"` und gibt `0` zurück, ohne zu werfen — kein Absturzpfad, nur ein harmloser
Stillparse auf `0x00` (durch das Host-Repro gegengeprüft).

## Nutzen

Verhindert, dass ein einzelnes `RAW_HID`-Kommando mit einem zu großen Hex-Wert — eigenes
Skript, nachgeladenes `.ds`-Payload oder ein Tippfehler — das gesamte Gerät per unbehandelter
C++-Ausnahme zum Absturz/Neustart bringt. Betrifft direkt den eigenen Einsatzzweck: ein laufender
autorisierter Pentest (HID-Injection-Sequenz) wird unterbrochen, ohne dass am Zielsystem etwas
falsch gelaufen wäre — der Bug liegt vollständig im eigenen Parser. Schließt damit einen der in
`settings-int-uint16-stoi-uncaught-exception-crash-fix.md` bereits als Fehlerklasse benannten,
aber dort bewusst nicht mitgeprüften weiteren `std::stoi()`-Aufrufstellen in Extensions.h ab (der
seit mehreren Sitzungen als Rückstellpunkt "`usb-army-penetrator`s restliche Ducky-Handler"
offene Review).

## Betroffene Dateien

- `src/Attacks/Ducky/Extensions.h` — nur `handleRawHid()`, plus ein neuer `#include <stdexcept>`.
  Keine Änderung an `Ducky::SplitString()`, `asciiOrVariableToInt()` oder einem anderen Handler.

Beim selben Durchgang gezielt gegen das gleiche Fehlermuster (`std::stoi`/`std::stol`/`atoi` ohne
Validierung) durchsucht (`grep -n "std::stoi\|std::stol\b" src/Attacks/Ducky/Extensions.h`):
`asciiOrVariableToInt()` verwendet `atoi()` statt `std::stoi()` (kein Wurf bei ungültiger Eingabe,
`atoi()` gibt bei Fehlparse `0` zurück) — nicht betroffen. Kein weiterer `std::stoi()`-Aufruf in
dieser Datei.

## Fix (Patch)

`try`/`catch` um den `std::stoi()`-Aufruf, analog zum bereits etablierten Muster aus dem
Settings.cpp-Fix: fängt `std::out_of_range`/`std::invalid_argument` (beide über
`std::exception`), loggt denselben Stil wie der bereits vorhandene `"Invalid value"`-Zweig und
kehrt mit `false` zurück (Funktion gibt ohnehin `bool`/`int` als Rückgabewert, kein Verhaltens-
unterschied für jeden bereits gültigen Wert).

```diff
--- a/src/Attacks/Ducky/Extensions.h
+++ b/src/Attacks/Ducky/Extensions.h
@@ -28,6 +28,8 @@
 
 #include "DuckyPayload.h"
 
+#include <stdexcept>
+
 // For VID and PID str handling
 static std::string vidStr;
 static const std::string Constant_VID = "#_VID_";
@@ -623,7 +625,16 @@ static int handleRawHid(const std::string &str, const std::unordered_map<std::st
 
     if (hidStr.rfind("0x", 0) == 0)
     {
-        auto tempInt = std::stoi(hidStr, nullptr, 16);
+        int tempInt = 0;
+        try
+        {
+            tempInt = std::stoi(hidStr, nullptr, 16);
+        }
+        catch (const std::exception &)
+        {
+            Debug::Log.error(LOG_DUCKY, "Invalid value, not a valid hex integer");
+            return false;
+        }
         if (tempInt <= static_cast<int>(UINT8_MAX) && tempInt >= 0)
         {
             uint8_t uint8Value = 0;
```

Vollständiger, per `git apply --check` gegen zwei frische, unabhängige Zweitkopien von
`G4MEOVER18/usb-army-penetrator` (`main`, Commit `411b153`) verifizierter Patch — real
angewendet, beide Ergebnisse per `md5sum` gegen die eigene Arbeitskopie abgeglichen: identisch.
Patch in `raw-hid-stoi-uncaught-exception-crash-fix/extensions_h.patch`.

Das `return false;` innerhalb des `catch`-Blocks verlässt die Funktion direkt (kein `switch`,
anders als beim Settings.cpp-Fix) — unproblematisch, da `ret` an dieser Stelle noch nicht auf
`true` gesetzt wurde und kein Aufräumcode danach folgt.

## Testhinweis

Host-kompilierbares Repro in `repro-raw-hid-stoi-uncaught-exception/repro.cpp` — reine
`std::stoi()`/Ausnahme-Logik 1:1 aus dem `hidStr`-Zweig von `handleRawHid()` übernommen (ALT =
Originalzustand ohne `try`/`catch`, NEU = Zustand nach dem Patch), kein Arduino/TinyUSB/HID-Objekt
nötig (die eigentliche Tastendruck-Ausgabe ist für diesen Fund irrelevant):

```
g++ -std=c++17 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.cpp && ./repro
g++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.cpp && ./repro
```

Lokal ausgeführt (beide Optimierungsstufen identisch):

```
=== ALT (vor Fix): Extensions.h handleRawHid() hidStr-Zweig ===
[ALT] RAW_HID 0x80000000 -> WIRFT (Absturz)
[ALT] RAW_HID 0xFFFFFFFF -> WIRFT (Absturz)
[ALT] RAW_HID 0x100000000 -> WIRFT (Absturz)
[ALT] RAW_HID 0x (allein) -> kein Wurf, accepted=1
[ALT] RAW_HID 0xZZZZ -> kein Wurf, accepted=1

=== NEU (nach Fix) ===
[NEU] RAW_HID 0x80000000 -> kein Wurf, accepted=0
[NEU] RAW_HID 0xFFFFFFFF -> kein Wurf, accepted=0
[NEU] RAW_HID 0x100000000 -> kein Wurf, accepted=0

27 Pruefungen, 0 fehlgeschlagen.
ALLE CHECKS BESTANDEN
```

27 Prüfungen: (1)–(3) ALT wirft für drei Hex-Werte ≥ `0x80000000` (`0x80000000`, `0xFFFFFFFF`,
`0x100000000`) — Fund bestätigt, unabhängig von der genauen Überlaufgröße; (4)–(5) Gegenprobe
zur `strtol`-Präfixregel: `"0x"` allein und `"0xZZZZ"` werfen nicht, werden aber korrekt als `0`
interpretiert und akzeptiert (kein Absturzpfad, bereits beim Settings.cpp-Fund etabliert, hier
nur gegen den `RAW_HID`-Zweig erneut bestätigt); vier ALT-Regressionsfälle (`0xFF`/`0x00`
akzeptiert, `0x100`/`0x7FFFFFFF` regulär — ohne Wurf — abgelehnt, da außerhalb `UINT8_MAX` aber
innerhalb des `int`-Bereichs); NEU: alle drei fund-auslösenden Eingaben kehren jetzt regulär
zurück (`accepted=false`, kein Wurf nach außen); sechs NEU-Regressionsfälle (identisches
Verhalten zu ALT für alle sechs bereits vor dem Fix gültigen/korrekt abgelehnten Eingaben). Unter
`-O0` und `-O1`: ALLE CHECKS BESTANDEN, identischer Output, keine ASan-/UBSan-Meldungen, keine
Compiler-Warnungen.

Lokal zusätzlich (sobald der Patch in die eigene Firmware übernommen ist, PlatformIO-Build
nötig, hier nicht nachbildbar): ein `.ds`-Testskript mit der Zeile `RAW_HID 0x80000000` über
`RUN_PAYLOAD` oder als Startpayload laufen lassen — vor dem Fix startet das Gerät neu
(Serial-Monitor zeigt den C++-Abort/Backtrace, `monitor_filters = esp32_exception_decoder` ist
in `platformio.ini` bereits aktiv), nach dem Fix bleibt es stabil, die Ducky-Ausführung läuft mit
der nächsten Skriptzeile weiter und die Log-Zeile "Invalid value, not a valid hex integer"
erscheint. Zusätzlich `RAW_HID 0x04` (gültiger HID-Code) wiederholen — Tastendruck wird
weiterhin wie vorher ausgelöst.

## Risiko

Niedrig. Rein additiv (`try`/`catch` um einen bestehenden Aufruf, ein neuer Include), keine
Änderung an der Bereichsprüfung, an `Ducky::SplitString()`/`asciiOrVariableToInt()` oder an der
öffentlichen Signatur von `handleRawHid()`. Für jeden bisher gültigen Wert (HID-Code im
`UINT8_MAX`-Bereich, korrekt abgelehnter zu großer, aber noch `int`-gültiger Wert) ist das
Verhalten byte-identisch zu vorher — nur der bisher ungefangene Absturzpfad wird durch denselben
Log-Stil ersetzt, der für den bereits durch die Bereichsprüfung abgedeckten Fall ohnehin schon
existiert.

## Sicherheitsrahmen

Reiner Stabilitätsfund in der eigenen DuckyScript-Interpreter-Schicht (kein Netzwerk-/Funkkanal,
keine Interaktion mit einem Zielsystem außer der ohnehin vom Skript beabsichtigten
HID-Tastendruck-Ausgabe). Fix ist eine kontrollierte Ablehnung eines ungültigen Arguments, keine
neue Funktionalität, kein verändertes Verhalten für ein korrekt formuliertes `RAW_HID`-Kommando.

## Quelle

- `G4MEOVER18/usb-army-penetrator` (main, Commit `411b153`): `src/Attacks/Ducky/Extensions.h`
  (`handleRawHid()`, Zeile ~608), `platformio.ini` (erneut gegen `-fno-exceptions` geprüft —
  nirgends gesetzt, unverändert seit dem Settings.cpp-Fund).
- Eigener vorheriger Fund in diesem Repo (dieselbe Schwachstellenklasse, andere Aufrufstelle):
  `app-improvements/usb-army-penetrator/settings-int-uint16-stoi-uncaught-exception-crash-fix.md`.
