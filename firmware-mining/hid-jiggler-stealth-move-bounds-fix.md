# HID Mouse Jiggler (Stealth): Bewegungswert wird ungeprüft in ein zu enges int8_t genarrowt

## Was
Ich habe aus der bei der letzten Sitzung aus `unlshd-093` vorgemerkten Restliste den nächsten
Kandidaten geprüft: `c742dfe6c` (PR #1111, "HID: bound stealth jiggler movement"), betrifft
`hid_mouse_jiggler_stealth_timer_callback()` in
`applications/system/hid_app/views/hid_mouse_jiggler_stealth.c`. Die relevanten Zeilen bei mir
(`mntm-012`) sind byteidentisch zum Unleashed-Vorher-Stand (die umgebende Datei weicht an
anderen, für diesen Fund irrelevanten Stellen ab — ältere UI-Feinheiten wie
Pfeil-Icon-Hinweise/benannte Intervall-Konstanten, die bei mir noch fehlen; direkt gegen den
unmittelbaren Vorgänger-Commit `c742dfe6c^` diffgeprüft und die drei betroffenen Zeilen
gesondert byteweise verglichen: identisch).

## Fund: `int`-Bewegungswert wird beim Aufruf still auf `int8_t` gekappt statt vorher begrenzt
```c
// Randomize the mouse movement distance and direction
int move_x = (rand() % 2001) - 1000; // Randomly between -1000 and 1000
int move_y = (rand() % 2001) - 1000; // Randomly between -1000 and 1000

// Perform the mouse move with the randomized values
hid_hal_mouse_move(hid_mouse_jiggler->hid, move_x, move_y);
```
Die reale Zielfunktion ist in `applications/system/hid_app/hid.h` deklariert als
```c
void hid_hal_mouse_move(Hid* instance, int8_t dx, int8_t dy);
```
— HID-Maus-Reports übertragen die Bewegungsdifferenz laut Protokoll ohnehin nur als signed
8-Bit-Wert (`-128`..`127`). `move_x`/`move_y` werden aber als `int` im Bereich `-1000`..`1000`
berechnet und beim Funktionsaufruf implizit auf `int8_t` konvertiert — eine Wertebereichs-
verletzende Konvertierung, deren Ergebnis laut C-Standard (C11 6.3.1.3p3) entweder
implementierungsdefiniert ist oder ein implementierungsdefiniertes Signal auslöst; auf dem
tatsächlichen GCC/ARM-Toolchain-Pfad läuft das in der Praxis auf eine reine Bit-Reinterpretation
der unteren 8 Bit hinaus (kein Absturz, kein Diagnostic) — der am Gerät tatsächlich gesendete
Bewegungswert steht dann in keinem erkennbaren zahlenmäßigen Verhältnis mehr zum "beabsichtigten"
Wert. Mit dem Host-Repro quantifiziert: über den vollständigen Eingabebereich von `move_x`
(jeder Wert, den `(rand() % 2001) - 1000` liefern kann) weichen **1745 von 2001 möglichen
Zügen (87,2 %)** vom beabsichtigten Wert ab — das ist der Regelfall, kein seltener Randfall. Ein
paar konkrete, mit dem Repro nachgewiesene Beispiele: beabsichtigt `+1000` kommt tatsächlich als
`-24` an, beabsichtigt `-1000` als `+24`, beabsichtigt `+900` (eine für eine "Stealth"-Jiggler-
Bewegung ohnehin schon fragwürdig große, aber plausibel wirkende Distanz) als `-124` — die
Kappung kann also sowohl das Vorzeichen kippen als auch die Größenordnung beliebig verfälschen,
ohne dass App oder Nutzer davon je etwas mitbekommen.

**Praktische Auswirkung:** Kein Speicherfehler (der `int8_t`-Parameter selbst ist über seinen
vollen Wertebereich hinweg gültig, es wird nichts Ungültiges gelesen/geschrieben) — der Fund ist
rein funktional. Der Zweck der "Stealth"-Variante des Mouse Jigglers ist gerade eine unauffällige,
kontrollierte Mausbewegung; durch die Kappung kann die tatsächlich gesendete Bewegung aber jeden
Wert im vollen `int8_t`-Bereich annehmen, unabhängig davon, was der Code "beabsichtigt" hat — bis
hin zu maximalen Ausschlägen (`-128`/`127` je Achse bei jedem Timer-Tick), was dem eigentlichen
Stealth-Zweck der View direkt zuwiderläuft. Erreichbar bei jeder Nutzung der Funktion, kein
externer Trigger nötig (reiner internen Logikfehler, kein Angriffsvektor auf mein eigenes Gerät).

## Host-Repro
Eigenständiges Host-Repro (keine Flipper-/Hardware-Abhängigkeiten), reproduziert die exakten
ALT-/NEU-Ausdrücke aus den beiden Quellzuständen sowie die reale `hid_hal_mouse_move(Hid*,
int8_t, int8_t)`-Parametersignatur (die Kappung passiert exakt an dieser Aufrufgrenze, dieselbe
Konvertierung wie im echten Firmware-Build):
```
gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```
Tatsächliche Ausgabe (lokal ausgeführt):
```
[group1] ALT: 1745/2001 draws (87.2%) deliver a movement value with no numeric relationship to the intended -1000..1000 delta
[group2] ALT examples: intended +1000 -> actual -24, intended -1000 -> actual 24, intended +900 -> actual -124 (all silently aliased, no crash/diagnostic)
[group3] NEU: 0/255 draws (0%) mismatch -- every possible draw is delivered exactly as intended
[group4] NEU bounds: min=-127 max=127 (both within int8_t, symmetric around 0)
ALL CHECKS PASSED (267 checks)
```
Gruppe 1 überstreicht den vollständigen Eingabebereich von ALT (`raw_x` von 0 bis 2000, jeder
Wert, den `rand() % 2001` liefern kann) und vergleicht den beabsichtigten Wert gegen den nach der
`int8_t`-Konvertierung tatsächlich ankommenden — die Fehlquote von 87,2 % ist analytisch
hergeleitet (2001 mögliche Werte, davon nur 256 im vollen `int8_t`-Bereich `[-128,127]`
darstellbar) und per Test gegen die exakte Zahl abgesichert. Gruppe 2 zeigt konkrete, für einen
Leser nachvollziehbare Vorher/Nachher-Beispiele. Gruppe 3 überstreicht ebenso vollständig den
Eingabebereich von NEU (`raw_x` von 0 bis `2*INT8_MAX`) und bestätigt: **kein einziger** möglicher
Zug weicht dort vom beabsichtigten Wert ab — der Fix beseitigt den Fund vollständig, verkleinert
ihn nicht nur. Gruppe 4 bestätigt, dass NEUs Extremwerte exakt bei `±INT8_MAX` (`±127`) liegen,
symmetrisch um 0, nicht beim asymmetrischen `INT8_MIN` (`-128`). Unter
`gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined` und separat unter `-O0`
gegengeprüft: keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen, identisches Ergebnis.

## Betroffene Dateien
- `applications/system/hid_app/views/hid_mouse_jiggler_stealth.c` —
  `hid_mouse_jiggler_stealth_timer_callback()`, einzige Fix-Stelle (zwei Zeilen plus ein
  zusätzlicher `#include <stdint.h>` für `INT8_MAX`). Keine Signatur-/Aufrufstellenänderung,
  kein Einfluss auf die (bei mir aus einer älteren Unleashed-Zwischenversion stammenden,
  unabhängigen) restlichen Abweichungen derselben Datei.

## Risiko
Sehr gering. Reine Logikänderung ohne Speicherzugriffsimplikation, betrifft ausschließlich die
Berechnung zweier lokaler Werte unmittelbar vor ihrer Verwendung — keine geänderte Signatur, kein
neuer Fehlerpfad, keine Auswirkung auf andere Teile der Datei oder auf die (bei mir bereits ohne
diesen Fund vorhandene) abweichende UI-Logik. Für den bisherigen Fall ändert sich das äußere
Verhalten sichtbar (kleinere, tatsächlich wie beabsichtigt begrenzte Mausbewegungen statt
zufällig verfälschter Ausschläge) — das ist die beabsichtigte Korrektur, kein Nebeneffekt.

## Port-Skizze / Fix
```c
// HID mouse reports use signed 8-bit movement deltas
int8_t move_x = (rand() % (2 * INT8_MAX + 1)) - INT8_MAX;
int8_t move_y = (rand() % (2 * INT8_MAX + 1)) - INT8_MAX;

// Perform the mouse move with the randomized values
hid_hal_mouse_move(hid_mouse_jiggler->hid, move_x, move_y);
```
Plus `#include <stdint.h>` am Dateianfang (für `INT8_MAX`; die restlichen bereits vorhandenen
Includes werden dadurch nicht berührt). 1:1 Unleasheds Fix übernommen, kein eigener Ansatz nötig.
Unified-Diff gegen meine unveränderte `mntm-012`-Basis in
`firmware-mining/hid-jiggler-stealth-move-bounds-fix/hid_mouse_jiggler_stealth_c.patch`
(`patch --dry-run -p1` gegen eine frische Basiskopie vorab verifiziert — Hunk 2 wendet sich mit
einem reinen Zeilenversatz an, keinem inhaltlichen Fuzzy-Match, da die umgebende Datei bei mir an
anderen Stellen kürzer ist; real angewendet und Byte-für-Byte gegen die separat host-getestete
Fassung abgeglichen — identisch).

## Testhinweis (lokal nachzuvollziehen)
`firmware-mining/repro-hid-jiggler-stealth-move-bounds-fix/repro.c`, host-kompilierbar, keine
Flipper-/Hardware-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: Patch anwenden, `fbt` neu bauen und flashen,
danach die Stealth-Jiggler-View über USB-HID oder BLE-HID mit einem verbundenen Rechner starten
und über mehrere Timer-Intervalle hinweg beobachten — die Mausbewegungen müssen jetzt durchgehend
klein und im erwarteten Bereich bleiben (kein gelegentlicher großer/unvorhersehbarer Ausschlag
mehr, wie es vor dem Fix bei den meisten Zügen der Fall war). Kein spezielles Testgerät nötig, der
Effekt ist mit jedem HID-fähigen Host sichtbar; der (nicht-stealth) normale Mouse-Jiggler ist von
diesem Fund nicht betroffen (er nutzt eine feste, bereits im `int8_t`-Bereich liegende
Schrittweite, `MOUSE_MOVE_SHORT`, keine `rand()`-basierte Distanzberechnung).

## Quelle
- `github.com/DarkFlippers/unleashed-firmware`, Commit `c742dfe6c` (PR #1111, "HID: bound
  stealth jiggler movement"), Tag `unlshd-093`.
- Zum Vergleich herangezogen: `applications/system/hid_app/views/hid_mouse_jiggler.c` (normale,
  nicht-stealth Variante) — verwendet bereits eine feste `int8_t`-taugliche Schrittweite, vom
  selben Fund nicht betroffen.
- API-87-Einordnung: reine Anwendungsdatei (`applications/system/hid_app`), bei mir (`mntm-012`)
  in den relevanten Zeilen byteidentisch zum Unleashed-Vorher-Stand vorhanden — kein
  Kompatibilitätsrisiko, 1:1 portierbar.
- Aus derselben `unlshd-093`-Restliste weiterhin offen: `cc3a1bc79` (CUID-Dictionary-Zähler,
  #1115), `95c35fb82` (MIFARE-Classic-Schlüssel-Dictionary-Feature, #1118), `104d96f86`
  (Superrollo/GW60-SubGHz-Protokoll, #1068).
