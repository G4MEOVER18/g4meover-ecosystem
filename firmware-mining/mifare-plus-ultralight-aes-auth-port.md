# MIFARE Plus SL3 / Ultralight-AES (MF0AES20): schlüsselbasierte Authentifizierung fehlt mir komplett

## Korrekturhinweis (nachträglich ergänzt)

Diese Datei dopplt unwissentlich eine bereits vorher existierende Quellsuche zu demselben Fund:
`firmware-mining/mfplus-mfultralight-aes-auth-port.md` (ebenfalls bei mir vorhanden). Beim
Schreiben dieser Datei habe ich `firmware-mining/` nicht auf einen bereits vorhandenen
MIFARE-Plus-/Ultralight-AES-Eintrag durchsucht und dadurch fälschlich "seit Beginn offen"
geschrieben, obwohl zu diesem Zeitpunkt bereits geliefert waren:

- **Phase 1** (Krypto-Kern, identisch zu dem hier unten skizzierten Phase-1-Schritt) — bereits
  host-verifiziert gegen FIPS-197, alle vier RFC-4493-CMAC-Testvektoren **und** einen
  4000-Fälle-Differenzfuzz gegen OpenSSL `EVP_MAC` (strenger als unten vorgesehen), siehe
  `firmware-mining/mfplus-mfultralight-aes-auth-port.md` + `firmware-mining/repro-mfplus-
  mfultralight-aes-crypto/repro.c`.
- **Phase 2** für **beide** Kartenfamilien (Struct-/Serialisierungserweiterung) — siehe
  `firmware-mining/mfplus-phase2-sl3-struct-serialization.md` und `firmware-mining/
  mfultralight-aes-phase2-struct-serialization.md`.
- Der host-testbare Teil von **Phase 3** (SL0-vs-SL3-SAK-/ATQA-Korrekturen) — siehe
  `firmware-mining/mfplus-sl0-sl3-detection-port.md`.

Gegengeprüft, ob der hier verwendete neuere Unleashed-Commit (`197ed204`, 2026-10-05) gegenüber
dem in den obigen Dokumenten verwendeten Commit (`945234a`, 2026-09-10) inhaltlich neuen Umfang
für dieses Feature brachte: `mf_plus_crypto.c` ist zwischen beiden Commits byte-identisch, die
Ultralight-Listener-Dateien sind zeilenzahlidentisch — kein neuer Umfang, diese Datei ist eine
reine, sachlich überflüssige Dopplung der bereits vorhandenen Arbeit.

Die ursprünglichen Inhalte unten bleiben unverändert stehen (siehe `CONSTRAINTS.md` Punkt 6,
nicht destruktiv), sind aber durch diesen Hinweis als Zweitquelle statt als "Phase 1 von 4
noch nicht umgesetzt" einzuordnen. Tatsächlich wirklich offen: Phase 3 Rest (Poller-
Zustandsautomaten), Phase 4 (Schreibfluss/Admin-Keys), Phase 5 (Listener/Emulation), Phase 6
(UI/Dictionary-Anbindung) — alle vier hardwaregebunden bzw. auf den vorherigen aufbauend, siehe
`BACKLOG.md` "Firmware-Feature-Mining (Fortsetzung 22)" für die vollständige Korrektur.

## Was

Dies ist die vertiefte Quellsuche/Scoping-Analyse zu der in `firmware-mining/nfc-unified-key-dictionary.md`
nur als Randbefund notierten Lücke (dort beim Abgleich der Unleashed-`unlshd-092`-Dictionary-
Manager-Tabelle entdeckt, aber bewusst nicht weiterverfolgt) und zum Backlog-Punkt
"MIFARE Plus / Ultralight AES: schlüsselbasierte Authentifizierung fehlt mir komplett ... Quellsuche
noch offen".

Bei mir (Momentum-Basis, Tag `mntm-012`, API 87.1) ist die Unterstützung für beide Kartentypen auf
reine Erkennung beschränkt:

- `lib/nfc/protocols/mf_plus/` (bei mir 1125 Zeilen über 7 `.c`/`.h`-Dateien): `MfPlusData` enthält
  nur `version`/`type`/`size`/`security_level` (`mf_plus.h:80-87`). Der Poller
  (`mf_plus_poller_i.h:19-26`) kennt nur sechs Zustände —
  `Idle → ReadVersion → ParseVersion → ParseIso4 → ReadFailed/ReadSuccess` — also GET_VERSION
  (0x60) plus das darunterliegende ISO14443-4A-Protokoll, aber keinen einzigen
  Authentifizierungs- oder Sektor-Lese/Schreib-Zustand. Grep über den gesamten Baum nach
  `MfPlusKey` liefert null Treffer.
