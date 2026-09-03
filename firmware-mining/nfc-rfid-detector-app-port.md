# NFC/RFID Detector — externe App aus RogueMaster-Bündel (Feld-Erkennung ohne Mitschnitt)

## Was
Ich habe den seit mehreren Sitzungen offenen Rückstellpunkt "restlicher NFC-Werkzeug-Cluster
aus dem RogueMaster-Bündel" (`ideas/pentest-app-katalog.md`) weiterverfolgt und **NFC/RFID
Detector** geprüft (`applications/external/nfc_rfid_detector/`, `fap_author` "SkorP") — mit
407 Zeilen über 5 C-Dateien der kleinste noch offene Kandidat aus dem Cluster, vollständig
gegengeprüft.

Die App ist funktional die einfachste im gesamten Cluster: sie sendet nichts, liest keine
Karte, sondern prüft nur, ob gerade ein 13,56-MHz-NFC-Feld und/oder ein 125-kHz-LF-RFID-Feld
in Reichweite der Antenne aktiv ist (inkl. exakter LF-Frequenz) — nützlich, um bei einem
unbekannten oder nicht dokumentierten Reader vor Ort schnell festzustellen, welche Technologie
er überhaupt verwendet, bevor man mit dem passenden Werkzeug (NFC-App vs. LFRFID-App)
weitermacht. `nfc_rfid_detector_scene_field_presence.c` fragt per 100-ms-Tick
`furi_hal_nfc_field_is_present()` und `furi_hal_rfid_field_is_present()` ab; die View
(`views/nfc_rfid_detector_view_field_presence.c`) hält jeden Treffer über einen
Decay-Zähler (`FIELD_FOUND_WEIGHT = 5`, also 500 ms) sichtbar, damit kurze Aussetzer nicht als
Flackern erscheinen — sauber implementiert, kein Fund in diesem Teil.

Beim Review von `nfc_rfid_detector_app_i.c` (dem einzigen Hardware-Anbindungscode der App)
einen echten Fehler in der Fehlerbehandlung gefunden:

```c
// nfc_rfid_detector_app_i.c, ALT — nfc_rfid_detector_app_field_presence_start():
void nfc_rfid_detector_app_field_presence_start(NfcRfidDetectorApp* app) {
    furi_hal_rfid_field_detect_start();

    furi_hal_nfc_acquire();          // <- Rueckgabewert FuriHalNfcError verworfen
    furi_hal_nfc_field_detect_start();
}
```

`furi_hal_nfc_acquire()` (`targets/furi_hal_include/furi_hal_nfc.h`) gibt einen
`FuriHalNfcError` zurück und kann `FuriHalNfcErrorBusy` liefern: intern
(`targets/f7/furi_hal/furi_hal_nfc.c`) nimmt die Funktion zunächst den SPI-Bus-Handle,
versucht dann `furi_hal_nfc.mutex` mit einem 100-Tick-Timeout zu bekommen — schlägt das fehl,
gibt sie den SPI-Bus-Handle sofort wieder frei und liefert `FuriHalNfcErrorBusy`, **ohne** dass
danach irgendetwas als "erfolgreich acquiriert" markiert ist. Die App prüft diesen Rückgabewert
nirgends und ruft unconditional `furi_hal_nfc_field_detect_start()` auf. Diese Funktion schreibt
über `st25r3916_write_reg()` NFC-Chip-Register per SPI — und jede SPI-Bus-Transaktion
(`furi_hal_spi_bus_tx()`/`_trx()`, `targets/f7/furi_hal/furi_hal_spi.c`) beginnt mit
`furi_check(handle->bus->current_handle == handle);`. Da nach einem fehlgeschlagenen `acquire()`
der Bus-Handle wieder auf `NULL` steht, schlägt diese Prüfung fehl — `furi_check()` ist ein
harter Abbruch (Reboot/Hard-Fault), kein rückgabewertbasierter Fehlerpfad.

Zusätzlich betrifft dasselbe Muster den Exit-Pfad: `nfc_rfid_detector_app_field_presence_stop()`
ruft unconditional `furi_hal_nfc_release()` auf, die `furi_check(furi_hal_nfc_is_mine())`
(Mutex-Eigentümer-Prüfung) verlangt — nach einem fehlgeschlagenen `acquire()` gehört der Mutex
dem aufrufenden Thread aber nie, also crasht auch der Exit-Pfad, falls der Start-Pfad es nicht
schon getan hat.

