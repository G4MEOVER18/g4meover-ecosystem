# Raspberry-Pi-Deck: automatisiertes Andocken via USB

Backlog-Punkt „Raspberry-Pi-Deck-Stufe" aufgegriffen — baut direkt auf `ideas/companion-tool-recherche.md`
auf: dort fand ich mit **SD Card Mounter** (aaronjamt/flipperzero-sd-mounter) den Flipper-seitigen Baustein,
der die interne SD-Karte per USB-MSC an einen PC/Pi durchreicht, ohne sie physisch zu entnehmen. Offen war
die Pi-Seite: was läuft dort, damit ein andockender Pi die Karte automatisch erkennt, synchronisiert und
sauber wieder freigibt — ohne Monitor/Tastatur am Pi (headless, Pi Zero 2 W als Steckdeck gedacht).

## Fund: pi-usb-automount (fasteddy516)

**Quelle:** github.com/fasteddy516/pi-usb-automount, BSD-2-Clause, aktiv gepflegt (84 Commits, für Pi 4B/5
und Raspberry Pi OS Lite/Debian 12 getestet).

**Was:** Udev-Regel (`pi-usb-automount.rules`) + parametrisierter systemd-Service
(`pi-usb-automount@.service`) für headless-Systeme ganz ohne Desktop-Umgebung. Beim Einstecken eines
USB-Massenspeichers mountet der Service ihn automatisch unter `/media/usb0` (bzw. `usb1`, `usb2`, … bei
mehreren Geräten), beim Abziehen wird sauber ausgehängt. Installation als fertiges `.deb`-Paket
(`dpkg -i pi-usb-automount.deb`). Pro Dateisystemtyp (`vfat`, `ntfs`, …) lassen sich in
`/etc/pi-usb-automount.d/` optionale Mount-Parameter-Skripte hinterlegen — die SD-Karte des Flipper ist
FAT32, dafür reicht die Standardkonfiguration.

**Nutzen für mich:** Genau das fehlende Gegenstück zu SD Card Mounter — ich muss den Pi nicht mehr manuell
bedienen (kein SSH-Login/Kommando nötig), sobald ich den Flipper im Mount-Modus per USB an den Pi
anschließe, erscheint die Karte automatisch unter `/media/usb0`. Grundlage für den geplanten
`Vault`/`Capture-Dedup-Indexer`-Workflow (`ideas/pentest-app-katalog.md`) sowie eine mögliche spätere
Feldnutzung (Captures im Rucksack automatisch sichern, ohne Laptop).

**Aufwand:** Sehr klein — ein `.deb`-Paket, keine eigene Firmware/App-Änderung. Für die Flipper-Seite ist
mit SD Card Mounter bereits alles vorhanden (siehe companion-tool-recherche.md).

## Fehlendes Bindeglied: Sync-Auslöser nach dem Mount

pi-usb-automount kümmert sich laut Doku nur ums Mounten/Unmounten selbst — es bietet **keinen** Hook, der
nach dem Mount automatisch ein eigenes Skript (z. B. rsync) anstößt; die Dateisystem-Konfigurationsdateien
in `/etc/pi-usb-automount.d/` sind nur für Mount-**Parameter**, nicht für Befehle nach dem Mount gedacht.
Diese Lücke lässt sich mit einer eigenen, kleinen `systemd.path`-Unit schließen, die auf das Erscheinen von
`/media/usb0` wartet und dann einen `oneshot`-Sync-Service triggert (gleiches Muster wie die verbreiteten
udev+rsync-Backup-Skripte, z. B. das öffentliche `usb-auto-backup`-Gist von mqu):

```
# /etc/systemd/system/g4meover-flipper-sync.path
[Unit]
Description=Watch for mounted Flipper SD card

[Path]
PathExists=/media/usb0
Unit=g4meover-flipper-sync.service

[Install]
WantedBy=multi-user.target
```

```
# /etc/systemd/system/g4meover-flipper-sync.service
[Unit]
Description=Sync new Flipper captures from mounted SD card

[Service]
Type=oneshot
ExecStart=/usr/local/bin/g4meover-flipper-sync.sh
```

Der Sync-Skript-Entwurf (`/usr/local/bin/g4meover-flipper-sync.sh`):

- Nur **kopieren**, nie `rsync --delete` — die Karte bleibt bei jedem Lauf unangetastet, nichts geht
  verloren, wenn das Skript abbricht.
