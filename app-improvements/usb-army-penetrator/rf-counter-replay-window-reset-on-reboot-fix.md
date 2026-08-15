# usb-army-penetrator — RX-seitiger Anti-Replay-Counter der `ukfe_rf`-Konsole öffnet das Replay-Fenster nach jedem Geräte-Neustart

## Was
Ich habe die Empfängerseite der `ukfe_rf`-Funkkonsole in `usb-army-penetrator` geprüft
(`src/Comms/Radio/UkfeRfControl.cpp`/`.h`) — die Heltec/ESP32-seitige Gegenstelle zu der
Flipper-seitigen RF-Console aus `lora-ukfe`, die ich in einer früheren Sitzung geprüft hatte
(dort war der Fund die fehlende Persistenz des **sendenden** Counters, siehe
`app-improvements/lora-ukfe/rf-counter-restart-lockout-fix.md`; die Heltec-Seite lag damals
außerhalb des Repos und war ausdrücklich als ungeprüft vermerkt). `usb-army-penetrator`
implementiert genau diese Gegenstelle: `UkfeRfControl` empfängt signierte Funkbefehle vom
Flipper und löst darüber vorhandene Ducky-HID-Payloads aus (`WifiDeauth`, `EvilPortal`,
`BeaconSpam`, `Trigger`/`PayloadRun`).

Das gemeinsame Protokoll (`ukfe_rf.c`, wortgleich in beiden Repos, laut Header-Kommentar
"auf beiden Seiten identisch implementieren") erzwingt einen strikt monoton steigenden,
MAC-authentifizierten Rolling-Counter als Anti-Replay-Schutz:

```c
if(last_counter) {                                        // Anti-Replay
    if(m.counter <= *last_counter) return false;          // verbraucht/replay
    *last_counter = m.counter;
}
```

Der Fund liegt wieder nicht im Protokoll, sondern in seiner Anbindung — diesmal auf der
Empfängerseite:

```cpp
class UkfeRfControl : public USBArmyKnifeCapability
{
    ...
private:
    bool _ready = false;
    uint32_t _lastCounter = 0; // Anti-Replay-Fenster
};
```

```cpp
void UkfeRfControl::begin(Preferences &prefs)
{
    (void)prefs;
    ...
}
void UkfeRfControl::loop(Preferences &prefs)
{
    (void)prefs;
    ...
    if (ukfe_rf_parse_frame(UKFE_SECRET, frame, real_len, &msg, &_lastCounter))
    { ... dispatch(&msg); }
    ...
}
```

`_lastCounter` ist ein reines RAM-Feld der `UkfeRfControl`-Instanz. Das Framework reicht
`begin()`/`loop()` bei jeder Capability ausdrücklich ein `Preferences &prefs` durch (dieselbe
`Preferences prefs; prefs.begin("usbarmyknife");`-Instanz aus `main.cpp`, mit der z. B.
`Attacks::Agent` bereits eigene Einstellungen persistiert, siehe `Agent.cpp`
`prefs.getBool(...)`/`prefs.getInt(...)`), aber `UkfeRfControl` verwirft diesen Parameter
in beiden Methoden ausdrücklich mit `(void)prefs;` — exakt dasselbe Muster wie der bereits
gefundene Bug auf der Flipper-Seite (dort war `rf_comm_counter()` laut eigenem
Kommentar "für Anzeige/Persistenz" vorgesehen, aber nie angeschlossen; hier ist der
Persistenz-Mechanismus sogar schon als Funktionsparameter vorhanden und wird aktiv ignoriert).

