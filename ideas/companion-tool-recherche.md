# Companion-Tool-Recherche: PC-Helfer für Flipper-Apps

Backlog-Punkt „Companion-Tool-Ideen: PC-Helfer für Flipper-Apps, danach Raspberry-Pi-Deck" aufgegriffen.
Meine geplanten PC-Companion-Tools (`Vault` als zentraler Capture-Manager, `Capture-Dedup-Indexer`,
siehe `ideas/pentest-app-katalog.md` → „Eigene Kreationen") brauchen als Grundlage einen schnellen,
zuverlässigen Weg, die SD-Karte des Flipper vom PC aus zu lesen/schreiben — bisher ziehe ich dafür die
Karte manuell. Recherche nach existierenden Bausteinen, die genau das lösen, statt das selbst zu bauen.

## Fund: SD Card Mounter (aaronjamt/flipperzero-sd-mounter)

**Quelle:** github.com/aaronjamt/flipperzero-sd-mounter, GPL-3.0, Flipper-seitige externe App (kein PC-Tool).

**Was:** Externe App (`fap_category="USB"`), die den internen microSD-Reader des Flipper direkt als
USB-Mass-Storage-Gerät an den PC durchreicht — die Karte erscheint als normales Laufwerk, ohne sie
physisch zu entnehmen. Ablauf laut Quelle: Storage-Thread pausieren, SD unmounten
(`storage_sd_unmount`), USB-MSC über `furi_hal_usb_unlock` aktivieren, warten bis der PC das Gerät
auswirft, danach `storage_sd_mount` + Storage-Thread fortsetzen. Vor dem Beenden verlangt die App das
erneute Einlegen der Original-Karte zur Integritätsprüfung.

**Nutzen für mich:** Genau der fehlende Baustein für die geplanten PC-Companion-Tools — `Vault`/
`Capture-Dedup-Indexer` könnten dann per Skript/Cron direkt gegen die gemountete Karte laufen
(neue `.sub`/`.nfc`/`.mag`/`.wgn`-Dateien seit letztem Sync erkennen, entdoppeln, taggen), statt dass
ich die Karte jedes Mal manuell ausbaue. Späterer Raspberry-Pi-Deck-Schritt (Backlog) profitiert
genauso: Pi kann den Flipper per USB andocken und automatisiert synchronisieren.

**Aufwand:** Klein — 5 Quelldateien (`sd_mounter.c/.h`, `sd_raw.h`, `ui.c/.h`) + Icon, keine externen
Libs/Submodule, `application.fam` ohne `fap_api_version`-Constraint. Reiner Übernahme-Port nach
`applications_user/`, kein eigener Logik-Patch nötig (im Gegensatz zu Wiegand/Access-Audit ist hier
kein größerer Port-Vorschlag mit Risikoanalyse notwendig — daher hier als Ideen-Eintrag statt in
`firmware-mining/`, bei Bedarf in künftiger Sitzung zum vollen Port-Vorschlag ausbauen).

**API-87-Status:** ❓→🔧 wahrscheinlich lauffähig. Genutzte APIs sind `storage_sd_mount`/
`storage_sd_unmount`/`storage_sd_status` (in offizieller `storage.h` bestätigt vorhanden, stabile
Core-API seit Jahren unverändert) sowie `furi_hal_usb_unlock`/`furi_hal_sd_is_present` (stabile
`furi_hal_usb`-Kernfunktion, u. a. auch von BadUSB/U2F-Apps genutzt — Header-Pfad in dieser Sitzung
nicht einzeln gegen mntm-012 verifiziert, vor dem lokalen Build kurz gegenprüfen). Kein API-Versions-
Constraint im Manifest, keine Fremd-Libs — geringes Kompatibilitätsrisiko.

**Sicherheitsrahmen:** Reines Storage-/USB-Tooling, kein Angriffscode, arbeitet ausschließlich auf der
eigenen SD-Karte des eigenen Geräts — passt uneingeschränkt in den defensiven Rahmen.

**Risiko (aus README):** Autor warnt ausdrücklich, dass die App den SD-Treiber der Firmware
„manipuliert" — bei Unterbrechung während des Mount-Vorgangs (Firmware-Absturz, Stromausfall) ist
Datenverlust/-korruption auf der Karte möglich. Backup der Karte vor dem ersten Test empfohlen.
Transferrate ist hardwarebedingt langsam (~300 kB/s lesen, ~150 kB/s schreiben) — für einzelne neue
Capture-Dateien seit letztem Sync unkritisch, für Voll-Backups der ganzen Karte spürbar langsamer als
Kartenleser/-Adapter.

## Geprüft, aber zurückgestellt: flippy (elijah629/flippy)

**Quelle:** github.com/elijah629/flippy, MIT, CLI-Tool (Rust) für Firmware-/Repo-Konfigurationsverwaltung
über Projektdateien (git-freundlich, deklarativ).

**Warum zurückgestellt:** Zielt auf Firmware-Source-/Build-Konfiguration (welche Firmware-Variante,
welche App-Repos), nicht auf Capture-/Datei-Synchronisation von der SD-Karte — deckt einen anderen
Bedarf als die geplanten `Vault`/`Capture-Dedup-Indexer`-Tools ab. Zusätzlich früh im Stadium (laut
Repo-Metadaten wenige Commits, kein automatisiertes Klonen/Fetchen implementiert, nur Validierung/
Planung). Für spätere Firmware-Variantenverwaltung (z. B. paralleles Testen mehrerer Momentum-Branches)
als Konzept im Hinterkopf behalten, aktuell kein Beitrag zum PC-Helfer-Bedarf.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

SD-Mounter-App lokal nach `applications_user/` übernehmen, `ufbt launch_app` bauen, am eigenen Gerät
mit einer Testkarte (nicht der Produktivkarte) den vollständigen Mount/Unmount-Zyklus prüfen, bevor er
in den `Vault`/`Capture-Dedup-Indexer`-Workflow eingebaut wird.
