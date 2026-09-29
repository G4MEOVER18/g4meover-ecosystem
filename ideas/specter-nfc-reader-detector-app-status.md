# Specter (at0m-b0mb/Specter-FlipperZero) — passiver NFC-Feld-Sweep gegen Skimmer/Rogue-Reader

## Quelle

`github.com/at0m-b0mb/Specter-FlipperZero`, MIT-Lizenz, Autor `at0m-b0mb`. Geprüfter Stand:
Commit `a2b2e35c` (`main`, HEAD), `fap_version="3.1.1"`. Eigenständige Kleinanwendung (5440
Zeilen Firmware-Quelltext über 36 Dateien, dazu ein separater `test/`-Ordner mit sieben
host-seitigen Test-Binärzielen für `emitter_classify`/`cadence`/`ema`/`field_scale`/
`survey_verdict`/`log_wrap`/`log_filter` — laut README 527 Einzelprüfungen), kein
RogueMaster-Bündel-Fund, aktiv gepflegt (`docs/changelog.md`, mehrere benannte Releases bis
v3.1.1, README nennt eine "security & correctness audit"-Version v2.9).

Wie in der Nyx-Sitzung vorgemerkt: Teil desselben vierteiligen Gegenüberwachungs-Werkzeugsatzes
von `at0m-b0mb` — Nyx (IR, bereits geprüft), **Specter** (NFC, diese Sitzung), Argus (WiFi,
laut README ESP32-gestützt), GhostTag (BLE) — beide Letzteren weiterhin offen für eine künftige
Sitzung.

## Was

Specter macht aus dem eingebauten ST25R3916-NFC-Chip einen reinen Feldsensor gegen aktive
13,56-MHz-Leser (Kartenskimmer an Zahlungsterminals, verdeckte Türleser, ein unter einem Tisch
angebrachter Logger):

- Nutzt ausschließlich `furi_hal_nfc_field_detect_start/_stop/_field_is_present()` — das
  ST25R3916-Hardwarebit "externer Träger gerade an/aus" — und sendet dabei nachweislich nie
  (per Grep über den gesamten Quellbestand bestätigt: keinerlei `furi_hal_nfc_poller_*`-,
  `_listener_*`- oder Feld-**Erzeugungs**-Aufruf, nur Feld-**Erkennung**).
- Fünf Modi: **Sweep** (EMF-artiger Live-Meter mit Trendpfeil/Peak-Hold), **Fingerprint**
  (klassifiziert die Trägerkadenz als Continuous/Polling/Intermittent/Unknown mit
  Konfidenzwert), **Site Survey** (fester Zeitfenster-Sweep → ein Verdikt CLEAN/TOO
  SHORT/TRACE/ACTIVE), **Watch** (unbeaufsichtigte Dauerüberwachung mit Weck-Alarm), **Logbook**
  (`.txt`+`.csv` auf der SD-Karte, gefiltert nach Fundtyp).
- Kernmesskette ist bewusst als reine, hardwarefreie Entscheidungsschicht abgetrennt
  (`helpers/emitter_classify.{c,h}`, `helpers/cadence.h`, `helpers/survey_verdict.{c,h}`,
  `helpers/field_scale.{c,h}`, `helpers/ema.h`, `helpers/present_hold.h`,
  `helpers/log_wrap.h`, `helpers/log_filter.h`) — dieselbe Bauweise wie bei Nyx, und laut den
  zahlreichen Inline-Kommentaren ebenfalls bereits durch eine eigene "Honesty pass" des Autors
  gegangen (jeder historische Fund — EMA-Rundungsfehler, unbegrenzt wachsender Kadenz-Ring,
  Zeichenumbruch mitten im Wort, Off-by-one beim Logbuch-Filter nach einem abgeschnittenen
  Tail-Read — ist im Code selbst mit Root-Cause dokumentiert, nicht nur behauptet).
- Skalierung des Rohtastgrads auf einen vollen 0–100-Meter (`SPECTER_FULL_SCALE_DUTY = 30`, aus
  Hardware-Messungen: ein Leser polling-typisch bei 20–35 % Tastgrad sättigt sonst weit unter
  dem oberen Skalenende); Einstellungs-Persistenz mit V2→V3-Migrationspfad
  (`specter_settings.c`) und Eingabe-Clamping gegen eine manipulierte/handbearbeitete
  Einstellungsdatei.

