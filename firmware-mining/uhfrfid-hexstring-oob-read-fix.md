# UHF RFID (YRM100) — Port-Vorschlag: convertToHexString() ohne Nullterminator + Free auf String-Literal

## Was
Ich habe den seit mehreren Sitzungen offenen Rückstellpunkt "restlicher NFC-Werkzeug-Cluster aus
dem RogueMaster-Bündel" weiterverfolgt und den bislang ungeprüften Kandidaten **UHF RFID
[(Q)M100]** geprüft (`applications/external/uhf_rfid/`, `fap_author` "frux-c", eigenständiges
Repo `frux-c/uhf_rfid`, Branch `dev`, unverändert in RogueMaster gebündelt). Anders als der
restliche NFC-Werkzeug-Cluster (13,56-MHz-NFC) deckt diese App eine bislang komplett fehlende
Kategorie ab: UHF-RFID (860–960 MHz, EPC Class 1 Gen 2 / ISO 18000-6C) über ein externes YRM100-
Modul (M5Stack-Nachbau), angeschlossen über den Erweiterungsport (UART, `FuriHalSerialIdUsart`).
Alle 15 C-Dateien und beide Header-Verzeichnisse (Scenes, Core, `uhf_uart`/`uhf_buffer`/
`uhf_module`/`uhf_tag`/`uhf_worker`) vollständig gelesen.

Fund in `uhf_app.c::convertToHexString()` — der zentralen Formatierfunktion, die **jede** Anzeige
von EPC-/TID-/USER-Bank-Rohdaten durchläuft (Read-Erfolgsanzeige direkt nach jedem Tag-Read, sowie
die Device-Info-Ansicht für alle drei Bänke):

```c
// uhf_app.c, ALT:
char* convertToHexString(uint8_t* array, size_t length) {
    if(array == NULL || length == 0) {
        return " ";                              // <- String-Literal
    }
    FuriString* temp_str = furi_string_alloc();
    for(size_t i = 0; i < length; i++) {
        furi_string_cat_printf(temp_str, "%02X ", array[i]);
    }
    const char* furi_str = furi_string_get_cstr(temp_str);

    size_t str_len = strlen(furi_str);
    char* str = (char*)malloc(sizeof(char) * str_len);   // <- fehlt: +1 fuer den Terminator
    memcpy(str, furi_str, str_len);                       // <- kopiert das '\0' NICHT mit
    furi_string_free(temp_str);
    return str;
}
```

Zwei getrennte Fehler in derselben Funktion:

1. **Fehlender Nullterminator (Normalfall, jeder erfolgreiche Read mit Daten).** `str_len` ist die
   Länge des aufgebauten Hex-Strings ohne Terminator. Die Allokation reserviert genau `str_len`
   Byte, `memcpy` kopiert genau `str_len` Byte — der Terminator aus `furi_str` wird nie
   übernommen. Beide Aufrufstellen (`uhf_scene_read_tag_success.c:58`,
   `uhf_scene_device_info.c:59/63/67`) reichen den Rückgabewert direkt an
   `widget_add_string_multiline_element()` weiter, das ihn als gewöhnlichen C-String behandelt
   (liest bis zum ersten `\0`-Byte). Da die Allokation kein `\0` enthält, liest die Anzeige über
   die Allokation hinaus — ein Heap-Buffer-Overflow-Read, der bei **jedem** erfolgreichen Tag-Read
   mit nicht-leerer Bank auftritt (EPC ist im normalen Ablauf nie leer, siehe Risiko-Abschnitt),
   nicht nur in einem Randfall.
2. **Free auf Nicht-Heap-Speicher (Randfall, Bank-Größe 0).** Ist `array == NULL` oder
   `length == 0` (z. B. `uhf_tag->epc->size` steht laut `uhf_tag_alloc()` initial auf `0`, bevor
   ein erster Read sie setzt), gibt die Funktion das String-Literal `" "` zurück. Beide
   Aufrufstellen rufen anschließend unconditional `free()` auf dem Rückgabewert
   (`uhf_scene_read_tag_success.c:76`, implizit auch der Device-Info-Pfad über denselben
   Rückgabewert) — Freigabe von Speicher, der nie von `malloc()` stammt.