Auslöser ist `FuriHalNfcErrorBusy`, also ein anderer Thread hält `furi_hal_nfc.mutex` bereits
beim Öffnen dieser App länger als die 100-Tick-Timeout-Spanne — in meiner eigenen
`mntm-012`-Basis nutzt laut `grep -rn furi_hal_nfc_acquire applications/` keine
Hintergrund-Firmwarekomponente diese API selbst (kein automatischer NFC-Erkennungsdienst im
Desktop), das Risiko besteht damit konkret dann, wenn eine andere, selbst fehlerhafte
NFC-nutzende App (zwei RogueMaster-Cluster-Kandidaten aus meinem eigenen Backlog sind noch
ungeprüft) beim vorherigen Beenden den Mutex nicht sauber freigegeben hat und dieser Zustand
bis zum nächsten App-Start bestehen bleibt. Der Fix macht die App unabhängig davon robust, statt
sich auf die Korrektheit aller anderen, potenziell gleichzeitig oder zuvor laufenden NFC-Apps zu
verlassen — passt in denselben Muster-Fehler (ungeprüfter `furi_check`-auslösender Rückgabewert),
den ich bereits bei anderen Firmware-Funden in diesem Repo dokumentiert habe (z. B.
`nfc-desfire-zero-key-furi-check-crash-fix.md`).

## Nutzen
- Sehr kleine, fokussierte App ohne Zusatzhardware — nutzt ausschließlich die eingebaute
  NFC-/LFRFID-Antenne, kein ESP32/nRF24-Modul.
- Deckt eine mir bislang fehlende, einfache aber praktische Kategorie ab: **passive
  Reader-Technologie-Erkennung** vor Ort, bevor man mit NFC- oder LFRFID-spezifischen Werkzeugen
  weiterarbeitet — nützlich beim autorisierten Audit unbekannter/undokumentierter Zutritts- oder
  Zahlungsreader.
- Rein empfangsseitig (Feld-Detektion, kein eigener Sendebetrieb) — passt in den defensiven
  Rahmen aus CONSTRAINTS Punkt 5.
- Der gefundene Fehler ist mit einer kleinen, lokal begrenzten Änderung behebbar (ein zusätzliches
  Statusfeld plus drei Guards) — kein Architektur- oder API-Eingriff nötig, macht die App robust
  gegen einen Zustand, den sie aktuell überhaupt nicht behandelt.

## Betroffene Dateien (lokal)
Neu, aus der Quelle übernehmbar (eigenständiger externer App-Ordner):
- `applications_user/nfc_rfid_detector/application.fam`
- `applications_user/nfc_rfid_detector/nfc_rfid_detector_app.c`
- `applications_user/nfc_rfid_detector/nfc_rfid_detector_app_i.c` — **mit dem unten
  beschriebenen Fix**
- `applications_user/nfc_rfid_detector/nfc_rfid_detector_app_i.h` — **mit einem zusätzlichen
  Statusfeld**
- `applications_user/nfc_rfid_detector/helpers/nfc_rfid_detector_event.h` /
  `nfc_rfid_detector_types.h`
- `applications_user/nfc_rfid_detector/scenes/*` (Scene-Handler, Config, unverändert)
- `applications_user/nfc_rfid_detector/views/nfc_rfid_detector_view_field_presence.c` / `.h`
  (unverändert, siehe Review oben)
- `applications_user/nfc_rfid_detector/images/*`,
  `applications_user/nfc_rfid_detector/nfc_rfid_detector_icon_10px.png`

Meine CI-Vorlage `ci-templates/build-fap.yml` passt unverändert (reine `.fap`, keine
Firmware-Kernänderung). Von den beiden `App()`-Einträgen im upstream `application.fam`
(`nfc_rfid_detector` unter Kategorie „RFID" und `nfc_detector` unter „NFC", identischer
`entry_point`) übernehme ich beide unverändert — reine Katalog-Doppelauflistung, kein Bug.

## Risiko
- Niedrig: additiver externer App-Ordner ohne Firmware-Kernänderung. Fehlschlag zeigt sich beim
  Build (`ufbt`/APPCHK) als Compile-Fehler.
