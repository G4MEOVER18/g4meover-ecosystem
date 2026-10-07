// Host-Repro: setSettingValue() Int16/UInt16-Pfad gegen std::stoi-Ausnahmen.
// ALT = 1:1 Logik aus Settings.cpp vor dem Fix (keine try/catch).
// NEU = Logik aus Settings.cpp nach dem Fix (try/catch um std::stoi).
// Kein Preferences/Arduino-Objekt noetig: statt prefs.putShort/putUShort
// wird ein bool-Flag gesetzt, um zu pruefen, ob der Erfolgspfad erreicht wurde.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                         \
        if(!(cond)) {                                                        \
            g_failed++;                                                     \
            printf("  FAIL: %s (line %d)\n", #cond, __LINE__);               \
        }                                                                    \
    } while(0)

// ---------------------------------------------------------------------------
// ALT: 1:1 aus Settings.cpp::setSettingValue(), Int16-Zweig, vor dem Fix.
// Gibt true zurueck, wenn eine C++-Ausnahme den Aufrufer ungefangen erreicht
// (entspricht std::terminate()/Absturz auf dem echten Geraet, da nirgends im
// gesamten Projekt ein catch existiert).
// ---------------------------------------------------------------------------
static bool alt_int16_throws(const std::string& value, bool* accepted) {
    int tempInt = 0;
    *accepted = false;
    try {
        // Produktionscode hat hier KEIN try/catch - wir fangen nur, um die
        // Ausnahme im Testharness kontrolliert zu beobachten, statt den
        // gesamten Testprozess abstuerzen zu lassen.
        tempInt = std::stoi(value);
    } catch(const std::exception&) {
        return true; // entspricht unbehandeltem Absturz im echten Code
    }
    if(tempInt <= INT16_MAX && tempInt >= INT16_MIN) {
        *accepted = true;
    }
    return false;
}