- `lib/nfc/protocols/mf_ultralight/` (bei mir 4652 Zeilen): `MfUltralightAuthPassword` (32-Bit-PWD,
  Ultralight-EV1/NTAG) und `MfUltralightC3DesAuthKey` (Ultralight-C, 3DES) existieren bereits und
  sind verdrahtet — aber `MfUltralightAesKey` kommt im gesamten Baum kein einziges Mal vor
  (gegen `lib/nfc/protocols/mf_ultralight/mf_ultralight.h` geprüft).

Unleashed-Firmware (`dev`-Branch, aktueller Stand Commit `197ed204678523e0236d3ff4759b74d12e2cc6bd`,
2026-10-05) hat beide Protokolle um eine vollständige AES-128-basierte
Drei-Pass-Authentifizierung samt Secure-Messaging-Session erweitert:

- `lib/nfc/protocols/mf_plus/` wächst auf 4084 Zeilen über 15 Dateien (+2959 ggü. meiner Basis).
  Neu: `mf_plus_crypto.c/.h` (279/129 Zeilen, AES-ECB/-CBC/-CMAC/-CMAC8/Session-Key-Ableitung/
  Read-/Write-IV-Aufbau), `mf_plus_listener.c/.h` + `mf_plus_listener_i.c/.h`
  (168+472 Zeilen, Kartenemulation mit Schlüsseln), dazu `mf_plus_i.c` (221→853 Zeilen),
  `mf_plus_poller.c` (210→809) und `mf_plus_poller_i.c` (67→437).
- `lib/nfc/protocols/mf_ultralight/` wächst auf 10019 Zeilen (+5367 ggü. meiner Basis). Neu:
  `mf_ultralight_aes_crypto.c/.h` (148/42 Zeilen, dieselben Primitive wie bei Plus, eigenständig
  für dieses Protokoll). `mf_ultralight_poller.c` wächst von 867 auf 1224 Zeilen,
  `_poller_i.c` von 430 auf 672, `_listener.c`/`_listener_i.c` ebenfalls deutlich.

Die neuen Poller-Zustände bei Unleashed zeigen den tatsächlichen Umfang:

```
MfPlus:       ... ReadFailed/ReadSuccess  (bei mir Endzustände)
           + RequestMode, ReadSignature, RequestKey, AuthSector, ReadSectorBlocks,
             RequestAdminKey, AuthAdminKey, ReadConfig,
             RequestWriteSector, AuthWriteSector, RequestWriteBlock, WriteBlock,
             WriteFailed, WriteSuccess

MfUltralight: ... Auth, AuthMfulC (bei mir vorhanden)
           + AuthAes   (ein einziger neuer Zustand — die eigentliche Komplexität steckt in den
                         aufgerufenen Krypto-/Protokoll-Funktionen, nicht im Zustandsautomaten)
```

Beide Protokolle verwenden dasselbe Schema (NXP AN10922-artige AES-128-Drei-Pass-Authentifizierung
mit RndA/RndB-Austausch, Session-Key-Ableitung per AES aus beiden Zufallswerten, anschließender
CMAC-gesicherter und teilweise AES-CBC-verschlüsselter Nachrichtenaustausch) und bringen dafür
jeweils ein eigenständiges, aber praktisch identisch aufgebautes Krypto-Hilfsmodul mit
(`mf_plus_crypto.c` / `mf_ultralight_aes_crypto.c` — eigene Quellcode-Kommentare bei Unleashed
bestätigen das: "Kept internal to the mf_ultralight protocol (mirrors mf_plus_crypto.c)").

