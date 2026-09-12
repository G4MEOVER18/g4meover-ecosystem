# MIFARE Plus SL3 / MIFARE Ultralight AES: schlüsselbasierte Authentifizierung fehlt komplett

## Was

Mein `lib/nfc/protocols/mf_plus` und `lib/nfc/protocols/mf_ultralight` (Momentum-Basis,
Tag `mntm-012`) lesen MIFARE Plus ausschließlich über UID/Version/Security-Level (Poller
nur bis `MfPlusPollerStateReadSuccess`, kein Auth-Zustand) und MIFARE Ultralight nur bis
zur bereits vorhandenen Ultralight-C-3DES-Authentifizierung — für die AES-128-Hälfte
beider Kartenfamilien (MIFARE Plus Security Level 3 und MIFARE Ultralight AES/MF0AES20)
fehlt mir jede schlüsselbasierte Authentifizierung. Das war bereits als Lücke in
`firmware-mining/nfc-unified-key-dictionary.md` vermerkt ("eigenständiges, größeres
Firmware-Feature, separat vorgemerkt"), dort aber nur als fehlende Voraussetzung notiert,
nicht als eigener Fund bearbeitet; die Quellsuche dafür war laut Backlog noch offen.

Ich habe die Quellsuche in dieser Sitzung abgeschlossen und den vollen Funktionsumfang
gegen meine eigene `mntm-012`-Basis kartiert. Ergebnis: Unleashed (`dev`, Commit
`945234a76701d556346f131c88f29f0feeec4d52`, 2026-09-10) hat seit meinem Pin-Stand zwei
große, parallel entwickelte Features bekommen:

1. **MIFARE Plus SL3** — vollständige AES-128-Sektor-Authentifizierung samt Secure
   Messaging (verschlüsseltes Lesen/Schreiben von Datenblöcken), Wiederherstellung der
   `0x90xx`-Admin-Schlüssel (Card Master/Config/L3-Switch/SL1-Card-Auth), Lesen der
   `0xB0xx`-Konfigurationsblöcke, Originalitätssignatur (`Read_Sig`), sowie eine
   **Emulationsseite** (`mf_plus_listener*.c`) für dieselbe SL3-Karte.
2. **MIFARE Ultralight AES (MF0AES20)** — 3-Pass-Authentifizierung (AES-128, CBC,
   Null-IV) über dieselben `AUTHENTICATE`-Befehlsbytes wie Ultralight-C, danach Secure
   Messaging (CMAC) für `READ`/`WRITE`/Counter-Lesen, sowie eine 48-Byte-Originalitäts-
   signatur (secp192r1).

Beide teilen sich dieselbe Architektur (eigenständiges `*_crypto.c` je Kartenfamilie,
zusätzliche Poller-Zustände für den Auth-/Datenfluss, neue Struct-Felder für Schlüssel/
Blöcke/Session, neue UI-Szenen). Insgesamt neu bzw. stark erweitert (Unleashed-Stand,
nur die beiden hier betroffenen Verzeichnisse):

| Bereich | Neue Dateien | Zeilen (neu) |
|---|---|---|
| `lib/nfc/protocols/mf_plus/` — Crypto | `mf_plus_crypto.c/.h` | 408 |
| `lib/nfc/protocols/mf_plus/` — Listener (Emulation) | `mf_plus_listener*.c/.h` (5 Dateien) | 774 |
| `lib/nfc/protocols/mf_ultralight/` — Crypto | `mf_ultralight_aes_crypto.c/.h` | 190 |
| `applications/main/nfc/helpers/` | `mf_plus_key_cache.c/.h` | 259 |
| `applications/main/nfc/helpers/protocol_support/` | `mf_plus_extra_scenes.c`, `mf_ultralight_extra_scenes.c` (Erweiterung bestehender Dateien) | +1576 |
| `applications/main/nfc/scenes/` | 8 neue Szenen (`mf_plus_show_keys`, `mf_plus_version`, `mf_plus_update_initial`, `mf_plus_dict_attack`, `mf_plus_iso4_info`, `mf_plus_more_info`, `mf_ultralight_aes_dict_attack`, `mf_ultralight_aes_dict_attack_warn`) | ~124 |

Dazu kommen substanzielle Erweiterungen bestehender, bei mir bereits vorhandener Dateien
(`mf_plus.h/.c`, `mf_plus_i.h/.c`, `mf_plus_poller*.h/.c`, `mf_ultralight.h`,
`mf_ultralight_poller_i.h/.c`) um neue Structs (`MfPlusPollerSession`,
`MfUltralightPollerAesCmac`), ~15 neue Poller-Zustände je Kartenfamilie und neue
Datenfelder (`MfPlusBlock`/`MfPlusKey`-Arrays samt Bitmasken, `MfUltralightAesKey`).
Insgesamt eine Größenordnung von grob 3000+ geänderten/neuen Zeilen über ~25 Dateien —
deutlich zu groß für einen einzelnen "klein & sicher"-Patch im Sinne von `WORKFLOW.md`.

Ich liefere in dieser Sitzung deshalb, analog zum bereits gelieferten Security+-2.0-
Keypad-Fund:

1. Diesen Port-Vorschlag mit vollständiger Bestandsaufnahme (was fehlt mir, was ist neu,
   Risiko, gestufte Port-Skizze für mehrere künftige Sitzungen).
2. Den einzigen Teil, der bereits jetzt eigenständig, ohne Karten-I/O und ohne
   Zustandsautomat portierbar ist — die reinen AES-128-Kryptografie-Primitive beider
   Kartenfamilien (`mf_plus_crypto.c/.h`, `mf_ultralight_aes_crypto.c/.h`) — 1:1
   übernommen und auf dem Host gegen unabhängige Referenzen verifiziert (siehe
   "Verifikation").

Die Poller-/Listener-Zustandsautomaten, die neuen Struct-Felder in `MfPlusData`/
`MfUltralightData`, das Secure-Messaging-Framing über die Luftschnittstelle und die
komplette UI liefere ich bewusst **nicht** als Patch — siehe "Risiko" und "Port-Skizze".

## Nutzen

- Schließt eine seit der letzten Sitzung bekannte, komplett fehlende Kartenkategorie:
  MIFARE Plus SL3 und MIFARE Ultralight AES sind die AES-Nachfolger von MIFARE Classic
  und Ultralight-C und in neueren Zutritts-/Transit-/Ticketing-Systemen verbreitet — ohne
  diese Unterstützung erscheinen solche Karten bei mir nur mit UID/Version, ohne jeden
  Sektor-/Speicherinhalt, selbst wenn ich den Schlüssel kenne (eigene Karte, autorisierter
  Test).
- Reine Erweiterung der Lese-/Auditfähigkeit für bereits unterstützte, öffentlich
  dokumentierte Kartentypen (NXP-Datenblätter, AN10922) — kein neues Angriffsprimitiv,
  Authentifizierung erfolgt weiterhin nur mit bekanntem/wörterbuch-ermitteltem Schlüssel,
  identisch zum bestehenden MIFARE-Classic-/Ultralight-C-Schlüsselwörterbuch-Modell, das
  ich bereits über `firmware-mining/nfc-unified-key-dictionary.md` abgedeckt habe.
- Die jetzt verifizierten Kryptografie-Primitive sind die einzige Komponente, die von
  *beiden* Feature-Hälften (Plus SL3 und Ultralight AES) sowie von *beiden* Rollen
  (Poller und, im Plus-Fall, Listener/Emulation) gemeinsam genutzt wird — ihre Korrektheit
  jetzt schon host-seitig abzusichern, entkoppelt spätere Sitzungen zur
  Zustandsautomat-Verdrahtung von der Frage "stimmt die Kryptografie überhaupt".

## Betroffene Dateien (gestuft, siehe Port-Skizze)

**Jetzt geliefert (Phase 1, siehe Verifikation):**
- `lib/nfc/protocols/mf_plus/mf_plus_crypto.c` (279 Zeilen) / `.h` (129 Zeilen) — neu,
  keine Abhängigkeit zu einer anderen bei mir fehlenden Datei.
- `lib/nfc/protocols/mf_ultralight/mf_ultralight_aes_crypto.c` (148 Zeilen) / `.h`
  (42 Zeilen) — neu, ebenfalls ohne Fremdabhängigkeit.

**Für künftige Sitzungen (Phase 2+, nicht Teil dieses Patches):**
- `lib/nfc/protocols/mf_plus/mf_plus.h/.c`, `mf_plus_i.h/.c` — neue Structs (`MfPlusBlock`,
  `MfPlusKey`, `MfPlusAdminKeyType`, Bitmasken) und Serialisierung (`mf_plus_sl3_data_save/
  load`); bei mir aktuell nur Identitäts-/Versionsfelder.
- `lib/nfc/protocols/mf_plus/mf_plus_poller_i.h/.c`, `mf_plus_poller.c` — ~15 neue
  Poller-Zustände für SL3-Lese-/Schreibfluss und Admin-Schlüssel-Wiederherstellung, neues
  `MfPlusPollerSession`-Struct (K_ENC/K_MAC/TI/Zähler).
- `lib/nfc/protocols/mf_plus/mf_plus_listener*.c/.h` (5 Dateien, 774 Zeilen) — komplett
  neue Emulationsseite, bei mir aktuell für MIFARE Plus nicht vorhanden.
- `lib/nfc/protocols/mf_ultralight/mf_ultralight.h`, `mf_ultralight_poller_i.h/.c` — neuer
  Kartentyp `MfUltralightTypeUltralightAES`, `MfUltralightAesKey`-Datenfeld, neuer
  Poller-Zustand `MfUltralightPollerStateAuthAes`, `MfUltralightPollerAesCmac`-Session.
- `applications/main/nfc/helpers/mf_plus_key_cache.c/.h` (259 Zeilen) — neu.
- `applications/main/nfc/helpers/protocol_support/mf_plus/mf_plus_extra_scenes.c`,
  `.../mf_ultralight/mf_ultralight_extra_scenes.c` — bei mir bereits vorhandene Dateien,
  stark erweitert (UI für Schlüsseleingabe/-anzeige, Wörterbuchangriff, Signaturanzeige).
- `applications/main/nfc/scenes/nfc_scene_mf_plus_{show_keys,version,update_initial,
  dict_attack,iso4_info,more_info}.c`, `nfc_scene_mf_ultralight_aes_dict_attack{,_warn}.c`
  — 8 neue Szenen, je 15-17 Zeilen (Boilerplate, referenzieren die erweiterten
  `extra_scenes`-Dateien).
- Schlüsselwörterbuch-Integration: nutzt bei mir bereits die generische
  `nfc_key_dict`-Infrastruktur aus `firmware-mining/nfc-unified-key-dictionary.md` —
  bei einem Port müssten `MfPlusAdminKeyType`/`MfUltralightAesKeyType` dort als weiterer
  Schlüsseltyp ergänzt werden, keine neue Parallel-Infrastruktur nötig.

## Verifikation (auf dem Host, ohne Hardware)

Beide `*_crypto.c`-Dateien sind reine, zustandslose AES-128-Funktionen ohne Karten-I/O,
ohne Zeiger auf variable Puffer außerhalb der von der jeweiligen Funktion selbst
kontrollierten Länge und ohne Bezug zum Poller-/Listener-Zustand — genau der Teil, der
sich unabhängig vom Rest des Ports prüfen lässt. Beide nutzen ausschließlich die eigene
Firmware-`mbedtls` (`mbedtls/aes.h`), keine Drittbibliothek.

Host-Repro in `firmware-mining/repro-mfplus-mfultralight-aes-crypto/repro.c`
(`gcc -O1 -Wall -Wextra -fsanitize=address,undefined`, gegen die unveränderten
`mf_plus_crypto.c`/`mf_ultralight_aes_crypto.c` aus Unleashed plus eine `furi.h`-Stub-Datei,
die `furi_check()` auf `assert()` abbildet), Ergebnis `ALL CHECKS PASSED`:

1. **AES-128-ECB** (`mf_plus_crypto_ecb_encrypt/decrypt`) gegen den veröffentlichten
   FIPS-197-Referenzvektor (Schlüssel `000102...0f`, bekannter Klartext/Geheimtext) —
   sowohl Verschlüsselung als auch Entschlüsselungs-Rundlauf.
2. **AES-CMAC** (`mf_plus_crypto_cmac` **und** `mf_ultralight_aes_cmac` — zwei
   unabhängig geschriebene Implementierungen desselben Algorithmus in derselben
   Feature-Lieferung) gegen alle vier veröffentlichten RFC-4493-Testvektoren
   (Mlen = 0/16/20/64, Referenzschlüssel `2b7e1516...cf4f3c`).
3. **Differenzieller Fuzz gegen OpenSSL** (`EVP_MAC` "CMAC", eine dritte, unabhängige
   Implementierung): 4000 zufällige (Schlüssel, Nachricht)-Paare, Nachrichtenlänge
   0-256 Byte — deutlich über die vier festen Vektoren hinaus, 0 Abweichungen für
   `mf_plus_crypto_cmac` und `mf_ultralight_aes_cmac`.
4. **Truncated-MAC** (`mf_plus_crypto_cmac8`, `mf_ultralight_aes_cmac8_ctr`) gegen dieselbe
   OpenSSL-Referenz, inkl. des bei `cmac8_ctr` vorangestellten 2-Byte-Zählers — 0
   Abweichungen über dieselben 4000 Fälle.
5. **AES-128-CBC-Rundlauf** (`mf_plus_crypto_cbc_encrypt`/`_decrypt`, 500 zufällige
   Fälle, 1-8 Blöcke) sowie Block-für-Block-Gegenrechnung von Hand über die bereits
   FIPS-197-verifizierte ECB-Primitive (klassische CBC-Verkettung) — 0 Abweichungen in
   beiden Prüfungen.
6. **Rotations-Helfer** (`mf_plus_crypto_rotate_left`, `mf_ultralight_aes_rol16`):
   16-facher Zyklus über ein 16-Byte-Blockergibt wieder den Ausgangswert — 1000 zufällige
   Blöcke je Funktion, 0 Abweichungen.
7. **Read-/Write-IV-Aufbau** (`mf_plus_crypto_build_read_iv`/`_write_iv`): Byte-Layout
   exakt gegen die im Header dokumentierte Konstruktion nachgerechnet (Zähler-Wort an
   den dokumentierten Positionen wiederholt, TI an der dokumentierten Position), beide
   IVs kollidieren nie für dieselben Eingaben.
8. **Session-Schlüssel-Ableitung** (`mf_plus_crypto_derive_session_keys`,
   `mf_ultralight_aes_derive_session_key`): deterministisch (zweifacher Aufruf mit
   identischen Eingaben liefert identisches Ergebnis), reagiert auf jedes geänderte
   Eingabe-Byte (200 zufällige Fälle je Funktion, kein degenerierter/konstanter Fall),
   `k_enc != k_mac` in jedem Fall.

**Nicht** durch diesen Host-Test abgedeckt (erfordert die noch nicht portierten Poller-/
Listener-Zustandsautomaten bzw. echte Hardware, siehe "Risiko"): die eigentliche
3-Pass-Authentifizierungssequenz über die Luftschnittstelle, Secure-Messaging-Framing im
Poller/Listener, SL3-Sektor-/Admin-Schlüssel-Wiederherstellungsfluss, sowie der im
Datei-Header von `mf_plus_crypto.h` selbst vermerkte Hinweis, dass die Write-IV-Variante
zwar PM3-Layout-konform, aber noch nicht hardwareseitig gegen eine echte Karten-
Schreiboperation validiert ist (Zitat: "not yet hardware-validated in this port").

## Risiko

- **Niedrig** für den gelieferten, host-verifizierten Kryptografie-Kern: rein additive
  neue Dateien ohne Änderung an einer bestehenden Datei, keine Signaturänderung, kein
  geteilter State, keine Registrierung in einer Protokoll-Registry. Beide Dateien sind
  in Unleashed selbst bereits als "cross-validated against Proxmark3 (mifare4.c +
  mfp_data_crypt in cmdhfmfp.c) and the flipperzero-mfp-reader fork" dokumentiert
  (Kommentar in `mf_plus_crypto.h`) bzw. gegen NXP MF0AES20 §8.8 referenziert
  (`mf_ultralight_aes_crypto.h`) — meine Host-Verifikation bestätigt das unabhängig
  gegen RFC 4493/FIPS-197/OpenSSL, ohne mich auf diese Fremdquellen verlassen zu müssen.
