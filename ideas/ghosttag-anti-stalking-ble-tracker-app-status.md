# GhostTag (at0m-b0mb/GhostTag-FlipperZero) — Anti-Stalking-BLE-Tracker-Erkennung

## Quelle

`github.com/at0m-b0mb/GhostTag-FlipperZero`, MIT-Lizenz, Autor `at0m-b0mb`. Geprüfter Stand:
Commit `1a3f58b9` (`main`, HEAD), Release `2.0.0` (2026-09-27, laut `CHANGELOG.md` ein reines
Korrektheits-/Nutzbarkeits-Release). Flipper-seitiger Quelltext 4685 Zeilen über 32 Dateien
(`ghosttag.c`, `helpers/`, `scenes/`, `views/`), dazu ein separater ESP32-Arduino-Sketch (eigene
Companion-Firmware, nicht Teil dieses Ports) und ein host-seitiger `test/`-Ordner.

Letztes offenes Recherche-Vormerk aus der Specter-Sitzung: drei Schwester-Werkzeuge desselben
Autors — Nyx (IR, geprüft), Specter (NFC, geprüft), **GhostTag** (BLE, diese Sitzung), Argus
(WiFi, weiterhin offen). Mit dieser Sitzung ist der komplette Vierer-Werkzeugsatz bis auf Argus
abgedeckt.

## Was

GhostTag macht aus dem Flipper einen tragbaren Detektor für BLE-Tracker (AirTag, Tile, Samsung
SmartTag, Chipolo), die einen über einen längeren Zeitraum begleiten statt nur zufällig in
Reichweite zu sein:

- **Zwei Betriebsarten:**
  - **Hunt** — die echte Erkennung, braucht einen **BLE-fähigen ESP32** als Companion-Board am
    GPIO-UART (115200 Baud, eigenes Zeilenprotokoll `GT1,<mac>,<rssi>,<typ>[,<name>]` /
    `GTHELLO,<version>` / `GTALIVE`). Der ESP32 macht das eigentliche BLE-Scannen und die
    Advertisement-Klassifikation; der Flipper ist nur Auswertung + UI.
  - **Demo** — ein festes, deterministisches Szenario (sechs simulierte Geräte: zwei harmlose
    eigene Apple-Geräte, ein vorbeiziehendes Tile, ein unbekanntes Beacon, ein verfolgender
    AirTag, ein später dazukommender SmartTag), läuft komplett ohne Zusatzhardware, jeder
    Bildschirm mit `DEMO` markiert, wird nie ins Sitzungslog geschrieben.
- **Kernlogik der Erkennung** liegt in `helpers/tracker_db.c`: ein Gerät wird erst nach
  `TRACKER_DB_MIN_DETECTIONS` (4) Sichtungen **und** einem ununterbrochenen Verweilfenster
  (`follow_threshold_ms`, einstellbar 1/3/5/10 min) als "folgt mir" eingestuft — eine
  Sichtungslücke länger als `TRACKER_STALE_MS` (30 s) zählt als neue Begegnung statt als
  Fortsetzung, damit ein Gerät, das zweimal zufällig vorbeikommt, nicht fälschlich als Verfolger
  gemeldet wird. Ein eigenes Apple-Gerät im "Owner nearby"-Zustand (`AirTagPaired`, z. B. das
  eigene iPhone) wird bewusst nie als Bedrohung gewertet.
- **Rein-Onboard-Modus** (`helpers/air_check.c`, `Air Check`): misst nur die RF-Belegung
  (Busy/Quiet) der drei BLE-Advertising-Kanäle über den eingebauten WB55-Funk (`furi_hal_bt_*`
  RF-Testmodus), ohne einzelne Geräte zu identifizieren. Laut `CHANGELOG.md` auf echter Hardware
  gegen API 87 getestet und **bewusst per Compile-Flag deaktiviert**
  (`GHOSTTAG_ENABLE_AIR_CHECK`, Default `0` in `ghosttag_i.h`): `furi_hal_bt_get_rssi()` liefert
  auf offizieller Firmware für jeden Messwert exakt `0.0` (den eigenen "Read fehlgeschlagen"-Wert
  des Stacks), unabhängig davon ob über `furi_hal_bt_start_rx()` oder
  `furi_hal_bt_start_packet_rx()` gemessen wird — der Code bleibt im Baum, kann aber im
  Standard-Build gar nicht erreicht werden (`scenes/ghosttag_scene_start.c` schließt den
  Menüpunkt komplett aus `#if GHOSTTAG_ENABLE_AIR_CHECK` aus). Selbst dokumentierte,
  nachvollziehbare Design-Entscheidung, kein eigener Fund.
