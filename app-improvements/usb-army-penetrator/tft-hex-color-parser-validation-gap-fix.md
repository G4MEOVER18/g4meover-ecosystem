# usb-army-penetrator — TFT: Hex-Zweig von `convertStringToColor()` validiert nicht vollständig

## Was
`src/Devices/TFT/HardwareTFT.cpp`, `HardwareTFT::convertStringToColor()`. Der zuvor als
"unerreicht, noch nicht geprüft" vorgemerkte Rückstellpunkt zum TFT-Hex-Farbparser wurde
jetzt aufgegriffen: alle Aufrufstellen im gesamten Repo (`src/main.cpp`,
`src/Devices/Platform/ESP32BoardSupport.cpp`) übergeben ausschließlich fest verdrahtete
Namen ("RED", "GREEN", "WHITE" usw.), die über `colorLookup` (Named-Color-Tabelle direkt
darüber in derselben Datei) aufgelöst werden — der Hex-Zweig der Funktion ist aktuell
tatsächlich unerreicht, bestätigt diesen Teil des Rückstellpunkts. Trotzdem lohnt sich die
Prüfung: der Funktionskommentar ("Check if it's an RGB888 hex color code") sowie die
`0x`/`0X`-Präfixbehandlung zeigen eindeutig, dass ein zukünftiger Aufrufer (z. B. ein
Ducky-Extension-Kommando oder ein Web-UI-Farbfeld, beides Muster, die diese App an anderer
Stelle bereits für andere Einstellungen verwendet) genau diesen Pfad mit extern kontrolliertem
Text erreichen soll:

```cpp
int HardwareTFT::convertStringToColor(const std::string &color)
{
    // Check if it's a named color
    const auto &it = colorLookup.find(color);
    if (it != colorLookup.cend())
    {
        return it->second;
    }

    // Check if it's an RGB888 hex color code (e.g., "0xFFFFFF" or "FFFFFF")
    std::string hexStr = color;
    if (hexStr.length() >= 2 && hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X'))
    {
        hexStr = hexStr.substr(2);
    }

    char* end;
    errno = 0;
    const unsigned long value = std::strtoul(hexStr.c_str(), &end, 16);

    if (errno != 0 || end == hexStr.c_str()) {
        return TFT_BLACK;
    }

    return lcd.color24to16(value);
}
```

Die Fehlerprüfung deckt nur zwei Fälle ab: `errno != 0` (Überlauf von `unsigned long`, z. B.
eine sehr lange Ziffernfolge) und `end == hexStr.c_str()` (überhaupt keine gültige Hex-Ziffer
konsumiert, z. B. leerer String oder "ZZZ"). Drei andere Klassen ungültiger Eingaben schlüpfen
durch beide Prüfungen hindurch und werden als scheinbar gültige Farbe an
`lcd.color24to16()` weitergereicht statt — wie der Kommentar und der Rückgabewert `TFT_BLACK`
in den beiden abgedeckten Fällen erkennbar beabsichtigen — abgewiesen zu werden:

1. **Trailing-Müll wird ignoriert.** `std::strtoul()` stoppt beim ersten ungültigen Zeichen
   und schreibt dessen Position nach `end`, prüft aber nicht, ob `end` das Stringende
   erreicht hat. `"FF00FFxyz"` wird still als `0xFF00FF` interpretiert, der Rest `"xyz"`
   verschwindet kommentarlos.
2. **Ein führendes Vorzeichen wird akzeptiert.** `strtoul()` erlaubt laut C-Standard ein
   optionales `+`/`-` vor den Ziffern; bei `-` wird der geparste Wert negiert und in den
   vorzeichenlosen Rückgabetyp zurückgewickelt (Zweierkomplement-Verhalten). `"-FF"` liefert
   dadurch einen riesigen, mit der Eingabe in keinerlei sinnvollem Zusammenhang stehenden
   Wert statt eines Fehlers — für eine Funktion, deren Kommentar ausschließlich reine
   Hex-Ziffernfolgen beschreibt, ein unerwarteter Erfolgsfall.
