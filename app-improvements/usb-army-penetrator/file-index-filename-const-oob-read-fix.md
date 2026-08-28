# usb-army-penetrator — `#_FILE_INDEX_FILE_NAME_`: Out-of-Bounds-Vektorzugriff nach ungueltigem `$FILE_INDEX`

## Was
`src/Attacks/Ducky/Extensions.h`, `addDuckyScriptExtensions()`, registriert unter
anderem die dynamische Konstante fuer `#_FILE_INDEX_FILE_NAME_`:

```cpp
consts.emplace_back([]
{
    if (curListOfFiles.size() != 0)
    {
        return std::pair(Constant_FileIndexFileName, curListOfFiles[curFileIndexVariable]);
    }
    else
    {
        return std::pair(Constant_FileIndexFileName, std::string());
    }
});
```

Diese Closure prueft nur, ob `curListOfFiles` (die von `LOAD_FILES_FROM_SD()`/
`LOAD_DS_FILES_FROM_SD()` gefuellte Dateiliste) nicht leer ist — sie prueft nicht,
ob `curFileIndexVariable` selbst innerhalb der Liste liegt. `curFileIndexVariable`
wird ausschliesslich in derselben Datei gesetzt, im `FILE_INDEX_VALID()`-Zweig von
`handleFileIndex()`:

```cpp
curFileIndexVariable = variables.at(varName); // $FILE_INDEX, beliebiger int-Wert
if (curFileIndexVariable >= curListOfFiles.size())
{
    ret = false;
}
else
{
    ret = true;
}
```

`$FILE_INDEX` ist eine normale DuckyScript-Skriptvariable
(`std::unordered_map<std::string, int>` in `DuckyParse.h`, keine Wertebeschraenkung).
`FILE_INDEX_VALID()` meldet einen ausserhalb liegenden Wert korrekt als `false` —
setzt `curFileIndexVariable` danach aber **nicht** zurueck. Es bleibt auf dem
ungueltigen Wert stehen.

Der eigentliche Fehler liegt darin, wann diese Closure aufgerufen wird: im
DuckyScript-Interpreter (`DuckScriptInterpreter`, `DuckyParse.cpp`,
`DuckyInterpreter::getLineAndProcess()`, Zeile ~1321) laeuft **jede** registrierte
`UserDefinedConstants`-Funktion bei **jeder** Skriptzeile, unabhaengig davon, ob die
Zeile `#_FILE_INDEX_FILE_NAME_` ueberhaupt referenziert:

```cpp
for (const auto &userDefinedFunction : userDefinedConstValues)
{
    const auto &constant = userDefinedFunction();
    line = replaceAllOccurrences(line, constant.first, constant.second);
}
```

Sobald `curFileIndexVariable` einmal ausserhalb von `curListOfFiles` steht, greift
`curListOfFiles[curFileIndexVariable]` (`std::vector::operator[]`, keine
Bounds-Pruefung, undefiniertes Verhalten bei ungueltigem Index) bei **jeder**
weiteren Zeile des laufenden Skripts daneben — nicht nur bei der einen Zeile, die
den ungueltigen Index gesetzt hat.

Das ist kein konstruierter Randfall, sondern der garantierte Endzustand des in
diesem Projekt selbst vorgesehenen Iterationsmusters: `LOAD_DS_FILES_FROM_SD()`
findet z. B. drei Dateien, ein Skript zaehlt `$FILE_INDEX` hoch und prueft nach
jedem Schritt `FILE_INDEX_VALID()`, um die Schleife zu verlassen. Der Abbruch
passiert regulaer bei `$FILE_INDEX == 3` (Listengroesse, außerhalb) — genau der
Wert, mit dem `curFileIndexVariable` danach stehen bleibt. Ein negativer Wert
(z. B. eine Dekrement-Schleife ohne Untergrenzenpruefung, oder direkt
`VAR $FILE_INDEX = -1`) ist ebenso moeglich; bei `operator[]` wird `-1` in einen
sehr grossen `size_t`-Wert konvertiert, also ein Zugriff weit ausserhalb des
Vektors.

## Nutzen
- Schliesst einen echten Speicherzugriffsfehler, keinen reinen Logikfehler:
  `std::vector::operator[]` mit ungueltigem Index ist laut Standard undefiniertes
  Verhalten — je nach Speicherlayout ein Absturz (Denial-of-Service mitten im
  Skriptlauf, z. B. waehrend eines laufenden `RUN_PAYLOAD`/HID-Angriffs) oder ein
  stiller Zugriff auf Heap-Speicher jenseits des `curListOfFiles`-Vektors.
- Im Absturz-freien Fall wird der aus dem Nachbarspeicher gelesene Byte-Inhalt als
  `std::string` interpretiert und ueber die normale Konstantenersetzung in JEDE
  nachfolgende Skriptzeile eingesetzt, die zufaellig `#_FILE_INDEX_FILE_NAME_`
  enthaelt — z. B. in eine `STRING`/`LOG`-Zeile. Damit koennte Heap-Speicherinhalt
  (Adressen, Fragmente anderer Strings) unbeabsichtigt getippt oder geloggt werden —
  ein Speicher-Leck, kein reiner Absturz.
- Betrifft direkt das dokumentierte `LOAD_DS_FILES_FROM_SD()`/`FILE_INDEX_VALID()`-
  Iterationsmuster dieser App selbst, nicht nur fehlerhafte Fremdskripte: der
  Bug wird beim ordnungsgemaessen Verlassen einer solchen Schleife ausgeloest, wie
  oben gezeigt.
