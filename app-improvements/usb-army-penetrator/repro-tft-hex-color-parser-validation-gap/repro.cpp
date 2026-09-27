// Host-Nachbau von HardwareTFT::convertStringToColor() (src/Devices/TFT/HardwareTFT.cpp),
// nur der Hex-Zweig (Named-Color-Lookup ist fuer diesen Fund irrelevant und hier weggelassen).
// color24to16() ist in LovyanGFX definiert (nicht Teil dieses Repos) -- fuer den Repro-Zweck
// durch eine Passthrough-Funktion ersetzt, da der Fund unabhaengig von ihrer konkreten
// Bit-Zuordnung ist (sie bekommt bei ALT einen nie beabsichtigten Wert ueberhaupt erst
// uebergeben).
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <string>

static const long TFT_BLACK = 0;

static long color24to16_stub(unsigned long rgb888)
{
    // Reale Funktion (LovyanGFX) verarbeitet nur die unteren 24 Bit einer RGB888-Farbe;
    // fuer den Repro genuegt ein Passthrough, der den vollen uebergebenen Wert sichtbar macht.
    return (long)rgb888;
}

static long convert_before(const std::string &color)
{
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

    return color24to16_stub(value);
}

static long convert_after(const std::string &color)
{
    std::string hexStr = color;
    if (hexStr.length() >= 2 && hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X'))
    {
        hexStr = hexStr.substr(2);
    }

    if (hexStr.empty() || hexStr.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
    {
        return TFT_BLACK;
    }

    char* end;
    errno = 0;
    const unsigned long value = std::strtoul(hexStr.c_str(), &end, 16);

    if (errno != 0 || end != hexStr.c_str() + hexStr.length() || value > 0xFFFFFFUL) {
        return TFT_BLACK;
    }

    return color24to16_stub(value);
}

struct Case { const char* label; const char* input; };

int main()
{
    Case cases[] = {
        // Regressionsfaelle: unveraendertes Verhalten fuer wohlgeformte Eingaben
        {"regression: 0x-Praefix",      "0xFF00FF"},
        {"regression: ohne Praefix",    "00FF00"},
        {"regression: leer",            ""},
        {"regression: reines '0x'",     "0x"},
        {"regression: ungueltiges Zeichen am Anfang", "ZZZZZZ"},
        // Fund: Eingaben, die ALT unbeabsichtigt als "gueltig" akzeptiert
        {"BUG: trailing Muell",         "FF00FFxyz"},
        {"BUG: fuehrendes Minuszeichen","-FF"},
        {"BUG: fuehrendes Pluszeichen", "+10"},
        {"BUG: fuehrendes Leerzeichen", " FF"},
        {"BUG: > 24 Bit (8 Hex-Ziffern)", "FFFFFFFF"},
    };

    int mismatches = 0;
    for (const auto &c : cases)
    {
        long before = convert_before(c.input);
        long after  = convert_after(c.input);
        bool isBugCase = std::strncmp(c.label, "BUG", 3) == 0;
        bool matches = (before == after);
        if (isBugCase && matches) { printf("UNEXPECTED: kein Unterschied bei %s\n", c.label); mismatches++; }
        if (!isBugCase && !matches) { printf("REGRESSION: %s weicht ab (ALT=%ld NEU=%ld)\n", c.label, before, after); mismatches++; }
        printf("%-40s input=%-12s ALT=0x%06lx NEU=0x%06lx  %s\n",
               c.label, c.input, (unsigned long)before, (unsigned long)after,
               isBugCase ? (matches ? "FEHLER: kein Unterschied" : "match (Fund bestaetigt)")
                         : (matches ? "match (Regression ausgeschlossen)" : "FEHLER: Regression"));
    }

    printf(mismatches == 0 ? "\nALLE CHECKS BESTANDEN\n" : "\n%d CHECK(S) FEHLGESCHLAGEN\n", mismatches);
    return mismatches == 0 ? 0 : 1;
}
