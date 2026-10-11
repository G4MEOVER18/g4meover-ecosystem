// Host-Repro: ducky_run() ueberliest eine zu lange physische Skriptzeile (>219 Nutzzeichen,
// line[220]-Puffer) nicht bis zum naechsten '\n', sondern bricht die Zeile einfach am
// Puffer-Limit ab und faengt die naechste Aussenschleifen-Iteration SOFORT an derselben
// Stelle (mitten in der ueberlangen Zeile) wieder an. Der Rest der echten Zeile wird dadurch
// als eigenstaendige, neue Kommandozeile fehlinterpretiert — ein Stichwort, das zufaellig im
// nicht mehr in den Puffer passenden Rest eines langen STRING-Texts auftaucht (z.B. "ENTER"
// oder ein Modifier-Kombo wie "GUI-r"), wird dann tatsaechlich als eigener Ducky-Befehl
// ausgefuehrt statt als reiner Text getippt zu werden.
//
// Fix (ducky_fixed.h, einzige Aenderung ggue. ducky_orig.h): wird der Zeilenpuffer voll,
// OHNE dass '\n' erreicht wurde, wird der Rest der echten Zeile bis zum naechsten '\n'
// verworfen statt in der naechsten Iteration als neue Zeile gelesen zu werden.
//
// Baut und laeuft rein auf dem Host (Arduino.h/USBHIDKeyboard.h hier als schlanke Mocks,
// die press()/write()/print()/releaseAll() in ein Log aufzeichnen statt echten USB-HID-
// Traffic zu senden) — reine Logik, kein Hardwarezugriff.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Einmal GLOBAL einbinden (pragma once), damit String/USBHIDKeyboard fuer beide
// Namensraeume (Orig/Fixed) dieselben, am Ende der Datei genutzten Typen sind —
// nur ducky_run()/ducky_keyname()/... selbst existieren unten in zwei Kopien.
#include "Arduino.h"
#include "USBHIDKeyboard.h"

std::vector<std::string> g_log;

namespace Orig {
#include "ducky_orig.h"
}
namespace Fixed {
#include "ducky_fixed.h"
}

static int g_fail = 0;

static void expect(bool cond, const char* what) {
    if(!cond) { printf("  [FAIL] %s\n", what); g_fail++; }
    else      { printf("  [ok]   %s\n", what); }
}

static std::vector<std::string> run(void (*impl)(USBHIDKeyboard&, const char*), const char* script) {
    g_log.clear();
    USBHIDKeyboard kb;
    impl(kb, script);
    return g_log;
}

static bool contains(const std::vector<std::string>& log, const std::string& needle) {
    for(auto& l : log) if(l == needle) return true;
    return false;
}

static void dump(const char* label, const std::vector<std::string>& log) {
    printf("    %s (%zu Events): ", label, log.size());
    for(auto& l : log) printf("%s | ", l.c_str());
    printf("\n");
}

