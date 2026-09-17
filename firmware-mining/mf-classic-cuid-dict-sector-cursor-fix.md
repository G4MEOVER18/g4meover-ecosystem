# MF Classic CUID-Dictionary: Anzeige-Sektorzähler bekommt den Pass-Ende-Marker statt echter Sektoren

## Was
Ich habe aus der aus `unlshd-093` vorgemerkten Restliste den nächsten Kandidaten geprüft:
`cc3a1bc79` (PR #1115, "NFC: keep the CUID dictionary pass end marker out of the sector
counter"), betrifft bei Unleashed `mf_classic_extra_scenes.c`. Bei mir (`mntm-012`) existiert
diese Datei nicht — meine NFC-App hat eine eigene, älter strukturierte MF-Classic-Dict-Attack-Szene
(`applications/main/nfc/scenes/nfc_scene_mf_classic_dict_attack.c`), die die CUID-Dictionary-Funktion
über einen eigenen, Bitmap-basierten Ansatz implementiert (Schlüssel+Index-Paare aus einer
`.nfc`-Datei je Karten-UID, Filterung per 256-Bit-Bitmap in `NfcMfClassicDictAttackContext`) statt
Unleasheds sequenziellem Scan — kein direkt anwendbarer Datei-für-Datei-Port, siehe unten. Der
zugrunde liegende **Fehler** ist bei mir aber unabhängig vorhanden und exakt derselbe Fehlerklasse:
der Anzeige-Sektorzähler übernimmt den vom Poller als "Pass ist zu Ende" genutzten Marker-Wert
(eine Sektornummer eins hinter der letzten echten), obwohl der Marker nur für den Poller bestimmt
ist, nicht für die Anzeige.

## Fund: Anzeige-Sektorzähler wird ungeprüft mit dem Pass-Ende-Marker überschrieben, kein Reset zwischen Phasen

In `nfc_dict_attack_worker_callback()`, Zweig `MfClassicPollerEventTypeNextSector`,
`is_cuid_dict`-Fall:
```c
// In CUID mode, increment the key index and calculate sector from it
if(is_cuid_dict) {
    instance->nfc_dict_context.current_key_idx++;
    // Calculate sector from key_idx (each sector has 2 keys: A and B)
    instance->nfc_dict_context.current_sector =
        instance->nfc_dict_context.current_key_idx / 2;
    // Write back to event data so poller can read it
    mfc_event->data->next_sector_data.current_sector =
        instance->nfc_dict_context.current_sector;
} else {
```
`current_key_idx / 2` läuft über das letzte gültige Sektorenpaar hinaus, sobald alle Schlüssel
(A/B je Sektor) durchprobiert sind — für eine 1K-Karte (16 Sektoren, gültige Indizes 0–15) erreicht
der Ausdruck bei `current_key_idx` = 32 den Wert 16: exakt der von Unleashed im PR-Titel
beschriebene Fall ("Unlocking sector: 16" auf einer 1K-Karte). Dieser Wert landet ungeprüft sowohl
im Anzeigefeld `instance->nfc_dict_context.current_sector` als auch im Event, das der Poller als
Ende-der-Pass-Signal zurückliest (`mf_classic_poller_handler_next_sector()` in
`lib/nfc/protocols/mf_classic/mf_classic_poller.c`, Zeilen 824–827 bei mir: "In CUID mode, NFC app
manages sector based on key_idx - read it back"). Der Marker ist also für den Poller
funktional notwendig — nur seine ungeprüfte Zweitverwendung als Anzeigewert ist der Fehler.

Zusätzlich setzt `nfc_scene_mf_classic_dict_attack_prepare_view()` bei jedem Phasenwechsel
(CUID-Dictionary → User-Dictionary → System-Dictionary) zwar `dict_keys_total`,
`dict_keys_current` und den Dictionary-Header neu, **aber nirgends `current_sector`**:
```c
instance->nfc_dict_context.dict_keys_total =
    keys_dict_get_total_keys(instance->nfc_dict_context.dict);
dict_attack_set_total_dict_keys(
    instance->dict_attack, instance->nfc_dict_context.dict_keys_total);
instance->nfc_dict_context.dict_keys_current = 0;

dict_attack_set_callback(
    instance->dict_attack, nfc_dict_attack_dict_attack_result_callback, instance);
nfc_scene_mf_classic_dict_attack_update_view(instance);
```
Ein Reset auf `current_sector = 0` existiert bei mir nur in `nfc_scene_mf_classic_dict_attack_on_exit()`
— das läuft beim Verlassen der gesamten Szene, nicht beim Wechsel zwischen den drei Dictionary-Phasen
innerhalb eines Laufs. Endet die CUID-Phase also mit dem Marker-Wert im Anzeigefeld, bleibt dieser
bis zum ersten `NextSector`-Ereignis der User-Dictionary-Phase sichtbar stehen — exakt der von
Unleashed beschriebene Folgefehler ("nothing resets the counter between phases").

**Praktische Auswirkung:** Kein Speicherfehler — `current_sector` ist `uint8_t`, der Marker-Wert
(max. 40 bei einer 4K-Karte) liegt immer im gültigen `uint8_t`-Bereich, es wird nirgends indiziert.
Rein ein UI-Korrektheitsfehler: Bei jedem vollständigen CUID-Dictionary-Lauf zeigt die
Sektoranzeige kurzzeitig einen Sektor, den die Karte gar nicht hat (z. B. "16" auf einer 1K-Karte
mit nur 16 Sektoren, gültig 0–15), und dieser Anzeigewert bleibt bis zum ersten echten
Sektorwechsel der nächsten Phase (User-Dictionary) fälschlich stehen. Erreichbar bei jedem
vollständigen CUID-Dictionary-Lauf über eine eigene, bereits gespeicherte CUID-Dictionary-Datei —
kein externer Trigger, kein Angriffsvektor auf mein eigenes Gerät, nur eine irreführende Anzeige
während eines autorisierten eigenen Pentests.

## Host-Repro
Eigenständiges Host-Repro (keine Furi-/Scene-Manager-/Poller-Framework-Abhängigkeiten), isoliert
die zwei betroffenen Ausdrücke wortgleich aus beiden Fassungen sowie die reale Feldtypisierung
(`uint16_t current_key_idx`, `uint8_t current_sector`/`sectors_total` aus `nfc_app_i.h`) und die
drei echten MIFARE-Classic-Sektorzahlen (Mini 5, 1K 16, 4K 40, aus
`lib/nfc/protocols/mf_classic/mf_classic.c`):
```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```
Tatsächliche Ausgabe (lokal ausgeführt):
```
[Mini (0.3K) ] ALT reaches out-of-range display sector 5 (valid: 0..4)
[Mini (0.3K) ] NEU stays within 0..4 for the entire 18-step sweep - no out-of-range display value
[1K          ] ALT reaches out-of-range display sector 16 (valid: 0..15) - matches the reported "Unlocking sector: 16" bug
[1K          ] NEU stays within 0..15 for the entire 40-step sweep - no out-of-range display value
[4K          ] ALT reaches out-of-range display sector 40 (valid: 0..39)
[4K          ] NEU stays within 0..39 for the entire 88-step sweep - no out-of-range display value
Cross-phase check: ALT still shows the CUID pass's end marker at the start of the User Dictionary phase on all 3 card types; NEU resets to sector 0 on all of them
ALL CHECKS PASSED (344 checks)
```
Deckt alle drei realen Kartentypen vollständig ab (nicht nur die 1K-Stichprobe aus dem
Commit-Titel): für jeden Typ ein vollständiger Sweep über den gesamten `key_idx`-Bereich eines
CUID-Passes plus einigen Schritten darüber hinaus, mit Prüfung, ob der Anzeigewert je den
gültigen Sektorbereich verlässt. Zusätzlich ein Phasenübergangs-Check (CUID-Ende →
`prepare_view()` der nächsten Phase): ALT lässt den Marker-Wert unverändert stehen, NEU
zurückgesetzt auf 0 — reproduziert exakt beide von Unleashed beschriebenen Symptome. Ein separater
Regressionsblock bestätigt, dass der unveränderte Nicht-CUID-Zweig (`current_sector` direkt vom
Poller übernommen) von der Änderung nicht berührt wird. Unter
`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined` sowie gegengeprüft unter `-O0`:
keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen, identisches Ergebnis.

## Betroffene Dateien
- `applications/main/nfc/scenes/nfc_scene_mf_classic_dict_attack.c` — zwei Stellen:
  `nfc_dict_attack_worker_callback()` (Zweig `MfClassicPollerEventTypeNextSector`,
  `is_cuid_dict`-Fall) und `nfc_scene_mf_classic_dict_attack_prepare_view()`. Keine
  Signaturänderung, keine Auswirkung auf den Nicht-CUID-Zweig, auf die Bitmap-Filterlogik in
  `MfClassicPollerEventTypeRequestKey` oder auf den Poller selbst
  (`lib/nfc/protocols/mf_classic/mf_classic_poller.c` bleibt unverändert — er liest weiterhin
  denselben `next_sector_data.current_sector`-Wert zurück, der weiterhin unbedingt/unverändert
  publiziert wird).

## Risiko
Sehr gering. Reine Anzeigelogik-Änderung, keine geänderte Datenstruktur, keine geänderte
Poller-Kommunikation (das an den Poller zurückgemeldete `next_sector_data.current_sector` bleibt
in jedem Fall — auch am Marker-Wert — unverändert unbedingt gesetzt, nur die zusätzliche Übernahme
in das reine Anzeigefeld wird bedingt). Der einzige äußerlich sichtbare Effekt ist eine
korrektere Sektoranzeige; an der eigentlichen Dictionary-Attack-Logik (welche Schlüssel wann
gegen welchen Sektor probiert werden) ändert sich nichts.

## Port-Skizze / Fix
Eigener Fix nach demselben Prinzip wie Unleasheds Original (Marker weiterhin ans Poller-Event
publizieren, aber nicht mehr ungeprüft in die Anzeige übernehmen; Anzeige-Sektor bei jedem
Phasenwechsel zurücksetzen), an meine tatsächliche Codestruktur angepasst — kein 1:1-Diff
übernehmbar, da die Datei bei mir nicht existiert:

```c
// In CUID mode, increment the key index and calculate sector from it
if(is_cuid_dict) {
    instance->nfc_dict_context.current_key_idx++;
    // Calculate sector from key_idx (each sector has 2 keys: A and B)
    uint8_t next_sector = instance->nfc_dict_context.current_key_idx / 2;
    // The poller reads this back as its own cursor and uses it to detect the end of
    // the pass (one past the last sector) - publish it unconditionally. The display
    // cursor is different: only adopt it while it still names a sector the card has,
    // so the view never shows an out-of-range sector.
    mfc_event->data->next_sector_data.current_sector = next_sector;
    if(next_sector < instance->nfc_dict_context.sectors_total) {
        instance->nfc_dict_context.current_sector = next_sector;
    }
} else {
```
und in `nfc_scene_mf_classic_dict_attack_prepare_view()`, direkt nach dem bestehenden
`dict_keys_current = 0`-Reset:
```c
instance->nfc_dict_context.dict_keys_current = 0;
// A phase reports no sector until it has cleared its first one, else a CUID pass that ran
// to its end marker keeps showing that stale, out-of-range sector into the next phase.
instance->nfc_dict_context.current_sector = 0;
```
Unified-Diff gegen meine unveränderte `mntm-012`-Basis in
`firmware-mining/mf-classic-cuid-dict-sector-cursor-fix/nfc_scene_mf_classic_dict_attack_c.patch`
(`patch --dry-run -p1` gegen eine frische Basiskopie vorab verifiziert, real angewendet und
Byte-für-Byte gegen die separat editierte Fassung abgeglichen — identisch).

## Testhinweis (lokal nachzuvollziehen)
`firmware-mining/repro-mf-classic-cuid-dict-sector-cursor-fix/repro.c`, host-kompilierbar, keine
Flipper-/Hardware-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: Patch anwenden, `fbt` neu bauen und flashen,
danach eine MIFARE-Classic-1K-Karte auflegen und über "Dict Attack" mit vorhandener eigener
CUID-Dictionary-Datei (`nfc/assets/mf_classic_dict_<UID>.nfc`) einen vollständigen Lauf bis zum
Übergang in die User-Dictionary-Phase beobachten:
1. Während der CUID-Phase die Sektoranzeige bis zum letzten Schlüsselpaar verfolgen — sie darf nie
   über 15 hinausgehen (0-indiziert, 16 Sektoren auf einer 1K-Karte).
2. Beim Übergang in die User-Dictionary-Phase (Kopfzeile wechselt auf "MF Classic User
   Dictionary") muss die Sektoranzeige sofort auf 0 zurückspringen, nicht kurzzeitig bei 16
   (bzw. dem zuvor erreichten Marker-Wert) stehen bleiben.
3. Regressionstest: ein normaler (Nicht-CUID) Dictionary-Lauf ohne vorhandene CUID-Datei für die
   aufgelegte UID — Sektoranzeige und Ablauf müssen unverändert wie vor dem Patch funktionieren.
4. Kein spezielles Testgerät nötig; jede eigene MIFARE-Classic-1K-/4K-/Mini-Testkarte mit
   passender CUID-Dictionary-Datei reicht.

## Quelle
- `github.com/DarkFlippers/unleashed-firmware`, Commit `cc3a1bc79` (PR #1115, "NFC: keep the CUID
  dictionary pass end marker out of the sector counter"), Tag `unlshd-093` — als Fehlerklasse und
  Fix-Prinzip übernommen (Marker vom Anzeigefeld trennen, Anzeige bei Phasenwechsel
  zurücksetzen), nicht als 1:1-Diff, da meine Datei strukturell abweicht (siehe "Was").
- Momentum-Firmware, Tag `mntm-012`: `applications/main/nfc/scenes/nfc_scene_mf_classic_dict_attack.c`,
  `applications/main/nfc/nfc_app_i.h` (Feldtypen `NfcMfClassicDictAttackContext`),
  `lib/nfc/protocols/mf_classic/mf_classic_poller.c` (Marker-Rücklese-Logik, unverändert),
  `lib/nfc/protocols/mf_classic/mf_classic.c` (reale Sektorzahlen je Kartentyp) als
  Vergleichs-/Kontextbasis.
- Verwandt zum bereits gelieferten `nfc-unified-key-dictionary.md`-Port (dort ging es um die
  Schlüssel-**Verwaltung** — Add/List/Delete eigener Dictionary-Einträge — hier um die
  **Anzeige** während eines laufenden Dictionary-Attack-Passes; unterschiedliche Szenen, kein
  Überschneidungsrisiko).
- Aus derselben `unlshd-093`-Restliste weiterhin offen: `95c35fb82` (MIFARE-Classic-Schlüssel-
  Dictionary-Feature, #1118), `104d96f86` (Superrollo/GW60-SubGHz-Protokoll, #1068).
