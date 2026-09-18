# NFC: gefundene MIFARE-Classic-Schlüssel in das Nutzer-Dictionary speichern

Quelle: Unleashed-Firmware, `unlshd-093`-Restliste, Commit `95c35fb82de2390531a1788b28612a793cd987ae`
(PR #1118, "NFC: Save recovered MIFARE Classic keys to the user dictionary"). Bei mir (mntm-012,
API 87.1) bislang unbearbeitet aus der seit der Expansion-Callback-Sentinel-Sitzung geführten
`unlshd-093`-Restliste (zusammen mit Superrollo/GW60-SubGHz, #1068, weiterhin offen).

Das Original ist im Unleashed-Baum auf die eigene, generische `nfc_key_dict.c/.h`-Infrastruktur
aus PR #1073 aufgesetzt (vier Dictionary-Typen: Classic, Plus, Ultralight C, Ultralight AES,
gemeinsame Szenen `nfc_scene_key_dict*`). Diese Infrastruktur habe ich bei mir nicht — der
zugehörige eigene Port-Vorschlag (`firmware-mining/nfc-unified-key-dictionary.md`) ist bislang nur
dokumentiert, nicht angewendet, und selbst dort bereits auf die Classic-/Ultralight-C-Hälfte
beschränkt, weil mir `MfPlusKey`/`MfUltralightAesKey` und die zugehörige schlüsselbasierte
Authentifizierung komplett fehlen (siehe dortiger Befund). Diesen Port setze ich deshalb bewusst
**nicht** auf jene noch unangewendete Abstraktion auf, sondern verdrahte das Feature direkt gegen
meine bestehende, unveränderte MIFARE-Classic-Szenenstruktur (`nfc_scene_mf_classic_show_keys.c`,
`nfc_scene_read_menu`/`saved_menu`-Einträge in `mf_classic.c`) — gleicher Nutzen für den einzigen
Kartentyp, den ich schlüsselbasiert unterstütze, ohne die größere, eigenständige
Plus-/Ultralight-AES-Baustelle vorzutäuschen.

## Was

Schlüssel, die eine Attacke (CUID-Dictionary, Nested, MFKey32, Handeingabe oder ein importierter
Dump) auf einer Karte findet, laufen bei mir aktuell in eine Sackgasse: Sie landen in
`MfClassicData` (Anzeige über "Show Keys"), im Dump und — nur bei einer per Dict-Attack
entsperrten Karte — im UID-gebundenen Schlüssel-Cache (`mf_classic_key_cache.c`, für den
schnellen Re-Read derselben physischen Karte). Ins gemeinsame Nutzer-Dictionary
(`assets/mf_classic_dict_user.nfc`), das jede künftige Dict-Attack auf jeder anderen Karte
durchsucht, kommen sie nirgends automatisch hinein — ich muss sie manuell aus dem "Show
Keys"-Bildschirm abtippen. Am schlechtesten betroffen ist die CUID-Dictionary-Variante (für
Karten mit statisch verschlüsselten Nonces): deren Schlüssel sind ausschließlich unter der UID
der Ursprungskarte gespeichert und für nichts anderes sichtbar — klone ich eine solche Karte auf
einen Magic-Tag, kann ich sie mit meinem eigenen Werkzeugbestand hinterher nicht mehr eigenständig
entsperren, weil der Magic-Tag eine andere UID trägt.