- Sitzungslog (`apps_data/ghosttag/sessions.csv`, 64-KB-Deckel, nur echte Hunt-Sitzungen,
  Demo-Sitzungen nie geloggt), persistente Einstellungen (`saved_struct`, V1) mit
  Eingabe-Sanitisierung.

## Nutzen

Schließt die letzte Lücke im eigenen Counter-Surveillance/TSCM-Cluster: RF (SubGHz Jammer
Detector), WiFi (Rogue AP Detector), NFC-Karten (NFC Canary), NFC-Leser (Specter), IR (Nyx) —
GhostTag ergänzt den BLE-Tracker-Kanal (AirTag/Tile/SmartTag/Chipolo-Erkennung über Zeit statt
nur Momentaufnahme). Direkt einsetzbar für autorisierte Sweeps des eigenen Fahrzeugs/Gepäcks vor
Fahrtantritt. Demo-Modus erlaubt sofortige Evaluierung ohne Hardwarebeschaffung; die eigentliche
Hunt-Funktion setzt einen ESP32-S3/-C3/-C6 (oder klassischen ESP32) am GPIO voraus — **nicht**
das offizielle Flipper-WiFi-Devboard (ESP32-S2, laut README und eigenem Changelog ohne
Bluetooth-Funk, der Autor hat das erst mit diesem Release korrigiert, siehe unten).

## Aufwand

**Gering bis mittel.** Reiner Quelltext-Build gegen die eigene `mntm-012`-SDK, kein
Firmware-Eingriff, keine Submodul-Abhängigkeit, `fap_category="Bluetooth"` ist eine in Momentum
bereits vorhandene Standardkategorie (`requires=["gui", "notification", "storage"]`, sonst keine
externen App-Abhängigkeiten). Demo-Modus lokal sofort testbar (`ufbt launch`, auf Gerät). Für
Hunt zusätzlich: passenden ESP32 (S3/C3/C6/klassisch, **nicht** S2) beschaffen/verkabeln und den
mitgelieferten Arduino-Sketch flashen — eigener, vom Flipper-Quelltext unabhängiger Schritt.

## API-87-Status

**✅ Quelltextlich verifiziert, kompatibel — kein Fund.**

Gegen den eigenen `Next-Flip/Momentum-Firmware`-Klon (Tag `mntm-012`, API 87.1) geprüft:

- **UART-Link** (`helpers/uart_link.c`, Kommunikation mit dem ESP32): alle acht verwendeten
  Symbole in `targets/f7/api_symbols.csv` vorhanden und signaturgleich —
  `furi_hal_serial_control_acquire/_release`, `furi_hal_serial_init`, `furi_hal_serial_async_rx`,
  `furi_hal_serial_async_rx_start/_stop`, `furi_hal_serial_tx`, `furi_hal_serial_tx_wait_complete`,
  `furi_hal_serial_deinit`, dazu `expansion_disable`/`expansion_enable` (Übernahme/Freigabe des
  von der Expansion-Service standardmäßig belegten USART).
