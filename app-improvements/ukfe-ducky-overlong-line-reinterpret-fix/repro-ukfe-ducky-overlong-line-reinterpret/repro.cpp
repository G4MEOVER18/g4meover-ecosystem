// Host-Repro: Zeilen-Splitting-Fehler in heltec-ukfe-rx/src/ducky.h::ducky_run().
//
// ducky_run() liest Skriptzeilen in einen festen Stack-Puffer char line[220]. Die
// innere Schleife, die eine physische Zeile einliest, bricht ab, sobald entweder
// '\n'/Stringende erreicht ODER der Puffer voll ist (li == sizeof(line)-1) --
// beide Abbruchgruende werden identisch behandelt: der Zeiger p zeigt danach auf
// die naechste noch nicht konsumierte Stelle, line[] wird mit 0 terminiert und
// SOFORT als vollstaendiges Kommando ausgefuehrt. Bei Pufferueberlauf ist p aber
// NICHT an der echten Zeilengrenze ('\n'), sondern mitten in der physischen
// Zeile -- die aeussere while-Schleife liest den Rest beim naechsten Durchlauf
// einfach als EIGENE neue "Zeile" erneut ein und fuehrt ihn als eigenstaendiges
// Kommando aus.
//
// Diese Datei transkribiert ducky_run()/ducky_keyname()/ducky_modkey()/
// ducky_press_mods() 1:1 aus dem echten Funktionskoerper (Arduino String durch
// std::string ersetzt, USBHIDKeyboard durch einen Aktions-Rekorder ersetzt,
// String::toInt() durch eine aequivalente Nachbildung ersetzt). ALT = heutiger
// Zustand (Pufferueberlauf-Behandlung fehlt), NEU = Fix (Rest der physischen
// Zeile beim Pufferueberlauf verwerfen statt ihn neu zu parsen).
//
// Baut und laeuft rein auf dem Host, keine Hardware/USB-HID noetig:
//   g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -O0 repro.cpp -o repro_o0 && ./repro_o0
//   g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -O1 repro.cpp -o repro_o1 && ./repro_o1

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---- Synthetische Keycodes (Werte beliebig, nur Unterscheidbarkeit zaehlt) ----
enum : uint8_t {
    KEY_RETURN = 0x90, KEY_TAB, KEY_ESC, KEY_BACKSPACE, KEY_DELETE, KEY_INSERT,
    KEY_HOME, KEY_END, KEY_PAGE_UP, KEY_PAGE_DOWN, KEY_UP_ARROW, KEY_DOWN_ARROW,
    KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_CAPS_LOCK, KEY_F1,
    // KEY_F1..KEY_F12 muessen fortlaufend sein (Original nutzt KEY_F1 + (n-1)).
    KEY_LEFT_GUI = 0xB0, KEY_LEFT_CTRL, KEY_LEFT_ALT, KEY_LEFT_SHIFT,
};

// ---- Aktions-Rekorder statt echter USBHIDKeyboard ----
struct Action {
    enum Kind { PRESS, WRITE, RELEASE_ALL, STRING_OUT, DELAY_MS } kind;
    int code = 0;
    std::string text;
    uint32_t ms = 0;
};
struct MockKb {
    std::vector<Action> log;
    void press(uint8_t k)             { log.push_back({Action::PRESS, k, "", 0}); }
    void write(uint8_t k)             { log.push_back({Action::WRITE, k, "", 0}); }
    void releaseAll()                 { log.push_back({Action::RELEASE_ALL, 0, "", 0}); }
    void print(const std::string& s)  { log.push_back({Action::STRING_OUT, 0, s, 0}); }
};
static void delay_ms(MockKb& kb, uint32_t ms) { kb.log.push_back({Action::DELAY_MS, 0, "", ms}); }

