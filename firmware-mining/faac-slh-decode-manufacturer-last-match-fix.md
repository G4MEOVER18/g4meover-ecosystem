# FAAC-SLH-Decoder: Schlüsselauswahl beim Empfang wählt den letzten statt den ersten passenden Keystore-Eintrag

## Was
Ich habe Unleasheds `dev`-Branch nach dem letzten geprüften Fenster (bis `946a52a64`) weiter
durchgesehen und bin auf Commit `178a99e5` gestoßen: „subghz: make genius remotes work"
(@xMasterX). Der Commit macht **Genius**-Fernbedienungen (KILO TX2/4 JLC, Amigo u. a.) nutzbar,
die denselben `FAAC SLH`-Rahmen wie FAAC-Empfänger verwenden, aber einen eigenen
Keystore-Herstellerschlüssel brauchen — bislang landete jede „Genius"-Erzeugung/-Erkennung
faktisch auf dem FAAC-Schlüssel, weil Unleasheds alte `faac_slh.c` beim Ver- und Entschlüsseln
hart auf den Namen `"FAAC_SLH"` verdrahtet war.

Beim Quervergleich mit meiner eigenen `lib/subghz/protocols/faac_slh.c` (Tag `mntm-012`, gegen
frischen `Next-Flip/Momentum-Firmware`-Klon verifiziert) zeigt sich: Meine Fassung ist an dieser
Stelle bereits deutlich allgemeiner gebaut als Unleasheds alte — sie iteriert nicht nach Namen,
sondern nach Schlüsseltyp (`manufacture_code->type == KEELOQ_LEARNING_FAAC`), also grundsätzlich
offen für mehrere Hersteller mit demselben Lerntyp. Genau dabei fällt aber ein eigener,
unabhängiger Fund auf: die Sendefunktion (`subghz_protocol_faac_slh_encrypt()`) und die
Empfangs-/Wiedergabefunktion (`subghz_protocol_faac_slh_check_remote_controller()`) behandeln
mehrere Treffer unterschiedlich.

Senden (`subghz_protocol_faac_slh_encrypt()`, funktioniert bereits korrekt):
```c
for
    M_EACH(manufacture_code, *subghz_keystore_get_data(instance->keystore), SubGhzKeyArray_t) {
        res = strcmp(furi_string_get_cstr(manufacture_code->name), instance->manufacture_name);
        if(res == 0) {
            switch(manufacture_code->type) {
            case KEELOQ_LEARNING_FAAC:
                man = subghz_protocol_keeloq_common_faac_learning(
                    instance->generic.seed, manufacture_code->key);
                hop = subghz_protocol_keeloq_common_encrypt(decrypt, man);
                break;
            }
            break;            // <-- verlässt die M_EACH-Schleife beim Namenstreffer
        }
    }
```
Hier bricht die äußere `if(res == 0) { ... break; }`-Klammer die Schleife beim ersten (und, da
Namen im Keystore eindeutig sind, einzig möglichen) Treffer ab — korrekt.

Empfangen/Anzeigen/erneutes Serialisieren (`subghz_protocol_faac_slh_check_remote_controller()`,
der eigentliche Fund):
```c
for
    M_EACH(manufacture_code, *subghz_keystore_get_data(keystore), SubGhzKeyArray_t) {
        switch(manufacture_code->type) {
        case KEELOQ_LEARNING_FAAC:
            man = subghz_protocol_keeloq_common_faac_learning(
                instance->seed, manufacture_code->key);
            decrypt = subghz_protocol_keeloq_common_decrypt(code_hop, man);
            *manufacture_name = furi_string_get_cstr(manufacture_code->name);
            break;            // <-- verlässt nur den switch, nicht die M_EACH-Schleife
        }
    }
instance->cnt = decrypt & 0xFFFFF;
```
Hier gibt es **kein** äußeres `break`. `break;` innerhalb von `case KEELOQ_LEARNING_FAAC:` beendet
in C ausschließlich den `switch`, nicht die umschließende Schleife — die `M_EACH`-Iteration läuft
über den **gesamten** Keystore weiter. Enthält der Keystore mehr als einen Eintrag vom Typ
`KEELOQ_LEARNING_FAAC` (genau die Situation, die Unleasheds Genius-Fix erzeugt — Genius sitzt laut
eigenem Kommentar des Commits bewusst als zweiter Eintrag desselben Lerntyps neben `FAAC_SLH` im
Keystore), überschreiben `decrypt`, `man` und `*manufacture_name` bei jedem weiteren Treffer die
Werte des vorherigen — am Ende gewinnt ausschließlich der **letzte** `KEELOQ_LEARNING_FAAC`-Eintrag
in Keystore-Reihenfolge, unabhängig davon, mit welchem Schlüssel das empfangene Signal tatsächlich
verschlüsselt wurde. Diese Funktion wird an vier Stellen aufgerufen — laufender Empfang
(`subghz_protocol_decoder_faac_slh_serialize()`, zum Schreiben der `.sub`-Datei), Bildschirmtext
(`subghz_protocol_decoder_faac_slh_get_string()`) und erneutes Senden einer bereits gespeicherten
Datei (`subghz_protocol_encoder_faac_slh_deserialize()`) — der Fund betrifft also nicht nur die
Anzeige, sondern auch den tatsächlich abgespeicherten/erneut gesendeten Schlüssel und Zähler.

