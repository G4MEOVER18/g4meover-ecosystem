# usb-army-penetrator — RUN_PAYLOAD: Dateiendungsvergleich ist Groß-/Kleinschreibungs-abhängig, Treffer auf Großbuchstaben-Endung wird stillschweigend verworfen

## Was

`src/Attacks/Ducky/Extensions.h`, Funktion `handleRunPayload()` (Handler für den
DuckyScript-Befehl `RUN_PAYLOAD`, zusammen mit `AGENT_RUN`/`ESP32M` der dritte der seit
mehreren Sitzungen als Rückstellpunkt offenen Ducky-Dispatch-Handler). Die Weiche über den
Dateityp vergleicht die Endung case-sensitiv:

```cpp
static int handleRunPayload(const std::string &str, ...)
{
    const std::string arg = str.substr(str.find(' ') + 1);
    if (arg.ends_with(".ds"))
    {
        Attacks::Ducky.setPayload(arg);
        duckyFileParser.Restart();
    }
    else if (arg.ends_with(".png"))
    {
        currentlyExecutingFile.clear();
        Attacks::Ducky.setPayloadCmdLine("DISPLAY_PNG " + arg);
    }

    return true;
}
```

`arg` ist der Dateiname genau so, wie ihn `RUN_PAYLOAD <Dateiname>` übergibt — in der Praxis
meist derselbe String, der zuvor beim Hochladen über die Web-UI verwendet wurde. Im
Upload-Handler (`handleUpload()` in `src/Comms/Web/WebServer.cpp`, Zeile ~530) wird der
Dateiname unverändert vom Browser übernommen:

```cpp
request->_tempFile = Devices::Storage.openFile(std::string("/")+filename.c_str(), "w");
```

An keiner Stelle im Upload-Pfad wird die Endung normalisiert (kein `tolower()`/`toupper()`).
Der im Browser-Dateidialog angezeigte Name entspricht direkt dem tatsächlichen Dateinamen auf
dem Host-Rechner — eine `.DS`- oder `.PNG`-Endung (z. B. von einem Editor, der Großendungen
verwendet, von einem anders benannten Payload-Export, oder einfach einer manuell umbenannten
Datei) landet unverändert so auf dem Gerät. Dieselbe case-sensitive `.ds`-Prüfung verwendet
auch `/payloadinfo` (`WebServer.cpp:278`, listet nur exakt kleingeschriebene `.ds`-Dateien in
der Web-UI) — eine großgeschriebene `.ds`-Datei taucht dort also nicht einmal in der
Payload-Liste auf, ist aber über `RUN_PAYLOAD <Dateiname>` (Web-UI `/rawinput`, verschachteltes
`.ds`-Skript, oder `AGENT_RUN`/Agent-gesteuerter Ablauf) weiterhin direkt adressierbar.

Trifft `RUN_PAYLOAD` auf eine Datei mit Großbuchstaben- oder gemischter Endung (`PAYLOAD.DS`,
`Image.PNG`, …), schlagen beide `ends_with()`-Zweige fehl — die Funktion tut **nichts**, loggt
**nichts** und gibt trotzdem `true` zurück (Ducky-Interpreter wertet den Befehl also als
erfolgreich ausgeführt). Für den Bediener ist das von einem erfolgreich gestarteten Payload
nicht zu unterscheiden, außer dass auf dem Zielsystem nie etwas passiert.

Zusätzlich beim Review bestätigt und mit demselben Patch behoben (kein eigener Fund, reine
Aufräumung): `duckyFileParser.Restart();` direkt nach `Attacks::Ducky.setPayload(arg)` ist
totes Doppel — `DuckyPayload::setPayload()` ruft `duckyFileParser.Restart()` bereits selbst auf
(`src/Attacks/Ducky/DuckyPayload.cpp`, Zeile ~295). Gegen die vendored
`DuckScriptInterpreter`-Bibliothek (`i-am-shodan/DuckScriptInterpreter`, exakt der in
`platformio.ini` gepinnte Commit `23aa3ba87e95f402ccab53beb9ead54602643dd7`) geprüft:
`DuckyInterpreter::Restart()` (`src/DuckyParse.h`, Zeile 185) setzt nur `_lineNumber = 0` zurück
und leert drei Stacks (`_whileLoopLineNumbers`, `_callstack`, `_linesToIgnore`) — ein zweiter,
unmittelbar folgender Aufruf ist dadurch beobachtbar wirkungslos (keine Zustandsänderung
zwischen den beiden Aufrufen möglich), aber verwirrend beim Lesen und wird mit demselben Patch
entfernt.

