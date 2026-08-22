// Host-Repro: lib/nfc/protocols/emv/emv_poller_i.c, emv_decode_tlv_tag() -- bei mir (mntm-012)
// unveraendert. tlen kommt ungeprueft aus emv_parse_tag() (buff[i], ein einzelnes Response-Byte
// der Karte) und wird fuer mehrere Tags direkt als memcpy()-Laenge in einen fest dimensionierten
// Stack-/Heap-Puffer verwendet, ohne gegen dessen Groesse geprueft zu werden. Zwei der Faelle
// werden hier host-seitig nachgebildet:
//
//   Fall A -- EMV_TAG_TRACK_1_EQUIV (0x56): "char track_1_equiv[80];" gefolgt von
//   "memcpy(track_1_equiv, &buff[i], tlen);" -- tlen ist ein uint8_t, kann also bis 255
//   betragen, unabhaengig von der 80-Byte-Kapazitaet des Puffers. Identisch bei mir und im
//   Unleashed-Vorher-Stand.
//
//   Fall B -- EMV_TAG_TRACK_2_EQUIV/EMV_TAG_TRACK_2_DATA (0x57/0x9F6B), ASCII-Konvertierung:
//   "char track_2_equiv[41];" gefolgt von einer Schleife, die pro Eingabe-Byte ZWEI ASCII-Zeichen
//   schreibt ("for(x = 0; x < tlen; x++) { track_2_equiv[x*2] = ...; track_2_equiv[x*2+1] = ...; }").
//   Bei stock Unleashed steht dieser Block hinter "#ifndef LOGS_RELEASE_BUILD" und existiert in
//   einem Release-Build gar nicht; bei mir (Momentum mntm-012, gegengeprueft) fehlt genau dieser
//   Guard -- der Block ist bei mir in JEDEM Build aktiv, das macht diesen Fall für mich
//   schwerwiegender als fuer einen Unleashed-Release-Build.
//
// ALT ist 1:1 die bei mir vorhandene Logik (unbounded memcpy/Schreibschleife). NEU ist die
// gebundene Fassung aus Unleashed-Commit 15e208c0 ("[NFC] Bound card-controlled TLV lengths in
// the EMV poller", PR #1048): Fall A lehnt tlen >= sizeof(dest) komplett ab (Tag verworfen,
// gleiche Semantik wie die anderen in diesem Commit gebundenen memcpy-Faelle), Fall B bricht die
// Schreibschleife ab, sobald das naechste Zeichenpaar nicht mehr passt.
//
// Jeder Testfall laeuft isoliert in einem fork()-Kindprozess (ein SEGV/ASan-Abbruch eines Falls
// darf den Testlauf selbst nicht abbrechen), kompiliert mit ASan/UBSan, das den
// Stack-Buffer-Overflow zuverlaessig erkennt.
//
//   gcc -O0 -g -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// ---- Fall A: EMV_TAG_TRACK_1_EQUIV ----

// ALT: emv_decode_tlv_tag(), case EMV_TAG_TRACK_1_EQUIV -- bei mir (mntm-012) unveraendert.
static void old_track1(const uint8_t* buff, uint8_t tlen) {
    char track_1_equiv[80];
    memcpy(track_1_equiv, buff, tlen);
    track_1_equiv[tlen] = '\0';
    printf("  ALT Fall A: memcpy(%d Byte) in track_1_equiv[80] abgeschlossen, kein Absturz\n", tlen);
}

// NEU: Unleashed-Fassung ab Commit 15e208c0 -- Laenge gegen die Zielpuffergroesse geprueft,
// Tag bei Ueberschreitung verworfen statt geschrieben (emv_tag_rejected()-Verhalten).
static bool new_track1(const uint8_t* buff, uint8_t tlen) {
    char track_1_equiv[80];
    if(tlen >= sizeof(track_1_equiv)) {
        printf(
            "  NEU Fall A: tlen=%d >= sizeof(track_1_equiv)=%zu -> Tag verworfen, kein Schreibzugriff\n",
            tlen,
            sizeof(track_1_equiv));
        return false;
    }
    memcpy(track_1_equiv, buff, tlen);
    track_1_equiv[tlen] = '\0';
    printf("  NEU Fall A: memcpy(%d Byte) in track_1_equiv[80] abgeschlossen, kein Absturz\n", tlen);
    return true;
}