## Nutzen

Ergänzt die in der Nyx-Sitzung neu angelegte Kategorie **Counter-Surveillance/TSCM** um den
NFC-Kanal — bisherige eigene Gegenüberwachungs-Funde decken jetzt RF (SubGHz Jammer Detector),
WiFi (Rogue AP Detector), NFC-Karten (NFC Canary), UART (UART Sniff) und IR (Nyx) ab; Specter
schließt die Lücke bei aktiven NFC-**Lesern** selbst (statt nur bei geklonten Karten). Direkt
ohne Zusatzhardware nutzbar (reiner Software-Sweep über den vorhandenen NFC-Chip), sinnvoll
z. B. beim autorisierten Sweep der eigenen Zahlungsgeräte, Türleser oder als Abrundung eines
physischen Zutrittskontroll-Audits neben den bereits vorhandenen Wiegand-/H10301-Funden.

## Aufwand

**Gering.** Reiner Quelltext-Build gegen die eigene `mntm-012`-SDK, kein Firmware-Eingriff,
keine Submodul-Abhängigkeit, `fap_category="NFC"` ist eine in Momentum bereits vorhandene
Standardkategorie (`requires=["gui"]`, sonst keine externen App-Abhängigkeiten). Lokal: Repo
klonen, `ufbt launch` (oder `fbt`) gegen die eigene `G4MEOVER-FW`-Toolchain, auf Gerät testen.
Kein Lötaufbau, keine zusätzliche Peripherie nötig (im Unterschied zu Nyx' optionalem
Probe-Modus).

## API-87-Status

**✅ Quelltextlich verifiziert, kompatibel — kein Fund.**

Gegen den eigenen `Next-Flip/Momentum-Firmware`-Klon (Tag `mntm-012`, API 87.1) geprüft:

- Alle sieben verwendeten `furi_hal_nfc_*`-Symbole in `targets/f7/api_symbols.csv` vorhanden:
  `furi_hal_nfc_acquire`, `_release`, `_low_power_mode_start`, `_low_power_mode_stop`,
  `_reset_mode`, `_field_detect_start`, `_field_detect_stop`, `_field_is_present`.
- Signaturen gegen `targets/furi_hal_include/furi_hal_nfc.h` einzeln abgeglichen — exakt
  identisch zu dem, wie `helpers/field_detector.c` sie aufruft (u. a. `bool
  furi_hal_nfc_field_is_present(void)`, parameterlos, wie im Worker verwendet).
- Übrige API-Fläche (`furi_thread_alloc_ex/_set_priority/_start/_join/_free`,
  `furi_mutex_alloc/_acquire/_release/_free`, `furi_delay_tick`, `furi_get_tick`,
  `furi_kernel_get_tick_frequency`, `saved_struct_load/_save`, `storage_file_*`,
  `furi_hal_rtc_get_datetime`, ViewDispatcher/SceneManager/NotificationApp) ist Standard-Furi-
  Kern-API, bei API 87.1 unverändert vorhanden — keine der von Nyx bekannten Zielplattform-
  Sonderfälle (kein ADC-Pin-Layout, keine `GpioPinRecord`-Abhängigkeit; Specter braucht keinen
  Erweiterungsport).
- Der von der App selbst dokumentierte Zwei-Build-Ansatz (`specter.fap` für Stock-API-87-Kanal,
  `specter-fw-dev.fap` für Unleashed/RogueMaster/Momentum-Dev-Kanal) betrifft nur die
  vorgefertigten Release-Binaries auf GitHub; ein eigener Quelltext-Build gegen die eigene
  `mntm-012`-SDK ist davon unabhängig und wie oben bestätigt symbolkompatibel.

