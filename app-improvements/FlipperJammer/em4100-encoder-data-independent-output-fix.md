# FlipperJammer: EM4100-Encoder erzeugt datenunabhaengige Ausgabe (Fix-Vorschlag)

## Was

`modules/spoofer.c`, Funktion `em4100_encode()` (RFID-Spoofer-Teil von
FlipperJammer). Vorher-Code:

```c
static uint16_t em4100_encode(uint32_t facility, uint32_t card_id, uint16_t* buf) {
    uint64_t data = ((uint64_t)0x01 << 32) |
                    ((uint64_t)(facility & 0xFF) << 24) |
                    ((uint64_t)(card_id  & 0xFFFF) << 8);

    uint16_t i = 0;

    for(int h = 0; h < 9; h++) {
        buf[i++] = 64; /* half-bit mark */
        buf[i++] = 64; /* half-bit space */
    }

    for(int bit = 39; bit >= 0; bit--) {
        uint8_t b = (uint8_t)((data >> bit) & 1);
        if(b) {
            buf[i++] = 64; /* space */
            buf[i++] = 64; /* mark  */
        } else {
            buf[i++] = 64; /* mark  */
            buf[i++] = 64; /* space */
        }
    }

    buf[i++] = 64;

    return i;
}
```

Zwei getrennte Bugs in dieser einen Funktion:

1. **Ausgabe ist unabhaengig vom Eingabewert.** Der Puffer `buf` ist ein
   reines `uint16_t`-Dauer-Array ohne separates Level-Feld. Ob ein Bit `0`
   oder `1` ist, wird korrekt in `b` berechnet — aber beide Zweige des
   `if(b) {...} else {...}` schreiben denselben Literalwert `64` an
   dieselben zwei Pufferpositionen. Der berechnete Wert `b` fliesst nirgends
   in den tatsaechlich geschriebenen Inhalt ein; nur seine Kommentare
   ("mark"/"space") unterscheiden sich, nicht die Zahlen. Damit erzeugt
   `em4100_encode(0x11, 0x2222, buf)` und `em4100_encode(0x99, 0xAAAA, buf)`
   byte-identische Puffer — die Funktion kodiert de facto keine Kartendaten,
   egal welche `facility`/`card_id`-Werte hereinkommen. Mit Testhinweis unten
   verifiziert (Test 1).
2. **Die im Funktionskommentar am Dateianfang beschriebenen 14 Zeilen-/
   Spaltenparitaetsbits fehlen komplett.** Echtes EM4100 ist ein 64-Bit-Frame
   (9 Header + 10×(4 Datenbits + 1 gerades Zeilenparitaetsbit) + 4
   Spaltenparitaetsbits + 1 Stop-Bit = 9+50+4+1 = 64 Bit). Der Vorher-Code
   liefert nur 9 Header + 40 Datenbits + 1 Stop-Bit = 50 Bit (99 statt 128
   Pufferemitrage bei 2 Eintraegen je Bit + 1 Einzeleintrag Stop). Jeder
   reale EM4100-Reader, der die Paritaet prueft (praktisch jeder), wuerde
   einen so kurzen, paritaetslosen Frame als ungueltig verwerfen — selbst
   wenn Bug 1 nicht vorlaege.

Aktuell wird der Rueckgabewert von `em4100_encode()` in
`rfid_spoofer_start()` ohnehin verworfen (`(void)n;`), der Puffer nirgends
gesendet — die Funktion ist also praktisch tote Logik. Das aendert nichts
daran, dass sie fehlerhaft ist: sobald (wie im vorhandenen Kommentar bereits
als naechster Schritt vorgesehen) eine echte Sende-Anbindung ergaenzt wird,
wuerde sie ohne diesen Fix weiterhin nur einen fixen, paritaetslosen
Blindframe erzeugen statt der gewuenschten Karten-ID.

## Nutzen

Voraussetzung dafuer, dass der RFID-Spoofer ueberhaupt jemals echte
EM4100-Kartendaten senden kann, statt bei jeder `facility`/`card_id`-Eingabe
denselben bedeutungslosen Blindframe zu erzeugen. Der Fix macht die
Encoder-Logik in sich korrekt und pruefbar (Rundtrip-Decode inkl.
Paritaetspruefung), unabhaengig vom noch offenen Schritt der eigentlichen
Sende-Anbindung.

