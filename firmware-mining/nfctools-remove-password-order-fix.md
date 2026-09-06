# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: Schreibreihenfolge bei "Remove Password" widerspricht sich bei Abbruch

## Was
Ich habe den seit mehreren Sitzungen offenen Rückstellpunkt "Passwort-Entfernungs-Input" aus dem
NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`, `fap_version` 1.3, Quelle
`github.com/wakdev/nfctools-fz`) aufgegriffen und die beiden zugehörigen Input-Szenen sowie die
beiden Passwort-Write-Szenen vollständig gelesen: `scenes/other/nfc_tools_scene_remove_password_input.c`
(49 Zeilen), `scenes/other/nfc_tools_scene_set_password_input.c` (47 Zeilen),
`scenes/other/nfc_tools_scene_remove_password_write.c` (278 Zeilen) und
`scenes/other/nfc_tools_scene_set_password_write.c` (283 Zeilen). Beide Input-Szenen sind reine
Tastatureingabe-Wrapper ohne Fund. Der Fund liegt in der Schreibreihenfolge von
"Remove Password".

## Fund: Reihenfolge der beiden Schreibschritte spiegelt bei einem Abbruch nicht den tatsächlichen Tag-Zustand
`nfc_tools_remove_password_write_cb()` führt nach erfolgreicher `PWD_AUTH` genau zwei Schreibschritte
in einer einzigen RF-Sitzung aus, in dieser Reihenfolge:

```c
// Schritt 3: CONFIG <- { 0x04, 0x00, 0x00, 0xFF }   -- AUTH0 = 0xFF: Schutz komplett aus
MfUltralightError err = mf_ultralight_poller_write_page(mfu, cfg_page, &cfg_data);
if(err != MfUltralightErrorNone) { /* "Password error" folgt erst NACH diesem Schritt, s.u. */ }

// Schritt 4: PASSWORD <- { 0xFF, 0xFF, 0xFF, 0xFF }  -- Standardpasswort wiederherstellen
err = mf_ultralight_poller_write_page(mfu, pwd_page, &pwd_data);
```

`ctx->success` wird ausschließlich gesetzt, wenn **beide** Schritte durchlaufen. Bricht die
RF-Verbindung nach Schritt 3, aber vor/während Schritt 4 ab (Tag zu früh vom Feld genommen — der
naheliegendste Bedienfehler bei einem physischen Lese-/Schreibvorgang), meldet die App
"Password error\npage 0x.. (err:N)" — also einen Fehlschlag. Tatsächlich ist der
Passwortschutz auf dem Tag zu diesem Zeitpunkt aber schon **vollständig deaktiviert**: `AUTH0 = 0xFF`
bedeutet laut NTAG21x-Spezifikation, dass ab keiner Seite mehr eine Authentifizierung verlangt wird,
unabhängig vom Inhalt der PASSWORD-Seite (die zu diesem Zeitpunkt noch das alte, geheime Passwort
trägt). Der Nutzer bekommt "Fehler" gemeldet und geht plausibel davon aus, dass der Tag weiterhin
geschützt ist — dabei kann inzwischen jeder ihn ohne jedes Passwort beschreiben.

Das ist dieselbe Fundklasse wie der bereits dokumentierte RollForge-OTG-Ownership-Fund und der
RollJam-"Signal B als gespeichert behauptet, bevor es das ist"-Fund: eine gemeldete
Erfolgs-/Fehlermeldung, die dem tatsächlichen Gerätezustand widerspricht.

Die Geschwister-Funktion `nfc_tools_set_password_write_cb()` hat dieselbe zweistufige Struktur
(CONFIG dann PASSWORD), ist von diesem Widerspruch aber praktisch nicht betroffen: bricht sie nach
dem CONFIG-Schritt ab, bleibt die PASSWORD-Seite auf dem Standardwert `FF FF FF FF` stehen, sodass
ein zweiter Versuch mit der (dort fest verdrahteten) Standard-Authentifizierung weiterhin gelingt —
"Fehler" und "neues Passwort noch nicht aktiv" stimmen hier überein, kein Fix nötig.

## Host-Repro
Eigenständiges Zustandsmodell der beiden Schreibschritte (kein NFC-/Flipper-Bezug), das für jede
mögliche Abbruchstelle (0, 1 oder 2 von 2 Schritten erfolgreich) vergleicht, was die App meldet
gegen den tatsächlichen `AUTH0`-Zustand des Tags — einmal in der ausgelieferten Reihenfolge (ALT)
und einmal in der vorgeschlagenen (NEU):

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

Tatsächliche Ausgabe (lokal ausgeführt):
```
== ALT (CONFIG vor PASSWORD, wie ausgeliefert) ==
ALT, Abbruch vor Schritt 1       | nach 0/2 Schritten | App meldet: Fehler  | Tag tatsaechlich offen: nein
ALT, Abbruch nach Schritt 1      | nach 1/2 Schritten | App meldet: Fehler  | Tag tatsaechlich offen: ja     <-- WIDERSPRUCH
ALT, beide Schritte (Regression) | nach 2/2 Schritten | App meldet: Erfolg  | Tag tatsaechlich offen: ja

