# usb-army-penetrator — ungeprüfter `std::stoi()` im Settings-Pfad: ein einziger HTTP-Request oder Ducky-`SETTING`-Befehl mit falschem Wert stürzt das Gerät ab

## Was

`src/Utilities/Settings.cpp`, Funktion `setSettingValue()` (zentrale Setting-Schreibfunktion, von
der Web-UI und von DuckyScript aus erreichbar). Für die Typen `Int16` und `UInt16` wird der vom
Aufrufer übergebene `value`-String ungeprüft an `std::stoi()` übergeben:

```cpp
case USBArmyKnifeCapability::SettingType::Int16:
    tempInt = std::stoi(value);
    if (tempInt <= static_cast<int>(INT16_MAX) && tempInt >= static_cast<int>(INT16_MIN))
    { ... }
    else { Debug::Log.info(LOG_SETTINGS, "Invalid setting value"); }
    break;
case USBArmyKnifeCapability::SettingType::UInt16:
    if (value.rfind("0x", 0) == 0)
    {
        tempInt = std::stoi(value, nullptr, 16);
        if (tempInt <= static_cast<int>(INT16_MAX) && tempInt >= 0)
        { ... }
        else { Debug::Log.info(LOG_SETTINGS, "Invalid setting value, invalid value"); }
    }
    else { Debug::Log.info(LOG_SETTINGS, "Invalid setting value, must start with 0x"); }
    break;
```

`std::stoi()` wirft laut Standard eine C++-Ausnahme, wenn der String keine gültige Zahl enthält
(`std::invalid_argument`) oder die Zahl außerhalb des `int`-Bereichs liegt
(`std::out_of_range`). Weder in dieser Funktion noch irgendwo sonst im Projekt existiert ein
`try`/`catch` (per `grep -rn "catch\s*(\|try\s*{" src/` im gesamten `src/`-Baum verifiziert — null
Treffer). Das `platformio.ini` setzt für keine der ESP32-/RP2040-Umgebungen `-fno-exceptions`, der
Arduino-ESP32-Kern hat C++-Exceptions standardmäßig aktiv — eine nicht gefangene Ausnahme läuft
also bis `std::terminate()` durch und reißt das gesamte Firmware-Image ab (Abort/Panic/Neustart),
nicht nur den aufrufenden Code-Pfad.

**Beide Werte kommen aus wirklich nicht vertrauenswürdigen Quellen, ohne jede Validierung davor:**

```cpp
// src/Comms/Web/WebServer.cpp:452-461 — Web-UI, GET/POST-Parameter direkt vom Client
else if (url.startsWith("/set") && preferences != nullptr && request->hasParam("name"))
{
    const String settingName = request->getParam("name")->value();
    if (request->hasParam("value"))
    {
        const String settingValue = request->getParam("value")->value();
        ...
        setSettingValue(*preferences, settingName.c_str(), settingValue.c_str());
```

```cpp
// src/Attacks/Ducky/Extensions.h:282 — DuckyScript-SETTING-Befehl
ret = setSettingValue(*preferences, settingName, settingValue);
```

Konkret betroffene, tatsächlich registrierte Settings (`grep -rn "SettingType::Int16\|SettingType::UInt16" src/`):

- `Int16`: `USB_AGENT_POLLING_TIME_IN_SEC` (`src/Attacks/Agent/Agent.cpp`).
- `UInt16`: `usbDeviceType`, `usbClassType`, `usbDeviceVID`, `usbDevicePID`, `usbVersion`,
  `usbDeviceVersion` (alle sechs in `src/Devices/USB/USBCore.cpp`).

Ein einzelner HTTP-Request wie `GET /set?name=usbDeviceVID&value=zzz` (kein Login/Token auf
diesem Endpunkt nötig — dieselbe ungeschützte `WebServer.cpp`-Oberfläche, auf der auch der bereits
behobene `webui-filename-path-traversal-fix` andere Endpunkte betraf) reicht aus, um das Gerät
mitten in einem laufenden
Pentest (HID-Injection, WiFi-Scan, Marauder-Lauf) abstürzen zu lassen. Derselbe Effekt tritt auch
rein versehentlich ein — ein Tippfehler im Wert eines eigenen `SETTING`-Befehls im eigenen
DuckyScript reicht bereits.