## Nutzen
- Reiner Kontrollfluss-Fund in meiner eigenen `mntm-012`-Basis, unabhängig davon, ob ich Unleasheds
  Genius-Feature überhaupt porte: Sobald mein Keystore (die verschlüsselte
  `keeloq_mfcodes`-Ressource, siehe Risiko) jemals einen zweiten Eintrag vom Typ
  `KEELOQ_LEARNING_FAAC` bekommt — durch einen künftigen Ressourcen-Update, einen eigenen
  Community-Keystore oder eben den unten skizzierten Genius-Port — kippt die Schlüsselauswahl beim
  Empfang/Anzeigen/erneuten Senden **aller** `FAAC SLH`-Signale stillschweigend auf den neuen
  Eintrag, ohne Fehlermeldung, ohne dass ich es am Gerät bemerken würde (der angezeigte
  Herstellername ändert sich mit, wirkt also plausibel).
- Der Fix ist die Voraussetzung dafür, den Genius-Port überhaupt sicher anzugehen: Ohne ihn würde
  das Hinzufügen eines zweiten `KEELOQ_LEARNING_FAAC`-Eintrags aktuell korrekt funktionierende
  `FAAC_SLH`-Erkennung regressieren, statt sie um Genius zu ergänzen.
- Sicherheitsrelevant im eigentlichen Sinn des Ökosystems: `FAAC SLH` ist ein KeeLoq-artiges
  Rolling-Code-Protokoll für Tor-/Garagenantriebe — welcher Herstellerschlüssel zum Entschlüsseln
  benutzt wird, entscheidet direkt über einen korrekten vs. einen falschen (aber unauffällig
  aussehenden) Zähler-/Schlüsselwert in Anzeige, gespeicherter Datei und erneuter Aussendung.
- Der Fix selbst ist minimal (eine Zeile Zustand + ein `break`), rein additiv und für den heutigen
  Stand (vermutlich genau ein `KEELOQ_LEARNING_FAAC`-Eintrag) verhaltensidentisch — siehe Risiko.

## Betroffene Dateien (lokal)
- `lib/subghz/protocols/faac_slh.c` — einzige betroffene Datei,
  `subghz_protocol_faac_slh_check_remote_controller()`: neue lokale Variable
  `matched_faac_learning`, gesetzt beim Treffer im `switch`, danach `if(matched_faac_learning)
  break;` direkt nach dem `switch`, innerhalb der `M_EACH`-Schleife — bricht die Iteration beim
  ersten Treffer ab, spiegelbildlich zum bereits vorhandenen Muster der Sendefunktion. Kein neuer
  Include, keine neue Konstante, keine Signaturänderung.

## Risiko
- Sehr niedrig für den heutigen Stand: Enthält mein Keystore — wie vor Unleasheds Genius-Commit bei
  ihnen selbst — genau einen Eintrag vom Typ `KEELOQ_LEARNING_FAAC`, ist „erster Treffer" und
  „letzter Treffer" derselbe Eintrag; das beobachtbare Verhalten ändert sich nicht (durch
  Host-Repro exhaustiv bestätigt, siehe unten).
- Bei zwei oder mehr Einträgen desselben Lerntyps wählt der Fix konsequent den **ersten** in
  Keystore-Reihenfolge — das ist eine bewusste, aber ehrliche Einschränkung: eine echte
  Unterscheidung „welcher Hersteller hat dieses konkrete Signal wirklich erzeugt" ist aus dem
  Luft-Signal allein nicht möglich (keine Prüfsumme im `FAAC-SLH`-Rahmen, die einen
  Schlüsselkandidaten verifizieren könnte) — weder meine aktuelle Fassung noch Unleasheds alte
  Fassung noch deren neue Fassung ohne das zusätzliche `Manufacture`-Dateifeld können das. Der Fix
  macht das Verhalten lediglich **deterministisch und stabil gegenüber künftigen
  Keystore-Ergänzungen** (Anhängen eines neuen Eintrags ändert nichts mehr an bereits
  funktionierenden Signalen), statt wie bisher bei jeder Keystore-Änderung stillschweigend
  umzuschwenken. Echte Mehrdeutigkeits-Auflösung wäre erst mit dem unten skizzierten
  `Manufacture`-Dateifeld (Unleasheds eigentlicher Ansatz) möglich — bewusst nicht Teil dieses
  kleinen Fixes.
