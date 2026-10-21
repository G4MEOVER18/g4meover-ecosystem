# usb-army-penetrator — SET_SETTING_STRING verwirft jeden Wert mit Leerzeichen statt ihn zu übernehmen

## Was

`src/Attacks/Ducky/Extensions.h`, Funktion `handleSetSetting()` (gemeinsamer Handler für die
DuckyScript-Befehle `SET_SETTING_BOOL`, `SET_SETTING_INT16`, `SET_SETTING_UINT16` und
`SET_SETTING_STRING`). Dies ist der konkrete, seit der Fortsetzung-2-Sitzung als eigenständiger
Rückstellpunkt benannte nächste Teil der "restlichen Ducky-Handler"-Liste: die
`std::stoi()`-Aufrufstelle dahinter in `Settings.cpp` ist bereits behoben
(`settings-int-uint16-stoi-uncaught-exception-crash-fix.md`), die Argument-Aufteilung in
`handleSetSetting()` selbst war bislang nicht gezielt geprüft.

Aktueller Zustand:

```cpp
const auto split = Ducky::SplitString(str);
if (split.size() != 3)
{
    Debug::Log.error(LOG_DUCKY, "SET_SETTING needs two arguments has: "+std::to_string(split.size() - 1));
    totalErrors++;
    break;
}

const auto settingName = split[1];
auto settingValue = split[2];
```

`Ducky::SplitString()` (aus der eingebundenen `i-am-shodan/DuckScriptInterpreter`-Bibliothek,
`DuckyParse.h`, Pin-Commit `23aa3ba8` laut `platformio.ini`) zerlegt die komplette Zeile an jedem
Whitespace-Zeichen in einzelne Wörter — ohne jede Anführungszeichen-/Escape-Unterstützung. Für
`SET_SETTING_STRING <Name> <Wert>` liefert das nur dann genau drei Teile, wenn `<Wert>` selbst
kein einziges Leerzeichen enthält.

**Fund:** Jeder `SET_SETTING_STRING`-Aufruf, dessen Wert mehr als ein Wort ist, erzeugt mehr als
drei Teile — die Zeile wird komplett verworfen (`"SET_SETTING needs two arguments has: N"`
geloggt, `totalErrors++`, Einstellung bleibt unverändert), obwohl der Rest der Zeile klar als
ein einziger String-Wert gemeint war. Das betrifft reale, bereits im eigenen Projekt registrierte
`String`-Einstellungen direkt:

- `WIFI_AP_NAME`/`WIFI_AP_PWD` (`src/Devices/WiFi/HardwareWiFi.cpp`) — der eigene WLAN-AP-Name
  bzw. das WLAN-Passwort; beides plausibel mehrwortig (z. B. `Free WiFi` oder eine Passphrase mit
  Leerzeichen).
- `USB_DeviceManufacturer`/`USB_DeviceProductDescriptor` (`src/Devices/USB/USBCore.cpp`) — die
  USB-Deskriptor-Strings, die ein Zielsystem beim Enumerieren anzeigt; realistische Werte wie
  `Generic Corp` oder `USB Flash Drive` sind mehrwortig.

In `setSettingValue()` (`src/Utilities/Settings.cpp:50-53`) ist der `String`-Zweig selbst
uneingeschränkt (`prefs.putString(name.c_str(), value.c_str())` ohne weitere Prüfung) — die
Speicherschicht unterstützt beliebige Leerzeichen im Wert vollständig. Die Einschränkung entsteht
ausschließlich durch die Zerlegung in `handleSetSetting()`, nicht durch eine echte Grenze der
Einstellung selbst.

Beim Review bewusst geprüft und **nicht** als weiteren Fund behauptet: mehrere Leerzeichen
zwischen Name und Wert, oder innerhalb des Werts, erzeugen durch `Ducky::SplitString()` keine
leeren Zwischen-Token (die Funktion überspringt aufeinanderfolgenden Whitespace), das Verhalten
ist also für `SET_SETTING_BOOL`/`_INT16`/`_UINT16` (deren Werte ohnehin nie Leerzeichen enthalten
dürfen) unverändert — nur `SET_SETTING_STRING` ist betroffen.

## Nutzen

Macht `SET_SETTING_STRING` für genau die Einstellungen nutzbar, für die es im eigenen Projekt
bereits registriert ist, aber bislang nur einwortige Werte zuließ. Ohne den Fix lässt sich über
ein DuckyScript kein WLAN-AP-Name/-Passwort mit Leerzeichen und keine mehrwortige USB-Deskriptor-
Zeichenkette setzen — ein Skript, das das versucht, schlägt beim `SET_SETTING_STRING`-Aufruf
sofort fehl und bricht die vorgesehene Konfiguration ab, statt (wie vom Dateiformat her
offensichtlich gemeint) den Rest der Zeile als einzelnen Wert zu übernehmen.

## Betroffene Dateien

