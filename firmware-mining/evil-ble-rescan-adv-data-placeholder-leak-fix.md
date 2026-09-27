# Evil BLE — Rescan reaktiviert exakt den Fehler, den mein eigener Port-Fix schließen sollte

## Was
Beim erneuten Durchgehen von `firmware-mining/evil-ble-app-port.md` (mein eigener,
bereits dokumentierter Fund/Fix im Fallback-Zweig von `evil_ble.c`s `evil_ble_start_clone()`
für den Namenlos-Fall) den zugrundeliegenden Scanner-Code in `evil_ble_scanner.c`
(`barkandbite/flipper_suite`, dieselbe Quelle, frisch gegengeprüft) noch einmal Zeile für
Zeile gegen den in derselben Datei bereits vorhandenen, korrekten Platzhalter-Check
gegengelesen — und einen zweiten, eigenständigen Fund gemacht, der meinen bereits
gelieferten Fix in der Praxis wirkungslos macht.

`evil_ble_scanner_rx_cb()` dedupliziert eingehende Marauder-Zeilen nach MAC. Wird ein
bereits bekanntes Gerät erneut gemeldet, aktualisiert der Update-Zweig Name und
Sende-Payload — aber nur, wenn der neue Name „echt" ist:

```c
// evil_ble_scanner.c, ALT:
scanner->devices[i].rssi = dev.rssi;
/* Refresh name only if we got a real one this time. */
if(strncmp(dev.name, "(unknown)", 9) != 0) {
    strncpy(scanner->devices[i].name, dev.name, EVIL_BLE_NAME_LEN - 1);
    scanner->devices[i].name[EVIL_BLE_NAME_LEN - 1] = '\0';
    scanner->devices[i].adv_data_len =
        build_adv_from_name(scanner->devices[i].name, scanner->devices[i].adv_data);
}
```

Das Problem: `dev->name` enthält für ein nur per MAC gemeldetes Gerät nie den String
`"(unknown)"`, sondern laut `parse_scanbt_line()` (dieselbe Datei) immer die
Anzeige-Platzhalterform `"(AA:BB:CC:DD:EE:FF)"` — exakt das Format, dessen führendes
Klammer-Zeichen die App an zwei anderen Stellen bereits korrekt selbst prüft: einmal am
Ende von `parse_scanbt_line()` (`if(dev->name[0] != '\0' && dev->name[0] != '(')`, direkt
vor dem Aufbau der Sende-Payload), und ein zweites Mal in meinem eigenen bereits
gelieferten Fix für `evil_ble.c`s Fallback-Zweig. Der `strncmp(..., "(unknown)", 9)`-Vergleich
in `evil_ble_scanner.c`s Update-Zweig ist damit toter, nie zutreffender Code — die
Bedingung ist für jede erdenkliche Eingabe wahr, der Zweig „aktualisiere Name/Payload" läuft
also bei **jedem** erneuten Sichten eines bereits bekannten Geräts, unabhängig davon, ob
diese Zeile einen echten Namen enthielt.