## Betroffene Datei

- `modules/spoofer.c` — `em4100_encode()` (Funktionskoerper ersetzt),
  Aufrufstelle `rfid_spoofer_start()` (Pufferdeklaration/-typ anpassen).
- `modules/spoofer.h` — keine Aenderung noetig (Signatur der oeffentlichen
  `rfid_spoofer_start()`/`rfid_spoofer_stop()` bleibt gleich).

## Fix (Port-Skizze)

Ersetzt den reinen `uint16_t`-Dauer-Puffer durch die im selben Repo bereits
etablierte `LevelDuration`-Konvention (siehe `modules/subghz_jam.c`, das
`lib/toolbox/level_duration.h` bereits einbindet und `level_duration_make()`
verwendet) — Level wird dadurch explizit aus dem Bitwert abgeleitet statt
implizit aus der Array-Position angenommen, und der vollstaendige 64-Bit-
Frame inkl. Paritaet wird erzeugt:

```c
#include <lib/toolbox/level_duration.h>

#define EM4100_HALF_BIT_TICKS 64

/* Manchester (EM4100): Bit=1 -> High-dann-Low, Bit=0 -> Low-dann-High */
#define EM4100_EMIT_BIT(bitval)                                          \
    do {                                                                  \
        bool first_high = (bitval) ? true : false;                       \
        buf[i++] = level_duration_make(first_high, EM4100_HALF_BIT_TICKS);\
        buf[i++] = level_duration_make(!first_high, EM4100_HALF_BIT_TICKS);\
    } while(0)

static uint16_t em4100_encode(uint32_t facility, uint32_t card_id, LevelDuration* buf) {
    uint64_t data = ((uint64_t)0x01 << 32) |
                    ((uint64_t)(facility & 0xFF) << 24) |
                    ((uint64_t)(card_id  & 0xFFFF) << 8);

    uint16_t i = 0;

    /* 9 Header-Bits (alle '1') */
    for(int h = 0; h < 9; h++) EM4100_EMIT_BIT(1);

    /* 10 Zeilen a 4 Datenbits + 1 gerades Zeilenparitaetsbit,
       plus laufende Spaltenparitaet ueber alle 10 Zeilen */
    uint8_t col_parity[4] = {0, 0, 0, 0};
    for(int row = 0; row < 10; row++) {
        uint8_t nibble = (uint8_t)((data >> (36 - row * 4)) & 0xF);
        uint8_t row_parity = 0;
        for(int b = 3; b >= 0; b--) {
            uint8_t bit = (nibble >> b) & 1;
            row_parity ^= bit;
            col_parity[3 - b] ^= bit;
            EM4100_EMIT_BIT(bit);
        }
        EM4100_EMIT_BIT(row_parity);
    }

    /* 4 Spaltenparitaetsbits */
    for(int c = 0; c < 4; c++) EM4100_EMIT_BIT(col_parity[c]);

    /* 1 Stop-Bit (fest 0) */
    EM4100_EMIT_BIT(0);

    return i; /* 9 + 50 + 4 + 1 = 64 logische Bits * 2 = 128 Eintraege */
}
#undef EM4100_EMIT_BIT
```

Aufrufstelle in `rfid_spoofer_start()`:

```c
static LevelDuration rfid_buf[128]; /* vorher: uint16_t rfid_buf[256] */

void rfid_spoofer_start(RfidSpooferType type, uint32_t facility, uint32_t card_id) {
    if(rfid_spoof_running) rfid_spoofer_stop();

    (void)type;

    uint16_t n = em4100_encode(facility, card_id, rfid_buf);
    (void)n; /* Puffer ist jetzt korrekt befuellt; Sende-Anbindung an eine
                echte raw-TX-API (z.B. lfrfid_worker_start_emulate() /
                furi_hal_rfid-Rohdaten-Pfad) bleibt separater, mit eigener
                Hardware zu verifizierender Folgeschritt — siehe "Offen". */

    furi_hal_rfid_tim_read_start(125000, 0.5f);
    rfid_spoof_running = true;
}
```

Die restliche Datei (`rfid_spoofer_stop()`, NFC-Spoofer-Teil) ist von diesem
Fix nicht betroffen.

## Testhinweis

