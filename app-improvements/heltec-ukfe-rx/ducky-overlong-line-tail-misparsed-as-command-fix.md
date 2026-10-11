# DuckyScript-Interpreter (heltec-ukfe-rx, byteidentisch geteilt mit lilygo-ukfe-rx) —
# eine zu lange Skriptzeile wird am Pufferlimit abgeschnitten, der Rest der echten
# Zeile aber als eigenständiger neuer Befehl fehlinterpretiert

## Was

`src/ducky.h::ducky_run()` liest jede Zeile eines DuckyScripts in einen festen
Stack-Puffer:

```c
char line[220];
while(*p) {
    int li = 0;
    while(*p && *p != '\n' && li < (int)sizeof(line) - 1) { if(*p != '\r') line[li++] = *p; p++; }
    if(*p == '\n') p++;
    line[li] = 0;
    ...
}
```

Die innere Schleife bricht korrekt ab, sobald der Puffer voll ist
(`li < sizeof(line)-1`), ohne über `line[]` hinauszuschreiben — kein
Speicherfehler. Das Problem liegt direkt danach: `if(*p == '\n') p++;` prüft
nur, ob `*p` zufällig schon ein Zeilenende ist. Bricht die innere Schleife
stattdessen ab, weil der Puffer voll ist (die physische Zeile also länger als
219 Nutzzeichen war), zeigt `p` mitten in dieser noch nicht zu Ende gelesenen
Zeile — `*p` ist weder `'\n'` noch `'\0'`. `p` wird dann nicht verschoben, und
die nächste Außenschleifen-Iteration beginnt sofort wieder an genau dieser
Stelle, liest den Rest der ursprünglich einen Zeile neu ein und verarbeitet
ihn als **eigenständige neue Kommandozeile**.

Jedes Wort, das zufällig im nicht mehr in den Puffer passenden Rest eines
langen `STRING`-Texts steht und mit einem der von `ducky_keyname()`/
`ducky_modkey()` erkannten Namen exakt übereinstimmt (`ENTER`, `TAB`, `DELETE`,
ein Modifier-Kombo wie `GUI-r`, `CTRL-ALT`, ...), wird dadurch nicht mehr als
reiner Text getippt, sondern tatsächlich als Tastendruck/Tastenkombination
ausgeführt — bei einem reinen Zufallstreffer ein harmloser zusätzlicher
Tastendruck, bei `GUI-r` z. B. der Windows-Ausführen-Dialog mitten im Tippen
eines langen Textes. Geprüft, welche eigenen Geräte dieselbe `ducky.h`
benutzen: `heltec-ukfe-rx` und `lilygo-ukfe-rx` enthalten die Datei
byteidentisch (`diff` gegen frische, unabhängige Klone beider Repos
bestätigt das), der Fund betrifft also beide gleich. `heltec-v4-hub` hat
dagegen gar keinen HID-/Ducky-Pfad (reiner ESP-NOW-Hub, kein
`USBHIDKeyboard`/`ducky.h`). `usb-army-penetrator` hat zwar auch BadUSB-
Funktionalität, aber eine komplett andere, eigenständige Implementierung
(`Attacks/Ducky/DuckyPayload.cpp/.h`, nicht dieses geteilte `ducky.h`) — ob
die denselben Fehlerfall hat, habe ich hier nicht geprüft (anderer Code,
eigener Rückstellpunkt für eine künftige Sitzung).

Mein Host-Repro (unten) zeigt das für zwei konkrete Fälle: einen Zeilenrest,
der exakt `ENTER` lautet (löst einen stillen zusätzlichen Tastendruck aus),
und einen Zeilenrest, der exakt `GUI-r` lautet (löst den Windows-Ausführen-
Dialog aus — der danach noch folgende Skripttext wird dann nicht mehr ins
beabsichtigte Zielfenster getippt).

## Nutzen

Die eingebauten `DUCKY_SCRIPTS[]` in `main.cpp` sind alle kurz genug, um nicht
betroffen zu sein. Betroffen ist der gestreamte Pfad `0x52 HidStream`: dort
baut der Flipper ein beliebig langes Skript aus vielen Funk-Frames zusammen
(`duckyStream[2048]`), bevor es als ein `ducky_run()`-Aufruf getippt wird — ein
einzelnes `STRING`/`STRINGLN` mit mehr als 219 Zeichen (z. B. ein längerer
PowerShell-Einzeiler) ist im autorisierten Testbetrieb ein realistischer Fall,
kein Exotenszenario. Der Fix macht das Verhalten für solche Zeilen
vorhersehbar (Zeile wird am Limit sauber abgeschnitten, kein Rest-Fragment
rutscht als neuer Befehl durch) statt vom zufälligen Inhalt des abgeschnittenen
Rests abhängig zu sein.

## Betroffene Dateien

