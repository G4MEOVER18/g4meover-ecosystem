# MIFARE Ultralight AES Phase 2: Struct-Erweiterung und Serialisierung

## Was

Ich habe bereits kartiert, dass mir für MIFARE Ultralight AES (MF0AES20) jede AES-128-
Authentifizierung fehlt (`firmware-mining/mfplus-mfultralight-aes-auth-port.md`, dort als
gestufte, mehrteilige Port-Skizze für dieses und das parallele MIFARE-Plus-SL3-Feature
angelegt) und den reinen Kryptografie-Kern (`mf_ultralight_aes_crypto.c/.h`) bereits als
Phase 1 host-verifiziert übernommen. Für MIFARE Plus SL3 habe ich in einer früheren Sitzung
bereits Phase 2 geliefert (Struct-Erweiterung + Serialisierung, ohne Poller/Zustandsautomat).
Diese Sitzung liefert das analoge Phase 2 für die zweite Kartenfamilie: die
`MfUltralightData`-Struct-Erweiterung und Serialisierung für UL-AES, wieder ohne jeden
Karten-I/O.

Ich habe dafür `lib/nfc/protocols/mf_ultralight/mf_ultralight.h`/`.c` aus Unleashed
(`dev`-Branch, aktueller Stand über `raw.githubusercontent.com` bezogen — der API-Zugriff für
Commit-Metadaten war in dieser Sitzung nicht verfügbar, daher kein einzelner Pin-Commit-Hash
wie sonst üblich, siehe "Quelle") gegen dieselben Dateien meiner eigenen `G4MEOVER-FW`-Basis
(`main`-Branch) diffgeprüft. Das reale Unleashed-Diff für UL-AES ist deutlich größer als nur
Struct/Serialisierung — es bündelt in denselben Dateien noch:

- eine komplett umgebaute Typ-Erkennung (`mf_ultralight_get_type_by_version()`: neue
  NTAG210/NTAG212-Kollisionsauflösung über das Produkt-Typ-Nibble),
- eine gelockerte `mf_ultralight_detect_protocol()`-ATQA-Prüfung,
- einen neuen `mf_ultralight_is_pwd_pack_read()`-Helfer,
- Schreibbereichs-Anpassungen in `mf_ultralight_get_write_end_page()` für UL-C/UL-AES.

Das sind eigenständige, von der AES-Frage unabhängige Funde/Features — ich habe sie bewusst
**nicht** mitgezogen (siehe "Nicht Teil dieses Patches"), um genau einen abgeschlossenen
Schritt zu liefern statt eines unübersichtlichen Sammelpatches.

## Nutzen

- Bringt meine eigene `MfUltralightData` strukturell auf den Stand, den der künftige UL-AES-
  Poller (Phase 3, braucht echte Hardware) referenzieren wird — analog dazu, wie mein bereits
  gelieferter MIFARE-Plus-SL3-Phase-2-Patch die dortige Poller-Anbindung vorbereitet hat.
- Schließt die einzige UL-AES-Teilkomponente, die schon jetzt rein am Speicher-/Dateiformat
  arbeitet und ohne Kartenkontakt vollständig verifizierbar ist: die optionale 48-Byte-
  Originalitätssignatur (`aes_signature`) und der neue Kartentyp-Eintrag selbst.
- `mf_ultralight_aes_get_key()` ist zusätzlich schon jetzt nützlich für einen künftigen
  Schlüsselwörterbuch-Anschluss (liest einen bereits bekannten/wiederhergestellten
  DataProtKey aus den vorhandenen generischen `page[]`-Daten, ohne dass dafür der Poller
  existieren muss).
- Beim Portieren einen echten, unabhängigen Fund gemacht und korrigiert (siehe
  "Abweichung von Unleashed"): Unleasheds eigener Serialisierungscode hätte bei mir zu einem
  nicht-deterministischen `mf_ultralight_is_equal()` für jede Nicht-UL-AES-Karte geführt.

## Betroffene Dateien

