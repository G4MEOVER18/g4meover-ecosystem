// Host-Repro: handleRawHid() (Extensions.h) RAW_HID-Hex-Parsing gegen std::stoi-Ausnahmen.
// ALT = 1:1 Logik aus Extensions.h vor dem Fix (keine try/catch um std::stoi).
// NEU = Logik aus Extensions.h nach dem Fix (try/catch um std::stoi).
// Kein Arduino/TinyUSB/HID-Objekt noetig: die eigentliche Tastendruck-Ausgabe
// ist fuer diesen Fund irrelevant, geprueft wird ausschliesslich der
// Hex-Parse-Pfad bis zum Punkt, an dem ALT wirft.
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
// ALT: 1:1 aus Extensions.h::handleRawHid(), nur der hidStr-Zweig (modStr/
// SplitString sind fuer diesen Fund irrelevant - die fuehrende Pruefung auf
// "0x"-Praefix plus der stoi-Aufruf sind exakt wie im Original). Gibt true
// zurueck, wenn eine C++-Ausnahme den Aufrufer ungefangen erreicht (entspricht
// std::terminate()/Geraete-Absturz, da im gesamten Projekt kein catch
// existiert - per grep -rn "catch\s*(\|try\s*{" src/ bestaetigt).
// ---------------------------------------------------------------------------
static bool alt_raw_hid_throws(const std::string& hidStr, bool* ret) {
    *ret = false;
    if(hidStr.rfind("0x", 0) == 0) {
        int tempInt = 0;
        try {
            // Produktionscode hat hier KEIN try/catch - wir fangen nur, um
            // die Ausnahme im Testharness kontrolliert zu beobachten, statt
            // den gesamten Testprozess abstuerzen zu lassen.
            tempInt = std::stoi(hidStr, nullptr, 16);
        } catch(const std::exception&) {
            return true; // entspricht unbehandeltem Absturz im echten Code
        }
        if(tempInt <= static_cast<int>(UINT8_MAX) && tempInt >= 0) {
            *ret = true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// NEU: Fix - try/catch um den std::stoi()-Aufruf, Fehler wird wie der
// bereits vorhandene "Invalid value"-Zweig behandelt (ret bleibt false,
// kein Werfen nach aussen).
// ---------------------------------------------------------------------------
static bool neu_raw_hid_set(const std::string& hidStr, bool* ret) {
    *ret = false;
    if(hidStr.rfind("0x", 0) == 0) {
        int tempInt = 0;
        try {
            tempInt = std::stoi(hidStr, nullptr, 16);
        } catch(const std::exception&) {
            return true; // "Invalid value, not a valid hex integer" geloggt, kein Wurf
        }
        if(tempInt <= static_cast<int>(UINT8_MAX) && tempInt >= 0) {
            *ret = true;
        }
    }
    return true; // Funktion kehrt immer regulaer zurueck
}

int main() {
    printf("=== ALT (vor Fix): Extensions.h handleRawHid() hidStr-Zweig ===\n");

    // (1)-(2) Fund: Hex-Werte, deren numerischer Wert ausserhalb des
    // int-Bereichs liegt (INT_MAX = 0x7FFFFFFF), werfen std::out_of_range
    // direkt aus std::stoi (unabhaengig von der Breite des Plattform-"long",
    // stoi prueft den int-Bereich zusaetzlich zur strtol-Konvertierung).
    {
        bool accepted;
        bool threw = alt_raw_hid_throws("0x80000000", &accepted);
        printf("[ALT] RAW_HID 0x80000000 -> %s\n", threw ? "WIRFT (Absturz)" : "ok");
        CHECK(threw);
    }
    {
        bool accepted;
        bool threw = alt_raw_hid_throws("0xFFFFFFFF", &accepted);
        printf("[ALT] RAW_HID 0xFFFFFFFF -> %s\n", threw ? "WIRFT (Absturz)" : "ok");
        CHECK(threw);
    }
    // (3) ein noch laengerer Hex-Wert - werfen bleibt unabhaengig von der
    // genauen Ueberlaufgroesse.
    {
        bool accepted;
        bool threw = alt_raw_hid_throws("0x100000000", &accepted);
        printf("[ALT] RAW_HID 0x100000000 -> %s\n", threw ? "WIRFT (Absturz)" : "ok");
        CHECK(threw);
    }

    // Gegenprobe zur bereits beim Settings.cpp-Fund dokumentierten
    // strtol-Praefixregel: "0x" allein bzw. ein ungueltiger Hex-Rest nach
    // "0x" wird NICHT als Praefix erkannt, stoi parst dann nur die
    // fuehrende "0" und wirft nicht (stiller Fehlparse auf 0, kein
    // Absturzpfad) - hier bewusst gegengeprueft, nicht erneut behauptet.
    {
        bool accepted;
        bool threw = alt_raw_hid_throws("0x", &accepted);
        printf("[ALT] RAW_HID 0x (allein) -> %s, accepted=%d\n",
               threw ? "WIRFT" : "kein Wurf", accepted);
        CHECK(!threw);
        CHECK(accepted == true); // 0 liegt im UINT8_MAX-Bereich -> wird akzeptiert
    }
    {
        bool accepted;
        bool threw = alt_raw_hid_throws("0xZZZZ", &accepted);
        printf("[ALT] RAW_HID 0xZZZZ -> %s, accepted=%d\n",
               threw ? "WIRFT" : "kein Wurf", accepted);
        CHECK(!threw);
        CHECK(accepted == true);
    }

    // Regressionsfaelle: gueltige, bereits vor dem Fix funktionierende Werte.
    {
        bool accepted;
        bool threw = alt_raw_hid_throws("0xFF", &accepted);
        CHECK(!threw);
        CHECK(accepted == true);
    }
    {
        bool accepted;
        bool threw = alt_raw_hid_throws("0x00", &accepted);
        CHECK(!threw);
        CHECK(accepted == true);
    }
    {
        // ausserhalb UINT8_MAX, aber innerhalb des int-Bereichs -> kein Wurf,
        // regulaer abgelehnt (bestehender "Invalid value"-Zweig).
        bool accepted;
        bool threw = alt_raw_hid_throws("0x100", &accepted);
        CHECK(!threw);
        CHECK(accepted == false);
    }
    {
        // exakt INT_MAX - kein Wurf (noch im int-Bereich), aber ausserhalb
        // UINT8_MAX -> regulaer abgelehnt.
        bool accepted;
        bool threw = alt_raw_hid_throws("0x7FFFFFFF", &accepted);
        CHECK(!threw);
        CHECK(accepted == false);
    }

    printf("\n=== NEU (nach Fix) ===\n");
    {
        bool accepted;
        bool ok = neu_raw_hid_set("0x80000000", &accepted);
        printf("[NEU] RAW_HID 0x80000000 -> kein Wurf, accepted=%d\n", accepted);
        CHECK(ok);
        CHECK(accepted == false);
    }
    {
        bool accepted;
        bool ok = neu_raw_hid_set("0xFFFFFFFF", &accepted);
        printf("[NEU] RAW_HID 0xFFFFFFFF -> kein Wurf, accepted=%d\n", accepted);
        CHECK(ok);
        CHECK(accepted == false);
    }
    {
        bool accepted;
        bool ok = neu_raw_hid_set("0x100000000", &accepted);
        printf("[NEU] RAW_HID 0x100000000 -> kein Wurf, accepted=%d\n", accepted);
        CHECK(ok);
        CHECK(accepted == false);
    }
    // NEU-Regressionsfaelle: identisches Verhalten zu ALT fuer alle
    // bisher schon gueltigen/bereits abgelehnten Werte.
    {
        bool accepted;
        neu_raw_hid_set("0xFF", &accepted);
        CHECK(accepted == true);
    }
    {
        bool accepted;
        neu_raw_hid_set("0x00", &accepted);
        CHECK(accepted == true);
    }
    {
        bool accepted;
        neu_raw_hid_set("0x100", &accepted);
        CHECK(accepted == false);
    }
    {
        bool accepted;
        neu_raw_hid_set("0x7FFFFFFF", &accepted);
        CHECK(accepted == false);
    }
    {
        bool accepted;
        neu_raw_hid_set("0x", &accepted);
        CHECK(accepted == true);
    }
    {
        bool accepted;
        neu_raw_hid_set("0xZZZZ", &accepted);
        CHECK(accepted == true);
    }

    printf("\n%d Pruefungen, %d fehlgeschlagen.\n", g_checks, g_failed);
    if(g_failed == 0) {
        printf("ALLE CHECKS BESTANDEN\n");
        return 0;
    }
    printf("FEHLER\n");
    return 1;
}
