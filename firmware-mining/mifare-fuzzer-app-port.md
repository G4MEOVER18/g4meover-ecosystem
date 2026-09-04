# Mifare Fuzzer — externe App aus RogueMaster-Bündel (UID-Fuzzing gegen NFC-Reader)

## Was
Ich habe den seit mehreren Sitzungen offenen Rückstellpunkt "restlicher NFC-Werkzeug-Cluster
aus dem RogueMaster-Bündel" (`ideas/pentest-app-katalog.md`) weiterverfolgt und **Mifare
Fuzzer** geprüft (`applications/external/mifare_fuzzer/`, `fap_author` "spheeere98 Sil333033",
Original-Quelle `github.com/spheeere98/mifare_fuzzer`) — mit 1503 Zeilen über 14 C-Dateien der
kleinste noch offene Kandidat aus dem Cluster (`mifare_fuzzer`, `nfctools`, `uhf_rfid`,
`nfc_comparator`, `simultaneous_rfid_reader`).

Die App emuliert Mifare-Classic-1k/-4k- und Ultralight-UIDs über die eingebaute NFC-Antenne
(`nfc_listener`), um zu beobachten, wie ein Reader auf wechselnde/unbekannte UIDs reagiert —
drei Modi: feste "Test Values" (9 vordefinierte UIDs), "Random Values" (zufällige UID pro
Zyklus) und "Load UIDs from file" (eigene `.txt`-Liste). Vollständig gelesen, keine
Zusatzhardware nötig (reine eingebaute Antenne, anders als der bereits zurückgestellte
`chameleon`-Kandidat).

Zwei eigene Funde, beide im selben ~50-Zeilen-Handler
(`mifare_fuzzer_scene_emulator_on_event()`, Zweig `MifareFuzzerEventStartAttack`,
`scenes/mifare_fuzzer_scene_emulator.c`):

**1. Heap-Leck bei jedem Angriffszyklus (Hauptfund dieser Sitzung, siehe Patch unten):**
Der Handler alloziert bei jedem Aufruf ein frisches `Iso14443_3aData`:

```c
Iso14443_3aData* nfc_data = iso14443_3a_alloc();
...
mifare_fuzzer_emulator_set_nfc_data(app->emulator_view, *nfc_data);   // per Wert übergeben
mifare_fuzzer_worker_set_nfc_data(app->worker, *nfc_data);            // per Wert übergeben
```

Beide Setter nehmen die Struktur **per Wert** entgegen — `mifare_fuzzer_worker_set_nfc_data()`
legt sich intern sogar eine eigene Kopie per `iso14443_3a_alloc()` an und gibt genau diese
danach korrekt wieder frei (`mifare_fuzzer_worker.c`, `iso14443_3a_free(nfc_14a_data)`). Der
ursprüngliche `nfc_data`-Pointer im Handler selbst wird nach diesen beiden Aufrufen nirgends
mehr gebraucht — es gibt dafür aber kein `iso14443_3a_free()`. `MifareFuzzerEventStartAttack`
feuert nicht nur einmal beim manuellen "Start", sondern bei laufendem Angriff automatisch bei
jedem UID-Wechsel-Tick (`SceneManagerEventTypeTick`-Handler in derselben Datei, sobald
`tick_counter >= ticks_between_cards`) — für "Test Values" und "Random Values", die beiden
Standard-Angriffe ganz ohne Datei, leckt damit jeder einzelne UID-Wechsel während eines
laufenden Angriffs eine weitere `Iso14443_3aData`.

**2. Unbegrenzte Schleife bei "Load UIDs from file" ohne gültige Zeile (dokumentiert,
zurückgestellt):** Derselbe Handler sucht im Dateimodus die nächste gültige UID-Zeile in einer
`while(true)`-Schleife, die bei `stream_read_line()`-Fehlschlag (Dateiende) per
`stream_rewind()` unbedingt von vorne beginnt:

```c
while(true) {
    furi_string_reset(app->uid_str);
    if(!stream_read_line(app->uids_stream, app->uid_str)) {
        stream_rewind(app->uids_stream);
        continue;                      // <- keine Abbruchbedingung
    }
    if(furi_string_get_char(app->uid_str, 0) == '#') continue;
    if((furi_string_size(app->uid_str) != 9) && (furi_string_size(app->uid_str) != 15)) continue;
    break;
}
```

