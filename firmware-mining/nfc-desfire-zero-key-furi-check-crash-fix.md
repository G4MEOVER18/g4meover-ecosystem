# NFC DESFire: furi_check-Absturz bei Applikation/PICC mit MaxKeys=0 — Official-dev vs. mntm-012

## Was
Ich habe den `dev`-Branch weiter ab `9148d50c` (letzter geprüfter Stand aus der vorherigen
Firmware-Mining-Sitzung) gegengeprüft: seitdem ist genau ein neuer Commit hinzugekommen,
`a55e3939` / PR #4439 (@jcz0, gemerged 17.08.2026, "Fix crash when reading card with a
zero-key application", schließt Upstream-Issue #3835). Der Fix betrifft den **DESFire-Stack**
(`lib/nfc/protocols/mf_desfire/`) und ist bei mir in der mntm-012-Basis unverändert vorhanden
(gegengeprüft gegen Tag `mntm-012`, beide betroffenen Dateien identisch zum hier zitierten
Vorher-Stand).

Kern des Bugs: `simple_array_init()` (`lib/toolbox/simple_array.c`) hat selbst intern
`furi_check(count > 0)` — ein `furi_check`-Fehlschlag hält das Gerät sofort an
(Absturz/Neustart), dasselbe Absturzmuster wie bereits bei den ISO15693- und
`bit_lib`-Funden aus früheren Sitzungen. Zwei Aufrufer reichen eine Elementanzahl durch, die
direkt von der Karte bzw. aus einem Dump stammt und legitim `0` sein kann:

```c
// mntm-012, lib/nfc/protocols/mf_desfire/mf_desfire_poller_i.c (fehlerhaft):
MfDesfireError mf_desfire_poller_read_key_versions(
    MfDesfirePoller* instance,
    SimpleArray* data,
    uint32_t count) {
    furi_check(instance);
    furi_check(count > 0);           // <- stuerzt bei count==0 direkt hier ab

    simple_array_init(data, count);  // <- wuerde auch hier abstuerzen (interner Check)
    ...
}
```

```c
// mntm-012, lib/nfc/protocols/mf_desfire/mf_desfire.c, mf_desfire_load() (fehlerhaft):
const uint32_t master_key_version_count = data->master_key_settings.max_keys;
simple_array_init(data->master_key_versions, master_key_version_count);  // kein Guard
```

`mf_desfire_poller_read_key_versions()` wird über zwei Pfade mit einem Karten-/Dump-Wert
aufgerufen, der `0` sein kann:
- **Live-Lesevorgang**, pro Applikation: `mf_desfire_poller_read_application()`
  (`mf_desfire_poller_i.c`) ruft es mit `data->key_settings.max_keys` auf — dem
  `KeySettings2`-Byte (low nibble) der jeweiligen DESFire-Applikation, direkt von der Karte.
  Eine reale Applikation mit `MaxKeys == 0` (laut Upstream-Issue #3835 auf einer echten Karte
  beobachtet, ist im DESFire-Wertebereich gültig) löst den `furi_check`-Abbruch bei **jedem**
  regulären Lesevorgang dieser Karte aus, ohne präparierten Dump.
- **Live-Lesevorgang**, PICC-Master-Keys: `mf_desfire_poller_handler_read_master_key_settings()`
  (`mf_desfire_poller.c`) ruft dieselbe Funktion mit `master_key_settings.max_keys` auf — trifft
  jede Karte, deren PICC-Ebene `MaxKeys == 0` meldet.

`mf_desfire_load()` (Laden eines gespeicherten `.nfc`-Dumps von SD, u. a. beim Emulieren
einer gespeicherten Karte) trifft denselben internen Check in `simple_array_init()` für den
Master-Key-Zweig, sobald ein Dump `Mifare DESFire Master Key Settings` mit `Max Keys: 0`
enthält — sei es, weil er von genau so einer Karte gespeichert wurde, oder weil er (versehentlich
oder absichtlich) editiert wurde.

## Nutzen
- Schließt einen echten Geräteabsturz beim ganz normalen Lesen einer DESFire-Karte, nicht nur
  bei einem präparierten Dump — passt in dieselbe Kategorie wie die bereits portierten
  ISO15693-Parser- und FeliCa-Response-Absturzfunde: ein Standard-Lesevorgang kann das Gerät
  anhalten, ohne dass ich aktiv etwas Ungewöhnliches tue.