**Warum das hier schwerer wiegt als der bereits gefundene TX-seitige Bug:** Auf der
Flipper-Seite führte die fehlende Persistenz zu einer Selbstaussperrung (Verfügbarkeits-
Bug: legitime Befehle werden nach einem Neustart abgelehnt, bis der Counter wieder
aufgeholt hat). Hier, auf der Empfängerseite, bewirkt der RAM-Reset das Gegenteil — er
**öffnet** das Anti-Replay-Fenster wieder: Jeder ESP32-Neustart (Absturz, Stromausfall/
Akku leer, versehentlicher Reset-Tastendruck, OTA-Update) setzt `_lastCounter` auf `0`
zurück. Ein Angreifer, der zuvor per SDR ein gültiges, signiertes Funkframe mitgeschnitten
hat (z. B. einen `WifiDeauth`- oder `EvilPortal`-Befehl mit Counter 25), kann dieses Frame
nach jedem Neustart des Geräts erneut senden — `25 <= 0` ist falsch, das Frame wird als
"neu" akzeptiert, obwohl es bereits einmal verwendet wurde. Der gesamte Zweck des Rolling-
Counters (Schutz vor Replay eines mitgeschnittenen, aber gültig signierten Befehls) ist
damit für jeden Angreifer wirkungslos, der einen Neustart des Zielgeräts abwarten oder
provozieren kann — und ein ESP32, der im Feld über Stunden/Tage per Akku betrieben wird,
neu startet gelegentlich ohnehin.

## Nutzen
- Schließt eine Sicherheitslücke im eigenen Anti-Replay-Mechanismus der `ukfe_rf`-Konsole,
  nicht nur einen Komfort-/Verfügbarkeits-Bug — passt direkt in den in CONSTRAINTS Punkt 5
  verlangten defensiven Rahmen: das eigene Pentest-Werkzeug soll selbst keine unnötig
  ausnutzbare Schwachstelle enthalten.
- Wiederverwendet exakt die bereits im Host-Repro der vorherigen Sitzung verifizierte
  Reserve-mit-Sicherheitsspanne-Logik (`rf_counter_initial_reserve()`/
  `rf_counter_needs_bump()` aus dem `lora-ukfe`-Fix), hier auf den RX-seitigen
  `last_counter` statt den TX-seitigen Sende-Counter angewendet — dieselbe bewährte
  Idee, keine neue Krypto/kein neues Protokollfeld.
- Nutzt exakt den Persistenz-Kanal, den das Framework für `UkfeRfControl` bereits
  bereitstellt (`Preferences &prefs`), statt eine neue Speicherlösung einzuführen —
  derselbe `usbarmyknife`-NVS-Namespace, den auch `Attacks::Agent` schon für eigene
  Einstellungen verwendet.
- Das Wire-Format, `ukfe_rf_mac()`/`ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()` und die
  `counter <= last_counter`-Regel selbst bleiben unverändert — ein bereits gepairter
  Flipper benötigt keine Anpassung, und `lora-ukfe` (Sender) ist von diesem Patch nicht
  betroffen.

## Betroffene Dateien
- `src/Comms/Radio/UkfeRfControl.h` — neues Feld `_persistedFloor` neben `_lastCounter`.
- `src/Comms/Radio/UkfeRfControl.cpp` — `begin()` lädt den persistierten Stand aus
  `prefs` statt `(void)prefs;`, `loop()` prüft nach jedem akzeptierten Frame, ob die
  Reserve nachgezogen werden muss.

## Port-Skizze / Patch

### `src/Comms/Radio/UkfeRfControl.h`
```cpp
private:
    bool _ready = false;
    uint32_t _lastCounter = 0;     // Anti-Replay-Fenster (RAM, pro Frame verglichen)
    uint32_t _persistedFloor = 0;  // NEU: in "prefs" gesicherte obere Schranke (Crash-/Reboot-Sicherheit)
```

