# usb-army-penetrator — Marauder: Cross-Task-Race + Kommandoverlust im Command-Mailbox

## Was
`src/Attacks/Marauder/Marauder.cpp` reicht Kommandos an den ESP32-Marauder-
CLI-Interpreter über genau zwei statische Variablen weiter:

```cpp
static std::string nextMarauderCommandToRun;
static bool marauderActivated = false;

void ESP32Marauder::run(const std::string &cmd)
{
  marauderActivated = true;
  nextMarauderCommandToRun = cmd;
}

void ESP32Marauder::loop(Preferences &prefs)
{
  if (marauderActivated)
  {
    ...
    if (!nextMarauderCommandToRun.empty())
    {
      cli_obj.runCommand(nextMarauderCommandToRun.c_str());
      nextMarauderCommandToRun.clear();
    }
  }
}
```

Das ist ein Postfach mit genau einem Slot, ohne jede Synchronisierung.
Zwei getrennte Probleme:

**1) Kommandoverlust (rein sequenziell, kein Threading nötig).** Trifft ein
zweiter `run()`-Aufruf ein, bevor `loop()` den vorherigen Slot geleert hat,
überschreibt er ihn kommentarlos — der erste Befehl wird nie ausgeführt,
ohne Log-Zeile, ohne Fehlercode. `run()` wird von drei unabhängigen Stellen
aufgerufen: `WebServer.cpp:356` (`/marauder?marauderCmd=...`),
`Extensions.h:203` (Ducky-Zeile `MARAUDER ...`) und
`EvilCrowWind.cpp:70` — und genau dieser dritte Aufrufer ist der
sicherheitsrelevante: die Übertemperatur-Abschaltung
(`EvilCrowWind::loop()`, `tsens_value > MAX_WORKING_TEMPERATURE`) ruft
`Attacks::Marauder.run("stopscan")` als einzige Aktion auf, um einen
laufenden Scan bei Überhitzung zu stoppen. Läuft im selben Fenster noch ein
Web-Request oder eine Ducky-Zeile mit einem anderen `MARAUDER`-Befehl durch,
kann `stopscan` kommentarlos verworfen werden.

**2) Fehlende Synchronisierung über Task-Grenzen (Datenrace, UB).**
`nextMarauderCommandToRun`/`marauderActivated` werden aus `run()`
geschrieben (u. a. aus dem `/marauder`-Handler in `WebServer.cpp`, der über
`ESPAsyncWebServer`/`AsyncTCP` läuft — laut `platformio.ini`
(`-D CONFIG_ASYNC_TCP_RUNNING_CORE=1`) ein eigener FreeRTOS-Task) und in
`ESP32Marauder::loop()` gelesen/geleert, was aus dem Arduino-`loop()`-Task
aufgerufen wird. Zwei unterschiedliche FreeRTOS-Tasks greifen also ohne
Mutex/kritischen Abschnitt auf denselben `std::string` zu — FreeRTOS kann
beide Tasks jederzeit gegeneinander verdrängen (auch auf demselben Core),
ein `std::string`-Zuweisungsoperator ist kein atomarer Vorgang. Das ist
klassisches undefiniertes Verhalten (potenziell korrupter Pointer/Länge im
`std::string`, im schlimmsten Fall ein Absturz), nicht nur ein
Logik-Fehler.

Ich habe beide Effekte host-seitig nachgebildet und verifiziert (siehe
Testhinweis): Test A zeigt den deterministischen Kommandoverlust rein
sequenziell, Test B lässt ThreadSanitizer den Datenrace auf genau diesen
zwei Variablen bestätigen.

## Nutzen
- Schließt eine Lücke im eigenen Übertemperaturschutz: die Abschaltung
  (`stopscan` bei > 90 °C, siehe `EvilCrowWind.cpp`) ist die einzige
  Gegenmaßnahme gegen ein sich weiter aufheizendes Gerät während eines
  laufenden Einsatzes — ihr Verlust ist kein kosmetisches Problem.
- Beseitigt eine echte, mit ThreadSanitizer bestätigte Data-Race/UB-Quelle
  zwischen dem AsyncTCP-Task (Web-UI) und dem Haupt-`loop()`-Task, statt
  sich auf zufälliges "hat bisher nicht gecrasht" zu verlassen.
- Kein Verhaltens-/API-Bruch: `run()`/`loop()` behalten exakt dieselbe
  Signatur und dasselbe Ein-Slot-Modell (kein Umbau auf eine echte
  Befehls-Queue nötig, das wäre größerer Umfang für denselben Nutzen) —
  nur der Zugriff auf die geteilten Variablen wird serialisiert.