- Der Fix ist eine reine Verschaerfung der bestehenden Pruefung an genau der einen
  Stelle, die den Vektor tatsaechlich indiziert — keine Aenderung an
  `handleFileIndex()`/`FILE_INDEX_VALID()` noetig, deren Rueckgabewert bereits
  korrekt ist.

## Betroffene Dateien
- `src/Attacks/Ducky/Extensions.h` — die `consts.emplace_back(...)`-Closure fuer
  `Constant_FileIndexFileName` (`#_FILE_INDEX_FILE_NAME_`) in
  `addDuckyScriptExtensions()`.

Keine Aenderung an `handleFileIndex()`, `curFileIndexVariable`s Zuweisung oder an
`DuckyPayload.cpp`/dem `DuckScriptInterpreter`-Bibliothekscode noetig.

## Patch
```cpp
consts.emplace_back([]
{
    // curFileIndexVariable stammt aus der Skriptvariable $FILE_INDEX (beliebiger
    // int-Wert) und bleibt nach einem als ungueltig erkannten FILE_INDEX_VALID()
    // auf diesem Wert stehen -- "Liste nicht leer" allein reicht hier nicht,
    // der Index selbst muss innerhalb der Liste liegen (sonst operator[] = UB).
    if (curFileIndexVariable >= 0 && curFileIndexVariable < (int)curListOfFiles.size())
    {
        return std::pair(Constant_FileIndexFileName, curListOfFiles[curFileIndexVariable]);
    }
    else
    {
        return std::pair(Constant_FileIndexFileName, std::string());
    }
});
```

Einzige Aenderung: die Bedingung `curListOfFiles.size() != 0` wird durch eine
Pruefung ersetzt, die zusaetzlich `curFileIndexVariable` selbst gegen die
tatsaechliche Groesse der Liste (und gegen negative Werte) absichert.

## Testhinweis (ohne Hardware)
Reine Zustands-/Indexlogik (kein Hardware-, USB- oder SD-Zugriff), host-kompilierbar
nachvollziehbar. Reproduktionsprogramm
(`g++ -std=c++17 -Wall -Wextra repro.cpp -o repro && ./repro`) in
`repro-file-index-oob/repro.cpp`, das `FILE_INDEX_VALID()` und beide Fassungen der
Konstanten-Closure 1:1 nachbaut und drei Faelle prueft:

1. Schleifenende (`$FILE_INDEX` erreicht Listengroesse, der reguläre
   Abbruchfall von `LOAD_DS_FILES_FROM_SD()`-Iterationsschleifen).
2. Negativer Index (`$FILE_INDEX = -1`).
3. Kontrollfall mit gueltigem Index (Regressionscheck: unveraendertes Verhalten).

Da `std::vector::operator[]` mit ungueltigem Index echtes UB ist (nicht sicher
reproduzierbar), verwendet das Repro an der ALT-Stelle `.at()` statt `operator[]`
und faengt die daraus resultierende `std::out_of_range`-Exception als Nachweis der
versuchten Out-of-Bounds-Zugriffsstelle ab; im echten Code steht dort `operator[]`,
das denselben Index ohne jede Pruefung akzeptieren wuerde. Erwartete Ausgabe: in
Fall 1 und 2 zeigt ALT den abgefangenen Out-of-Bounds-Zugriffsversuch, NEU liefert
sauber einen leeren String; in Fall 3 liefern beide Fassungen identisch `"b.ds"`.

Funktionaler Test danach lokal auf dem Geraet: ein `.ds`-Skript mit
`LOAD_DS_FILES_FROM_SD()`, einer `$FILE_INDEX`-Zaehlschleife bis
`FILE_INDEX_VALID()` `false` liefert, gefolgt von mindestens einer weiteren Zeile,
die `LOG #_FILE_INDEX_FILE_NAME_` enthaelt — vor dem Fix riskiert diese Zeile
Absturz/Garbage-Ausgabe, danach liefert sie zuverlaessig einen leeren String.

## Risiko
- Sehr niedrig: verschaerft nur eine bereits vorhandene Pruefung um die tatsaechlich
  fehlende Indexgrenze, keine Aenderung an Kontrollfluss, Dateiliste oder der
  ausserhalb liegenden `FILE_INDEX_VALID()`-Logik.
- Fuer den bisher einzig als korrekt getesteten Fall (Index innerhalb der Liste)
  identisches Verhalten vorher/nachher (Fall 3 im Repro).
- Kein zusaetzliches Sicherheits-/Missbrauchsrisiko; der Fix reduziert eines
  (Heap-Speicher-Leck ueber eine Skript-Konstante).

## Quelle
- `G4MEOVER18/usb-army-penetrator` (main, `411b153`):
  `src/Attacks/Ducky/Extensions.h` (`addDuckyScriptExtensions`,
  `handleFileIndex`).
- `i-am-shodan/DuckScriptInterpreter` (Commit `23aa3ba`, per `platformio.ini`
  `lib_deps_core` gepinnt): `src/DuckyParse.cpp`,
  `DuckyInterpreter::getLineAndProcess()` — Aufrufstelle, die pro Zeile
  bedingungslos ueber alle `UserDefinedConstants`-Closures iteriert; sowie
  `DuckyParse.h` (`UserDefinedConstants`-Typdefinition,
  `_variables`-Typ `std::unordered_map<std::string, int>`).