- **Mittel-hoch** für die noch ausstehende Zustandsautomat-/UI-Integration — bewusst
  nicht als fertiger Patch geliefert:
  - Die Write-IV-Variante ist laut Unleasheds eigenem Kommentar noch nicht
    hardware-validiert; vor einem eigenen Schreib-/Emulationspfad muss ich das an einer
    echten SL3-Karte mit bekanntem Schlüssel selbst nachvollziehen, nicht blind
    übernehmen.
  - Zwei komplett neue Poller-Zustandsautomaten (SL3-Lese-/Schreibfluss +
    Admin-Schlüssel-Wiederherstellung; UL-AES-3-Pass-Auth + Secure Messaging) sind ein
    erheblich größeres Fehlerrisiko als reine Bit-Mathematik: falsch verdrahtete
    Zustandsübergänge zeigen sich typischerweise erst an echter Hardware (Timing,
    Kartenverhalten bei ungültiger Antwort), nicht auf dem Host.
  - Eine komplett neue Listener-/Emulationsseite für MIFARE Plus (774 Zeilen,
    5 Dateien) bedeutet: bei einem Fehler dort emuliert mein Flipper eine SL3-Karte
    möglicherweise fehlerhaft gegenüber einem echten Lesegerät — höheres Testaufwand
    (zweites Gerät/Lesegerät nötig), nicht rein durch Quelltextprüfung abschließend
    bewertbar.
  - Schlüsselverwaltung: `MfPlusAdminKeyType`/`MfUltralightAesKeyType` müssen sauber in
    die bereits bei mir vorhandene generische `nfc_key_dict`-Infrastruktur integriert
    werden, ohne die dort bereits behobenen Klassic-/Ultralight-C-Fixes
    (`firmware-mining/nfc-unified-key-dictionary.md`) zu regressieren.