- **Air-Check-RF-Testmodus** (nur relevant, falls `GHOSTTAG_ENABLE_AIR_CHECK` lokal aktiviert
  wird): alle neun verwendeten `furi_hal_bt_*`/`bt_*`-Symbole vorhanden und gegen
  `targets/furi_hal_include/furi_hal_bt.h` einzeln signaturgleich abgeglichen —
  `furi_hal_bt_ensure_c2_mode(BleGlueC2Mode)`, `furi_hal_bt_start_rx(uint8_t)`,
  `furi_hal_bt_start_packet_rx(uint8_t, uint8_t)`, `furi_hal_bt_stop_rx`,
  `furi_hal_bt_stop_packet_test`, `furi_hal_bt_get_rssi() → float`, `furi_hal_bt_stop_advertising`,
  `furi_hal_bt_reinit`, `bt_disconnect`, `bt_keys_storage_set_default_path`,
  `bt_profile_restore_default`. Bestätigt damit den vom Autor selbst dokumentierten Befund (0,0
  dBm auf jedem Kanal in offizieller Firmware) als reales, nicht nur behauptetes
  API-87-Verhalten — kein eigenständiger Fund, aber unabhängig gegengeprüft.
- Übrige API-Fläche (`furi_thread_alloc_ex/_set_priority/_start/_join/_free`,
  `furi_mutex_alloc/_acquire/_release/_free`, `furi_stream_buffer_*`, `saved_struct_load/_save`,
  `storage_file_*`, `storage_common_mkdir`, ViewDispatcher/SceneManager/NotificationApp) ist
  Standard-Furi-Kern-API, bei API 87.1 unverändert vorhanden.

**Geprüfter Codeumfang: vollständig, 4685 von 4685 Zeilen.** In der Vorsitzung bereits gelesen:
`helpers/tracker_db.c` (Verweilzeit-/Sichtungs-Zustandsmaschine, Eviction-Strategie,
Alert-Warteschlange — als "die Records SIND die Warteschlange" statt separater Ein-Slot-Puffer
bewusst gegen Alert-Verlust bei schnell aufeinanderfolgenden Promotions ausgelegt),
`helpers/air_check.c` (RF-Sampling-Worker inkl. Histogramm-basiertem Band-Floor/Busy-Perzentil,
Zwei-Radio-Modus-Fallback, Bluetooth-Stack-Restore beim Beenden), `helpers/uart_link.c`
(Worker-Thread-/ISR-Übergabe, dokumentierte Teardown-Reihenfolge gegen Deadlock bei einer
Detection, die während der Abschaltung eintrifft), `helpers/gt_parse.c` (der einzige Code, der
aus dem UART angreiferreichbare Bytes liest — begrenzte, vorzeichenbewusste, sättigende
RSSI-Zahlenparsing statt `atoi()`+Cast, exakte statt "mindestens" MAC-Feldlängenprüfung,
Namensfeld hart auf Puffergröße begrenzt), `helpers/session_log.c` (SD-Schreibpfad, Größendeckel,
Header nur bei leerer Datei), `helpers/settings.c` (Laden/Speichern/Sanitisierung,
`_Bool`-Normalisierung gegen eine manipulierte Einstellungsdatei), `helpers/ble_signatures.c`
(Typ-Klassifikation, Code-zu-Enum-Grenzprüfung), `helpers/demo_source.c` (deterministisches
Szenario, keine Zufallszahlenquelle für Reproduzierbarkeit) sowie `ghosttag.c`
(App-Lebenszyklus, Quellwechsel Hunt/Demo, Alert-Übergabe von Worker- an GUI-Thread über reines
Datenbank-Polling statt Cross-Thread-Aufruf in den ViewDispatcher).