Die drei konkreten Eingaben, die den **`Int16`**-Zweig werfen lassen (durch das Host-Repro
verifiziert):
1. leerer String (`value=""`, z. B. ein Query-Parameter ohne Inhalt) → `std::invalid_argument`.
2. nicht-numerischer String (`"not_a_number"`) → `std::invalid_argument`.
3. eine syntaktisch gültige, aber zu lange Ziffernfolge (`"99999999999999999999999999"`) →
   `std::out_of_range`.

Für den **`UInt16`**-Zweig (nur Werte mit `"0x"`-Präfix erreichen überhaupt `std::stoi`):
4. ein zu langer Hex-Wert außerhalb des `int`-Bereichs (`"0x100000000"`, `"0x80000000"`) →
   `std::out_of_range`.

Beim Analysieren selbst widerlegt, bewusst **nicht** als Fund behauptet: `"0xZZZZ"` oder `"0x"`
allein werfen entgegen der ersten Vermutung **nicht** — `std::stoi(value, nullptr, 16)` folgt der
`strtol`-Präfixregel, wonach `"0x"` nur dann als Präfix erkannt wird, wenn direkt eine gültige
Hex-Ziffer folgt; sonst wird nur die führende `"0"` (für jede Basis ≥ 2 immer eine gültige Ziffer)
geparst und `0` zurückgegeben, ohne zu werfen. Das ist ein eigener, deutlich harmloserer
Logikfehler (ein ungültiger Hex-Rest nach `"0x"` wird stillschweigend als `0` akzeptiert statt
abgelehnt) — kein Absturzpfad, daher in diesem Beitrag absichtlich nicht mitbehoben, um diesen
Beitrag auf den Absturzfund zu beschränken.

## Nutzen

Verhindert, dass ein einzelner, auch unabsichtlich falsch formatierter Settings-Wert — über die
ungeschützte Web-API oder einen eigenen DuckyScript-`SETTING`-Befehl — das gesamte
USB-Army-Knife-Gerät per unbehandelter C++-Ausnahme zum Absturz/Neustart bringt. Das betrifft
direkt den eigenen Einsatzzweck des Geräts: ein laufender autorisierter Pentest (HID-Payload,
WiFi-Recon, Marauder) wird dadurch unterbrochen, ohne dass am Zielsystem irgendetwas falsch
gelaufen wäre — der Bug liegt vollständig in der eigenen Firmware. Der Fix macht die bereits
vorhandene Validierungs-Logik (die für ungültige Werte bereits "Invalid setting value" loggt)
tatsächlich vollständig, statt nur für den bereits durch die Bereichsprüfung abgedeckten Teilfall
zu greifen.

## Betroffene Dateien

- `src/Utilities/Settings.cpp` — `setSettingValue()`, nur die beiden `case`-Zweige
  `SettingType::Int16` und `SettingType::UInt16`. Keine Änderung an `registerSettingName()`,
  `getIntegerSettingValue()`, `enumerateSettingsAsJson()` oder `resetSettings()` nötig — keine
  dieser Funktionen ruft `std::stoi()` auf.

## Fix (Patch)

`try`/`catch` um jeden der beiden `std::stoi()`-Aufrufe, analog zu den bereits vorhandenen
"Invalid setting value"-Zweigen für den Bool-Typ (der schon vorsichtig genug ist, nie ungeprüft in
einen werfenden Parser zu laufen). Fängt sowohl `std::invalid_argument` als auch
`std::out_of_range` (beide erben von `std::exception`), loggt denselben Hinweis wie die
bestehenden Validierungszweige und bricht den `switch`-Zweig ab (`ret` bleibt `false`) — kein
Verhaltensunterschied für jeden bereits gültigen Wert.

