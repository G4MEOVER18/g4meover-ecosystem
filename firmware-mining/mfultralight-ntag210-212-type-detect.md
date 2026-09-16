# MIFARE Ultralight/NTAG: NTAG210/212-Typ-Erkennung + Unknown-Fallback

## Was

Letzter der vier beim UL-AES-Phase-2-Port zurückgestellten, von der AES-Frage unabhängigen
Unleashed-Funde (siehe `firmware-mining/mfultralight-aes-phase2-struct-serialization.md`,
inzwischen abgearbeitet bis auf diesen Punkt): Unleashed erkennt in `mf_ultralight.c` inzwischen
zwei zusätzliche Kartentypen, NTAG210 und NTAG212, und ersetzt dabei den bisherigen
`mf_ultralight_get_type_by_version()`-Aufbau durch eine klarere, Switch-basierte Fassung mit
einem echten `Unknown`-Fallback. Zwei zusammengehörige Änderungen:

**1. `mf_ultralight_get_type_by_version()` — Umbau + Familien-Nibble-Unterscheidung**

Meine Basis (`main`-Branch) initialisiert `type` auf `MfUltralightTypeOrigin` und überschreibt es
in einer `if`/`else if`-Kette; bleibt am Ende keine Bedingung wahr (GetVersion liefert eine
Antwort, aber `storage_size`/`prod_subtype` passen zu keinem bekannten Muster), bleibt `type` beim
Origin-Anfangswert stehen — ein echtes NTAG/Ultralight, das per GetVersion geantwortet hat, wird
so mit der Bedeutung "hat gar nicht auf GetVersion geantwortet" verwechselt. Zusätzlich kollidiert
`storage_size` bei zwei Werten zwischen zwei Kartenfamilien: `0x0B`/`0x00` (MF0UL11 **und**
NTAG210) sowie `0x0E` (MF0UL21 **und** NTAG212) — meine Basis kennt in beiden Fällen nur die
Ultralight-Seite.

