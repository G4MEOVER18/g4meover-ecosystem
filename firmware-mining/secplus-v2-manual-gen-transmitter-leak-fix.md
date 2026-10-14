# SubGHz "Add Manually": `subghz_txrx_gen_secplus_v2_protocol()` gibt den Wegwerf-Transmitter nie wieder frei

## Was

Ausgangspunkt war der Unleashed-`dev`-Branch-Diff gegen meinen zuletzt geprüften Stand
(`af2e764`): ein neuer Commit, "SubGHz: safe exit on unknown preset, fix Security+ 2.0
transmitter leak" (schließt Unleashed-Issues #1159/#1161), acht Dateien, zwei eigenständige
Fixes gebündelt. Den größeren der beiden (fünf Dateien, API-Änderung von `furi_crash()` auf
einen `-1`-Rückgabewert über mehrere Aufrufer hinweg, `subghz_setting_get_inx_preset_by_name()`
und ihre Aufrufer) für eine künftige Sitzung zurückgestellt — zu groß für einen einzelnen,
abgeschlossenen Beitrag. Den zweiten, vollständig eigenständigen Fix (eine Zeile, eine Datei)
aufgegriffen: der "Security+ 2.0 transmitter leak".

`applications/main/subghz/helpers/subghz_txrx_create_protocol_key.c` enthält acht
`subghz_txrx_gen_*_protocol()`-Funktionen, die der "Add Manually"-Dialog für Rolling-Code-
Protokolle (KeeLoq, KeeLoq BFT, Nice FlorS, FAAC SLH, Alutech AT-4N, CAME Atomo, Somfy Telis,
Security+ 2.0, Phoenix V2) aufruft, um aus Seriennummer/Taste/Zähler einen sendefertigen
Schlüssel zu erzeugen. Jede dieser Funktionen alloziert dafür einen reinen Wegwerf-
`SubGhzTransmitter` (`instance->transmitter`), nutzt ihn nur, um `instance->fff_data` zu
befüllen, und gibt ihn danach wieder frei — belegt für alle sieben anderen Protokolle anhand
des exakt gleichen Musters (`subghz_transmitter_alloc_init()` … `subghz_transmitter_free()`
unmittelbar vor dem jeweiligen `return`):

```c
// z. B. subghz_txrx_gen_phoenix_v2_protocol() (dieselbe Datei) -- das durchgehende Muster
txrx->transmitter = subghz_transmitter_alloc_init(txrx->environment, SUBGHZ_PROTOCOL_PHOENIX_V2_NAME);
if(txrx->transmitter && subghz_protocol_phoenix_v2_create_data(...)) {
    ret = true;
}
subghz_transmitter_free(txrx->transmitter);
return ret;
```

`subghz_txrx_gen_secplus_v2_protocol()` ist bei mir (mntm-012, byte-identisch zum
Unleashed-Vorher-Stand verifiziert) die einzige Ausnahme von diesem Muster:

```c
// applications/main/subghz/helpers/subghz_txrx_create_protocol_key.c (mntm-012, unverändert)
bool subghz_txrx_gen_secplus_v2_protocol(
    SubGhzTxRx* instance, const char* name_preset, uint32_t frequency,
    uint32_t serial, uint8_t btn, uint32_t cnt) {
    furi_assert(instance);

    bool ret = false;
    instance->transmitter =
        subghz_transmitter_alloc_init(instance->environment, SUBGHZ_PROTOCOL_SECPLUS_V2_NAME);
    subghz_txrx_set_preset(instance, name_preset, frequency, NAN, NAN, NULL, 0);
    if(instance->transmitter) {
        subghz_protocol_secplus_v2_create_data(
            subghz_transmitter_get_protocol_instance(instance->transmitter),
            instance->fff_data, serial, btn, cnt, instance->preset);
        ret = true;
    }
    return ret;   // <-- kein subghz_transmitter_free(instance->transmitter)
}
```