- **API-87-Kompatibilität**: gegen den eigenen `mntm-012`-Header-Stand verifiziert (Momentum-
  Firmware-Tag sparse ausgecheckt: `targets/furi_hal_include/furi_hal_nfc.h`,
  `targets/f7/furi_hal/furi_hal_rfid.h`, sowie `targets/f7/furi_hal/furi_hal_nfc.c`/
  `furi_hal_spi.c` als Implementierungsnachweis für den Fund). Alle verwendeten Signaturen 1:1
  vorhanden: `furi_hal_nfc_acquire()`/`_release()`/`_field_detect_start()`/`_field_detect_stop()`/
  `_field_is_present()` (letztere bei mir ohne Frequenz-Parameter, NFC hat nur eine Frequenz),
  `furi_hal_rfid_field_detect_start()`/`_stop()`/`_field_is_present(uint32_t*)`. GUI-Seite nutzt
  ausschließlich Standardheader (`gui/gui.h`, `gui/view_dispatcher.h`, `gui/scene_manager.h`,
  `gui/modules/submenu.h`, `gui/modules/widget.h`, `notification/notification_messages.h`), kein
  `requires`-Eintrag über das Standard-NFC/-GUI hinaus.
- **Der gefundene Fehler selbst**: kein Speicherzugriffsfehler, sondern ein ungeprüfter, auf
  `furi_check()` (Hard-Fault/Reboot) hinauslaufender Fehlerpfad — tritt nur bei
  `FuriHalNfcErrorBusy` auf (Mutex bereits von einem anderen Thread gehalten), nicht bei jedem
  App-Start. In meiner eigenen `mntm-012`-Basis gibt es dafür keinen bekannten, garantiert
  auslösenden Trigger aus der Stock-/Momentum-Firmware selbst (siehe Analyse oben) — das
  Restrisiko liegt bei anderen, gleichzeitig fehlerhaften Dritt-Apps, die den NFC-Mutex nicht
  sauber freigeben. Der Fix macht die App dagegen robust, statt sich auf die Korrektheit
  anderer Apps zu verlassen.
- `furi_hal_rfid_*` teilt sich laut eigener Quellprüfung keinen vergleichbaren Mutex/SPI-Ownership-
  Mechanismus mit `furi_hal_nfc_*` — vom Fund unberührt, kein Fix nötig.

## Port-Skizze
1. App-Ordner `nfc_rfid_detector` (alle unter „Betroffene Dateien" gelisteten Dateien) aus
   `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
   `applications/external/nfc_rfid_detector/`) nach `applications_user/nfc_rfid_detector/`
   kopieren.
2. In der kopierten `nfc_rfid_detector_app_i.h` ein Statusfeld ergänzen:

   ```c
   struct NfcRfidDetectorApp {
       Gui* gui;
       ViewDispatcher* view_dispatcher;
       SceneManager* scene_manager;
       NotificationApp* notifications;
       Submenu* submenu;
       Widget* widget;
       NfcRfidDetectorFieldPresence* nfc_rfid_detector_field_presence;
       bool nfc_acquired; // neu: true nur, wenn furi_hal_nfc_acquire() erfolgreich war
   };
   ```

3. In der kopierten `nfc_rfid_detector_app_i.c` alle drei Hardware-Funktionen gegen den
   Rückgabewert absichern:

   ```c
   void nfc_rfid_detector_app_field_presence_start(NfcRfidDetectorApp* app) {
       furi_assert(app);

       furi_hal_rfid_field_detect_start();

       app->nfc_acquired = (furi_hal_nfc_acquire() == FuriHalNfcErrorNone);
       if(app->nfc_acquired) {
           furi_hal_nfc_field_detect_start();
       } else {
           FURI_LOG_W(TAG, "furi_hal_nfc_acquire() busy, NFC field detection disabled");
       }
   }

   void nfc_rfid_detector_app_field_presence_stop(NfcRfidDetectorApp* app) {
       furi_assert(app);

       furi_hal_rfid_field_detect_stop();

       if(app->nfc_acquired) {
           furi_hal_nfc_field_detect_stop();
           furi_hal_nfc_release();
           app->nfc_acquired = false;
       }
   }

   bool nfc_rfid_detector_app_field_presence_is_nfc(NfcRfidDetectorApp* app) {
       furi_assert(app);

       if(!app->nfc_acquired) return false;
       return furi_hal_nfc_field_is_present();
   }
   ```

   `nfc_rfid_detector_app_field_presence_is_rfid()` bleibt unverändert (kein Zugriff auf den
   NFC-Mutex/-Bus).
4. `ufbt launch_app APPSRC=applications_user/nfc_rfid_detector` bzw.
   `fbt launch_app APPSRC=applications_user/nfc_rfid_detector` — deckt etwaige
   Header-Abweichungen als Compile-Fehler auf.
5. Host-seitiges Mini-Repro (`repro.c`, unten) mit `gcc -O0 -g -Wall -Wextra
   -fsanitize=address,undefined` bauen und laufen lassen, um ALT (Absturz nur im Busy-Fall)
   gegen NEU (kein Absturz in beiden Fällen) zu bestätigen — bereits lokal ausgeführt, siehe
   Testhinweis.
6. Funktionstest: App normal gegen einen eigenen NFC-Reader/-Tag sowie einen eigenen
   LF-RFID-Reader/-Transponder öffnen, korrekte Erkennung (inkl. LF-Frequenzanzeige) prüfen —
   der Normalfall ist von der Änderung nicht betroffen.
7. Den Busy-Fall selbst provozieren ist am Gerät nur künstlich nachstellbar (z. B. absichtlich
   fehlerhafte NFC-App parallel/kurz zuvor beenden lassen, die den Mutex hält) — für den
   eigentlichen Nachweis reicht das Host-Repro; am Gerät nur der Normalfall zwingend zu testen,
   der Fix selbst ist rein defensiv.
8. Bei erfolgreichem Test: als schnelles Vorab-Diagnosewerkzeug vor NFC-/LFRFID-spezifischen
   Apps führen.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`) in