// ---- Fall B: EMV_TAG_TRACK_2_EQUIV ASCII-Konvertierung ----

// ALT: emv_decode_tlv_tag(), case EMV_TAG_TRACK_2_DATA/EMV_TAG_TRACK_2_EQUIV -- bei mir
// (Momentum mntm-012) ohne "#ifndef LOGS_RELEASE_BUILD"-Guard in jedem Build aktiv.
static void old_track2_ascii(const uint8_t* buff, uint8_t tlen) {
    char track_2_equiv[41];
    uint8_t track_2_equiv_len = 0;
    for(int x = 0; x < tlen; x++) {
        char top = (buff[x] >> 4) + '0';
        char bottom = (buff[x] & 0x0F) + '0';
        track_2_equiv[x * 2] = top;
        track_2_equiv_len++;
        if(top == '?') break;
        track_2_equiv[x * 2 + 1] = bottom;
        track_2_equiv_len++;
        if(bottom == '?') break;
    }
    track_2_equiv[track_2_equiv_len] = '\0';
    printf(
        "  ALT Fall B: %d Eingabe-Byte -> %d ASCII-Zeichen in track_2_equiv[41] geschrieben, kein Absturz\n",
        tlen,
        track_2_equiv_len);
}

// NEU: Unleashed-Fassung ab Commit 15e208c0 -- Schleife bricht ab, sobald das naechste
// Zeichenpaar (2 Byte) nicht mehr in track_2_equiv[41] passt (ein Byte bleibt fuer den
// Nullterminator reserviert).
static bool new_track2_ascii(const uint8_t* buff, uint8_t tlen) {
    char track_2_equiv[41];
    uint8_t track_2_equiv_len = 0;
    int x;
    for(x = 0; x < tlen && (size_t)(x * 2 + 2) < sizeof(track_2_equiv); x++) {
        char top = (buff[x] >> 4) + '0';
        char bottom = (buff[x] & 0x0F) + '0';
        track_2_equiv[x * 2] = top;
        track_2_equiv_len++;
        if(top == '?') break;
        track_2_equiv[x * 2 + 1] = bottom;
        track_2_equiv_len++;
        if(bottom == '?') break;
    }
    track_2_equiv[track_2_equiv_len] = '\0';
    bool truncated = (x < tlen);
    printf(
        "  NEU Fall B: %d Eingabe-Byte -> %d ASCII-Zeichen in track_2_equiv[41] geschrieben%s, kein Absturz\n",
        tlen,
        track_2_equiv_len,
        truncated ? " (an Puffergrenze abgebrochen)" : "");
    return true;
}

typedef void (*OldFn)(const uint8_t*, uint8_t);
typedef bool (*NewFn)(const uint8_t*, uint8_t);

// Fuehrt fn in einem Kindprozess aus; gibt zurueck, ob der Kindprozess ungewoehnlich beendet
// wurde (per Signal, z. B. SEGV, ODER per ASan-Fehler-Exit -- ASAN_OPTIONS=abort_on_error=0
// ist auf dieser Plattform Default, ASan beendet dann per _exit(1) statt per Signal).
static bool run_isolated_old(const char* label, OldFn fn, const uint8_t* buff, uint8_t tlen) {
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        fn(buff, tlen);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool crashed = WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
    printf(
        "%s: %s\n",
        label,
        crashed ? "ABSTURZ (Stack-Buffer-Overflow, von ASan erkannt)" : "kein Absturz");
    return crashed;
}

static bool run_isolated_new(const char* label, NewFn fn, const uint8_t* buff, uint8_t tlen) {
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        fn(buff, tlen);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool crashed = WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
    printf("%s: %s\n", label, crashed ? "ABSTURZ" : "kein Absturz");
    return crashed;
}