Aufgerufen wird sie aus `subghz_scene_set_counter.c` und `subghz_scene_set_type.c` — jedes Mal,
wenn der Nutzer im "Add Manually"-Dialog für Security+ 2.0 den Zähler bzw. den Protokolltyp
bestätigt. Die `SubGhzTxRx`-Instanz (`subghz->txrx`) ist für die gesamte Lebensdauer der
SubGHz-App langlebig; jeder erneute Aufruf dieser Funktion überschreibt `instance->transmitter`
unmittelbar mit einer frischen Allokation, bevor die vorherige je freigegeben wurde — sie wird
dabei unerreichbar, nicht nur vorübergehend ungenutzt. Das trifft nicht nur den einmaligen
Pfad ("Add Manually" einmal durchlaufen → Sichern), sondern jeden wiederholten Durchlauf ohne
die SubGHz-App zu verlassen (z. B. "Zurück" zu "Set Type", erneut einen anderen Zähler/Button
eingeben, erneut bestätigen — ein normaler, nicht fehlerhafter Bedienpfad). Auf dem
speicherknappen STM32WB55 (kein virtueller Speicher, kleiner Heap) summiert sich das bei
wiederholter Nutzung zu echtem Speicherdruck.

Ich habe geprüft, ob `instance->transmitter` an anderer Stelle in der SubGHz-App beim Verlassen
noch aufgeräumt wird (`subghz_txrx_free()`, der zentrale Teardown beim App-Exit) — er tut es
nicht: `subghz_txrx_free()` fasst `instance->transmitter` überhaupt nicht an. Das ist ein
bereits bestehender, von diesem Fix unabhängiger Umstand (betrifft die zuletzt vom
Sende-/Empfangspfad gehaltene Transmitter-Referenz beim App-Exit, nicht Teil des
Unleashed-Fixes) — der hier behobene Fund ist eigenständig davon: er betrifft ausschließlich
die (n-1) Zwischen-Allokationen bei n Aufrufen *innerhalb* einer laufenden App-Sitzung, die
ohne diesen Fix bei jedem erneuten Aufruf zusätzlich zur letzten verloren gehen.

## Nutzen

Schließt einen echten, bei normaler Bedienung wiederholt auslösbaren Heap-Leak im "Add
Manually"-Dialog für Security+ 2.0 — dem am häufigsten genutzten Rolling-Code-Protokoll in
diesem Dialog (Garagentor-/Torantriebe vieler US-Hersteller). Rein additiv, gleicht die
Funktion exakt an das in derselben Datei bereits etablierte, von den sieben Schwesterfunktionen
konsequent eingehaltene Alloc/Free-Muster an.

## Betroffene Dateien

- `applications/main/subghz/helpers/subghz_txrx_create_protocol_key.c` —
  `subghz_txrx_gen_secplus_v2_protocol()`, eine neue Zeile direkt vor dem bestehenden
  `return ret;`.

## Risiko

Minimal. Eine einzelne, rein additive Zeile (`subghz_transmitter_free(instance->transmitter);`),
kein geänderter Kontrollfluss, keine geänderte Funktionssignatur, keine andere Codestelle
betroffen. `instance->transmitter` ist an dieser Stelle garantiert nicht `NULL` (der
`subghz_transmitter_free()`-Aufruf steht nach dem `if(instance->transmitter)`-Block, exakt wie
bei allen sieben Schwesterfunktionen, die dieselbe unbedingte Free-Anweisung ohne zusätzlichen
NULL-Schutz direkt vor ihrem `return` verwenden). `instance->transmitter` wird danach nicht auf
`NULL` zurückgesetzt — ebenfalls identisch zum Verhalten aller sieben Schwesterfunktionen, die
dasselbe Zwischenspeicher-Feld für denselben Wegwerf-Zweck nutzen; kein neues Verhalten, keine
Verschlechterung gegenüber dem bereits etablierten Zustand dieser Datei.

## Port-Skizze

1. `firmware-mining/secplus-v2-manual-gen-transmitter-leak-fix/subghz_txrx_create_protocol_key_c.patch`
   gegen die eigene `applications/main/subghz/helpers/subghz_txrx_create_protocol_key.c`
   anwenden (`git apply --check` vorab, dann real).
2. Mit `ufbt`/`fbt firmware_all` bauen.
3. Funktionstest (siehe unten).

## Testhinweis

