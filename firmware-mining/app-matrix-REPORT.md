# App-Matrix-Report — G4MEOVER-FW-v2

Stand: 2026-11-18 · reine Recherche/Inventar, keine Firmware-Änderungen.

## Datenbasis

- Firmware-Baum: `C:\Data\KI\github\flipper\G4MEOVER-FW-v2`
- Eigene Apps: `applications_user/*` (10 Ordner)
- Externe Apps: `applications/external/*` — Git-Submodul von `Next-Flip/Momentum-Apps`, Snapshot-Commit `85b0cdb` (Stand ca. August 2026)
- Upstream-Abgleich: `Next-Flip/Momentum-Apps` Branch `dev` (Live-Stand 2026-11-18)
- Insgesamt inventarisiert: 244 Apps mit `application.fam` (234 extern + 10 eigen).
- Vollständige Tabelle: `app-matrix.csv` (253 Zeilen inkl. Lücken-Kandidaten).

Verteilung nach Kategorie (Top): Games 41, Tools 31, GPIO 25, Sub-GHz 23, NFC 18, Media 15, GPIO/ESP 11, Infrared 11, GPIO/FlipperHTTP 11, RFID 5, iButton 2, Bluetooth 3.

## Eigene Apps des Betreibers

Alle vom Betreiber selbst gepflegten Apps sind im Baum aktuell und deckungsgleich mit den GitHub-Repos (keine neueren Stände offen):

- `rollforge` RollForge 1.0 — Repo G4MEOVER18/RollForge, synchron
- `rolljam_research` RollJam Research 1.0 — Repo G4MEOVER18/RollJam, synchron
- `rolllab_research` RollLab Research 1.0 — Repo G4MEOVER18/RollLab, synchron
- `proto_pirate` ProtoPirate 3.1 — Repo G4MEOVER18/ProtoPirate, synchron (zusätzlich AM-/FM-Plugins)
- `jammer_suite` Jammer Suite 1.0 — Repo G4MEOVER18/FlipperJammer, synchron
- `lora_ukfe` USB Army Knife FE 1.0 — Repo G4MEOVER18/lora-ukfe, synchron (im Standalone-Repo heißt die App `G4MEOVER RF`, gleiche Version)

Hinweis: `proto_pirate` ist inzwischen auch im Momentum-Katalog vorhanden — vor Doppelpflege prüfen, welcher Strang führt.

Drei Ordner in `applications_user` sind **Dritt-Apps**, nicht vom Betreiber geschrieben, nur eingebettet — Upstream getrennt pflegen:

- `flipdeflock` FlipDeFlock 0.53 — Autor @ReconGrunt
- `wendigo` [ESP32] Wendigo 0.5 — Autor Chris BC
- `fz_nrf24_jammer` [NRF24] Jammer 1.4.0 — Autor W0rthlessS0ul

## Top-10 Upgrade-Kandidaten (veraltet gegen Momentum dev)

1. **seader** Seader 3.4 -> 4.1 (NFC) — größter Versionssprung, HID/SEOS-Lesestack, hoch
2. **metroflip** Metroflip 0.9 -> 1.1 (NFC) — Transit-Karten-Parser, viele neue Formate, hoch
3. **esp_flasher** [ESP] ESP Flasher 1.7 -> 1.9 (GPIO/ESP) — Basis zum Flashen von Marauder/Ghost ESP, hoch
4. **ghost_esp** [ESP32] Ghost ESP 1.6.3 -> 1.7.0 (GPIO/ESP) — populäre WiFi/BLE-Suite, hoch
5. **wifi_marauder_companion** [ESP32] WiFi Marauder 7.6 -> 7.8 (GPIO/ESP) — Kern-Recon-Tool, hoch
6. **mfkey** MFKey 4.0 -> 4.1 (NFC) — MIFARE-Classic-Key-Recovery, mittel
7. **picopass** HID iClass (PicoPass) 1.21 -> 1.22 (NFC) — iCLASS/PicoPass, mittel
8. **nfc_magic** NFC Magic 1.14 -> 1.15 (NFC) — Magic-/Gen-Karten schreiben, mittel
9. **seos** Seos Compatible 1.2 -> 1.3 (NFC) — SEOS-Zutrittskarten, mittel
10. **passy** Passport Reader 1.4 -> 1.5 (NFC) — eMRTD/Reisepass, niedrig

Weiterer Treffer knapp darunter: `weebo` 0.4 -> 0.5 (NFC, niedrig). Alle anderen stichprobenartig geprüften prominenten Apps sind aktuell: `protoview` 1.2, `ble_spam` 6.6, `totp` 5.190, `flipbip` 1.18, `mfc_editor` 1.0, `nfc_apdu_runner` 0.3, `mayhem_marauder` 7.2, `wardriver` 1.0, `nrf24mousejacker` 1.2, `multi_fuzzer` 1.7, `ibutton_converter` 1.1, `t5577_multiwriter` 0.2, `saflip` 0.1, `nfc_maker` 2.1.

