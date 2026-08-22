# usb-army-penetrator — Storage: trailing '\r' bei CRLF-Skripten in `readLineFromFile`

## Was
`src/Devices/Storage/HardwareStorage.cpp`, `HardwareStorage::readLineFromFile()`,
ist die einzige Stelle, über die mein Ducky-Interpreter (`DuckyPayload::loop()` →
`readLineFromFileOrCmdLine()`, `src/Attacks/Ducky/DuckyPayload.cpp`) jede Zeile
eines von SD geladenen Skripts liest — sowohl `AUTORUN.dss` als auch jede über
Web-UI/USB-MSC hochgeladene `.ds`-Payload-Datei:

```cpp
int currentLine = 0;
String line;
while (file.available())
{
    line = file.readStringUntil('\n'); // todo read up to EOF
    if (currentLine == lineNumber)
    {
        break;
    }
    line = "";
    currentLine++;
}

std::string ret = std::string(line.c_str());

if (ret.length() == 1 && ret[0] == '\n')
{
    return "";
}
if (ret.length() == 2 && ret[0] == '\r' && ret[1] == '\n')
{
    return "";
}

return ret;
```

`Stream::readStringUntil('\n')` (Arduino-Core, `Stream.cpp`) konsumiert das
Trennzeichen `'\n'` beim Lesen, hängt es aber nie an den zurückgegebenen String
an — das ist dokumentiertes Standardverhalten, nicht projektspezifisch. Damit
kann `line` nie ein `'\n'` enthalten, die beiden obigen Sonderfälle
(`ret == "\n"` bzw. `ret == "\r\n"`) sind also toter Code, der nie greift.

Was dabei übersehen wird: bei einer CRLF-terminierten Quelldatei (Windows-Editor,
oder eine aus einer Payload-Sammlung kopierte `.dss`-Datei — bei
Rubber-Ducky-/BadUSB-Skripten aus dem Netz sehr verbreitet) endet **jede**
Zeile vor dem konsumierten `'\n'` noch auf ein `'\r'`, das komplett unangetastet
in `ret` landet. Eine leere Zeile wird so nicht zu `""`, sondern zu `"\r"`
(Sonderfall greift nicht, da `ret[0]` `'\r'` statt `'\n'` ist). Schlimmer: eine
echte Befehlszeile wie `STRING Hello` bekommt ein unsichtbares `\r` angehängt
(`"STRING Hello\r"`) und wird als solche an den Ducky-Interpreter
weitergereicht.

## Nutzen
- Betrifft nicht nur Leerzeilen, sondern jede einzelne Zeile jeder
  CRLF-kodierten Skriptdatei — bei `STRING`/`STRINGLN` würde das overhead-freie
  `\r` als zusätzlicher, unsichtbarer Tastendruck ans Zielsystem mitgetippt
  (z. B. `STRING calc\r` löst je nach Kontextfenster ein zusätzliches Enter
  aus), bei numerischen Argumenten (`DELAY`, `MOUSE_MOVE`, Settings-Werten)
  ist das Risiko geringer (führende Ziffern werden meist toleranzfrei
  geparst), aber nicht bei jedem String-Vergleich im Interpreter/den
  Extensions — z. B. würde ein exakter Gleichheitsvergleich gegen einen
  erwarteten Wert (Dateinamen, Settings-Namen, `KEYBOARD_LAYOUT`-Bezeichner)
  am angehängten `\r` scheitern.
- Schließt eine echte Lücke zwischen meinen eigenen (LF-)Skripten, mit denen
  ich den Bug nie auslöse, und importierten/geteilten Payloads, bei denen ich
  die Zeilenenden nicht kontrolliere — genau der Importpfad, den auch schon
  der `pp_parse_hex_u64_strict`-Fix in ProtoPirate und der
  Path-Traversal-Fix im Web-UI dieser App absichern.
- Ersetzt zwei nachweislich unerreichbare Sonderfälle durch die tatsächlich
  nötige, allgemeine Behandlung — keine neue Fallunterscheidung, sondern eine
  Korrektur der bestehenden.

## Betroffene Dateien
- `src/Devices/Storage/HardwareStorage.cpp` — `HardwareStorage::readLineFromFile()`.

Keine Änderung an der Funktionssignatur, an `readLineFromFileOrCmdLine()` oder
am Ducky-Interpreter selbst nötig.

## Patch
```cpp
std::string HardwareStorage::readLineFromFile(const std::string &filename, const int lineNumber)
{
    File file = FILE_INTERFACE.open(filename.c_str(), "r");
    if (!file)
    {
        Debug::Log.info(LOG_MMC, "Could not open file: " + filename);
        return "";
    }

    int currentLine = 0;
    String line;
    while (file.available())
    {
        line = file.readStringUntil('\n'); // todo read up to EOF
        if (currentLine == lineNumber)
        {
            break;
        }
        line = "";
        currentLine++;
    }

    std::string ret = std::string(line.c_str());

    // readStringUntil('\n') konsumiert den Trenner, haengt ihn aber nie an --
    // "ret" kann also nie "\n" oder "...\r\n" enden. Bei CRLF-Quelldateien
    // bleibt stattdessen ein trailing '\r' auf JEDER Zeile stehen (auch auf
    // inhaltstragenden, nicht nur auf Leerzeilen). Das hier ersetzt die beiden
    // toten Sonderfaelle durch die tatsaechlich noetige allgemeine Behandlung.
    if (!ret.empty() && ret.back() == '\r')
    {
        ret.pop_back();
    }

    return ret;
}
```

