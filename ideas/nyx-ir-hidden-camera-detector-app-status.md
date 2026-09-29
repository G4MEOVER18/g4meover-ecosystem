# Nyx (at0m-b0mb/Nyx-FlipperZero) — Infrared-Sweep-Detektor für verdeckte Nachtsicht-Kameras

## Quelle

`github.com/at0m-b0mb/Nyx-FlipperZero`, MIT-Lizenz, Autor `at0m-b0mb`. Geprüfter Stand: Commit
`5552524` (2026-09-26), `fap_version="1.2"`. Eigenständige Kleinanwendung (2402 Zeilen über 15
Quelldateien: `nyx.c`, `helpers/ir_sense.c`/`.h` (Kernmessung), `helpers/nyx_store.c`
(Einstellungen), fünf Szenen, drei Views), kein RogueMaster-Bündel-Fund, aktiv gepflegt
(`CHANGELOG.md` dokumentiert eine ausführliche "Honesty pass" v1.2 mit klaren
Root-Cause-Begründungen für jeden behobenen Fund).

Laut README Teil eines vierteiligen Gegenüberwachungs-Werkzeugsatzes desselben Autors: Nyx (IR),
Specter (passiver NFC-Feld-Sweep), Argus (WiFi), GhostTag (BLE) — nur Nyx für diese Sitzung
geprüft, die drei anderen als Kandidaten für eine künftige Sitzung vorgemerkt (siehe unten).

## Was

Verdeckte Nachtsicht-Kameras beleuchten den Raum mit für das Auge unsichtbarem 850/940-nm-Infrarot.
Nyx macht dieses Leck zu einem Messwert:

- **Onboard-Modus:** nutzt den eingebauten TSOP-75338-IR-Empfänger (bandpassgefiltert auf 38 kHz,
  demoduliert). Zählt Flankenwechsel/Tastgrad statt zu dekodieren — erkennt jede gepulste/modulierte
  IR-Quelle (Fernbedienungen, Beacons, viele Kamera-Illuminatoren), ist aber gegenüber einer mit
  konstantem Gleichstrom betriebenen Nachtsicht-LED nachweislich blind (Bandpassfilter wirft
  Gleichlicht weg — von der App selbst so dokumentiert, nicht verschwiegen).
- **Probe-Modus (optional):** ein handelsüblicher IR-Fototransistor (~1 €, 940 nm, tageslichtgefiltert)
  plus 10-kΩ-Widerstand an einem beliebigen ADC-fähigen Erweiterungsport-Pin liest den echten
  Gleichspannungspegel und unterscheidet über die Rippel-Frequenz (Mittelwert-Kreuzungen im
  32-ms-Abtastburst) STEADY (Nachtsicht-Signatur) von FLICKER (Netzbrumm, 70–150 Hz) von PULSED.
  Ambient-Null (8 Fenster Mittelwert bei Aktivierung) macht die Anzeige "Überschuss über
  Raumhelligkeit" statt eines absoluten Rohwerts.
- Auge-Messwertanzeige (Ring/Pegel/Peak-Tick/Trend-Pfeil), Ton/Vibration/LED-Alarm mit
  Präsenz-Hysterese (mind. 3 von 16 Fenstern über der Schwelle, erst bei völliger Stille wieder
  gelöscht — verhindert Flackern bei bursty Quellen), Einstellungs-Persistenz mit
  Rückwärtskompatibilität zum v1-Dateilayout.
- Listen-only: sendet nie eigenes IR (kein `furi_hal_infrared_async_tx_*`-Aufruf im gesamten
  Quellbestand — per Grep bestätigt), reine Empfangsanwendung.

## Nutzen

Füllt eine bislang komplett fehlende Kategorie im eigenen Katalog: **Counter-Surveillance/TSCM**
(technische Überwachungsgegenmaßnahmen). Bisherige defensive Erkennungs-Funde (SubGHz Jammer
Detector, Rogue AP Detector, NFC Canary, UART Sniff) decken RF/WiFi/NFC-Feld ab — Nyx ergänzt
IR als eigenständigen Kanal, nützlich beim autorisierten Sweep eines Hotelzimmers/Airbnb/
Besprechungsraums vor sensiblen Gesprächen oder als Ergänzung zu einem physischen
Zutrittskontroll-Audit (Wiegand Reader/Access Audit/H10301 Writer) um eine rein passive,
funkstille Vorabprüfung. Ohne Zusatzhardware sofort nutzbar (Onboard-Modus); Probe-Modus ist ein
reiner Steckaufbau ohne Löten.

## Aufwand

**Gering.** Reiner Quelltext-Build gegen die eigene `mntm-012`-SDK, kein Firmware-Eingriff, keine
Submodul-Abhängigkeit. `fap_category="Infrared"` ist eine in Momentum bereits vorhandene
Standardkategorie. Lokal: Repo klonen, `ufbt launch` (oder `fbt`) gegen die eigene
`G4MEOVER-FW`-Toolchain, auf Gerät testen. Für den Probe-Modus zusätzlich Fototransistor+Widerstand
auf einen ADC-Pin des Erweiterungsports auflöten/stecken (README dokumentiert die Verdrahtung,
App selbst führt beim ersten Start durch einen Pull-up-Belastungstest, der erkennt, ob überhaupt
etwas angeschlossen ist).