- `src/Attacks/Ducky/Extensions.h` — nur `handleSetSetting()`. Keine Änderung an
  `Ducky::SplitString()` (aus der Bibliothek, nicht im eigenen Baum), `startsWith()`,
  `setSettingValue()`/`Settings.cpp` oder einem anderen Handler.

## Fix (Patch)

Additiv: Für `SET_SETTING_STRING` wird die Mindestanzahl der Teile statt der exakten Anzahl
geprüft (`split.size() < 3` statt `!= 3`, zusätzlich weiterhin `!= 3` für alle anderen drei
Varianten — deren Validierung bleibt byte-identisch zum bisherigen Verhalten). Der eigentliche
Wert wird dann nicht aus `split[2]` übernommen, sondern aus der Originalzeile neu ausgeschnitten:
ab dem Ende des gefundenen Einstellungsnamens wird der führende Trenn-Whitespace übersprungen,
der Rest der Zeile bildet unverändert (inklusive innerer Leerzeichen) den Wert.

```diff
--- a/src/Attacks/Ducky/Extensions.h
+++ b/src/Attacks/Ducky/Extensions.h
@@ -239,7 +239,9 @@ static int handleSetSetting(const std::string &str, const std::unordered_map<std
     do
     {
         const auto split = Ducky::SplitString(str);
-        if (split.size() != 3)
+        const bool isStringSetting = startsWith(str, "SET_SETTING_STRING");
+
+        if (split.size() < 3 || (!isStringSetting && split.size() != 3))
         {
             Debug::Log.error(LOG_DUCKY, "SET_SETTING needs two arguments has: "+std::to_string(split.size() - 1));
             Debug::Log.error(LOG_DUCKY, str);
@@ -250,6 +252,17 @@ static int handleSetSetting(const std::string &str, const std::unordered_map<std
         const auto settingName = split[1];
         auto settingValue = split[2];
 
+        if (isStringSetting)
+        {
+            // SET_SETTING_STRING values (WiFi SSID/password, USB descriptor strings, ...)
+            // may contain whitespace, which Ducky::SplitString() above has already cut at
+            // the first space. Reconstruct the full value from the original line instead
+            // of using split[2] alone.
+            const auto nameEnd = str.find(settingName, str.find(' ') + 1) + settingName.length();
+            const auto valueStart = str.find_first_not_of(" \t", nameEnd);
+            settingValue = (valueStart == std::string::npos) ? std::string() : str.substr(valueStart);
+        }
+
         if (startsWith(str, "SET_SETTING_UINT16") && settingValue.rfind("0x", 0) != 0)
         {
             // need to add 0x to front
```

Vollständiger, per `git apply --check` gegen zwei frische, unabhängige Zweitkopien von
`G4MEOVER18/usb-army-penetrator` (`main`, Commit `411b153`) verifizierter Patch — real
angewendet, beide Ergebnisse per `md5sum` gegen die eigene Arbeitskopie abgeglichen: identisch.
Patch in `set-setting-string-whitespace-value-fix/extensions_h.patch`.

`str.find(settingName, str.find(' ') + 1)` sucht den Namen ab der ersten Stelle nach dem
Befehlswort (statt ab Zeilenanfang), damit ein Name, der zufällig mit dem Befehlswort kollidiert,
nicht fälschlich vorn gefunden wird; da `Ducky::SplitString()` den Namen bereits als eigenes
Token erkannt hat, liegt die erste Fundstelle ab dieser Position garantiert exakt am
tatsächlichen Namens-Token — unabhängig davon, wie viele zusätzliche Leerzeichen zwischen
Befehlswort und Namen stehen.

## Testhinweis

Host-kompilierbares Repro in `repro-set-setting-string-whitespace-value/repro.cpp` — `handleSetSetting()`
in ALT- (Originalzustand) und NEU-Form (nach Patch) sowie `Ducky::SplitString()` 1:1
transkribiert, `setSettingValue()` durch einen Stand-in ersetzt, der nur Name/Wert/Rückgabewert
aufzeichnet (kein `Preferences`/NVS-Objekt nötig, da der Fund ausschließlich die Argument-
Zerlegung betrifft):

```
g++ -std=c++17 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.cpp && ./repro
g++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.cpp && ./repro
```

Lokal ausgeführt (beide Optimierungsstufen identisch, keine Compiler-Warnungen, keine
Sanitizer-Befunde):

```
[string-wifi-pwd-three-words     ] ALT accepted=0 ... errors=1 | NEU accepted=1 name=WIFI_AP_PWD value=my secret password errors=0
[string-wifi-ap-name-two-words   ] ALT accepted=0 ... errors=1 | NEU accepted=1 name=WIFI_AP_NAME value=Free WiFi errors=0
[string-usb-manufacturer-two-words] ALT accepted=0 ... errors=1 | NEU accepted=1 name=USB_DeviceManufacturer value=Generic Corp errors=0
[string-multiple-interior-spaces ] ALT accepted=0 ... errors=1 | NEU accepted=1 value=extra   spaced   value errors=0
[string-single-word-value        ] ALT accepted=1 value=MyAP   | NEU accepted=1 value=MyAP   (unverändert)
[string-missing-value            ] ALT accepted=0 errors=1     | NEU accepted=0 errors=1     (unverändert)
[bool/int16/uint16-* ]                                           ALT und NEU byte-identisch in allen 12 Fällen

All 19 checks passed.
```