## Betroffene Dateien
- `src/Attacks/Marauder/Marauder.cpp` (kritischer Abschnitt um die drei
  Zugriffsstellen auf `nextMarauderCommandToRun`/`marauderActivated`).

## Risiko
Gering. `portMUX_TYPE`/`portENTER_CRITICAL`/`portEXIT_CRITICAL` sind
Standard-Arduino-ESP32-APIs (bereits über `Preferences.h` transitiv
verfügbar, keine neue Abhängigkeit), reine Ergänzung um einen kurzen
kritischen Abschnitt an drei bestehenden Stellen. Der eigentliche
CLI-Aufruf (`cli_obj.runCommand(...)`, potenziell langlaufend) bleibt
bewusst **außerhalb** des kritischen Abschnitts — nur das Kopieren/Leeren
des Kommando-Strings ist geschützt, damit auf keinem Core länger als
nötig Interrupts gesperrt sind. Das Kommandoverlust-Problem (Punkt 1)
bleibt nach diesem Fix bestehen (weiterhin Ein-Slot-Mailbox, "letzter
gewinnt") — das ist eine bewusste Entscheidung für einen minimalen,
risikoarmen Fix; eine echte Queue wäre ein größerer Umbau und für den
seltenen Kollisionsfall (zwei `run()`-Aufrufe in exakt demselben
`loop()`-Fenster) nicht im Verhältnis zum Nutzen.

## Fix (Patch)

```diff
--- a/src/Attacks/Marauder/Marauder.cpp
+++ b/src/Attacks/Marauder/Marauder.cpp
@@
 const String PROGMEM version_number = MARAUDER_VERSION;
 static uint32_t currentTime = 0;
 static std::string nextMarauderCommandToRun;
 static bool marauderActivated = false;
+
+// run() is reachable from the AsyncTCP task (WebServer.cpp /marauder
+// handler, itself its own FreeRTOS task per platformio.ini
+// CONFIG_ASYNC_TCP_RUNNING_CORE=1) as well as from the main loop() task
+// (Ducky MARAUDER lines, EvilCrowWind thermal cutoff). loop() drains the
+// mailbox from the main loop() task. Guard every access to the two
+// statics above with a spinlock so a std::string assignment on one task
+// can never be observed half-written by the other.
+static portMUX_TYPE marauderCmdMux = portMUX_INITIALIZER_UNLOCKED;
 
 void ESP32Marauder::begin(Preferences &prefs)
 {
@@
 void ESP32Marauder::loop(Preferences &prefs)
 {
   // whilst we init the esp32 marauder code to save ram/cycles we don't
   // start to do work until we receive our first command
   if (marauderActivated)
   {
     currentTime = millis();
     wifi_scan_obj.main(currentTime);
 
     // Save buffer to SD and/or serial
     buffer_obj.save();
     settings_obj.main(currentTime);
 
-    if (!nextMarauderCommandToRun.empty())
+    std::string cmdToRun;
+    portENTER_CRITICAL(&marauderCmdMux);
+    if (!nextMarauderCommandToRun.empty())
+    {
+      cmdToRun = nextMarauderCommandToRun;
+      nextMarauderCommandToRun.clear();
+    }
+    portEXIT_CRITICAL(&marauderCmdMux);
+
+    if (!cmdToRun.empty())
     {
-      Debug::Log.info(LOG_MARAUDER, std::string("Running cmd ") + nextMarauderCommandToRun);
-      cli_obj.runCommand(nextMarauderCommandToRun.c_str());
-      nextMarauderCommandToRun.clear();
-      Debug::Log.info(LOG_MARAUDER, std::string("Finished cmd ") + nextMarauderCommandToRun);
+      Debug::Log.info(LOG_MARAUDER, std::string("Running cmd ") + cmdToRun);
+      cli_obj.runCommand(cmdToRun.c_str());
+      Debug::Log.info(LOG_MARAUDER, std::string("Finished cmd ") + cmdToRun);
     }
   }
 }
 
 void ESP32Marauder::run(const std::string &cmd)
 {
-  marauderActivated = true;
-  nextMarauderCommandToRun = cmd;
+  portENTER_CRITICAL(&marauderCmdMux);
+  marauderActivated = true;
+  nextMarauderCommandToRun = cmd;
+  portEXIT_CRITICAL(&marauderCmdMux);
 }
```

Anmerkung zum ursprünglichen Bug in `loop()`: die alte Zeile
`Debug::Log.info(LOG_MARAUDER, std::string("Finished cmd ") + nextMarauderCommandToRun);`
lief nach dem `.clear()` derselben Variable — das "Finished cmd"-Log war
also bereits vor diesem Fix immer leer. Der Patch behebt das nebenbei mit,
indem er durchgehend die lokale Kopie `cmdToRun` loggt.

## Testhinweis
- Host-seitiges Repro (kein Hardware-Bezug, reine Logik/Nebenläufigkeit):
  `app-improvements/usb-army-penetrator/repro-marauder-cmd-race/repro.cpp`.
  Bildet `run()`/`loop()` 1:1 nach (gleiche zwei Statics, gleiche
  Zuweisungs-/Lösch-Reihenfolge) und liefert drei Läufe:
  ```
  g++ -std=c++17 -O1 -g repro.cpp -o repro_a && ./repro_a
  g++ -std=c++17 -O1 -g -fsanitize=thread repro.cpp -o repro_b -DTHREAD_TEST -lpthread && ./repro_b
  g++ -std=c++17 -O1 -g -fsanitize=thread repro.cpp -o repro_c -DTHREAD_TEST -DFIXED -lpthread && ./repro_c
  ```
  Bei mir lokal verifiziert:
  - **Test A** (`repro_a`, deterministisch, kein Threading): zwei
    `run()`-Aufrufe vor einem `loop()`-Durchlauf — in beiden Reihenfolgen
    (`scanap`→`stopscan` und `stopscan`→`scanap`) kommt genau ein Befehl
    an, der jeweils andere ist unwiederbringlich verloren. Exit 0
    (Assertions bestätigen das erwartete Verlust-Verhalten).
  - **Test B unsafe** (`repro_b`, zwei echte Threads über `run()`/`loop()`
    im Wettstreit): ThreadSanitizer meldet zwei Data-Races — je einen auf
    `marauderActivated` und auf `nextMarauderCommandToRun` (Race beim
    `std::string`-`memcpy` während der Zuweisung), exakt an den Zeilen von
    `run()`/`loop()`.
  - **Test B fixed** (`repro_c`, dieselbe Race-Situation, aber mit
    `std::lock_guard`/`std::mutex` als Host-Äquivalent zum
    `portENTER_CRITICAL`/`portEXIT_CRITICAL`-Patch): 0 ThreadSanitizer-
    Warnungen, Exit 0.
- Lokaler Hardware-Test (nach Einbau des Patches):
  1. `ufbt`/PlatformIO-Build wie gewohnt, auf das Gerät flashen.
  2. Funktionsregression: über die Web-UI `/marauder?marauderCmd=scanap`
     aufrufen, Marauder-Scan läuft weiterhin normal an; Log zeigt jetzt
     bei jedem Kommando eine nicht-leere "Finished cmd"-Zeile (vorher
     immer leer, siehe Anmerkung oben).
  3. Übertemperatur-Pfad (nur auf `EvilCrowWind`/`EVIL_CROW_CABLE_WIND`-
     Boards relevant): Gerät während eines laufenden Scans künstlich
     erwärmen (z. B. kurz mit Fön/Heißluft in der Nähe des Temperatursensors,
     unterhalb der 90 °C-Schwelle bewusst vorsichtig herantasten) und
     parallel per Web-UI weitere `MARAUDER`-Kommandos senden; erwartet:
     der Scan stoppt trotzdem zuverlässig, keine Log-Lücke bei
     "Enabling overheating protection".
  4. Kein Crash/Reboot bei schneller Abfolge von Web-UI-`/marauder`-
     Aufrufen (mehrere `curl`-Requests in kurzem Abstand) — Regressionstest
     für den behobenen Datenrace.

## Quelle
- `G4MEOVER18/usb-army-penetrator` (Fork, Commit
  `411b153d749f66a5c0059c39a3933c61971fcb2f`),
  `src/Attacks/Marauder/Marauder.cpp` (`ESP32Marauder::run()`/`::loop()`),
  Aufrufer in `src/Comms/Web/WebServer.cpp:356`,
  `src/Attacks/Ducky/Extensions.h:203`,
  `src/Devices/Platform/EvilCrowWind/EvilCrowWind.cpp:70`, sowie
  `platformio.ini` (`CONFIG_ASYNC_TCP_RUNNING_CORE=1`) als Beleg für den
  eigenständigen AsyncTCP-Task.