3. **Werte über dem eigenen 24-Bit-RGB888-Format werden nicht begrenzt.** Weder der
   Kommentar noch der Funktionsname lassen einen Wert über `0xFFFFFF` zu, geprüft wird das
   aber nirgends — eine achtstellige Hex-Eingabe wie `"FFFFFFFF"` (passt auf jeder
   Zielarchitektur verlustfrei in `unsigned long`, löst also auch keinen `errno`-Überlauf
   aus) wird unverändert an `lcd.color24to16()` durchgereicht, deren Bit-Zuordnung (Teil von
   LovyanGFX, nicht dieses Repos) für Werte außerhalb des dokumentierten 24-Bit-Bereichs
   nicht spezifiziert ist.

## Nutzen
- Schließt die Lücke, bevor sie über einen künftigen Aufrufer (Ducky-Kommando, Web-UI,
  Settings-Datei) tatsächlich mit extern kontrolliertem Text erreichbar wird — genau der
  Zeitpunkt, an dem still falsch interpretierte statt sauber abgewiesene Eingaben zu einem
  echten Problem werden (unbeabsichtigte/unvorhersehbare Anzeigefarbe statt einer
  nachvollziehbaren Fehlerfarbe).
- Bringt den Fehlerpfad der Funktion in Einklang mit ihrer eigenen Kommentar-Beschreibung
  ("RGB888 hex color code") und ihrem eigenen Namen (`convertStringToColor`, kein
  `convertHexOrSignedIntegerStringToColor`).
- Sehr kleiner, lokaler Zusatz (eine Zeichenklassen-Prüfung vor `strtoul()`, eine
  Bereichsprüfung danach) ohne Änderung der Funktionssignatur oder der Named-Color-Logik
  davor.

## Betroffene Dateien
- `src/Devices/TFT/HardwareTFT.cpp` — `HardwareTFT::convertStringToColor()`, einziger
  Fund-/Fix-Ort. Aktuell ohne Aufrufer, die den Hex-Zweig überhaupt erreichen (siehe oben) —
  keine Verhaltensänderung an einer heute genutzten Aufrufstelle.

## Patch
```cpp
    // Check if it's an RGB888 hex color code (e.g., "0xFFFFFF" or "FFFFFF")
    std::string hexStr = color;
    if (hexStr.length() >= 2 && hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X'))
    {
        hexStr = hexStr.substr(2);
    }

    // Nur reine Hex-Ziffern zulassen -- lehnt leere Eingaben, ein fuehrendes '+'/'-'
    // (von strtoul() sonst stillschweigend akzeptiert) sowie jeden Trailing-Rest ab,
    // statt ihn nur zu ignorieren.
    if (hexStr.empty() ||
        hexStr.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
    {
        return TFT_BLACK;
    }

    char* end;
    errno = 0;
    const unsigned long value = std::strtoul(hexStr.c_str(), &end, 16);

    // "end" erreicht durch die Zeichenklassen-Pruefung oben immer das Stringende;
    // zusaetzlich auf das eigene 24-Bit-RGB888-Format begrenzen.
    if (errno != 0 || end != hexStr.c_str() + hexStr.length() || value > 0xFFFFFFUL) {
        return TFT_BLACK;
    }

    return lcd.color24to16(value);
```

## Testhinweis (ohne Hardware)
Reine String-/Zahlparsing-Logik, host-kompilierbar nachvollziehbar (`lcd.color24to16()`
stammt aus LovyanGFX, nicht Teil dieses Repos, und ist für diesen Fund durch eine
Passthrough-Funktion ersetzt — der Fund liegt vollständig davor). Reproduktionsprogramm
(`g++ -std=c++17 -O1 -Wall -Wextra -fsanitize=address,undefined repro.cpp -o repro &&
./repro`) in
`app-improvements/usb-army-penetrator/repro-tft-hex-color-parser-validation-gap/repro.cpp`,
ALT/NEU aus der jeweils echten Funktion 1:1 übernommen, zehn Fälle (fünf Regressions-,
fünf Fund-Fälle):