19 Prüfungen: drei Fund-Szenarien (mehrwortiger WiFi-Passwort-, WiFi-AP-Name- und
USB-Hersteller-Wert — ALT verwirft, NEU übernimmt ihn vollständig und korrekt), eine
Gegenprobe mit mehreren inneren Leerzeichen (Mehrfach-Whitespace zwischen den Wörtern bleibt im
übernommenen Wert erhalten, nur der trennende Whitespace nach dem Namen wird übersprungen), zwei
`SET_SETTING_STRING`-Regressionsfälle (einwortiger Wert weiterhin identisch zu ALT; fehlender
Wert weiterhin abgelehnt) sowie zwölf Regressionsfälle über alle drei anderen Varianten
(`BOOL`/`INT16`/`UINT16`, je mit gültigem Wert, ungültigem Wert bzw. zu vielen Token) — für jeden
davon sind Rückgabewert, übernommener Name/Wert und Fehlerzähler zwischen ALT und NEU paarweise
identisch. Zusätzliche Mutationsprobe (nicht Teil des gelieferten Repros): Entfernen des
Whitespace-Überspringens nach dem Namen (`valueStart = nameEnd` statt
`find_first_not_of(" \t", nameEnd)`) lässt die erste Prüfung sofort mit einem führenden
Leerzeichen im übernommenen Wert fehlschlagen — bestätigt, dass die Prüfung diesen Teil der
Logik tatsächlich abdeckt.

Lokal zusätzlich (sobald der Patch in die eigene Firmware übernommen ist, PlatformIO-Build
nötig, hier nicht nachbildbar): ein `.ds`-Testskript mit `SET_SETTING_STRING WIFI_AP_PWD mein
geheimes passwort` laufen lassen — vor dem Fix erscheint im Serial-Log "SET_SETTING needs two
arguments has: 3" und die Einstellung bleibt auf ihrem alten Wert, nach dem Fix wird der
komplette dreiwortige Wert übernommen (per `GET_SETTING`/Neustart und erneuter AP-Verbindung
prüfbar, je nachdem welche der beiden Einstellungen getestet wird). Danach zur Regressionsprobe
`SET_SETTING_BOOL` mit einem gültigen und einem ungültigen Wert sowie `SET_SETTING_UINT16` mit
und ohne `"0x"`-Präfix wiederholen — alle vier verhalten sich unverändert zum Zustand vor dem
Fix.

## Risiko

Niedrig. Rein additiv für `SET_SETTING_STRING` (neuer Zweig, der den Wert vor der bereits
vorhandenen `UINT16`-Präfix- und `BOOL`-Validierung neu zuweist — beide bleiben für den
`String`-Typ ohnehin wirkungslos, da deren `startsWith()`-Bedingungen nur für ihre jeweils
eigenen Befehlswörter zutreffen). Für `SET_SETTING_BOOL`/`_INT16`/`_UINT16` ist die Prüfung
`!isStringSetting && split.size() != 3` exakt die bisherige `split.size() != 3`-Bedingung, das
Verhalten dieser drei Varianten ist damit byte-identisch zu vorher — durch den Host-Repro über
alle zwölf Nicht-String-Fälle hinweg paarweise bestätigt.

## Sicherheitsrahmen

Reiner Funktionsfund in der eigenen DuckyScript-Interpreter-Schicht (kein Netzwerk-/Funkkanal,
keine Interaktion mit einem Zielsystem). Der Fix erweitert nur, welche bereits unterstützten
Werte durch einen bereits vorhandenen Befehl gesetzt werden können — kein neues Kommando, keine
neue Einstellung, keine Änderung an der Validierung der drei anderen Varianten.

## Quelle

- `G4MEOVER18/usb-army-penetrator` (main, Commit `411b153`): `src/Attacks/Ducky/Extensions.h`
  (`handleSetSetting()`, Zeile ~235), `src/Devices/WiFi/HardwareWiFi.cpp`,
  `src/Devices/USB/USBCore.cpp` (registrierte `String`-Einstellungen), `src/Utilities/Settings.cpp`
  (`setSettingValue()`, `String`-Zweig).
- `i-am-shodan/DuckScriptInterpreter` (Pin-Commit `23aa3ba8`, aus `platformio.ini` `lib_deps_core`):
  `src/DuckyParse.h` (`Ducky::SplitString()`).
- Eigener vorheriger Fund in diesem Repo (dieselbe Aufrufstelle, andere Schwachstellenklasse):
  `app-improvements/usb-army-penetrator/settings-int-uint16-stoi-uncaught-exception-crash-fix.md`.
