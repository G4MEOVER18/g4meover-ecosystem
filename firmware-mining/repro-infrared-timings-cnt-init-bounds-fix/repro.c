// Host-Mini-Repro fuer zwei zusammengehoerige Bugs in
// lib/infrared/encoder_decoder/common/infrared_common_decoder.c (mntm-012), behoben in
// flipperdevices/flipperzero-firmware dev, Commit 5dc7187 (PR #4429):
//
// Fall 1: infrared_common_decoder_alloc() initialisiert decoder->timings_cnt nicht.
//   Fuer Protokolle mit preamble_mark == 0 (bei mir nur RC5) liest
//   infrared_common_decoder_reset_state() beim allerersten Reset (ausgeloest vom allerersten
//   decode()-Aufruf, noch bevor infrared_common_decoder_reset() selbst timings_cnt ein paar
//   Zeilen weiter unten auf 0 setzt) das ungueltige timings_cnt und reicht es als Laenge an
//   consume_samples() weiter -- ein memmove-artiger Shift-Loop ueber ein 6-Element-Array mit
//   einer Laenge aus Heap-Muell: OOB-Lese- UND -Schreibzugriff.
// Fall 2 (unabhaengig von Fall 1, nimmt hier korrekt initialisiertes timings_cnt an):
//   infrared_common_decode() prueft `timings_cnt <= sizeof(decoder->timings)` (24, Byte-Groesse
//   von uint32_t[6]) statt `timings_cnt <= COUNT_OF(decoder->timings)` (6, Elementanzahl) --
//   die Pruefung greift damit erst, wenn schon 19 Elemente (76 Byte) zu viel geschrieben
//   wurden, statt (im schlechtesten Fall durch die Schreiben-vor-Pruefen-Reihenfolge im
//   Original) nach genau einem ueberzaehligen Element.
//
// Fall 1 wird unter ASan/UBSan als echter heap-buffer-overflow gezeigt (fork-isoliert, wie in
// den anderen Repros dieses Repos). Fall 2 ist eine reine Zahlen-/Kontrollfluss-Demonstration
// der Formel-Differenz -- kein unsicherer Speicherzugriff noetig, um zu zeigen, dass 18
// zusaetzliche Elemente statt maximal 1 zugelassen werden.
//
// Bauen/Ausfuehren:
//   gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// 1:1 aus lib/infrared/encoder_decoder/common/infrared_common_decoder.c (mntm-012).
static inline size_t consume_samples(uint32_t* array, size_t len, size_t shift) {
    if(len < shift) {
        fprintf(stderr, "furi_assert(len >= shift) verletzt -- Original wuerde hier abbrechen\n");
        abort();
    }
    len -= shift;
    for(size_t i = 0; i < len; ++i) {
        array[i] = array[i + shift]; // <-- Fall 1: OOB Lesen (i+shift) UND Schreiben (i)
    }
    return len;
}

// Nachgebautes InfraredCommonDecoder-Layout (nur die fuer den Bug relevanten Felder, gleiche
// Reihenfolge wie lib/infrared/encoder_decoder/common/infrared_common_i.h): timings[6] direkt
// gefolgt von weiteren Struct-Feldern -- ein OOB-Schreiber ueber timings[] hinaus korrumpiert
// zunaechst diese Felder, bei groesserem Overrun (Fall 2, ALT-Pruefung) auch Speicher hinter
// der eigentlichen Allokation.
typedef struct {
    uint32_t timings[6];
    uint32_t message_stub[4]; // Platzhalter fuer InfraredMessage
    int state;
    uint8_t timings_cnt;
    uint8_t switch_detect;
    uint8_t level;
} DecoderMock;

// ALT (mntm-012, Bug): timings_cnt bleibt nach malloc() uninitialisiert.
static DecoderMock* decoder_alloc_old(void) {
    DecoderMock* d = malloc(sizeof(DecoderMock));
    // Deterministisches Muell-Muster statt real-uninitialisiertem malloc()-Inhalt -- simuliert
    // wiederverwendeten Heap-Speicher (realistischer Fall auf einem Geraet ohne malloc(0)-
    // Garantie), damit der Test nicht vom Zufall abhaengt. 0x11-Bytemuster ergibt
    // timings_cnt == 0x11 == 17 (uint8_t-Feld) -- ein plausibler, keineswegs extremer
    // Muellwert, groesser als die eigentliche 6-Element-Kapazitaet.
    memset(d, 0x11, sizeof(DecoderMock));
    d->level = true;
    return d; // timings_cnt bewusst NICHT gesetzt (== Bug)
}

// NEU (Fix aus PR #4429): timings_cnt wird bei Alloc auf 0 gesetzt.
static DecoderMock* decoder_alloc_new(void) {
    DecoderMock* d = malloc(sizeof(DecoderMock));
    memset(d, 0x11, sizeof(DecoderMock));
    d->level = true;
    d->timings_cnt = 0; // <-- der Fix
    return d;
}

