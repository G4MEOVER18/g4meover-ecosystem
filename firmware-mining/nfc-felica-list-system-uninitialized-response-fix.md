# NFC FeliCa: uninitialisierter Response-Pointer bei List-System-Code-Fehler — Official-dev vs. mntm-012

## Was
Ich habe den aus der letzten Firmware-Mining-Sitzung vorgemerkten Kandidaten `c9c77f66`
(gleicher `dev`-Durchlauf wie die bereits portierten PAC/Stanley-, ISO15693-, `bit_lib`- und
Pyramid-Fixes) aufgegriffen: PR #4428 (@munzzyy, gemerged 27.07.2026) behebt einen Bug im
**FeliCa-Poller** (`lib/nfc/protocols/felica/felica_poller.c`,
`felica_poller_state_handler_list_system()`), der bei mir in der mntm-012-Basis unverändert
vorhanden ist (gegengeprüft gegen Tag `mntm-012`, Datei identisch zum hier zitierten
Vorher-Stand).

```c
// mntm-012 (fehlerhaft):
NfcCommand felica_poller_state_handler_list_system(FelicaPoller* instance) {
    FURI_LOG_D(TAG, "List System");
    NfcCommand command = NfcCommandContinue;

    FelicaListSystemCodeCommandResponse* response_system_code;
    FelicaError error = felica_poller_list_system_code(instance, &response_system_code);

    instance->systems_total = response_system_code->system_count;   // <- vor Fehlerpruefung!
    simple_array_init(instance->data->systems, instance->systems_total);
    uint8_t* system_codes = response_system_code->system_code;

    for(uint8_t i = 0; i < instance->systems_total; i++) {
        FelicaSystem* system = simple_array_get(instance->data->systems, i);
        system->system_code = system_codes[i * 2] << 8 | system_codes[i * 2 + 1];
        system->system_code_idx = i;
    }

    if(error == FelicaErrorNone) {
        instance->state = FelicaPollerStateSelectSystemIndex;
    } else if(error != FelicaErrorTimeout) {
        instance->felica_event.type = FelicaPollerEventTypeError;
        instance->felica_event_data.error = error;
        instance->state = FelicaPollerStateReadFailed;
    }
    return command;
}
```

`response_system_code` ist eine **uninitialisierte lokale Pointer-Variable**.
`felica_poller_list_system_code()` (`lib/nfc/protocols/felica/felica_poller_i.c`, bei mir
ebenfalls unverändert) schreibt `*response_ptr` nur im Erfolgsfall:

```c
FelicaError felica_poller_list_system_code(
    FelicaPoller* instance,
    FelicaListSystemCodeCommandResponse** const response_ptr) {
    ...
    FelicaError error = felica_poller_frame_exchange(
        instance, instance->tx_buffer, instance->rx_buffer, FELICA_POLLER_POLLING_FWT);
    if(error != FelicaErrorNone) {
        FURI_LOG_E(TAG, "Request system code failed with error: %d", error);
        return error;                       // *response_ptr NICHT geschrieben
    }

    size_t rx_len = bit_buffer_get_size_bytes(instance->rx_buffer);
    if(rx_len < sizeof(FelicaCommandHeaderRaw) + 3) return FelicaErrorProtocol; // ebenso

    // error is known to be FelicaErrorNone here
    *response_ptr = (FelicaListSystemCodeCommandResponse*)bit_buffer_get_data(instance->rx_buffer);
    return error;
}
```

Bei **jedem** der beiden Fehlerpfade (Timeout/CRC-Fehler im `frame_exchange`, oder eine
Antwort kürzer als 13 Byte) bleibt `response_system_code` unverändert und wird trotzdem
sofort danach dereferenziert (`->system_count`), bevor `error` überhaupt geprüft wird. Da
`instance->auth.context.skip_auth` per Default `true` ist (Standard-FeliCa-Karten fordern
keine Authentifizierung), ist `List System` der reguläre erste Lese-Schritt bei jeder
Standard-FeliCa-Karte — ein schwacher Read, eine während des Lesens entfernte Karte, oder ein
Emulator/eine Karte, die `Request System Code` nicht beantwortet, lösen diesen Pfad aus. Genau
das ist der in Upstream-Issue #4314 gemeldete Absturz.

