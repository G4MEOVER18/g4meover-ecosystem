# usb-army-penetrator — USBHID::IsQueueEmpty() sieht nur die Keyboard-Queue

## Was
`src/Devices/USB/USBHID.cpp` verwaltet drei getrennte Warteschlangen für
ausstehende HID-Reports:

```cpp
static std::queue<KEY_EVENT> eventsToProcess;                        // Tastatur
static std::queue<std::pair<uint8_t, uint16_t>> cdcEventsToProcess;   // Consumer Control (Medientasten)
static std::queue<MOUSE_EVENT> mouseEventsToProcess;                  // Maus
```

`IsQueueEmpty()` prüft aber nur die erste davon:

```cpp
bool USBHID::IsQueueEmpty()
{
    return eventsToProcess.empty();
}
```

Der einzige Aufrufer ist `DuckyPayload::loop()` (`src/Attacks/Ducky/DuckyPayload.cpp`),
und zwar als Gate, bevor die nächste Skriptzeile ausgeführt wird:

```cpp
else if (!Devices::USB::HID.IsQueueEmpty())
{
    // do nothing, yield to other event processing jobs
}
else if (!currentlyExecutingFile.empty() || !localCmdLineToExecute.empty())
{
    // ... nächste Zeile ausführen ...
}
```

Der Kommentar sagt klar, was gemeint ist: warten, bis alle noch ausstehenden
HID-Events verarbeitet sind, bevor die nächste Skriptzeile drankommt. Weil
`IsQueueEmpty()` aber nur `eventsToProcess` (Tastatur) sieht, ist dieses Gate
blind für alles, was ausschließlich `cdcEventsToProcess` oder
`mouseEventsToProcess` befüllt — und genau das tun zwei meiner
Ducky-Erweiterungsbefehle (`src/Attacks/Ducky/Extensions.h`):

```cpp
static int handleMouseJiggle(...)
{
    Devices::USB::HID.mouseMove(1, 1);
    Devices::USB::HID.mouseMove(-1, -1);
    return true;
}
```

`MOUSE_JIGGLE` (und `MOUSE_MOVE`, dieselbe Queue) rührt `eventsToProcess`
nie an. Solange ein Skript nur solche Zeilen ausführt, bleibt `IsQueueEmpty()`
dauerhaft `true`, egal wie voll `mouseEventsToProcess` bereits ist — der
Interpreter rückt bei jedem `loop()`-Tick eine weitere Zeile vor, während
`USBHID::loop()` pro Tick höchstens einen Report abschickt (`usb_hid->ready()`-
gated, mit `HID_WAIT_TIME`-Pause danach). Bei einer Folge von
`MOUSE_JIGGLE`-Zeilen ohne zwischengeschaltetes `DELAY` — genau das Muster,
das ein Dauer-Jiggler zur Anti-Idle-Nutzung typischerweise braucht, sei es
direkt so geschrieben oder über `REPEAT` erzeugt — produziert der Interpreter
damit 2 Events/Tick, während die Queue nur 1 Event/Tick abbaut: der
Rückstand in `mouseEventsToProcess` wächst linear mit der Skriptlänge, ohne
jede Schranke. Derselbe Blindspot betrifft `cdcEventsToProcess`
(Medientasten-Befehle, `consumer_device_keypress()` legt dort sogar zwei
Einträge pro Aufruf ab).

Kein Speicherfehler (der `std::queue` wächst korrekt mit), aber ein echter
Kontraktbruch des eigenen Yield-Gates mit unmittelbarer praktischer Folge:
unbegrenztes Heap-Wachstum bei jedem längeren Jiggler-/Medientasten-Skript,
und Skript-Fortschritt, der komplett vom physischen HID-Sendetempo
entkoppelt ist, obwohl das Gate genau das verhindern soll.

## Betroffene Dateien
- `src/Devices/USB/USBHID.cpp` (`USBHID::IsQueueEmpty()`) — Fix hier.
- `src/Devices/USB/USBHID.h` — keine Signaturänderung nötig.
- `src/Attacks/Ducky/DuckyPayload.cpp` — Aufrufer, unverändert; profitiert
  direkt vom Fix, da das Gate dort bereits so gemeint war.
- `src/Attacks/Ducky/Extensions.h` (`handleMouseJiggle`, `handleMouseMove`,
  Consumer-Control-Zeilen) — unverändert, sind die Quelle der bislang
  ungesehenen Events.
- `src/Devices/USB/NoUSBCore.cpp` (`USBHID::IsQueueEmpty() { return true; }`,
  No-USB-Baukonfiguration) — unverändert, dort gibt es keine Queues.

## Risiko
Gering, reine Bugfix-Erweiterung einer bereits vorhandenen Bedingung um zwei
zusätzliche, bereits existierende Member-Queues derselben Klasse — keine
neue Abhängigkeit, keine Änderung der Aufrufsignatur, kein Verhalten für
reine Tastatur-Skripte (die einzige bisher genutzte Queue bleibt
ausschlaggebend, wenn die anderen beiden ohnehin leer sind).

## Port-Skizze
```cpp
bool USBHID::IsQueueEmpty()
{
    return eventsToProcess.empty() && cdcEventsToProcess.empty() && mouseEventsToProcess.empty();
}
```
Eine Zeile, keine weiteren Änderungen nötig — `DuckyPayload::loop()` nutzt
das korrigierte Ergebnis automatisch, da es bereits exakt so gegen
`IsQueueEmpty()` gated.

## Testhinweis
Lokal (ohne Hardware): `repro-hid-queue-empty-mouse-cdc-blindspot/repro.cpp`
baut ein Host-Modell der drei Queues plus der beiden `loop()`-Gates
(`USBHID::loop()`-Drain-Priorität cdc > mouse > keyboard, `DuckyPayload::loop()`-
Gate) nach und lässt ein 50-Zeilen-`MOUSE_JIGGLE`-Skript laufen:
- **Alter Zustand** (`g++ -std=c++17 -O1 -g repro.cpp -o repro && ./repro`):
  das komplette Skript läuft in 50 Ticks unabhängig vom Rückstand durch,
  `mouseEventsToProcess`-Rückstand wächst linear auf 50 (skaliert 1:1 mit
  der Skriptlänge, ohne jede Schranke).
- **Gefixter Zustand** (`-DFIXED`): der Interpreter blockiert, bis die
  Queue leer ist, bevor er die nächste Zeile ausführt — Rückstand bleibt
  durchgehend bei höchstens 2 (der Produktion einer einzelnen Zeile), das
  Skript läuft trotzdem bis zum Ende durch, nur im Sendetempo der Hardware
  statt ungebremst.

Auf eigener Hardware zusätzlich empfohlen (nicht sicherheitskritisch, nur
Verhaltenskontrolle): ein Ducky-Skript mit vielen aufeinanderfolgenden
`MOUSE_JIGGLE`-Zeilen (oder `REPEAT` darauf) über mehrere Minuten laufen
lassen und beobachten, dass die Mausbewegung weiterhin gleichmäßig im
Sendetempo passiert statt in Schüben, sowie dass parallel ausgeführte
Tastatur-/Medientasten-Zeilen im selben Skript nicht mehr vor bereits
gequeuten Maus-/Medienevents "vorbeirutschen" können.
