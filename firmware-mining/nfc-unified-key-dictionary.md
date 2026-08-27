# NFC: ein gemeinsamer Schlüssel-Dictionary-Manager statt Duplikat je Kartentyp — Unleashed vs. mntm-012

Quelle: Unleashed-Firmware, `unlshd-092` (gegen zuvor geprüften Stand `unlshd-091`), Commit
`2b4411f234ac9e798d941410e45ded1121fede40` (PR #1073, @myte, gemerged 20.08.2026 — großer,
gesquashter RAM-Reduktions-Umbau des NFC-Apps). Der überwiegende Teil dieses Commits verlagert
Protokoll-Szenen aus dem residenten App-Image in dynamisch geladene Plugins (`.fal`); das ist eine
architektonische Änderung, die ich hier bewusst **nicht** aufgreife. Ich habe daraus gezielt nur
den in sich abgeschlossenen Teilschritt "one user key dictionary manager instead of four copies"
(plus die zugehörige Nachbesserung "clear the key input between dictionaries") herausgelöst — er
berührt ausschließlich Dateien unter `applications/main/nfc/{helpers,scenes}/`, unabhängig vom
Plugin-Mechanismus.

## Was

Bei mir (mntm-012, API 87.1) hat jeder schlüsselbasierte NFC-Kartentyp eine eigene, fast
identische Fünfer-Gruppe von Szenen für Anlegen/Auflisten/Löschen eigener Schlüssel:
`nfc_scene_mf_classic_keys{,_list,_delete,_add,_warn_duplicate}.c` und
`nfc_scene_mf_ultralight_c_keys{,_list,_delete,_add,_warn_duplicate}.c` — zehn Dateien, die sich
nur in Titel, den beiden Dictionary-Pfaden und der Schlüssellänge unterscheiden. Die
Ultralight-C-Variante liest ihren Schlüsseltyp direkt über die konkrete
`helpers/mf_user_dict.c`, die fest auf MIFARE-Classic-Pfad und -Schlüssellänge verdrahtet ist
(nutzt für Ultralight-C also eine eigene Parallelimplementierung, keine Wiederverwendung).

Unleashed ersetzt das durch eine generische Beschreibungstabelle
(`applications/main/nfc/helpers/nfc_key_dict.c/.h`) plus fünf generische Szenen
(`nfc_scene_key_dict{,_list,_delete,_add,_warn_duplicate}.c`), die über einen neuen Selektor
`NfcApp::key_dict_type` (`NfcKeyDictType`) wissen, mit welchem Dictionary (Titel, System-/
User-Pfad, Schlüssellänge) sie gerade arbeiten. `helpers/mf_user_dict.c/.h` entfällt komplett.

**Wichtiger Befund beim Abgleich gegen meine Basis:** Im Unleashed-Original deckt die Tabelle vier
Typen ab — zusätzlich zu MIFARE Classic und Ultralight C auch **MIFARE Plus** und **Ultralight
AES**. Bei mir existieren die dafür nötigen Schlüsseltypen aber gar nicht:
`lib/nfc/protocols/mf_plus/mf_plus.h` hat bei mir kein `MfPlusKey`, kein `key_a[]`/`key_b[]`-Array,
keine Auth-Poller-Logik — meine `MfPlusData` beschränkt sich auf UID/Version/Security-Level
(reine Erkennung, keine schlüsselbasierte SL3-Authentifizierung). Ebenso fehlt
`MfUltralightAesKey` komplett (grep über den gesamten `mntm-012`-Baum: keine Treffer für beide
Typnamen). Das bedeutet: die Plus-/Ultralight-AES-Hälfte des Unleashed-Features hat bei mir keine
Grundlage — es fehlt nicht nur die UI, sondern das komplette schlüsselbasierte Authentifizierungs-
Feature für diese beiden Protokolle. Das ist ein eigenständiges, deutlich größeres Portierungsvorhaben
(neue Poller-/Auth-Logik, neue Datenfelder in `MfPlusData`/`MfUltralightData`, vermutlich über
mehrere Unleashed-Versionen hinweg gewachsen) und **kein** Teil dieses Vorschlags — siehe Risiko
und Offene Punkte.

Dieser Vorschlag portiert deshalb bewusst nur die **Classic-/Ultralight-C-Hälfte** des Features:
zwei Dictionary-Typen statt vier, aber mit identischem Nutzen (Code-Duplikat weg, ein
Korrektheits- und ein Datenleck-Fix, siehe unten) und ohne die fehlende Plus-/AES-Grundlage
vorzutäuschen.