- `heltec-ukfe-rx/src/ducky.h` (`ducky_run()`, Zeilenlese-Schleife)
- Identischer Fund in derselben, byteidentisch geteilten Datei auch in
  `lilygo-ukfe-rx/src/ducky.h` (`diff` gegen frische, unabhängige Klone
  beider Repos bestätigt) — hier als `heltec-ukfe-rx`-Fund dokumentiert (von
  dort auch zuletzt vollständig gelesen), der Patch ist 1:1 auf
  `lilygo-ukfe-rx` übertragbar.
- `heltec-v4-hub` hat kein `ducky.h`/keinen HID-Pfad, nicht betroffen.
  `usb-army-penetrator` hat eine andere, eigenständige Ducky-Implementierung
  (`Attacks/Ducky/DuckyPayload.cpp/.h`) — ungeprüft, eigener
  Rückstellpunkt.

## Risiko

Rein additiv: die neue Zeile greift ausschließlich, wenn der Puffer exakt
voll gelaufen ist (`li == sizeof(line) - 1`); in diesem Fall steht `*p`
entweder bereits auf `'\n'`/`'\0'` (Zeile passte exakt, kein Überlauf — die
neue Skip-Schleife läuft dann 0 Iterationen, siehe Host-Repro-Szenario 2) oder
mitten in echtem Zeilenrest, der jetzt bis zum nächsten `'\n'` verworfen statt
fehlinterpretiert wird. Für jede Zeile unter dem Limit (der ganz überwiegende
Normalfall) ändert sich nichts. Keine neue Abhängigkeit, keine
Protokoll-/Funk-Berührung.

## Fix

```c
while(*p && *p != '\n' && li < (int)sizeof(line) - 1) { if(*p != '\r') line[li++] = *p; p++; }
if(li == (int)sizeof(line) - 1) {       // Zeile zu lang fuer den Puffer: den Rest der
    while(*p && *p != '\n') p++;        // echten Zeile verwerfen statt ihn als neue,
}                                        // unabhaengige Zeile (= neuer Befehl) zu lesen.
if(*p == '\n') p++;
line[li] = 0;
```

Patch in
`ducky-overlong-line-tail-misparsed-as-command-fix/ducky_h.patch` (`git apply
--check` gegen einen frischen, unabhängigen Klon von `heltec-ukfe-rx`
verifiziert, real angewendet und byte-für-byte mit der beabsichtigten
Endfassung abgeglichen — identisch).

## Testhinweis (lokal)

1. Patch anwenden (`git apply --check` vorab, dann real) in beiden
   betroffenen Repos (`heltec-ukfe-rx`, `lilygo-ukfe-rx`) — reine
   Drei-Zeilen-Ergänzung in einer bestehenden Schleife, kein neuer
   Include/keine neue Abhängigkeit, danach jeweils `pio run`.
2. Host-Repro
   (`repro-ducky-overlong-line-tail-misparsed-as-command/repro.cpp`) deckt die
   reine Interpreter-Logik ab — `Arduino.h`/`USBHIDKeyboard.h` sind hier
   schlanke Host-Mocks (`mock/`, `String`-Klasse auf `std::string` sowie
   `press()`/`write()`/`print()`/`releaseAll()` als Log-Aufzeichnung statt
   echtem USB-HID-Traffic), die unveränderte (`ducky_orig.h`) und die gefixte
   Fassung (`ducky_fixed.h`) sind dabei unverändert übernommene Kopien aus
   `src/ducky.h`, je in einem eigenen Namensraum eingebunden. Fünf Szenarien:
   ein normales kurzes Skript (Regression, ALT/NEU identisch), eine Zeile
   exakt an der Puffergrenze ohne Überlauf (ALT/NEU identisch, bestätigt dass
   die neue Skip-Schleife beim exakten Grenzfall 0 Iterationen läuft), der
   Fund-Fall mit Zeilenrest `ENTER` (ALT injiziert einen zusätzlichen
   `WRITE`-Tastendruck, NEU nicht), der Fund-Fall mit Zeilenrest `GUI-r` (ALT
   presst zusätzlich GUI+`r`, NEU nicht) und zwei direkt aufeinanderfolgende
   überlange Zeilen (ALT injiziert zwei zusätzliche Tastendrücke, NEU
   synchronisiert sich nach jeder Zeile korrekt neu und injiziert keinen).
   Unter `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
   -fno-sanitize-recover=undefined` bei `-O0` und `-O1`: alle acht Prüfungen
   über die fünf Szenarien bestanden, identische Ausgabe beider
   Optimierungsstufen, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen.
3. Funktionstest auf Hardware (optional, da bereits über das Host-Repro
   abgedeckt): über `0x52 HidStream` ein Skript mit einer `STRING`-Zeile von
   mehr als 219 Zeichen senden, deren abgeschnittener Rest absichtlich
   `ENTER` oder `GUI-r` lautet — vor dem Fix löst das Gerät den zusätzlichen
   Tastendruck/die zusätzliche Kombination tatsächlich am Zielrechner aus,
   danach nicht mehr (die Zeile wird stattdessen nur bis zum Limit getippt).
