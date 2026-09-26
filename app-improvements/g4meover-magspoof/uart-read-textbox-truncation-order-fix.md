# g4meover-magspoof — UART-Lese-Textbox: Kappung rechnet mit der falschen Puffergröße

## Was
Bislang nur `mag_device_load_data()`/`mag_device_parse_card_string()`/`mag_encode()` sowie
`mag_state_load()` geprüft; diese Sitzung den bislang noch nicht angesehenen UART-Kartenleser-
Pfad selbst durchgesehen: `uart_worker()` in `scenes/mag_scene_read.c`, der Hintergrund-Thread,
der die rohe Antwort eines an UART angeschlossenen Kartenlesers (MSR-Reader, siehe
„UART MSR:"-Option in `mag_scene_settings.c`) byteweise einliest und in
`mag->uart_text_box_store` (einer `FuriString`, angezeigt im „READ"-Widget) sammelt, bis ich auf
„Parse" drücke.

Fund: die eigene Rolling-Kappung dieses Puffers rechnet mit der falschen Größe.

```c
if(len > 0) {
    // If text box store gets too big, then truncate it
    mag->uart_text_box_store_strlen += len;                      // (1) VOR der Kappungs-
                                                                   //     entscheidung UND VOR
                                                                   //     dem eigentlichen Anhängen
                                                                   //     hochgezählt

    if(mag->uart_text_box_store_strlen >= UART_TERMINAL_TEXT_BOX_STORE_SIZE - 1) {
        furi_string_right(
            mag->uart_text_box_store, mag->uart_text_box_store_strlen / 2);  // (2) Kappungs-Index
        mag->uart_text_box_store_strlen =
            furi_string_size(mag->uart_text_box_store) + len;
    }

    mag->uart_rx_buf[len] = '\0';
    furi_string_cat_printf(mag->uart_text_box_store, "%s", mag->uart_rx_buf);  // (3) erst HIER angehängt
}
```

`mag->uart_text_box_store_strlen` wird in Zeile (1) bereits um die Länge des **gerade erst
eingetroffenen** UART-Häppchens (`len`, bis zu `UART_RX_BUF_SIZE` = 320 Byte pro Empfang) erhöht
— aber der eigentliche `FuriString`-Puffer bekommt diesen Häppchen erst in Zeile (3) angehängt,
also **nachdem** die Kappung in Zeile (2) bereits mit dem hochgezählten Wert gerechnet hat.
`furi_string_right(v, index)` ist laut `Next-Flip/Momentum-Firmware`, `furi/core/string.c`
(`void furi_string_right(FuriString* v, size_t index) { string_right(v->string, index); }`) ein
reiner 1:1-Wrapper um mlibs `m_string_right()` (`P-p-H-d/mlib`, `m-string.h`):

```c
M_INLINE void
m_string_right(m_string_t v, size_t index)
{
  const size_t size = m_string_size(v);
  if (index >= size) {           // <-- Index >= tatsächliche Größe: gesamter String wird geleert
    ptr[0] = 0;
    m_str1ng_set_size(v, 0);
    return;
  }
  size_t s2 = size - index;      // sonst: Suffix ab index behalten
  memmove(&ptr[0], &ptr[index], s2 + 1);
  m_str1ng_set_size(v, s2);
}
```

Der an `furi_string_right()` übergebene Index (`mag->uart_text_box_store_strlen / 2`, also die
Hälfte der **projizierten** Größe *nach* dem noch ausstehenden Anhängen) ist damit systematisch
zu groß gegenüber der Hälfte der Größe, die der `FuriString` zum Zeitpunkt des Aufrufs
**tatsächlich** hat — der Puffer wird bei jeder ausgelösten Kappung stärker gekürzt, als
beabsichtigt: statt „behalte die letzte Hälfte des bisher gesammelten Texts" wird effektiv nur
`(bisherige_Größe − len) / 2` Byte des bereits gelesenen Kartentexts behalten, also bis zu
`len / 2` (bis zu 160 Byte pro Auslösung) mehr an bereits erfasstem Karten-Rohtext verworfen, als
das Kommentar über der Zeile verspricht — lautlos, ohne Fehlermeldung, während ich noch auf
„Parse" warte.

Mit den tatsächlichen Puffergrößen dieser App (`UART_TERMINAL_TEXT_BOX_STORE_SIZE` = 4096,
`UART_RX_BUF_SIZE` = 320) bleibt der `index >= size`-Zweig von `m_string_right()` unerreichbar,
weil die Kappung erst auslöst, wenn der Puffer bereits nahe an 4096 Byte ist — immer weit mehr
als die maximal 320 Byte eines einzelnen UART-Häppchens; das ist also (bei mir, mit diesen
Konstanten) „nur" verstärkte, aber begrenzte Datenverluste, kein Leeren des gesamten Puffers.
Das zugrunde liegende Muster selbst ist aber unabhängig von diesen konkreten Zahlenwerten
fehlerhaft: sobald ein UART-Häppchen mindestens so groß ist wie der bisher gesammelte Text
(`len >= bisherige Größe`), würde derselbe Code den **kompletten** bislang gesammelten
Karten-Rohtext auf einen Schlag verwerfen, ohne dass ich das bemerke — mit Host-Repro anhand
eines künstlich verkleinerten Speichers demonstriert (siehe Testhinweis).

## Nutzen
- Die Rolling-Kappung des UART-Lesepuffers macht künftig genau das, was der eigene Kommentar
  verspricht: die letzte Hälfte des **tatsächlich bereits gesammelten** Kartentexts bleibt
  erhalten, unabhängig davon, wie groß der gerade eintreffende UART-Häppchen ist.
- Kein stiller Verlust von zusätzlichem, bereits erfasstem Karten-Rohtext bei einer langen
  Kartenleser-Sitzung (z. B. mehrere Swipes/Antworten hintereinander, bevor ich auf „Parse"
  drücke) mehr — relevant bei einem autorisierten Pentest eines UART-MSR-Readers, wo genau
  dieser Rohtext später mit „Parse" (`mag_device_parse_card_string()`) ausgewertet wird.
- Minimal-invasiv: reine Umstellung der Reihenfolge/Eingabewerte innerhalb desselben
  `if(len > 0)`-Blocks, keine neue Variable, kein geänderter Datentyp, keine andere Aufrufstelle
  betroffen.
- Unterhalb der Kappungsschwelle (der ganz überwiegende Alltagsfall — ein normaler Kartenlese-
  Versuch bleibt weit unter 4096 Byte) unverändertes Verhalten, durch Host-Repro-Regressionsfall
  bestätigt.

## Betroffene Dateien (lokal, in meiner MagSpoof-App)
- `scenes/mag_scene_read.c` — `uart_worker()`, `WorkerEvtRxDone`-Zweig: Kappungs-Entscheidung
  prüft jetzt `mag->uart_text_box_store_strlen + len` (die *unveränderte* Bedingung, nur ohne
  die vorzeitige Erhöhung), der an `furi_string_right()` übergebene Kappungs-Index bleibt
  `mag->uart_text_box_store_strlen / 2` — aber jetzt mit dem Wert, den der Puffer zu diesem
  Zeitpunkt tatsächlich hat, nicht mit dem bereits um `len` erhöhten. Die anschließende
  Neusynchronisation (`mag->uart_text_box_store_strlen = furi_string_size(...) + len`) läuft
  unverändert immer, nicht mehr nur innerhalb des Kappungs-Zweigs — notwendig, da sie vorher
  implizit von der vorzeitigen Erhöhung mitgetragen wurde.

Keine Änderung an `UART_RX_BUF_SIZE`/`UART_TERMINAL_TEXT_BOX_STORE_SIZE`, am UART-Worker-Thread
selbst, an `mag_device_parse_card_string()` oder an einer anderen Datei nötig.

## Patch
Siehe `app-improvements/g4meover-magspoof/uart-read-textbox-truncation-order-fix/mag_scene_read_c.patch`
(Unified-Diff gegen die unveränderte `zacharyweiss/magspoof_flipper`-Basis, `main`, Commit
`9e782404`).

## Testhinweis (ohne Hardware)
Eigenständiges, hostseitiges Reproduktionsprogramm der reinen Puffer-Größenarithmetik in
`app-improvements/g4meover-magspoof/repro-uart-truncation-order-fix/repro.c` — bildet exakt
zwei Dinge 1:1 aus den jeweiligen Originalquellen nach, ohne Furi-/Flipper-Abhängigkeit:
1. `m_string_right()` (`P-p-H-d/mlib`, `m-string.h`): Index `>= size` leert den ganzen String,
   sonst wird das Suffix ab `index` behalten — wortgetreu aus der Bibliotheksquelle übernommen.
2. `uart_worker()`s Kappungsblock, einmal als ALT (wie ausgeliefert) und einmal als NEU (dieser
   Fix), beide auf demselben Mock-`FuriString` operierend — der einzige Unterschied zwischen
   ALT und NEU ist exakt die Code-Umstellung aus dem Patch oben.

```sh
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O0 -o repro_O0 repro.c && ./repro_O0
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -O1 -o repro_O1 repro.c && ./repro_O1
```

Drei Testgruppen, mit den echten Konstanten dieser App (`UART_TERMINAL_TEXT_BOX_STORE_SIZE` =
4096, `UART_RX_BUF_SIZE` = 320) sowie einem verkleinerten Modellspeicher für den sonst nicht
erreichbaren Totalverlust-Fall:
1. **Echte Konstanten, vollständiger Auslösebereich** (jede `(bisherige_Größe, len)`-Kombination,
   die mit den echten Puffergrößen tatsächlich eine Kappung auslösen kann): 51360 ausgelöste
   Kappungen durchgerechnet. NEU behält in jedem Fall exakt `bisherige_Größe / 2` Byte des
   bereits gesammelten Texts (und bleibt mit der eigenen Buchhaltungsvariable
   `uart_text_box_store_strlen` synchron); ALT behält in jedem einzelnen Fall **weniger** —
   in Summe über den gesamten Bereich 5.486.880 Byte an bereits gesammeltem Kartentext mehr
   verworfen als NEU, bei identischen Eingaben.
2. **Verkleinerter Modellspeicher, `len >= bisherige_Größe`-Bereich** (mit den echten
   Konstanten dieser App nicht erreichbar, da `UART_RX_BUF_SIZE` = 320 immer deutlich kleiner
   ist als der Speicherstand, ab dem überhaupt gekappt wird — zeigt aber, dass das Muster selbst
   unabhängig von den konkreten Zahlenwerten fehlerhaft ist): 1054 Fälle, in denen ALT den
   **gesamten** bisher gesammelten Text auf einen Schlag verwirft (0 Byte behalten), während
   NEU weiterhin korrekt die Hälfte behält.
3. **Unterhalb der Kappungsschwelle:** keine Kappung bei ALT oder NEU, Ergebnis in beiden Fällen
   identisch — Regressionsfall für den normalen Lesealltag.

Bei mir lokal reproduziert, `-O0` und `-O1`, je 260165 Prüfungen: `ALL CHECKS PASSED`, keine
ASan-/UBSan-Meldungen, keine Compiler-Warnungen.

Funktionaler Test nach dem Patch, lokal auf dem Flipper (UART-MSR-Reader-Hardware nötig, um den
Pfad überhaupt anzusteuern):
1. `ufbt` bauen, App starten, „UART MSR:" in den Einstellungen aktivieren (Bestätigungsdialog
   bejahen), zur „Read"-Szene wechseln.
2. Einen angeschlossenen UART-MSR-Reader über eine sehr lange Sitzung hinweg wiederholt Karten
   lesen lassen (oder testweise am Reader-seitigen UART wiederholt Daten senden), bis der
   Textbox-Inhalt mehrfach die interne Kappungsschwelle erreicht.
3. Regressionsprüfung: „Clear"/„Parse" verhalten sich wie zuvor, ein normaler einzelner
   Kartenlese-Versuch (weit unterhalb von 4096 Byte) zeigt keinerlei sichtbaren Unterschied.
4. Der eigentliche Nachweis der Fund-Größenordnung ist der Host-Repro oben — am Gerät selbst ist
   nur durch sehr lange UART-Sitzungen und exaktes Auszählen der empfangenen vs. angezeigten
   Bytes überhaupt beobachtbar, dass vorher mehr verworfen wurde als beabsichtigt.

## Risiko
- Niedrig: Änderung betrifft ausschließlich die Reihenfolge/den Eingabewert innerhalb eines
  einzelnen `if(len > 0)`-Blocks in `uart_worker()`, keine neue Variable, kein geänderter
  Datentyp, keine andere Aufrufstelle.
- Verhalten unterhalb der Kappungsschwelle (der weit überwiegende Alltagsfall) bleibt
  unverändert — durch den Host-Repro-Regressionsfall bestätigt.
- Der mit den echten Konstanten dieser App nicht erreichbare Totalverlust-Fall
  (`len >= bisherige Größe`) wird durch den Fix ebenfalls behoben (siehe Host-Repro-Gruppe 2),
  ohne dass das dafür zusätzliche Risiko birgt — derselbe, jetzt korrekt sortierte Code deckt
  beide Fälle ab.
- Kein Einfluss auf `mag_device_parse_card_string()` selbst (unverändert, bereits vorher
  ungeprüft und ohne Fund) oder auf das `.mag`-Dateiformat.

## Quelle
- `zacharyweiss/magspoof_flipper` (main, `9e782404`): `scenes/mag_scene_read.c` (`uart_worker()`,
  `UART_RX_BUF_SIZE`/`UART_TERMINAL_TEXT_BOX_STORE_SIZE` in `scenes/mag_scene_read.h`).
- `Next-Flip/Momentum-Firmware` (`mntm-012`-Zweig, aktueller Stand): `furi/core/string.c`/`.h`,
  `furi_string_right()` — bestätigt den reinen 1:1-Wrapper um `string_right()`.
- `P-p-H-d/mlib`, `m-string.h`, `m_string_right()` — Referenzsemantik für den Kappungs-Aufruf
  (Index `>= size` leert den String vollständig, sonst Suffix-Erhalt).