- DESFire ist bei mir bereits über den `nfc-desfire-mful-stack-overflow-fixes.md`-Port aktiv
  im Fokus (Access-Audit-/Zutrittskontroll-Kontext, siehe `access-audit-app.md`) — derselbe
  Kartentyp, zusätzlicher Härtungsschritt am selben Stack.
- Minimal-invasiv: reine Guard-Ergänzung an zwei bestehenden Aufrufstellen, keine
  Signaturänderung, kein neues Feld, kein API-Bruch. Der Erfolgsfall (`count > 0`) verhält
  sich exakt wie vorher.
- Betrifft sowohl den Live-Lesepfad als auch den Dump-Lade-/Emulationspfad — ein Fix deckt
  beide ab, da beide über denselben `simple_array_init()`-Aufruf abstürzen.

## Betroffene Dateien (lokal)
- `lib/nfc/protocols/mf_desfire/mf_desfire_poller_i.c`,
  `mf_desfire_poller_read_key_versions()`: das feste `furi_check(count > 0);` durch einen
  Guard ersetzen —
  ```c
  if(count > 0) {
      simple_array_init(data, count);
  } else {
      FURI_LOG_W(TAG, "Application reports zero keys, key versions skipped");
  }
  ```
  (der bisherige `simple_array_init(data, count);`-Aufruf direkt danach entfällt, wandert in
  den `if`-Zweig).
- `lib/nfc/protocols/mf_desfire/mf_desfire.c`, `mf_desfire_load()`, Master-Key-Zweig: denselben
  Guard um den bestehenden `simple_array_init(data->master_key_versions,
  master_key_version_count);`-Aufruf legen, `else`-Zweig loggt
  `"PICC reports zero master keys, key versions skipped"`. Zusätzlich `#define TAG
  "MfDesfire"` am Dateianfang ergänzen (für den neuen `FURI_LOG_W`-Aufruf, ist in dieser Datei
  bei mir noch nicht vorhanden).
- Keine Änderung an `mf_desfire_poller.c` oder anderen Aufrufstellen nötig — beide betroffenen
  Call-Sites (Live-Applikation, Live-PICC-Master-Keys, Dump-Master-Keys) laufen durch die
  beiden oben genannten Funktionen.

## Risiko
- Sehr niedrig: additiver Guard um bereits bestehende Aufrufe, keine neue Datenstruktur, keine
  Änderung der Erfolgspfad-Semantik. Der leere `SimpleArray` bleibt im `count==0`-Fall
  ungesetzt/leer statt allokiert — nachgelagerter Code, der über die Array-Länge iteriert
  (`simple_array_get_count()`), sieht dann korrekt `0` Elemente statt eines Absturzes.
- Vor dem Übernehmen `mf_desfire_poller_read_key_versions()`-Signatur sowie den
  `mf_desfire_load()`-Kontrollfluss in meiner mntm-012-Basis gegenlesen — bei mir identisch
  zum zitierten Stand (siehe Port-Skizze Schritt 2).
- Der Build (`fbt`/`ufbt`) deckt jede Signatur-/Strukturabweichung als Compile-Fehler auf.
- Funktionale Prüfung ohne Hardware nur eingeschränkt möglich: den `furi_check`-Kontrollfluss
  (Absturz bei `count==0` vs. Guard) habe ich mit einem host-kompilierbaren Mini-Repro
  nachgestellt (siehe Testhinweis) — der eigentliche Bug-Pfad (eine reale Karte mit
  `MaxKeys == 0`, bzw. ein entsprechend präparierter `.nfc`-Dump) braucht einen echten Test,
  lokal, nicht Teil dieses Repos.

## Port-Skizze
1. `lib/nfc/protocols/mf_desfire/mf_desfire_poller_i.c::mf_desfire_poller_read_key_versions()`
   und `lib/nfc/protocols/mf_desfire/mf_desfire.c::mf_desfire_load()` in meiner mntm-012-Basis
   öffnen.