- Nur bekannte Capture-Endungen (`*.sub *.nfc *.mag *.wgn *.ir`) einsammeln, sonst nichts von der Karte
  anfassen (keine `.dfu`/Firmware-Dateien, kein Ausführen irgendeiner Datei von der Karte — wichtig, siehe
  Sicherheitsrahmen unten).
- Nach erfolgreichem Sync eine `SYNCED_<timestamp>.marker`-Datei **auf** `/media/usb0` schreiben — reines
  Signal für mich am Flipper-Display (Dateiliste), dass der Pi fertig ist, bevor ich am Flipper den
  Mount-Modus beende.
- Anschließend `sync` + `umount /media/usb0` explizit selbst ausführen (nicht nur auf das automatische
  Unmount beim Abziehen verlassen), damit der Schreibcache sicher geleert ist, bevor ich das Kabel ziehe.

## Ablaufsicherheit (Reihenfolge Pi ↔ Flipper)

companion-tool-recherche.md hält bereits fest, dass SD Card Mounter selbst warnt: Unterbrechung während
des Mount-Vorgangs (Firmware-Crash, Stromausfall) kann die Karte beschädigen. Die Pi-Seite verschärft das
Zeitfenster zusätzlich — ich darf den Mount-Modus am Flipper erst beenden, **nachdem** der Pi fertig
synchronisiert und sauber ausgehängt hat, sonst schreibe ich mitten in einen laufenden rsync-Lauf hinein.
Die oben beschriebene `SYNCED_*.marker`-Datei ist dafür die einfachste Absicherung: sie taucht in der
Flipper-eigenen Dateiliste erst auf, wenn der Pi wirklich fertig ist — ich beende den Mount-Modus am
Flipper nie vor diesem Marker.

## Geprüft, aber nicht Teil dieses Bausteins

- **FlipperPi** (DanNAlexander, AGPL-3.0, Python/Flask, Web-UI): eigenständige Flipper-Zero-artige
  Neuimplementierung *auf* dem Pi (HID/SubGHz/NFC über CC1101/MFRC522-Module, browsergesteuert) — kein
  Andock-Companion für einen echten Flipper, sondern ein Ersatzgerät. Anderer Bedarf, deckt mein
  Andock-Szenario nicht ab.
- **Flip-pi / „Pi Terminal for Flipper Zero"** (InfoSecREDD, MIT, Flipper-seitige externe App): UART-
  Terminal-App fürs Flipper-Display, um seriell mit einem angeschlossenen Gerät (z. B. Pi über GPIO-UART)
  zu sprechen. Interessanter separater Baustein für einen künftigen **Kommandokanal** Pi→Flipper (Skript-
  gesteuerte Aktionen auslösen, ohne Tasten zu drücken) — aber ein anderes Problem als die hier behandelte
  Datei-Synchronisation über USB-MSC. Für eine spätere Sitzung vorgemerkt, hier zurückgestellt.

## API-87-Status / Kompatibilitätsrahmen

pi-usb-automount und das Sync-Skript laufen komplett auf der Pi-Seite (Linux/systemd) — keine
Flipper-Firmware- oder API-Abhängigkeit. Die einzige Flipper-seitige Abhängigkeit ist SD Card Mounter,
deren API-87-Einschätzung (❓→🔧 wahrscheinlich lauffähig, geringes Risiko) bereits in
`ideas/companion-tool-recherche.md` dokumentiert ist.

## Sicherheitsrahmen

Reines Storage-/Sync-Tooling auf meinem eigenen Gerätepaar (Flipper + eigener Pi), kein Angriffscode.
Wichtige Leitplanke für das Sync-Skript: es darf **niemals** Dateien von der Karte ausführen oder
interpretieren (kein Auto-Run von `.js`/Skripten, die theoretisch auf der Karte liegen könnten) — reine
Byte-Kopie anhand der Dateiendung. Damit bleibt der Pi ein passiver Sync-Punkt und keine Angriffsfläche,
falls jemals eine manipulierte Datei auf der Karte landen sollte.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

pi-usb-automount auf einem Pi Zero 2 W installieren (`.deb`), die beiden systemd-Units oben anlegen und
`enable --now` starten, Sync-Skript mit ein paar selbst erzeugten Test-Dateien (keine echten Captures)
auf einer Test-SD-Karte durchspielen: Flipper im Mount-Modus an den Pi stecken → Auto-Mount prüfen →
Sync-Log/Marker-Datei prüfen → sauberes Unmount prüfen → erst dann Mount-Modus am Flipper beenden. Danach
Anbindung an den geplanten `Vault`/`Capture-Dedup-Indexer`-Workflow.