## Nutzen

- **Weniger Duplikat-Code**: zehn fast identische Dateien werden zu fünf generischen plus einer
  kleinen Tabelle. Künftige Änderungen an der Add/List/Delete-Logik (z. B. ein UI-Fix) müssen nur
  noch an einer Stelle gepflegt werden, statt in zwei Kopien synchron gehalten zu werden.
- **Korrektheits-Fix beim Löschen**: meine aktuelle `nfc_scene_mf_classic_keys_delete.c` löscht
  anhand eines beim Öffnen der Liste angelegten Snapshots (Index in einem beim Scene-Enter
  einmalig geladenen Array) — ändert sich die Dictionary-Datei zwischen Listenaufruf und
  Löschbestätigung (z. B. weil ich dieselbe Datei parallel per PC/SD-Karten-Tool bearbeite), löscht
  der Bestätigungs-Dialog denselben *Index*, nicht zwingend denselben *Schlüssel*. Die
  Ultralight-C-Variante macht das bei mir bereits richtig (liest den Schlüssel am Index frisch
  aus der Datei). Die generische `nfc_scene_key_dict_delete.c` übernimmt das robustere
  Ultralight-C-Verhalten für beide Typen: sie liest den Schlüssel am gewählten Index unmittelbar
  vor der Lösch-Bestätigung neu aus der Datei und bietet "Delete" nur an, wenn der Index noch
  existiert (sonst "Key Not Found" statt eines Lösch-Dialogs auf einer bereits verschwundenen
  Zeile).
- **Schließt ein Datenleck zwischen den beiden Dictionaries**: `byte_input_store` ist ein
  einzelner, App-weiter 16-Byte-Puffer, den beim Wechsel zwischen Dictionaries bei mir aktuell
  niemand zurücksetzt. Trage ich einen 6-Byte-MIFARE-Classic-Schlüssel ein, breche ab und öffne
  danach "Add" bei Ultralight C (ebenfalls über denselben Puffer, nur mit anderer Feldlänge über
  `byte_input_set_result_callback`), zeigt das Eingabefeld die ersten Bytes des zuvor getippten
  Classic-Schlüssels als Vorbelegung. Kein Speicherfehler, aber ein für einen autorisierten
  Pentest-Kontext ärgerliches Verhalten (verwechselbar mit "das ist noch der alte Schlüssel").
  Der generische `nfc_scene_key_dict.c` löscht `byte_input_store` gezielt beim Drücken von "Add"
  auf dem Dictionary-Übersichtsbildschirm (nicht im `Add`-Scene-Enter selbst, weil der
  Duplikat-Warnhinweis dorthin zurückspringt und ein Reset an der falschen Stelle den gerade
  korrigierten Schlüssel wieder löschen würde).
- Kein Funktionsverlust: Add/List/Delete-Dateilogik selbst (`toolbox/keys_dict.c`) ändert sich
  nicht — nur die Verdrahtung außenherum.

## Betroffene Dateien (Portumfang, auf Classic + Ultralight C beschränkt)

**Neu:**
- `applications/main/nfc/helpers/nfc_key_dict.h` — `NfcKeyDictType`-Enum (nur
  `NfcKeyDictTypeNone`, `NfcKeyDictTypeMfClassic`, `NfcKeyDictTypeMfUltralightC`,
  `NfcKeyDictTypeNum`; `NfcKeyDictTypeNone` bleibt Reserve-Wert für den nullinitialisierten Fall,
  siehe Risiko) und `NfcKeyDict`-Struct (`title`, `system_path`, `user_path`, `key_size`).
- `applications/main/nfc/helpers/nfc_key_dict.c` — Tabelle mit genau zwei Zeilen
  (`NfcKeyDictTypeMfClassic`, `NfcKeyDictTypeMfUltralightC`), Pfade/Größen aus meiner
  bestehenden `nfc_app_i.h` (`NFC_APP_MF_CLASSIC_DICT_*_PATH`,
  `NFC_APP_MF_ULTRALIGHT_C_DICT_*_PATH`, bereits vorhanden) bzw. `sizeof(MfClassicKey)`/
  `sizeof(MfUltralightC3DesAuthKey)`. Kein Plus-/AES-Eintrag.