// ---- String-Hilfsfunktionen aequivalent zu Arduino String ----
static std::string upper(std::string s) {
    for(char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}
// Arduino String::toInt(): liest optionales Vorzeichen + Ziffern vom Anfang, 0 wenn keine.
static long arduino_toInt(const std::string& s) {
    size_t i = 0;
    while(i < s.size() && isspace((unsigned char)s[i])) i++;
    size_t start = i;
    if(i < s.size() && (s[i] == '+' || s[i] == '-')) i++;
    size_t digits_start = i;
    while(i < s.size() && isdigit((unsigned char)s[i])) i++;
    if(i == digits_start) return 0;
    return strtol(s.substr(start, i - start).c_str(), nullptr, 10);
}

// ---- 1:1 aus ducky.h transkribiert ----
static uint8_t ducky_keyname(const std::string& k) {
    std::string u = upper(k);
    if(u == "ENTER" || u == "RETURN")       return KEY_RETURN;
    if(u == "TAB")                          return KEY_TAB;
    if(u == "SPACE")                        return ' ';
    if(u == "ESC" || u == "ESCAPE")         return KEY_ESC;
    if(u == "BACKSPACE")                    return KEY_BACKSPACE;
    if(u == "DELETE" || u == "DEL")         return KEY_DELETE;
    if(u == "INSERT")                       return KEY_INSERT;
    if(u == "HOME")                         return KEY_HOME;
    if(u == "END")                          return KEY_END;
    if(u == "PAGEUP" || u == "PAGE_UP")     return KEY_PAGE_UP;
    if(u == "PAGEDOWN" || u == "PAGE_DOWN") return KEY_PAGE_DOWN;
    if(u == "UP" || u == "UPARROW")         return KEY_UP_ARROW;
    if(u == "DOWN" || u == "DOWNARROW")     return KEY_DOWN_ARROW;
    if(u == "LEFT" || u == "LEFTARROW")     return KEY_LEFT_ARROW;
    if(u == "RIGHT" || u == "RIGHTARROW")   return KEY_RIGHT_ARROW;
    if(u == "CAPSLOCK")                     return KEY_CAPS_LOCK;
    if(u.length() >= 2 && u[0] == 'F') {
        long n = arduino_toInt(u.substr(1));
        if(n >= 1 && n <= 12) return (uint8_t)(KEY_F1 + (n - 1));
    }
    if(u.length() == 1) return (uint8_t)tolower((unsigned char)u[0]);
    return 0;
}

static uint8_t ducky_modkey(const std::string& t) {
    std::string u = upper(t);
    if(u == "GUI" || u == "WINDOWS" || u == "WIN" || u == "COMMAND") return KEY_LEFT_GUI;
    if(u == "CTRL" || u == "CONTROL") return KEY_LEFT_CTRL;
    if(u == "ALT")   return KEY_LEFT_ALT;
    if(u == "SHIFT") return KEY_LEFT_SHIFT;
    return 0;
}

static bool ducky_press_mods(MockKb& kb, const std::string& tok) {
    std::string first = tok;
    size_t h = tok.find('-');
    if(h != std::string::npos) first = tok.substr(0, h);
    if(!ducky_modkey(first)) return false;
    std::string c = tok;
    while(!c.empty()) {
        size_t i = c.find('-');
        std::string part = (i == std::string::npos) ? c : c.substr(0, i);
        c = (i == std::string::npos) ? std::string("") : c.substr(i + 1);
        uint8_t m = ducky_modkey(part);
        if(m) kb.press(m);
        else { uint8_t k = ducky_keyname(part); if(k) kb.press(k); }
    }
    return true;
}

// fix=false: heutiger Zustand (ALT). fix=true: mit Pufferueberlauf-Behandlung (NEU).
static void ducky_run(MockKb& kb, const char* script, bool fix) {
    uint32_t defDelay = 0;
    const char* p = script;
    char line[220];
    while(*p) {
        int li = 0;
        while(*p && *p != '\n' && li < (int)sizeof(line) - 1) { if(*p != '\r') line[li++] = *p; p++; }
        if(fix) {
            if(*p && *p != '\n') {              // Zeile laenger als der Puffer: Rest dieser physischen
                while(*p && *p != '\n') p++;    // Zeile verwerfen statt ihn als eigene, unbeabsichtigte
            }                                    // Kommandozeile zu parsen.
        }
        if(*p == '\n') p++;
        line[li] = 0;
        char* s = line; while(*s == ' ' || *s == '\t') s++;
        if(!*s) continue;

        char* sp = strchr(s, ' ');
        std::string cmd  = sp ? std::string(s, (size_t)(sp - s)) : std::string(s);
        std::string rest = sp ? std::string(sp + 1) : std::string("");
        std::string C = upper(cmd);

        if(C == "REM") { /* Kommentar */ }
        else if(C == "DELAY")            { delay_ms(kb, (uint32_t)arduino_toInt(rest)); }
        else if(C == "DEFAULT_DELAY" || C == "DEFAULTDELAY") { defDelay = (uint32_t)arduino_toInt(rest); }
        else if(C == "STRING")           { kb.print(rest); }
        else if(C == "STRINGLN")         { kb.print(rest); kb.write(KEY_RETURN); }
        else if(C == "REPEAT")           { /* optional: hier ignoriert */ }
        else if(ducky_press_mods(kb, cmd)) {
            std::string r = rest;
            while(!r.empty()) {
                size_t i = r.find(' ');
                std::string tok = (i == std::string::npos) ? r : r.substr(0, i);
                r = (i == std::string::npos) ? std::string("") : r.substr(i + 1);
                if(!ducky_press_mods(kb, tok)) { uint8_t k = ducky_keyname(tok); if(k) kb.press(k); }
            }
            delay_ms(kb, 20); kb.releaseAll();
        }
        else {
            uint8_t k = ducky_keyname(cmd);
            if(k) kb.write(k);
        }
        if(defDelay) delay_ms(kb, defDelay);
    }
}

// ---- Testhilfen ----
static int g_checks = 0, g_fails = 0;
#define CHECK(cond) do { g_checks++; if(!(cond)) { g_fails++; \
    fprintf(stderr, "CHECK FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); } } while(0)

static bool has_write(const std::vector<Action>& log, uint8_t code) {
    for(auto& a : log) if(a.kind == Action::WRITE && a.code == code) return true;
    return false;
}
static int count_kind(const std::vector<Action>& log, Action::Kind k) {
    int n = 0; for(auto& a : log) if(a.kind == k) n++; return n;
}

int main() {
    // ---- Szenario 1: Regression -- normales, kurzes Skript bleibt ALT/NEU identisch ----
    {
        const char* script =
            "REM Testskript\n"
            "DEFAULT_DELAY 5\n"
            "STRING Hallo Welt\n"
            "CTRL-ALT DELETE\n"
            "ENTER\n";
        MockKb alt, neu;
        ducky_run(alt, script, false);
        ducky_run(neu, script, true);
        CHECK(alt.log.size() == neu.log.size());
        bool identical = alt.log.size() == neu.log.size();
        for(size_t i = 0; identical && i < alt.log.size(); i++) {
            identical = alt.log[i].kind == neu.log[i].kind &&
                        alt.log[i].code == neu.log[i].code &&
                        alt.log[i].text == neu.log[i].text &&
                        alt.log[i].ms   == neu.log[i].ms;
        }
        CHECK(identical);
        CHECK(has_write(neu.log, KEY_RETURN));            // ENTER-Zeile hat gewirkt
        CHECK(count_kind(neu.log, Action::STRING_OUT) == 1);
    }

    // ---- Szenario 2: Ueberlange STRING-Zeile, Ueberlauf-Rest faellt exakt auf "ENTER" ----
    // Puffer fasst 219 Zeichen (li < sizeof(line)-1 = 219). "STRING " (7) + 212 Fuellzeichen
    // = exakt 219 Zeichen -> der Rest der physischen Zeile ("ENTER") beginnt exakt an der
    // Pufferkante und wird von ALT als eigenstaendige zweite Kommandozeile neu geparst.
    {
        std::string filler(212, 'x');
        std::string script = "STRING " + filler + "ENTER\n" + "STRING danach\n";
        CHECK(std::string("STRING ").size() + filler.size() == 219);

        MockKb alt;
        ducky_run(alt, script.c_str(), false);
        // ALT: Truncated STRING, DANACH ein unbeabsichtigtes WRITE(KEY_RETURN) aus dem
        // als "ENTER" fehlinterpretierten Zeilenrest, erst danach die naechste echte Zeile.
        CHECK(alt.log.size() == 3);
        CHECK(alt.log[0].kind == Action::STRING_OUT && alt.log[0].text == filler);
        CHECK(alt.log[1].kind == Action::WRITE && alt.log[1].code == KEY_RETURN); // Injektion!
        CHECK(alt.log[2].kind == Action::STRING_OUT && alt.log[2].text == "danach");

        MockKb neu;
        ducky_run(neu, script.c_str(), true);
        // NEU: Ueberschuss wird verworfen -- kein ungewolltes ENTER, direkt weiter zur naechsten Zeile.
        CHECK(neu.log.size() == 2);
        CHECK(neu.log[0].kind == Action::STRING_OUT && neu.log[0].text == filler);
        CHECK(neu.log[1].kind == Action::STRING_OUT && neu.log[1].text == "danach");
    }

    // ---- Szenario 3: Ueberschuss, der selbst wieder den Puffer sprengt (zwei volle ----
    // Pufferbreiten Fuellzeichen, bevor ein erkennbares Schluesselwort folgt) -- NEU muss
    // den GESAMTEN Rest bis zur echten Zeilengrenze verwerfen, nicht nur eine Pufferbreite.
    {
        std::string filler(212, 'y');
        std::string tail = std::string(438, 'z') + "TAB";  // 438 = genau 2x219 (zwei volle
                                                             // Ueberlauf-Puffer), dann "TAB"
        std::string script = "STRING " + filler + tail + "\n" + "STRING danach\n";

        MockKb alt;
        ducky_run(alt, script.c_str(), false);
        // ALT: Truncated STRING, zwei wirkungslose "zzz..."-Fehlparse-Durchlaeufe, DANN ein
        // unbeabsichtigtes WRITE(KEY_TAB) aus dem als "TAB" fehlinterpretierten Zeilenende --
        // zeigt, dass sich der Fehler ueber beliebig viele Pufferbreiten fortsetzt.
        CHECK(alt.log.size() == 3);
        CHECK(alt.log[0].kind == Action::STRING_OUT && alt.log[0].text == filler);
        CHECK(alt.log[1].kind == Action::WRITE && alt.log[1].code == KEY_TAB);   // Injektion!
        CHECK(alt.log[2].kind == Action::STRING_OUT && alt.log[2].text == "danach");

        MockKb neu;
        ducky_run(neu, script.c_str(), true);
        // NEU: der komplette Ueberschuss (ueber zwei Pufferbreiten hinweg) wird in einem
        // Rutsch bis zur echten Zeilengrenze verworfen -- kein TAB, direkt weiter.
        CHECK(neu.log.size() == 2);
        CHECK(neu.log[0].kind == Action::STRING_OUT && neu.log[0].text == filler);
        CHECK(neu.log[1].kind == Action::STRING_OUT && neu.log[1].text == "danach");
    }

    // ---- Szenario 4: Ueberlaufende Zeile am Skriptende (kein folgendes '\n') ----
    {
        std::string filler(212, 'a');
        std::string script = "STRING " + filler + "HOME";   // kein trailing '\n'

        MockKb alt;
        ducky_run(alt, script.c_str(), false);
        CHECK(alt.log.size() == 2);
        CHECK(alt.log[1].kind == Action::WRITE && alt.log[1].code == KEY_HOME); // Injektion am Skriptende

        MockKb neu;
        ducky_run(neu, script.c_str(), true);
        CHECK(neu.log.size() == 1);
        CHECK(neu.log[0].kind == Action::STRING_OUT && neu.log[0].text == filler);
    }

    // ---- Szenario 5: Kurze Zeilen am/unter der Pufferschwelle bleiben unveraendert ----
    {
        std::string filler(211, 'b');                 // "STRING " + 211 = 218 < 219 -> passt vollstaendig
        std::string script = "STRING " + filler + "\n" + "STRING danach\n";
        MockKb alt, neu;
        ducky_run(alt, script.c_str(), false);
        ducky_run(neu, script.c_str(), true);
        CHECK(alt.log.size() == 2 && neu.log.size() == 2);
        CHECK(alt.log[0].text == filler && neu.log[0].text == filler);
        CHECK(alt.log[1].text == "danach" && neu.log[1].text == "danach");
    }

    printf("Checks: %d, Fehlschlaege: %d\n", g_checks, g_fails);
    if(g_fails) { printf("ALLE CHECKS FEHLGESCHLAGEN\n"); return 1; }
    printf("ALLE CHECKS BESTANDEN\n");
    return 0;
}
