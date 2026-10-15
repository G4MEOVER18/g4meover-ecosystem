# PINGEQUA RF Lab — 2,4-GHz-Spektrumanalysator + nRF24-Signalgenerator (PINGEQUA 2-in-1-Board)

## Quelle

`github.com/pingequalab/rf-lab`, MIT-Lizenz (`LICENSE`-Datei im Repo verifiziert, Copyright
PINGEQUA 2026). Aktiv gepflegtes Hersteller-FAP zum eigenen `PINGEQUA Flipper Zero
NRF24+CC1101 2-in-1 RF Devboard` (nRF24L01+ + CC1101 auf gemeinsamem, softwaregeschaltetem
SPI-Bus). Frisch, unabhängig geklont (`git clone --depth 20`), `HEAD` auf `6275e03`, aktuellster
Release-Tag `v0.5.4` (`application.fam`: `fap_version=(0, 5, 4)`), `dist/v0.5.4/` enthält das
ausgelieferte Binary. Gesamter Quellbestand vollständig gelesen: `pingequa_app.c` plus 29
Dateien über `core/`/`scenes/`/`views/` (4770 Zeilen) — Chip-Arbiter (gemeinsamer SPI-Bus,
CS-Mutex), nRF24-Register-Layer, CC1101-Minimaltreiber, Board-Erkennung, Scanner-/Jammer-Szenen
und -Views, Settings-/Log-Persistenz, About-Screen.

## Was

Zwei Hauptfunktionen über das PINGEQUA-2-in-1-Board:

- **2,4-GHz-Kanal-Scanner**: 126 nRF24-Kanäle (2400–2525 MHz) in Echtzeit, RPD-Träger­erkennung
  je Kanal, Max-Hold-Modus (friert das Bild ein, sobald ein Balken sättigt), WiFi-1/6/11- und
  BLE-37/38/39-Frequenzmarker, Cursor mit Live-Frequenzanzeige, einstellbare Dwell-Zeit
  (130–2000 µs), CSV-Export aller 126 Kanäle per Lang-Druck OK.
- **nRF24-Signalgenerator** (bis `v0.5.2` als "Jammer" benannt, seit `v0.5.3` rein cosmetic als
  "Signal Generator" umbenannt — Code-Identifier unverändert `jammer_*`/`JammerMode*`): sieben
  Modi (CW Custom, BLE Adv, BLE React [RPD-reaktiv], WiFi 1/6/11, ALL 2.4G), Sitzungs-Log und
  Mode-/Kanal-Persistenz (`jammer.conf`).
- Drittens ein dreiseitiger About-Screen (Marke+QR, GitHub+QR, Legal-Hinweis).

Für die Board-Erkennung liest die App beim Start Referenzregister beider Chips (CC1101
PARTNUM/VERSION, nRF24 `SETUP_AW`-Rückschreibtest) und bricht bei Nichterkennung sauber in eine
eigene Error-Szene ab, statt den Bus unkontrolliert anzufassen.

## Nutzen

Erste eigene 2,4-GHz-Spektrum-Recon-Kategorie: die bestehenden nRF24-Einträge
(`ideas/nrf24-sniffer-mousejack-api-status.md`) decken Sniffing/Mousejack/Kanal-Scan auf
Protokollebene ab, aber keinen visuellen Breitband-Spektrumüberblick analog zum bereits
vorhandenen `Frequency Analyzer+` im Sub-GHz-Bereich — diese Lücke schließt RF Lab für 2,4 GHz
(WiFi/BLE-Band). Der Signalgenerator-Teil ergänzt den bereits katalogisierten
`nRF24 Jammer (W0rthlessS0ul)` um einen zweiten, unabhängig implementierten CW-Pfad plus den
reaktiven BLE-Modus, den die andere App nicht hat — nützlich zur eigenen TSCM-Gegenprobe (eigene
Jammer-Detektoren wie `ideas/subghz-jammer-detector-app-status.md` sind sub-GHz-fokussiert,
ein 2,4-GHz-Gegenstück zum Testen fehlt bislang).

