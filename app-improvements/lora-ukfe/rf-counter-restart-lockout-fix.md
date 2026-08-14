# lora-ukfe — RF-Console sperrt sich selbst nach jedem App-Neustart aus (Rolling-Counter ohne Persistenz)

## Was
Ich habe die neue G4MEOVER-RF-Console (`rf/rf_comm.c`, `rf/ukfe_rf.c`) geprüft — den
CC1101-Funkkanal, der die bisherige UART/JSON-Bridge als Fernsteuerung zwischen
Flipper und dem Heltec-Agenten ersetzt (siehe `rf/RF_PROTOCOL.md`). Das Protokoll ist
sauber gebaut: MAC über Secret+Counter+Kommando (§5), CRC16, und ein monoton
steigender 32-Bit-Rolling-Counter als Anti-Replay-Schutz, ausdrücklich mit KeeLoq
verglichen.

Der Fund liegt nicht im Protokoll selbst, sondern in seiner einzigen Flipper-seitigen
Implementierung, `rf_comm.c`:

```c
typedef struct {
    const SubGhzDevice* device;
    bool     inited;
    uint32_t counter;
    ...
} RfComm;

static RfComm s_rf = {0};
...
bool rf_comm_send(const UkfeRfMessage* in) {
    ...
    msg.counter = ++s_rf.counter;   // Rolling-Counter
```

`s_rf` ist eine statische Variable, die nur für die Laufzeit des Flipper-Apps im RAM
lebt — bei jedem Neustart der App (Verlassen des Menüs und erneutes Öffnen,
Geräte-Neustart, Akku leer) ist `s_rf.counter` wieder `0`, während der Heltec-Agent
(läuft typischerweise durchgehend als stationäres Ziel) seinen zuletzt akzeptierten
Counter-Stand behält. Die Anti-Replay-Prüfung auf Empfängerseite (`ukfe_rf.c`,
`ukfe_rf_parse_frame()`) ist strikt:

```c
if(last_counter) {                                        // Anti-Replay
    if(m.counter <= *last_counter) return false;          // verbraucht/replay
    *last_counter = m.counter;
}
```

Nach jedem Neustart sendet der Flipper also wieder bei Counter 1, 2, 3, ... — und der
Heltec verwirft jedes einzelne dieser Kommandos, bis der lokale Counter den zuvor
erreichten Stand wieder überholt hat. Bei einer Konsole, die im Feld nach Bedarf immer
wieder geöffnet und verlassen wird (jeder Menüpunkt-Wechsel im aktuellen `scene_menu.c`
geht über `rf_comm_init()`/`rf_comm_send()`), heißt das: **jeder** Neustart macht die
RF-Console für eine unbestimmte Zahl an Tastendrücken komplett wirkungslos, ohne dass
das für mich am Flipper sichtbar wäre (keine ACK-Anzeige verdrahtet, siehe unten) — ich
sähe einfach, dass "Trigger"/"Deauth"/etc. nichts bewirken, und wüsste nicht, warum.

Dass das nicht beabsichtigt ist, zeigt der Code selbst an zwei Stellen:
- `rf/RF_PROTOCOL.md` §5 verspricht ausdrücklich ein *"Fenster +1000 gegen Desync,
  sonst Re-Pair"* — also einen Mechanismus, der genau dieses Szenario auffangen soll.
  In `ukfe_rf_parse_frame()` gibt es weder das Fenster noch einen Re-Pair-Pfad, nur die
  strikte `<=`-Ablehnung oben.
