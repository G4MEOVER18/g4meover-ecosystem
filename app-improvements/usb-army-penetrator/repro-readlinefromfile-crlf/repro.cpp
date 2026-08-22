// Host-Repro fuer HardwareStorage::readLineFromFile() (src/Devices/Storage/HardwareStorage.cpp).
//
// readStringUntilLF() bildet Arduino Stream::readStringUntil('\n') 1:1 nach: das
// Trennzeichen wird konsumiert (Leseposition wandert dahinter), taucht aber NIE im
// zurueckgegebenen String auf -- exakt das dokumentierte Verhalten der echten
// Arduino-Core-Funktion. readLineFromFile() selbst wird hier zeilengetreu als
// parse_before()/parse_after() nachgebaut (nur File/Debug::Log durch die
// Buffer-Simulation ersetzt), alle Vergleiche/Sonderfaelle sind unveraendert aus dem
// Original uebernommen bzw. (im "nachher"-Fall) durch den Fix ersetzt.
#include <cstdio>
#include <string>

static std::string readStringUntilLF(const std::string &buf, size_t &pos)
{
    std::string out;
    while (pos < buf.size())
    {
        char c = buf[pos++];
        if (c == '\n')
        {
            break; // Trennzeichen wird konsumiert, aber nicht angehaengt
        }
        out.push_back(c);
    }
    return out;
}

// Vorher (Bug): prueft nur auf "Zeile ist exakt das Trennzeichen selbst" -- ein Fall,
// der bei readStringUntil('\n') nie eintreten kann, da '\n' nie im Ergebnis auftaucht.
static std::string parse_before(const std::string &buf, int lineNumber)
{
    size_t pos = 0;
    int currentLine = 0;
    std::string line;
    while (pos < buf.size())
    {
        line = readStringUntilLF(buf, pos);
        if (currentLine == lineNumber)
        {
            break;
        }
        line = "";
        currentLine++;
    }

    std::string ret = line;

    if (ret.length() == 1 && ret[0] == '\n')
    {
        return "";
    }
    if (ret.length() == 2 && ret[0] == '\r' && ret[1] == '\n')
    {
        return "";
    }

    return ret;
}

// Nachher (Fix): entfernt ein tatsaechlich vorhandenes trailing '\r' -- unabhaengig
// davon, ob die Zeile leer oder Inhalt hat.
static std::string parse_after(const std::string &buf, int lineNumber)
{
    size_t pos = 0;
    int currentLine = 0;
    std::string line;
    while (pos < buf.size())
    {
        line = readStringUntilLF(buf, pos);
        if (currentLine == lineNumber)
        {
            break;
        }
        line = "";
        currentLine++;
    }

    std::string ret = line;

    if (!ret.empty() && ret.back() == '\r')
    {
        ret.pop_back();
    }

    return ret;
}

static void check(const char *label, const std::string &buf, int lineNumber, const std::string &expected)
{
    const std::string before = parse_before(buf, lineNumber);
    const std::string after = parse_after(buf, lineNumber);

    auto show = [](const std::string &s)
    {
        std::string out;
        for (char c : s)
        {
            if (c == '\r') out += "\\r";
            else if (c == '\n') out += "\\n";
            else out.push_back(c);
        }
        return out;
    };

    printf("%-34s vorher=\"%-16s\" nachher=\"%-16s\" erwartet=\"%s\"  %s\n",
        label, show(before).c_str(), show(after).c_str(), show(expected).c_str(),
        after == expected ? "OK" : "MISMATCH");
}

int main()
{
    // LF-Skript (unix-typisch): Zeile 2 (0-indiziert) ist eine echte Leerzeile.
    const std::string lfScript = "STRING Hello\nSTRINGLN Copyright\n\nDELAY 500\n";
    check("LF: Inhaltszeile", lfScript, 0, "STRING Hello");
    check("LF: echte Leerzeile", lfScript, 2, "");

    // CRLF-Skript (z.B. auf Windows editiert/aus einer Payload-Sammlung kopiert):
    // dieselben vier logischen Zeilen, aber mit \r\n statt \n getrennt.
    const std::string crlfScript = "STRING Hello\r\nSTRINGLN Copyright\r\n\r\nDELAY 500\r\n";
    check("CRLF: Inhaltszeile", crlfScript, 0, "STRING Hello");
    check("CRLF: echte Leerzeile", crlfScript, 2, "");
    check("CRLF: letzte Zeile", crlfScript, 3, "DELAY 500");

    // CRLF-Skript ohne abschliessenden Zeilenumbruch nach der letzten Zeile.
    const std::string crlfNoFinalNewline = "GUI r\r\nDELAY 500\r\nSTRING calc\r";
    check("CRLF ohne EOF-Newline", crlfNoFinalNewline, 2, "STRING calc");

    return 0;
}