Konkrete Auswirkung: Ein Gerät, das Marauder ausschließlich per MAC meldet (kein
Namensfeld je gesendet — genau der Fall, für den mein `evil_ble.c`-Fix gedacht ist), wird
beim allerersten Sichten korrekt mit `adv_data_len = 0` angelegt (`parse_scanbt_line()`
selbst überspringt `build_adv_from_name()` wegen desselben, dort korrekt vorhandenen
Checks). Meldet Marauder dasselbe Gerät nur ein einziges weiteres Mal — bei einer
laufenden `scanbt`-Sitzung gegen ein Gerät in Reichweite der absolute Normalfall, keine
Rand-Bedingung — greift der Update-Zweig, findet den Klammer-Platzhalter „echt" (weil er
nicht wortwörtlich `"(unknown)"` ist) und ruft `build_adv_from_name()` **ungeprüft** mit
genau diesem Platzhalter als „Name" auf. `scanner->devices[i].adv_data_len` kippt dadurch
von 0 (richtig: „kein Name bekannt") auf einen positiven Wert, dessen Payload den
Platzhaltertext `"(AA:BB:CC:DD:EE:FF)"` als vermeintliches „Complete Local Name"-Feld
(AD-Typ `0x09`) enthält.

Das ist exakt der Fehlerzustand, den mein bereits gelieferter `evil_ble.c`-Fix verhindern
sollte — nur entsteht er jetzt schon in der Scanner-Tabelle selbst, bevor `evil_ble.c`
überhaupt zum Zug kommt. `evil_ble_start_clone()` prüft nur `if(dev.adv_data_len > 0)` und
übernimmt die Payload dann unbesehen (`memcpy(adv, dev.adv_data, dev.adv_data_len)`) — mein
Fix sitzt ausschließlich im `else`-Fallback-Zweig für `adv_data_len == 0` und wird hier gar
nicht erst erreicht, weil `adv_data_len` durch den hier gefundenen Fehler längst wieder auf
einen positiven Wert gesetzt wurde. Für jedes Zielgerät, das während des Scans mehr als
einmal gemeldet wird — praktisch jedes real existierende Gerät in Reichweite über die
Laufzeit eines Scans — hebelt dieser zweite Fund meinen ersten Fix vollständig aus, ohne
dass das an der Oberfläche sichtbar wäre (kein Absturz, kein offensichtlich falscher
Anzeigewert in der Geräteliste, da die Liste ohnehin `"(AA:BB:CC:DD:EE:FF)"` als
Platzhalter-„Namen" anzeigt — nur der tatsächlich gesendete Klon-Payload ist betroffen).

## Nutzen
- Schließt die Lücke, die mein bereits gelieferter `evil-ble-app-port.md`-Fix ohne diesen
  zweiten Fund faktisch offen ließe: ohne beide Fixes gemeinsam bleibt der ursprünglich
  gefundene Fehler (Klon eines namenlosen Zielgeräts broadcastet fälschlich einen
  Namens-AD-Eintrag) für den weit überwiegenden praktischen Fall — jedes Gerät, das
  während des Scans mehr als einmal erfasst wird — unverändert bestehen, obwohl der erste
  Fix isoliert betrachtet korrekt und vollständig getestet war.
- Der genau in der Ursprungs-App-Beschreibung genannte Testzweck („test if systems properly
  validate device identity beyond name/MAC") bleibt für den Namenlos-Fall damit erst mit
  beiden Fixes zusammen tatsächlich erfüllt.
- Minimal-invasiv: derselbe, bereits an zwei anderen Stellen in genau dieser App
  verwendete Platzhalter-Check, nur an der dritten, noch fehlenden Stelle ergänzt — keine
  Verhaltensänderung für den Regelfall (Gerät mit echtem, wiederholt gemeldetem Namen),
  durch Host-Repro bestätigt.

## Betroffene Dateien (lokal)
- `applications_user/evil_ble/evil_ble_scanner.c` — **mit dem unten beschriebenen Fix**,
  ergänzend zum bereits in `firmware-mining/evil-ble-app-port.md` dokumentierten Fix in
  `evil_ble.c`. Beide Fixes gehören zusammen appliziert; keiner der beiden macht den
  anderen entbehrlich.

## Risiko
- Niedrig, reine Ein-Zeilen-Bedingungsänderung in einer bestehenden `if`-Guard, keine neue
  Kontrollflusslogik, kein Speicherzugriffsfehler (weder ALT noch NEU — beide Varianten
  bleiben innerhalb der bereits fest allozierten `EVIL_BLE_NAME_LEN`/
  `EXTRA_BEACON_MAX_DATA_SIZE`-Puffer, durch Host-Repro unter ASan/UBSan bestätigt).
- Kein Regressionsrisiko für den Regelfall: ein Gerät mit echtem, mehrfach gemeldetem Namen
  verhält sich unter ALT und NEU byte-identisch (Testhinweis unten, Fall 2).
- Ohne diesen Fix bleibt der bereits als „behoben" dokumentierte `evil_ble.c`-Fund in der
  Praxis weiterhin aktiv — wer nur den `evil_ble.c`-Fix aus `evil-ble-app-port.md` isoliert
  übernimmt, bekommt keinen wirksamen Schutz gegen den ursprünglich gefundenen
  Namenlos-Fehler.

## Port-Skizze
1. Patch `evil_ble_scanner_c.patch` (unten) auf die bereits nach
   `applications_user/evil_ble/` kopierte `evil_ble_scanner.c` anwenden — dieselbe Kopie,
   die laut `evil-ble-app-port.md` bereits um den `evil_ble.c`-Fix ergänzt wurde/wird.
2. `ufbt launch_app APPSRC=applications_user/evil_ble` bzw.
   `fbt launch_app APPSRC=applications_user/evil_ble` — reine Bedingungsänderung, keine
   neuen Includes/Symbole, sollte ohne Compile-Fehler durchlaufen.
3. Host-seitiges Mini-Repro (`repro.c`, unten) mit
   `gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O0`/`-O1` bauen und laufen
   lassen — bereits von mir lokal ausgeführt, siehe Testhinweis.
4. Funktionstest (**nur gegen eigene Geräte, autorisiertes Testumfeld**), Fortsetzung des in
   `evil-ble-app-port.md` beschriebenen Regressionstests:
   - Ein eigenes Gerät ohne broadcasteten Namen scannen und **mindestens zweimal** von
     Marauder erfassen lassen (z. B. kurz warten, bevor „Clone" ausgelöst wird) — vor
     diesem zweiten Fix erscheint im Klon-Broadcast trotz des bereits angewendeten
     `evil_ble.c`-Fixes wieder ein Namens-AD-Feld mit dem MAC-Platzhaltertext; nach diesem
     Fix bleibt der Klon-Broadcast korrekt namenlos.
   - Regressionstest: ein eigenes Gerät mit echtem Namen mehrfach erfassen lassen, dann
     klonen — Name/Payload müssen unverändert dem Original entsprechen.

## Testhinweis (lokal nachvollzogen)
Host-kompilierbares Mini-Repro (`repro.c`) des exakten Dedup-/Update-Codeblocks aus
`evil_ble_scanner_rx_cb()`: ALT (unverändert aus der Quelle übernommen) vs. NEU (mit dem
oben beschriebenen Platzhalter-Check). Gebaut/ausgeführt mit
`gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined` bei `-O0` und `-O1`, identisches
Ergebnis bei beiden Optimierungsstufen:

```
[ALT] case1 MAC-only device, 2nd sighting: adv_data_len=21 (BUG: cloner will broadcast
      "(AC:23:3F:11:22:33)" as the target's name)
[NEU] case1 MAC-only device, 2nd sighting: adv_data_len=0 (correct: MAC-only clone, no
      fabricated name)
[regression] case2 named device ("MyPhone"), 3x rescanned: ALT len=9 NEU len=9,
      payload identical=yes
[regression] case3 empty-name re-report: adv_data_len stays 0 under both variants

17/17 CHECKS PASSED
RESULT: bug reproduced under ALT, fix confirmed under NEU, no regression in case2/case3.
```

Drei Fälle geprüft: (1) ein nur per MAC gemeldetes Gerät, beim zweiten Sichten — ALT
kippt `adv_data_len` fälschlich auf 21 (Länge des kodierten Platzhaltertextes), NEU
belässt es korrekt bei 0; (2) Regression — ein Gerät mit echtem, dreimal in Folge
gemeldetem Namen liefert unter ALT und NEU byte-identische `adv_data`-Payload und
-Länge; (3) Regression — ein Gerät, das (defensiv gegen einen theoretischen Leerzeilen-Fall)
mit leerem Namen erneut gemeldet wird, erzeugt unter beiden Varianten weiterhin
`adv_data_len == 0`. Keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen bei
`-Wall -Wextra` unter beiden Optimierungsstufen.

Unified-Diff (`git apply --check` vorab gegen eine frische Kopie von
`barkandbite/flipper_suite`s `evil_ble/evil_ble_scanner.c` verifiziert, real angewendet und
byte-für-byte gegen die beabsichtigte Endfassung abgeglichen) in
`evil-ble-rescan-adv-data-placeholder-leak-fix/evil_ble_scanner_c.patch`. Host-Repro in
`repro-evil-ble-rescan-adv-data-placeholder-leak-fix/repro.c`.

## Quelle
- `barkandbite/flipper_suite`, `evil_ble/evil_ble_scanner.c` (frischer Klon, `main`,
  Commit `0d75d6a`) — dieselbe Quelle wie mein bereits dokumentierter Fund/Fix in
  `firmware-mining/evil-ble-app-port.md`.
- Eigener, bereits dokumentierter Vorfund: `firmware-mining/evil-ble-app-port.md`
  (Fallback-Zweig-Fix in `evil_ble.c`) — dieser Fund ergänzt jenen Fix um die zweite,
  bislang übersehene Stelle, an der genau dasselbe Platzhalter-Problem erneut auftritt.