Der Port fügt einen neuen Menüpunkt "Save Keys to Dictionary" neben "Show Keys" in beide
MIFARE-Classic-Menüs ein (`Read`-Menü direkt nach dem Lesevorgang, `Saved`-Menü für eine bereits
gespeicherte Datei), sichtbar nur, wenn die Karte mindestens einen Schlüssel trägt
(`key_a_mask | key_b_mask != 0`). Die neue Szene liest die gefundenen Schlüssel direkt aus den
Masken (nicht aus dem Weg, über den sie gefunden wurden) — ein Codepfad deckt damit CUID, Nested,
MFKey32, Handeingabe und Dump-Import gleichermaßen ab. Vor dem Speichern wird gegen **beide**
Dictionaries gefiltert (System- und Nutzer-Dictionary), damit eine Karte mit werkseitigen
Standard-Schlüsseln das Nutzer-Dictionary nicht unnötig aufbläht, und das Ergebnis ("Neue
Schlüssel: N / Bereits bekannt: M") angezeigt.

## Nutzen

- **Schließt die CUID-Sackgasse**: gefundene Schlüssel einer statisch-verschlüsselten Karte
  überleben jetzt einen Klon-auf-Magic-Tag-Zyklus, weil sie zusätzlich im UID-unabhängigen
  Nutzer-Dictionary landen.
- **Ein Klick statt Abtippen**: bisher musste ich jeden auf "Show Keys" angezeigten Schlüssel von
  Hand über "Add" in `nfc_scene_mf_classic_keys_add.c` eintragen — fehleranfällig bei mehreren
  Sektoren mit unterschiedlichen Schlüsseln.
- **Effizienter Dictionary-Abgleich**: ein Durchlauf pro Dictionary-Datei statt eines
  `keys_dict_is_key_present()`-Aufrufs (voller Dateiscan) pro gefundenem Schlüssel — bei einer
  4K-Karte (bis zu 80 Schlüssel) gegen ein mehrere tausend Zeilen großes System-Dictionary sonst
  bis zu 80 volle Scans.
- **Bonus-Performancefix in `keys_dict.c`**: beim Nachvollziehen des Originals fiel auf, dass
  `keys_dict_add_key_str()` bei mir (byteidentisch zum Unleashed-Vorher-Stand) jeden neuen
  Schlüssel per `stream_insert_string()` einfügt statt anzuhängen — das kopiert laut
  `file_stream_delete_and_insert()`-Implementierung die *gesamte* Datei in eine Scratch-Datei und
  zurück, obwohl zuvor exakt ans Dateiende gesprungen wird (nichts zu verschieben). Bei einem
  einzelnen `mf_classic_keys_add`-Aufruf harmlos, aber quadratisch für jeden Aufrufer, der mehrere
  Schlüssel in einer Schleife schreibt — bislang nur MFKey32 (`mfkey32_logger.c`), jetzt zusätzlich
  dieser Import mit bis zu 80 Schlüsseln in einem Rutsch. Unleashed hat genau diesen Fix im selben
  PR mitgeliefert (`stream_write_string()` statt `stream_insert_string()`); ich übernehme ihn 1:1,
  da meine Datei an der betroffenen Stelle identisch ist.

## Host-Repro

`firmware-mining/repro-mf-classic-save-keys-to-dict/repro.c` extrahiert die drei
Kernalgorithmen der neuen Szene (`nfc_mf_classic_save_keys_push_unique`,
`_collect`, `_mark_known`) nahezu wörtlich (reine `memcmp`-Logik ohne Storage-/Stream-Abhängigkeit)
und bindet sie an einen minimalen Array-Nachbau der `KeysDict`-Iterationsschnittstelle
(`get_total_keys`/`get_next_key`: einmal von vorne durchlaufen, `false` bei Erschöpfung) statt an
echte Dateien. Sieben Testgruppen:

1. Sammeln nur über Masken, nicht über Byte-Inhalt: Duplikat über zwei Sektoren wird auf einen
   Eintrag dedupliziert, ein nicht gefundener Sektor mit zufälligem Byte-Inhalt wird ignoriert.
2. Ein echter Schlüssel aus lauter Nullen (Maskenbit gesetzt) wird genauso gesammelt wie ein
   nicht gefundener Slot mit ebenfalls lauter Nullen (Maskenbit **nicht** gesetzt) — Regressionsfall
   für "Maske entscheidet, nicht der Byte-Wert".
3. Voller 4K-Sweep (40 Sektoren, beide Schlüssel je Sektor, alle unterschiedlich): exakt 80
   gesammelte Schlüssel, passend zur dokumentierten Obergrenze `NFC_MF_CLASSIC_SAVE_KEYS_MAX`.
4. Der Überlauf-Schutz in `push_unique` selbst (in der Praxis unerreichbar, da 40 Sektoren × 2
   Schlüssel exakt 80 ergibt) — bewusst trotzdem geprüft, wie in früheren Sitzungen bei
   Verteidigungscode ohne echten Erreichbarkeitspfad üblich.
5. `mark_known` gegen System- und Nutzer-Dictionary nacheinander: ein im System-Dictionary
   bekannter, ein im Nutzer-Dictionary bekannter und ein neuer Schlüssel werden korrekt getrennt.
6. Leeres/fehlendes Dictionary (`total == 0`) markiert nichts als bekannt und stürzt nicht ab.
7. Vollständiger Durchlauf mit vier Kandidaten (je einer bekannt pro Dictionary, zwei neu):
   `bekannt + neu == Kandidaten` bestätigt.

Unter `gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined`: **14 von 14 Checks bestanden**,
keine ASan-/UBSan-Meldungen.

## Betroffene Dateien

- `applications/main/nfc/scenes/nfc_scene_mf_classic_save_keys.c` — **neu**. Sammelt die
  gefundenen Schlüssel aus `MfClassicData` (Masken-basiert, dedupliziert), filtert gegen
  System- und Nutzer-Dictionary, hängt die neuen Schlüssel an, zeigt das Ergebnis über
  `Widget`+"Back"-Button (gleiches Muster wie das bestehende `nfc_scene_mf_classic_show_keys.c`,
  bewusst kein `Popup` mit Auto-Timeout gewählt, da das Ergebnis in Ruhe gelesen werden soll).
- `applications/main/nfc/scenes/nfc_scene_config.h` — ein `ADD_SCENE`-Eintrag für die neue Szene,
  direkt hinter `mf_classic_show_keys`.
- `applications/main/nfc/helpers/protocol_support/mf_classic/mf_classic.c` — neuer
  `SubmenuIndexSaveKeys`, ein neuer Menüpunkt "Save Keys to Dictionary" (nur bei
  `key_a_mask | key_b_mask != 0`) in `nfc_scene_read_menu_on_enter_mf_classic()` und
  `nfc_scene_saved_menu_on_enter_mf_classic()`, plus Event-Weiterleitung in den beiden
  zugehörigen `_on_event_`-Funktionen. 1:1 dieselbe Verdrahtungsstruktur wie im Original
  (`nfc_scene_menu_add_save_keys_mf_classic()`/`nfc_scene_menu_on_event_save_keys_mf_classic()`),
  nur ohne den bei mir nicht vorhandenen `instance->key_dict_type`-Selektor — die neue Szene weiß
  fest, dass sie für MIFARE Classic arbeitet, da es bei mir keinen zweiten Dictionary-Typ mit
  eigenem Auth-Schlüssel gibt.
- `lib/toolbox/keys_dict.c` — `keys_dict_add_key_str()`: `stream_insert_string()` durch
  `stream_write_string()` ersetzt (Bonus-Performancefix, siehe Nutzen).

Nicht angefasst: `applications/main/nfc/helpers/mf_user_dict.c/.h` (Read/List/Delete-Pfad für das
Nutzer-Dictionary, von "Show Keys"/"Add"/"Delete" verwendet) — der neue Import nutzt `keys_dict.c`
direkt, exakt wie `nfc_scene_mf_classic_keys_add.c` das für einen einzelnen Schlüssel bereits tut.

## Risiko

- **Kein Speicherzugriffsfehler zu erwarten**: `NFC_MF_CLASSIC_SAVE_KEYS_MAX` (80) ist exakt die
  reale Obergrenze eines 4K-Cards (`mf_classic_get_total_sectors_num()` liefert für 4K 40, siehe
  `mf_classic.c` bei mir), beide Puffer (`keys[]`, `known[]`) sind fest auf diese Größe allokiert
  und `push_unique()` verweigert das Schreiben, sobald sie voll sind (Test 4 oben).
- **Kein neuer Schreibpfad ohne bestehende Absicherung**: `keys_dict_add_key()` ist dieselbe,
  bereits produktiv genutzte Funktion wie in `nfc_scene_mf_classic_keys_add.c` — kein neues
  Dateiformat, keine neue Fehlerbehandlung nötig.
- **Verhalten bei fehlendem System-Dictionary**: wird geloggt (`FURI_LOG_W`), aber bewusst nicht
  blockiert — ein fehlendes System-Dictionary ist bei mir kein Normalfall (es liegt im
  Firmware-Image), würde aber im Extremfall dazu führen, dass Standard-Schlüssel unnötig ins
  Nutzer-Dictionary übernommen werden. Kein Datenverlust, nur ein größeres Dictionary als nötig.
- **`stream_write_string()`-Änderung in `keys_dict.c`** ändert das Verhalten für **jeden**
  Aufrufer von `keys_dict_add_key()`, nicht nur den neuen — betrifft also auch den bestehenden
  Handeingabe-Pfad (`mf_classic_keys_add`, `mf_ultralight_c_keys_add`) und MFKey32. Da jeder
  Aufrufer bereits vorher ans Dateiende springt (`stream_seek(..., StreamOffsetFromEnd)`), ist das
  beobachtbare Ergebnis laut Unleashed-Commit-Beschreibung identisch — kein abweichendes Verhalten,
  nur weniger Kopieraufwand. Lokal beim Testen (siehe unten) zusätzlich gegenprüfen, dass eine
  bereits vorhandene, nicht-leere Dictionary-Datei nach mehrfachem Hinzufügen weiterhin gültig
  bleibt (korrekte Zeilenenden, keine abgeschnittene letzte Zeile).

## Port-Skizze / Fix

Als drei Unified-Diffs gegen meine unveränderte `mntm-012`-Basis (`patch --dry-run -p1` vorab
verifiziert, real gegen eine frische Kopie der drei Originaldateien angewendet und Byte-für-Byte
gegen die separat editierte Fassung abgeglichen — identisch):

- `firmware-mining/mf-classic-save-keys-to-dict-port/nfc_scene_config_h.patch`
- `firmware-mining/mf-classic-save-keys-to-dict-port/mf_classic_c.patch`
- `firmware-mining/mf-classic-save-keys-to-dict-port/keys_dict_c.patch`

Die neue Szenendatei selbst liegt als vollständige Quelle (kein Diff, da neu) in
`firmware-mining/mf-classic-save-keys-to-dict-port/nfc_scene_mf_classic_save_keys.c` — lokal nach
`applications/main/nfc/scenes/nfc_scene_mf_classic_save_keys.c` kopieren, bevor die drei Patches
angewendet werden.

## Testhinweis (lokal nachzuvollziehen)

1. Die neue Datei kopieren, die drei Patches anwenden, `fbt` neu bauen und flashen.
2. Eine MIFARE-Classic-Karte lesen (Dict Attack oder Nested reicht) und danach im Read- oder
   Saved-Menü prüfen, dass "Save Keys to Dictionary" nur erscheint, wenn mindestens ein Schlüssel
   gefunden wurde — bei einer komplett ungelesenen Karte (nur UID) darf der Menüpunkt fehlen.
3. "Save Keys to Dictionary" auf einer frisch gelesenen Karte mit noch unbekannten Schlüsseln
   auslösen: Ergebnis muss "New keys: N / Already known: 0" zeigen, N = Anzahl distinkter
   gefundener Schlüssel; anschließend `assets/mf_classic_dict_user.nfc` auf dem PC öffnen und die
   neuen Zeilen am Dateiende bestätigen (keine beschädigte vorherige Zeile durch den
   `stream_write_string()`-Wechsel).
4. Denselben Vorgang ein zweites Mal auf derselben Karte wiederholen: jetzt muss "Nothing to Add"
   mit "Already known: N" erscheinen (Dedup gegen das eben erweiterte Nutzer-Dictionary greift).
5. Regressionstest: einen einzelnen Schlüssel weiterhin über "Show Keys" → zurück →
   `nfc_scene_mf_classic_keys_add` (bestehender Handeingabe-Pfad) hinzufügen — muss weiterhin wie
   vor dem `keys_dict.c`-Fix funktionieren (Dictionary bleibt lesbar, Duplikat-Warnung greift bei
   erneuter Eingabe desselben Schlüssels).
6. Falls vorhanden: eine Karte mit werkseitigen Standard-Schlüsseln lesen und "Save Keys to
   Dictionary" auslösen — Ergebnis sollte "Already known" für die Standard-Schlüssel zeigen
   (System-Dictionary-Filter greift), nicht "New keys".

## Quelle

`DarkFlippers/unleashed-firmware`, `dev`-Branch, Commit `95c35fb82` (PR #1118, @myte). Gegen die
eigene `Next-Flip/Momentum-Firmware`-Basis am Tag `mntm-012` per Sparse-`git fetch` geprüft
(Dateien existieren bei mir unter identischem Pfad wie im Original, da meine `mf_classic.c` an
dieser Stelle noch vor der `unlshd-092`/PR-#1073-Restrukturierung steht).