Enthält die vom Nutzer gewählte `.txt`-Datei **keine einzige** gültige Zeile (leere Datei, nur
Kommentarzeilen, oder ausschließlich Zeilen mit falscher Länge — die App akzeptiert nur exakt
4-Byte- oder 7-Byte-UIDs), dreht sich diese Schleife beim ersten `MifareFuzzerEventStartAttack`
unbegrenzt weiter (Busy-Loop ohne Wartezeit) und friert die App ein.

## Nutzen
- Ergänzt eine Kategorie, die im Cluster bislang fehlt: aktives UID-Fuzzing gegen einen Reader
  (die bereits geprüften Cluster-Apps sind rein passiv — Feld-Erkennung, Skimming-Detektor) bzw.
  Sniffing/Mutation anderer Feldparameter (NFC Fuzzer). Nützlich, um beim autorisierten Audit
  eines Zutritts-/Zahlungsreaders zu prüfen, ob er unbekannte/falsch geformte UIDs akzeptiert
  oder korrekt zurückweist.
- Reine eingebaute Antenne, keine Fremdhardware — passt in den gleichen einfachen Bau-/Testrahmen
  wie die übrigen bereits geporteten Cluster-Apps.
- Fund 1 (Leck) tritt bei **jeder** normalen Nutzung auf (Test Values, Random Values, Load UIDs
  gleichermaßen), nicht nur bei einer Fehlereingabe — bei einem länger laufenden,
  unbeaufsichtigten Angriff (typischer Einsatz: UID automatisch durchwechseln lassen) wächst der
  Heap unbegrenzt mit der Anzahl der UID-Wechsel; auf dem speicherbeschränkten STM32WB55 ein
  Pfad zu Speichererschöpfung und App-Absturz/Neustart bei längerer Laufzeit.
- Der Fix zu Fund 1 ist eine einzige ergänzte Zeile, exakt an der Stelle, an der die App für ihre
  eigene interne Kopie bereits korrekt alloziert-und-wieder-freigegeben arbeitet — reines
  Nachziehen des schon vorhandenen, richtigen Musters.

## Betroffene Dateien (lokal)
Neu, aus der Quelle übernehmbar (eigenständiger externer App-Ordner):
- `applications_user/mifare_fuzzer/application.fam`
- `applications_user/mifare_fuzzer/mifare_fuzzer.c`, `mifare_fuzzer.h`,
  `mifare_fuzzer_i.h`, `mifare_fuzzer_custom_events.h`
- `applications_user/mifare_fuzzer/mifare_fuzzer_worker.c` / `.h` (unverändert, Referenzmuster
  für den Fix)
- `applications_user/mifare_fuzzer/scenes/*` — `mifare_fuzzer_scene_emulator.c` **mit dem unten
  beschriebenen Fix**, alle übrigen Szenen unverändert
- `applications_user/mifare_fuzzer/views/mifare_fuzzer_emulator.c` / `.h` (unverändert)
- `applications_user/mifare_fuzzer/resources/mifare_fuzzer/example_uids04.txt` / `example_uids07.txt`
  (Beispiel-UID-Listen, keine echten Kartendaten — reine Testwerte wie `11:22:33:44`)
- `applications_user/mifare_fuzzer/images/*`

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (reine `.fap`, keine
Firmware-Kernänderung).

## Risiko
- Niedrig: additiver externer App-Ordner ohne Firmware-Kernänderung. Fehlschlag zeigt sich beim
  Build (`ufbt`/APPCHK) als Compile-Fehler.
- **API-87-Kompatibilität**: gegen den eigenen `mntm-012`-Header-Stand verifiziert (sparse
  ausgecheckt: `lib/nfc/nfc.h`, `lib/nfc/nfc_listener.h`, `lib/nfc/nfc_device.h`,
  `lib/nfc/protocols/iso14443_3a/iso14443_3a.h`). Alle verwendeten Signaturen 1:1 vorhanden:
  `nfc_alloc()`/`nfc_free()`, `nfc_listener_alloc()`/`_start()`/`_stop()`/`_free()`,
  `nfc_device_alloc()`/`_free()`/`_clear()`/`_set_data()`/`_get_data()`/`_get_protocol()`/
  `_get_protocol_name()`, `iso14443_3a_alloc()`/`_free()`/`_copy()`. `Iso14443_3aData` erlaubt
  laut eigenem Header bis zu `ISO14443_3A_MAX_UID_SIZE` (10 Byte) UID-Länge — die App nutzt
  selbst nur 4/7 Byte, damit auch das UID-Textfeld im Draw-Callback (`char uid[25]`, geprüft:
  für 7 Byte maximal 21 Byte belegt inkl. Trenner und Nullterminierung) unkritisch.