- Ob mein eigener Keystore (`applications/main/subghz/resources/subghz/assets/keeloq_mfcodes`)
  aktuell bereits mehr als einen `KEELOQ_LEARNING_FAAC`-Eintrag enthält, kann ich aus dem
  Quelltext nicht feststellen — die Datei ist verschlüsselt (`IV`+Chiffretext, eigenes Format laut
  `subghz_keystore.c`), der Klartext-Herstellername wird erst zur Laufzeit im Gerät entschlüsselt.
  Byte-Vergleich bestätigt nur: meine Ressourcendatei ist **nicht** identisch mit Unleasheds
  (unterschiedlicher SHA-256, mein Stand vermutlich älter/anders zusammengestellt) — ob „Genius"
  bei mir also schon (inert, da der Auswahl-Bug ihn verdeckt) vorhanden ist oder komplett fehlt,
  bleibt ein lokal zu klärender Punkt (z. B. über eine Debug-Ausgabe der geladenen
  Keystore-Namen).
- Host-Repro deckt ausschließlich die Auswahl-Kontrolllogik ab (welcher Keystore-Eintrag gewinnt,
  bei welcher Reihenfolge/Anzahl), nicht die eigentliche KeeLoq-FAAC-Kryptofunktion selbst (bereits
  in früheren Sitzungen unabhängig geprüft) und nicht das Laden der echten, verschlüsselten
  Keystore-Datei.

## Port-Skizze
1. `lib/subghz/protocols/faac_slh.c` öffnen,
   `subghz_protocol_faac_slh_check_remote_controller()` aufsuchen (Empfangs-/Anzeige-Zweig, zweite
   Funktion dieses Namens in der Datei — die erste ist nur die Vorwärtsdeklaration).
2. Patch anwenden (siehe `faac-slh-decode-manufacturer-last-match-fix/faac_slh_c.patch`, `git
   apply --check` vorab gegen eine frische Kopie meiner eigenen `G4MEOVER18/G4MEOVER-FW`-Basis
   verifizieren) — fügt ausschließlich die lokale `matched_faac_learning`-Variable und das
   abschließende `if(matched_faac_learning) break;` ein, keine bestehende Zeile wird entfernt oder
   umgestellt.
3. Build: `ufbt` (bzw. `fbt firmware_all`) — reine Kontrollfluss-Ergänzung innerhalb einer bereits
   vorhandenen Schleife, kein Signatur-/API-Wechsel, sollte ohne Fehler durchlaufen.
4. Funktionstest lokal: Mit genau einem `KEELOQ_LEARNING_FAAC`-Eintrag im Keystore (heutiger
   vermuteter Stand) ein vorhandenes `FAAC_SLH`-Capture einlesen/erneut senden — Anzeige,
   gespeicherter Zähler und Herstellername müssen exakt wie vor dem Patch bleiben (reine
   Regressionsprobe, siehe Host-Repro für die abgedeckte Logik).
5. **Folgeschritt, bewusst nicht Teil dieses Fixes:** Für echten Genius-Support zusätzlich
   Unleasheds `Manufacture`-Dateifeld-Ansatz (Commit `178a99e5`) portieren — ein optionales
   `Manufacture`-Feld in der `.sub`-Datei, das beim erneuten Senden/Anzeigen bevorzugt gegen den
   Namen statt nur gegen den Typ matcht, plus die zugehörigen „Add Manually"-Menüeinträge
   („Genius SLH 433/868MHz"). Unleasheds Szenen-/Menü-Dateien
   (`applications/main/subghz/plugins/add_manually/*`) liegen bei mir strukturell anders
   (`applications/main/subghz/helpers/subghz_gen_info.c`,
   `applications/main/subghz/helpers/subghz_txrx_create_protocol_key.c`,
   `applications/main/subghz/scenes/subghz_scene_set_type.c` statt eines eigenen
   `plugins/add_manually/`-Verzeichnisses) — ein direkter Patch-Transfer scheidet aus, das
   Menü-/Generierungs-Wiring muss lokal anhand dieser drei Dateien neu aufgebaut werden. Setzt
   außerdem voraus, lokal zu klären, ob mein Keystore bereits einen „Genius"-Eintrag vom Typ
   `KEELOQ_LEARNING_FAAC` enthält (siehe Risiko) — ohne einen solchen Eintrag bliebe auch ein
   vollständiger Port wirkungslos.

## Quelle
- `DarkFlippers/unleashed-firmware`, `dev`-Branch, Commit
  `178a99e5d6acecf32e6831aa8ca8b7daf08b6ae2` (@xMasterX): „subghz: make genius remotes work" —
  Anstoß für den Quervergleich, der zu diesem unabhängigen Fund in meiner eigenen Basis geführt
  hat.
- `Next-Flip/Momentum-Firmware`, Tag `mntm-012`: `lib/subghz/protocols/faac_slh.c` als
  Vergleichs-/Patch-Basis (Fund dort bestätigt vorhanden).
