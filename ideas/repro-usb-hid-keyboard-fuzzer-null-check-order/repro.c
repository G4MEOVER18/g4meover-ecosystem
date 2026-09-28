// Host-Repro: usb_hid_keyboard_process() in n-92/kb_fuzzer, views/usb_hid_keyboard.c
//
// Transkribiert 1:1 aus dem oeffentlichen Quelltext:
//   char* ptr_rand_string = malloc(random_length);
//   send_string(create_rand_string(ptr_rand_string, random_length));   // <- benutzt vor NULL-Pruefung
//   if (ptr_rand_string != NULL) { free(ptr_rand_string); }            // <- Pruefung kommt zu spaet
//
// create_rand_string() wird hier unveraendert uebernommen, malloc() wird durch einen Fehlschlag
// simuliert (calloc-Ersatz, der wie das Original nur den NULL-Fall auf Verlangen liefert).
//
// ALT  = Originalreihenfolge (Nutzung vor Pruefung)   -> Crash bei fehlgeschlagenem malloc()
// NEU  = Pruefung vor Nutzung                          -> sauberer, definierter Ausstieg
//
// Aufruf: ./repro alt   (erwartet Crash unter ASan)
//         ./repro neu   (erwartet sauberes Beenden)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char charset[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 `~!@#$%^&*()-_=+[]{};:\'\",.<>/?";

// 1:1 aus views/usb_hid_keyboard.c uebernommen
static char* create_rand_string(char* str, size_t size) {
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int)(sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

static int sends = 0;
static void send_string_stub(const char* s) {
    // steht hier fuer furi_hal_hid_kb_press()/_release() aus dem Original send_string()
    sends += (int)strlen(s);
}

// Simuliert den Alloc-Fehlschlagfall, den echtes malloc() unter Speicherdruck liefern kann
static char* failing_malloc(size_t n) {
    (void)n;
    return NULL;
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "alt";
    int random_length = 12;

    if (strcmp(mode, "alt") == 0) {
        // Originalreihenfolge: Nutzung des malloc()-Ergebnisses vor der NULL-Pruefung
        char* ptr_rand_string = failing_malloc((size_t)random_length);
        send_string_stub(create_rand_string(ptr_rand_string, (size_t)random_length));
        if (ptr_rand_string != NULL) {
            free(ptr_rand_string);
        }
        printf("ALT: kein Crash abgefangen (unerwartet)\n");
        return 0;
    } else {
        // Reihenfolge nach Fix: NULL-Pruefung vor Nutzung
        char* ptr_rand_string = failing_malloc((size_t)random_length);
        if (ptr_rand_string != NULL) {
            send_string_stub(create_rand_string(ptr_rand_string, (size_t)random_length));
            free(ptr_rand_string);
        }
        printf("NEU: malloc()-Fehlschlag sauber abgefangen, kein Absturz, sends=%d\n", sends);
        return 0;
    }
}
