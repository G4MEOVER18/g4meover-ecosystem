// Host-repro: bit_lib_get_bits() liest ein Byte hinter dem Puffer, wenn die
// angefragten Bits unausgerichtet sind, aber vollstaendig im aktuellen Byte
// liegen. ALT/NEU sind 1:1 aus lib/bit_lib/bit_lib.c uebernommen (nur der
// Funktionsname ist pro Variante eindeutig gemacht, keine sonstige Aenderung).
//
// Standardbuild prueft nur die NEUE (gefixte) Variante -- muss unter
// ASan/UBSan sauber durchlaufen und exakt den erwarteten Wert liefern:
//   gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
//
// Mit -DDEMO_OLD_BUG wird zusaetzlich zuerst die ALTE (fehlerhafte) Variante
// mit identischer Eingabe aufgerufen -- muss unter ASan mit
// heap-buffer-overflow abbrechen (Beleg des Bugs vor dem Fix):
//   gcc -O0 -Wall -Wextra -fsanitize=address,undefined -DDEMO_OLD_BUG -o repro_bug repro.c && ./repro_bug

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// ALT: lib/bit_lib/bit_lib.c, Stand mntm-012 (Bug, TODO FL-3534)
static uint8_t bit_lib_get_bits_old(const uint8_t* data, size_t position, uint8_t length) {
    uint8_t shift = position % 8;
    if(shift == 0) {
        return data[position / 8] >> (8 - length);
    } else {
        uint8_t value = (uint8_t)(data[position / 8] << (shift));
        value |= data[position / 8 + 1] >> (8 - shift); // <-- liest ggf. hinter dem Puffer
        value = value >> (8 - length);
        return value;
    }
}

// NEU: Fix aus flipperdevices/flipperzero-firmware dev, Commit 8713d1c4 (PR #4424)
static uint8_t bit_lib_get_bits_new(const uint8_t* data, size_t position, uint8_t length) {
    uint8_t shift = position % 8;
    if(shift == 0) {
        return data[position / 8] >> (8 - length);
    } else if(shift + length <= 8) {
        return (uint8_t)(data[position / 8] << shift) >> (8 - length);
    } else {
        uint8_t value = (uint8_t)(data[position / 8] << (shift));
        value |= data[position / 8 + 1] >> (8 - shift);
        value = value >> (8 - length);
        return value;
    }
}

int main(void) {
    // Minimalfall: 1-Byte-Puffer, exakt so gross wie fuer die Anwendung
    // gebraucht (kein Sicherheitsabstand danach) -- genau die Situation am
    // Ende eines LFRFID/NFC-Decode-Puffers, deren Groesse exakt auf die
    // erwartete Bitzahl zugeschnitten ist.
    uint8_t* buf = malloc(1);
    buf[0] = 0b10110100;

    // position=4, length=4: die letzten 4 Bit des einzigen Bytes -- passt
    // vollstaendig in das aktuelle Byte (shift=4, length=4, shift+length=8).
#ifdef DEMO_OLD_BUG
    printf("== ALT (erwartet: ASan heap-buffer-overflow, Programm bricht ab) ==\n");
    uint8_t old_value = bit_lib_get_bits_old(buf, 4, 4);
    printf("Rueckgabewert (ALT): 0x%X -- diese Zeile sollte unter ASan nie erreicht werden.\n",
           old_value);
#endif

    printf("== NEU ==\n");
    uint8_t new_value = bit_lib_get_bits_new(buf, 4, 4);
    printf("Rueckgabewert (NEU): 0x%X, kein Zugriff auf buf[1] (ausserhalb der 1-Byte-Allokation).\n",
           new_value);

    if(new_value != (uint8_t)(0b0100)) {
        printf("FEHLER: unerwarteter Rueckgabewert, erwartet 0x4\n");
        free(buf);
        return 1;
    }

    printf("BESTAETIGT: NEU liefert den erwarteten Wert (0x4) ohne Pufferzugriff ausserhalb.\n");

    free(buf);
    return 0;
}
