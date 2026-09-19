# MIFARE-Classic-Woerterbuch-Angriff laeuft bei Schluesselwiederverwendung ueber eine bereits vollstaendig geloeste Karte hinaus

Quelle: Unleashed-Firmware-Commit `7db0c26d0596a427b9568f8a9e6b6b2633b21b62` ("NFC: stop the MIFARE
Classic dictionary attack once the card is fully read", PR #1151, 19.09.2026, Fixes #1150). Noch
nicht in einem `unlshd-`-Tag enthalten (`dev` steht aktuell 12 Commits vor `unlshd-093`). Gefunden
beim Nachpruefen des zuletzt als offen vermerkten Punkts "Unleashed dev seit unlshd-093" -- dabei
zunaechst festgestellt, dass die im letzten Bericht dafuer notierten sieben Kandidaten-Hashes
(`c873eb5`, `7518dc7`, `a534a72`, `97e088e`, `6c83a67`, `101c20d`, `9148d50`) allesamt bereits
Vorfahren von `unlshd-093` sind (`git merge-base --is-ancestor <hash> unlshd-093` liefert fuer jeden
Wahr) und mit dem tatsaechlichen `dev`-Fenster seit `unlshd-093` nichts zu tun haben -- Korrektur
dazu unten und im Backlog. Die real neuen zwoelf Commits seit `unlshd-093` habe ich stattdessen
selbst aufgelistet (`git log --oneline unlshd-093..origin/dev`); dieser Fund ist der erste
inhaltliche Treffer daraus (die uebrigen elf sind Versions-/Katalog-Housekeeping oder RFID-Themen
ausserhalb des NFC-Schwerpunkts dieser Sitzung, siehe unten offen).

## Was

`lib/nfc/protocols/mf_classic/mf_classic_poller.c` sowie
`applications/main/nfc/scenes/nfc_scene_mf_classic_dict_attack.c`: der Woerterbuch-Angriff gegen
MIFARE-Classic-Karten kennt keinen fruehzeitigen Abbruch, wenn die Karte bereits vollstaendig
gelesen ist.

Viele reale Karten (v. a. Werks-/Testkarten und Transit-/Zutrittsmedien mit einem einzigen,
sektoruebergreifenden Schluessel) verwenden denselben Schluessel fuer mehrere oder alle Sektoren.
Mein Woerterbuch-Angriff nutzt das bereits aus (`mf_classic_set_key_found()` traegt einen
gefundenen Schluessel fuer jeden Sektor ein, an dem er tatsaechlich passt, nicht nur fuer den
gerade angefragten) -- eine solche Karte kann dadurch vollstaendig geloest sein, waehrend der
Sektor-Cursor der eigentlichen Angriffsschleife noch bei Sektor 0 steht. Zwei Stellen bemerken das
nicht:

1. **Poller-Ebene** (`mf_classic_poller_handler_request_key()`, State
   `MfClassicPollerStateRequestKey`): fragt bei jedem Sektorwechsel unbedingt erneut das Woerterbuch
   nach einem Schluessel -- auch fuer laengst (durch Wiederverwendung) bereits aufgeschlossene
   Sektoren. Jeder verbleibende Sektor kostet dadurch einen vollen Woerterbuchdurchlauf, obwohl das
   Ergebnis (Karte bereits vollstaendig) laengst feststeht. Dasselbe gilt fuer
   `mf_classic_poller_handler_nested_controller()` beim Eintritt in die Nested-Attack-Phase
   (`nested_phase == MfClassicNestedPhaseNone`): der Nested-Angriff hat auf einer bereits geloesten
   Karte nichts mehr zu gewinnen, wird aber trotzdem initialisiert.
2. **Szenen-Ebene** (`nfc_scene_mf_classic_dict_attack_on_event()`): sowohl beim regulaeren
   Abschluss einer Woerterbuch-Phase (`NfcCustomEventDictAttackComplete`) als auch beim manuellen
   Ueberspringen (`NfcCustomEventDictAttackSkip`) wechselt die Szene bedingungslos in die naechste
   Phase (CUID-Dictionary -> User-Dictionary -> System-Dictionary), sobald die aktuelle Phase zu
   Ende ist bzw. uebersprungen wird -- unabhaengig davon, ob die Karte zu diesem Zeitpunkt bereits
   vollstaendig gelesen ist. Jeder solche unnoetige Phasenwechsel alloziert einen neuen Poller,
   startet einen neuen Blink/Notification-Zyklus und haengt (bei User-/System-Dictionary) einen
   vollen Datei-Woerterbuch-Scan an.

Nettoeffekt bei mir (identisch zum Unleashed-Vorher-Stand, per Quellvergleich verifiziert, siehe
unten): eine Karte mit sektoruebergreifend wiederverwendetem Schluessel wird zwar korrekt und
vollstaendig gelesen, der Angriff wirkt dabei aber "haengengeblieben" -- er durchlaeuft weiterhin
jeden restlichen Sektor gegen das komplette Woerterbuch und startet anschliessend unnoetig die
naechste Woerterbuch-Phase auf einer Karte, die nichts mehr zu finden hat, bevor er schliesslich
doch zu "Read Success" kommt. Bei grossen Woerterbuchdateien (System-Dictionary) macht das den
eigentlich sofort abgeschlossenen Angriff spuerbar langsamer, ohne dass sich am Ergebnis etwas
aendert.

## Betroffene Dateien

Meine `mntm-012`-Basis hat eine deutlich aeltere NFC-App-Architektur als der Unleashed-Vorher-Stand
(eigenstaendige Szene `applications/main/nfc/scenes/nfc_scene_mf_classic_dict_attack.c` statt
Unleasheds neuerem `applications/main/nfc/helpers/protocol_support/mf_classic/mf_classic_extra_scenes.c`)
-- der Fund selbst (fehlender fruehzeitiger Abbruch) sowie die betroffenen Zustandsnamen
(`DictAttackStateCUIDDictInProgress`/`UserDictInProgress`/`SystemDictInProgress`,
`NfcCustomEventDictAttackComplete`/`DictAttackSkip`, `nested_phase`) und die zugrunde liegende
Poller-API (`mf_classic_get_read_sectors_and_keys()`, `mf_classic_is_card_read()`,
`instance->sectors_total`, `instance->data`) sind aber byteidentisch bzw. strukturell identisch
vorhanden -- der Port ist eine direkte Uebertragung derselben Guard-Bedingung auf meine eigene
Dateilage, keine Neukonstruktion.

- `lib/nfc/protocols/mf_classic/mf_classic_poller.c`:
  - neue statische Hilfsfunktion `mf_classic_poller_is_card_read()` (identisch zu Unleasheds
    Fassung uebernommen).
  - `mf_classic_poller_handler_request_key()`: Guard am Funktionsanfang, springt bei bereits
    vollstaendig gelesener Karte direkt zu `MfClassicPollerStateSuccess`.
  - `mf_classic_poller_handler_nested_controller()`: derselbe Guard beim Eintritt in
    `nested_phase == MfClassicNestedPhaseNone`.
- `applications/main/nfc/scenes/nfc_scene_mf_classic_dict_attack.c` (mein eigenstaendiges
  Aequivalent zu Unleasheds `mf_classic_extra_scenes.c`, andere Datei/Funktionsnamen, gleiche
  Zustandsmaschine):
  - neue statische Hilfsfunktion `nfc_scene_mf_classic_dict_attack_is_card_read()` -- bewusst ueber
    `nfc_poller_get_data(instance->poller)` statt ueber `nfc_device_get_data()` (Unleasheds
    Variante): bei mir schreibt die Szene die Kartendaten nur beim finalen Erfolg
    (`MfClassicPollerEventTypeSuccess`) in `instance->nfc_device` zurueck, waehrend der Poller
    selbst waehrend des laufenden Angriffs kontinuierlich aktualisiert wird und an jeder
    betroffenen Aufrufstelle noch alloziert ist (erst danach freigegeben) -- die Geraetedaten waeren
    hier also veraltet, die Poller-Daten sind die tatsaechlich aktuellen.
  - `nfc_scene_mf_classic_dict_attack_on_event()`, `NfcCustomEventDictAttackComplete`: beide
    Phasenuebergaenge (CUID->User, User->System) zusaetzlich an `!card_read` gebunden.
  - `nfc_scene_mf_classic_dict_attack_on_event()`, `NfcCustomEventDictAttackSkip`: beide
    Phasenuebergaenge zusaetzlich an `!card_read` gebunden (dort bereits vorhandene
    `mfc_data`-Variable wiederverwendet, kein zusaetzlicher Poller-Zugriff noetig).

## Risiko

Gering. Beide Guards sind rein additiv (ein zusaetzlicher frueher Ausstiegspunkt bzw. eine
zusaetzliche `&&`-Bedingung an bereits bestehenden Uebergaengen) und aendern das Verhalten nur in
dem Fall, der vorher ohnehin bereits zum selben Endergebnis fuehrte (Karte vollstaendig gelesen) --
nur schneller und ohne die ueberfluessigen Zwischenschritte. Der Regressionsfall (Karte noch nicht
vollstaendig gelesen) durchlaeuft exakt denselben Code wie vorher, da `card_read` dort `false` ist
und keine der neuen Bedingungen greift (Host-Repro unten, Test 1 Regression / Test 2 Fall B / Test 3
Regressionsfaelle). `mf_classic_poller_is_card_read()` liest ausschliesslich bereits vorhandene,
unveraenderte Zaehlfunktionen (`mf_classic_get_read_sectors_and_keys()`); keine neue Zustandsvariable,
kein zusaetzlicher Speicher noetig ausser dem einen `bool card_read` je Aufrufstelle.

## Host-Repro

`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro` in
`firmware-mining/repro-mf-classic-dict-attack-stop-on-full-read/`. Bildet drei Ebenen nach:

1. Die unveraenderte Referenzsemantik von `mf_classic_get_read_sectors_and_keys()`/
   `mf_classic_is_card_read()` selbst (Sanity-Check).
2. Die Poller-Ebene (`mf_classic_poller_handler_request_key()`): simuliert einen
   Schluesselwiederverwendungs-Treffer an Sektor 0, der die gesamte Karte sofort vollstaendig
   loest, und zaehlt Woerterbuch-Abfragen fuer die restlichen sieben Sektoren ALT vs. NEU.
3. Die Szenen-Ebene (`nfc_scene_mf_classic_dict_attack_on_event()`), sowohl fuer den
   Complete- als auch den Skip-Pfad: zaehlt, wie oft ALT vs. NEU eine neue Woerterbuch-Phase
   startet, wenn die Karte am Ende der aktuellen Phase bereits vollstaendig ist.

Tatsaechliche Ausgabe (lokal ausgefuehrt):

```
== Sanity: is_card_read()-Referenzsemantik ==
  OK

== Test 1: Poller-Ebene (mf_classic_poller_handler_request_key) ==
  ALT: 320 Woerterbuch-Abfragen fuer 8 Sektoren (voller Durchlauf trotz geloester Karte)
  NEU: 40 Woerterbuch-Abfragen (bricht sofort nach der geloesten Karte ab)
  Regression (keine Wiederverwendung): ALT=320 NEU=320 Abfragen (gleich)

== Test 2: Szenen-Ebene (nfc_scene_mf_classic_dict_attack_on_event, NfcCustomEventDictAttackComplete) ==
  Fall A (Karte bereits bei CUID-Abschluss geloest): ALT startet Phase 1 (phase_starts=1), NEU springt direkt zu ReadSuccess (phase_starts=0)
  Fall B (Karte am CUID-Ende noch unvollstaendig, Regression): ALT und NEU wechseln beide in Phase 1 (phase_starts=1)
  Fall C (Karte bereits bei User-Abschluss geloest): ALT startet Phase 2 (phase_starts=1), NEU springt direkt zu ReadSuccess (phase_starts=0)

== Test 3: Szenen-Ebene, Skip-Pfad (NfcCustomEventDictAttackSkip) ==
  Karte anwesend + bereits geloest: ALT startet Phase 1, NEU springt zu ReadSuccess
  Regressionsfaelle (Karte unvollstaendig / nicht anwesend): ALT und NEU identisch

14/14 Checks bestanden
ALLE CHECKS BESTANDEN
```

Keine ASan-/UBSan-Meldungen. Der Poller-Test zeigt den Kerneffekt quantitativ: acht Mal so viele
Woerterbuch-Abfragen bei ALT (320 statt 40) fuer denselben Endzustand; der Szenen-Test zeigt, dass
ALT in allen drei betroffenen Faellen eine zusaetzliche, nutzlose Woerterbuch-Phase anstoesst,
waehrend die Regressionsfaelle (Karte noch unvollstaendig, Karte nicht anwesend) in allen Tests
byteidentisches Verhalten liefern.

## Betroffene Dateien (Diffs)

Zwei Unified-Diffs gegen meine unveraenderte mntm-012-Basis in
`firmware-mining/mf-classic-dict-attack-stop-on-full-read-port/` -- beide per `patch -p1 --dry-run`
gegen eine frische Basiskopie vorab verifiziert, real angewendet und Byte-fuer-Byte gegen die
separat editierte Fassung abgeglichen (identisch):

- `mf_classic_poller_c.patch` (28 Zeilen) -- neue Hilfsfunktion plus zwei Guard-Einfuegungen in
  `mf_classic_poller.c`.
- `nfc_scene_mf_classic_dict_attack_c.patch` (33 Zeilen) -- neue Hilfsfunktion plus vier
  Bedingungserweiterungen in `nfc_scene_mf_classic_dict_attack.c`.

## Testhinweis (lokal nachzuvollziehen)

1. `firmware-mining/repro-mf-classic-dict-attack-stop-on-full-read/repro.c` host-kompilieren/
   ausfuehren (Befehl und tatsaechliche Ausgabe siehe Host-Repro oben) -- reine Logikverifikation
   ohne Hardware.
2. Beide Patches gegen eine frische `mntm-012`-Basis anwenden (`patch -p1 <
   firmware-mining/mf-classic-dict-attack-stop-on-full-read-port/<datei>.patch`, Reihenfolge egal,
   unabhaengig voneinander), `fbt` neu bauen.
3. Am Geraet: eine Testkarte mit sektoruebergreifend identischem Schluessel (z. B. eine eigene,
   frisch mit demselben Schluessel auf allen Sektoren beschriebene Karte, oder eine Werks-/
   Transitkarte mit bekanntermassen wiederverwendetem Schluessel) gegen das System-Dictionary
   angreifen -- vor dem Fix laeuft der Angriff sichtbar weiter (Sektor-Fortschrittsanzeige zaehlt
   nach dem eigentlichen Treffer sichtbar langsamer/laenger weiter, ggf. zusaetzlicher
   Phasenwechsel-Bildschirm), nach dem Fix springt er unmittelbar nach dem entscheidenden Treffer zu
   "Read Success".
4. Regressionsprobe: eine gewoehnliche Karte ohne Schluesselwiederverwendung (jeder Sektor braucht
   seinen eigenen Schluessel) vollstaendig durchlaufen lassen -- Verhalten/Zeitbedarf unveraendert
   gegenueber dem bisherigen Stand.
5. Regressionsprobe Skip-Taste: waehrend eines laufenden Angriffs auf eine noch nicht vollstaendig
   gelesene Karte "Skip" druecken -- weiterhin der bisherige Wechsel in die naechste
   Woerterbuch-Phase (CUID->User bzw. User->System).

## Quelle

- `github.com/DarkFlippers/unleashed-firmware`, Commit `7db0c26d0596a427b9568f8a9e6b6b2633b21b62`
  (PR #1151, "NFC: stop the MIFARE Classic dictionary attack once the card is fully read", Fixes
  #1150, 19.09.2026) -- Teil der zwoelf Commits zwischen `unlshd-093` und `origin/dev`
  (`git log --oneline unlshd-093..origin/dev`), noch nicht in einem `unlshd-`-Tag enthalten.
- `github.com/Next-Flip/Momentum-Firmware`, Tag `mntm-012`, als eigene Basis geprueft: beide
  betroffenen Konzepte (Poller-Zustandsautomat in `mf_classic_poller.c`, eigenstaendige
  Dict-Attack-Szene mit denselben drei Phasen) vollstaendig vorhanden, ohne den portierten Fix.
- **Korrektur zum letzten Bericht (`reports/2026-09-19-0634.md`):** die dort unter "Unleashed dev
  seit unlshd-093" vorgemerkten sieben Kandidaten (`c873eb5`, `7518dc7`, `a534a72`, `97e088e`,
  `6c83a67`, `101c20d`, `9148d50`) sind samt und sonders bereits Vorfahren von `unlshd-093`
  (per `git merge-base --is-ancestor <hash> unlshd-093` einzeln bestaetigt, Commit-Datumsbereich
  November 2025 bis August 2026, also lange vor dem Tag-Datum von `unlshd-093` selbst). Stichprobe
  `c873eb5` ("Fix Ultralight EV1 regression") zusaetzlich inhaltlich gegengeprueft: der Fix
  (`if(data->type == MfUltralightTypeMfulC)`-Absicherung um die `tdes_key`-Zuweisung in
  `applications/main/nfc/helpers/protocol_support/mf_ultralight/mf_ultralight.c`) ist bei mir
  bereits identisch vorhanden -- kein Portaufwand, nichts zu tun. Die tatsaechlichen zwoelf Commits
  zwischen `unlshd-093` und `origin/dev` habe ich frisch selbst ermittelt (siehe oben); dieser
  Fund ist der erste davon. Damit ist der alte, fehlerhafte Kandidaten-Hinweis aus dem Backlog zu
  entfernen (siehe `BACKLOG.md`).
- API-87-Einordnung: reine interne Zustandsautomat-/Szenenlogik, keine neuen HAL-/API-Aufrufe, keine
  geaenderte Signatur einer exportierten Funktion -- kein Kompatibilitaetsrisiko.
