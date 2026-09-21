# HID-über-BLE: Verbindungs-Callback wird erst NACH Advertising-Start registriert

## Was
Ich habe den seit der letzten Sitzung neu hinzugekommenen Unleashed-`dev`-Commit `8fdb70dd3`
(PR #1112, "HID: improve Mouse Jiggler timers and Bluetooth controls") geprüft. Der Commit
bündelt mehrere, voneinander unabhängige Fixes über vier Dateien (`hid.c`, `hid.h`,
`hid_mouse_jiggler.c`, `hid_mouse_jiggler_stealth.c`): Stop stoppt jetzt tatsächlich den Timer
statt ihn leerlaufen zu lassen, Verlassen der Ansicht setzt `running` zurück, die
Stealth-Variante bewaffnet ihren Einmal-Timer unter dem Modell-Lock neu (Race zwischen Stop und
Neubewaffnung), die Stealth-Bewegung ist nie 0 auf einer Achse, und über Bluetooth wartet der
Jiggler jetzt auf die Verbindung statt ins Leere zu senden. Für den hier üblichen
Sitzungsumfang habe ich daraus den einen, unabhängig verständlichen und in sich vollständig
korrigierbaren Fund herausgelöst, der **nicht** auf die (bei mir noch fehlenden) übrigen
Jiggler-Änderungen dieses Commits angewiesen ist: die Registrierungsreihenfolge von
Advertising-Start und Verbindungsstatus-Callback in `hid_ble_app()`
(`applications/system/hid_app/hid.c`). Die betroffenen Zeilen sind bei mir (`mntm-012`) direkt
gegen den unmittelbaren Vorgänger-Commit `8fdb70dd3^` byteidentisch zum Unleashed-Vorher-Stand
verifiziert (`furi_hal_bt_start_advertising();` gefolgt von
`bt_set_status_changed_callback(...)`, exakt dieselbe Zeilenfolge in derselben Funktion).

## Fund: Eine bereits gekoppelte Gegenstelle kann sich verbinden, bevor der Callback registriert ist
```c
furi_check(app->ble_hid_profile);

furi_hal_bt_start_advertising();
bt_set_status_changed_callback(app->bt, bt_hid_connection_status_changed_callback, app);

dolphin_deed(DolphinDeedPluginStart);
```
`bt_set_status_changed_callback()` legt ausschließlich einen Funktionszeiger + Kontext im
Bt-Service ab und ändert selbst nichts am Funkzustand. `furi_hal_bt_start_advertising()` ist der
Aufruf, der das Advertising tatsächlich beginnt — und genau ab diesem Zeitpunkt kann eine bereits
gekoppelte Gegenstelle (z. B. ein Rechner, mit dem vorher schon einmal per BLE-HID gekoppelt
wurde) sofort neu verbinden, noch bevor die Funktion überhaupt weiterläuft. Die tatsächliche
Ereigniszustellung läuft über den zweiten Bluetooth-Kern und dessen Interrupt-/RTOS-Anbindung,
nicht synchron innerhalb dieser einen Codezeile — der Reihenfolgefehler besteht unabhängig davon,
ob das konkrete Ereignis "noch während des Aufrufs" oder "kurz danach, aber noch vor der
nächsten Zeile" eintrifft: Entscheidend ist nur, welcher der beiden Aufrufe zuerst passiert.
Registriert `hid_ble_app()` den Callback erst nach dem Start des Advertisings, geht ein
Verbindungsereignis, das in diesem Fenster eintrifft, komplett verloren — der Bt-Service hat zu
diesem Zeitpunkt noch keinen Callback, an den er es weiterleiten könnte, und liefert es später
nicht erneut nach.

**Praktische Auswirkung:** `bt_hid_connection_status_changed_callback()` ist die einzige Quelle,
über die **jede** BLE-HID-Ansicht ihren `connected`-Modellwert erfährt — bei mir u. a.
`hid_mouse_jiggler_set_connected_status()` und `hid_mouse_jiggler_stealth_set_connected_status()`
(beide Ansichten zeigen bereits jetzt, unabhängig vom größeren PR #1112-Umfang, ein
Verbindungssymbol `I_Ble_connected_15x15`/`I_Ble_disconnected_15x15`, gesteuert über exakt dieses
Modellfeld), außerdem alle übrigen HID-Ansichten (Keynote, Keyboard, Numpad, Media, MusicMacOS,
Movie, Mouse, MouseClicker, PTT, TikTok). Geht das Startup-Verbindungsereignis durch den
Reihenfolgefehler verloren, bleibt das Symbol auf "getrennt" stehen, obwohl tatsächlich eine
Verbindung besteht — und bleibt es, bis irgendein späteres, unabhängiges Statusereignis (z. B.
eine echte Trennung) eintritt. Kein Speicherfehler, reiner Zustands-/Anzeigefehler, der aber bei
jeder BLE-HID-Sitzung mit bereits gekoppelter Gegenstelle auftreten kann (USB-HID ist von diesem
Fund nicht betroffen, dort gibt es keinen Advertising-Schritt).

## Host-Repro
Eigenständiges Host-Repro (keine Flipper-/Hardware-/BLE-Stack-Abhängigkeit), modelliert die
beiden Operationen (Callback-Registrierung im Bt-Service, Start des Advertisings) sowie ein
synchron zustellbares "bereits gekoppelte Gegenstelle verbindet erneut"-Ereignis, das exakt an
den Advertising-Start gekoppelt ist — unabhängig von der Reihenfolge der beiden echten Aufrufe,
nur die relative Reihenfolge von Registrierung und Advertising-Start entscheidet, ob das
Ereignis einen Empfänger hat:
```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```
Tatsächliche Ausgabe (lokal ausgeführt, auch unter `-O0` gegengeprüft, identisch):
```
[group1] no bonded device in range: ALT connected=0 invocations=0, NEU connected=0 invocations=0 (both idle, as expected)
[group2] bonded device reconnects during startup: ALT connected=0 invocations=0 (event lost), NEU connected=1 invocations=1 (event delivered)
[group3] regression, ordinary post-startup connect/disconnect under NEU: final connected=0 invocations=2 (both events delivered exactly once each)
ALL CHECKS PASSED (12 checks)
```
Gruppe 1 bestätigt, dass ohne Startup-Race beide Reihenfolgen identisch (und unauffällig)
bleiben — der Fund tritt nicht allgemein auf, nur im konkreten Rennfenster. Gruppe 2 ist der
eigentliche Fund: ALT verliert das Verbindungsereignis vollständig (0 Aufrufe, Modell bleibt bei
seinem Initialwert `false`), NEU erhält es genau einmal und setzt `connected` korrekt auf `true`.
Gruppe 3 ist der Regressionsfall: ein späteres, gewöhnliches Verbinden/Trennen (unabhängig vom
Startup-Fenster) wird unter NEU weiterhin genau je einmal zugestellt — die frühere Registrierung
verdoppelt oder verschluckt keine späteren Ereignisse. Keine ASan-/UBSan-Meldungen, keine
Compiler-Warnungen unter `-Wall -Wextra`.

## Betroffene Dateien
- `applications/system/hid_app/hid.c` — `hid_ble_app()`, genau zwei Zeilen vertauscht, kein
  neuer Code, keine Signaturänderung. Wirkt sich nur auf den BLE-HID-Einstiegspunkt aus, der
  USB-HID-Einstiegspunkt (`hid_usb_app()`) kennt keinen Advertising-Schritt und ist unberührt.

## Risiko
Sehr gering. Reine Vertauschung zweier bereits vorhandener, voneinander unabhängiger Aufrufe
(Callback-Registrierung hat selbst keine Nebenwirkung auf den Funkzustand, Advertising-Start
liest den zu diesem Zeitpunkt bereits gesetzten Callback nur passiv aus, falls ein Ereignis
eintritt) — kein neuer Fehlerpfad, keine geänderte Aufräum-Reihenfolge (`bt_set_status_changed_callback(app->bt, NULL, NULL);`
beim Beenden bleibt unverändert an seiner Stelle). Sichtbare Verhaltensänderung ist exakt die
beabsichtigte Korrektur: das Verbindungssymbol auf allen BLE-HID-Ansichten (inkl. beider
Mouse-Jiggler-Varianten) zeigt eine bereits beim Start bestehende Verbindung korrekt an, statt
bis zum nächsten unabhängigen Statuswechsel auf "getrennt" zu verharren.

## Port-Skizze / Fix
```c
furi_check(app->ble_hid_profile);

bt_set_status_changed_callback(app->bt, bt_hid_connection_status_changed_callback, app);
furi_hal_bt_start_advertising();

dolphin_deed(DolphinDeedPluginStart);
```
1:1 Unleasheds Reihenfolge übernommen, kein eigener Ansatz nötig. Unified-Diff gegen meine
unveränderte `mntm-012`-Basis in
`firmware-mining/hid-ble-status-callback-registered-after-advertising-fix/hid_c.patch`
(`patch --dry-run -p1` gegen eine frische Basiskopie vorab verifiziert, real angewendet und
Byte-für-Byte gegen die separat host-getestete Fassung abgeglichen — identisch).

## Testhinweis (lokal nachzuvollziehen)
`firmware-mining/repro-hid-ble-status-callback-registered-after-advertising/repro.c`,
host-kompilierbar, keine Flipper-/Hardware-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe
Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: Patch anwenden, `fbt`/`ufbt` neu bauen und
flashen, danach mit einem bereits einmal gekoppelten Rechner/Smartphone in Reichweite eine
BLE-HID-Ansicht (z. B. Mouse Jiggler) öffnen — das Bluetooth-Symbol muss unmittelbar "verbunden"
zeigen, ohne dass vorher eine sichtbare Trennung/Neuverbindung nötig ist. Zum Gegenprobe-Vergleich
vor dem Patch: bei wiederholtem Öffnen/Schließen derselben Ansicht mit bereits verbundener
Gegenstelle zeigt sich das Symbol gelegentlich zunächst "getrennt", obwohl die Verbindung laut
Host (Maus-/Tastatur-Eingaben kommen bereits an) tatsächlich sofort funktioniert — das
Zeitfenster ist vom genauen Timing des Gegenstellen-Reconnects abhängig und daher nicht bei
jedem Versuch reproduzierbar, aber bei mehreren Wiederholungen beobachtbar.

## Quelle
- `github.com/DarkFlippers/unleashed-firmware`, Commit `8fdb70dd3` (PR #1112, "HID: improve
  Mouse Jiggler timers and Bluetooth controls"), Stand `dev` nach `unlshd-093`.
- Zum Vergleich herangezogen: `github.com/Next-Flip/Momentum-Firmware`, Tag `mntm-012`
  (`applications/system/hid_app/hid.c`) — betroffene Zeilen byteidentisch zum
  Unleashed-Vorher-Stand.
- API-87-Einordnung: reine Anwendungsdatei (`applications/system/hid_app`), keine
  Signatur-/Struct-Änderung — kein Kompatibilitätsrisiko, 1:1 portierbar.
- Aus demselben Commit zurückgestellt (nicht in diesem Vorschlag enthalten, da sie auf die bei
  mir noch fehlende `hid_model_connected()`-Makro-Infrastruktur bzw. auf den ebenfalls noch
  nicht übernommenen `hid-jiggler-stealth-move-bounds-fix.md`-Vorschlag aufbauen und deutlich
  mehr Dateien/Zustandslogik anfassen): Stop-Taste stoppt den Jiggler-Timer nicht wirklich,
  Verlassen der Ansicht setzt `running` nicht zurück, Stealth-Neubewaffnung-Race unter dem
  Modell-Lock, Stealth-Bewegung kann 0 sein, Jiggler wartet über BLE nicht auf die Verbindung
  vor dem Senden — für eine künftige, eigens dafür vorgesehene Sitzung vorgemerkt (setzt sinnvoll
  auf dem hier bereits vorgeschlagenen `hid-jiggler-stealth-move-bounds-fix.md` auf).