- `applications/main/nfc/scenes/nfc_scene_key_dict.c`
- `applications/main/nfc/scenes/nfc_scene_key_dict_add.c`
- `applications/main/nfc/scenes/nfc_scene_key_dict_delete.c`
- `applications/main/nfc/scenes/nfc_scene_key_dict_list.c`
- `applications/main/nfc/scenes/nfc_scene_key_dict_warn_duplicate.c`

  Alle fünf Szenen greifen ausschließlich generisch über `nfc_key_dict(instance->key_dict_type)`
  auf Titel/Pfade/Größe zu — kein Bezug auf konkrete Enum-Werte im Szenencode selbst. Damit
  können sie **unverändert** (byte-identisch) aus `unlshd-092` übernommen werden; die Beschränkung
  auf zwei Typen passiert allein in `nfc_key_dict.c`.

**Geändert:**
- `applications/main/nfc/nfc_app_i.h` — `#include "helpers/mf_user_dict.h"` durch
  `#include "helpers/nfc_key_dict.h"` ersetzen; Feld `MfUserDict* mf_user_dict;` durch
  `NfcKeyDictType key_dict_type;` ersetzen.
- `applications/main/nfc/scenes/nfc_scene_config.h` — die zehn `ADD_SCENE(nfc, mf_classic_keys*, …)`
  / `ADD_SCENE(nfc, mf_ultralight_c_keys*, …)`-Zeilen durch die fünf generischen
  `ADD_SCENE(nfc, key_dict*, …)`-Zeilen ersetzen (Reihenfolge/Namen wie in `unlshd-092`, ohne die
  dortige `mf_ultralight_aes_dict_attack_warn`-Zeile — die gehört zu einem anderen, hier nicht
  portierten Feature).
- `applications/main/nfc/scenes/nfc_scene_extra_actions.c` — die beiden Einträge "MIFARE Classic
  Keys" / "MIFARE Ultralight C Keys" bleiben im Menü, springen aber jetzt beide auf
  `NfcSceneKeyDict`, nachdem sie vorher `instance->key_dict_type` gesetzt haben
  (`NfcKeyDictTypeMfClassic` bzw. `NfcKeyDictTypeMfUltralightC`), statt direkt auf
  `NfcSceneMfClassicKeys`/`NfcSceneMfUltralightCKeys` zu springen. Kein neuer Menüpunkt für
  Plus/Ultralight AES (siehe oben).
- `applications/main/nfc/scenes/nfc_scene_delete_success.c` und `.../nfc_scene_save_success.c` —
  die jeweils zwei `if(scene_manager_has_previous_scene(nfc->scene_manager,
  NfcSceneMfClassicKeys)) … else if(…, NfcSceneMfUltralightCKeys)) …`-Zweige durch einen
  einzigen `if(…, NfcSceneKeyDict))`-Zweig ersetzen. (Im Unleashed-Original ist das ein
  Vierer-auf-Eins-Zusammenzug, bei mir bleibt es bei Zwei-auf-Eins — gleiches Prinzip.)

**Entfällt:**
- `applications/main/nfc/helpers/mf_user_dict.c` / `.h`
- `applications/main/nfc/scenes/nfc_scene_mf_classic_keys{,_list,_delete,_add,_warn_duplicate}.c`
  (5 Dateien)
- `applications/main/nfc/scenes/nfc_scene_mf_ultralight_c_keys{,_list,_delete,_add,_warn_duplicate}.c`
  (5 Dateien)

## Risiko

- **Struct-/Header-Änderung mit mehreren Aufrufstellen**: `NfcApp::mf_user_dict` entfällt,
  `NfcApp::key_dict_type` kommt neu hinzu; die Scene-IDs `NfcSceneMfClassicKeys*`/
  `NfcSceneMfUltralightCKeys*` verschwinden aus `nfc_scene_config.h`. Ich habe in meiner Basis
  gezielt nach jeder Referenz auf diese Bezeichner gesucht (`nfc_scene_extra_actions.c`,
  `nfc_scene_delete_success.c`, `nfc_scene_save_success.c` — siehe oben) und keine weiteren
  Fundstellen außerhalb der zu löschenden zehn Dateien selbst gefunden. Vor dem tatsächlichen
  Einbau empfiehlt sich trotzdem ein `grep -rn "NfcSceneMfClassicKeys\|NfcSceneMfUltralightCKeys\|mf_user_dict\|MfUserDict"`
  über den vollen `applications/main/nfc`-Baum, da mein Abgleich nur auf Basis der bei mir
  vorhandenen (mit `unlshd-091` byteidentischen) Dateien erfolgte, nicht auf Basis eines
  tatsächlichen lokalen Kompilierlaufs.