```diff
--- a/src/Utilities/Settings.cpp
+++ b/src/Utilities/Settings.cpp
@@ -5,6 +5,7 @@
 #include <unordered_map>
 #include <vector>
 #include <algorithm>
+#include <stdexcept>
 #include <ArduinoJson.h>

 #define LOG_SETTINGS "Settings"
@@ -76,7 +77,15 @@ bool setSettingValue(Preferences &prefs, const std::string &name, const std::str
                     }
                     break;
                 case USBArmyKnifeCapability::SettingType::Int16:
-                    tempInt = std::stoi(value);
+                    try
+                    {
+                        tempInt = std::stoi(value);
+                    }
+                    catch (const std::exception &)
+                    {
+                        Debug::Log.info(LOG_SETTINGS, "Invalid setting value, not a valid integer");
+                        break;
+                    }
                     if (tempInt <= static_cast<int>(INT16_MAX) && tempInt >= static_cast<int>(INT16_MIN))
                     {
                         int16_t int16Value = 0;
@@ -92,7 +101,15 @@ bool setSettingValue(Preferences &prefs, const std::string &name, const std::str
                 case USBArmyKnifeCapability::SettingType::UInt16:
                     if (value.rfind("0x", 0) == 0)
                     {
-                        tempInt = std::stoi(value, nullptr, 16);
+                        try
+                        {
+                            tempInt = std::stoi(value, nullptr, 16);
+                        }
+                        catch (const std::exception &)
+                        {
+                            Debug::Log.info(LOG_SETTINGS, "Invalid setting value, not a valid hex integer");
+                            break;
+                        }
                         if (tempInt <= static_cast<int>(INT16_MAX) && tempInt >= 0)
                         {
                             uint16_t uint16Value = 0;
```

Vollständiger, per `git apply --check` gegen eine frische, unabhängige Kopie von
`G4MEOVER18/usb-army-penetrator` (`main`, Commit `411b153`) verifizierter Patch in
`settings-int-uint16-stoi-uncaught-exception-crash-fix/settings_cpp.patch`.

Das `break;` innerhalb des jeweiligen `catch`-Blocks bricht korrekt den umgebenden `switch`
(nicht etwa eine nicht-existierende Schleife) — Standard-C++, `try`/`catch` ist kein eigener
Schleifen-/Switch-Scope.

## Testhinweis

Host-kompilierbares Repro in
`repro-settings-int-uint16-stoi-uncaught-exception/repro.cpp` — reine `std::stoi()`/Ausnahme-Logik
1:1 aus `setSettingValue()` übernommen (ALT = Originalzustand ohne `try`/`catch`, NEU = Zustand
nach dem Patch), kein Arduino/ESP32-Header, kein `Preferences`-Objekt nötig:

```
g++ -std=c++17 -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.cpp && ./repro
g++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.cpp && ./repro
```

Lokal ausgeführt (beide Optimierungsstufen identisch):

```
=== ALT (vor Fix): Settings.cpp setSettingValue() Int16/UInt16 ===
[ALT] Int16 value="" -> WIRFT (Absturz)
[ALT] Int16 value="not_a_number" -> WIRFT (Absturz)
[ALT] Int16 value="99999999999999999999999999" -> WIRFT (Absturz)
[ALT] UInt16 value="0x100000000" -> WIRFT (Absturz)
[ALT] UInt16 value="0x8000" -> ok (accepted=0, erwartet 0)

=== NEU (nach Fix) ===
[NEU] Int16 value="" -> kein Wurf, accepted=0
[NEU] Int16 value="not_a_number" -> kein Wurf, accepted=0
[NEU] Int16 value="99999999999999999999999999" -> kein Wurf, accepted=0
[NEU] UInt16 value="0x100000000" -> kein Wurf, accepted=0
[NEU] UInt16 value="0x80000000" -> kein Wurf, accepted=0

30 Pruefungen, 0 fehlgeschlagen.
ALLE CHECKS BESTANDEN
```