- **Kein zusätzliches Missbrauchsrisiko** über das hinaus, was mein bestehendes
  MIFARE-Classic-/Ultralight-C-Schlüsselwörterbuch ohnehin hat: Authentifizierung
  funktioniert ausschließlich mit einem bereits bekannten oder per Wörterbuch
  ermittelten Schlüssel; kein neuer Kryptoangriff, keine Schlüsselableitung aus
  öffentlichen Daten allein.

## Port-Skizze (mehrteilig, für künftige Sitzungen gestuft)

1. **Phase 1 (dieser Patch):** `mf_plus_crypto.c/.h` und `mf_ultralight_aes_crypto.c/.h`
   unverändert nach `lib/nfc/protocols/mf_plus/` bzw. `lib/nfc/protocols/mf_ultralight/`
   übernehmen, in die jeweilige `application.fam`/`SConscript` der Bibliothek
   aufnehmen (reine Quelldatei-Ergänzung, kein neues Ziel). Baut eigenständig, wird von
   nichts referenziert, bis Phase 2 beginnt — risikofrei einbaubar, keine
   Funktionsänderung am bestehenden Verhalten.
2. **Phase 2:** `MfPlusData`/`MfUltralightData`-Struct-Erweiterung (Schlüssel-/
   Blockspeicher, Bitmasken) plus Serialisierung (`mf_plus_sl3_data_save/load`) —
   host-verifizierbar (reines Speicher-/Dateiformat, kein Karten-I/O), aber erst nach
   Phase 1 sinnvoll, da sie `MfPlusKey`/`MfPlusBlock` aus `mf_plus.h` referenziert.
