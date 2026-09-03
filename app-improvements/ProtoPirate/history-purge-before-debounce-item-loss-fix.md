# ProtoPirate — Auto-Purge verdrängt bei vollem Verlauf einen zweiten Eintrag pro Tastendruck

## Was
`protopirate_history_add_to_history()` (`protopirate_history.c`) prüft die Verlaufsgröße/
Auto-Purge-Eviction **vor** der Duplikat-Entprellung:

```c
bool protopirate_history_add_to_history(
    ProtoPirateHistory* instance,
    void* context,
    SubGhzRadioPreset* preset) {
    ...
    if(ProtoPirateHistoryItemArray_size(instance->data) >= instance->history_limit) {
        if(instance->history_auto_purge) {
            protopirate_history_delete_item(instance, 0);
        } else {
            return false;
        }
    }

    SubGhzProtocolDecoderBase* decoder_base = context;

    if((instance->code_last_hash_data ==
        subghz_protocol_decoder_base_get_hash_data(decoder_base)) &&
       ((furi_get_tick() - instance->last_update_timestamp) < 500)) {
        instance->last_update_timestamp = furi_get_tick();
        return false;
    }

    protopirate_history_release_scratch(instance);
    ...
```

Ein Fernbedienungs-Taster sendet dieselbe Bitfolge während der gesamten Druckdauer mehrfach
hintereinander; jede vollständig dekodierte Wiederholung löst bei mir einen eigenen
`protopirate_history_add_to_history()`-Aufruf über den SubGHz-Receiver-Callback aus. Genau dafür
existiert die zweite Prüfung — dieselbe `code_last_hash_data` innerhalb von 500 ms wird als
Wiederholung erkannt und verworfen, ohne einen neuen Eintrag anzulegen.

Ist der Verlauf zum Zeitpunkt des *ersten* wirklich neuen Signals bereits voll und Auto-Purge
aktiviert, läuft das so ab:
1. Erstes (neues) Signal: Limit erreicht → ältester Eintrag wird verdrängt (`count` sinkt auf
   `limit - 1`) → Entprellung greift nicht (anderer Hash) → neuer Eintrag wird hinzugefügt
   (`count` wieder `== limit`). Korrekt — genau ein alter Eintrag wurde für einen neuen
   verdrängt.
2. Dieselbe Bitfolge wird (typisch, oft schon binnen weniger Millisekunden) erneut dekodiert:
   der Verlauf ist durch Schritt 1 wieder voll, die Limit-Prüfung greift **ein zweites Mal** und
   verdrängt einen weiteren ältesten Eintrag — **bevor** die anschließende Entprellung erkennt,
   dass es sich um dieselbe Signatur handelt und gar nichts hinzugefügt wird.

Ergebnis: ein einzelner Tastendruck bei vollem Verlauf verdrängt **zwei** echte, unabhängige
Captures, obwohl nur ein einziger neuer Eintrag entstanden ist. Die zweite Verdrängung trifft
einen Eintrag, der mit dem gerade eingetroffenen Signal nichts zu tun hat und sonst erhalten
geblieben wäre.

## Nutzen
- Betrifft ausschließlich die Kombination "Verlauf voll" + "Auto-Purge an" (Einstellung in
  `protopirate_scene_receiver_config.c`, Standard: aus) — genau der Modus, den ich für längere
  Erfassungssitzungen ohne manuelles Aufräumen aktiviere, und in dem der Verlust am wenigsten
  auffällt, weil der Verlauf ohnehin laufend rotiert.
- Der Verlust ist real und dauerhaft: `protopirate_history_delete_item()` löscht auch die
  zugehörige Capture-Datei von der SD-Karte (`protopirate_history_delete_capture_file()`), nicht
  nur den Verlaufseintrag im Speicher.
- Reiner Buchhaltungsfehler in der Reihenfolge zweier bereits vorhandener Prüfungen — kein
  Speicherzugriffsfehler, aber ein stiller Datenverlust ohne jede Fehlermeldung.

