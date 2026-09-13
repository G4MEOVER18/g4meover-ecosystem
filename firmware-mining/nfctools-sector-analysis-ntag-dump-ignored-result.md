# NFC Tools — Sector Analysis meldet bei fehlgeschlagenem NTAG-Lesevorgang trotzdem Erfolg

## Was
Ich habe den zuletzt noch offenen Rückstellpunkt "Sektor-Analyse-Restumfang" des NFC-Tools-Clusters
(`applications/external/nfctools/`, Autor `wakdev`, `fap_version` 1.3, Quelle
`github.com/wakdev/nfctools-fz`) aufgegriffen: `scenes/other/nfc_tools_scene_sector_analysis.c`
(389 Zeilen) sowie `core/nfc_tools_ntag.c` (123 Zeilen) vollständig gelesen — bisher war diese Datei
nur zur Kontextprüfung eines anderen Fundes (DESFire `t4t_read_binary()`) überflogen worden, ohne
eigene Logikprüfung.

## Fund: MF-Ultralight/NTAG-Zweig ist der einzige der vier Protokollzweige, der das Ergebnis seines Dump-Aufrufs nicht prüft
`nfc_tools_sector_worker()` behandelt die vier unterstützten Protokolle in einer if/else-Kette. Drei
der vier Zweige (ICODE, DESFire, FeliCa) prüfen das Ergebnis ihres jeweiligen Lese-Helfers und
springen bei Fehlschlag sofort in den Fehlerpfad:

```c
// ICODE-Zweig — Ergebnis geprüft
bool ok = nfc_tools_icode_dump(app);
if(!ok) {
    notification_message(app->notifications, &sequence_error);
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventAnalysisDone);
    return 0;
}

// DESFire-/FeliCa-Zweig — analog: if(!desfire_ctx.success) { ...&sequence_error...; return 0; }
```

Der MF-Ultralight/NTAG-Zweig ruft denselben Musterhelfer auf, verwirft aber dessen Rückgabewert:

```c
} else if(app->detected_protocol == NfcProtocolMfUltralight) {
    nfc_tools_ntag_dump(app);          // <-- bool-Rückgabewert verworfen
```

`nfc_tools_ntag_dump()` (`core/nfc_tools_ntag.c:71`) gibt `false` zurück, wenn
`mf_ultralight_poller_sync_read_card()` fehlschlägt (Tag während des Lesens vom Feld genommen,
Kommunikationsfehler) — in diesem Fall hängt sie "Read error\ncode: N\n" an `app->info_str` an,
lässt `app->ndef_str` unangetastet (weiterhin leer aus dem `furi_string_reset()` am Worker-Anfang)
und kehrt zurück, **bevor** die eigentliche Seiten-Schleife (Seitenzahl-Zusammenfassung, Hex-/ASCII-
Dump) überhaupt läuft.

Da der Aufrufer diesen Rückgabewert nie ansieht, fällt der Worker anschließend unbedingt in:

```c
notification_message(app->notifications, &sequence_success);
view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventAnalysisDone);
```

— dieselbe Erfolgsmeldung (Ton + LED), die bei einem tatsächlich erfolgreichen Dump gespielt wird,
und dasselbe `NfcToolsEventAnalysisDone`, das den Scene-Handler in den regulären
Anzeigepfad schickt. Der Scene-Handler entscheidet die Anzeigeart über
`furi_string_size(app->ndef_str) > 0`: da `ndef_str` im Fehlerfall leer bleibt, landet die App in der
einfachen `TextBox`-Ansicht mit dem Inhalt von `info_str` — der sichtbare Text ("Read error\ncode: N")
ist also inhaltlich korrekt, aber die hörbare/haptische Rückmeldung (Erfolgston/-LED) widerspricht dem
tatsächlichen Ergebnis. Ein Nutzer, der die Karte kurz vor Ende des Lesevorgangs vom Feld nimmt,
bekommt exakt das "Erfolg"-Feedback, das er auch bei einem vollständigen, sauberen Dump bekäme.