Beide Funde mit einem eigenständigen Host-Repro (`repro.c`, unten) unter AddressSanitizer/UBSan
nachvollzogen: Fehler 1 löst einen `heap-buffer-overflow`-Lesefehler in `strlen()` aus, Fehler 2
einen Absturz in `free()`.

## Nutzen
- Erste Abdeckung der UHF-RFID-Kategorie (860–960 MHz, EPC Gen2/ISO 18000-6C) im gesamten
  Werkzeug-Bestand — bislang ausschließlich 13,56-MHz-NFC und 125-kHz-LFRFID vorhanden. Relevant
  für Zutritts-/Inventar-Systeme, die UHF-Tags statt klassischer NFC-/LFRFID-Badges nutzen
  (Lager-/Fuhrpark-Tags, manche Zutrittskarten).
- Nutzt ausschließlich Standard-Erweiterungsport-UART (`furi_hal_serial_control_acquire`/
  `furi_hal_serial_init`/`_async_rx_start`/`_tx`) plus `furi_hal_power_enable_otg()` mit sauberer
  Fremd-Ownership-Prüfung (`furi_hal_power_is_otg_enabled()` vor dem eigenen Enable, `is_5v_
  enabled_by_app`-Flag vor dem Disable) — dasselbe Muster, dessen **Fehlen** ich beim RollForge-
  OTG-Fund in diesem Repo bereits dokumentiert habe; hier bereits korrekt implementiert, kein
  Fund.
- Der gefundene Fehler ist mit einer kleinen, lokal begrenzten Änderung an einer einzigen Funktion
  behebbar (zwei Zeilen Allokationsgröße + eine Zeile für den Leerfall) — kein Architektur- oder
  API-Eingriff, keine Verhaltensänderung im Erfolgsfall.
- Externe Modul-Abhängigkeit (YRM100, ~15–20 € Fremdhardware) ist explizit im Zweck der App
  begründet (UHF-Antenne kann der Flipper selbst nicht), passt in denselben Rahmen wie bereits
  zurückgestellte Zweitgeräte-Apps (`can_commander`, Seader) — hier aber inklusive echtem
  Speicherfehlerfund, daher jetzt als Port-Vorschlag statt nur als Idee kuratiert.

## Betroffene Dateien (lokal)
Neu, aus der Quelle übernehmbar (eigenständiger externer App-Ordner):
- `applications_user/uhf_rfid/application.fam`
- `applications_user/uhf_rfid/uhf_app.c` — **mit dem unten beschriebenen Fix in
  `convertToHexString()`**
- `applications_user/uhf_rfid/uhf_app.h`, `uhf_app_i.h` (unverändert)
- `applications_user/uhf_rfid/uhf_uart.c`/`.h`, `uhf_buffer.c`/`.h`, `uhf_device.c`/`.h`,
  `uhf_module.c`/`.h`, `uhf_module_cmd.h`, `uhf_module_settings.h`, `uhf_tag.c`/`.h`/
  `uhf_tag_i.h`, `uhf_worker.c`/`.h` (unverändert, mitgeprüft — kein weiterer Fund)
- `applications_user/uhf_rfid/scenes/*` (17 Scene-Dateien, unverändert bis auf die beiden bereits
  genannten Aufrufstellen, die vom Fix in `uhf_app.c` profitieren, selbst aber unverändert bleiben)
- `applications_user/uhf_rfid/icons/*`, `applications_user/uhf_rfid/assets/img/*` (Icons/Bilder,
  unverändert)

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (reine `.fap`, keine
Firmware-Kernänderung, kein `requires`-Eintrag im `application.fam` über Standard-GUI/-Serial/
-Power/-Expansion hinaus).

## Risiko
- Niedrig: additiver externer App-Ordner ohne Firmware-Kernänderung. Fehlschlag zeigt sich beim
  Build (`ufbt`/APPCHK) als Compile-Fehler.
