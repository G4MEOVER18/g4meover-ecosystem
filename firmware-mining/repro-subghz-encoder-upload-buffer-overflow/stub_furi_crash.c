#include <unistd.h>

// Stub for the firmware's furi_crash(): instead of the real halt-and-catch-fire,
// exit with a fixed, distinguishable code so the test harness can tell "the
// guard fired" apart from "returned cleanly" and from an ASan trap.
void furi_crash(const char* message) {
    (void)message;
    _exit(42);
}