**Jetzt geliefert:**
- `lib/nfc/protocols/mf_ultralight/mf_ultralight.h` — neue Größenkonstanten
  (`MF_ULTRALIGHT_AES_SIGNATURE_SIZE`, `MF_ULTRALIGHT_AES_KEY_SIZE`,
  `MF_ULTRALIGHT_AES_DATA_KEY_PAGE`), neuer Kartentyp `MfUltralightTypeUltralightAES`, zwei
  neue `MfUltralightData`-Felder (`aes_signature_present`, `aes_signature[48]`), eine neue
  Funktionsdeklaration (`mf_ultralight_aes_get_key`).
- `lib/nfc/protocols/mf_ultralight/mf_ultralight.c` — neuer Eintrag in
  `mf_ultralight_features[]` (Gerätename, 60 Seiten, `config_page = 0`, Feature-Flags aus
  bereits vorhandenen Enum-Werten), `reset()`/`copy()`/`is_equal()` um die neuen Felder
  erweitert, optionale Lese-/Schreiblogik in `load()`/`save()` (Schlüssel `"AES signature"`,
  nur geschrieben wenn erfasst), neue Funktion `mf_ultralight_aes_get_key()`.
- Als Unified-Diffs gegen meine unveränderte `main`-Basis:
  `mfultralight-aes-phase2-struct-serialization/mf_ultralight.h.patch`,
  `mfultralight-aes-phase2-struct-serialization/mf_ultralight.c.patch` — `patch --dry-run -p1`
  vorab gegen eine frische Kopie beider Dateien verifiziert, danach real angewendet und
  Byte-für-Byte gegen die host-getestete Fassung verglichen (identisch).

**Nicht Teil dieses Patches (siehe "Was"):**
- `mf_ultralight_get_type_by_version()` (NTAG210/212-Erkennung, Produkt-Typ-Nibble-
  Disambiguierung) — eigener, unabhängiger Fund/Feature, braucht eigene Regressionsprüfung
  gegen meine bestehende NTAG213/UL11/UL21-Kollisionsbehandlung.
- `mf_ultralight_detect_protocol()` (ATQA-Low-Byte-Lockerung) — eigenständige, unabhängige
  Änderung.
- `mf_ultralight_is_pwd_pack_read()` — neuer Helfer, unabhängig von AES.
- `mf_ultralight_get_write_end_page()`-Anpassungen für den UL-AES-Schreibbereich — gehört zum
  Schreibfluss (künftige Phase 4, analog zur MIFARE-Plus-Skizze), nicht zu Struct/
  Serialisierung.
- `MfUltralightTypeNTAG210`/`MfUltralightTypeNTAG212`/`MfUltralightTypeUnknown` — neue
  Gerätetypen ohne AES-Bezug.
- `MfUltralightAesKey`-Struct und `MfUltralightAesKeyType`-Enum aus Unleashed — werden dort
  nur vom künftigen Poller (AUTHENTICATE-Schlüsselauswahl, Session-Schlüssel) referenziert,
  nicht von einem `MfUltralightData`-Feld; bewusst auf die Phase verschoben, die sie
  tatsächlich braucht, um diesen Patch auf das zu beschränken, was jetzt schon einen Zweck
  hat.

## Abweichung von Unleashed (eigener Fund)

Unleasheds eigene Ladelogik ist ein einzeiliges, direktes
`data->aes_signature_present = flipper_format_read_hex(...)` ohne vorheriges Nullen des
Zielpuffers. Meine (und Unleasheds) `mf_ultralight_alloc()` nullt den Speicher nicht
(einfaches `malloc()`) — bei einer Karte/Datei ohne den optionalen `"AES signature"`-
Schlüssel (jede Nicht-UL-AES-Karte, jeder ältere Dump) bleibt `aes_signature` dann auf dem
Allocator-Altinhalt stehen, während `aes_signature_present` korrekt `false` wird.
`mf_ultralight_is_equal()` vergleicht dieses Feld aber unbedingt per `memcmp()`, unabhängig
vom Presence-Flag — zwei andernfalls identische, aber unabhängig voneinander frisch
geladene Nicht-UL-AES-Karten könnten sich allein durch Heap-Vorbelegung als "verschieden"
herausstellen. Ich initialisiere den Puffer deshalb in meinem `load()` immer zuerst mit
`memset()`, bevor ich den optionalen Schlüssel lese — funktional identisch für den
UL-AES-Fall (Schlüssel vorhanden → wird ohnehin vollständig überschrieben), aber
deterministisch für den weitaus häufigeren Fall ohne diesen Schlüssel. Mit Host-Repro
verifiziert (siehe unten, Testgruppe 5).