```
regression: 0x-Praefix                   input=0xFF00FF     ALT=0xff00ff NEU=0xff00ff  match (Regression ausgeschlossen)
regression: ohne Praefix                 input=00FF00       ALT=0x00ff00 NEU=0x00ff00  match (Regression ausgeschlossen)
regression: leer                         input=             ALT=0x000000 NEU=0x000000  match (Regression ausgeschlossen)
regression: reines '0x'                  input=0x           ALT=0x000000 NEU=0x000000  match (Regression ausgeschlossen)
regression: ungueltiges Zeichen am Anfang input=ZZZZZZ       ALT=0x000000 NEU=0x000000  match (Regression ausgeschlossen)
BUG: trailing Muell                      input=FF00FFxyz    ALT=0xff00ff NEU=0x000000  match (Fund bestaetigt)
BUG: fuehrendes Minuszeichen             input=-FF          ALT=0xffffffffffffff01 NEU=0x000000  match (Fund bestaetigt)
BUG: fuehrendes Pluszeichen              input=+10          ALT=0x000010 NEU=0x000000  match (Fund bestaetigt)
BUG: fuehrendes Leerzeichen              input= FF          ALT=0x0000ff NEU=0x000000  match (Fund bestaetigt)
BUG: > 24 Bit (8 Hex-Ziffern)            input=FFFFFFFF     ALT=0xffffffff NEU=0x000000  match (Fund bestaetigt)

ALLE CHECKS BESTANDEN
```

Hinweis zum Minuszeichen-Fall: `unsigned long` ist auf dem Host (x86_64) 64 Bit breit, auf
dem realen ESP32-Ziel (ILP32) 32 Bit — der konkrete Zweierkomplement-Wert von `"-FF"`
unterscheidet sich dadurch zwischen Host-Repro (`0xffffffffffffff01`) und Zielgerät
(`0xffffff01`); der Fund selbst (ein Vorzeichen wird überhaupt akzeptiert und liefert einen
mit der Eingabe unzusammenhängenden Wert statt eines Fehlers) ist davon unabhängig und auf
beiden Breiten identisch reproduzierbar. Der `"FFFFFFFF"`-Fall wurde bewusst so gewählt,
dass er auf beiden Breiten verlustfrei in `unsigned long` passt (kein plattformabhängiges
`errno`-Verhalten).

## Risiko
- Sehr niedrig: reine Verschärfung der Eingabevalidierung vor der bereits vorhandenen
  `TFT_BLACK`-Rückgabe, keine Änderung an der Named-Color-Logik, an der `0x`-Präfixbehandlung
  oder an `lcd.color24to16()` selbst.
- Aktuell ohne Wirkung auf laufenden Betrieb, da der Hex-Zweig von keiner realen Aufrufstelle
  erreicht wird (siehe oben) — reine Absicherung für den Zeitpunkt, an dem eine externe
  Eingabequelle (Ducky-Kommando, Web-UI, Settings-Datei) angebunden wird.
- Für alle heute genutzten Named-Color-Aufrufe (ausschließlich über `colorLookup`, nie über
  diesen Zweig) keine Verhaltensänderung.

## Quelle
- `G4MEOVER18/usb-army-penetrator` (main, `411b153`): `src/Devices/TFT/HardwareTFT.cpp`
  (`convertStringToColor`); Aufrufstellen-Vollsuche in `src/main.cpp` und
  `src/Devices/Platform/ESP32BoardSupport.cpp` (ausschließlich Named-Color-Literale).
- Schließt den zuletzt offenen `usb-army-penetrator`-Rückstellpunkt "TFT-Hex-Farbparser
  unerreicht" ab (Reichweite bestätigt: aktuell unerreicht; Funktion selbst jetzt geprüft und
  gehärtet statt nur als offen vermerkt).