## API-87-Status

**✅ Quelltextlich verifiziert, kompatibel — kein Fund.**

Gegen den eigenen `Next-Flip/Momentum-Firmware`-Klon (Tag `mntm-012`) geprüft:

- Alle verwendeten HAL-Symbole in `targets/f7/api_symbols.csv` vorhanden und mit identischer
  Signatur: `furi_hal_infrared_async_rx_start/_stop/_set_timeout/_set_capture_isr_callback/
  _set_timeout_isr_callback`, `furi_hal_infrared_is_busy`, `furi_hal_adc_acquire/_release/_read/
  _convert_to_voltage/_configure_ex`, sowie die Variablen `gpio_pins[]`/`gpio_pins_count`.
- `GpioPinRecord`-Struct-Layout (`targets/f7/furi_hal/furi_hal_resources.h`) exakt gegen die von
  `ir_sense.c::build_probe_pins()` gelesenen Felder (`pin`, `name`, `channel`, `number`, `debug`)
  abgeglichen — identisch, keine Feldumbenennung/-Neuanordnung seit dem von der App
  vorausgesetzten Stand.
- Eigene `mntm-012`-Pintabelle (`furi_hal_resources.c`) ausgezählt: genau sechs nicht-Debug-Pins
  mit ADC-Kanal (`PA7`/`PA6`/`PA4`/`PC3`/`PC1`/`PC0`) — deutlich unter der von der App fest
  reservierten `MAX_PROBE_PINS=8`, also kein Pin geht durch den `build_probe_pins()`-Größendeckel
  verloren.
- Der von der App selbst dokumentierte Zwei-Build-Ansatz (`nyx.fap` für Stock-API-87-Kanal,
  `nyx-fw-dev.fap` für Unleashed/RogueMaster/Momentum-Dev-API-88-Kanal) betrifft nur die
  vorgefertigten Release-Binaries auf GitHub; ein eigener Quelltext-Build gegen die eigene
  `mntm-012`-SDK (API 87.1) ist davon unabhängig und wie oben bestätigt symbolkompatibel.

**Geprüfter Codeumfang (~1400 von 2402 Zeilen, gezielt auf die messungskritischen/
sicherheitsrelevanten Pfade):** vollständig `helpers/ir_sense.c` (Kern-Messengine: ISR-Flankenzählung,
Präsenz-Hysterese, Probe-ADC-Burst-Abtastung, Ambient-Null, Thread-Lebenszyklus inkl. der in
`CHANGELOG.md` als v1.2-Fund dokumentierten Worker-Verwaisung), `helpers/nyx_store.c`
(Einstellungs-Laden/-Speichern inkl. v1→v2-Migration, Eingabe-Clamping gegen eine manipulierte
SD-Karte), `scenes/nyx_scene_sweep.c` (Haupt-Eingabeschleife: Sensitivitäts-Änderung,
Alarm-Timing-Arithmetik geprüft — kein Overflow/Underflow bei den Intervall-/Pegel-Berechnungen),
`scenes/nyx_scene_settings.c`, `scenes/nyx_scene_probe.c`. Kein eigener Fund — die
Kern-Messengine ist bereits durch die eigene v1.2-"Honesty pass" des Autors ungewöhnlich
gründlich durchgesehen (dokumentierte Root-Cause-Fixes für Chatter-Verhalten, Presence/Meter-
Inkonsistenz, verwaiste Worker-Threads, tote Validierung). Ungeprüfter Rest (~1000 Zeilen,
überwiegend reines Zeichnen/Layout in `views/probe_view.c`/`views/sweep_view.c`/
`views/splash_view.c` sowie `scenes/nyx_scene_about.c`/`_splash.c`/`_start.c`) für eine
künftige Sitzung vorgemerkt.

## Sicherheitsrahmen

Rein passiv/defensiv — genau der in `CONSTRAINTS.md` verlangte Rahmen. Sendet nachweislich nie
(kein `furi_hal_infrared_async_tx_*`-Aufruf im Quellbestand), erfasst keine fremden Zugangsdaten
oder Kartendaten, dient der eigenen Gegenüberwachung (Suche nach nicht offengelegten
Überwachungsgeräten in Räumen, die man selbst nutzt oder für die man eine Autorisierung hat).

## Nächste Recherche-Aufträge

- Restlichen Codeumfang (Views/verbleibende Szenen, überwiegend Zeichenroutinen) für eine
  künftige Sitzung vorgemerkt.
- Die drei Schwester-Werkzeuge desselben Autors/Werkzeugsatzes als neue Kandidaten für die
  Kategorie Counter-Surveillance/TSCM vorgemerkt: **Specter** (`at0m-b0mb/Specter-FlipperZero`,
  passiver 13,56-MHz-NFC-Feld-Sweep über den eingebauten NFC-Chip, keine Zusatzhardware, laut
  eigener Beschreibung ebenfalls sendefrei), **Argus** (WiFi) und **GhostTag** (BLE) — beide
  laut Websuche vermutlich ESP32/Marauder-gestützt wie die bereits gekaten Rogue-AP-/
  BLE-Scanner-Funde, noch nicht quelltextlich verifiziert.