int main() {
    printf("== Szenario 1: Regressionsfall (kurzes, normales Skript) ==\n");
    {
        const char* script =
            "REM Marker\nGUI r\nDELAY 10\nSTRING notepad\nENTER\nDELAY 10\n"
            "STRING hallo welt\nENTER\n";
        auto a = run(Orig::ducky_run, script);
        auto b = run(Fixed::ducky_run, script);
        dump("ALT", a); dump("NEU", b);
        expect(a == b, "ALT und NEU liefern fuer ein normales Skript identische Events");
    }

    printf("\n== Szenario 2: Grenzfall genau auf Puffergroesse (kein Ueberlauf) ==\n");
    {
        // "STRING " (7) + 212 'A' = 219 Zeichen == sizeof(line)-1 (passt noch EXAKT,
        // kein Ueberlauf-Pfad). Danach eine normale zweite Zeile.
        std::string s1 = "STRING " + std::string(212, 'A');
        std::string script = s1 + "\nENTER\n";
        auto a = run(Orig::ducky_run, script.c_str());
        auto b = run(Fixed::ducky_run, script.c_str());
        dump("ALT", a); dump("NEU", b);
        expect(a == b, "Zeile exakt an der Puffergrenze: ALT und NEU identisch (keine Regression)");
        expect(a.size() == 2 && a[0] == std::string("PRINT:") + std::string(212, 'A') &&
               a[1] == "WRITE:0xB0", "Grenzfall: STRING vollstaendig + ENTER korrekt verarbeitet");
    }

    printf("\n== Szenario 3: Fund — ueberlange STRING-Zeile, Rest spellt \"ENTER\" ==\n");
    {
        // "STRING " (7) + 212 'A' = 219 Zeichen fuellen den Puffer exakt, der Rest der
        // physischen Zeile ist exakt "ENTER" (ducky_keyname vergleicht exakt, kein Praefix).
        std::string s1 = "STRING " + std::string(212, 'A') + "ENTER";
        std::string script = s1 + "\nDELAY 10\n";
        auto a = run(Orig::ducky_run, script.c_str());
        auto b = run(Fixed::ducky_run, script.c_str());
        dump("ALT", a); dump("NEU", b);
        expect(contains(a, "WRITE:0xB0"),
               "ALT: injiziert einen ungewollten ENTER-Tastendruck aus dem Zeilenrest");
        expect(!contains(b, "WRITE:0xB0"),
               "NEU: kein ungewollter ENTER-Tastendruck mehr (Zeilenrest korrekt verworfen)");
        expect(b.size() == 1 && b[0] == std::string("PRINT:") + std::string(212, 'A'),
               "NEU: genau ein PRINT-Event mit dem (laengenbegrenzten) Originaltext, sonst nichts");
    }

    printf("\n== Szenario 4: Fund — Rest spellt ein Modifier-Kombo (\"GUI-r\", Ausfuehren-Dialog) ==\n");
    {
        // Realistischeres Risiko als ein stilles ENTER: der Zeilenrest trifft zufaellig auf
        // "GUI-r" (Windows-Ausfuehren-Dialog) und wird als eigener Befehl ausgefuehrt.
        std::string s1 = "STRING " + std::string(212, 'A') + "GUI-r";
        std::string script = s1 + "\nSTRINGLN calc\n";
        auto a = run(Orig::ducky_run, script.c_str());
        auto b = run(Fixed::ducky_run, script.c_str());
        dump("ALT", a); dump("NEU", b);
        expect(contains(a, "PRESS:0x83") && contains(a, "PRESS:0x72"),
               "ALT: presst GUI+'r' (Ausfuehren-Dialog) aus dem Zeilenrest, nicht Teil des Skripts");
        expect(!contains(b, "PRESS:0x83"),
               "NEU: kein GUI aus dem Zeilenrest mehr gepresst");
    }

    printf("\n== Szenario 5: zwei aufeinanderfolgende ueberlange Zeilen (Resync-Faehigkeit) ==\n");
    {
        std::string s1 = "STRING " + std::string(212, 'A') + "ENTER";
        std::string s2 = "STRING " + std::string(212, 'B') + "ENTER";
        std::string script = s1 + "\n" + s2 + "\nDELAY 5\n";
        auto a = run(Orig::ducky_run, script.c_str());
        auto b = run(Fixed::ducky_run, script.c_str());
        dump("ALT", a); dump("NEU", b);
        int altReturns = 0; for(auto& l : a) if(l == "WRITE:0xB0") altReturns++;
        int neuReturns = 0; for(auto& l : b) if(l == "WRITE:0xB0") neuReturns++;
        char buf[80];
        snprintf(buf, sizeof(buf), "ALT injiziert %d ungewollte ENTERs ueber zwei Zeilen (erwartet 2)", altReturns);
        expect(altReturns == 2, buf);
        snprintf(buf, sizeof(buf), "NEU injiziert 0 ungewollte ENTERs (erwartet 0, Resync pro Zeile)");
        expect(neuReturns == 0, buf);
        expect(b.size() == 2 &&
               b[0] == std::string("PRINT:") + std::string(212, 'A') &&
               b[1] == std::string("PRINT:") + std::string(212, 'B'),
               "NEU: beide Zeilen unabhaengig korrekt als STRING verarbeitet, DELAY danach erreicht");
    }

    printf("\n%s\n", g_fail ? "FAIL" : "ALLE SZENARIEN BESTANDEN");
    return g_fail ? 1 : 0;
}
