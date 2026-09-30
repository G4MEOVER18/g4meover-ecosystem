# NFC MFC CUID-Dictionary-Scan: Ladeanzeige (Verdrahtung des Loading-Widget-Ports)

## Was
Ich habe den in `loading-widget-label-and-progress-bar.md` bewusst zurückgestellten
Verdrahtungspunkt aufgegriffen: die generische `Loading`-Widget-Erweiterung um Label und
Progress-Bar (Port aus Unleashed, Commits `69c03681`/`9e4951074`, bei mir bislang nur als
Patch auf `applications/services/gui/modules/loading.c`/`.h` vorgeschlagen, noch nicht
verdrahtet) an genau der Stelle einsetzen, für die ich sie selbst als nächsten Kandidaten
vorgemerkt hatte: der CUID-Dictionary-Scan beim MIFARE-Classic-Wörterbuchangriff.

Mein `applications/main/nfc/scenes/nfc_scene_mf_classic_dict_attack.c` (identisch zum
mntm-012-Ausgangsstand, gegen einen frischen `Next-Flip/Momentum-Firmware`-Klon sowie gegen
meine eigene `G4MEOVER18/G4MEOVER-FW`-Basis abgeglichen — beide byte-identisch) liest beim
Betreten der Szene (`nfc_scene_mf_classic_dict_attack_on_enter()`) zuerst synchron das
gesamte UID-spezifische Wörterbuch (`nfc/assets/mf_classic_dict_<UID>.nfc`) Zeile für Zeile
ein, um eine Bitmap der vorhandenen Schlüsselindizes aufzubauen — **bevor** überhaupt auf die
eigentliche `NfcViewDictAttack`-Ansicht umgeschaltet wird
(`view_dispatcher_switch_to_view(..., NfcViewDictAttack)` steht erst danach in
`_on_enter()`). Während dieses Scans bleibt schlicht die zuvor sichtbare Ansicht der
vorherigen Szene eingefroren stehen — keine Spinner-Animation, kein Text, keine
Fortschrittsanzeige, nichts. Ich habe das gegen Unleasheds `dev`-Stand geprüft: Unleashed
zeigt an genau dieser Stelle (`mf_classic_extra_scenes.c`, dieselbe Struktur wie meine,
Unleashed hat sie nur in eine eigene Datei ausgelagert) schon länger zumindest einen
beschrifteten Spinner (`nfc_show_loading_label_popup()`), den ihr Commit `418d802b` (PR
#1167, "Show progress while a large CUID dictionary loads on Read", 22.09.2026) seither um
genau dieselbe Fortschrittsleiste ergänzt, die ich mit dem `Loading`-Widget-Port bereits
vorbereitet hatte. Bei mir fehlt aber schon die Vorstufe (der beschriftete Spinner selbst) —
ich habe kein `views/loading_label.c`-Äquivalent, mein `nfc_app.c` hat bislang nur den
nackten `Loading`-Spinner (`instance->loading`/`nfc_show_loading_popup()`) für andere,
kürzere Ladevorgänge (z. B. `nfc_supported_cards_load_cache()`).

Bei einem großen, populären UID (z. B. eine oft wiederverwendete Werks-/Transit-/
Zutrittskarte mit vielen zuvor gesammelten Schlüsseln) kann dieser Scan durch die
Zeilen-für-Zeile-Lesung über `keys_dict_get_next_key()` spürbar dauern — ohne jede
Rückmeldung sieht das für mich am Gerät wie ein Hänger aus.

## Nutzen
- Schließt eine eigene, tatsächlich schlechtere Lücke als der von Unleashed behobene Zustand:
  ich hatte nicht einmal deren Vorher-Zustand (bloßer Spinner), sondern gar keine Anzeige
  während dieses Scans.
- Nutzt ausschließlich bereits vorgeschlagene, additive Infrastruktur
  (`loading_set_text()`/`loading_set_progress()`/`loading_reset_progress()` aus
  `loading-widget-label-and-progress-bar.md`) — keine neue Abhängigkeit, kein zusätzliches
  API-Symbol jenseits dessen, was dieser Port bereits mitbringt.
- Schließt den in `loading-widget-label-and-progress-bar.md` selbst benannten
  Rückstellpunkt ("nfc_app.c für das UID-Dictionary-Label") ab, an genau der dort skizzierten
  Stelle (zweite, eigenständige `Loading`-Instanz statt der ohnehin schon für andere
  Ladevorgänge genutzten, damit Label/Bar nie auf dem einfachen Spinner-Bildschirm
  auftauchen — exakt Unleasheds eigene Begründung für die getrennte Instanz).

## Betroffene Dateien
Unified-Diffs beiliegend:
`firmware-mining/nfc-mfc-cuid-dict-scan-progress-port/{nfc_app_i_h,nfc_app_c,nfc_scene_mf_classic_dict_attack_c}.patch`.

**Setzt den bereits vorgeschlagenen `loading-widget-label-and-progress-bar`-Patch auf
`applications/services/gui/modules/loading.c`/`.h` voraus** (liefert
`loading_set_text()`/`loading_set_progress()`/`loading_reset_progress()`, ohne die
existiert dieser Port nicht).

1. `applications/main/nfc/nfc_app_i.h`:
   - Neues Feld `Loading* loading_dict;` neben dem bestehenden `Loading* loading;`.
   - Neuer Eintrag `NfcViewLoadingDict` im `NfcView`-Enum.
   - Zwei neue Deklarationen: `nfc_show_loading_dict_popup(void* context, const char* text,
     bool show)` (Pendant zu `nfc_show_loading_popup()`, aber auf der zweiten Instanz, mit
     Textlabel) und `nfc_set_loading_dict_progress(void* context, float progress)`.
2. `applications/main/nfc/nfc_app.c`:
   - `loading_dict` wird in `nfc_app_alloc()` analog zu `loading` alloziert und als
     `NfcViewLoadingDict` registriert, in `nfc_app_free()` symmetrisch wieder freigegeben.
   - Zwei neue Funktionen: `nfc_show_loading_dict_popup()` (setzt beim Anzeigen den Text
     über `loading_set_text()`, schaltet auf `NfcViewLoadingDict` um, hebt/senkt die
     Timer-Priorität exakt wie das bestehende `nfc_show_loading_popup()`; setzt beim
     Ausblenden zusätzlich `loading_reset_progress()` zurück, damit ein späterer Aufruf nicht
     mit der zuletzt gezeigten Balkenposition startet) und `nfc_set_loading_dict_progress()`
     (dünner Wrapper um `loading_set_progress()` auf der zweiten Instanz).
3. `applications/main/nfc/scenes/nfc_scene_mf_classic_dict_attack.c`:
   - Im CUID-Zweig von `nfc_scene_mf_classic_dict_attack_prepare_view()`: unmittelbar vor dem
     bereits bestehenden Scan-Once-Schleifendurchlauf `nfc_show_loading_dict_popup(instance,
     "Loading CUID\nindex...", true)`, direkt danach `stream_size(dict->stream)` einmalig
     gelesen (dieselbe Formel wie Unleasheds PR #1167: `dict->total_keys *
     dict->key_size_symbols * 100 / dict_size`, mit `MIN(..., 100U)` geklemmt, da eine Datei
     ohne exaktes Vielfaches der Zeilenlänge — z. B. fehlende letzte Newline — den rohen Wert
     über 100 treiben kann). Fortschritt wird pro gelesenem Schlüssel aktualisiert, sofern
     `dict_size != 0` (Schutz vor Division durch Null bei einer fehlgeschlagenen
     Größenabfrage — dann bleibt einfach nur der Spinner+Label ohne Balken sichtbar).
   - Direkt nach der bestehenden `keys_dict_rewind(dict)`-Zeile (die sowohl den
     Erfolgs- als auch den "0 Schlüssel gefunden"-Ausstiegspfad durchläuft, bevor die beiden
     sich trennen) `nfc_show_loading_dict_popup(instance, NULL, false)` — ein einziger
     Ausblendpunkt deckt damit beide Ausgänge ab, kein zweiter Aufruf nötig.
   - `MIN()` (aus `furi/core/common_defines.h`, transitiv über das bereits eingebundene
     `furi.h`) und `stream_size()` (aus `toolbox/stream/stream.h`, transitiv über das bereits
     eingebundene `toolbox/stream/buffered_file_stream.h`) sind beide bereits ohne neuen
     `#include` verfügbar — geprüft.

Keine andere Datei betroffen. Die Bitmap-Aufbaulogik selbst (Bit-Setzen, Schlüsselindex-
Extraktion) bleibt unverändert — nur die neuen Aufrufe kommen dazu.

## Risiko
- **Host-seitig verifiziert, nicht auf echtem Display**: Das Repro prüft die reine
  Fortschritts-Formel (Klemmung, Monotonie, Divide-by-Zero-Schutz) sowie die
  Kontrollfluss-Reihenfolge (Anzeigen/Ausblenden/Fortschritts-Update-Aufrufe an den exakt
  richtigen Stellen, für alle Ausgänge des Scans) 1:1 gegen die echte Funktionsstruktur — die
  tatsächliche Pixeldarstellung von Label und Balken selbst ist bereits durch das Repro des
  vorgelagerten `loading-widget-label-and-progress-bar`-Ports abgedeckt, hier nicht erneut
  geprüft.
- Zwei zusätzliche `Loading`-Instanzen kosten zusätzlichen RAM (eine `LoadingModel` inkl.
  `IconAnimation`) — vernachlässigbar gegenüber den ohnehin schon zahlreichen View-Instanzen
  in `NfcApp` (Submenu, DialogEx, Popup, TextInput, ByteInput, TextBox, Widget,
  DetectReader, DictAttack), aber nicht Null; bei sehr knappem RAM-Budget gegenprüfen.
- `nfc_show_loading_dict_popup(..., false)` ruft `loading_reset_progress()` auch dann auf,
  wenn nie eine Progress-Bar gezeigt wurde (z. B. wenn `dict_size == 0` war) — laut dem
  bereits verifizierten `loading_reset_progress()`-Verhalten (Redraw-Unterdrückung, wenn
  bereits verborgen) unschädlich, aber der Vollständigkeit halber genannt.
- Reiner UI-/Verdrahtungs-Fund: ändert nichts an der eigentlichen Bitmap-/Schlüsselindex-Logik
  oder am Protokollverhalten des Wörterbuchangriffs selbst.
- Setzt zwingend den `loading-widget-label-and-progress-bar`-Patch voraus — ohne ihn fehlen
  `loading_set_text()`/`loading_set_progress()`/`loading_reset_progress()`, dieser Patch
  kompiliert dann nicht.

## Host-Repro
`firmware-mining/repro-nfc-mfc-cuid-dict-scan-progress/repro.c` — zwei unabhängige,
wörtlich aus dem echten, gepatchten Code transkribierte Teile:

1. **Fortschritts-Formel** (`percent_for()`, exakter Ausdruck aus der Schleife): über
   5000 simulierte wohlgeformte Dictionary-Größen (je 1–5000 Schlüssel) geprüft, dass der
   Wert nie `[0,1]` verlässt, mit fortschreitendem Scan monoton nicht fällt und beim
   vorletzten Schlüssel bereits nahe 100 % liegt (Toleranz: der eine Prozentpunkt, den
   Ganzzahl-Division verlieren kann); zusätzlich über 200×15 Kombinationen mit einer Datei,
   die kein exaktes Vielfaches der Zeilenlänge ist (fehlende letzte Bytes/Newline), geprüft,
   dass der rohe, über 100 hinausschießende Wert korrekt auf 1,0 geklemmt wird; Divide-by-
   Zero-Schutz separat geprüft. 12.505.501 Einzelprüfungen.
2. **Kontrollfluss** (`run_prepare_view_cuid_branch()`, Struktur 1:1 aus dem gepatchten
   `do { ... } while(false)`-Block übernommen, `break` durch `return` ersetzt, da der
   umgebende, hier nicht nachgebaute Nicht-CUID-Fallback-Codepfad unverändert bleibt):
   geprüft, dass der Popup bei fehlender Datei und bei fehlgeschlagenem Öffnen **nie**
   aufgerufen wird (beide `break`-Punkte liegen vor dem neuen Anzeigeaufruf), bei leerem
   Dictionary genau einmal gezeigt und einmal wieder ausgeblendet wird (ohne
   Fortschritts-Updates, da die Schleife nie läuft), bei einem normalen Dictionary genau ein
   Fortschritts-Update pro Schlüssel erfolgt, und bei `dict_size == 0` Anzeigen/Ausblenden
   weiterhin passiert, aber keine Fortschritts-Updates. 59 Einzelprüfungen.

Unter ASan/UBSan bei `-O0` und `-O1` ausgeführt: **ALLE CHECKS BESTANDEN**
(12.505.560 Einzelprüfungen insgesamt), keine Compiler-Warnungen (`-Wall -Wextra`).

Patch-Verifikation: `git apply --check` vorab gegen eine frische Kopie sowohl von
`Next-Flip/Momentum-Firmware` (Tag `mntm-012`) als auch meiner eigenen
`G4MEOVER18/G4MEOVER-FW`-Basis bestanden (beide byte-identisch zueinander an allen drei
betroffenen Dateien), alle drei Patches real angewendet und das Ergebnis byte-für-byte
(`diff`) gegen die vorbereitete Zielfassung abgeglichen — identisch.

## Testhinweis (lokal)
1. Beide Patch-Sätze in dieser Reihenfolge anwenden: zuerst
   `firmware-mining/loading-widget-label-and-progress-bar/{loading_c,loading_h}.patch`, dann
   die drei Patches aus diesem Ordner.
2. Bauen (`fbt`/`ufbt`), flashen.
3. Ein UID-spezifisches Wörterbuch mit möglichst vielen Einträgen auf die SD-Karte legen
   (`nfc/assets/mf_classic_dict_<UID>.nfc`, `<UID>` = eigene, groß geschriebene Hex-UID der
   Testkarte) und einen MIFARE-Classic-Wörterbuchangriff auf genau diese Karte starten.
   Erwartung: statt des bisher eingefrorenen Vorschauschirms erscheint kurz "Loading CUID
   index..." mit Spinner und einer sich füllenden Fortschrittsleiste, bevor die eigentliche
   Angriffsansicht erscheint.
4. Gegenprobe mit einer Karte ohne vorhandenes UID-Dictionary (Popup darf gar nicht
   erscheinen) sowie mit einem absichtlich leeren `mf_classic_dict_<UID>.nfc`
   (Popup erscheint kurz und verschwindet wieder, kein sichtbarer Balken-Sprung).

## Quelle
- Eigener Fund (Vergleich meines `nfc_scene_mf_classic_dict_attack.c` gegen Unleasheds
  `mf_classic_extra_scenes.c`), Vorlage/Formel aus `DarkFlippers/unleashed-firmware`,
  Branch `dev`, Commit `418d802b413305c05252cbadabc71550fdc63b1c` (PR #1167, "Show progress
  while a large CUID dictionary loads on Read", Closes #1166) — per authentifizierungsfreiem
  Klon bezogen, nur als Referenz für die Formel genutzt, da Unleasheds eigentliche
  Aufrufstelle (`mf_classic_extra_scenes.c`, `views/loading_label.c`) strukturell nicht bei
  mir existiert.
- Eigener vorheriger Fund `loading-widget-label-and-progress-bar.md` (liefert die
  vorausgesetzte `Loading`-Widget-Infrastruktur und benennt diese Verdrahtungsstelle
  namentlich als Rückstellpunkt).
- Frischer, unauthentifizierter `git clone --depth 1 --branch mntm-012
  Next-Flip/Momentum-Firmware` sowie meine eigene `G4MEOVER18/G4MEOVER-FW`-Basis zum Abgleich
  des Ausgangsstands aller drei betroffenen Dateien (beide byte-identisch zueinander).