- **API-87-Kompatibilität**: gegen den eigenen `mntm-012`-Header-Stand verifiziert (Momentum-
  Firmware-Tag sparse ausgecheckt: `targets/furi_hal_include/furi_hal_serial.h`,
  `furi_hal_serial_control.h`, `furi_hal_power.h`, sowie `applications/services/expansion/
  expansion.h`). Alle verwendeten Signaturen 1:1 vorhanden: `furi_hal_serial_control_acquire/
  release()`, `furi_hal_serial_init/deinit()`, `furi_hal_serial_async_rx_start/stop()`,
  `furi_hal_serial_tx()/_tx_wait_complete()`, `furi_hal_serial_set_br()`,
  `furi_hal_power_is_otg_enabled()/enable_otg()/disable_otg()`, `expansion_disable()/enable()`.
  Kein Abweichungsrisiko festgestellt.
- **Der gefundene Fehler selbst**: Fehler 1 (fehlender Terminator) ist kein theoretischer
  Randfall — er tritt bei jedem erfolgreichen Read mit nicht-leerer Bank auf, also im normalen
  Betrieb der App (Read-Erfolgsanzeige ist die unmittelbare Folgeszene nach
  `UHFWorkerEventSuccess`, siehe `uhf_scene_read_tag.c`). Auf dem echten Gerät äußert sich ein
  Heap-Buffer-Overflow-Read je nach Heap-Layout als zusätzliche/falsche Zeichen in der Anzeige bis
  hin zu einem `furi_check`-Abbruch, falls der gelesene Speicher außerhalb des Heap-Pools liegt —
  nicht notwendigerweise reproduzierbar bei jedem einzelnen Gerätestart, aber strukturell in jedem
  Build vorhanden. Fehler 2 (Free auf Leerfall) tritt nur auf, wenn eine Bank tatsächlich mit
  Größe 0 gelesen/angezeigt wird; im regulären Erfolgspfad unwahrscheinlich (der Worker meldet nur
  bei geparster EPC Erfolg), aber die Funktion selbst behandelt den Fall explizit — die
  Codepfad-Existenz allein rechtfertigt den Fix, unabhängig davon, wie leicht er am Gerät
  auslösbar ist.
- Der Fix ändert das Verhalten im Erfolgsfall nicht (identischer Hex-String-Inhalt, nur korrekt
  terminiert) und macht den Leerfall zusätzlich robust (echter freibarer Heap-String statt
  String-Literal).

## Port-Skizze
1. App-Ordner `uhf_rfid` (alle unter „Betroffene Dateien" gelisteten Dateien) aus
   `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
   `applications/external/uhf_rfid/`) nach `applications_user/uhf_rfid/` kopieren.
2. In der kopierten `uhf_app.c` `convertToHexString()` ersetzen:

   ```c
   char* convertToHexString(uint8_t* array, size_t length) {
       if(array == NULL || length == 0) {
           char* empty = (char*)malloc(1);
           empty[0] = '\0';
           return empty; // echter, freibarer Heap-String statt String-Literal
       }
       FuriString* temp_str = furi_string_alloc();
       for(size_t i = 0; i < length; i++) {
           furi_string_cat_printf(temp_str, "%02X ", array[i]);
       }
       const char* furi_str = furi_string_get_cstr(temp_str);

       size_t str_len = strlen(furi_str);
       char* str = (char*)malloc(sizeof(char) * (str_len + 1)); // Platz fuer den Terminator
       memcpy(str, furi_str, str_len + 1);                       // Terminator wird mitkopiert
       furi_string_free(temp_str);
       return str;
   }
   ```

   Keine weitere Änderung nötig — beide Aufrufstellen (`uhf_scene_read_tag_success.c`,
   `uhf_scene_device_info.c`) rufen bereits unconditional `free()` auf einem nun in jedem Fall
   echten Heap-Pointer.
3. `ufbt launch_app APPSRC=applications_user/uhf_rfid` bzw.
   `fbt launch_app APPSRC=applications_user/uhf_rfid` — deckt etwaige Header-Abweichungen als
   Compile-Fehler auf.
4. Host-seitiges Mini-Repro (`repro.c`, unten) mit `gcc -O1 -g -Wall -Wextra
   -fsanitize=address,undefined` bauen und laufen lassen, um ALT (Absturz in beiden Szenarien)
   gegen NEU (kein Absturz in beiden Szenarien) zu bestätigen — bereits lokal ausgeführt, siehe
   Testhinweis.
5. Funktionstest am Gerät: eigenes YRM100-Modul per Erweiterungsport anschließen (Verkabelung
   laut Original-README, `docs.m5stack.com/en/unit/uhf_rfid`), App öffnen, mindestens einen
   eigenen UHF-Tag lesen (EPC-Bank-Anzeige in der Read-Erfolgsszene, danach "More" für die
   Device-Info-Ansicht mit EPC/TID/USER-Bank-Umschaltung per Rechts-Taste prüfen) — Hex-Anzeige
   muss vor/nach dem Fix inhaltlich identisch bleiben, nur ohne zusätzliche/abgeschnittene
   Zeichen am Ende.
6. Den Leerfall selbst (Bank-Größe 0) ist am Gerät nur künstlich nachstellbar (z. B. Device-Info
   sofort nach App-Start ohne vorherigen Read aufrufen, falls über die Szenen-Navigation
   erreichbar) — für den eigentlichen Nachweis reicht das Host-Repro; am Gerät nur der Normalfall
   zwingend zu testen.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`) in