Einzige Änderung: die beiden `if`-Blöcke, die auf ein im String enthaltenes
`'\n'` prüfen, werden durch eine einzige Prüfung auf ein trailing `'\r'`
ersetzt.

## Testhinweis (ohne Hardware)
Reine String-Logik (Zeilenextraktion + Nachbearbeitung), host-kompilierbar
nachvollziehbar. Reproduktionsprogramm
(`g++ -std=c++17 repro.cpp -o repro && ./repro`), das `readStringUntil('\n')`
über einen Buffer nachbildet (identisches Verhalten: Trennzeichen wird
konsumiert, aber nie angehängt) und `readLineFromFile()` vorher/nachher
1:1 nachstellt:

```cpp
#include <cstdio>
#include <string>

static std::string readStringUntilLF(const std::string &buf, size_t &pos)
{
    std::string out;
    while (pos < buf.size())
    {
        char c = buf[pos++];
        if (c == '\n') break;
        out.push_back(c);
    }
    return out;
}

static std::string parse_before(const std::string &buf, int lineNumber)
{
    size_t pos = 0;
    int currentLine = 0;
    std::string line;
    while (pos < buf.size())
    {
        line = readStringUntilLF(buf, pos);
        if (currentLine == lineNumber) break;
        line = "";
        currentLine++;
    }
    std::string ret = line;
    if (ret.length() == 1 && ret[0] == '\n') return "";
    if (ret.length() == 2 && ret[0] == '\r' && ret[1] == '\n') return "";
    return ret;
}

static std::string parse_after(const std::string &buf, int lineNumber)
{
    size_t pos = 0;
    int currentLine = 0;
    std::string line;
    while (pos < buf.size())
    {
        line = readStringUntilLF(buf, pos);
        if (currentLine == lineNumber) break;
        line = "";
        currentLine++;
    }
    std::string ret = line;
    if (!ret.empty() && ret.back() == '\r') ret.pop_back();
    return ret;
}

int main()
{
    const std::string crlfScript = "STRING Hello\r\nSTRINGLN Copyright\r\n\r\nDELAY 500\r\n";
    printf("Inhaltszeile vorher=\"%s\" nachher=\"%s\"\n",
        parse_before(crlfScript, 0).c_str(), parse_after(crlfScript, 0).c_str());
    printf("Leerzeile    vorher=\"%s\" nachher=\"%s\"\n",
        parse_before(crlfScript, 2).c_str(), parse_after(crlfScript, 2).c_str());
    return 0;
}
```

Vollständige Version mit sechs Testfällen (LF- und CRLF-Skript, Inhalts- und
Leerzeile, letzte Zeile mit/ohne abschließenden Zeilenumbruch) liegt bei:
`repro.cpp` in diesem Ordner.

Erwartete Ausgabe (gekürzt): bei der CRLF-Inhaltszeile liefert `vorher`
`"STRING Hello\r"` (mit sichtbarem `\r` am Ende), `nachher` das korrekte
`"STRING Hello"`. Bei der CRLF-Leerzeile liefert `vorher` `"\r"` statt der
erwarteten leeren Zeile, `nachher` korrekt `""`. Bei reinen LF-Skripten ist
`vorher`/`nachher` identisch — keine Verhaltensänderung für meine eigenen,
bereits LF-kodierten Skripte.

Funktionaler Test danach lokal auf dem Gerät: eine `.dss`-Testdatei mit
Windows-Zeilenenden (`STRING calc\r\n`) auf die SD-Karte legen und ausführen —
vor dem Fix tippt das Gerät ein zusätzliches, unsichtbares Zeichen nach
`calc` mit, danach nicht mehr.

## Risiko
- Sehr niedrig: reine Verschärfung/Korrektur der Nachbearbeitung einer bereits
  gelesenen Zeile, keine Änderung an Dateizugriff, Zeilenzählung oder
  EOF-Erkennung (`file.available()`-Schleife bleibt unverändert).
- Für meine eigenen, LF-kodierten Skripte (das bisher einzige real getestete
  Format) identisches Verhalten vorher/nachher — der Fix wirkt ausschließlich
  auf den bisher kaputten CRLF-Fall.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko.

## Quelle
- `G4MEOVER18/usb-army-penetrator` (main, `411b153`):
  `src/Devices/Storage/HardwareStorage.cpp` (`readLineFromFile`), Aufrufer
  `src/Attacks/Ducky/DuckyPayload.cpp` (`readLineFromFileOrCmdLine`).
