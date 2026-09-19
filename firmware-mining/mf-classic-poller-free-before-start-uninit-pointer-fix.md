# MFC-Poller: mf_classic_poller_free() liest uninitialisierten Speicher als Zeiger, wenn der Poller vor dem Start-State freigegeben wird

Quelle: Unleashed-Firmware-Commits `093d77e5b7a27e3f5880d084d26eda31a7f2c95f` ("NFC: Fix BusFault
in Write to Initial Card", PR #4362, 30.06.2026) und dessen unmittelbarer Nachbesserung
`de0e3cf3b13c7b641ee8a10f8c13bb8b9e75f770` ("Fix condition for cleaning up dict attack resources",
PR #4400, 01.07.2026), gefunden beim erneuten Gegenprüfen des Unleashed-`dev`-Stands seit
`unlshd-093` (offener Punkt aus mehreren vorherigen Berichten). Beide Commits stammen ursprünglich
aus der Official-Firmware (`flipperdevices/flipperzero-firmware`, gleiche PR-Nummern) und wurden
von Unleashed übernommen.

## Was

`lib/nfc/protocols/mf_classic/mf_classic_poller.c`, `mf_classic_poller_free()`: liest bei mir
bedingungslos `instance->mode_ctx.dict_attack_ctx.{mf_classic_system_dict, mf_classic_user_dict,
nested_nonce.nonces}` und ruft für jeden scheinbar nicht-NULL wirkenden Wert
`keys_dict_free()`/`free()` darauf auf — unabhängig davon, ob der Poller die Session überhaupt
jemals als Dict-Attack betrieben hat.

`instance->mode_ctx` ist ein `union` aus `MfClassicPollerWriteContext`, `MfClassicPollerReadContext`
und `MfClassicPollerDictAttackContext` (`mf_classic_poller_i.h`). Es wird erst in
`mf_classic_poller_handler_start()` — dem Handler des `MfClassicPollerStateStart`-States — per
`memset(&instance->mode_ctx, 0, sizeof(MfClassicPollerModeContext))` genullt und danach je nach
gewähltem Modus mit dem passenden Union-Member befüllt. `mf_classic_poller_alloc()` reserviert die
gesamte Instanz aber mit `malloc()`, nicht `calloc()` — bis zum ersten Durchlauf von `handler_start`
ist `mode_ctx` uninitialisierter Heap-Inhalt.

Zwischen `DetectType` (dem *ersten* State, noch vor `Start`) und dem Erreichen von `Start` liegt ein
Fenster, in dem `mode_ctx` garantiert noch kein einziges Mal geschrieben wurde — `handler_detect_type()`
prüft nur Kartentyp/`current_type_check` und rührt `mode_ctx` nicht an. Freigegeben wird der Poller
davon unabhängig: `lib/nfc/nfc_poller.c`, `nfc_poller_list_free()` ruft
`instance->list.head->poller_api->free(instance->list.head->poller)` (bei mir `mf_classic_poller_free`)
**unbedingt**, ganz gleich, in welchem internen State sich der Poller gerade befindet. Endet also eine
Session — Karte während der Typerkennung weggezogen, Nutzer bricht die Szene ab, Timeout — während der
Poller noch im `DetectType`-State steckt, liest `mf_classic_poller_free()` drei Zeigerfelder aus
Speicher, den ich nie geschrieben habe, und übergibt jeden davon, der zufällig ungleich `NULL`
aussieht, an `free()`/`keys_dict_free()`. Das ist exakt der von Unleashed unter "Write to Initial
Card" beschriebene BusFault-Mechanismus — bei mir strukturell identisch reproduzierbar, weil
`mf_classic_poller_alloc()`/`_free()`/`_handler_start()` bei mir bis auf die CUID-Erweiterung
byteidentisch zum Unleashed-Vorher-Stand sind.

**Sicherheitsrelevanz:** kein von außen kontrollierter Speicherzugriffsfehler im klassischen Sinn
(die drei gelesenen Bytefolgen stammen aus dem eigenen Heap, nicht aus Kartendaten), aber ein
lokal triggerbarer Absturz/BusFault, der beim normalen Gebrauch auftreten kann (Karte zu früh
weggezogen, Szene während der Erkennung verlassen) — ohne dass eine präparierte Karte nötig ist.

## Betroffene Dateien

- `lib/nfc/protocols/mf_classic/mf_classic_poller_i.h` — neues Feld `MfClassicPollerMode mode;` in
  `struct MfClassicPoller`, unmittelbar vor `mode_ctx` (siehe Patch). Bewusst getrennt vom bereits
  vorhandenen `mode_ctx.dict_attack_ctx.mode`-Feld, das eine andere Aufgabe hat (siehe Abgrenzung
  unten) und für Read/Write nie gesetzt wird.
- `lib/nfc/protocols/mf_classic/mf_classic_poller.c` — drei Stellen:
  1. `mf_classic_poller_alloc()`: `instance->mode = MfClassicPollerModeRead;` direkt nach den
     übrigen Feldinitialisierungen — sicherer Default, falls der Poller vor `handler_start`
     freigegeben wird (Read triggert den Cleanup-Block nie).
  2. `mf_classic_poller_handler_start()`: `instance->mode = instance->mfc_event_data.poller_mode.mode;`
     direkt nach dem `RequestMode`-Callback, vor der bestehenden `if`/`else if`-Kette — deckt alle
     vier Modi in einer Zeile ab, unabhängig von den beiden `else if`-Zweigen, die weiterhin nur
     `mode_ctx.dict_attack_ctx.mode` für ihre eigene, unveränderte interne Verwendung setzen (siehe
     unten).
  3. `mf_classic_poller_free()`: der komplette bisherige Cleanup-Block (Dictionaries +
     `nested_nonce`) wandert unter ein `if(instance->mode == ...DictAttackStandard ||
     ...DictAttackCUID || ...DictAttackEnhanced)`-Gate — alle drei Dict-Attack-Modi, wie im
     Unleashed-Endstand nach `de0e3cf` (der ursprüngliche `093d77e`-Commit vergaß zunächst
     `DictAttackCUID`, was `de0e3cf` einen Tag später nachreichte).

## Abgrenzung zum bereits vorhandenen `dict_attack_ctx.mode`-Feld

Ich habe in meiner Basis bereits ein `mode`-Feld *innerhalb* von `MfClassicPollerDictAttackContext`
(anders als Unleashed, das dieses Feld gar nicht kennt — bei mir vermutlich im Zuge der eigenen
CUID-Erweiterung entstanden). Dieses Feld wird ausschließlich in den beiden Dict-Attack-Zweigen von
`handler_start()` gesetzt und ausschließlich innerhalb von Dict-Attack-spezifischen Zuständen
gelesen (`mf_classic_poller_handler_nested_controller()` u. Ä., je zur Unterscheidung
CUID-Dict-Attack vs. Standard/Enhanced). Es wird für Read/Write **nie** geschrieben — es für das
Gate in `mf_classic_poller_free()` zu verwenden, wäre derselbe Fehler nur eine Indirektionsstufe
tiefer (ein Lesen von `dict_attack_ctx->mode` ist beim `DetectType`-Fall genauso uninitialisiert wie
alles andere in `mode_ctx`). Der Port fügt deshalb ein eigenes, vom Union-Speicher unabhängiges
Feld auf Höhe des äußeren `struct MfClassicPoller` hinzu — exakt der Weg, den Unleashed selbst
gewählt hat.

## Eigene Ergänzung gegenüber Unleashed: Default-Initialisierung in `_alloc()`

Unleasheds eigener Fix (`093d77e`, auch nach `de0e3cf`) initialisiert das neue `instance->mode`-Feld
**nicht** in `mf_classic_poller_alloc()` — dort bleibt es wie zuvor uninitialisiert, bis
`handler_start()` es setzt. Für den Normalfall (Session erreicht `Start`) macht das keinen
Unterschied. Für exakt das Szenario, das der Fix eigentlich beheben soll — Freigabe *vor*
`handler_start` — bliebe damit aber ein Rest-Loch: das neue `mode`-Feld selbst wäre in diesem Fall
ebenfalls uninitialisierter Heap-Inhalt und könnte zufällig einen der drei Dict-Attack-Werte
annehmen, wodurch der Cleanup-Block trotz Gate erneut auf ungeschriebenen Speicher zugreifen würde.
Mein Port schließt das, indem `mf_classic_poller_alloc()` das neue Feld explizit auf
`MfClassicPollerModeRead` setzt (siehe Patch) — ein Poller, der nie über `DetectType` hinauskommt,
hat damit für die gesamte Lebensdauer einen definierten, sicheren Modus.

## Risiko

Gering. Die Änderung ist rein additiv (ein neues Struct-Feld, ein neuer Gate-Zweig um bereits
bestehenden Code, zwei neue Zuweisungen) und ändert das Verhalten für jede Session, die `Start`
erreicht, nicht: `instance->mode` wird dort exakt auf denselben Wert gesetzt, den
`instance->mfc_event_data.poller_mode.mode` ohnehin schon trägt (Regressionstest unten). Für Sessions,
die vor `Start` enden, ändert sich das Verhalten gezielt zum Sicheren hin: statt ungeschriebenen
Speicher als Zeiger zu interpretieren, wird der komplette Cleanup-Block übersprungen — kein
Ressourcenleck, weil in diesem Fenster ohnehin nie ein `KeysDict` oder `nested_nonce`-Array
allokiert wurde (beide entstehen erst in Dict-Attack-spezifischen States, die alle hinter `Start`
liegen).

## Host-Repro

`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro` in
`firmware-mining/repro-mf-classic-poller-free-before-start-uninit-pointer-fix/`. Bildet
`mf_classic_poller_alloc()`/`_free()`/`_handler_start()` sowie alle drei relevanten Union-Member
1:1 nach (gleiche Feldreihenfolge/-typen wie `mf_classic_poller_i.h`), vergleicht ALT (mein
bisheriger, unveränderter Stand) gegen NEU (mit `instance->mode`-Gate). Statt vom nicht
deterministischen Inhalt einer echten `malloc()`-Region abzuhängen, wird `mode_ctx` gezielt mit
einem gewählten Bytemuster "poisoned" — das modelliert den Bereich realer, nicht-deterministischer
Heap-Inhalte, die eine ungenullte Allokation tragen kann. Statt echtem `free()` auf `KeysDict*`
kommt ein Tracking-Allokator zum Einsatz, der jeden tatsächlich selbst vergebenen Zeiger in einer
Tabelle führt und einen Abbruch meldet, sobald ein nicht dort geführter Zeiger freigegeben werden
soll — das bildet einen Invalid-Free/BusFault auf einen nicht besessenen Zeiger deterministisch ab,
unabhängig vom konkreten Allocator-/ASan-Heap-Layout. Tatsächliche Ausgabe (lokal ausgeführt):

```
== Test 1: Poller freigegeben waehrend DetectType (mode_ctx nie geschrieben) ==
  ALT: 199/199 Poison-Muster (!= 0x00) fuehren zu einem Free auf einen nicht besessenen Zeiger
  NEU: 0/200 Invalid-Frees (mode=Read blockt den gesamten Cleanup-Block)
== Test 2: Regression -- echte Dict-Attack-Session gibt weiterhin alles frei ==
  3 von 3 echten Allokationen wurden freigegeben (invalid_frees=0)
== Test 3: Alle drei Dict-Attack-Modi loesen das NEU-Gate aus, Read/Write nicht ==
  3/3 Dict-Attack-Modi geben frei, 2/2 Read/Write-Faelle bleiben trotz Poison unangetastet

ALLE CHECKS BESTANDEN
```

Drei Testgruppen: (1) 200 durchprobierte Poison-Bytemuster (0x00–0xC7) für den
`DetectType`-Freigabefall — ALT löst bei 199 von 199 Mustern ungleich `0x00` mindestens einen
Invalid-Free aus (das Muster `0x00` ist der einzige garantiert sichere Fall, weil daraus lauter
`NULL`-Zeiger werden — zeigt zugleich, dass der Bug sporadisch auftritt, abhängig vom tatsächlichen
Heap-Inhalt, was zur "manchmal, nicht immer"-Charakteristik des real gemeldeten BusFaults passt),
NEU löst bei keinem der 200 Muster einen Invalid-Free aus, weil das Gate den ganzen Block
überspringt; (2) Regressionstest — eine echte Dict-Attack-Session mit drei tatsächlich allokierten
Ressourcen (System-Dict, User-Dict, Nonce-Array) wird nach dem Fix weiterhin vollständig und korrekt
freigegeben, keine übrig gebliebene, keine fälschlich abgelehnte Freigabe; (3) alle drei
Dict-Attack-Modi (`Standard`/`CUID`/`Enhanced`) lösen das Gate korrekt aus, während `Read`/`Write`
selbst bei vorsätzlich vergiftetem `mode_ctx` keinen einzigen Cleanup-Zugriff auslösen. Unter
`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined`: keine ASan-/UBSan-Meldungen.

## Betroffene Dateien (Diffs)

Zwei Unified-Diffs gegen meine unveränderte mntm-012-Basis in
`firmware-mining/mf-classic-poller-free-before-start-uninit-pointer-fix/` — beide per
`patch -p1 --dry-run` gegen eine frische Basiskopie vorab verifiziert, real angewendet und
Byte-für-Byte gegen die separat editierte Fassung abgeglichen (identisch):
- `mf_classic_poller_i_h.patch` (11 Zeilen) — neues `mode`-Feld in `struct MfClassicPoller`.
- `mf_classic_poller_c.patch` (57 Zeilen) — die drei oben beschriebenen Änderungen in `_alloc()`,
  `_free()` und `handler_start()`.

## Testhinweis (lokal nachzuvollziehen)

1. `firmware-mining/repro-mf-classic-poller-free-before-start-uninit-pointer-fix/repro.c`
   host-kompilieren/ausführen (Befehl und tatsächliche Ausgabe siehe Host-Repro oben) — reine
   Logikverifikation ohne Hardware.
2. Beide Patches gegen eine frische `mntm-012`-Basis anwenden (`patch -p1 <
   firmware-mining/mf-classic-poller-free-before-start-uninit-pointer-fix/<datei>.patch`,
   Reihenfolge egal, unabhängig voneinander), `fbt` neu bauen.
3. Regressionstest am Gerät: einen MIFARE-Classic-Lesevorgang, einen Schreibvorgang und einen
   Standard-/CUID-/Enhanced-Dict-Attack jeweils normal bis zum Ende durchlaufen lassen und
   bestätigen, dass sich am sichtbaren Verhalten nichts ändert (insbesondere: Dict-Attack-Dateien
   werden weiterhin wie gewohnt geschrieben/gelesen, kein Speicherleck über mehrere
   Wiederholungen hinweg spürbar).
4. Den eigentlichen Fund gezielt nachstellen: einen MIFARE-Classic-Lese-, Schreib- oder
   Dict-Attack-Vorgang starten und die Karte **während der Kartentyp-Erkennung** (unmittelbar nach
   dem Auflegen, bevor der Screen zum eigentlichen Lese-/Schreib-/Angriffsbildschirm wechselt)
   wegziehen bzw. die Szene per Zurück-Taste verlassen — mehrfach wiederholen (das Zeitfenster ist
   kurz, und ob der ungeschriebene Speicher zufällig wie ein gültiger, nicht-NULL-Zeiger aussieht,
   hängt vom bisherigen Heap-Zustand ab, siehe Host-Repro Test 1). Vor dem Fix: sporadischer
   Absturz/BusFault beim Verlassen der Szene möglich. Nach dem Fix: kein Unterschied im
   Abbruchverhalten, unabhängig vom Zeitpunkt des Abbruchs.

## Quelle

- `github.com/DarkFlippers/unleashed-firmware`, Commits `093d77e5` (PR #4362, "NFC: Fix BusFault in
  Write to Initial Card") und `de0e3cf3` (PR #4400, "Fix condition for cleaning up dict attack
  resources"), Branch `dev`, Stand nach `unlshd-093` (noch kein neuer Tag mit diesen Commits
  veröffentlicht, `dev` selbst per `git ls-remote` gegen `unlshd-093` gegengeprüft).
- Ursprung beider Commits: `flipperdevices/flipperzero-firmware`, gleiche PR-Nummern (#4362/#4400),
  von Unleashed übernommen.
- `github.com/Next-Flip/Momentum-Firmware`, Tag `mntm-012`, als eigene Basis geprüft (beide
  betroffenen Dateien vorhanden, `mf_classic_poller_alloc()`/`_free()`/die relevanten Teile von
  `handler_start()` byteidentisch zum Unleashed-Vorher-Stand bis auf die bereits vorhandene eigene
  CUID-Erweiterung — Offsets der betroffenen Union-Felder per Host-`offsetof()`-Rechnung gegen die
  reale `mntm-012`-Struct-Definition verifiziert, siehe Was-Abschnitt).
- API-87-Einordnung: reine interne Struct-/Kontrollfluss-Änderung, keine neuen HAL-/API-Aufrufe,
  keine geänderte Signatur einer exportierten Funktion — kein Kompatibilitätsrisiko.