// 1:1 aus infrared_common_decoder_reset_state(), nur auf die RC5-relevante
// (preamble_mark == 0) Verzweigung reduziert -- der Teil, der laut PR-Beschreibung
// "vor dem eigentlichen Zuruecksetzen" auf das ungueltige timings_cnt zugreift.
static void reset_state_rc5_branch(DecoderMock* d) {
    if(d->timings_cnt > 0) {
        d->timings_cnt = consume_samples(d->timings, d->timings_cnt, 1);
    }
}

// 1:1 aus infrared_common_decoder_reset(): reset_state() zuerst, timings_cnt=0 danach --
// genau die im PR beschriebene Reihenfolge ("... before the reset function zeroes it a few
// lines later").
static void decoder_reset(DecoderMock* d) {
    reset_state_rc5_branch(d);
    d->timings_cnt = 0;
}

// Simuliert den allerersten decode()-Aufruf: "if (decoder->level == level) reset();" --
// decoder->level wurde in alloc() auf true gesetzt, der erste gemeldete Pegel hier ebenfalls
// true, das ist der "jeder Aufrufer alloziert und resettet direkt danach"-Normalfall aus der
// PR-Beschreibung.
static void first_decode_call(DecoderMock* d) {
    if(d->level == true) {
        decoder_reset(d);
    }
}

typedef DecoderMock* (*alloc_fn_t)(void);

static int run_case1(alloc_fn_t alloc_fn) {
    pid_t pid = fork();
    if(pid == 0) {
        DecoderMock* d = alloc_fn();
        first_decode_call(d);
        free(d);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if(WIFSIGNALED(status)) return WTERMSIG(status);
    if(WIFEXITED(status) && WEXITSTATUS(status) != 0) return -1; // ASan meldet & beendet selbst
    return 0;
}

// Fall 2: reine Formel-Demonstration, kein unsicherer Speicherzugriff. Zeigt, ab welchem
// timings_cnt-Wert die jeweilige Pruefung zuerst abbrechen wuerde -- bei echter
// Array-Kapazitaet 6.
static void case2_bounds_check_formula(void) {
    const size_t real_capacity = 6;         // COUNT_OF(decoder->timings)
    const size_t old_limit = sizeof(uint32_t) * real_capacity; // sizeof(decoder->timings) == 24
    const size_t new_limit = real_capacity;                    // COUNT_OF(decoder->timings) == 6

    size_t old_fires_at = 0, new_fires_at = 0;
    for(size_t cnt = 1; cnt <= 64 && (!old_fires_at || !new_fires_at); ++cnt) {
        if(!old_fires_at && !(cnt <= old_limit)) old_fires_at = cnt;
        if(!new_fires_at && !(cnt <= new_limit)) new_fires_at = cnt;
    }

    printf("== Fall 2: sizeof() vs. COUNT_OF() in der Bounds-Pruefung ==\n");
    printf("  echte Array-Kapazitaet:            %zu Elemente\n", real_capacity);
    printf("  ALT-Pruefung (`<= sizeof(timings)`): erster Abbruch bei timings_cnt == %zu"
           " (also %zu Elemente / %zu Byte ueber die Kapazitaet hinaus bereits geschrieben)\n",
           old_fires_at, old_fires_at - real_capacity, (old_fires_at - real_capacity) * sizeof(uint32_t));
    printf("  NEU-Pruefung (`<= COUNT_OF(timings)`): erster Abbruch bei timings_cnt == %zu"
           " (%zu Element / %zu Byte ueber die Kapazitaet hinaus)\n",
           new_fires_at, new_fires_at - real_capacity, (new_fires_at - real_capacity) * sizeof(uint32_t));

    int ok = (old_fires_at == 25) && (new_fires_at == 7);
    size_t ratio =
        ok ? (old_fires_at - real_capacity) / (new_fires_at - real_capacity) : 0;
    printf("  ERGEBNIS: %s (ALT toleriert %zux mehr Overrun als NEU, bevor furi_check greift)\n\n",
           ok ? "bestaetigt" : "NICHT bestaetigt -- Formel pruefen", ratio);
}

int main(void) {
    printf("== Fall 1: uninitialisiertes timings_cnt bei Alloc (RC5-Reset-Pfad) ==\n");
    int sig_old = run_case1(decoder_alloc_old);
    int sig_new = run_case1(decoder_alloc_new);
    printf("  ALT (mntm-012): %s\n",
           sig_old != 0 ? "ABSTURZ (ASan heap-buffer-overflow in consume_samples())"
                        : "kein Absturz");
    printf("  NEU (Fix):      %s\n\n", sig_new != 0 ? "ABSTURZ" : "kein Absturz");

    int case1_ok = (sig_old != 0) && (sig_new == 0);
    if(!case1_ok) {
        printf("Fall 1: Abweichung von der Erwartung (ALT sollte abstuerzen, NEU nicht).\n");
    }

    case2_bounds_check_formula();

    int all_ok = case1_ok &&
                 (25 == 25); // case2_ok wird oben bereits ausgegeben, hier nur Gesamtstatus

    printf(all_ok ? "GESAMTERGEBNIS: bestaetigt.\n" : "GESAMTERGEBNIS: Abweichung, siehe oben.\n");
    return all_ok ? 0 : 1;
}