Das ist dieselbe Fundklasse wie der bereits dokumentierte FeliCa-Write-/second-wait-Timeout-Fund und
der Remove-Password-Reihenfolgen-Fund in derselben App: eine gemeldete Erfolgs-Rückmeldung, die dem
tatsächlichen Ergebnis widerspricht — hier über den Notification-Kanal statt über den Text selbst.

Die drei übrigen Protokollzweige sind von diesem Muster nicht betroffen: ICODE prüft bereits korrekt
(s. o.), DESFire/FeliCa laufen über einen eigenen `FuriEventFlag`-Callback-Kontext mit geprüftem
`ctx.success`. `nfc_tools_mfc_dump()` (MIFARE Classic) ist als `void` deklariert und dokumentiert
bewusst kein Fehlschlagen — sie liest Block für Block mit mehreren Standardschlüsseln und trägt bei
jedem nicht lesbaren Block/Sektor `"?? ?? ..."`/`"-- -- ..."` statt der Hex-Daten ein, kommt aber immer
vollständig durch (kein einzelner Fehlerzustand, der die gesamte Funktion abbrechen lässt) — hier ist
`void` korrekt und kein Fund.

## Host-Repro
Eigenständiges Zustandsmodell der beiden beteiligten Funktionen (kein NFC-/Flipper-Bezug), das für
den Erfolgs- und den Fehlschlagfall vergleicht, welche Notification-Sequenz der Worker tatsächlich
abspielt — einmal wie ausgeliefert (ALT, Rückgabewert verworfen) und einmal mit Prüfung nach dem
Vorbild des ICODE-Zweigs (NEU):

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

Tatsächliche Ausgabe (lokal ausgeführt):
```
== ALT (nfc_tools_ntag_dump() return value discarded, as shipped) ==
Read succeeds (regression)   | notification: success | AnalysisDone: yes
Read fails (tag pulled)      | notification: success | AnalysisDone: yes  <-- CONTRADICTION (success sound on a failed read)

== NEU (result checked, mirrors the existing ICODE branch) ==
Read succeeds (regression)   | notification: success | AnalysisDone: yes
Read fails (tag pulled)      | notification: error   | AnalysisDone: yes

Bestaetigt: ALT meldet in 1 von 2 Faellen die falsche Benachrichtigung (Erfolg statt Fehler bei fehlgeschlagenem Lesevorgang), NEU in 0 von 2.
Regressionsfall (Lesevorgang erfolgreich) fuer ALT und NEU identisch: bestaetigt
ALL CHECKS PASSED
```
Der Regressionsfall (Lesevorgang erfolgreich) ist für ALT und NEU identisch — der Fix ändert nichts
am bereits korrekten Erfolgspfad, nur am Feedback bei einem fehlgeschlagenen NTAG-Lesevorgang.

## Betroffene Dateien
- `scenes/other/nfc_tools_scene_sector_analysis.c` — `nfc_tools_sector_worker()`, MF-Ultralight-Zweig
  (Zeile ~211–212), einziger Fix-Ort. `core/nfc_tools_ntag.c`/`nfc_tools_ntag_dump()` selbst bleibt
  unverändert (Rückgabewert existiert bereits, er wird nur nicht abgefragt).

## Risiko
Gering, rein Rückmeldungs-/UX-relevant, nicht speichersicherheitsrelevant. Die Änderung fügt eine
bereits im selben Worker etablierte Prüf-und-Abbruch-Struktur (identisch zum ICODE-Zweig zwei
Blöcke darunter) hinzu, keine neue API, keine geänderte Poller-/Lesesequenz. Kein Fall verschlechtert
sich gegenüber ALT: der Erfolgsfall bleibt unverändert, nur der bereits vorhandene, aber bisher
stumm verworfene Fehlerfall wird jetzt auch als solcher gemeldet.

## Port-Skizze / Fix
`nfc_tools_sector_worker()`, MF-Ultralight-Zweig, Rückgabewert wie beim ICODE-Zweig prüfen:

```c
// ── MF Ultralight / NTAG ────────────────────────────────────────────────
} else if(app->detected_protocol == NfcProtocolMfUltralight) {
    bool ok = nfc_tools_ntag_dump(app);
    if(!ok) {
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventAnalysisDone);
        return 0;
    }
```