`firmware-mining/repro-uhfrfid-hexstring-oob-read/` — bildet `convertToHexString()` 1:1 nach
(Allokations-/Kopiergrößen wortgetreu aus dem gelesenen Quelltext übernommen; der
`FuriString`-Zwischenschritt wird durch eine trivial gleichwertige Host-Zeichenkette ersetzt, ohne
Bedeutung für die geprüfte Lücke). Jedes der vier Szenarien läuft isoliert in einem eigenen
`fork()`-Kindprozess. Gebaut/ausgeführt mit
`gcc -O1 -g -Wall -Wextra -fsanitize=address,undefined`:

```
Szenario A (ALT, EPC-Bank 4 Byte, nicht leer):
==ERROR: AddressSanitizer: heap-buffer-overflow ... READ of size 13 ...
    #0 in strlen ...
    #1 in scenario_a_alt ...
0x... is located 0 bytes after 12-byte region [...]
[CRASH] Szenario A (ALT) beendete sich mit Exit-Code 1

Szenario A (NEU):
[OK] strlen() = 12, erwartet 12 -> stimmt ueberein
[KEIN ABBRUCH] Szenario A (NEU) lief sauber durch

Szenario B (ALT, Bank-Groesse 0):
AddressSanitizer:DEADLYSIGNAL
==ERROR: AddressSanitizer: SEGV ... in free ...
    #3 in free ...
    #4 in scenario_b_alt ...
[CRASH] Szenario B (ALT) beendete sich mit Exit-Code 1

Szenario B (NEU):
[OK] Rueckgabe ist ein echter, 1 Byte grosser Heap-String (strlen=0)
[OK] free() auf Heap-Allokation lief sauber durch
[KEIN ABBRUCH] Szenario B (NEU) lief sauber durch
```

Bestätigt: ALT löst in beiden Fehlerszenarien einen Abbruch unter AddressSanitizer aus
(Heap-Buffer-Overflow-Read bei nicht-leerer Bank, ungültige Freigabe bei Bank-Größe 0); NEU läuft
in beiden Szenarien sauber durch, bei inhaltlich identischem Ergebnis im Normalfall (Szenario A:
`strlen()` liefert in beiden Versionen den erwarteten Wert, nur ALT ermittelt ihn über eine
bereits ungültige Speicherzugriffsoperation).

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`, Commit `2855aa43`,
  `applications/external/uhf_rfid/` (Original-Repo `frux-c/uhf_rfid`, Branch `dev`, `fap_author`
  "frux-c").
- Eigener Fund: `convertToHexString()` in `uhf_app.c` allokiert/kopiert ohne Platz für den
  Nullterminator (Heap-Buffer-Overflow-Read bei jeder nachgelagerten C-String-Nutzung) und gibt im
  Leerfall ein String-Literal zurück, das die Aufrufstellen anschließend per `free()` freigeben —
  mit Host-Repro unter AddressSanitizer/UBSan bestätigt (siehe
  `firmware-mining/repro-uhfrfid-hexstring-oob-read/repro.c`).
- Restlicher NFC-Werkzeug-Cluster aus demselben Rückstellpunkt weiterhin offen: `chameleon`,
  `nfc_relay` (beide Fremdgerät/Zweitgerät), `nfctools` (~35 weitere Schreib-Szenen, siehe
  eigene laufende Teilserie).