Besonderheit gegenüber den bereits katalogisierten nRF24-Apps: Letztere laufen auf gewöhnlichen
nRF24L01+-Modulen über den Standard-Pinsatz. Das PINGEQUA-Board verkabelt beide Chips stattdessen
auf einem geteilten SPI-Bus mit Nicht-Standard-Pins (nRF24 CSN auf PC3, CE auf PB2, CC1101 CSN
auf PA4, gemeinsame MISO/MOSI/SCK auf PA6/PA7/PB3) — RF Labs eigenes README dokumentiert dafür
explizit den nötigen Umschalt-Schritt für die schon vorhandenen Dritt-Apps
(`Settings → Protocols → GPIO Pins → NRF24 SPI → Extra 7`), sodass ein einziges Board sowohl
dieses FAP als auch die bereits katalogisierten Mousejack-/Sniffer-Apps bedienen kann, ohne
Neuverkabelung.

## Aufwand

Gering, reine externe `.fap` (44 KB), keine Firmware-Änderung nötig (bis auf die unten
beschriebene kosmetische Korrektur, die ausschließlich im Fremdprojekt selbst liegt). Zusätzliche
Hardware zwingend: das PINGEQUA-2-in-1-Board (nRF24L01+ + CC1101, Erweiterungsport). DIY-Nachbau
mit Einzelmodulen laut eigener Doku (`docs/HARDWARE.md`) möglich, aber nicht offiziell
unterstützt.

## API-87-Status

✅ Kompatibel, geprüft. Frischer, blobloser Sparse-Checkout von `G4MEOVER18/G4MEOVER-FW`
(Tag `v2.0.0`, Commit `b359f6f` — derselbe mntm-012-Stand, den vorherige Sitzungen bereits
referenzieren) gegen den eigenen Header-/Implementierungs-Stand abgeglichen:

- GPIO (`targets/f7/furi_hal/furi_hal_resources.c`): `gpio_ext_pa4`, `gpio_ext_pb2`,
  `gpio_ext_pc3` (alle drei vom Chip-Arbiter direkt per `furi_hal_gpio_write()`/`_init()`
  angesteuert) byteidentisch vorhanden.
- SPI-HAL (`targets/furi_hal_include/furi_hal_spi.h`, `.../furi_hal_spi_config.c`):
  `furi_hal_spi_bus_handle_external` sowie alle sechs vom Chip-Arbiter verwendeten Funktionen
  (`furi_hal_spi_bus_handle_init/_deinit`, `_acquire`/`_release`, `furi_hal_spi_bus_trx`)
  identisch vorhanden — dasselbe Muster, das bereits bei SPI Flash Dump/SWD Probe/i2c Tools/CAN
  Transceiver gegen denselben Firmware-Stand verifiziert wurde.
- `pq_chip_arbiter.c`s eigener Kommentarkopf dokumentiert bereits einen selbst behobenen
  API-Kompatibilitätsfund aus `v0.2.1`: ein zunächst genutztes
  `furi_hal_spi_bus_handle_external_extra`-Handle war "in der aktuellen Firmware-API-Tabelle
  nicht exportiert" — seit `v0.2.2` ausschließlich über das öffentliche
  `furi_hal_spi_bus_handle_external` plus manuell verwaltete CS-Pins ersetzt, exakt das Muster,
  das jetzt auch gegen meinen `mntm-012`/`v2.0.0`-Stand verifiziert bestätigt vorliegt.
- `application.fam`: `requires=["gui", "storage", "notification"]`, `fap_category="GPIO"` — alle
  drei Dienste standardmäßig vorhanden, keine ungewöhnliche Abhängigkeit.
- Firmware-Kompatibilitätstabelle der App selbst (`README.md`/`docs/HARDWARE.md`) stuft Momentum/
  Unleashed/RogueMaster als "getestet" und Official-Firmware (API ≥ 87.1)/Xtreme als "kompatibel
  nach API-Fläche" ein — deckt sich mit der hier unabhängig durchgeführten Symbolprüfung.

## Fund: About-Screen zeigt veraltete Versionsnummer (v0.5.3 statt v0.5.4)

`views/about_view.c::draw_page_brand()` zeichnet die App-Version als festen Literal-String:

```c
canvas_draw_str(canvas, 2, 24, "v0.5.3");
```