int main(void) {
    uint8_t card_buf[255];
    for(int i = 0; i < 255; i++) card_buf[i] = (uint8_t)(0x30 + (i % 10)); // harmlose ASCII-Ziffern

    int failures = 0;

    printf("== Fall A: EMV_TAG_TRACK_1_EQUIV (Ziel: track_1_equiv[80]) ==\n");
    // Realistischer Track-1-Wert (Name+PAN+Datum, typ. < 79 Byte): ALT und NEU verhalten sich
    // identisch (kein Overflow, kein Ablehnen).
    if(run_isolated_old("  ALT, tlen=37 (realistischer Track-1-Wert)", old_track1, card_buf, 37))
        failures++;
    if(run_isolated_new("  NEU, tlen=37 (realistischer Track-1-Wert)", new_track1, card_buf, 37))
        failures++;
    // Kartenkontrollierter Maximalwert (tlen bis 255, da uint8_t): ALT ueberschreibt den
    // 80-Byte-Stackpuffer um bis zu 175 Byte mit kartenkontrolliertem Inhalt.
    if(!run_isolated_old("  ALT, tlen=255 (kartenkontrollierter Maximalwert)", old_track1, card_buf, 255)) {
        fprintf(stderr, "FEHLER: ALT Fall A haette bei tlen=255 unter ASan abstuerzen muessen\n");
        failures++;
    }
    if(run_isolated_new("  NEU, tlen=255 (kartenkontrollierter Maximalwert)", new_track1, card_buf, 255)) {
        fprintf(stderr, "FEHLER: NEU Fall A haette bei tlen=255 nicht abstuerzen duerfen\n");
        failures++;
    }

    printf("\n== Fall B: EMV_TAG_TRACK_2 ASCII-Konvertierung (Ziel: track_2_equiv[41]) ==\n");
    printf("(bei mir/Momentum ohne LOGS_RELEASE_BUILD-Guard in jedem Build aktiv)\n");
    // Realistischer Track-2-Wert (PAN+Trenner+Datum, typ. <= 19 Byte): identisch bei ALT/NEU.
    if(run_isolated_old("  ALT, tlen=19 (realistischer Track-2-Wert)", old_track2_ascii, card_buf, 19))
        failures++;
    if(run_isolated_new("  NEU, tlen=19 (realistischer Track-2-Wert)", new_track2_ascii, card_buf, 19))
        failures++;
    // Kartenkontrollierter Maximalwert: jedes Eingabe-Byte erzeugt 2 Ausgabe-Byte, tlen=255
    // -> bis zu 510 Byte Schreibversuch in einen 41-Byte-Puffer.
    if(!run_isolated_old("  ALT, tlen=255 (kartenkontrollierter Maximalwert)", old_track2_ascii, card_buf, 255)) {
        fprintf(stderr, "FEHLER: ALT Fall B haette bei tlen=255 unter ASan abstuerzen muessen\n");
        failures++;
    }
    if(run_isolated_new("  NEU, tlen=255 (kartenkontrollierter Maximalwert)", new_track2_ascii, card_buf, 255)) {
        fprintf(stderr, "FEHLER: NEU Fall B haette bei tlen=255 nicht abstuerzen duerfen\n");
        failures++;
    }

    printf("\n");
    if(failures == 0) {
        printf(
            "OK: beide Faelle bestaetigt -- ALT stuerzt bei kartenkontrolliertem tlen=255 in "
            "beiden Faellen unter ASan ab (Stack-Buffer-Overflow mit kartenkontrolliertem "
            "Inhalt), NEU (Laengenpruefung/abgebrochene Schreibschleife aus Commit 15e208c0) "
            "nicht. Fuer realistische, spezifikationskonforme Werte verhalten sich ALT und NEU "
            "identisch (kein Verhaltensunterschied im Erfolgsfall).\n");
        return 0;
    } else {
        fprintf(stderr, "FEHLGESCHLAGEN: %d Erwartung(en) nicht erfuellt\n", failures);
        return 1;
    }
}