2. Gegenlesen, ob beide Funktionen weiterhin wie oben zitiert aussehen (insbesondere
   `SimpleArray`/`simple_array_init()`-Signatur aus `lib/toolbox/simple_array.h`) — falls
   Momentum den DESFire- oder `simple_array`-Code strukturell verändert hat, Fix entsprechend
   anpassen statt blind zu übernehmen.
3. Die beiden Guards gemäß „Betroffene Dateien" einsetzen, inkl. `#define TAG "MfDesfire"` in
   `mf_desfire.c`, falls dort noch nicht vorhanden.
4. Host-seitiges Mini-Repro (`repro.c`, unten) mit
   `gcc -O2 -Wall -Wextra -fsanitize=address,undefined` bauen und laufen lassen, um den
   `furi_check`-Kontrollfluss-Unterschied ALT/NEU zu bestätigen — bereits von mir lokal
   ausgeführt, siehe Testhinweis.
5. Build: `ufbt` (bzw. `fbt firmware_all`).
6. Funktionstest lokal: eine eigene DESFire-Karte normal lesen (Erfolgsfall unverändert,
   `MaxKeys > 0` auf allen eigenen Testkarten voraussichtlich der Regelfall). Für den
   eigentlichen Bug-Pfad einen `.nfc`-Dump einer eigenen DESFire-Karte im Dateimanager auf
   `Max Keys: 0` (PICC- und/oder Applikationsebene) editieren und laden/emulieren — vorher
   Geräteabsturz erwartet, nachher sauberes Laden mit leerer Key-Versions-Liste.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`) des `furi_check`-Kontrollflusses aus
`mf_desfire_poller_read_key_versions()` (Live-Pfad) und dem Master-Key-Zweig von
`mf_desfire_load()` (Dump-Lade-Pfad), inklusive einer Nachbildung des internen
`furi_check(count > 0)` in `simple_array_init()` selbst. Gebaut/ausgeführt mit
`gcc -O2 -Wall -Wextra -fsanitize=address,undefined` (jeder Pfad läuft isoliert in einem
`fork()`-Kindprozess, damit ein `abort()` den Testlauf selbst nicht abbricht):

```
== Poller: Applikation MaxKeys=0 (count=0) ==
  ALT (mntm-012): ABSTURZ (furi_check-Abbruch)
  NEU (Fix):      kein Absturz

== Poller: Applikation MaxKeys=3 (Sanity) (count=3) ==
  ALT (mntm-012): kein Absturz
  NEU (Fix):      kein Absturz

== Load: PICC MasterKeys=0 (count=0) ==
  ALT (mntm-012): ABSTURZ (furi_check-Abbruch)
  NEU (Fix):      kein Absturz

== Load: PICC MasterKeys=1 (Sanity) (count=1) ==
  ALT (mntm-012): kein Absturz
  NEU (Fix):      kein Absturz

ERGEBNIS: bestaetigt -- ALT stuerzt bei count==0 auf beiden Pfaden ab, NEU nicht.
```

Bestätigt: die ALT-Version stürzt auf beiden Aufrufpfaden (Live-Applikation/-PICC sowie
Dump-Laden) bei `count==0` zuverlässig über `furi_check` ab; die NEU-Version (Guard vor dem
`simple_array_init()`-Aufruf) tut das in keinem der vier Fälle, während der
Sanity-Fall (`count > 0`) auf beiden Seiten unverändert bleibt.

## Quelle
- `flipperdevices/flipperzero-firmware`, `dev`-Branch, Commit `a55e3939` / PR #4439 (@jcz0,
  gemerged 17.08.2026): „NFC DESFire: Fix crash when reading card with a zero-key
  application" — `lib/nfc/protocols/mf_desfire/mf_desfire.c` und
  `lib/nfc/protocols/mf_desfire/mf_desfire_poller_i.c`. Referenziert Upstream-Issue #3835 als
  ursprünglichen Crash-Report.
- Momentum-Firmware, Tag `mntm-012`: beide Dateien als Vergleichsbasis (Fix dort noch nicht
  vorhanden, Code identisch zum hier zitierten Vorher-Stand).
- `dev` steht nach diesem Commit weiterhin exakt auf `a55e3939` (kein weiterer neuer Commit
  seit `9148d50c`) — als neuer Startpunkt für die nächste Gegenprüfung vormerken, sobald
  weitere `dev`-Commits vorliegen.