## Nutzen

- Ein `RUN_PAYLOAD`-Aufruf auf eine tatsächlich vorhandene, aber groß- oder gemischtgeschrieben
  benannte `.ds`- oder `.png`-Datei startet den Payload jetzt tatsächlich, statt stillschweigend
  gar nichts zu tun. Betrifft direkt den eigenen Einsatzzweck: ein über die Web-UI hochgeladenes
  oder von einem Agent-gesteuerten Ablauf benanntes Payload mit Großendung lief bisher nie.
- Jeder bisher nicht abgedeckte Dateityp (falsche Endung, Tippfehler, absichtlich ungültiger
  Dateiname) erzeugt jetzt eine Log-Zeile (`"RUN_PAYLOAD unsupported file type: ..."`) statt
  eines unsichtbaren No-ops — sichtbar über die ohnehin vorhandene Log-Anzeige der Web-UI, ohne
  dass der Rückgabewert des Ducky-Befehls selbst verändert wird.
- Entfernt den funktional wirkungslosen doppelten `Restart()`-Aufruf, ohne Verhaltensänderung.

## Betroffene Dateien

- `src/Attacks/Ducky/Extensions.h` — nur `handleRunPayload()`, plus ein neuer
  `#include <algorithm>` (für `std::transform`, nach demselben bereits im Projekt etablierten
  Muster wie `src/Utilities/Settings.cpp`, dort ebenfalls `std::transform(..., ::toupper)` für
  eine Groß-/Kleinschreibungs-unabhängige Prüfung). Keine Änderung an `setPayload()`,
  `setPayloadCmdLine()`, am Upload-Pfad in `WebServer.cpp` oder an `AGENT_RUN`/`ESP32M`
  (beide separat als Teil dieser Sitzung überprüft, siehe Zusätzlich unten).

## Zusätzlich geprüft, bewusst **kein** weiterer Fund

- `handleAgentRun()`/`handleESP32Marauder()` (dieselbe Rückstellpunkt-Liste): beide folgen dem
  bereits in einer früheren Sitzung einzeln als sicher bestätigten
  `str.substr(str.find(' ') + 1)`-Muster — ohne Leerzeichen liefert `find()` `npos`, `+ 1`
  überläuft wohldefiniert auf `0`, `substr(0)` liefert den ganzen String zurück (kein Absturz,
  `substr(0)` ist immer gültig). Beide verwenden kein `std::stoi()`/`std::stol()` und keine
  Dateiendungsprüfung — die hier gefundene Fehlerklasse betrifft sie nicht.
- Die über die Web-UI erreichbare `AGENT_RUN`-Abkürzung (`/runagentcmd` in `WebServer.cpp`,
  Zeile 249, baut `"AGENT_RUN " + cmdline` als Ducky-Rohzeile) öffnet keine zusätzliche
  Angriffsfläche gegenüber dem bereits vollständig offenen `/rawinput`-Endpunkt (Zeile 241):
  wer `/rawinput` erreicht, kann ohnehin jede beliebige, auch mehrzeilige Ducky-Rohzeile
  einschleusen (`DuckyPayload::setPayloadCmdLine()` zerlegt den String ausdrücklich an `\n` in
  einzelne Kommandos, `src/Attacks/Ducky/DuckyPayload.cpp`, Zeile ~301) — `/runagentcmd` schränkt
  nichts ein, erweitert aber auch nichts.
- `handleFileIndex()`/`.ends_with(".ds")`-Prüfung in `WebServer.cpp:278` (`/payloadinfo`) hat
  dieselbe Case-Schwäche wie der hier gefundene Fund, ebenso der exakte String-Vergleich
  `filename == "autorun.ds"` im Upload-Handler (`WebServer.cpp:525`, verhindert bei
  Großschreibung nicht das versehentliche Überschreiben eines laufenden Autorun-Payloads). Beide
  bewusst **nicht** in diesen Patch aufgenommen — reiner Web-UI-Code außerhalb des hier
  bearbeiteten Ducky-Dispatch-Rückstellpunkts, für eine eigene Sitzung vorgemerkt.

## Fix (Patch)

