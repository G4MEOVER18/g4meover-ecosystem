# usb-army-penetrator — Agent: Polling-Intervall als `bool` statt Sekundenwert

## Was
`src/Attacks/Agent/Agent.cpp` verwaltet das Intervall, in dem das Gerät im
Serial-Modus nach einem host-seitigen Agenten pollt (`RequestAgentStatus`),
über eine statische Variable:

```cpp
#define USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT 15

static bool agentPollingEnabled = USB_AGENT_POLLING_ENABLED_DEFAULT;
static bool agentPollingIntervalInSec = USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT; // <-- bool
```

Die zweite Zeile ist ein Copy-&-Paste-Typfehler aus der Zeile darüber: der
Wert ist ausweislich Name (`...IntervalInSec`), Default (`15`) und
Registrierung ein **ganzzahliger Sekundenwert**, wird aber als `bool`
deklariert. Der Rest des Codes behandelt ihn korrekt als Int:

```cpp
// begin(): als Int16-Setting registriert, mit 15 als Default …
registerUserConfigurableSetting(CATEGORY_USB, USB_AGENT_POLLING_TIME_IN_SEC,
    USBArmyKnifeCapability::SettingType::Int16, (int16_t)USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT);
// … und per getInt() eingelesen:
agentPollingIntervalInSec = prefs.getInt(USB_AGENT_POLLING_TIME_IN_SEC,
                                         USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT);

// loop(): Intervall * 1000 als Millisekunden-Fenster
if (currentMillis - previousMillis >= agentPollingIntervalInSec * 1000) {
    Devices::USB::CDC.writeBinary(HostCommand::RequestAgentStatus, nullptr, 0);
    previousMillis = currentMillis;
}
```

Weil die Zuweisung `bool = int` das Ergebnis auf `0`/`1` zusammenfaltet,
geht jede Sekundenangabe verloren, **bevor** sie mit `1000` multipliziert
wird:

- Default `15` → `true` → `1 * 1000` = **1000 ms** (1 s statt 15 s).
- Jede Nutzerkonfiguration ≠ 0 (30, 60, 5 …) → `true` → ebenfalls **1000 ms**.
  Das Int16-Setting ist damit vollständig wirkungslos — jeder Wert außer 1
  wird ignoriert.
- Konfiguration `0` → `false` → `0 * 1000` = **0 ms**: die Zeitschranke ist
  immer erfüllt, der Poll läuft in **jedem** `loop()`-Durchlauf (Dauer-Spam
  der `RequestAgentStatus`-Nachricht an den USB-Serial-Host).

## Nutzen
- Das Poll-Intervall verhält sich wieder wie dokumentiert/registriert: Default
  15 s, per Setting frei einstellbar.
- Kein 15-facher (bzw. bei `0` unbegrenzter) Overhead mehr auf dem
  USB-Serial-Kanal, solange kein Agent verbunden ist — die
  `RequestAgentStatus`-Sonde belastet Host und Bus nicht mehr im
  Sekunden-/Dauertakt.
- Behebt eine stillschweigend tote Nutzereinstellung (Int16-Poll-Intervall),
  ohne Verhaltensänderung an irgendeiner anderen Stelle.

## Betroffene Dateien
- `src/Attacks/Agent/Agent.cpp` — Zeile 20 (Deklaration von
  `agentPollingIntervalInSec`). Einzige zu ändernde Stelle; alle Nutzungen
  (`begin()`/`loop()`) bleiben unverändert und werden durch den korrekten Typ
  erst sinnvoll.

## Risiko
- Minimal. Reine Typkorrektur einer einzelnen Deklaration; kein Schnittstellen-,
  Speicher- oder Ablaufeingriff. `int16_t` deckt den registrierten
  Int16-Wertebereich exakt ab. Der einzige sichtbare Effekt ist das
  wiederhergestellte, korrekte Poll-Intervall.
- Kein Persistenz-/Migrationsproblem: der Prefs-Schlüssel wird bereits als Int
  geschrieben und mit `getInt()` gelesen; nur die Zielvariable im RAM ändert
  ihren Typ.

## Port-Skizze
```diff
--- a/src/Attacks/Agent/Agent.cpp
+++ b/src/Attacks/Agent/Agent.cpp
@@
 static bool agentPollingEnabled = USB_AGENT_POLLING_ENABLED_DEFAULT;
-static bool agentPollingIntervalInSec = USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT;
+static int16_t agentPollingIntervalInSec = USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT;
 static unsigned long previousMillis = 0;
```

`int16_t` passt zum registrierten `SettingType::Int16`; alternativ `int`, da
`prefs.getInt()` einen `int` liefert — `int16_t` hält den Typ aber deckungs-
gleich zur Setting-Registrierung und ist für das 15-s-Default/Sekunden-Fenster
mehr als ausreichend.

## Testhinweis
- Host-Repro (ohne Hardware, in `repro-agent-poll-interval-bool/`):
  - `g++ -O2 -Wall -Wextra -o repro repro.cpp && ./repro`
  - Modelliert Deklaration, `getInt()`-Einlesen und die
    `Intervall * 1000`-Berechnung aus `loop()` für IST (`bool`) und SOLL
    (`int16_t`). Ergebnis: im `bool`-Modell liefern Default und alle
    konfigurierten Werte {15, 30, 60, 5} identisch **1000 ms**, `0` liefert
    **0 ms** (Dauerpoll); im `int16_t`-Modell jeweils `N * 1000 ms`. Exit 0,
    alle Assertions bestätigt.
- Lokaler Hardware-Test (nach Einbau des Patches):
  1. PlatformIO-Build wie gewohnt, auf das Gerät flashen, Gerät in den
     Serial-Modus versetzen (ohne verbundenen Host-Agenten).
  2. Über das Debug-Log prüfen, dass `RequestAgentStatus` im
     eingestellten Intervall (Default ~15 s) gesendet wird, nicht mehr
     im Sekundentakt.
  3. Poll-Intervall-Setting im Menü z. B. auf 30 s ändern; erwartet: der
     Sende-Abstand verdoppelt sich sichtbar (vorher: keinerlei Reaktion auf
     die Einstellung).

## Quelle
- `G4MEOVER18/usb-army-penetrator` (Fork, Commit
  `411b153d749f66a5c0059c39a3933c61971fcb2f`),
  `src/Attacks/Agent/Agent.cpp` (Zeile 20 Deklaration; `begin()` Zeilen
  30/33 Registrierung + `getInt()`; `loop()` Zeile 60 Intervall-Berechnung),
  `src/Attacks/Agent/Agent.h`.