static bool alt_uint16_throws(const std::string& value, bool* accepted) {
    int tempInt = 0;
    *accepted = false;
    if(value.rfind("0x", 0) == 0) {
        try {
            tempInt = std::stoi(value, nullptr, 16);
        } catch(const std::exception&) {
            return true;
        }
        if(tempInt <= INT16_MAX && tempInt >= 0) {
            *accepted = true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// NEU: Fix - try/catch um jeden std::stoi()-Aufruf, Fehler wird wie die
// bereits vorhandenen "Invalid setting value"-Zweige behandelt (ret bleibt
// false, kein Werfen nach aussen).
// ---------------------------------------------------------------------------
static bool neu_int16_set(const std::string& value, bool* accepted) {
    int tempInt = 0;
    *accepted = false;
    try {
        tempInt = std::stoi(value);
    } catch(const std::exception&) {
        return true; // "Invalid setting value, not a valid integer" geloggt, kein Wurf
    }
    if(tempInt <= INT16_MAX && tempInt >= INT16_MIN) {
        *accepted = true;
    }
    return true; // Funktion kehrt immer regulaer zurueck
}

static bool neu_uint16_set(const std::string& value, bool* accepted) {
    int tempInt = 0;
    *accepted = false;
    if(value.rfind("0x", 0) == 0) {
        try {
            tempInt = std::stoi(value, nullptr, 16);
        } catch(const std::exception&) {
            return true;
        }
        if(tempInt <= INT16_MAX && tempInt >= 0) {
            *accepted = true;
        }
    }
    return true;
}

int main() {
    printf("=== ALT (vor Fix): Settings.cpp setSettingValue() Int16/UInt16 ===\n");

    // Szenario 1: leerer Wert (z.B. HTTP GET ohne "value"-Inhalt oder mit
    // leerem Query-Parameter "?name=usbAgentPollingTime&value=") -> invalid_argument.
    {
        bool accepted;
        bool threw = alt_int16_throws("", &accepted);
        printf("[ALT] Int16 value=\"\" -> %s\n", threw ? "WIRFT (Absturz)" : "ok");
        CHECK(threw);
    }
    // Szenario 2: nicht-numerischer Wert, wie er ueber die Web-API ohne
    // jede serverseitige Validierung ankommen kann (GET /set?name=...&value=zzz).
    {
        bool accepted;
        bool threw = alt_int16_throws("not_a_number", &accepted);
        printf("[ALT] Int16 value=\"not_a_number\" -> %s\n", threw ? "WIRFT (Absturz)" : "ok");
        CHECK(threw);
    }
    // Szenario 3: zu grosse Zahl (gueltige Ziffernfolge, aber ausserhalb des
    // int-Bereichs) -> out_of_range.
    {
        bool accepted;
        bool threw = alt_int16_throws("99999999999999999999999999", &accepted);
        printf("[ALT] Int16 value=\"99999999999999999999999999\" -> %s\n",
               threw ? "WIRFT (Absturz)" : "ok");
        CHECK(threw);
    }
    // Szenario 4: UInt16-Zweig, Hex-Wert ausserhalb des int-Bereichs (9 Hex-
    // Ziffern, > INT_MAX) -> out_of_range. (Hinweis, bei der Analyse selbst
    // widerlegt: "0xZZZZ"/"0x" allein werfen NICHT - std::stoi(..., 16)
    // parst dank strtol-Praefixregel in diesem Fall nur die fuehrende "0"
    // und gibt 0 zurueck, siehe "Bewusst nicht behoben" unten.)
    {
        bool accepted;
        bool threw = alt_uint16_throws("0x100000000", &accepted);
        printf("[ALT] UInt16 value=\"0x100000000\" -> %s\n", threw ? "WIRFT (Absturz)" : "ok");
        CHECK(threw);
    }
    // Szenario 5: UInt16-Zweig, Grenzfall direkt oberhalb von INT16_MAX als
    // Hex (0x8000 = 32768) - wirft nicht (passt in int), wird aber korrekt
    // als ungueltiger Wert abgelehnt (Bereichspruefung, kein Fix-Gegenstand).
    {
        bool accepted;
        bool threw = alt_uint16_throws("0x8000", &accepted);
        printf("[ALT] UInt16 value=\"0x8000\" -> %s (accepted=%d, erwartet 0)\n",
               threw ? "WIRFT" : "ok", accepted);
        CHECK(!threw);
        CHECK(!accepted);
    }
    // Regression ALT: ein gueltiger Wert funktioniert nach wie vor.
    {
        bool accepted;
        bool threw = alt_int16_throws("1234", &accepted);
        CHECK(!threw);
        CHECK(accepted);
    }
    {
        bool accepted;
        bool threw = alt_uint16_throws("0x1A2B", &accepted);
        CHECK(!threw);
        CHECK(accepted);
    }

    printf("\n=== NEU (nach Fix) ===\n");
    // Dieselben fuenf boesartigen/fehlerhaften Werte: Funktion muss jetzt
    // immer regulaer zurueckkehren (kein Wurf nach aussen), accepted=false.
    const char* bad_values[] = {"", "not_a_number", "99999999999999999999999999"};
    for(const char* v : bad_values) {
        bool accepted;
        bool returned_cleanly = neu_int16_set(v, &accepted);
        printf("[NEU] Int16 value=\"%s\" -> %s, accepted=%d\n",
               v, returned_cleanly ? "kein Wurf" : "WIRFT weiterhin", accepted);
        CHECK(returned_cleanly);
        CHECK(!accepted);
    }
    const char* bad_hex[] = {"0x100000000", "0x80000000"};
    for(const char* v : bad_hex) {
        bool accepted;
        bool returned_cleanly = neu_uint16_set(v, &accepted);
        printf("[NEU] UInt16 value=\"%s\" -> %s, accepted=%d\n",
               v, returned_cleanly ? "kein Wurf" : "WIRFT weiterhin", accepted);
        CHECK(returned_cleanly);
        CHECK(!accepted);
    }

    // Regression NEU: gueltige Werte weiterhin korrekt angenommen, identisch
    // zum ALT-Verhalten im Erfolgsfall.
    {
        bool accepted;
        bool returned_cleanly = neu_int16_set("1234", &accepted);
        CHECK(returned_cleanly);
        CHECK(accepted);
    }
    {
        bool accepted;
        bool returned_cleanly = neu_int16_set("-5000", &accepted);
        CHECK(returned_cleanly);
        CHECK(accepted);
    }
    // Regression: aus dem Bereich fallende, aber syntaktisch gueltige Zahl
    // (INT16_MAX+1) wird weiterhin abgelehnt (accepted=false), nicht als Bug behandelt.
    {
        bool accepted;
        bool returned_cleanly = neu_int16_set("40000", &accepted);
        CHECK(returned_cleanly);
        CHECK(!accepted);
    }
    {
        bool accepted;
        bool returned_cleanly = neu_uint16_set("0x1A2B", &accepted);
        CHECK(returned_cleanly);
        CHECK(accepted);
    }
    {
        bool accepted;
        // Praefix-Pflicht bleibt unveraendert (kein "0x" -> sofort abgelehnt,
        // ohne stoi je aufzurufen).
        bool returned_cleanly = neu_uint16_set("1234", &accepted);
        CHECK(returned_cleanly);
        CHECK(!accepted);
    }

    printf("\n%d Pruefungen, %d fehlgeschlagen.\n", g_checks, g_failed);
    if(g_failed == 0) {
        printf("ALLE CHECKS BESTANDEN\n");
    }
    return g_failed == 0 ? 0 : 1;
}
