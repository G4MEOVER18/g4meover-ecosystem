# USB Keyboard Fuzzer ("USB Fuzzer") — API-87-Status

Neue Recherche außerhalb bestehender Rückstellpunkte: gezielt nach einer Ergänzung für die
Kategorie "BadUSB / HID" gesucht (bislang nur eigenes Bad USB und Mouse Click Recorder), dabei
`n-92/kb_fuzzer` gefunden — eine eigenständige, sehr kleine App, kein RogueMaster-Bündel-Fund.

## Quelle
- Repo: `n-92/kb_fuzzer` (GitHub, `master`-Branch, 3 Commits, 20 Stars/2 Forks — sehr kleines,
  wenig aktives Projekt). `README.md`: modifizierte Fassung von Gabriel Cirligs ursprünglichem
  "flipper usb keyboard"; Original unter BSD-2, eigene Änderungen unter MIT (`LICENSE.md`
  verifiziert, kein separates `LICENSE` im Root — zunächst 404, korrigiert gefunden).
  Ausdrücklich gegen die **Xtreme-Firmware** gebaut (README verweist auf deren Build-Anleitung),
  nicht gegen Momentum/Unleashed/Official getestet.
- Vollständiger Quellbestand (klein genug für vollständige Durchsicht): `usb_hid.c/.h`
  (App-Gerüst, Menü, Exit-Dialog), `views/usb_hid_keyboard.c/.h` (eigentliche Fuzz-Logik +
  Anzeige), `application.fam` (`appid="usb_keyboard_fuzz"`, `fap_category="Misc"`).

## Was
Schaltet das Gerät in den USB-HID-Tastatur-Modus (`furi_hal_usb_set_config(&usb_hid, NULL)`,
identisches Muster zur eigenen Bad-USB-App) und sendet bei **jedem** Tastendruck am Flipper
selbst einen neu generierten Zufalls-String (1–20 Zeichen aus Buchstaben/Ziffern/Sonderzeichen)
gefolgt von Enter an den per USB verbundenen Host — `send_string()` mappt jedes Zeichen über
`HID_ASCII_TO_KEY()` auf einen HID-Keycode und drückt/löst ihn sofort. Kein Angriffsskript wie
beim eigenen Bad USB (DuckyScript, deterministische Payload), sondern reines Zufalls-Eingabe-
Rauschen — die on-screen "Tastatur"-Ansicht ist dabei nur eine optische Hülle, sie sendet bei
jedem Input-Event einen neuen Zufalls-String statt des angezeigten/anvisierten Zeichens.

## Nutzen für mich
Andere Kategorie als das bestehende Bad USB: dort geht es um **gezielte** Payload-Injektion
(bekannter Befehlsablauf), hier um **robustheitstestendes Rauschen** gegen die
Texteingabe-Verarbeitung eines angeschlossenen Zielsystems (Kiosk-/Terminal-Anmeldemasken,
eingebettete Geräte mit einfachen USB-Tastatur-Eingabefeldern, Formularvalidierung) — autorisierte
Fuzzing-Variante von HID-Input, die ich bisher nicht abdecke. Klein genug, um bei Bedarf komplett
neu zu schreiben statt zu portieren.

## API-87-Status
🔧 **Bricht den Build gegen `mntm-012` unverändert.** Frischer, unauthentifizierter
`git clone --depth 1 --branch mntm-012 https://github.com/Next-Flip/Momentum-Firmware.git`
(Commit `e1784e74`) gegen den in `views/usb_hid_keyboard.c` verwendeten Symbolbestand
abgeglichen:
- `furi_hal_hid_kb_press()`/`_release()`/`_release_all()`, `HID_ASCII_TO_KEY()`,
  `HID_KEYBOARD_NONE`/`_RETURN`/`_DELETE`, `furi_hal_usb_get_config()`/`_unlock()`/`_set_config()`,
  `usb_hid`-Interface (`targets/furi_hal_include/furi_hal_usb_hid.h`,
  `targets/f7/furi_hal/furi_hal_usb_hid.c`) — alle unverändert vorhanden, identisch zum
  eigenen Bad-USB-Backend.
- **`HID_KEYBOARD_L_CTRL`, `HID_KEYBOARD_L_ALT`, `HID_KEYBOARD_L_GUI`** (drei Vergleiche in
  `usb_hid_keyboard_draw_key()`, zur Großschreibung der Anzeige bei gedrückter Modifikatortaste)
  **existieren in `furi_hal_usb_hid.h` bei mir nicht** — nur die Bitmasken-Varianten
  `KEY_MOD_LEFT_CTRL`/`KEY_MOD_LEFT_SHIFT`/`KEY_MOD_LEFT_ALT`/`KEY_MOD_LEFT_GUI` sind vorhanden.
  Drei undefinierte Bezeichner → Kompilierfehler, kein Laufzeitproblem.