Per `git log -- views/about_view.c` nachvollzogen: diese Zeile stammt aus dem
`v0.5.3`-Release-Commit (`16e6752`, "rebrand transmit feature 'Jammer' → 'Signal Generator'") und
wurde seitdem nicht mehr angefasst. Der nachfolgende `v0.5.4`-Release-Commit (`312e6a0`,
"Signal Generator actually radiates + honest power wording") hat `application.fam`s
`fap_version` auf `(0, 5, 4)` angehoben und laut `CHANGELOG.md` einen echten Funktionsfix
(Bus-Arbitrierung des Signalgenerators) sowie README-Textänderungen vorgenommen — die
On-Device-Versionsanzeige aber nicht mit aktualisiert. Per `grep` über den gesamten Quellbestand
bestätigt: `"v0.5.3"` in `about_view.c:96` ist die einzige Versions-String-Stelle im ganzen
Projekt; keine andere Datei trägt eine Versionsnummer, die stattdessen hätte synchron gehalten
werden können.

Gegen das tatsächlich ausgelieferte Release-Binary verifiziert: `strings
dist/v0.5.4/pingequa_rf_toolkit.fap | grep -i "v0.5"` liefert ausschließlich `v0.5.3` — die
aktuelle, zum Zeitpunkt dieser Prüfung neueste veröffentlichte `.fap`-Datei zeigt auf dem Gerät
also eine Versionsnummer, die bereits eine Veröffentlichung hinter der tatsächlich installierten
liegt. Reiner Informationsfund (irreführende Selbstauskunft beim Support/Debugging, z. B. wenn
ein Nutzer anhand des About-Screens meldet, welche Version er ausführt), kein
Funktions-/Sicherheitsrisiko — Scanner und Signalgenerator selbst sind von diesem String
vollständig unberührt.

## Fix-Skizze (additiv, eine Zeile)

```diff
-    canvas_draw_str(canvas, 2, 24, "v0.5.3");
+    canvas_draw_str(canvas, 2, 24, "v0.5.4");
```

Patch in `ideas/pingequa-rf-lab-stale-about-version-fix/about_view_c.patch`, gegen zwei frische,
unabhängige Zweitkopien von `pingequalab/rf-lab` mit `git apply --check` verifiziert, real
angewendet, beide Endstände byte-für-byte identisch. Kein API-/Verhaltensrisiko: reiner
Textstring in einem bereits vorhandenen `canvas_draw_str()`-Aufruf, keine neue Codepfad-/
Speicherzugriffsänderung.

## Testhinweis (lokal nachvollzogen)

Da es sich um eine reine Textstring-Korrektur in einer Canvas-Zeichenfunktion handelt (kein
Zustands-/Speicherfehler), ist ein Host-Repro hier nicht aussagekräftig — der Fund wurde
stattdessen direkt am bereits vorliegenden, offiziell veröffentlichten Artefakt verifiziert:

- `strings dist/v0.5.4/pingequa_rf_toolkit.fap | grep -i "v0.5"` → `v0.5.3` (Fund, vor dem Fix).
- Patch angewendet, `grep -n "v0.5" views/about_view.c` → `v0.5.4` (nach dem Fix) in beiden
  unabhängigen Zweitkopien übereinstimmend.

Lokal zu tun: Patch gegen die eigene `views/about_view.c` anwenden, mit `ufbt launch` neu bauen.
Funktionstest: `Apps → GPIO → PINGEQUA RF Lab → About` (dritter Menüpunkt), erste Seite zeigt
jetzt `v0.5.4` statt `v0.5.3` — alle anderen Funktionen (Scanner, Signalgenerator, CSV-/Log-Export,
Settings-Persistenz) unverändert, da der Patch ausschließlich diese eine Zeichenkette betrifft.

## Sicherheitsrahmen

Der Scanner-Teil ist rein passiv/listen-only (liest nur das nRF24-RPD-Bit, sendet nichts). Der
Signalgenerator-Teil sendet aktiv auf dem regulierten 2,4-GHz-ISM-Band — die App selbst hält die
Sendeleistung laut eigenem README/Legal-Hinweis bewusst unter dem elektrischen Maximum des Chips,
um "innerhalb eines vertretbaren Rahmens für das regulierte 2,4-GHz-Band" zu bleiben, und weist
im eigenen About-Screen explizit auf "Authorized testing only. Mind local RF laws." hin. Nutzung
hier ausschließlich zur eigenen TSCM-Gegenprobe und Spektrum-Diagnose an eigener Hardware/eigenem
Netz, passt in den defensiven Rahmen aus `CONSTRAINTS.md` Punkt 5 — exakt dieselbe Einordnung wie
beim bereits katalogisierten `nRF24 Jammer (W0rthlessS0ul)`. Der hier gefundene Fix ist ein reines
Informations-/Kosmetikproblem (Versionsanzeige), kein Sicherheits- oder Missbrauchsrisiko.
