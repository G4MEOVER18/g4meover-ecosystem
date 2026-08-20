// Host-Repro fuer den Agent-Polling-Intervall-Typfehler in
// src/Attacks/Agent/Agent.cpp.
//
// Modelliert exakt die Deklaration, das Einlesen aus den Preferences und die
// Intervall-Berechnung in SerialAgent::loop(). Zeigt, dass der als Int16
// registrierte, per getInt() gelesene Intervallwert durch die bool-Deklaration
// auf 0/1 kollabiert und damit jede Nutzerkonfiguration wirkungslos wird.
//
// Bauen/Ausfuehren:
//   g++ -O2 -Wall -Wextra -o repro repro.cpp && ./repro
// Exit 0 = alle Assertions bestaetigt (Bug im "buggy"-Modell, korrekt im "fixed").

#include <cassert>
#include <cstdint>
#include <cstdio>

#define USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT 15

// ---- Modell des IST-Zustands (Agent.cpp:20) ------------------------------
// static bool agentPollingIntervalInSec = USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT;
struct BuggyAgent {
    bool agentPollingIntervalInSec = USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT; // 15 -> true

    // begin(): agentPollingIntervalInSec = prefs.getInt(name, default);
    void loadFromPrefs(int prefsValue) {
        agentPollingIntervalInSec = prefsValue; // int -> bool: !=0 -> 1, ==0 -> 0
    }

    // loop(): if (currentMillis - previousMillis >= agentPollingIntervalInSec * 1000)
    unsigned long windowMs() const {
        return (unsigned long)agentPollingIntervalInSec * 1000UL;
    }
};

// ---- Modell des SOLL-Zustands (Fix: int16_t) -----------------------------
struct FixedAgent {
    int16_t agentPollingIntervalInSec = USB_AGENT_POLLING_TIME_IN_SEC_DEFAULT;

    void loadFromPrefs(int prefsValue) {
        agentPollingIntervalInSec = (int16_t)prefsValue;
    }

    unsigned long windowMs() const {
        return (unsigned long)agentPollingIntervalInSec * 1000UL;
    }
};

int main() {
    // 1) Default (15 s) direkt nach Deklaration, ohne Prefs-Ueberschreibung.
    {
        BuggyAgent b;
        FixedAgent f;
        printf("default:            buggy=%lums  fixed=%lums\n", b.windowMs(), f.windowMs());
        assert(b.windowMs() == 1000);   // 15 -> true -> 1*1000 = 1s (falsch)
        assert(f.windowMs() == 15000);  // 15s (korrekt)
    }

    // 2) Nutzer konfiguriert verschiedene Intervalle in Sekunden.
    const int configs[] = {15, 30, 60, 5, 1};
    for (int c : configs) {
        BuggyAgent b; b.loadFromPrefs(c);
        FixedAgent f; f.loadFromPrefs(c);
        printf("cfg=%3ds:           buggy=%lums  fixed=%lums\n",
               c, b.windowMs(), f.windowMs());
        // Jeder Nicht-1-Wert kollabiert im Bug auf 1000 ms:
        assert(b.windowMs() == 1000);
        assert(f.windowMs() == (unsigned long)c * 1000UL);
    }

    // 3) Grenzfall 0 s -> Bug: 0 ms Fenster => Poll in JEDEM loop()-Durchlauf.
    {
        BuggyAgent b; b.loadFromPrefs(0);
        FixedAgent f; f.loadFromPrefs(0);
        printf("cfg=  0s:           buggy=%lums  fixed=%lums\n", b.windowMs(), f.windowMs());
        assert(b.windowMs() == 0);   // Dauerpolling (Busy-Loop-Spam an den Host)
        assert(f.windowMs() == 0);   // im Fix ebenfalls 0 (bewusste Nutzerwahl), aber verstellbar
    }

    printf("\nAlle Assertions bestaetigt: der bool-Typ kollabiert jedes\n");
    printf("konfigurierte Intervall auf 0 s (=Dauerpoll) oder 1 s statt N Sekunden.\n");
    return 0;
}