`firmware-mining/repro-nfc-rfid-detector-unchecked-nfc-acquire/` — bildet `furi_hal_nfc_acquire()`/
`_release()`/`_field_detect_start()` sowie den zugrunde liegenden SPI-Bus-Ownership-Check
(`furi_check(handle->bus->current_handle == handle)`) 1:1 aus `furi_hal_nfc.c`/`furi_hal_spi.c`
nach, mit `furi_check()` als `abort()` und je Testfall in einem eigenen `fork()`-Kindprozess
isoliert (ein Absturz in einem Szenario beendet nicht den Testlauf). Gebaut/ausgeführt mit
`gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined` für zwei Szenarien (Mutex frei /
Mutex bereits belegt):

```
ALT (verbatim nfc_rfid_detector_app_i.c):
=== ALT (upstream nfc_rfid_detector_app_i.c, verbatim) ===
[OK   ] mutex free on enter -> no crash
furi_check FAILED: handle->bus->current_handle == handle
[CRASH] mutex busy on enter (FuriHalNfcErrorBusy) -> furi_check() aborted (device would hard-fault/reboot)
Confirmed: ALT crashes only when furi_hal_nfc_acquire() returns Busy,
silently ignored by nfc_rfid_detector_app_field_presence_start().
Exit-Code: 0 (Testprogramm selbst laeuft sauber durch, bestaetigt genau das erwartete Crash-Muster)

NEU (Rueckgabewert geprueft):
=== NEW (return value of furi_hal_nfc_acquire() checked) ===
[OK   ] mutex free on enter -> no crash
[OK   ] mutex busy on enter (FuriHalNfcErrorBusy) -> no crash
Confirmed: NEW survives both the normal case and the Busy case.
Exit-Code: 0
```

Bestätigt: die ALT-Fassung crasht ausschließlich im Busy-Fall (`furi_check()` in der
SPI-Bus-Transaktion von `furi_hal_nfc_field_detect_start()`, da der Bus-Handle nach
fehlgeschlagenem `acquire()` nicht als "aktuell" markiert ist); die NEU-Fassung übersteht
beide Szenarien ohne Zugriffsfehler, bei unverändertem Normalfall-Verhalten.

## Quelle
- RogueMaster-Firmware, `flipperzero-firmware-wPlugins`, Branch `420`,
  `applications/external/nfc_rfid_detector/`, `fap_author` "SkorP".
- Eigener Fund: ungeprüfter `FuriHalNfcError`-Rückgabewert von `furi_hal_nfc_acquire()` in
  `nfc_rfid_detector_app_i.c`, führt bei `FuriHalNfcErrorBusy` zu einem `furi_check()`-Abbruch
  in der nachfolgenden SPI-Transaktion — verifiziert gegen `targets/f7/furi_hal/furi_hal_nfc.c`/
  `furi_hal_spi.c` (Momentum-Firmware, Tag `mntm-012`) und mit Host-Repro unter
  AddressSanitizer/UBSan bestätigt (siehe
  `firmware-mining/repro-nfc-rfid-detector-unchecked-nfc-acquire/repro.c`).
- Restlicher NFC-Werkzeug-Cluster aus demselben Rückstellpunkt weiterhin offen: `chameleon`,
  `nfc_relay`, `mifare_fuzzer`, `nfctools`, `uhf_rfid`, `nfc_canary`, `nfc_comparator`,
  `simultaneous_rfid_reader`.