## Nutzen
- Schließt einen Absturz-Pfad, der bei **jedem** unvollständigen oder fehlgeschlagenen
  FeliCa-Lesevorgang auftreten kann — nicht nur bei einer präparierten Karte. FeliCa
  (Sony/Suica/Transit- und Zugangskarten in JP-Kontext, aber auch einige EU/US-NFC-Transit-
  Deployments) ist bei mir bereits Teil der NFC-Angriffsfläche, die ich mit den Access-Audit-/
  DESFire-/ISO15693-Funden in diesem Repo sukzessive abdecke.
- Winzige, rein additive Änderung (nur Einrückung/Verschiebung der bestehenden drei Zeilen in
  den `if`-Zweig) — kein API-Bruch, kein Verhaltensunterschied im Erfolgsfall.
- Direkt gegen meine mntm-012-Basis anwendbar, da die Datei seit dem mntm-012-Stand in dieser
  Hinsicht unverändert ist.

## Betroffene Dateien (lokal)
- `lib/nfc/protocols/felica/felica_poller.c`, Funktion
  `felica_poller_state_handler_list_system()`: die drei Zeilen
  ```c
  instance->systems_total = response_system_code->system_count;
  simple_array_init(instance->data->systems, instance->systems_total);
  uint8_t* system_codes = response_system_code->system_code;

  for(uint8_t i = 0; i < instance->systems_total; i++) {
      FelicaSystem* system = simple_array_get(instance->data->systems, i);
      system->system_code = system_codes[i * 2] << 8 | system_codes[i * 2 + 1];
      system->system_code_idx = i;
  }
  ```
  aus dem Funktionskörper heraus in den `if(error == FelicaErrorNone)`-Zweig verschieben,
  direkt vor `instance->state = FelicaPollerStateSelectSystemIndex;`. Kein neuer Include,
  kein neuer State, keine Signaturänderung.

## Risiko
- Sehr niedrig: reine Umsortierung bestehender Zeilen, keine neue Logik. Der Erfolgsfall
  (`error == FelicaErrorNone`) verhält sich identisch zu vorher, da genau derselbe Code jetzt
  nur eine Ebene tiefer im selben `if`-Zweig läuft, der vorher schon direkt danach folgte.
- Vor dem Übernehmen `FelicaListSystemCodeCommandResponse`-Layout und
  `felica_poller_list_system_code()`-Signatur in meiner mntm-012-Basis gegenlesen — bei mir
  identisch zum zitierten Stand (siehe Port-Skizze Schritt 2).
- Der Build (`fbt`/`ufbt`) deckt jede Signatur-/Strukturabweichung als Compile-Fehler auf.
- Funktionale Prüfung ohne Hardware nur eingeschränkt möglich: den Kontrollfluss-Bug (Zugriff
  vor Fehlerprüfung vs. danach) habe ich mit einem host-kompilierbaren Mini-Repro nachgestellt
  (siehe Testhinweis) — der eigentliche Bug-Pfad (echter Stack-Wert an dieser Stelle im
  realen Flipper-Build) braucht einen echten Test mit einer eigenen FeliCa-Karte bzw. einem
  gezielt unterbrochenen Lesevorgang, lokal, nicht Teil dieses Repos.

## Port-Skizze
1. `lib/nfc/protocols/felica/felica_poller.c::felica_poller_state_handler_list_system()` in
   meiner mntm-012-Basis öffnen.
2. Gegenlesen, ob `FelicaListSystemCodeCommandResponse` (Felder `system_count`,
   `system_code[]`) und die Signatur von `felica_poller_list_system_code()` weiterhin wie oben
   zitiert aussehen — falls Momentum den FeliCa-Stack strukturell verändert hat, Fix
   entsprechend anpassen statt blind zu übernehmen.