== NEU (PASSWORD vor CONFIG) ==
NEU, Abbruch vor Schritt 1       | nach 0/2 Schritten | App meldet: Fehler  | Tag tatsaechlich offen: nein
NEU, Abbruch nach Schritt 1      | nach 1/2 Schritten | App meldet: Fehler  | Tag tatsaechlich offen: nein
NEU, beide Schritte (Regression) | nach 2/2 Schritten | App meldet: Erfolg  | Tag tatsaechlich offen: ja

Bestaetigt: ALT widerspricht sich in 1 von 3 Faellen, NEU in 0 von 3 Faellen.
```
Regressionsfall (beide Schritte erfolgreich) ist für ALT und NEU identisch — der Fix ändert nichts
am Erfolgsfall, nur am Verhalten bei Abbruch nach genau einem der beiden Schritte.

## Betroffene Dateien
- `scenes/other/nfc_tools_scene_remove_password_write.c` — `nfc_tools_remove_password_write_cb()`,
  Schritte 3/4 (Zeile ~97–116), einziger Fix-Ort. Reine Reihenfolgenänderung der beiden bereits
  vorhandenen `mf_ultralight_poller_write_page()`-Aufrufe, keine neue API nötig.

## Risiko
Gering, rein sicherheitsrelevant bei der Meldung, nicht speichersicherheitsrelevant. Beide
Schreibaufrufe bleiben unverändert, nur ihre Reihenfolge innerhalb derselben bereits
authentifizierten RF-Sitzung tauscht — laut Kopfkommentar der Datei selbst ("Removal sequence,
source: user iOS code") ist die ausgelieferte Reihenfolge ohnehin nur einer Referenzimplementierung
entnommen, nicht durch eine Protokollvorgabe erzwungen (`PWD_AUTH` gilt für die gesamte
Feld-Sitzung, beide Zielseiten liegen bereits unter dem bestehenden `AUTH0`, das erst am Ende der
Sitzung geändert wird — die Reihenfolge der beiden Schreibzugriffe hat auf die Zulässigkeit keinen
Einfluss). Kein Fall verschlechtert sich gegenüber ALT: der Regressionsfall (beide Schritte
erfolgreich) bleibt identisch erhalten.

## Port-Skizze / Fix
Die beiden Schreibaufrufe in `nfc_tools_remove_password_write_cb()` vertauschen — zuerst PASSWORD
auf den Standardwert zurücksetzen, erst danach CONFIG (AUTH0) auf "kein Schutz" setzen:

```c
// Schritt 3 (neu): PASSWORD <- { 0xFF, 0xFF, 0xFF, 0xFF }
const MfUltralightPage pwd_data = {.data = {0xFF, 0xFF, 0xFF, 0xFF}};
MfUltralightError err = mf_ultralight_poller_write_page(mfu, pwd_page, &pwd_data);
if(err != MfUltralightErrorNone) {
    snprintf(ctx->err, sizeof(ctx->err), "Password error\npage 0x%02X (err:%d)", pwd_page, (int)err);
    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// Schritt 4 (neu): CONFIG <- { 0x04, 0x00, 0x00, 0xFF }
const MfUltralightPage cfg_data = {.data = {0x04, 0x00, 0x00, 0xFF}};
err = mf_ultralight_poller_write_page(mfu, cfg_page, &cfg_data);
if(err != MfUltralightErrorNone) {
    snprintf(ctx->err, sizeof(ctx->err), "Config error\npage 0x%02X (err:%d)", cfg_page, (int)err);
    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}
```

Damit gilt nach jedem denkbaren Abbruch: solange die Meldung "Fehler" lautet, ist `AUTH0` noch
unverändert (Tag weiterhin geschützt, ggf. bereits mit dem Standardpasswort statt dem alten
geheimen — mit leerem Eingabefeld beim nächsten Versuch weiterhin entfernbar); "Erfolg" wird nur
gemeldet, wenn der Schutz tatsächlich vollständig entfernt ist.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-remove-password-order/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: Fix einbauen (`ufbt launch_app` gegen
`nfctools`), mit einem eigenen NTAG213/215/216-Tag zunächst "Set Password" ausführen, danach
"Remove Password" starten und den Tag **während** des Schreibvorgangs (nach dem Popup
"Approach NTAG", vor Abschluss) bewusst vom Feld nehmen, um Schritt 3 (neu: PASSWORD-Schritt)
erfolgreich, Schritt 4 (neu: CONFIG-Schritt) aber unterbrochen zu simulieren. Danach mit einem
zweiten Tap prüfen: Vor dem Fix kann dieser Zustand (falls der Abbruch stattdessen nach dem
CONFIG-Schritt erfolgt) dazu führen, dass ein Tap ohne jedes Passwort bereits schreibt, obwohl die
App zuvor "Fehler" gemeldet hatte; nach dem Fix verlangt ein erneuter Tap in jedem
Abbruchfall weiterhin eine (ggf. bereits auf Standard zurückgesetzte) Authentifizierung, bis die
App tatsächlich "Erfolg" meldet.

## Zusätzlich geprüft, kein Fund
- `nfc_tools_scene_remove_password_input.c`/`nfc_tools_scene_set_password_input.c`: reine
  Tastatur-Eingabe-Wrapper (`Keyboard`-View, `ndef_buf1`), `keyboard_set_minimum_length()` korrekt
  auf 0 (Remove, leer = Standardpasswort) bzw. 1 (Set, Passwort zwingend) gesetzt — kein Fund.
- `nfc_tools_set_password_write_cb()`: MD5-Berechnung (`nfc_tools_md5()`) und
  Authentifizierungs-/Schreibreihenfolge gegengeprüft — bei Abbruch nach dem CONFIG-Schritt bleibt
  die PASSWORD-Seite auf dem Standardwert stehen, ein erneuter Versuch mit der dort fest
  verdrahteten Standard-Authentifizierung bleibt möglich; "Fehler" und tatsächlicher Zustand
  ("neues Passwort noch nicht aktiv") stimmen hier bereits überein — kein Fix nötig (siehe "Fund"
  oben).
- `ntag21x_get_pages()` (identisch dupliziert in beiden Dateien): deckt exakt die drei über
  `mf_ultralight_get_type_by_version()` unterscheidbaren NTAG21x-Varianten ab, `default: return
  false` fängt jeden anderen `MfUltralightType` — kein Fund, bereits in einer früheren Sitzung für
  die Geschwister-Funktion "NFC Commands" als Vergleichsmaßstab genutzt.
- Feste Puffergrößen (`pwd_bytes[4]`, `auth_bytes[4]`, `err[80]`) an allen Aufrufstellen mit
  konstanten Literalen bzw. `snprintf`/`strlcpy` mit expliziter Zielgröße befüllt — kein
  Speicherzugriffsfehler in beiden Dateien.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout, Stand dieser
  Sitzung): `applications/external/nfctools/`, `fap_author` "wakdev", Original-Quelle
  `github.com/wakdev/nfctools-fz`, `fap_version` 1.3.
- API-87-Einordnung: reine Reihenfolgenänderung zweier bereits genutzter, stabiler
  `mf_ultralight_poller_write_page()`-Aufrufe innerhalb derselben Datei — kein
  firmwareversionsabhängiges Kompatibilitätsrisiko, keine neue API-Fläche.
- Eigener Fund: `scenes/other/nfc_tools_scene_remove_password_write.c`, Schreibreihenfolge in
  `nfc_tools_remove_password_write_cb()`, mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-nfctools-remove-password-order/repro.c`).
- Restlicher Umfang des Clusters (~35 Schreib-Szenen für Kontakt/Bitcoin/WLAN/Social/etc.,
  Tag-Info-/QR-Code-/NDEF-Detail-Lesepfade) für künftige Sitzungen offen; der Rückstellpunkt
  "Passwort-Entfernungs-Input" ist mit dieser Sitzung abgeschlossen.