- **Fund 1 (Leck)**: reiner Ressourcen-Lebenszyklus-Fehler, kein Speicherzugriffsfehler — der
  Fix ist eine ergänzte `free()`-Anweisung auf einen Pointer, der ab dieser Stelle ohnehin nicht
  mehr referenziert wird (kein Double-Free, da nirgends sonst freigegeben). Kein Einfluss auf
  Timing, Protokoll oder emulierte Daten.
- **Fund 2 (Endlosschleife)**: nur bei einer semantisch leeren/ungültigen Nutzerdatei auslösbar,
  nicht bei den mitgelieferten Beispiel-Listen — als Härtung für eine spätere Sitzung
  vorgemerkt (Fix-Skizze unten), diese Sitzung bewusst auf Fund 1 begrenzt.

## Port-Skizze
1. App-Ordner `mifare_fuzzer` (alle unter „Betroffene Dateien" gelisteten Dateien) aus
   `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
   `applications/external/mifare_fuzzer/`) nach `applications_user/mifare_fuzzer/` kopieren.
2. In der kopierten `mifare_fuzzer_scene_emulator.c`, Funktion
   `mifare_fuzzer_scene_emulator_on_event()`, Zweig `MifareFuzzerEventStartAttack`, direkt nach
   dem bestehenden `if(nfc_device_parsed) {...} else {...}`-Block (der `nfc_data` an Emulator-
   und Worker-Setter per Wert übergibt) eine Zeile ergänzen:

   ```c
   mifare_fuzzer_emulator_set_nfc_data(app->emulator_view, *nfc_data);
   if(nfc_device_parsed) {
       mifare_fuzzer_worker_set_nfc_device(app->worker, nfc_device);
   } else {
       mifare_fuzzer_worker_set_nfc_data(app->worker, *nfc_data);
   }
   iso14443_3a_free(nfc_data);
   ```

   Einzige Änderung: `iso14443_3a_free(nfc_data);` — `nfc_data` wird ab dort nicht mehr
   gelesen, beide Konsumenten haben ihre eigene Kopie bereits übernommen.
3. `ufbt launch_app APPSRC=applications_user/mifare_fuzzer` bzw.
   `fbt launch_app APPSRC=applications_user/mifare_fuzzer` — deckt etwaige
   Header-Abweichungen als Compile-Fehler auf.
4. Host-seitiges Mini-Repro (`repro.c`, unten) mit
   `gcc -O1 -Wall -Wextra -fsanitize=address,undefined` bauen und laufen lassen, um ALT
   (LeakSanitizer meldet Lecks) gegen NEU (sauberer Exit) zu bestätigen — bereits lokal
   ausgeführt, siehe Testhinweis.
5. Funktionstest: "Random Values" oder "Test Values" gegen einen eigenen NFC-Reader mehrere
   Minuten mit niedrigem `ticks_between_cards`-Wert laufen lassen (schnellere Wechsel,
   schnellerer Heap-Druck); freien Heap vor/nach dem Fix vergleichen (Debug-Log/Heap-Anzeige) —
   vor dem Fix sinkt der freie Heap kontinuierlich mit jedem UID-Wechsel, nach dem Fix bleibt er
   stabil. Reader-Reaktion auf die UID-Wechsel selbst ist vom Fix unberührt.
6. Fund 2 (Endlosschleife bei leerer/ungültiger UID-Datei) bewusst nicht in diesem Port
   enthalten — Fix-Skizze für eine künftige Sitzung: im `MifareFuzzerAttackLoadUidsFromFile`-
   Zweig einen `found_any_this_pass`-Merker mitführen und bei `stream_read_line()`-Fehlschlag
   ohne Treffer seit dem letzten `stream_rewind()` den Angriff sauber abbrechen
   (`mifare_fuzzer_worker_stop()` + Benachrichtigung) statt erneut zu spulen — Referenz-
   Kontrollfluss bereits als `neu_select_line()` im zweiten Repro-Ordner unten vorhanden.

## Testhinweis (lokal nachvollzogen)
Zwei host-kompilierbare Mini-Repros, beide ohne Flipper-/NFC-Abhängigkeiten:

**Fund 1 (Leck)** —
`firmware-mining/repro-mifare-fuzzer-startattack-nfc-data-leak/repro.c` bildet
`iso14443_3a_alloc`/`_free` als echte `malloc`/`free`-Wrapper nach, dazu den bereits korrekten
alloziert-und-wieder-freigegebenen Worker-Pfad und den reinen Wert-Kopie-Pfad des
Emulator-Views, und simuliert eine Serie von `MifareFuzzerEventStartAttack`-Zyklen wie bei
einem laufenden "Random Values"-Angriff:

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
./repro 50 alt   # ALT: LeakSanitizer meldet 50 verlorene Allokationen (700 Byte)
./repro 50 neu   # NEU: sauberer Exit, 0 Lecks
```

Tatsächliches Ergebnis (lokal ausgeführt):
```
=== ALT (50 Zyklen) ===
==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 700 byte(s) in 50 object(s) allocated from: ... in main
SUMMARY: AddressSanitizer: 700 byte(s) leaked in 50 allocation(s).
Exit-Code: 1

=== NEU (50 Zyklen) ===
Simulated 50 MifareFuzzerEventStartAttack cycles (NEU, fixed).
Exiting now -- ASan LeakSanitizer will report anything still allocated.
Exit-Code: 0
```

**Fund 2 (Endlosschleife, dokumentiert)** —
`firmware-mining/repro-mifare-fuzzer-load-uids-no-valid-line-hang/repro.c` bildet die
Zeilenauswahl-Schleife mit einer In-Memory-Zeilenliste nach (Stellvertreter für
`stream_read_line()`/`stream_rewind()`), mit einer Iterationsobergrenze, um ein Hängenbleiben
messbar statt den Testlauf selbst blockierend zu machen:

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
```

Ergebnis: bei einer Datei mit mindestens einer gültigen Zeile terminieren ALT und NEU
gleichermaßen in wenigen Iterationen (Regressions-Check: NEU verändert den Normalfall nicht);
bei leerer Datei, reiner Kommentardatei oder ausschließlich falsch-langen Zeilen terminiert ALT
nie (Iterationsobergrenze erreicht), NEU bricht nach einem vollständigen Durchlauf ohne Treffer
sauber ab.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout, Stand dieser
  Sitzung): `applications/external/mifare_fuzzer/`, `fap_author` "spheeere98 Sil333033",
  Original-Quelle `github.com/spheeere98/mifare_fuzzer`.
- API-87-Nachweis: `Next-Flip/Momentum-Firmware`, Tag `mntm-012`, Sparse-Checkout
  `lib/nfc/nfc.h`, `lib/nfc/nfc_listener.h`, `lib/nfc/nfc_device.h`,
  `lib/nfc/protocols/iso14443_3a/iso14443_3a.h`.
- Eigener Fund 1 (Leck): `mifare_fuzzer_scene_emulator.c`,
  `MifareFuzzerEventStartAttack`-Zweig, gegen `mifare_fuzzer_worker.c` als Referenz für das
  korrekte Muster verifiziert, mit Host-Repro unter AddressSanitizer/LeakSanitizer bestätigt
  (siehe `firmware-mining/repro-mifare-fuzzer-startattack-nfc-data-leak/repro.c`).
- Eigener Fund 2 (Endlosschleife, dokumentiert/zurückgestellt): dieselbe Datei,
  `MifareFuzzerAttackLoadUidsFromFile`-Zweig, mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-mifare-fuzzer-load-uids-no-valid-line-hang/repro.c`).
- Restlicher NFC-Werkzeug-Cluster aus demselben Rückstellpunkt weiterhin offen: `nfctools`,
  `uhf_rfid`, `nfc_comparator`, `simultaneous_rfid_reader` (letztere zwei vermutlich
  Fremdhardware-Fall, externes UHF-Lesegerät nötig, noch nicht abschließend bestätigt).