3. **Phase 3:** Poller-Zustandsautomat für SL3-Lesefluss (Auth → Sektor lesen →
   nächster Sektor) und UL-AES-3-Pass-Auth + Secure-Messaging-Read — erfordert echte
   Hardware zum Testen (Timing, Antwortverhalten einer echten Karte); Port-Skizze anhand
   der jetzt kartierten Zustände (`MfPlusPollerState*`, `MfUltralightPollerStateAuthAes`)
   in einer eigenen künftigen Sitzung konkretisieren, sobald Phase 1/2 lokal gebaut und
   die Konstanten gegenrechnet sind.
4. **Phase 4:** SL3-Schreibfluss und Admin-Schlüssel-Wiederherstellung — nach Phase 3,
   erst nach hardwareseitiger Bestätigung der Write-IV (siehe "Risiko").
5. **Phase 5:** `mf_plus_listener*.c` (Emulation) — eigenständige, spätere Sitzung;
   benötigt ein zweites Lesegerät zum Testen, daher zeitlich unabhängig von Phase 3/4.
6. **Phase 6:** UI (`mf_plus_key_cache.c/.h`, erweiterte `extra_scenes`, 8 neue Szenen,
   Anbindung an die eigene `nfc_key_dict`-Infrastruktur) — letzter Schritt, sobald die
   darunterliegenden Phasen lokal gebaut/geflasht funktionieren.