- `rf_comm.h` kommentiert `rf_comm_counter()` bereits mit *"Aktuellen Counter-Stand
  lesen (für Anzeige/**Persistenz**)"* — die Funktion existiert, wird aber im
  gesamten Repo nirgends aufgerufen (`grep -rn rf_comm_counter` liefert nur die
  Definition selbst). Die Persistenz war offensichtlich vorgesehen, aber nie
  angeschlossen.

## Nutzen
- Behebt eine Selbstaussperrung, die *bei jedem einzelnen App-Neustart* auftritt —
  nicht nur in einem Randfall. Da RF laut `RF_PROTOCOL.md` §6 der neue Standardweg für
  `UkfeModeSubGhz`/drahtlos werden soll, betrifft das den Kernbetrieb der Konsole.
- Der Fix baut exakt die in `rf_comm.h`/`RF_PROTOCOL.md` bereits angekündigte
  Persistenz nach — kein neues Konzept, sondern das Schließen einer offen gelassenen
  Lücke zwischen Doku/Kommentar und tatsächlichem Code.
- Löst das Problem, ohne die vom Protokoll geforderte Sicherheitseigenschaft
  (strikt monoton steigender, MAC-authentifizierter Counter, kein Replay möglich) in
  irgendeiner Weise aufzuweichen — im Gegenteil, es macht das im Protokoll erwähnte
  Fenster/Re-Pair-Fallback für den Normalfall (App-Neustart) überflüssig, weil der
  Desync gar nicht erst entsteht.
- Reine Zustands-/Persistenzlogik in `rf_comm.c`; `ukfe_rf.c` (Frame-Bau, MAC, CRC,
  Anti-Replay-Prüfung selbst) bleibt unverändert — das gemeinsame Protokoll, das auch
  auf der Heltec-Seite identisch implementiert sein muss (`ukfe_rf.h`-Kommentar),
  wird nicht angefasst.

## Betroffene Dateien
- `rf/rf_comm.c` — neue statische Hilfsfunktionen (`rf_counter_initial_reserve()`,
  `rf_counter_needs_bump()` reine Logik; `rf_counter_load()`/`rf_counter_save()`
  Storage-I/O), neues Feld `reserved_up_to` in `RfComm`, `rf_comm_init()` lädt beim
  ersten Aufruf den persistierten Stand und reserviert sofort vor, `rf_comm_send()`
  verlängert die Reservierung bei Bedarf.
- `application.fam` — `"storage"` zu `requires` ergänzen (die App nutzt bisher keine
  Storage-API).

## Port-Skizze / Patch

### Funktionsprinzip (wie bei KeeLoq-artigen EEPROM-Countern üblich)
Statt bei jedem Senden den Counter auf die SD-Karte zu schreiben (Flash-Verschleiß,
unnötig oft), wird eine **Reservierung mit Sicherheitsspanne** gespeichert: beim ersten
Start nach dem Laden wird sofort `geladener_wert + MARGIN` auf die SD-Karte
geschrieben und der lokale Counter beginnt bei `geladener_wert`. Läuft der lokale
Counter bis auf `MARGIN/2` an die gespeicherte Reservierung heran, wird die
Reservierung weiter nach vorne verschoben und neu gespeichert. Damit ist der
gespeicherte Wert *immer* mindestens `MARGIN/2` über dem zuletzt tatsächlich
gesendeten Counter — auch wenn die App ohne sauberes Beenden abstürzt oder der Akku
mitten in der Sitzung leer wird, ist der beim nächsten Start geladene Wert garantiert
größer als alles, was der Heltec je akzeptiert hat.

### `rf/rf_comm.c` — neue Felder und reine Logik (Storage-unabhängig, host-testbar)
```c
#include <storage/storage.h>

#define RF_COUNTER_MARGIN 200u
#define RF_COUNTER_PATH   APP_DATA_PATH("rf_counter.u32")

typedef struct {
    const SubGhzDevice* device;
    bool     inited;
    uint32_t counter;
    uint32_t reserved_up_to;    // NEU: gespeicherte obere Schranke (Crash-Sicherheitsspanne)
    uint8_t  syms[RF_SYM_MAX];
    uint16_t sym_count;
    uint16_t sym_pos;
} RfComm;

static RfComm s_rf = {0};

// ---- Reine Logik (kein Storage-I/O), host-testbar ohne Flipper-Hardware ----
static uint32_t rf_counter_initial_reserve(uint32_t loaded, uint32_t margin) {
    return loaded + margin;
}
static bool rf_counter_needs_bump(uint32_t counter, uint32_t reserved_up_to,
                                   uint32_t margin, uint32_t* new_reserved_up_to) {
    if(counter + margin / 2 < reserved_up_to) return false;
    *new_reserved_up_to = counter + margin;
    return true;
}

// ---- Storage-I/O ----
static uint32_t rf_counter_load(void) {
    uint32_t value = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, RF_COUNTER_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t got = storage_file_read(file, &value, sizeof(value));
        if(got != sizeof(value)) value = 0;   // fehlt/beschaedigt -> sicherer Fallback (0)
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return value;
}

static void rf_counter_save(uint32_t value) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, RF_COUNTER_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &value, sizeof(value));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
```

### `rf_comm_init()` — Counter beim ersten Aufruf laden und vorreservieren
```c
bool rf_comm_init(void) {
    if(s_rf.inited) return true;
    memset(&s_rf, 0, sizeof(s_rf));

    uint32_t loaded = rf_counter_load();                 // 0 bei erster Paarung
    s_rf.counter = loaded;
    s_rf.reserved_up_to = rf_counter_initial_reserve(loaded, RF_COUNTER_MARGIN);
    rf_counter_save(s_rf.reserved_up_to);                 // sofort vorreservieren

    subghz_devices_init();
    s_rf.device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!s_rf.device) { subghz_devices_deinit(); return false; }
    subghz_devices_begin(s_rf.device);
    subghz_devices_reset(s_rf.device);
    subghz_devices_load_preset(s_rf.device, FuriHalSubGhzPreset2FSKDev476Async, NULL);
    subghz_devices_set_frequency(s_rf.device, UKFE_RF_FREQUENCY_HZ);
    subghz_devices_idle(s_rf.device);
    s_rf.inited = true;
    return true;
}
```

### `rf_comm_send()` — Reservierung bei Bedarf verlängern
```c
bool rf_comm_send(const UkfeRfMessage* in) {
    if(!s_rf.inited || !in) return false;
    UkfeRfMessage msg = *in;
    msg.counter = ++s_rf.counter;   // Rolling-Counter

    uint32_t new_reserve;
    if(rf_counter_needs_bump(s_rf.counter, s_rf.reserved_up_to, RF_COUNTER_MARGIN, &new_reserve)) {
        s_rf.reserved_up_to = new_reserve;
        rf_counter_save(s_rf.reserved_up_to);
    }

    uint8_t frame[UKFE_RF_MAX_FRAME];
    size_t n = ukfe_rf_build_frame(RF_SECRET, &msg, frame, sizeof(frame));
    ...   // unveraendert ab hier
```

### `application.fam`
```python
App(
    appid="lora_ukfe",
    name="USB Army Knife FE",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="lora_ukfe_app",
    requires=["gui", "storage"],   # "storage" ergaenzt
    ...
)
```

Einziger inhaltlicher Eingriff: Counter-Ursprung und -Fortschreibung in `rf_comm.c`.
`ukfe_rf.c` (Frame-Format, MAC, CRC, die Anti-Replay-Prüfung `counter <= last_counter`
selbst), `rf/rf_comm.h`-Signaturen und alle Aufrufer (`scene_menu.c`) bleiben
unverändert — `rf_comm_init()`/`rf_comm_send()` behalten exakt ihre bisherige
Signatur und ihr bisheriges Rückgabeverhalten.

## Testhinweis (ohne Hardware)
Der eigentliche Fehler ist reine Zustandslogik (wann startet der Counter bei 0, wann
wird er persistiert) und lässt sich vollständig ohne CC1101/Heltec nachstellen.
`repro-rf-counter-restart/repro.c` bildet `rf_counter_initial_reserve()` und
`rf_counter_needs_bump()` 1:1 aus dem Patch nach, sowie die Heltec-seitige
`counter <= last_counter`-Prüfung aus `ukfe_rf_parse_frame()`, und simuliert zwei
Flipper-"Sitzungen" (App-Neustart dazwischen) sowohl für den Ist-Zustand als auch für
den Fix:

```
$ gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
$ ./repro
BUG:  Sitzung 2 nach Neustart: 50 von 60 Kommandos abgelehnt (Geraet effektiv unbenutzbar bis Counter aufgeholt hat)
FIX:  Sitzung 2 nach Neustart: 0 von 60 Kommandos abgelehnt
FIX:  nach unsauberem Absturz bei Counter 150 (Reserve war bei 300): erstes Kommando nach Neustart (Counter 301) sofort akzeptiert
Alle Szenarien bestaetigt (exit code 0)
```
Drittes Szenario bestätigt zusätzlich die Crash-Sicherheit: selbst wenn die App mitten
in einer Sitzung ohne sauberes Beenden abstürzt (kein finales Speichern), bleibt der
zuletzt gespeicherte Reservierungsstand oberhalb des tatsächlich verwendeten
Höchst-Counters, sodass die nächste Sitzung trotzdem ohne Ablehnung startet.

Funktionaler Test danach lokal auf dem Flipper (benötigt den gepaarten Heltec-Agenten
mit demselben `RF_SECRET`, siehe `RF_PROTOCOL.md` §5 "out-of-band gepairt"): einen
`TRIGGER`/`STATUS`-Befehl senden, App verlassen und neu öffnen, erneut senden — vor dem
Fix bleibt die Reaktion am Heltec (LED/ACK) beim zweiten Mal aus, nach dem Fix reagiert
er sofort wieder. Zusätzlich empfiehlt es sich, `/ext/apps_data/lora_ukfe/` einmal
manuell zu löschen, um den "erste Paarung"-Pfad (`loaded == 0`) zu prüfen.

## Risiko
- Niedrig: rein additiv in `rf_comm.c` (neue Felder/Funktionen, zwei ergänzte
  Codezeilen in `rf_comm_init()`/`rf_comm_send()`). Das Wire-Format, die MAC- und
  CRC-Berechnung sowie die Anti-Replay-Regel selbst (`ukfe_rf.c`) sind unverändert —
  ein bereits gepairter Heltec-Agent benötigt keine Anpassung.
- Neue Storage-Abhängigkeit (`"storage"` in `application.fam`) — Standard-Record, von
  praktisch jeder Flipper-App genutzt, kein zusätzliches Sicherheitsrisiko.
- Geringer Flash-Verschleiß durch die periodischen Speicherungen; durch die
  Batch-Reservierung (`RF_COUNTER_MARGIN = 200`) im Schnitt nur alle 100 Sendungen ein
  Schreibzugriff, nicht bei jedem einzelnen Kommando.
- Bewusst nicht behoben: der von `RF_PROTOCOL.md` §5 erwähnte Fenster-/Re-Pair-Pfad
  für einen *echten* Desync (z. B. Secret-Rotation, Ersatz-Flipper ohne die
  gespeicherte Datei, absichtliches Zurücksetzen) bleibt offen — dieser Patch behebt
  nur den Normalfall (App-/Geräte-Neustart), der bislang bei *jedem* Neustart
  auftritt. Ein echter Re-Pair-Mechanismus (z. B. manueller "Resync"-Befehl mit
  Bestätigung auf beiden Seiten) betrifft auch die Heltec-Firmware und bleibt für
  eine künftige Sitzung offen, falls gewünscht.
- Ganzzahlüberlauf von `uint32_t counter` nach 2^32 Sendungen ist unverändert
  theoretisch möglich (bestand schon vor diesem Patch), aber praktisch irrelevant
  (mehrere Milliarden Funkkommandos).

## Quelle
- `G4MEOVER18/lora-ukfe` (main): `rf/rf_comm.c` (`RfComm`, `rf_comm_init`,
  `rf_comm_send`), `rf/rf_comm.h` (`rf_comm_counter()`-Kommentar "für
  Anzeige/Persistenz"), `rf/ukfe_rf.c` (`ukfe_rf_parse_frame`, Anti-Replay-Prüfung),
  `rf/RF_PROTOCOL.md` §5 (dokumentiertes, aber nicht implementiertes
  Fenster-/Re-Pair-Verhalten), `application.fam`.