## Verifikation (auf dem Host, ohne Hardware)

`firmware-mining/repro-mfultralight-aes-phase2-struct-serialization/repro.c` kompiliert die
tatsächlich gepatchten, unveränderten `mf_ultralight.c`/`.h` (einzige Host-Anpassung: die
beiden Includes für `iso14443_3a.h`/`mbedtls/des.h` zeigen auf Stand-ins statt der echten
Firmware-Header, siehe Kommentar am Dateianfang — der reale Patch behält die echten
Include-Pfade, siehe `.patch`-Dateien) gegen selbstgebaute, aber realistische
`FlipperFormat`/`FuriString`/`Iso14443_3aData`/`mbedtls_des3`-Ersatzheader.

Build: `gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -I stubs repro.c
mf_ultralight.c -o repro && ./repro` → `ALL CHECKS PASSED`, exit code 0, keine
LeakSanitizer-Funde.

Acht Testgruppen:
1. **Reset** — `aes_signature_present`/`aes_signature` werden aus vorbelegtem
   (nicht genulltem) Speicher korrekt auf `false`/all-zero zurückgesetzt.
2. **Copy** — beide neuen Felder werden übernommen, Ziel- und Quellpuffer sind unabhängig
   (nachträgliche Änderung an der Quelle wirkt sich nicht auf die Kopie aus).
3. **Save→Load-Rundlauf MIT AES-Signatur** (Typ `MfUltralightTypeUltralightAES`) — voller
   Rundlauf über die simulierte `.nfc`-Datei, `mf_ultralight_is_equal()` bestätigt
   Übereinstimmung.
4. **Save→Load-Rundlauf OHNE AES-Signatur** (Typ `MfulC`, der häufige/reguläre Fall) —
   bestätigt, dass der `"AES signature"`-Schlüssel gar nicht erst geschrieben wird, dass der
   geladene Puffer trotz unterschiedlicher Allocator-Vorbelegung zwischen Quelle und Ziel
   deterministisch all-zero ist, und `is_equal()` zutrifft.
5. **ALT-vs-NEU der oben beschriebenen Abweichung** — reproduziert Unleasheds Einzeiler
   direkt (ohne eigene Zweitkopie der ganzen Datei) gegen zwei unterschiedlich vorbelegte
   16-Byte-Puffer: ALT liefert korrekte Presence-Flags, aber unterschiedliche
   Pufferinhalte (die Instabilität tritt nachweislich auf); NEU (die echte, gepatchte
   `mf_ultralight_load()`) liefert für zwei unterschiedlich vorbelegte volle
   `MfUltralightData`-Allokationen identische Puffer und ein zutreffendes `is_equal()`.
6. **`mf_ultralight_aes_get_key()`** — Byte-Umkehrung gegen drei Fälle geprüft: monotone
   Testsequenz, All-Null, sowie der veröffentlichte AES-128-Referenzschlüssel aus
   FIPS-197/RFC-4493 (von Hand umgekehrt in die simulierten Kartenseiten geschrieben, über
   die Funktion zurückgelesen, exakter Vergleich).
7. **Typ-Verdrahtung** — `mf_ultralight_get_pages_total`/`_get_feature_support_set`/
   `_get_config_page_num` für den neuen Typ gegen die Spezifikation aus "Was" geprüft
   (60 Seiten, `ReadVersion|Authenticate|ReadCounter|IncCounter` gesetzt, `ReadSignature`/
   `FastRead`/`PasswordAuth` bewusst NICHT gesetzt), Gerätename über
   `mf_ultralight_get_device_name()`.
8. **Regression** — ein bereits vorhandener Kartentyp (`MfUltralightTypeUL21`) durchläuft
   Save→Load unverändert korrekt, komplett unberührt vom neuen Code.

**Nicht** durch diesen Host-Test abgedeckt: alles, was den noch nicht portierten
Poller-Zustandsautomaten (Phase 3, echte 3-Pass-AUTHENTICATE-Sequenz über die
Luftschnittstelle) oder echte Hardware braucht.