30 Prüfungen: (1)–(3) ALT wirft für die drei Int16-Eingaben (leer, nicht-numerisch, Überlauf) —
Fund bestätigt; (4) ALT wirft für einen zu großen Hex-Wert im UInt16-Zweig — Fund bestätigt; (5)
Gegenprobe zur eigenen ersten (verworfenen) Vermutung: `"0x8000"` wirft nicht, wird aber korrekt
als außerhalb des gültigen Bereichs abgelehnt (`accepted=0`) — bestehende Bereichsprüfung
unverändert korrekt; zwei ALT-Regressionsfälle (gültiger `Int16`- und `UInt16`-Wert werden
weiterhin akzeptiert); NEU: alle fünf fund-auslösenden Eingaben kehren jetzt regulär zurück
(`accepted=false`, kein Wurf nach außen); vier NEU-Regressionsfälle (gültiger positiver/negativer
Int16-Wert weiterhin akzeptiert, ein syntaktisch gültiger aber außerhalb des `int16_t`-Bereichs
liegender Wert `"40000"` weiterhin korrekt abgelehnt statt als Fund behandelt, gültiger Hex-Wert
weiterhin akzeptiert, fehlendes `"0x"`-Präfix weiterhin sofort abgelehnt ohne `stoi` aufzurufen).
Unter `-O0` und `-O1`: ALLE CHECKS BESTANDEN, identischer Output, keine ASan-/UBSan-Meldungen,
keine Compiler-Warnungen.

Lokal zusätzlich (sobald der Patch in die eigene Firmware übernommen ist, PlatformIO-Build nötig,
hier nicht nachbildbar): per Browser oder `curl` gegen das laufende Gerät
`GET /set?name=usbDeviceVID&value=zzz` senden — vor dem Fix startet das Gerät neu
(Serial-Monitor zeigt den C++-Abort/Backtrace, `monitor_filters = esp32_exception_decoder` ist in
`platformio.ini` bereits aktiv), nach dem Fix bleibt es stabil und die Log-Zeile "Invalid setting
value, not a valid hex integer" erscheint. Zusätzlich denselben Request mit einem gültigen Wert
(z. B. `value=0x1234`) wiederholen — Setting wird weiterhin wie vorher übernommen.

## Risiko

Niedrig. Rein additiv (`try`/`catch` um zwei bestehende Aufrufstellen, ein neuer Include), keine
Änderung an der Bereichsprüfung, am Rückgabewert-Vertrag der Funktion oder an einer öffentlichen
Signatur. Für jeden bisher gültigen Wert (Zahl im Bereich, korrektes `"0x"`-Hex) ist das Verhalten
byte-identisch zu vorher — nur der bisher ungefangene Absturzpfad wird durch denselben
"Invalid setting value"-Logeintrag ersetzt, der für die bereits durch die Bereichsprüfung
abgedeckten Fälle ohnehin schon existiert. Bewusst nicht mitbehoben: der oben beschriebene,
beim Analysieren selbst gefundene und wieder verworfene Misparse-Nebenfund (`"0xZZZZ"`/`"0x"`
werden stillschweigend als `0` statt als Fehler behandelt) — harmlos (kein Absturz, nur ein
falscher Zahlenwert für einen ohnehin über die Web-UI frei wählbaren Setting-Wert), bleibt für
eine künftige Sitzung vorgemerkt, falls eine strengere Hex-Validierung gewünscht wird.

## Quelle

- `G4MEOVER18/usb-army-penetrator` (main, Commit `411b153`): `src/Utilities/Settings.cpp`
  (`setSettingValue()`), `src/Utilities/Settings.h`, `src/USBArmyKnifeCapability.h`
  (`SettingType`-Enum), `src/Comms/Web/WebServer.cpp` (`/set*`-Endpunkt, Zeile ~452),
  `src/Attacks/Ducky/Extensions.h` (Zeile 282, `SETTING`-Befehl), `src/Attacks/Agent/Agent.cpp`
  und `src/Devices/USB/USBCore.cpp` (tatsächlich registrierte `Int16`/`UInt16`-Settings),
  `platformio.ini` (alle Build-Umgebungen gegen `-fno-exceptions` geprüft — nirgends gesetzt).
- Eigener vorheriger Fund in diesem Repo (derselbe Endpunkt, andere Schwachstellenklasse):
  `app-improvements/usb-army-penetrator/webui-filename-path-traversal-fix.md`.