## Top-Neuzugänge (hochwertige legitime Pentest-/Recon-FAPs, noch nicht im Baum)

Der Momentum-Katalog ist bereits weitgehend ausgeschöpft; der Baum ist sehr vollständig (inkl. spi_mem_manager, i2ctools, swd_probe, dap_link, uart_terminal, signal_generator, oscilloscope, avr_isp, flipper_blackhat). Die folgenden Kandidaten sind sicher belegt und fehlen konkret:

Neu im Momentum-Katalog seit dem Submodul-Snapshot (gleiche Quelle, leicht zu übernehmen):

1. **ulc_brute** — NFC — Bruteforce des 3DES-Schlüssels von MIFARE Ultralight-C — Momentum-Apps/ulc_brute — hoch
2. **ulc_relay** — NFC — Relay-Angriff gegen Ultralight-C-Auth (Research/PoC) — Momentum-Apps/ulc_relay — hoch
3. **ulcfkey** — NFC — abgeleitete Ultralight-C-Schlüssel verwalten/prüfen — Momentum-Apps/ulcfkey — mittel
4. **iso15693_nfc_writer** — NFC — beschreibt ISO15693-Vicinity-Tags (NFC-V, 13,56 MHz), schließt eine Lücke — Momentum-Apps/iso15693_nfc_writer — hoch
5. **can_commander** — GPIO — interaktiver CAN-Bus (Automotive), ergänzt vorhandenes `can_tools` — Momentum-Apps/can_commander — mittel
6. **flipper_wedge** — Tools — sendet Text als USB-HID-Tastenanschläge (Wedge) — Momentum-Apps/flipper_wedge — niedrig

Aus etablierten externen Repos (separate Pflege nötig, Kompatibilität prüfen):

7. **flipper_nested (MFC Nested)** — NFC — Nested-, Static-Nested- und Hard-Nested-Schlüsselrückgewinnung; starke Ergänzung zu `mfkey` — github.com/AloneLiberty/FlipperNested — hoch — ACHTUNG: hängt an der NFC-Stack-/Firmware-Version, vor Integration prüfen
8. **flipper_kdf (KDF-Plugins)** — NFC — Community-KDF-`.fal`-Plugins für sofortige Schlüsselableitung bei vielen Marken (z. B. Saflok); nach `/apps_data/nfc/plugins/` — github.com/noproto/flipper_kdf — hoch
9. **logic_analyzer** — GPIO/Debug — Mehrkanal-Digitalmitschnitt, Export nach sigrok/PulseView für HW-/Protokoll-Recon — github.com/ElectronicCats/flipper-LogicAnalyzer — mittel

Bewusste Begrenzung: statt die Liste künstlich auf 15 aufzufüllen, sind hier nur sicher verifizierte Einträge geführt. Weitere Quellen für Nachschub: offizieller Katalog `lab.flipper.net` / `catalog.flipperzero.one`, sowie die Firmware-eigenen Extra-Apps von Unleashed/RogueMaster. EMV-/Bankkarten-Lesen ist bewusst nicht als App gelistet — es steckt bereits im NFC-Stack der Firmware (NFC -> Extra actions -> read EMV card).

## Besonderheiten

- Das externe App-Set stammt 1:1 aus `Next-Flip/Momentum-Apps` (Submodul). Ein Upgrade heißt praktisch: Submodul-Pointer auf einen neueren `dev`-Commit setzen; dabei kommen alle 11 veralteten Apps **und** die 6 neuen Katalog-Apps automatisch mit. Einzelne Cherry-Picks sind nur nötig, wenn der Rest eingefroren bleiben soll.
- `applications/external` ist aus Momentum gepinnt, die Firmware selbst ist ein Momentum-Fork (Rebrand, API-kompatibel). Upstream-Merges berühren das G4MEOVER-Branding nicht direkt, aber Katalog-/Menü-Reihenfolge prüfen.
- Fehlende/leere `fap_version`-Felder (z. B. `evil_portal`, `subghz_bruteforcer`, `uhf_rfid`, viele GPIO-Badges) sind in der Matrix als `unklar` markiert — das ist kein Fehler, diese Apps führen schlicht kein Versionsfeld in der `.fam`.
- 210 Apps wurden nicht einzeln gegen Upstream geprüft (`status=unklar`), da der Fokus auf prominenten Pentest-/Recon-Apps lag. Für einen Vollabgleich genügt der Submodul-Bump statt hunderter Einzelprüfungen.
- Die eigenen Research-Apps (RollForge/RollJam/RollLab/ProtoPirate) tragen niedrige Versionsnummern (1.0/3.1), sind aber inhaltlich aktuell gepflegt (letzte Commits Aug 2026) — die Versionsnummer ist hier kein Reifegrad-Indikator.