### `src/Comms/Radio/UkfeRfControl.cpp`
```cpp
#define UKFE_RF_COUNTER_MARGIN 10u        // wie viele Counter-Werte vorreserviert werden
#define UKFE_RF_COUNTER_PREF_KEY "ukfeRfCtr"

// ---- Reine Logik (kein NVS-I/O), identisch zum bereits verifizierten TX-seitigen Muster
//      aus lora-ukfe/app-improvements/lora-ukfe/rf-counter-restart-lockout-fix.md ----
static uint32_t ukfeRfInitialReserve(uint32_t loaded, uint32_t margin) {
    return loaded + margin;
}
static bool ukfeRfNeedsBump(uint32_t counter, uint32_t reservedUpTo,
                             uint32_t margin, uint32_t *newReservedUpTo) {
    if (counter + margin / 2 < reservedUpTo) return false;
    *newReservedUpTo = counter + margin;
    return true;
}

void UkfeRfControl::begin(Preferences &prefs)
{
    uint32_t loaded = prefs.getUInt(UKFE_RF_COUNTER_PREF_KEY, 0);  // 0 bei Erst-Inbetriebnahme
    _lastCounter = loaded;                                        // vor dem Fix: fest 0
    _persistedFloor = ukfeRfInitialReserve(loaded, UKFE_RF_COUNTER_MARGIN);
    prefs.putUInt(UKFE_RF_COUNTER_PREF_KEY, _persistedFloor);      // sofort vorreservieren

    loraSpi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
    // ... Rest von begin() unveraendert ...
}

void UkfeRfControl::loop(Preferences &prefs)
{
    if (!_ready || !s_rxFlag)
        return;
    s_rxFlag = false;

    uint8_t frame[UKFE_RF_MAX_FRAME];
    int len = radio.readData(frame, UKFE_RF_MAX_FRAME);
    if (len == RADIOLIB_ERR_NONE)
    {
        UkfeRfMessage msg;
        size_t real_len = (size_t)frame[0] + 1;
        if (real_len > UKFE_RF_MAX_FRAME)
            real_len = UKFE_RF_MAX_FRAME;
        if (ukfe_rf_parse_frame(UKFE_SECRET, frame, real_len, &msg, &_lastCounter))
        {
            Debug::Log.info(TAG, "gueltiger Funk-Befehl");
            uint32_t newFloor;
            if (ukfeRfNeedsBump(_lastCounter, _persistedFloor, UKFE_RF_COUNTER_MARGIN, &newFloor))
            {
                _persistedFloor = newFloor;
                prefs.putUInt(UKFE_RF_COUNTER_PREF_KEY, _persistedFloor);
            }
            dispatch(&msg);
        }
    }
    radio.startReceive();
}
```

Einziger inhaltlicher Eingriff: Ursprung und Fortschreibung von `_lastCounter` in
`UkfeRfControl`. `ukfe_rf.c` (Frame-Format, MAC, CRC, die `counter <= last_counter`-Prüfung
selbst) bleibt unverändert — das gemeinsame Protokoll wird nicht angefasst, nur die
Empfängerseite lädt ihre Anti-Replay-Grenze jetzt aus dem bereits vorhandenen
`Preferences`-Namespace statt sie bei jedem Neustart auf `0` zu setzen.

## Testhinweis (ohne Hardware)
`repro-rf-counter-replay-window-reset/repro.c` nutzt die unveränderten Original-Dateien
`ukfe_rf.c`/`ukfe_rf.h` (1:1 aus diesem Repo übernommen, laut eigenem Kommentar
"plattformunabhaengig, ohne HW-Abhaengigkeit") — keine ESP32/RadioLib/Flipper-Abhängigkeit
nötig:

```
$ gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c ukfe_rf.c
$ ./repro
Szenario 1 (Sitzung vor Neustart): 0/50 abgelehnt (sollte 0 sein)
BUG:  captured WifiDeauth-Frame (Counter 25) nach Neustart erneut gesendet -> ANGENOMMEN (Replay erfolgreich!)
Szenario 2 (Sitzung vor Neustart, mit Fix): 0/50 abgelehnt, 10 NVS-Schreibzugriffe (statt 50)
FIX:  dasselbe captured Frame (Counter 25) nach Neustart erneut gesendet -> abgelehnt (Replay verhindert)
FIX:  neuer, legitimer Befehl (Counter 61) nach Neustart -> angenommen
Alle Szenarien bestaetigt (exit code 0)
```

