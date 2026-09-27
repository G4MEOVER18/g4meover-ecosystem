# g4meover-magspoof — `helpers/mag_text_input.c`/`.h` als totes Gewicht entfernen

## Was
Bei der vollständigen Durchsicht dieser App hatte ich `helpers/mag_text_input.c`/`.h` (582 bzw.
83 Zeilen, ein eigenständiges Tastatur-Widget `Mag_TextInput` samt eigenem View/Draw-/Input-
Callback und eigenem Timer für Shift-Wiederholung) bereits als „nirgends im Baum aufgerufen"
vorgemerkt, ohne es abschließend zu klären. Das jetzt vollständig nachgeprüft:

- Jede Funktion, die `mag_text_input.h` deklariert (`mag_text_input_alloc()`,
  `mag_text_input_free()`, `mag_text_input_reset()`, `mag_text_input_get_view()`,
  `mag_text_input_set_result_callback()`, `mag_text_input_set_header_text()`) wird außerhalb von
  `mag_text_input.c` selbst kein einziges Mal aufgerufen — keine Scene, keine `mag.c`, kein
  `mag_i.h`.
- Der von `mag_text_input.c` definierte Typ `Mag_TextInput`/`Mag_TextInputModel` taucht in keiner
  anderen Datei des Baums auf (`mag_i.h`, `mag.c`, alle `scenes/*.c`, `helpers/mag_helpers.c`
  durchsucht).
- Die einzige oberflächliche Namensähnlichkeit ist `mag_text_input_callback()` (deklariert in
  `mag_i.h`, definiert in `mag.c`, verwendet von `mag_scene_input_name.c`/
  `mag_scene_input_value.c`) — das ist aber eine andere Funktion für den **Standard**-`TextInput`
  aus `gui/modules/text_input.h`, den diese beiden Szenen tatsächlich verwenden
  (`text_input_set_result_callback(text_input, mag_text_input_callback, ...)`). Sie hat mit dem
  eigenen `Mag_TextInput`-Widget nichts zu tun und ruft keine seiner Funktionen auf.
- `application.fam` listet keine explizite Quelldateiliste (kein `sources=[...]`), `fbt`/`ufbt`
  bindet App-Quelldateien standardmäßig per Verzeichnis-Glob ein — beide Dateien werden also bei
  jedem Build mitkompiliert und mitgelinkt, obwohl zur Laufzeit nichts sie je instanziiert.

Ergebnis: totes Gewicht im Build (Flash-Platz, Kompilierzeit), aber kein Laufzeitfehler, da der
Code schlicht nie erreicht wird. Meine App nutzt für Texteingabe durchgehend den Standard-
`TextInput`-Modul, das eigene Widget ist ein nie fertiggestellter, verwaister Anlauf.

## Nutzen
- Reduziert die kompilierte App um zwei vollständig ungenutzte Dateien (665 Zeilen) — weniger
  Flash-Verbrauch, kürzere Baumzeit, kein toter Code, der bei künftigen Durchsichten fälschlich
  für aktiven Code gehalten und mitprüft werden muss.
- Kein Verhaltensrisiko: da nichts im Baum diese Funktionen je aufruft, ändert das Entfernen
  nichts an der Laufzeit-Funktionalität der App — rein subtraktiv.
- Räumt einen bereits in einer früheren Durchsicht vorgemerkten offenen Punkt ab, statt ihn
  unbegrenzt weiterzuschieben.

## Betroffene Dateien (lokal, in meiner MagSpoof-App)
- `helpers/mag_text_input.c` — vollständig entfernt.
- `helpers/mag_text_input.h` — vollständig entfernt.

Keine andere Datei muss geändert werden: da keine von ihnen `mag_text_input.h` inkludiert oder
eine ihrer Funktionen aufruft, entstehen durch das Entfernen keine offenen Symbole.

## Patch
Siehe
`app-improvements/g4meover-magspoof/mag-text-input-dead-code-removal/mag_text_input_removal.patch`
(Unified-Diff gegen die unveränderte `zacharyweiss/magspoof_flipper`-Basis, `main`, Commit
`9e782404` — derselbe Stand, den auch die bisherigen `g4meover-magspoof`-Funde zitieren. Löscht
schlicht beide Dateien. `git apply --check` vorab gegen eine frische Kopie verifiziert, real
angewendet, Ergebnis danach byte-für-byte mit einer unabhängig per `git rm` erzeugten
Referenzkopie abgeglichen — identisch).

## Testhinweis (ohne Hardware)
Reine Baum-/Referenz-Prüfung, kein eigenständiges Repro-Programm nötig, da es sich um eine
Löschung ungenutzten Codes handelt, nicht um eine Logikänderung:

1. `grep -rn "mag_text_input_alloc\|mag_text_input_free\|mag_text_input_reset\|mag_text_input_get_view\|mag_text_input_set_result_callback\|mag_text_input_set_header_text\|Mag_TextInput"` über den gesamten App-Baum außerhalb von `helpers/mag_text_input.c`/`.h` selbst liefert **keinen** Treffer.
2. `application.fam` referenziert keine explizite Quelldateiliste, in der die beiden Dateien
   einzeln ausgetragen werden müssten.

Funktionaler Test nach dem Patch, lokal auf dem Flipper:
1. `ufbt` bauen — muss unverändert erfolgreich durchlaufen (kein Linker-Fehler durch fehlende
   Symbole, da nichts sie referenziert).
2. App starten, alle Szenen wie gewohnt durchgehen (insbesondere `Input Name`/`Input Value`, die
   den Standard-`TextInput` nutzen) — Verhalten unverändert, da diese beiden Szenen nie das
   entfernte Widget verwendet haben.

## Risiko
- Sehr niedrig: reine Löschung zweier vollständig isolierter, nie referenzierter Dateien. Kein
  bestehender Aufrufer, kein geteilter Zustand, keine geänderte Signatur an irgendeiner anderen
  Stelle.
- Einziges denkbares Risiko wäre ein bislang übersehener Aufrufer außerhalb der durchsuchten
  Dateitypen (`*.c`/`*.h`/`application.fam`) — dagegen spricht, dass dies eine reine C-App ohne
  weitere Quellsprachen ist und der Build ausschließlich über `fbt`/`ufbt` läuft.

## Quelle
- `zacharyweiss/magspoof_flipper` (main, `9e782404`): `helpers/mag_text_input.c`/`.h`.
