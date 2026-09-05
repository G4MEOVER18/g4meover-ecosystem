# Idee-Status: NFC Magic App (Magic-Karten Gen1a/Gen2/Gen4)

Backlog-Rückstellpunkt "NFC/RFID-Tooling-Deltas (Dictionaries, Parser, Magic-Support) ...
Parser/Magic-Support weiterhin offen" aufgegriffen. Auslöser war zusätzlich die bestehende
Katalogzeile `ideas/pentest-app-katalog.md` ("NFC Magic | Magic-Karten (Gen1a/Gen4) schreiben |
✅ | Klonen") — im Unterschied zu allen anderen Zeilen dort ohne Quellenbeleg, ohne verifizierten
Submodul-Commit und ohne eigenes Detail-Dokument. Diese Sitzung schließt genau diese Lücke: Status
jetzt mit Quelle, Commit-Verifikation und API-87-Nachweis statt einer bloßen Annahme.

## Fund

**Quelle:** App-ID `nfc_magic`, laut `.gitsubtree`-Herkunftsnachweis der Momentum-Apps-Kopie
gepflegt in `flipperdevices/flipperzero-good-faps` (offizielles, von den Flipper-Devs kuratiertes
Zusatz-App-Repo) sowie parallel in `xMasterX/all-the-plugins`; `fap_version = "1.14"`
("Update for new API" laut eigenem `.catalog`-Changelog).

**Bereits Teil meiner Basis-Firmware, kein Port nötig.** Wie bei Subbrute/PicoPass/Seader bindet
Momentum externe Apps über das Submodul `applications/external` → `Next-Flip/Momentum-Apps` ein.
Der bei Tag `mntm-012` gepinnte Submodul-Commit (`85b0cdb4e7dec08d16db6ceea3ec3fc5078bedb4` —
identisch zum bereits für Subbrute/PicoPass/Seader verifizierten Commit) enthält bereits das
vollständige Verzeichnis `nfc_magic/` (14 Quelldateien, 8387 Zeilen). Verifiziert per Sparse-
Checkout dieses exakten Commits aus dem echten `Next-Flip/Momentum-Apps`-Repo (nicht nur aus
Sekundärquellen zitiert). Die App ist unter NFC → Magic bereits auf meinem Gerät vorhanden, sobald
G4MEOVER-FW v2 (Basis mntm-012) geflasht ist — keine SD-Karten-Installation, kein eigener Build
nötig.

**Meine eigene Firmware verweist selbst auf diese App:** `applications/main/nfc/helpers/
protocol_support/mf_classic/mf_classic.c:305` zeigt beim Versuch, eine Mifare-Classic-Karte auf
eine reguläre (nicht-magische) Zielkarte zu schreiben, den Hinweistext `"Use source card!\nTo
write blanks\nuse NFC Magic app"` — die offizielle Haupt-NFC-App delegiert Magic-Card-Writes
also bewusst an genau diese separate App, statt eigene Backdoor-Kommandos mitzubringen. Damit ist
NFC Magic nicht nur ein Community-Nice-to-have, sondern das von meiner eigenen Firmware
vorgesehene Werkzeug für diese Lücke.

## Was die App abdeckt

Laut eigenem `.catalog`-Beschreibungstext (gegen den mitgelieferten Quellcode gegengeprüft):

**Unterstützt:**
- Mifare Classic 1K/4K Gen1A/Gen1B (inkl. OTP-Varianten) — Backdoor-Kommandos (`nfc_poller_trx`
  mit Custom-Frames, gegen `lib/nfc/nfc.h` bei mir 1:1 signaturgleich)
- Mifare Classic 1K/4K Gen2 (DirectWrite/CUID/FUID/UFUID)
- Ultimate Magic Card Gen4 (GTU) — sowohl Ultralight- als auch Classic-Modus, mit wählbarem
  Custom-Passwort, inkl. Shadow-Mode/Direct-Write-to-Block-0-Konfiguration und einer
  "Set default config"-Wiederbelebungsfunktion für Karten, die nach Fehlbedienung nicht mehr
  reagieren

**Nicht unterstützt** (Lücke bleibt, für künftige Sitzung vorgemerkt):
- Mifare Classic 1K/4K Gen3 (APDU-basiert)
- Mifare Classic Magic Gen4 (GDM-Variante, neuer als GTU)
- Supercard (Gen1 und Gen2)
- Gesamte Mifare-Ultralight-Backdoor-Familie (Gen1A/1B inkl. OTP, DirectWrite, EV1-DirectWrite,
  Ultralight-C Gen1/DirectWrite)
- NTAG (alle Versionen), DESFire (alle Versionen), NFC-V/ISO15693-Magic-Karten

Die o. g. Gen3/GDM-Lücke ist die einzige aus der Beschreibung ersichtliche, real fehlende
Abdeckung für die bei mir bereits vorhandene Kartenfamilie (Mifare Classic) — Gen3/GDM-Blanks
sind mittlerweile verbreiteter als die älteren Gen1a/Gen2-Typen. Ob eine neuere Version von
`flipperdevices/flipperzero-good-faps`/`all-the-plugins` das inzwischen nachgerüstet hat, ist noch
nicht geprüft (Momentum pinnt einen festen Commit, ein Update davon wäre ein separater
Momentum-Apps-Submodul-Bump, nicht in meiner Kontrolle).

## API-87-Status

✅ **Verifiziert, kein Kompatibilitätsrisiko.** Neben der bereits genannten Submodul-Commit-
Identität mit den anderen bestätigten Funden gegen die tatsächlichen Header meiner `mntm-012`-
Sparse-Checkout-Kopie abgeglichen:
- `nfc_poller_alloc()`/`nfc_poller_free()`/`nfc_poller_start()`/`nfc_poller_stop()`
  (`magic/nfc_magic_scanner.c`, `magic/protocols/gen4/gen4_poller.c`) — Signaturen identisch zu
  `lib/nfc/nfc_poller.h`.
- `nfc_poller_trx(Nfc*, const BitBuffer*, BitBuffer*, uint32_t)` (`magic/protocols/gen1a/
  gen1a_poller_i.c`, das für die Gen1a-Backdoor-Kommandos kritische Low-Level-API) — Signatur
  identisch zu `lib/nfc/nfc.h:238`, dieselbe Funktion wird auch intern von
  `iso14443_3a_poller_i.c`/`felica_poller_i.c`/`iso15693_3_poller_i.c` in meiner eigenen
  Firmware verwendet.
- Includes (`nfc/nfc.h`, `nfc/nfc_device.h`, `nfc/nfc_poller.h`, `nfc/protocols/mf_classic/
  mf_classic.h`, `nfc/protocols/iso14443_3a/iso14443_3a.h`, `nfc/helpers/iso14443_crc.h`,
  `nfc/helpers/nfc_data_generator.h`) — alle Header an den erwarteten Pfaden in meiner
  `lib/nfc`-Sparse-Checkout-Kopie vorhanden.

Das eigene `.catalog`-Changelog belegt zusätzlich aktive Pflege gegen genau diese API-Drift
("1.14 Update for new API", "1.11 Fixed Mifare Ultralight types with latest API update",
"1.6 Rework with new bit lib API") — die App wird kontinuierlich gegen dieselbe API-Basis
gepflegt, auf der auch Momentum aufsetzt.

## Sicherheitsrahmen

Einsatzzweck: Duplizieren/Backup eigener, autorisierter Mifare-Classic-Zugangsmedien auf ein
beschreibbares Magic-Blank (z. B. Ersatztranspoder für ein eigenes Zutrittssystem, Testkarten für
Access-Audit/Wiegand-Reader-Portvorschläge). Kein Einsatz gegen fremde oder nicht ausdrücklich
freigegebene Karten. Die Gen1a/Gen2-Backdoor-Kommandos wirken ausschließlich gegen die speziell
dafür gefertigten "magischen" Blanko-Karten, nicht gegen reguläre Mifare-Classic-Karten — kein
zusätzliches Missbrauchspotential gegenüber der ohnehin vorhandenen Standard-Emulation. Kein Bezug
zu Massenangriffen oder Detection-Evasion.

## Verhältnis zum offenen Backlog-Punkt

Löst den "Magic-Support"-Teil von "NFC/RFID-Tooling-Deltas (Dictionaries, Parser,
Magic-Support)" auf: kein Portaufwand, bereits vollständig vorhanden und von der eigenen
Haupt-NFC-App als vorgesehenes Werkzeug referenziert. Offen bleibt aus demselben Backlog-Punkt
weiterhin der "Parser"-Teilaspekt (unklar spezifiziert, in einer künftigen Sitzung zu
konkretisieren) sowie die hier neu dokumentierte Gen3/GDM-Blank-Lücke.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

Nach dem nächsten Flash von G4MEOVER-FW v2 unter NFC → Magic prüfen, dass die App erscheint;
mit einem eigenen beschreibbaren Gen1a- oder Gen2-Blank (reiner Funktionstest, keine fremden
Kartendaten nötig) einen Schreib-/Wiederherstellungs-Testlauf durchführen und dabei insbesondere
den Gen4-Shadow-Mode/Set-default-config-Pfad einmal gegenprüfen, da dessen Fehlbedienung laut
eigenem Changelog Karten in einen nicht mehr reagierenden Zustand versetzen kann.