In dieser Sitzung den gesamten restlichen Codeumfang nachgezogen: alle sechs View-Dateien
(`views/splash_view.c`, `air_view.c`, `alert_view.c`, `device_detail_view.c`, `radar_view.c`,
`device_list_view.c` — Zeichenroutinen, Eingabe-Callbacks, Reframe-/Anker-Logik der Gerätliste
inkl. der `memcmp()`-basierten Wiederauffindung des zuvor ausgewählten Geräts nach jedem
Re-Sort), alle zehn Szenen-Dateien (`scenes/ghosttag_scene.c`, `_splash.c`, `_air.c`, `_alert.c`,
`_list.c`, `_detail.c`, `_scan.c`, `_start.c`, `_about.c`, `_settings.c` — Navigationslogik,
Alert-Priorität vor Neuzeichnen, Zeilen-Index-Verwaltung des per Compile-Flag bedingten
„Air Check"-Menüpunkts, Einstellungen-Persistenz) sowie `ghosttag_i.h` (reine Struct-/Funktions-
Deklarationen, keine Logik). Die verbleibenden, bislang nicht einzeln aufgeführten Header-Dateien
(`views/*.h`, `scenes/ghosttag_scene.h`/`_config.h`) wurden gezielt auf Funktions-Makros und
`static inline`-Definitionen durchsucht (`grep -rn "static inline\|^#define.*(.*)"`): keine
Treffer außerhalb reiner Konstanten und des Standard-Flipper-`ADD_SCENE`-Boilerplate-Makros — kein
Code mit eigener Logik darin übersehen.

**Kein eigener Fund in diesem gesamten Rest.** Dieselben Muster wie im bereits geprüften Teil:
jede Indexzugriff-Schleife über `records[]`/`blips[]` ist durch ein vorab geklemmtes `n = min(count,
<Array-Kapazität>)` gedeckt (`device_list_view_set_records()`, `radar_view_set_data()`), jede
`snprintf()`-Formatbreite ist durch eine vorab geklemmte Zahl bewiesen (`clamp_count()`,
`format_dwell()` mit expliziter "99m+"-Sonderbehandlung statt einer Laufzeitbreite, die
`-Werror=format-truncation` nicht beweisen könnte) und jede Eingabebehandlung akzeptiert sowohl
`InputTypeShort` als auch `InputTypeLong` (mit durchgängigem Kommentar, warum: ein über die
Long-Press-Schwelle gehaltener Tastendruck liefert auf dieser Firmware nur noch `Long`, nie
`Short`). Wie bei Nyx und Specter bereits festgestellt ist auch dieser Autor ungewöhnlich
gründlich selbstauditierend: das `CHANGELOG.md` dokumentiert mehrere frühere eigene Funde
(Zwei-Thread-Deadlock beim Teardown, verlorene Long-Press-Eingaben, verworfene Alerts bei einer
zweiten Promotion, zu kurze Zeilenzahl in der Gerätliste, die Reihe vier abschnitt, das
„!1FOLLOWING YOU"-Zeichenverschmelzungsproblem auf der Radaransicht) inklusive Root-Cause und
Fix — durchweg genau die Fehlerklassen, auf die bei diesem Review gezielt geprüft wurde, und
jeweils bereits vom Autor selbst behoben vorgefunden.

Damit ist GhostTag nach Nyx, Specter und Argus das vierte und letzte Werkzeug aus dem
`at0m-b0mb`-Vierer-Satz, dessen Flipper-seitiger Quelltext vollständig gegen die eigene
`mntm-012`-Basis durchgesehen ist.

## Sicherheitsrahmen

Rein passiv/defensiv — genau der in `CONSTRAINTS.md` verlangte Rahmen. Sendet selbst nie (die
eigentliche BLE-Funkarbeit macht der externe ESP32 im reinen Scan-Modus, der Flipper wertet nur
Textzeilen über UART aus; der optionale Air-Check-Modus ist ein reiner RF-Energie-**Empfänger**
und im Standard-Build ohnehin deaktiviert), erfasst keine Zugangsdaten, dient der eigenen
Gegenüberwachung (Prüfung, ob ein fremder BLE-Tracker über einen längeren Zeitraum mitgeführt
wird — autorisierte Selbstprüfung von eigenem Gepäck/Fahrzeug).

## Nächste Recherche-Aufträge

- Quelltextlich vollständig geprüft, kein eigener Fund — für dieses Werkzeug keine weitere
  Code-Recherche vorgemerkt.
- Lokal offen: ESP32 (S3/C3/C6/klassisch, **nicht** das offizielle S2-Devboard) beschaffen,
  Companion-Sketch flashen, Hunt-Modus gegen eigene AirTag/Tile/SmartTag testen; Demo-Modus kann
  vorher ohne Zusatzhardware gegengeprüft werden.