Host-kompilierbares Repro in
`app-improvements/FlipperJammer/repro-em4100-encoder/repro.c` — keine
Flipper-Header noetig, `LevelDuration` wird dort durch eine identisch
aufgebaute lokale Struktur (`LevelDurationSim { bool level; uint32_t
duration; }`) simuliert, `ALT` ist 1:1 aus dem Original uebernommen.

```
gcc -O2 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c
./repro
```

Lokal ausgefuehrt, Ausgabe:

```
[ALT] Laenge A=99 B=99
[ALT] Buffer fuer facility=0x11/card_id=0x2222 vs. facility=0x99/card_id=0xAAAA identisch: JA (Bug bestaetigt)
[ALT] Eintraege gesamt: 99 (erwartet fuer echtes EM4100: 128)
[NEU] Laenge A=128 B=128 (erwartet 128)
[NEU] Buffer fuer beide Karten identisch: NEIN (korrekt unterschiedlich)
[NEU] Decode A: ok=1 facility=0x11 card_id=0x2222 (erwartet 0x11/0x2222)
[NEU] Decode B: ok=1 facility=0x99 card_id=0xAAAA (erwartet 0x99/0xAAAA)
[NEU] Decode mit verfaelschtem Bit: ok=0 (erwartet 0, Paritaetsfehler erkannt)

Alle Tests bestanden.
```

Fuenf Pruefungen: (1) ALT erzeugt fuer zwei verschiedene Karten-IDs
byte-identische Puffer — Bug 1 bestaetigt; (2) ALT liefert nur 99 statt der
fuer ein echtes EM4100-Frame noetigen 128 Eintraege — Bug 2 bestaetigt;
(3) NEU erzeugt fuer verschiedene Karten-IDs unterschiedliche Puffer;
(4) NEU laesst sich per Rundtrip-Decoder (inkl. Zeilen-/Spaltenparitaets-
pruefung) korrekt in die urspruenglichen `facility`/`card_id`-Werte
zurueckverwandeln; (5) ein einzelnes verfaelschtes Datenbit wird vom
Decoder ueber die Paritaetspruefung zuverlaessig erkannt, was belegt, dass
die Paritaetsbits tatsaechlich aus den Daten berechnet werden und nicht nur
mitgefuehrte Konstanten sind.

## Risiko

Niedrig. Reine Berechnungs-/Datenstruktur-Aenderung innerhalb einer einzigen
Funktion plus Anpassung des Pufferdeklarationstyps an der einen
Aufrufstelle; keine Aenderung an Timing-Konstanten (125 kHz Traeger bleibt
unveraendert), keine Aenderung an oeffentlichen Funktionssignaturen. Die
eigentliche Sende-Anbindung (Puffer tatsaechlich per Raw-TX ausgeben) ist
weiterhin nicht implementiert — dieser Fix macht nur die Daten, die dabei
irgendwann gesendet wuerden, korrekt statt bedeutungslos.

## Quelle

Eigener Fund beim Tiefenaudit von `G4MEOVER18/FlipperJammer`
(`modules/spoofer.c`), aufgegriffen aus dem seit mehreren Sitzungen im
Backlog offen stehenden Punkt "FlipperJammer/lora-ukfe (eigene Apps, noch
nicht geprueft)". Restlicher Modul-Code der App (`ble_jam.c`, `fuzzer.c`,
`ir_jam.c`, `jammer_storage.c`, `nfc_jam.c`, `nrf24_jam.c`, `nrf24_spi.c`,
`rfid_jam.c`, `usb_fuzz.c`, `wifi_jam.c`, `jammer_app.c`) mitgeprueft, keine
weiteren Logikfehler dieser Kategorie gefunden — `subghz_jam.c` hat
insbesondere bereits eine saubere State-Machine ohne den in RollForges
Jammer-Modul gefundenen Doppel-Stop-Bug. Der NFC-Spoofer-Teil derselben
Datei (`nfc_spoofer_start()`) ist ebenfalls nur ein Traeger-Stoersender ohne
echte UID-Emulation, aber dort wird kein Encoder-Bug behauptet oder
begonnen (kein `(void)`-verworfener Rueckgabewert einer Encode-Funktion) —
daher kein eigener Fund in dieser Sitzung, bleibt als Randnotiz fuer eine
kuenftige NFC-Emulations-Recherche.
