# DuckyScript-Interpreter (heltec-ukfe-rx / lilygo-ukfe-rx) — Zeile über Pufferlänge
# hinweg wird in eine eigenständige, unbeabsichtigte Folgezeile zerschnitten

## Was

`src/ducky.h` ist bei mir in `heltec-ukfe-rx` und `lilygo-ukfe-rx` byte-identisch (Datei
laut eigenem Kommentar bewusst "Wiederverwendbar (V3/LilyGo/V4)"). `ducky_run()` liest
jede physische Skriptzeile in einen festen Stack-Puffer:

```c
static void ducky_run(USBHIDKeyboard& kb, const char* script) {
    ...
    char line[220];
    while(*p) {
        int li = 0;
        while(*p && *p != '\n' && li < (int)sizeof(line) - 1) { if(*p != '\r') line[li++] = *p; p++; }
        if(*p == '\n') p++;
        line[li] = 0;
        ...   // line[] wird sofort als vollstaendiges Kommando ausgefuehrt
    }
}
```

Die innere Schleife bricht aus zwei völlig verschiedenen Gründen ab, behandelt beide
Fälle aber identisch: entweder die physische Zeile ist zu Ende (`*p=='\n'`/`*p==0`),
oder der 220-Byte-Puffer ist voll (`li == sizeof(line)-1`), während die Zeile im Skript
noch weitergeht. Im zweiten Fall zeigt `p` **nicht** auf eine echte Zeilengrenze, sondern
mitten in die noch laufende physische Zeile. `line[li]=0` terminiert trotzdem, und das
Kommando wird sofort ausgeführt — danach liest die äußere Schleife beim nächsten
Durchlauf den Rest der ursprünglichen Zeile einfach als **neue, eigenständige**
Kommandozeile ein und führt auch diese aus.

Konkrete Auswirkung: Bei jeder physischen Zeile über 219 Zeichen (z. B. eine lange
`STRING`-Payload — Standard-DuckyScript kennt für `STRING` keine Zeilenlängenbegrenzung)
wird

1. das `STRING`-Argument stillschweigend auf die ersten ca. 212 Zeichen abgeschnitten
   (Datenverlust, aber für sich genommen schon vom knappen Puffer zu erwarten), UND
2. der abgeschnittene Rest als **eigenes neues Kommando** erneut geparst. Enthält dieser
   Rest zufällig (z. B. weil die eigentliche Payload ein normales englisches Wort wie
   "home", "end", "insert", "tab" enthält, oder weil ein Angriffsskript absichtlich so
   gebaut ist) an seinem Anfang ein von `ducky_keyname()`/`ducky_modkey()` erkanntes
   Token, wird dieses als **zusätzlicher, nicht beabsichtigter Tastendruck** ausgeführt
   (z. B. ein ungewolltes `ENTER`, `TAB`, `CTRL`, `GUI`/Windows-Taste …) — mitten in
   einer eigentlich reinen Texteingabe. Der Fehler wiederholt sich fortlaufend über
   beliebig viele Pufferbreiten hinweg, bis die echte Zeilengrenze erreicht ist.

Betrifft ausschließlich `ducky_run()` selbst (die Zeilen-Extraktion); `ducky_keyname()`,
`ducky_modkey()` und `ducky_press_mods()` sind von diesem Fund unabhängig korrekt und
bleiben unverändert.

## Nutzen

Verlässliche BadUSB-Payload-Wiedergabe ohne unvorhersehbare, von der konkreten
Zeichenkette abhängige Fehltastendrücke — relevant sowohl für die beabsichtigte
Nutzung (ein zu langer Text wird sauber abgeschnitten statt zusätzlich eine zufällige
Taste auszulösen) als auch für die Robustheit bei versehentlich zu langen Zeilen in
selbst geschriebenen Skripten.

## Betroffene Dateien

- `heltec-ukfe-rx/src/ducky.h`
- `lilygo-ukfe-rx/src/ducky.h`

(identische Datei in beiden Repos — ein Patch, zweimal gegen den jeweiligen Stand
verifiziert.)

## Risiko

Reiner Logik-/Korrektheitsfund, keine Protokoll-/Sicherheitsänderung, kein
Zusammenspiel mit `ukfe_rf.c`/ESP-NOW. Der Fix ist rein additiv (3 Zeilen, keine
bestehende Zeile verändert) und ändert das Verhalten nur für den bisher fehlerhaften
Überlauffall — reguläre Zeilen bis 219 Zeichen sind byte-identisch zum Ist-Zustand
(siehe Host-Repro, Szenario 1 und 5). Die inhärente Puffergrenze von ~212 nutzbaren
Zeichen pro `STRING`-Zeile bleibt bestehen (das ist eine bewusste Speicher-/Stack-Grenze,
kein Ziel dieses Fixes) — der Fix verhindert nur, dass der abgeschnittene Rest als
eigenständiges Kommando fehlinterpretiert wird.

## Fix

In `ducky_run()` nach der Zeilen-Einlese-Schleife erkennen, ob der Abbruch durch
Pufferüberlauf (statt durch `'\n'`/Skriptende) erfolgte, und in diesem Fall den Rest der
physischen Zeile bis zur echten Zeilengrenze verwerfen, bevor `line[]` verarbeitet wird:

```c
while(*p && *p != '\n' && li < (int)sizeof(line) - 1) { if(*p != '\r') line[li++] = *p; p++; }
if(*p && *p != '\n') {              // Zeile laenger als der Puffer: Rest dieser physischen
    while(*p && *p != '\n') p++;    // Zeile verwerfen statt ihn als eigene, unbeabsichtigte
}                                    // Kommandozeile zu parsen.
if(*p == '\n') p++;
line[li] = 0;
```

Die Verwerfen-Schleife hat keine eigene Längenbegrenzung und überspringt damit auch
Überschüsse, die ihrerseits mehrere Pufferbreiten lang sind, bis zur echten
Zeilengrenze (Host-Repro Szenario 3).

## Testhinweis (lokal)

1. Patch auf beide Repos anwenden (`git apply --check` vorab, dann real), `pio run`
   (bzw. Arduino-IDE-Build) für `heltec-ukfe-rx` und `lilygo-ukfe-rx` — reine
   Header-Änderung, kein neuer Include/keine neue Abhängigkeit.
2. Funktionstest auf Hardware: ein BadUSB-Skript mit einer `STRING`-Zeile deutlich über
   220 Zeichen senden (z. B. ein Fülltext, der irgendwo ein Wort wie "home" oder "tab"
   enthält) — vor dem Fix erscheint gelegentlich ein unerwarteter Cursor-Sprung/
   Tastendruck mitten im getippten Text, nach dem Fix wird der Text sauber (wenn auch
   bei >212 Zeichen abgeschnitten) ohne Fremdeingabe getippt.
3. Host-Repro (`repro-ukfe-ducky-overlong-line-reinterpret/repro.cpp`) deckt die reine
   Zeilen-Splitting-/Dispatch-Logik ab (kein USB-HID/Arduino nötig): Regression bei
   normalen Zeilen, Überlauf-Grenzfall mit exakt einer Pufferbreite, Überlauf über
   mehrere Pufferbreiten hinweg, überlaufende Zeile am Skriptende ohne `\n`, Zeilen
   exakt unterhalb der Schwelle unverändert.