```diff
--- a/src/Attacks/Ducky/Extensions.h
+++ b/src/Attacks/Ducky/Extensions.h
@@ -2,6 +2,8 @@
 #include "Adafruit_TinyUSB.h"
 #endif
 
+#include <algorithm>
+
 #ifdef ARDUINO_ARCH_ESP32
 #include "esp_ota_ops.h" // for OTA
 #endif
@@ -473,16 +475,23 @@
 static int handleRunPayload(const std::string &str, const std::unordered_map<std::string, std::string> &constants, const std::unordered_map<std::string, int> &variables)
 {
     const std::string arg = str.substr(str.find(' ') + 1);
-    if (arg.ends_with(".ds"))
+
+    std::string argUpper = arg;
+    std::transform(argUpper.begin(), argUpper.end(), argUpper.begin(), ::toupper);
+
+    if (argUpper.ends_with(".DS"))
     {
         Attacks::Ducky.setPayload(arg);
-        duckyFileParser.Restart();
     }
-    else if (arg.ends_with(".png"))
+    else if (argUpper.ends_with(".PNG"))
     {
         currentlyExecutingFile.clear();
         Attacks::Ducky.setPayloadCmdLine("DISPLAY_PNG " + arg);
     }
+    else
+    {
+        Debug::Log.error(LOG_DUCKY, "RUN_PAYLOAD unsupported file type: " + arg);
+    }
 
     return true;
 }
```

Der Vergleich läuft über eine Großbuchstaben-Kopie (`argUpper`), der an `Attacks::Ducky.setPayload()`
bzw. `setPayloadCmdLine()` übergebene Dateiname (`arg`) bleibt unverändert in Originalschreibung
— die tatsächliche Datei auf dem Speicher wird weiterhin mit dem exakten, echten Namen
angesprochen, nur die Endungsprüfung selbst ist jetzt unabhängig von deren Schreibweise.

Vollständiger, per `git apply --check` gegen zwei frische, unabhängige Zweitkopien von
`G4MEOVER18/usb-army-penetrator` (`main`, Commit `411b153`) verifizierter Patch — real
angewendet, beide Ergebnisse per `md5sum` gegen die eigene Arbeitskopie abgeglichen: identisch.
Patch in `run-payload-case-insensitive-extension-fix/extensions_h.patch`.

## Testhinweis

Host-kompilierbares Repro in
`repro-run-payload-case-insensitive-extension/repro.cpp` — die Dispatch-Logik aus
`handleRunPayload()` 1:1 übernommen (ALT = Originalzustand, NEU = Zustand nach dem Patch),
Seiteneffekte (`setPayload()`/`setPayloadCmdLine()`/Log) durch einen einfachen Rückgabewert
(`Branch`) ersetzt, kein Arduino/ESP32/SD-Zugriff nötig:

```
g++ -std=c++20 -O0 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.cpp && ./repro
g++ -std=c++20 -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.cpp && ./repro
```

Lokal ausgeführt (beide Optimierungsstufen identisch):

```
=== ALT (vor Fix): case-sensitives ends_with(".ds")/(".png") ===
[OK] ALT RUN_PAYLOAD payload.ds -> SetPayload
[OK] ALT RUN_PAYLOAD payload.DS -> None (silent no-op)
[OK] ALT RUN_PAYLOAD Payload.Ds -> None (silent no-op)
[OK] ALT RUN_PAYLOAD image.png -> DisplayPng
[OK] ALT RUN_PAYLOAD image.PNG -> None (silent no-op)
[OK] ALT RUN_PAYLOAD IMAGE.Png -> None (silent no-op)
[OK] ALT RUN_PAYLOAD notes.txt -> None (silent no-op)

=== NEU (nach Fix): Grossschreibungs-unabhaengiger Vergleich ===
[OK] NEU RUN_PAYLOAD payload.ds -> SetPayload
[OK] NEU RUN_PAYLOAD payload.DS -> SetPayload
[OK] NEU RUN_PAYLOAD Payload.Ds -> SetPayload
[OK] NEU RUN_PAYLOAD image.png -> DisplayPng
[OK] NEU RUN_PAYLOAD image.PNG -> DisplayPng
[OK] NEU RUN_PAYLOAD IMAGE.Png -> DisplayPng
[OK] NEU RUN_PAYLOAD notes.txt -> Unsupported (logged)
[OK] NEU preserves original filename case for setPayload() arg: 'Payload.Ds'
[OK] NEU RUN_PAYLOAD (kein Argument) -> Unsupported (logged)

16 Pruefungen, 0 fehlgeschlagen.
ALLE CHECKS BESTANDEN
```