## Betroffene Dateien
- `protopirate_history.c` — nur `protopirate_history_add_to_history()`.

## Patch
```c
bool protopirate_history_add_to_history(
    ProtoPirateHistory* instance,
    void* context,
    SubGhzRadioPreset* preset) {
    furi_check(instance);
    furi_check(context);

    SubGhzProtocolDecoderBase* decoder_base = context;

    if((instance->code_last_hash_data ==
        subghz_protocol_decoder_base_get_hash_data(decoder_base)) &&
       ((furi_get_tick() - instance->last_update_timestamp) < 500)) {
        instance->last_update_timestamp = furi_get_tick();
        return false;
    }

    if(ProtoPirateHistoryItemArray_size(instance->data) >= instance->history_limit) {
        if(instance->history_auto_purge) {
            protopirate_history_delete_item(instance, 0);
        } else {
            return false;
        }
    }

    protopirate_history_release_scratch(instance);
    ...
```

Einzige Änderung: die Duplikat-Entprellung läuft jetzt **vor** der Limit-Prüfung/Eviction,
`SubGhzProtocolDecoderBase* decoder_base = context;` wandert dafür an den Anfang der Funktion
(wird jetzt vor statt nach dem ersten Block gebraucht). Der restliche Funktionskörper (ab
`protopirate_history_release_scratch(instance);`) bleibt unverändert.

## Testhinweis (ohne Hardware)
Reine Buchhaltungslogik (Reihenfolge zweier bestehender Prüfungen), host-nachvollziehbar ohne
`FlipperFormat`/Storage-/Decoder-Abhängigkeit. Reproduktionsprogramm in diesem Ordner
(`repro-history-purge-before-debounce/repro.c`) bildet die Kontrollstruktur von
`protopirate_history_add_to_history()` vor und nach dem Fix nach (Limit-Prüfung+Eviction,
Entprellung, Hinzufügen — reduziert auf die für den Bug relevante Größen-/Reihenfolge-
Buchhaltung, Storage-I/O ist für den Fehler irrelevant):

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
```

Ergebnis: bei vollem Verlauf mit Auto-Purge verdrängt ALT für ein einzelnes neu aufgenommenes
Signal zwei Einträge, sobald dieses Signal innerhalb von 500 ms erneut dekodiert wird; NEU
verdrängt in derselben Situation nur den einen Eintrag, der für den einen tatsächlich neuen
Eintrag nötig ist, und bleibt bei weiteren Wiederholungen unverändert.

Funktionaler Test danach lokal auf dem Gerät: Verlaufslimit auf 5 stellen, Auto-Purge
aktivieren, den Verlauf mit fünf unterschiedlichen Signalen füllen, dann einen Taster einer
sechsten, neuen Fernbedienung für eine volle Sekunde gedrückt halten (mehrere Wiederholungen
derselben Bitfolge) und anschließend den Verlauf zählen — vor dem Fix fehlen zwei ältere
Einträge statt einem, danach genau einer.

## Risiko
- Sehr niedrig: vertauscht nur die Reihenfolge zweier bereits vorhandenen, unabhängigen
  Prüfungen — keine neue Bedingung, kein entfernter Pfad.
- Verhalten für alle anderen Fälle unverändert: Verlauf nicht voll (Entprellung entscheidet wie
  bisher, Limit-Prüfung greift ohnehin nicht), Verlauf voll ohne Auto-Purge (führt in beiden
  Reihenfolgen zum selben `return false`, da die Entprellung an dieser Kombination nichts
  ändert), Verlauf voll mit Auto-Purge und wirklich neuem, nicht-doppeltem Signal (identisches
  Ergebnis: genau ein Eintrag wird verdrängt).
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko — der Fix verhindert ausschließlich
  ungewollten Datenverlust.

## Quelle
- `G4MEOVER18/ProtoPirate` (main, `e3d9da5`): `protopirate_history.c`
  (`protopirate_history_add_to_history`, `protopirate_history_delete_item`).