Das Repro baut mit den echten `ukfe_rf_build_frame()`/`ukfe_rf_parse_frame()`-Funktionen
50 gültig signierte Frames (simuliert eine normale Sitzung), schneidet eines davon als
"vom Angreifer mitgeschnitten" mit, simuliert dann einen Geräte-Neustart (`_lastCounter`
zurück auf den jeweiligen Startwert) und spielt das mitgeschnittene Frame erneut ein —
einmal mit dem Ist-Zustand (`0`, Replay gelingt) und einmal mit der vorgeschlagenen
Fix-Logik (persistierter Floor, Replay wird abgelehnt, neue legitime Befehle funktionieren
weiterhin, Flash-Schreibzugriffe bleiben selten).

Funktionaler Test danach lokal (benötigt den gepaarten Flipper mit `lora-ukfe`, gleicher
`UKFE_RF_SECRET`/`RF_SECRET`): Befehl senden (z. B. `WifiDeauth`), ESP32 per Reset-Taste
oder Stromunterbrechung neu starten, mit einem SDR/zweiten Empfänger das zuvor
mitgeschnittene Frame erneut aussenden — vor dem Fix löst der Heltec/USB-Army-Knife-Client
den Payload erneut aus, nach dem Fix bleibt die Reaktion aus. Zusätzlich empfiehlt sich ein
Test der Erst-Inbetriebnahme (NVS-Namespace `usbarmyknife` einmal löschen, `loaded == 0`
prüfen) sowie ein harter Reset mitten in einer laufenden Sitzung (Crash-Pfad: der zuletzt
gespeicherte `_persistedFloor` muss danach weiterhin über jedem tatsächlich akzeptierten
Counter liegen).

## Risiko
- Niedrig: rein additiv in `UkfeRfControl.h`/`.cpp` (ein neues Feld, zwei neue freie
  Funktionen, ein `putUInt`/`getUInt`-Aufruf in `begin()`, ein bedingter `putUInt`-Aufruf
  in `loop()`). Wire-Format, MAC/CRC-Berechnung und die Anti-Replay-Regel selbst
  (`ukfe_rf.c`) sind unverändert.
- Neue NVS-Nutzung über den bereits vorhandenen `prefs`-Parameter — kein neuer Speicherpfad,
  kein zusätzliches Risiko; `UKFE_RF_COUNTER_MARGIN = 10` hält die Schreibhäufigkeit gering
  (ein Schreibzugriff etwa alle 5 akzeptierten Funkbefehle, deutlich seltener als bei den
  überwiegend User-getriggerten Konsolen-Befehlen ohnehin zu erwarten).
- Bewusst nicht behoben: ein *echter* Desync (z. B. Secret-Rotation oder Austausch des
  Flipper- oder Heltec-Geräts ohne die gespeicherte Datei) bleibt weiterhin ohne
  Re-Pair-Mechanismus — dieser Patch schließt nur das Zeitfenster, das *jeder* ESP32-
  Neustart bislang unabhängig vom Betriebsszenario öffnet.
- Verändert nicht die Auslösepfade selbst (`dispatch()`, `Attacks::Ducky`) — ausschließlich
  die Frage, ob ein Funkbefehl als "neu genug" akzeptiert wird, bleibt unangetastet.

## Quelle
- `G4MEOVER18/usb-army-penetrator` (master): `src/Comms/Radio/UkfeRfControl.cpp`/`.h`
  (`_lastCounter`, `begin()`/`loop()`, `(void)prefs;`), `src/Comms/Radio/ukfe_rf.c`/`.h`
  (`ukfe_rf_parse_frame`, Anti-Replay-Prüfung, Header-Kommentar "auf beiden Seiten
  identisch implementieren"), `src/USBArmyKnifeCapability.h` (`begin(Preferences&)`-
  Vertrag), `src/main.cpp` (`prefs.begin("usbarmyknife")`, `Comms::UkfeRf.begin(prefs)`),
  `src/Attacks/Agent/Agent.cpp` (bereits etabliertes `prefs.getBool/getInt`-Muster für
  Capability-eigene Einstellungen).
- `G4MEOVER18/g4meover-ecosystem`: `app-improvements/lora-ukfe/rf-counter-restart-lockout-fix.md`
  (TX-seitiger Fund derselben Protokoll-Anbindungslücke, Ursprung der hier wiederverwendeten
  Reserve-Logik).