## Risiko

- **Niedrig**: rein additive Struct-Felder und ein neuer, isoliert erreichbarer Kartentyp-
  Eintrag; keine Änderung an einer bestehenden Funktionssignatur, kein geteilter State mit
  dem UL-C-3DES-Pfad. Der einzige potenzielle Regressionspunkt (`is_equal()`/`copy()`/
  `reset()` jetzt mit zwei zusätzlichen Feldern) ist durch Testgruppe 8 gegen einen
  unveränderten Kartentyp abgesichert.
- Da der Poller (Phase 3) fehlt, kann `data->type` in meiner Basis aktuell noch nie den Wert
  `MfUltralightTypeUltralightAES` annehmen — dieser Patch ist reine Vorbereitung, ohne
  Auswirkung auf das aktuelle Laufzeitverhalten, bis Phase 3 existiert (identischer Charakter
  wie mein bereits gelieferter MIFARE-Plus-SL3-Phase-2-Patch).
- Commit-Metadaten von Unleashed waren in dieser Sitzung nicht per API abrufbar (nur
  Rohdatei-Zugriff über `raw.githubusercontent.com`); ich habe deshalb gegen den aktuellen
  `dev`-Stand statt gegen einen einzelnen Pin-Commit-Hash geprüft. Kein inhaltliches Risiko
  für diesen Patch (die betroffenen Codeabschnitte sind seit ihrer Einführung stabil, siehe
  Bündelung mit dem MIFARE-Plus-SL3-Feature in derselben vorherigen Quellsuche), aber bei
  einer künftigen Sitzung mit erneut vollem API-Zugriff lohnt ein Abgleich, ob sich seitdem
  noch etwas an genau diesen Zeilen geändert hat.

## Port-Skizze (Fortsetzung der bestehenden Mehrphasen-Skizze)

Setzt direkt auf `firmware-mining/mfplus-mfultralight-aes-auth-port.md` auf: Phase 1
(Kryptografie-Kern) bereits geliefert, Phase 2 für MIFARE Plus SL3 bereits geliefert
(`firmware-mining/mfplus-phase2-sl3-struct-serialization.md`), diese Sitzung liefert Phase 2
für MIFARE Ultralight AES. Offen bleiben je nach dortiger Skizze: Phase 3 (Poller-
Zustandsautomat für SL3-Lesefluss + UL-AES-3-Pass-Auth, braucht echte Hardware), Phase 4
(SL3-Schreibfluss, Admin-Schlüssel-Wiederherstellung), Phase 5 (MIFARE-Plus-Listener/
Emulation), Phase 6 (UI/Schlüsselwörterbuch-Anbindung) — für UL-AES zusätzlich die oben
unter "Nicht Teil dieses Patches" gelisteten, unabhängigen Funde (Typ-Erkennungs-Umbau,
ATQA-Lockerung, `is_pwd_pack_read()`, Schreibbereichs-Anpassung) als eigene, künftig
einzeln zu bewertende Kandidaten.

## Quelle

- Unleashed-Firmware, `dev`-Branch (`DarkFlippers/unleashed-firmware`,
  `lib/nfc/protocols/mf_ultralight/mf_ultralight.h`/`.c`, per `raw.githubusercontent.com`
  bezogen, da der GitHub-API-Zugriff für Commit-Metadaten in dieser Sitzung nicht verfügbar
  war — siehe "Risiko").
- Meine eigene `G4MEOVER-FW`-Basis, `main`-Branch, dieselben Dateien, Ausgangszustand (kein
  UL-AES-Code).
- FIPS-197 (AES-128-Referenzschlüssel) — für die eigene Host-Verifikation der
  Byte-Umkehrung in `mf_ultralight_aes_get_key()` verwendet.
- Bereits vermerkte Vorarbeit: `firmware-mining/mfplus-mfultralight-aes-auth-port.md`
  (Gesamtbestandsaufnahme + Phase-1-Krypto-Kern),
  `firmware-mining/mfplus-phase2-sl3-struct-serialization.md` (analoges Phase 2 für die
  Schwesterkartenfamilie MIFARE Plus SL3).