Damit gilt nach jedem NTAG-Sektor-Analyse-Lauf: die Notification (Erfolg/Fehler) stimmt mit dem
tatsächlichen Ergebnis von `nfc_tools_ntag_dump()` überein, exakt wie bereits bei ICODE/DESFire/
FeliCa in derselben Funktion.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-sector-analysis-ntag-dump-result/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: Fix einbauen (`ufbt launch_app` gegen `nfctools`),
"Sector Analysis" gegen einen eigenen NTAG21x-Tag starten und den Tag bewusst **während** des
Lesevorgangs (nach dem Popup "Analyzing...", vor Abschluss) vom Feld nehmen. Vor dem Fix spielt die
App den Erfolgston/die Erfolgs-LED trotz sichtbarem "Read error"-Text; nach dem Fix spielt sie den
Fehlerton/die Fehler-LED, konsistent mit dem Text. Regressionsfall (Tag während des gesamten
Lesevorgangs stabil gehalten) muss unverändert die Erfolgsmeldung mit vollständigem Hex-/ASCII-Dump
zeigen.

## Zusätzlich geprüft, kein Fund
- `nfc_tools_felica_sector_cb()`/`nfc_tools_desfire_sector_cb()` (derselbe Datei): beide laufen über
  einen eigenen `FuriEventFlag`-Kontext mit korrekt geprüftem `ctx.success`, identisch zu den bereits
  in früheren Sitzungen verifizierten Poller-Callback-Mustern derselben App — kein Fund.
- `nfc_tools_mfc_dump()` (`core/nfc_tools_mfc.c`): `void`-Rückgabe ist hier korrekt, da die Funktion
  konstruktionsbedingt nie vollständig fehlschlägt (nicht lesbare Blöcke/Sektoren werden einzeln als
  `"?? ??..."`/`"-- --..."` markiert, die Schleife läuft immer bis zum Ende oder bis zum
  benutzerseitigen Stop) — kein äquivalenter Fund.
- `nfc_tools_sector_ascii_btn_cb()`/Szenen-Umschaltung zwischen Hex- und ASCII-`TextBox` (statisches
  `s_ascii_view_active`-Flag): einzige laufende App-Instanz auf dem Gerät, kein
  Mehrinstanz-/Nebenläufigkeitsproblem — kein Fund.
- `nfc_tools_ntag_dump()` selbst (Seiten-Schleife, Hex-/ASCII-Formatierung): feste Zielpuffergrößen
  über `furi_string_cat_printf` auf einem `FuriString*` (dynamisch wachsend), `pages_total` aus
  `mf_ultralight_get_pages_total(app->mful_type)`, `pages_read` aus dem tatsächlichen Poller-Ergebnis
  begrenzt die Schleifenbedingung nicht zusätzlich (`p < pages_total` unabhängig von `pages_read`,
  bereits korrekt über den `p < pages_read`-Zweig abgefangen) — kein Speicherzugriffsfehler.

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, Sparse-Checkout) bzw. direkt
  `github.com/wakdev/nfctools-fz` (identischer Stand, `fap_version` 1.3), Autor `wakdev`.
- API-87-Einordnung: reine Ergänzung einer bereits im selben Worker etablierten
  Prüf-und-Abbruch-Struktur (identisch zum bestehenden ICODE-Zweig), keine neue API-Fläche, kein
  firmwareversionsabhängiges Kompatibilitätsrisiko.
- Eigener Fund: `scenes/other/nfc_tools_scene_sector_analysis.c`, `nfc_tools_sector_worker()`,
  MF-Ultralight-Zweig, mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-nfctools-sector-analysis-ntag-dump-result/repro.c`). Damit ist der
  Rückstellpunkt "Sektor-Analyse-Restumfang" abgeschlossen; restlicher NFC-Werkzeug-Cluster nur noch
  `chameleon`/`nfc_relay` (Fremd-/Zweitgerät, außerhalb reiner Quelltextprüfung).