7. Bauen nach jeder Phase: `ufbt` (bzw. `fbt firmware_all`) — jede Struct-/
   Signaturabweichung fällt sofort als Compile-Fehler auf.
8. Funktionstest lokal (erst ab Phase 3 mit Hardware nötig): eigene MIFARE-Plus-SL3-Karte
   mit bekanntem Sektorschlüssel aus Phase 1-3 lesen, Sektorinhalt gegen ein unabhängiges
   Referenzwerkzeug (Proxmark3, falls vorhanden) vergleichen; für UL-AES analog mit einer
   eigenen NTAG-424-artigen/MF0AES20-Karte.

## Quelle

- Unleashed-Firmware, `dev`-Branch, Commit `945234a76701d556346f131c88f29f0feeec4d52`
  (2026-09-10, API 88.9): `lib/nfc/protocols/mf_plus/`, `lib/nfc/protocols/mf_ultralight/`,
  `applications/main/nfc/helpers/`, `applications/main/nfc/scenes/`.
- Momentum-Firmware, Tag `mntm-012` (Commit `e1784e7418d8b074e971983ceb6fef0f37e52ae4`):
  dieselben Verzeichnisse, Ausgangszustand (kein AES-Auth-Code).
- `mf_plus_crypto.h` selbst zitiert als Kreuzvalidierungsquellen: Proxmark3
  (`mifare4.c`, `mfp_data_crypt()` in `cmdhfmfp.c`) und den `flipperzero-mfp-reader`-Fork.
- RFC 4493 (AES-CMAC), FIPS-197 (AES-Referenzvektor) — für die eigene Host-Verifikation
  verwendete, von der Kartenquelle unabhängige Standards.
- Bereits vermerkte Lücke: `firmware-mining/nfc-unified-key-dictionary.md`
  ("Plus-/Ultralight-AES-Hälfte... separat vorgemerkt").