**Wichtiger Befund für die Risikoeinschätzung:** Unleasheds eigener Datei-Header in
`mf_plus_crypto.h` dokumentiert selbst, dass der Schreibpfad noch nicht an echter Hardware
verifiziert ist ("the WRITE IV follows PM3's proven layout but is not yet hardware-validated in
this port — confirm with a real-card write trace before the write / emulation paths (PR6/7)").
Der Lesepfad gilt dort als bestätigt (Proxmark3- und Fork-Quelle stimmen überein, beide lesen
echte Karten). Das obere Fork selbst behandelt dies also bereits als mehrteiliges, stufenweise
validiertes Vorhaben (PR6/PR7 referenziert) — kein einmaliger, abgeschlossener Commit.

## Nutzen

- MIFARE Plus (SL1/SL3) und MIFARE Ultralight AES (MF0AES20) sind aktuelle NXP-Produktfamilien für
  Zutrittskontrolle/Transit/ÖPNV — bei Zutrittsaudits auf eigenen/autorisierten Anlagen taucht
  man ihnen zunehmend statt reinem MIFARE Classic. Aktuell kann ich solche Karten nur erkennen
  (Typ/Version/UID), aber weder mit einem bekannten (eigenen, autorisiert ausgehändigten) Schlüssel
  authentifizieren noch geschützte Sektoren/Seiten lesen — ein echter Funktionslücken-Fall, nicht
  nur ein Komfortfeature.
- Baut auf bereits vorhandener, generischer Dictionary-Infrastruktur auf: der in
  `firmware-mining/nfc-unified-key-dictionary.md` bereits portierte gemeinsame
  Schlüssel-Dictionary-Manager (`nfc_key_dict.c/.h`) ist laut Unleashed-Originalvorlage von Anfang
  an für vier Kartentypen ausgelegt, bei mir aktuell nur auf die zwei schon vorhandenen beschränkt
  (Classic, Ultralight-C). Sobald `MfPlusKey`/`MfUltralightAesKey` existieren, ist die UI-Seite
  (Schlüssel anlegen/auflisten/löschen) ein kleiner Erweiterungsschritt auf bereits portiertem
  Code, keine weitere Neuentwicklung.
- Die benötigte Kryptoprimitive (AES-128, Software, `mbedtls/aes.h`) ist in meiner Firmware bereits
  vendort und an anderer Stelle in Gebrauch (`lib/mbedtls.scons`, u. a. `crypto_cli.c`,
  `furi_hal_crypto`-Testsuite) — kein neuer Abhängigkeitsimport nötig, nur zwei neue, in sich
  geschlossene Wrapper-Dateien (CMAC/CBC/Session-Key-Ableitung) nach demselben Muster wie bei
  Unleashed.

## Betroffene Dateien (Umfang laut Unleashed-Quelle, zu portieren)

MIFARE Plus (`lib/nfc/protocols/mf_plus/`):
- `mf_plus.h` — `MfPlusKey`/`MfPlusKeyType`/`MfPlusAdminKeyType`, `MfPlusBlock`,
  `key_a[MF_PLUS_MAX_SECTORS]`/`key_b[...]`-Arrays plus Block-Speicher in `MfPlusData`.
- `mf_plus_crypto.c/.h` — **neu**: AES-ECB/-CBC/-CMAC/-CMAC8, Session-Key-Ableitung (Kenc/Kmac aus
  RndA/RndB), MAC-Berechnung über Kommando+Zähler+TI+Daten, Read-/Write-IV-Aufbau.
- `mf_plus_poller.c`/`mf_plus_poller_i.c`/`.h` — Auth-Sektor-/Admin-Key-/Schreib-Zustände (siehe
  Zustandsliste oben).
- `mf_plus_i.c`/`.h` — Setter/Getter für gefundene Schlüssel (`mf_plus_set_key_found()`,
  `mf_plus_set_admin_key_found()`), Laden/Speichern der erweiterten Struct.
- `mf_plus_listener.c/.h` + `mf_plus_listener_i.c/.h` — **neu**, Kartenemulation inkl. Schlüssel
  (für einen reinen Lese-/Auth-Port nicht zwingend, siehe Port-Skizze Phase 2).

MIFARE Ultralight AES (`lib/nfc/protocols/mf_ultralight/`):
- `mf_ultralight.h` — `MfUltralightAesKey`/`MfUltralightAesKeyType`
  (`Data`=DataProtKey Seiten 0x30-0x33, `Uid`=UIDRetrKey Seiten 0x34-0x37, `Originality`).
- `mf_ultralight_aes_crypto.c/.h` — **neu**, eigenständig (nicht mit `mf_plus_crypto.c` geteilt,
  bewusst getrennt gehalten laut Unleashed-Kommentar — dieselbe Trennung sollte ich übernehmen,
  siehe Risiko).
- `mf_ultralight_poller.c`/`_poller_i.c`/`.h` — ein neuer Zustand `AuthAes` plus die
  Drei-Pass-Sequenz und CMAC-gesichertes Secure-Messaging für die eigentlichen Lesezugriffe.
- `mf_ultralight_listener.c/_i.c/.h` — Kartenemulation (Phase-2-Umfang, s. u.).

Nicht betroffen: `protocol_items.c`/Registry (beide Protokolle sind bei mir bereits registriert,
nur ihre Fähigkeiten werden erweitert), `iso14443_4a*` (MIFARE Plus läuft bereits über die bei mir
vorhandene ISO14443-4A-Transportschicht, keine Änderung nötig).

## Risiko

- **Umfang**: kein "klein & sicher"-Fund. Kombiniert ca. 2959 (Plus) + 5367 (Ultralight-AES) neue/
  geänderte Zeilen laut Unleashed-Quelle — deutlich über jedem bisher in diesem Verzeichnis
  portierten Einzelfund (zum Vergleich: der komplette Nice-O-Code-Port über drei Phasen lag bei
  wenigen hundert Zeilen Diff). Realistisch nur stufenweise über mehrere Sitzungen zu portieren,
  analog zum bereits etablierten Muster (Nice O-Code: Grundlagen → Decoder-Anzeige →
  Encoder-Gegenstück).
- **Krypto-Korrektheit ist sicherheitskritisch in beide Richtungen**: ein Fehler in der
  Session-Key-Ableitung oder MAC-Prüfung könnte entweder echte Authentifizierungsversuche gegen
  eigene/autorisierte Karten grundlos scheitern lassen (reiner Funktionsausfall) oder — schlimmer
  — eine MAC-Prüfung so implementieren, dass sie zu lax verwirft (z. B. Konstantzeit-Vergleich
  falsch, Trunkierung falsch) und dadurch fälschlich Erfolg meldet. Jede Portierung dieser
  Funktionen braucht denselben Host-Repro-Maßstab wie bisher für Krypto-Funktionen üblich (siehe
  Nice O-Code: Encrypt/Decrypt-Rundlauf über tausende Zufallswerte unter ASan/UBSan) — insbesondere
  CMAC/CMAC8 gegen die öffentlichen RFC-4493-Testvektoren, nicht nur gegen sich selbst.
- **Unvalidierter Schreibpfad bei der Quelle selbst**: wie oben zitiert, markiert Unleashed den
  Write-IV-Aufbau selbst als noch nicht hardwarebestätigt. Eine eigene Portierung sollte diese
  Einschränkung übernehmen und den Schreibpfad (MIFARE Plus: `RequestWriteSector`/`WriteBlock`;
  Ultralight-AES: verschlüsseltes `WRITE`) explizit als "ungetestet, nur Lesepfad zuerst" markieren,
  statt die Unsicherheit der Quelle stillschweigend zu übernehmen.
- **Getrennte Implementierungen bewusst beibehalten**: Unleashed hält `mf_plus_crypto.c` und
  `mf_ultralight_aes_crypto.c` absichtlich getrennt, trotz nahezu identischer Primitive (eigener
  Kommentar: "mirrors mf_plus_crypto.c"). Für einen eigenen Port spricht das dafür, diese Trennung
  zu übernehmen statt vorzeitig eine gemeinsame Bibliothek zu bauen — ein Fehler in einer
  gemeinsamen Funktion hätte sonst Auswirkung auf beide Protokolle gleichzeitig.
- **Kein zusätzliches Missbrauchsrisiko über das hinaus, was MIFARE-Classic-Schlüssel-Dictionary-
  Angriffe bei mir bereits abdecken**: Schlüssel müssen weiterhin aus eigenem/autorisiertem Besitz
  stammen oder über ein Dictionary erraten werden (bei AES-128 praktisch nicht per Bruteforce
  machbar) — reine Lese-/Verwaltungsfunktion für bekannte Schlüssel, kein neuer Bruteforce- oder
  Seitenkanal-Angriffsvektor.

## Port-Skizze (mehrteilig, analog zum etablierten Nice-O-Code-Muster)

**Phase 1 — Krypto-Kern, host-verifiziert, kein Hardwarezugriff (kleinster, sicherster Schritt):**
1. `mf_plus_crypto.c/.h` und `mf_ultralight_aes_crypto.c/.h` 1:1 aus Unleashed übernehmen (reine
   `mbedtls/aes.h`-Wrapper-Funktionen, kein geteilter State, keine Protokoll-Verdrahtung).
2. Host-Repro gegen öffentliche RFC-4493-AES-CMAC-Testvektoren (NIST SP 800-38B Anhang D) für
   beide `*_cmac()`-Funktionen, dazu AES-128-ECB/-CBC gegen FIPS-197/SP-800-38A-Testvektoren.
   Zusätzlich Session-Key-Ableitung und IV-Aufbau gegen die in `mf_plus_crypto.h` zitierten
   Proxmark3-Quellen (`mifare4.c`, `mfp_data_crypt()` in `cmdhfmfp.c`) gegenprüfen, da dort die
   eigentliche Referenzimplementierung liegt.
3. Ergebnis: zwei eigenständig nutzbare, verifizierte Krypto-Module — noch ohne jede
   Poller-Integration, also ohne Risiko für den produktiven Lesepfad.

**Phase 2 — MIFARE Plus SL3 Lese-Authentifizierung (kein Schreiben, keine Emulation):**
4. `MfPlusData` um `key_a[]`/`key_b[]`/Block-Speicher erweitern (additiv,
   `mf_plus_load()`/`_save()` bleiben für Dateien ohne diese Felder rückwärtskompatibel, nach dem
   bereits etablierten Muster aus den Nice-O-Code-/Dictionary-Ports).
5. Poller um `RequestKey → AuthSector → ReadSectorBlocks` erweitern — **ohne**
   `RequestWriteSector`/`WriteBlock`/`RequestAdminKey`/`AuthAdminKey` (Admin-Keys und Schreiben
   bewusst für eine spätere Sitzung zurückstellen).
6. Testfall: eigene MIFARE-Plus-SL3-Karte mit bekanntem (selbst vergebenem/autorisiert erhaltenem)
   Sektorschlüssel — Authentifizierung muss gelingen, Blockinhalt muss mit einem unabhängigen
   Referenzlesegerät (Proxmark3) übereinstimmen.

**Phase 3 — Ultralight-AES Lese-Authentifizierung (analog, unabhängig von Phase 2):**
7. `MfUltralightAesKey`/`-KeyType` ergänzen, Poller um `AuthAes` plus den zugehörigen
   verschlüsselten/CMAC-gesicherten Lesepfad erweitern — wieder ohne Schreibpfad.

**Phase 4 — UI/Dictionary-Anbindung:**
8. `nfc_key_dict.c/.h` (bereits portiert) um die beiden neuen Typen erweitern — laut
   Unleashed-Originaltabelle ohnehin so vorgesehen, hier nur bei mir bisher nicht nachvollzogen.

**Bewusst zurückgestellt (eigene, separate künftige Sitzungen):** Schreibpfad für beide Protokolle
(unvalidierter Write-IV bei der Quelle selbst, siehe Risiko), Kartenemulation/Listener für beide
Protokolle, Admin-Key-Verwaltung (MIFARE Plus SL1→SL3-Umschaltung).

## Quellen

- Unleashed-Firmware, `dev`-Branch, Commit `197ed204678523e0236d3ff4759b74d12e2cc6bd`
  (2026-10-05): `lib/nfc/protocols/mf_plus/`, `lib/nfc/protocols/mf_ultralight/` (vollständiger
  AES-Authentifizierungs-Umfang, inkl. Datei-Header-Kommentare zu Proxmark3-Abgleich und
  Write-IV-Validierungsstatus).
- Momentum-Firmware, Tag `mntm-012`: `lib/nfc/protocols/mf_plus/`,
  `lib/nfc/protocols/mf_ultralight/` (Ausgangszustand — reine Erkennung ohne Schlüssel-Auth).
- `firmware-mining/nfc-unified-key-dictionary.md` (Erstbefund dieser Lücke, dort bewusst
  zurückgestellt).
- Proxmark3 (`mifare4.c`, `cmdhfmfp.c`) und NXP AN10922 — bei Unleashed selbst als Referenz für
  das AES-Drei-Pass-Schema zitiert, für eine eigene Portierung ebenfalls als Primärquelle
  heranzuziehen statt nur der abgeleiteten Fork-Implementierung.
- RFC 4493 / NIST SP 800-38B (AES-CMAC-Testvektoren), FIPS-197 / NIST SP 800-38A (AES-128-ECB/-CBC-
  Testvektoren) — für die in Phase 1 vorgesehene host-seitige Verifikation gegen öffentliche
  Referenzwerte statt nur gegen sich selbst.
