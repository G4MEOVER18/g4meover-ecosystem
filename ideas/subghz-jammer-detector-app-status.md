# SubGHz Jammer Detector — API-87-Status (RogueMaster-Rückstellpunkt aufgegriffen)

## Quelle
- Repo: `barkandbite/flipper_suite` (MIT-Lizenz, eigenständige App-Sammlung, keine RogueMaster-
  Bündel-Kopie — ich hatte den App-Namen aus einer früheren RogueMaster-Bündeldurchsicht
  vorgemerkt, die eigentliche Herkunft ist dieses separate Repo).
- Pfad: `subghz_jammer/` (appid `subghz_jammer`, Name „SubGHz Jammer Detect", `fap_author`
  „PentestSuite", `fap_category` Sub-GHz, `fap_version` 1.0).
- Aktivität: 8 Commits an diesem Pfad, letzter am 2026-05-04 („Add furi_assert after worker
  malloc for OOM crash clarity"); davor mehrere Härtungs-Commits derselben Sitzungsreihe
  („Fix 20+ bugs found during full codebase code review", „Show CC1101 hardware error on
  screen instead of silent failure", NULL-Check-Nachträge) — aktiv gepflegt, mit sichtbarer
  Qualitätsarbeit über die Zeit.

## Was
Eigenständige App, überwacht die vier Standard-SubGHz-Bänder (315/433.92/868.35/915 MHz)
nacheinander (200 ms Dwell je Frequenz, ein voller Zyklus ≈ 800 ms) auf ungewöhnlich starken/
andauernden RSSI-Pegel und klassifiziert je Band in „Ok"/„Suspicious"/„Jammer" (8er-Rolling-
Fenster pro Band, mehrere aufeinanderfolgende erhöhte Zyklen nötig, bevor ein Alarm auslöst —
explizit zur Reduktion von Fehlalarmen). Alarmmodi: still, LED-Blinken, Vibration. Reine
Empfangs-/Anzeige-App, sendet selbst nichts.

## Nutzen für mich
Bislang habe ich ausschließlich **offensive** SubGHz-Jammer (RollForge-Jammer-Modul,
FlipperJammer `subghz_jam.c`) im Katalog — eine erste **defensive** Gegenstück-Kategorie
(Jamming-Erkennung) fehlt mir komplett. Direkter Eigennutzen: Verifikation der eigenen
Jammer-Reichweite/-Wirksamkeit bei autorisierten Tests (Detector auf einem zweiten Gerät als
Referenzmessung), sowie als eigenständiges RF-Lagebild-Werkzeug bei Einsätzen (SubGHz-Umfeld
vor einer Aktion prüfen). Nutzt ausschließlich die eingebaute CC1101-Antenne, keine
Zusatzhardware.

## API-87-Status
🔧 wahrscheinlich kompatibel, lokale Bauprobe empfohlen. Begründung: die App verwendet
durchgehend die SubGHz-Devices-Abstraktionsschicht (`lib/subghz/devices/devices.h`,
`lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h`, `lib/subghz/devices/preset.h`)
statt direkter `furi_hal_subghz_*`-Aufrufe — exakt dieselbe API-Familie
(`subghz_devices_init/get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME)/begin/reset/idle/
load_preset(..., FuriHalSubGhzPresetOok650Async, ...)/set_frequency/set_rx/get_rssi/sleep/end/
deinit`), die ich bei RollJam/RollForge (`subghz_devices_stop_async_tx()` u. Ä., siehe
`app-improvements/RollForge/jammer-freq-switch-double-stop-crash-fix.md`) bereits gegen
mntm-012 als vorhanden/funktionsfähig verifiziert habe. Kein `requires`-Eintrag im
`application.fam` über das Standard-GUI hinaus, `stack_size` 4 KB — unauffällig. Kein
Firmware-Patch nötig (reine externe `.fap`), lokale Bauprobe (`ufbt launch_app`) bleibt aber
nötig, um den exakten mntm-012-API-87.1-Header-Stand zu bestätigen (Quelltext selbst habe ich
nur über GitHub-Ansicht geprüft, nicht Byte-für-Byte gegen meinen lokalen Header-Stand
diffen können).

## Aufwand
Gering — reine externe App, keine Hardware-Modifikation, keine Firmware-Änderung. Nach
Bauprobe direkt nutzbar; bei API-Abweichungen (unwahrscheinlich laut Abstraktionsschicht)
wären das lokal sichtbare Compile-Fehler, keine stillen Fehlfunktionen.

## Sicherheitsrahmen
Rein defensiv (Erkennung, kein Sendebetrieb) — passt uneingeschränkt in den autorisierten
Rahmen aus CONSTRAINTS Punkt 5. Bekannte Grenzen laut eigenem README, die ich für die
Bewertung übernehme: Einzelkanal-Dwell (kann Frequenzhopping-Jammer verpassen), mögliche
Fehlalarme durch starke legitime Sender, kurze Jamming-Bursts unter dem 200-ms-Dwell-Fenster
können durchrutschen, keine SD-Card-Protokollierung (nur Live-Anzeige) — für ein
Lagebild-/Verifikationswerkzeug ausreichend, nicht als forensischer Nachweis geeignet.

## Zurückgestellter Geschwisterfund
Aus demselben Repo/derselben Commit-Reihe (`b8fe671`, „Add 6 new security tools") stammt auch
**Rogue AP Detector** (`rogue_ap_detector/`) — baseline-basierte Erkennung von Evil-Twin-/
Rogue-WiFi-APs (Duplicate-SSID-von-anderer-MAC, RSSI-Anomalie, bis zu 128 AP-Einträge),
ebenfalls defensiv und thematisch verwandt. Anders als der SubGHz-Detector benötigt er
zwingend ein externes ESP32-Devboard mit Marauder-kompatibler Firmware (analog zu meiner
bestehenden WiFi-Kategorie über Heltec/ESP-Satelliten) — für eine künftige Sitzung
zurückgestellt, sobald die Marauder/GhostESP-Integrationsseite dieses Katalogs konkret
ansteht.

## Offen
- Lokale Bauprobe (`ufbt launch_app APPSRC=<app-ordner>`) gegen die eigene mntm-012-Basis.
- Funktionstest: mit einem zweiten eigenen Gerät (z. B. RollForge-Jammer-Modul oder
  FlipperJammer `subghz_jam.c`) auf einem der vier überwachten Bänder senden und prüfen, dass
  die Klassifikation nach einigen Zyklen auf „Jammer" springt; danach Sendung stoppen und
  Rückkehr auf „Ok" verifizieren.
- Rogue AP Detector (siehe oben) sowie `h10301_writer` und der noch nicht durchgesehene
  NFC-Werkzeug-Cluster aus dem laufenden RogueMaster-Bündel-Rückstellpunkt bleiben für
  künftige Sitzungen offen.