Lokal, ohne Hardware verifizierbar als reine Logikprüfung (siehe Host-Repro unten) — zusätzlich
auf dem Gerät: SubGHz-App öffnen, "Add Manually" → "Security+ 2.0", eine beliebige
Seriennummer/Taste/Zähler eingeben, bestätigen (speichert oder sendet). Danach erneut "Add
Manually" → "Security+ 2.0" mit anderen Werten wiederholen, mehrfach hintereinander, ohne die
SubGHz-App zwischendurch zu verlassen. Vor dem Fix bleibt bei jedem Durchlauf ein Transmitter-
Objekt verloren (nicht direkt am Gerät beobachtbar ohne Heap-Statistik-Tooling, siehe Host-Repro
für den quantitativen Nachweis); nach dem Fix keiner. Funktional (Schlüssel wird weiterhin
korrekt erzeugt, gespeichert, gesendet) unverändert vor/nach dem Fix in beiden Fällen.

## Host-Repro

`firmware-mining/repro-secplus-v2-manual-gen-transmitter-leak-fix/repro.c`: bildet
`subghz_txrx_gen_secplus_v2_protocol()` sowohl ALT (unverändert) als auch NEU (mit dem
portierten Fix) nach, mit echten `malloc()`/`free()`-Wrappern um einen Platzhalter anstelle des
echten `SubGhzTransmitter` (auf dem Host nicht verfügbar). Simuliert *n* aufeinanderfolgende
"Add Manually → Security+ 2.0 → Zähler bestätigen"-Durchläufe gegen dieselbe langlebige
`SubGhzTxRx`-Instanz.

```
gcc -std=c11 -O1 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=undefined -o repro repro.c
./repro 5 alt   # ALT
./repro 5 neu   # NEU
```

Ergebnis (gegen zwei frische, unabhängige Kopien von `g4meover-ecosystem/G4MEOVER-FW`
patch-verifiziert, Repro separat unter `-O0` und `-O1` ausgeführt): ALT meldet bei 5 simulierten
Durchläufen zuverlässig `LeakSanitizer: detected memory leaks` — "Direct leak of 20 byte(s) in 5
object(s)" (Exit-Code 1), NEU beendet sich in beiden Optimierungsstufen sauber ohne jede Meldung
(Exit-Code 0). Keine Compiler-/Sanitizer-Warnungen in beiden Versionen.

## Offen / zurückgestellt

- Der zweite, größere Teil desselben Upstream-Commits ("safe exit on unknown preset": ändert
  `subghz_setting_get_inx_preset_by_name()` von `furi_crash()` auf einen `-1`-Rückgabewert,
  passt fünf Dateien/Aufrufer entsprechend an — `lib/subghz/subghz_setting.c`/`.h`,
  `applications/main/subghz/helpers/subghz_txrx.c`, `subghz_i.c`,
  `scenes/subghz_scene_receiver_config.c`) bleibt eigener Rückstellpunkt für eine künftige
  Sitzung. Gegen meine `mntm-012`-Basis geprüft: alle fünf Dateien existieren, die betroffenen
  Funktionen (`subghz_txrx_tx_start()`, `subghz_txrx_rx_start()`, `subghz_key_load()`,
  `subghz_scene_receiver_config_var_list_enter_callback()`) sind byte-identisch zum
  Unleashed-Vorher-Stand — grundsätzlich portierbar, aber mehrteilig genug (API-Änderung über
  mehrere Aufrufer hinweg) für einen eigenen, vollständigen Beitrag statt eines Nebenprodukts
  dieser Sitzung.
- Die übrigen seit längerem zurückgestellten Backlog-Punkte (NFC/RFID-Parser-Teilaspekt,
  MIFARE-Plus-/Ultralight-AES Phase 3 Rest, PSA-Bruteforce-Krypto-Konstanten,
  `sanitize_filename()`-Zeichenbereichs-Nebenfund, `usb-army-penetrator`s restliche
  Ducky-Handler, `texkom`/`hitagmicro.c` aus dem Unleashed-dev-Diff, Hitag-S-Phase-2-
  Integrationsschicht, neun verbleibende FeliCa-Multisystem-Teilfixe aus PR #4465,
  `g4meover-deck`s `flipper_link.py`-Bestätigung) bleiben unverändert offen.

## Quelle

Unleashed-Firmware (`DarkFlippers/unleashed-firmware`), `dev`-Branch, Commit `e1ebaa3`
("SubGHz: safe exit on unknown preset, fix Security+ 2.0 transmitter leak", schließt Issues
#1159/#1161).