Unleashed löst beides zugleich: ein `switch` über `storage_size` mit einem echten `default:
return MfUltralightTypeUnknown;`, und für genau die beiden kollidierenden Werte eine zusätzliche
Unterscheidung über das untere Nibble von `prod_type` (AN10833: Produktfamilie; `0x03` =
Ultralight, `0x04` = NTAG) — bei jedem anderen `storage_size`-Wert bleibt die bisherige, alleinige
`storage_size`-Zuordnung unverändert (kein härterer Zwang zu einem "korrekten" `prod_type").
Ebenso ersetzt im NTAG-I2C-Zweig (Subtype 5, Major 2) ein `return MfUltralightTypeUnknown;` das
bisherige stille Nicht-Setzen bei unbekanntem `storage_size`/`minor`.

**2. Feature-Tabelle — drei neue Einträge**

`MfUltralightTypeNTAG210` (identisches Speicherlayout wie MF0UL11: 20 Seiten, Konfig-Seite 16,
aber ohne die EV1-Zähler-Fähigkeiten von UL11 — NTAG210 hat laut Unleashed-Kommentar keine),
`MfUltralightTypeNTAG212` (identisches Layout wie MF0UL21: 41 Seiten, Konfig-Seite 37, Dynamic
Lock) und `MfUltralightTypeUnknown` (konservativ 16 Seiten lesen, exakt derselbe `feature_set`
— nur `MfUltralightFeatureSupportCompatibleWrite` —, den der bisherige implizite
Origin-Fallback schon hatte; nur der Typ-Name/die Bedeutung ändert sich, nicht das Verhalten für
diesen Fall).

## Nutzen

- Schließt den letzten der vier beim UL-AES-Phase-2-Port zurückgestellten, unabhängigen
  Unleashed-Funde ab.
- Behebt eine reale Fehlerkennung: eine echte NTAG210- oder NTAG212-Karte wird bei mir aktuell
  über `storage_size` allein der falschen Ultralight-Familie zugeordnet (UL11 bzw. UL21) —
  mit entsprechend falscher `device_name`-Anzeige und falsch angenommenen EV1-Zähler-Fähigkeiten
  (UL11/UL21 haben `MfUltralightFeatureSupportReadCounter`/`IncCounter`, NTAG210/212 laut
  Unleashed nicht). Seitenanzahl und Konfig-Seite bleiben zufällig richtig, da beide Familien
  jeweils dasselbe Speicherlayout teilen — der Fehler betrifft ausschließlich Name und
  Fähigkeits-Flags.
- Behebt eine zweite, unabhängige Fehlerkennung: jedes GetVersion-antwortende, aber nicht
  modellierte IC (unbekannter Klon, künftiger NXP-Chip) wird aktuell mit dem Anfangswert
  `MfUltralightTypeOrigin` verwechselt — einem Typ, dessen ganze Bedeutung eigentlich "kein
  GetVersion, klassisches Original-Ultralight" ist. Der neue `Unknown`-Typ trennt beide Fälle
  sauber, ohne das tatsächliche Verhalten für diesen Fall zu ändern (identischer `feature_set`,
  siehe Testgruppe 5).
- Beide Änderungen sind rein additiv zur Typerkennung: kein bestehender, bereits korrekt
  erkannter Kartentyp ändert sein Ergebnis (siehe Testgruppen 1 und 3 — inklusive des
  NTAG-I2C-Zweigs, der unverändert vier Untertypen erkennt).

## Betroffene Dateien

**Jetzt geliefert** (Unified-Diffs gegen meine unveränderte `main`-Basis, `patch --dry-run -p1`
vorab gegen frische Kopien beider Dateien verifiziert, danach real angewendet und
Byte-für-Byte gegen die host-getestete Fassung abgeglichen):

- `lib/nfc/protocols/mf_ultralight/mf_ultralight.h` —
  `mfultralight-ntag210-212-type-detect/mf_ultralight_h.patch`: drei neue Enum-Werte
  (`MfUltralightTypeNTAG210`, `MfUltralightTypeNTAG212`, `MfUltralightTypeUnknown`), angehängt
  ans Ende von `MfUltralightType`, vor `MfUltralightTypeNum`. Keine sonstige Header-Änderung.
- `lib/nfc/protocols/mf_ultralight/mf_ultralight.c` —
  `mfultralight-ntag210-212-type-detect/mf_ultralight_c.patch`: drei neue Einträge in der
  `mf_ultralight_features[]`-Tabelle sowie der komplette Umbau von
  `mf_ultralight_get_type_by_version()` wie oben beschrieben.

**Nicht Teil dieses Patches:**

- Kein Eingriff in `mf_ultralight_get_write_end_page()` — siehe "Risiko" unten, das ist bewusst
  so, nicht vergessen.
- Kein Eingriff in `mf_ultralight_is_counter_configured()`: das bestehende `switch` dort hat
  bereits einen `default`-Zweig (`configured = true`), NTAG210/212/Unknown fallen dort korrekt
  hindurch, ohne dass eine `case`-Ergänzung nötig wäre — geprüft, nicht nur angenommen (siehe
  "Verifikation").
- Kein Eingriff in `mf_ultralight_get_device_name_by_type()`, `mf_ultralight_is_all_data_read()`
  oder sonstige typ-abhängige Funktionen in dieser Datei: alle lesen generisch aus
  `mf_ultralight_features[data->type]` bzw. prüfen nur einzelne, konkrete Typen, die von diesem
  Patch nicht berührt werden.
- Die MFUL-AES-Erkennung (`storage_size == 0x0F`, Familien-Nibble `0x03` vs. NTAG213) aus
  Unleasheds aktuellem Stand ist bewusst **nicht** mitgeliefert — der `MfUltralightTypeUltralightAES`-Typ
  existiert in meiner Basis nur als eigener, noch nicht angewendeter Vorschlag
  (`firmware-mining/mfultralight-aes-phase2-struct-serialization/`); `storage_size == 0x0F`
  bleibt in diesem Patch unverändert bei `MfUltralightTypeNTAG213`. Siehe "Risiko" zur
  Reihenfolge, falls beide Vorschläge lokal zusammen angewendet werden sollen.

## Verifikation (auf dem Host, ohne Hardware)

`firmware-mining/repro-mfultralight-ntag210-212-type-detect/repro.c` kompiliert die tatsächlich
gepatchte, unveränderte `mf_ultralight.c`/`.h` (einzige Host-Anpassung: derselbe
Include-Stand-in wie bei den vorherigen UL-Ports) gegen die etablierten `stubs/` und ruft die
reale, kompilierte `mf_ultralight_get_type_by_version()` sowie die realen
`mf_ultralight_get_pages_total()`/`_get_config_page_num()`/`_get_feature_support_set()` auf.
`old_get_type_by_version()` ist eine reine, unveränderte Kopie meiner bisherigen Basisfassung,
ausschließlich für den ALT-vs-NEU-Vergleich (kein Teil des Patches).

Build: `gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -I . -I stubs repro.c
mf_ultralight.c -o repro && ./repro` → `ALL CHECKS PASSED`, exit code 0.

Sieben Testgruppen:
1. **Unveränderte, nicht-kollidierende `storage_size`-Werte** (0x0F, 0x11, 0x13): weiterhin
   NTAG213/215/216, unabhängig vom `prod_type`-Familien-Nibble.
2. **Die beiden Kollisionen, neu aufgelöst:** 0x0B/0x00 mit Familie 0x03 weiterhin UL11, mit
   Familie 0x04 neu NTAG210; 0x0E mit Familie 0x03 weiterhin UL21, mit Familie 0x04 neu NTAG212.
3. **Nicht-Standard-`prod_type` bleibt lenient:** ein Klon mit Familien-Nibble `0x00` an Größe
   0x0B/0x0E landet weiterhin bei UL11/UL21 — die Familienprüfung ist eine Zusatzunterscheidung
   für die beiden Kollisionsfälle, keine neue Voraussetzung für jede andere Karte.
4. **NTAG-I2C-Zweig unverändert + neuer Unknown-Fallback:** alle vier bestehenden
   Subtype/Major/Minor/Size-Kombinationen weiterhin korrekt; ein unbekannter `storage_size`
   innerhalb des Zweigs sowie ein unbekannter `minor`-Wert liefern neu `Unknown` statt (vorher)
   stillschweigend `Origin`.
5. **Genereller Unknown-Fallback:** ein komplett unbekannter `storage_size`-Wert außerhalb des
   NTAG-I2C-Zweigs liefert `Unknown` statt `Origin`.
6. **ALT-vs-NEU:** dieselbe reale NTAG210-Antwort liefert bei `old_get_type_by_version()`
   fälschlich `UL11`, bei der echten gepatchten Funktion korrekt `NTAG210`; dieselbe unbekannte
   IC-Antwort liefert bei ALT fälschlich `Origin`, bei NEU korrekt `Unknown`.
7. **Feature-Tabelle:** Seitenzahl/Konfig-Seite/Feature-Flags der drei neuen Einträge geprüft,
   inklusive der expliziten Gegenprobe, dass NTAG210/212 (anders als UL11/UL21) **kein**
   `ReadCounter` tragen, und dass `Unknown` exakt denselben `feature_set` hat wie der alte
   `Origin`-Fallback (`MfUltralightFeatureSupportCompatibleWrite`, keine sonstigen Bits).

Zusätzlich gegen die reale, unveränderte Basis zurückgeprüft: dieselbe Test-Datei kompiliert
gegen `mf_ultralight.h`/`.c` **ohne** diesen Patch überhaupt nicht (fehlende Enum-Werte
`MfUltralightTypeNTAG210`/`NTAG212`/`Unknown`) — der direktere, stärkere Beleg dafür, dass die
Suite nicht unabhängig vom eigentlichen Patch grün wird, den ein reiner Laufzeit-Fehlschlag
liefern könnte.

Zusätzlich geprüft (nur gelesen, nicht verändert): `mf_ultralight_is_counter_configured()`s
bestehendes `switch(data->type)` hat einen `default`-Zweig — die drei neuen Enum-Werte
erfordern dort keine Ergänzung, kompilieren ohne `-Wswitch`-Warnung (im Host-Build mangels
`-Wswitch`-Aktivierung ohnehin nicht sichtbar, hier durch Lesen des Codes bestätigt).

## Risiko

- **Niedrig für die Erkennung selbst.** Beide Änderungen sind auf Enum/Tabelle/eine Funktion
  begrenzt, ändern keine bestehende Datenstruktur-Größe, keine Signatur, keinen Include.
- **Ein echtes, von Unleashed selbst geerbtes Randrisiko, nicht durch diesen Patch neu
  eingeführt:** `mf_ultralight_get_write_end_page()` hat eine feste `furi_assert()`-Positivliste
  erlaubter Typen für den "auf Blanko-Karte schreiben"-Ablauf — NTAG210/212 stehen dort weder in
  meiner Basis noch im aktuellen Unleashed-`dev`-Stand drin (geprüft, siehe "Quelle"). Vor diesem
  Patch landete eine echte NTAG210/212-Karte ohnehin fälschlich bei UL11/UL21 (die *sind* in der
  Positivliste) — der Schreibablauf lief also (mit falschem Kartentyp) durch. **Nach** diesem
  Patch wird eine solche Karte korrekt als NTAG210/212 erkannt, aber genau deshalb würde
  `mf_ultralight_get_write_end_page()` bei einem Schreibversuch auf einen mit ihrem korrekten Typ
  gespeicherten Dump jetzt den `furi_assert()` auslösen (Absturz), da beide Typen dort fehlen.
  Da Unleashed selbst dieselbe Lücke hat, ist das keine Regression gegenüber der Portquelle,
  aber ein realer Verhaltensunterschied gegenüber meiner eigenen Vor-Patch-Basis: **lokal vor
  dem Einsatz prüfen, dass kein Schreib-Workflow einen NTAG210/212-Dump als Ziel anbietet** (in
  `applications/main/nfc` außerhalb des Scopes dieses Repos; per Sichtprüfung der App-Szenen
  lokal zu bestätigen). Bewusst nicht mitgeliefert: die Positivliste selbst um NTAG210/212 zu
  erweitern, ohne zu wissen, ob deren tatsächliche Schreib-Semantik (Sperren, Konfig-Seiten-Layout)
  dafür wirklich geeignet ist — das wäre eine über den reinen Port hinausgehende, eigenständige
  Annahme ohne Hardware-Beleg.
- **Patch-Reihenfolge, falls der separate UL-AES-Phase-2-Vorschlag ebenfalls lokal angewendet
  werden soll:** beide Vorschläge fügen ihre neuen Enum-Werte an derselben Stelle ein (Ende von
  `MfUltralightType`, vor `MfUltralightTypeNum`) und ihre Feature-Tabellen-Einträge an derselben
  Stelle (vor der schließenden `};`). Ausprobiert in beide Richtungen: der Header-Patch schlägt
  in beiden Reihenfolgen mit einem klaren `Hunk FAILED`-Fehler fehl (kein stiller Fehlschlag),
  der C-Patch wendet sich in beiden Reihenfolgen nur per Fuzzy-Offset an (`patch` verschiebt den
  Hunk automatisch, `Hunk succeeded ... with fuzz`/`with offset`) — das ist kein Fehler, aber
  eine manuelle Kontrolle wert. Sollen beide Vorschläge zusammen einfließen: zunächst nur einen
  der beiden `.patch`-Header anwenden, den zweiten Enum-Eintrag danach von Hand ergänzen (eine
  Zeile), C-Datei-Patches wie gewohnt anwenden und das Ergebnis gegen die jeweiligen
  Host-Repros beider Vorschläge laufen lassen.
- Commit-Metadaten von Unleashed waren wie bei den vorherigen UL-Ports nicht per API abrufbar
  (nur Rohdatei-Zugriff über `raw.githubusercontent.com`); geprüft gegen den aktuellen
  `dev`-Stand.

## Port-Skizze

Kein mehrphasiger Port — eine in sich abgeschlossene Typ-Erkennungs-Änderung in einer Funktion
plus drei neue, rein additive Feature-Tabellen-Einträge. War der letzte von vier beim
UL-AES-Phase-2-Port zurückgestellten Funden
(`firmware-mining/mfultralight-aes-phase2-struct-serialization.md`); mit diesem Patch sind alle
vier ursprünglich zurückgestellten Punkte abgearbeitet (`is_pwd_pack_read()`-Helfer,
`detect_protocol()`-ATQA-Gate, jetzt diese Typ-Erkennung — die vierte, `get_write_end_page()`s
Schreibbereichs-Anpassung, gehört wie schon vermerkt zum künftigen UL-AES-Schreibfluss, nicht zu
diesem eigenständigen Fund).

## Testhinweis (lokal)

1. Die zwei `.patch`-Dateien in `lib/nfc/protocols/mf_ultralight/` einbauen (`patch -p1 <
   mf_ultralight_h.patch` / `... mf_ultralight_c.patch`, aus dem Firmware-Wurzelverzeichnis).
   Falls der separate UL-AES-Phase-2-Vorschlag lokal bereits angewendet ist: siehe "Risiko" zur
   Patch-Reihenfolge, den Header-Konflikt von Hand auflösen (einen Enum-Wert ergänzen).
2. `ufbt` (bzw. `fbt firmware_all`) bauen — keine Signatur geändert, sollte ohne Warnungen
   durchlaufen.
3. Host-Repro laufen lassen (`gcc ... && ./repro`), `ALL CHECKS PASSED` erwarten.
4. Regressionstest am Gerät: bestehende NFC-Lese-Workflows für UL11/UL21/NTAG213/215/216/NTAG-I2C
   müssen sich durch diesen Patch in keiner Weise ändern (Testgruppen 1–4 decken das auf
   Logik-Ebene ab).
5. Falls vorhanden: eine echte NTAG210- oder NTAG212-Karte einlesen und prüfen, dass
   `nfc_scanner`/die App-Detailansicht jetzt "NTAG210"/"NTAG212" statt "Mifare Ultralight 11"/
   "Mifare Ultralight 21" anzeigt — das ist der eigentliche, am Gerät sichtbare Effekt dieses
   Patches. Ohne eine solche Karte zur Hand ist der Host-Test (Testgruppe 2/6) der einzige
   verfügbare Nachweis.
6. Vor produktivem Einsatz: den in "Risiko" beschriebenen Schreib-Workflow-Punkt lokal prüfen
   (keine App-Szene sollte einen NTAG210/212-Dump als Blanko-Schreibziel anbieten, solange
   `mf_ultralight_get_write_end_page()` diese Typen nicht in seiner Positivliste führt).

## Quelle

- Unleashed-Firmware, `dev`-Branch (`DarkFlippers/unleashed-firmware`,
  `lib/nfc/protocols/mf_ultralight/mf_ultralight.c`/`.h`), per `raw.githubusercontent.com`
  bezogen — GitHub-API-Zugriff für Commit-Metadaten war wie bei den vorherigen UL-Ports nicht
  verfügbar. Für den Risiko-Punkt zu `get_write_end_page()` zusätzlich denselben aktuellen Stand
  gegengeprüft (Positivliste enthält dort ebenfalls kein NTAG210/212).
- Meine eigene `G4MEOVER-FW`-Basis, `main`-Branch: `mf_ultralight.c`/`.h` (Portgrundlage),
  `mf_ultralight_poller.c` (nur gelesen, zur Bestätigung des einzigen Aufrufers von
  `mf_ultralight_get_write_end_page()`), `applications/main/nfc` (nur oberste Verzeichnisebene
  gesichtet, um zu bestätigen, dass die App-Szenen außerhalb des sparse geklonten Bereichs liegen
  — Ausgangspunkt für Testhinweis 6, nicht abschließend lokal geprüft).
- Bereits vermerkte Vorarbeit: `firmware-mining/mfultralight-aes-phase2-struct-serialization.md`
  (dort ursprünglich als einer von vier zurückgestellten Funden notiert),
  `firmware-mining/mf-ultralight-is-pwd-pack-read-helper.md`,
  `firmware-mining/mfultralight-detect-protocol-atqa-gate.md`.