- **Verhaltensänderung beim Löschen (Classic)**: wie unter Nutzen beschrieben, liest die neue
  Delete-Szene den Schlüssel je Aufruf frisch aus der Datei statt aus einem Snapshot. Für den
  Normalfall (ich lösche über das Gerätemenü, ohne die Datei parallel von außen zu verändern)
  identisches Verhalten; nur relevant, falls ich dieselbe `.nfc`-Dictionary-Datei gleichzeitig auf
  der SD-Karte extern bearbeite, während ich am Gerät im Löschmenü stehe.
- **Bewusst unvollständiges Enum**: `NfcKeyDictTypeNone` bleibt als Reserve-Wert erhalten (schützt
  über den bestehenden `furi_check()` in `nfc_key_dict()` davor, dass eine Szene ohne vorheriges
  Setzen von `key_dict_type` — `NfcApp` wird nullinitialisiert — unbemerkt auf Dictionary-Index 0
  liefe). Ich lasse aber bewusst **keine** `NfcKeyDictTypeMfPlus`/`NfcKeyDictTypeMfUltralightAes`-
  Einträge im Enum stehen, auch nicht als Platzhalter: ein Enum-Wert ohne passende Tabellenzeile
  und ohne `MfPlusKey`/`MfUltralightAesKey`-Typen wäre entweder ein toter Zweig oder — schlimmer —
  ein `furi_check()`-Absturz, falls ihn künftig doch einmal jemand aus Versehen setzt. Die
  Erweiterung auf vier Typen ist erst sinnvoll, nachdem die zugrundeliegende Plus-/Ultralight-AES-
  Schlüssellogik selbst portiert ist (eigener, separater Backlog-Punkt, siehe unten) — dann ist
  auch nur eine Tabellenzeile pro neuem Typ nachzutragen, keine weitere Szenenänderung.
- Reiner Verdrahtungs-Umbau ohne neue Dateilese-/Schreiblogik: `toolbox/keys_dict.c` (das
  eigentliche Add/List/Delete-Dateiformat) bleibt unangetastet. Der Build (`fbt`/`ufbt`) deckt jede
  vergessene Umbenennung als Compile-Fehler auf.

## Port-Skizze

1. `applications/main/nfc/helpers/nfc_key_dict.h` und `.c` neu anlegen (Tabelle mit den zwei
   Classic-/Ultralight-C-Zeilen, siehe "Betroffene Dateien").
2. Die fünf `nfc_scene_key_dict*.c`-Dateien unverändert aus `unlshd-092` übernehmen (kein
   protokollspezifischer Code darin, siehe oben).
3. `nfc_app_i.h`: Include und Struct-Feld tauschen (`mf_user_dict.h`/`MfUserDict*` →
   `nfc_key_dict.h`/`NfcKeyDictType`).
4. `nfc_scene_config.h`: die zehn alten `ADD_SCENE`-Zeilen durch die fünf neuen `key_dict*`-Zeilen
   ersetzen.
5. `nfc_scene_extra_actions.c`: die beiden bestehenden Menüeinträge auf `instance->key_dict_type =
   …; scene_manager_next_scene(…, NfcSceneKeyDict);` umstellen (siehe Vorher/Nachher oben).
6. `nfc_scene_delete_success.c` / `nfc_scene_save_success.c`: die Zwei-Zweige auf den
   `NfcSceneKeyDict`-Einzelzweig zusammenziehen.
7. `helpers/mf_user_dict.c`/`.h` sowie die zehn alten `nfc_scene_mf_classic_keys*`/
   `nfc_scene_mf_ultralight_c_keys*.c`-Dateien löschen.
8. Vor dem Löschen Schritt 7 den empfohlenen Grep aus dem Risiko-Abschnitt laufen lassen, um
   weitere Fundstellen in meinem konkreten Baum auszuschließen.
9. Build: `ufbt` (bzw. `fbt firmware_all`) — deckt jede vergessene Umbenennung/Signatur-Abweichung
   als Compile-Fehler auf.
10. Funktionstest lokal (siehe Testhinweis).

## Testhinweis (lokal, nur am Gerät — reine Szenen-/Navigationsverdrahtung, kein Host-Repro)

Reiner UI-Verdrahtungs-Umbau ohne neue Algorithmus-/Dateiformatlogik (Add/List/Delete rufen
weiterhin dieselben, unveränderten `keys_dict_*()`-Funktionen); ein Host-Repro wie bei den
bisherigen Speicherfehler-Funden bringt hier nichts, da es kein isoliert testbares
Speicher-/Parsing-Verhalten gibt. Test ausschließlich am Gerät nach dem Build:

1. NFC-App → Extra Actions → "MIFARE Classic Keys": Übersichtsbildschirm zeigt System-/
   User-Schlüsselanzahl wie vorher.
2. "Add" → 6-Byte-Testschlüssel eintragen (z. B. `FF FF FF FF FF FF`) → Save-Success-Popup →
   landet wieder auf dem Classic-Dictionary-Bildschirm (nicht auf File Select) — prüft den
   zusammengezogenen `save_success`-Zweig.
3. Denselben Schlüssel erneut über "Add" eintragen → muss den Warn-Duplicate-Popup zeigen statt
   ihn doppelt zu speichern.
4. "List" → Testschlüssel erscheint → auswählen → "Delete" → Delete-Success-Popup → landet wieder
   auf dem Classic-Dictionary-Bildschirm mit reduzierter User-Anzahl — prüft den
   zusammengezogenen `delete_success`-Zweig sowie die Live-Neuvalidierung des Index.
5. Schritte 1–4 identisch für "MIFARE Ultralight C Keys" wiederholen (5-Byte-3DES-Schlüssel).
6. Byte-Input-Leck gezielt prüfen: bei Classic "Add" einen gut erkennbaren Schlüssel eintippen
   (z. B. `AA BB CC DD EE FF`), mit Zurück abbrechen (nicht speichern), danach direkt "MIFARE
   Ultralight C Keys" → "Add" öffnen — das Eingabefeld muss leer/genullt starten, nicht mit den
   führenden Bytes des zuvor eingetippten Classic-Schlüssels vorbelegt sein.
7. Bereits vorhandene, selbst gespeicherte Dictionary-Einträge (falls vorhanden) nach dem Umbau
   erneut über "List" gegenprüfen — Dateiformat ändert sich nicht, alte Einträge müssen
   unverändert erscheinen.

## Offene Punkte / nicht Teil dieses Vorschlags

- **MIFARE Plus und Ultralight AES bleiben ohne Schlüsselverwaltung**, weil bei mir die
  zugrundeliegende schlüsselbasierte Authentifizierung für beide Protokolle komplett fehlt
  (`MfPlusKey`, `MfUltralightAesKey` und zugehörige Auth-Poller-Logik nicht in `mntm-012`
  vorhanden). Das Portieren dieser Grundlage aus Unleashed ist ein eigenständiges, deutlich
  größeres Firmware-Feature (neue Datenfelder, neue Poller-Zustände, vermutlich über mehrere
  Unleashed-Versionen gewachsen) und gehört als eigener, künftiger Firmware-Mining-Punkt ins
  Backlog, nicht in diesen Vorschlag.
- Der weitaus größere Teil von Commit `2b4411f23` (Verlagerung der Protokoll-Szenen in
  dynamisch geladene Plugins zur RAM-Reduktion) bleibt hier unbewertet — eigenständige,
  weitreichende Architekturänderung, kein kleiner Beitrag.
- Restlicher `unlshd-092`-Diff (Beispiele: NFC-Datengenerator, weitere `supported_cards`-Parser,
  `js_app`-Anwendungsmanifest) noch nicht geprüft.

## Quelle

- `DarkFlippers/unleashed-firmware`, Tag `unlshd-092`, Commit
  `2b4411f234ac9e798d941410e45ded1121fede40` (PR #1073, @myte, gemerged 20.08.2026): „[NFC]: cut
  the app's resident RAM by 54% by moving protocol scenes into their plugins" — Teilschritte
  „NFC: one user key dictionary manager instead of four copies" und „NFC: clear the key input
  between dictionaries" daraus.
- Momentum-Firmware, Tag `mntm-012`: `applications/main/nfc/helpers/mf_user_dict.c`/`.h`,
  `applications/main/nfc/scenes/nfc_scene_mf_classic_keys*.c`,
  `applications/main/nfc/scenes/nfc_scene_mf_ultralight_c_keys*.c`,
  `applications/main/nfc/nfc_app_i.h`, `applications/main/nfc/scenes/nfc_scene_config.h`,
  `applications/main/nfc/scenes/nfc_scene_extra_actions.c`,
  `applications/main/nfc/scenes/nfc_scene_{delete,save}_success.c`,
  `lib/nfc/protocols/mf_plus/mf_plus.h`, `lib/nfc/protocols/mf_ultralight/mf_ultralight.h` als
  Vergleichsbasis (Classic-/Ultralight-C-Dateien byteidentisch zum `unlshd-091`-Vorher-Stand;
  Plus-/Ultralight-AES-Schlüsseltypen bei mir nicht vorhanden, siehe oben).