16 Prüfungen: drei ALT-Fundbestätigungen (`payload.DS`/`Payload.Ds` für `.ds`, `image.PNG`/
`IMAGE.Png` für `.png` — fünf Großbuchstaben-Varianten insgesamt landen ALT bei `None`, obwohl
die jeweilige Datei real existiert); eine ALT-Regression (`notes.txt`, bleibt unverändert
`None`, keine neue Zweigwahl für einen bereits vorher ignorierten Dateityp); sieben NEU-Checks
(alle sechs ALT-Fundfälle laufen jetzt korrekt in den richtigen Zweig, `notes.txt` landet
korrekt im neuen `Unsupported`-Zweig statt weiterhin unsichtbar durchzufallen); eine
Byte-Identitäts-Prüfung (der an `setPayload()` übergebene Dateiname bleibt `"Payload.Ds"`,
**nicht** `"PAYLOAD.DS"` — nur der Vergleich läuft großgeschrieben, nicht die tatsächlich
verwendete Datei); ein bereits aus einer früheren Sitzung bekannter Grenzfall erneut bestätigt
(`RUN_PAYLOAD` ganz ohne Argument — `substr(0)` liefert den ganzen String zurück, kein Absturz,
fällt jetzt korrekt in den `Unsupported`-Zweig statt wie vorher unsichtbar durchzufallen). Unter
`-O0` und `-O1`: **ALLE CHECKS BESTANDEN**, identischer Output, keine ASan-/UBSan-Meldungen,
keine Compiler-Warnungen.

Lokal zusätzlich (sobald der Patch in die eigene Firmware-Kopie übernommen ist,
PlatformIO-Build nötig, hier nicht nachbildbar): über die Web-UI eine Datei mit Großendung
hochladen (z. B. `TEST.DS` mit einer einzelnen `STRING hello`-Zeile), per `RUN_PAYLOAD TEST.DS`
(`/rawinput`) starten — vor dem Fix passiert sichtbar nichts, nach dem Fix läuft das Skript und
tippt `hello`. Zusätzlich ein bereits vorher funktionierendes `.ds`/`.png`-Payload in
Kleinschreibung wiederholen — unverändertes Verhalten.

## Risiko

Niedrig. Die Vergleichslogik wird nur erweitert (eine Großbuchstaben-Kopie vor dem bestehenden
`ends_with()`), nicht ersetzt — jede bisher über `.ds`/`.png` (klein) erfolgreich erkannte Datei
bleibt identisch erkannt, der tatsächlich verwendete Dateiname ändert sich nicht. Der entfernte
`duckyFileParser.Restart()`-Aufruf ist nach Prüfung der vendored Bibliotheksimplementierung
nachweislich wirkungslos. Der neue `else`-Zweig fügt nur eine Log-Zeile hinzu, ändert nicht den
Rückgabewert (`true`, unverändert zu vorher) und damit nicht den Kontrollfluss des
Ducky-Interpreters für einen nicht erkannten Dateityp.

## Sicherheitsrahmen

Reiner Korrektheits-/Stabilitätsfund in der eigenen DuckyScript-Interpreter-Schicht (keine neue
Funktionalität, keine Netzwerk-/Funkkanal-Interaktion über das durch `RUN_PAYLOAD` ohnehin
beabsichtigte Verhalten hinaus). Der Fix macht ein bereits vorhandenes, beabsichtigtes Kommando
zuverlässiger, erweitert keine Rechte und öffnet keine neue Angriffsfläche — die unter
"Zusätzlich geprüft" dokumentierte Web-UI-Analyse bestätigt das ausdrücklich für die
`AGENT_RUN`/`/runagentcmd`-Abkürzung.

## Quelle

- `G4MEOVER18/usb-army-penetrator` (main, Commit `411b153`): `src/Attacks/Ducky/Extensions.h`
  (`handleRunPayload()`, Zeile ~473), `src/Comms/Web/WebServer.cpp` (`handleUpload()` Zeile
  ~530, `/payloadinfo` Zeile 278, `/rawinput`/`/runagentcmd` Zeile 241/249),
  `src/Attacks/Ducky/DuckyPayload.cpp` (`setPayload()` Zeile 286, `setPayloadCmdLine()` Zeile
  301).
- `i-am-shodan/DuckScriptInterpreter` (Commit `23aa3ba87e95f402ccab53beb9ead54602643dd7`, exakt
  der in `platformio.ini` gepinnte Stand): `src/DuckyParse.h`, `DuckyInterpreter::Restart()`,
  Zeile 185.
- Eigener vorheriger Fund in diesem Repo (dieselbe Datei, anderer Ducky-Handler):
  `app-improvements/usb-army-penetrator/raw-hid-stoi-uncaught-exception-crash-fix.md`.