- **Praktisch folgenlos beheben:** `usb_hid_keyboard_keyset[][]` enthält nur einen einzigen
  aktiven Eintrag (Breite 12, Beschriftung "Pump!", `value = 0`) — die restlichen 23 Zellen des
  2×12-Rasters sind nullinitialisiert (Breite 0, wird vor der defekten Stelle bereits
  übersprungen). Da `HID_KEYBOARD_NONE` ebenfalls `0x00` ist, ist `key.value == HID_KEYBOARD_L_CTRL`
  (und die beiden Schwesterprüfungen) zur Laufzeit so oder so **niemals wahr** — sichtbares
  Überbleibsel aus der Bad-USB-eigenen Bildschirmtastatur-Ansicht, von der dieser Code sichtbar
  abstammt (identisches Zeichenraster-/Zeichnungsmuster), hier aber auf einen einzigen
  Riesenknopf reduziert. Port-Skizze: die drei toten Vergleiche ersatzlos streichen (oder, falls
  eine spätere Erweiterung um mehr Tasten gewünscht ist, durch
  `(model->ctrl && (key.value & KEY_MOD_LEFT_CTRL))` u. Ä. ersetzen) — Ein-Zeilen-Änderung je
  Symbol, kein struktureller Eingriff.
- `application.fam`: `requires=["gui"]`, `stack_size=1*1024` — unverändert übernehmbar.

## Quelltext-Review (Speichersicherheit) und gefundener Fund
`usb_hid_keyboard_process()` (aufgerufen bei **jedem** Tasten-Event der View):
```c
char* ptr_rand_string = malloc(random_length);
send_string(create_rand_string(ptr_rand_string, random_length));
if (ptr_rand_string != NULL) {
    free(ptr_rand_string);
}
```
Das `malloc()`-Ergebnis wird in `create_rand_string()` (schreibt `size - 1` Zeichen plus
Null-Terminator hinein) und in `send_string()` **verwendet, bevor** die NULL-Prüfung greift — die
Prüfung schützt nur noch den `free()`-Aufruf, nicht die vorangehende Nutzung. Bei einem
fehlgeschlagenen `malloc()` (Speicherdruck; `random_length` liegt zwischen 1–20 Byte, also klein,
aber bei einem Dauerlauf mit sehr vielen aufeinanderfolgenden Alloc/Free-Zyklen auf begrenztem
Heap nicht grundsätzlich ausgeschlossen) dereferenziert `create_rand_string()` sofort einen
Nullzeiger — Absturz der eigenen App, kein Zielsystem-Risiko. Host-Repro (Funktion 1:1
transkribiert, `malloc()` durch einen erzwungenen Fehlschlag ersetzt) unter ASan/UBSan verifiziert:
ALT (Nutzung vor Prüfung) stürzt mit `SEGV`/"store to null pointer" ab, NEU (Prüfung vor Nutzung)
beendet sich sauber — in
`ideas/repro-usb-hid-keyboard-fuzzer-null-check-order/repro.c`. Fix: die bereits vorhandene
`if (ptr_rand_string != NULL)`-Prüfung vor statt nach `create_rand_string()`/`send_string()`
platzieren (Ein-Zeilen-Verschiebung, kein neuer Codepfad).

Alle übrigen Speicherzugriffe (`create_rand_string()` selbst mit gültigem Puffer,
`key_string[5]`-Ziel in `strcpy()` innerhalb `usb_hid_keyboard_draw_key()` gegen den
5-Byte-Puffer bei maximal 4-Zeichen-Tastenbeschriftungen im vorhandenen Tastensatz) unauffällig.

## Aufwand
Gering. Vier Quelldateien, ~250 Zeilen insgesamt, keine Zusatzhardware (nutzt die eingebaute
USB-Buchse wie Bad USB). Migration = drei tote Vergleiche streichen + eine
NULL-Prüfungs-Verschiebung — beides ohne Funktionsverlust, da beide Stellen bereits heute
folgenlos sind (nie wahr bzw. nur bei Alloc-Fehlschlag relevant).

## Sicherheitsrahmen
Aktiv (sendet HID-Eingaben) — ausschließlich an eigenen oder ausdrücklich für den Test
freigegebenen Zielsystemen einsetzen (CONSTRAINTS Punkt 5). Sinnvoller Einsatzrahmen: robustheits-
testende Eingabe gegen ein eigenes Terminal/Kiosk-System vor produktivem Einsatz, nicht gegen
fremde/produktive Systeme ohne Freigabe.

## Offen
- Fix + lokale Bauprobe (Klon gegen die eigene `mntm-012`-Basis, `ufbt launch_app`/`fbt`,
  `fap_category` ggf. auf eine bereits genutzte Kategorie statt `Misc` anpassen), danach
  Funktionstest gegen ein eigenes Testsystem (Text-Eingabefeld beobachten, ob laufend
  Zufalls-Strings + Enter ankommen).
- Nur ein Video (`youtube.com/watch?v=573f5jhSXeg`) als einzige weitere Dokumentation über das
  README hinaus — nicht abgerufen (kein Recherchebedarf über den bereits vollständig gelesenen
  Quelltext hinaus).