3. Die drei betroffenen Zeilen/den Block in den `if(error == FelicaErrorNone)`-Zweig
   verschieben (siehe „Betroffene Dateien" oben).
4. Host-seitiges Mini-Repro (`repro.c`, unten) mit `gcc -O2 -Wall -Wextra
   -fsanitize=address,undefined` bauen und laufen lassen, um den Kontrollfluss-Unterschied
   ALT/NEU zu bestätigen — bereits von mir lokal ausgeführt, siehe Testhinweis.
5. Build: `ufbt` (bzw. `fbt firmware_all`).
6. Funktionstest lokal: mit einer eigenen Standard-FeliCa-Karte einen normalen Lesevorgang
   mehrfach wiederholen (Erfolgsfall unverändert), danach gezielt einen unvollständigen
   Lesevorgang provozieren (Karte während der Anfrage vom Feld entfernen, oder aus größerer
   Distanz lesen) und prüfen, dass kein Geräteabsturz/-neustart mehr auftritt, wo er vorher
   auftrat.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`) des Kontrollflusses aus
`felica_poller_state_handler_list_system()` plus einem Mock von
`felica_poller_list_system_code()`, der — wie das Original — `*response_ptr` nur im
Erfolgsfall schreibt. Um den Bug deterministisch und portabel zu zeigen (ohne auf einen
zufälligen, ggf. zufällig gültigen Stack-Wert angewiesen zu sein), wird die uninitialisiert
bleibende Pointer-Variable bewusst auf eine garantiert ungültige Adresse (`0x1`) gesetzt — das
deckt den in der Praxis schlimmsten (aber keineswegs unrealistischen) Fall ab; ein echter
Stack-Rest kann ebenso gut eine zufällig gültige Adresse mit Fremdinhalt sein und dann *ohne*
Absturz falsche `systems_total`/`system_code`-Werte liefern, was in der Praxis schwerer zu
erkennen wäre als ein klarer Crash. Gebaut/ausgeführt mit
`gcc -O2 -Wall -Wextra -fsanitize=address,undefined` (jeder Handler-Aufruf läuft isoliert in
einem `fork()`-Kindprozess, damit ein SEGV den Testlauf selbst nicht abbricht):

```
== Fall: FelicaErrorNone (Erfolg) ==
  ALT (mntm-012): kein Absturz
  NEU (Fix):      kein Absturz

== Fall: FelicaErrorTimeout ==
  ALT (mntm-012): ABSTURZ (SEGV bei Dereferenzierung vor Fehlerpruefung)
  NEU (Fix):      kein Absturz

== Fall: FelicaErrorProtocol (zu kurze Antwort) ==
  ALT (mntm-012): ABSTURZ (SEGV bei Dereferenzierung vor Fehlerpruefung)
  NEU (Fix):      kein Absturz

ERGEBNIS: bestaetigt -- ALT stuerzt bei jedem Fehlerpfad ab, NEU nicht.
```

Bestätigt: die ALT-Version dereferenziert den Response-Pointer bei beiden Fehlerpfaden
unbedingt und stürzt bei einer ungültigen Adresse zuverlässig ab; die NEU-Version (Fix
verschoben in den Erfolgszweig) tut das in keinem der drei Fälle.

## Quelle
- `flipperdevices/flipperzero-firmware`, `dev`-Branch, PR #4428 (@munzzyy, gemerged
  27.07.2026): „NFC FeliCa: check the poller error before reading the system-code response"
  — `lib/nfc/protocols/felica/felica_poller.c`. Referenziert Upstream-Issue #4314 als
  ursprünglichen Crash-Report.
- Momentum-Firmware, Tag `mntm-012`: `lib/nfc/protocols/felica/felica_poller.c` und
  `lib/nfc/protocols/felica/felica_poller_i.c` als Vergleichsbasis (Fix dort noch nicht
  vorhanden, Code identisch zum hier zitierten Vorher-Stand).
- Weiterhin aus demselben Durchlauf offen (vorgemerkt seit PR #4427): `5dc71870` (Infrared:
  `timings_cnt` bei Decoder-Alloc uninitialisiert plus fehlerhafte Bounds-Prüfung mit
  `sizeof()` statt `COUNT_OF()` — geprüft und ebenfalls bei mir vorhanden, für eine künftige
  Sitzung vorgemerkt). Danach weiter ab `9148d50c` (2026-08-04) gegenprüfen.