**Geprüfter Codeumfang (~1820 von 5440 Zeilen Firmware-Quelltext, gezielt auf die
messungskritische/sicherheitsrelevante Schicht statt breit über alle Views/Szenen):**
vollständig `helpers/field_detector.c` (Sampling-Worker inkl. Thread-Lebenszyklus,
Kalibrierungs-Zustandsmaschine, Mutex-Disziplin zwischen UI- und Worker-Thread — u. a. die im
Code dokumentierte Race zwischen zwei schnell aufeinanderfolgenden Kalibrierungsanforderungen,
laut Kommentar bereits selbst gefunden und durch sofortiges Publizieren des Zustandswechsels vor
dem nächsten Sampling-Fenster behoben), `helpers/emitter_classify.c`/`.h` (Kadenz→Klassen-
Entscheidungslogik inkl. Konfidenzberechnung — Grenzfälle Silence/Continuous/Unknown/
Polling/Intermittent einzeln nachgerechnet, kein Overflow bei den `uint32_t`-Zwischenwerten
gefunden), `helpers/cadence.h` (Ringpuffer-Zeitmessung inkl. der im Code dokumentierten
Discontinuity-Regel gegen unbegrenzt wachsende "ein Lauf = eine Phase"-Fehlklassifikation),
`helpers/survey_verdict.c`/`.h`, `helpers/field_scale.c`/`.h`, `helpers/ema.h`,
`helpers/present_hold.h`, `helpers/log_wrap.h`, `helpers/log_filter.h` (alle acht: reine,
zustandslose bzw. flach zustandsbehaftete Funktionen, Grenzfälle Null/Puffer-Erschöpfung/
Divisions-durch-Null einzeln geprüft, keine gefunden), `helpers/specter_log.c` (SD-Schreibpfad:
Größendeckel `SPECTER_LOG_MAX_BYTES`, Tail-Read mit Entry-Grenzen-Erkennung, CSV-Escaping von
Komma/Zeilenumbruch im Detail-Text — Puffergrößen `DETAIL_MAX`/`STAMP_MAX`/Summenpuffer gegen
`snprintf`-Rückgabewerte nachgerechnet, kein Truncation-Fund), `helpers/specter_settings.c`
(V2→V3-Migration, `_Bool`-Sanitisierung gegen eine manipulierte Einstellungsdatei), `specter.c`
(Alarm-/Benachrichtigungs-Verdrahtung inkl. Rate-Limiting gegen eine unbegrenzte
Benachrichtigungswarteschlange, App-Lebenszyklus) sowie `scenes/specter_scene_watch.c` (der
unbeaufsichtigte Dauerbetrieb — geprüft auf denselben Fehlerklassen-Typ wie die vorher in diesem
Repo gefundenen HID-Jiggler-Timing-Bugs: Intervallarithmetik gegen Über-/Unterlauf bei
`strength_ref` 0–100 durchgerechnet, kein Fund). Kein eigener Fund — vergleichbar mit Nyx ist
auch dieser Autor bereits ungewöhnlich gründlich selbstauditierend (jeder im Code erwähnte
historische Bug ist mit Root-Cause und Fix dokumentiert). Ungeprüfter Rest (~3620 Zeilen,
überwiegend Zeichen-/Layout-Code in `views/*_view.c` sowie die restlichen Szenen-Dateien
`scenes/specter_scene_{sweep,fingerprint,survey,settings,logbook,logfilter,logclear,about,
splash,start,config}.c`) für eine künftige Sitzung vorgemerkt.

## Sicherheitsrahmen

Rein passiv/defensiv — genau der in `CONSTRAINTS.md` verlangte Rahmen. Sendet nachweislich nie
(kein Feld-Erzeugungs- oder Poller-/Listener-Aufruf im gesamten Quellbestand, nur die
Hardware-Feld-**Erkennung**), erfasst keine Kartendaten oder Zugangsdaten, dient der eigenen
Gegenüberwachung (Suche nach nicht offengelegten NFC-Lesern in Räumen/an Geräten, die man selbst
nutzt oder für die man eine Autorisierung hat).

## Nächste Recherche-Aufträge

- Restlichen Codeumfang (Views/verbleibende Szenen, überwiegend Zeichenroutinen) für eine
  künftige Sitzung vorgemerkt.
- **Argus** (WiFi Deauth/Evil-Twin-Watchdog, laut README ESP32-gestützt — vermutlich ähnlich zu
  den bereits gekaten Rogue-AP-/BLE-Scanner-Funden) und **GhostTag** (BLE-Anti-Stalking, findet
  AirTags/Tiles/SmartTags) desselben Autors/Werkzeugsatzes weiterhin als offene Kandidaten
  vorgemerkt, noch nicht quelltextlich verifiziert.
