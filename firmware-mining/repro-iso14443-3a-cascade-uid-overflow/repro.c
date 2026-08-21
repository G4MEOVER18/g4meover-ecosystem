// Host-Repro: iso14443_3a_poller_i.c, Kollisionsaufloesungsschleife
// (Iso14443_3aPollerColResStateStateSelectCascade-Zweig). Bei mir (mntm-012) entscheidet die
// Schleife ausschliesslich anhand von "sel_req.nfcid[0] == 0x88" (Kaskadierungs-Tag-Byte), ob
// noch eine weitere Kaskadenstufe folgt -- dieses Byte stammt unveraendert aus der SDD-Antwort
// des Tags (sdd_resp.nfcid[0]), ist also vollstaendig tag-/angreifer-kontrolliert. Es gibt KEINE
// Obergrenze fuer cascade_level: ein Tag, das immer wieder 0x88 als erstes NFCID-Byte liefert,
// haelt die Schleife beliebig lange am Laufen, und jede Runde haengt bedingungslos 3 weitere
// Byte an data->uid[] an. ISO14443-3 definiert hoechstens 3 Kaskadenstufen (10-Byte-UID:
// 3+3+4), das Datenlayout unten ist 1:1 aus iso14443_3a.h uebernommen (uid[10], uid_len,
// atqa[2], sak, in dieser Reihenfolge) -- ab der 4. Kaskadenstufe (cascade_level == 3) schreibt
// der bedingungslose memcpy(&uid[uid_len], ..., 3) hinter das 10-Byte-Array in uid_len/atqa/sak
// und danach in den Heap dahinter, mit vom Tag kontrolliertem Inhalt.
//
// ALT ist 1:1 der Entscheidungs-/Kopierzweig aus iso14443_3a_poller_i.c uebernommen (nur auf die
// fuer den Bug relevanten Felder reduziert, keine sonstige Aenderung). NEU ergaenzt einzig eine
// Kaskadenstufen-Obergrenze (cascade_level >= 2 -> Abbruch als Iso14443_3aErrorColResFailed),
// unabhaengig von der 0x88-Erkennung selbst (die bleibt unveraendert, siehe
// iso14443-3a-cascade-uid-overflow.md fuer die Einordnung ggu. der separaten Unleashed-Fixes).
//
// Normalbetrieb (3 aufeinanderfolgende Tags mit 4/7/10-Byte-UID, korrektes Terminierungsbyte)
// muss unter beiden Varianten identisch und ohne ASan-Fund durchlaufen:
//   gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
//
// Mit -DDEMO_HOSTILE_TAG_OLD wird ein Tag simuliert, der bei jeder Kaskadenstufe erneut 0x88
// liefert (nie terminiert) -- auf einem exakt 10-Byte grossen Heap-Puffer fuer uid[] (identisch
// zur echten Feldgroesse) muss das unter ASan mit heap-buffer-overflow abbrechen:
//   gcc -O0 -Wall -Wextra -fsanitize=address,undefined -DDEMO_HOSTILE_TAG_OLD -o repro_bug repro.c && ./repro_bug
//
// Mit -DDEMO_HOSTILE_TAG_NEW derselbe hostile Tag gegen die gefixte Variante -- muss sauber mit
// Iso14443_3aErrorColResFailed nach der 3. Kaskadenstufe abbrechen, kein ASan-Fund:
//   gcc -O0 -Wall -Wextra -fsanitize=address,undefined -DDEMO_HOSTILE_TAG_NEW -o repro_fixed repro.c && ./repro_fixed

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISO14443_3A_UID_10_BYTES (10U)
#define ISO14443_3A_POLLER_SDD_CL (0x88U)

typedef enum {
    Iso14443_3aErrorNone,
    Iso14443_3aErrorColResFailed,
} Iso14443_3aError;

// Layout 1:1 aus iso14443_3a.h (Iso14443_3aData), nur auf die relevanten Felder reduziert.
typedef struct {
    uint8_t uid[ISO14443_3A_UID_10_BYTES];
    uint8_t uid_len;
    uint8_t atqa[2];
    uint8_t sak;
} Iso14443_3aData;

// Liefert die naechste SDD-Antwort eines simulierten Tags: nfcid[0] entscheidet ueber
// Kaskadenfortsetzung (0x88 = "weiter"), nfcid[1..3] sind der eigentliche UID-Teil.
typedef struct {
    uint8_t nfcid[4];
} SddResp;

static SddResp fake_tag_step(int step, int total_steps, bool hostile_never_terminate) {
    SddResp r;
    if(hostile_never_terminate) {
        // Angreifer-kontrollierter Tag: immer 0x88, terminiert nie von sich aus.
        r.nfcid[0] = ISO14443_3A_POLLER_SDD_CL;
        r.nfcid[1] = 0xAA;
        r.nfcid[2] = 0xBB;
        r.nfcid[3] = 0xCC;
        return r;
    }
    // Normalbetrieb: letzte Stufe terminiert (kein 0x88 als erstes Byte).
    bool is_last = (step == total_steps - 1);
    r.nfcid[0] = is_last ? 0x04 : ISO14443_3A_POLLER_SDD_CL;
    r.nfcid[1] = (uint8_t)(0x10 + step);
    r.nfcid[2] = (uint8_t)(0x20 + step);
    r.nfcid[3] = (uint8_t)(0x30 + step);
    return r;
}

// ALT: iso14443_3a_poller_i.c, Iso14443_3aPollerColResStateStateSelectCascade-Zweig
// (unveraendert -- keine Kaskadenstufen-Obergrenze). "sim_total_steps" ist nur ein Parameter
// der Tag-Simulation (wann ein NICHT-hostiler Tag terminiert), keine Groesse im echten Code.
static Iso14443_3aError
    old_resolve_collision(Iso14443_3aData* data, int sim_total_steps, bool hostile) {
    int cascade_level = 0;
    data->uid_len = 0;
    for(int step = 0;; step++) {
        SddResp sel_req = fake_tag_step(step, sim_total_steps, hostile);
        if(sel_req.nfcid[0] == ISO14443_3A_POLLER_SDD_CL) {
            // Copy part of UID -- keine Pruefung von cascade_level.
            memcpy(&data->uid[data->uid_len], &sel_req.nfcid[1], 3);
            data->uid_len += 3;
            cascade_level++;
        } else {
            memcpy(&data->uid[data->uid_len], &sel_req.nfcid[0], 4);
            data->uid_len += 4;
            return Iso14443_3aErrorNone;
        }
        if(hostile && step > 10) return Iso14443_3aErrorNone; // Sicherheitsnetz fuer den Host-Repro
    }
}

// NEU: einzige Aenderung ist die Kaskadenstufen-Obergrenze (ISO14443-3: max. 3 Stufen,
// cascade_level in {0,1,2}), unabhaengig von der 0x88-Erkennung selbst.
static Iso14443_3aError
    new_resolve_collision(Iso14443_3aData* data, int sim_total_steps, bool hostile) {
    int cascade_level = 0;
    data->uid_len = 0;
    for(int step = 0;; step++) {
        SddResp sel_req = fake_tag_step(step, sim_total_steps, hostile);
        if(sel_req.nfcid[0] == ISO14443_3A_POLLER_SDD_CL) {
            if(cascade_level >= 2) {
                return Iso14443_3aErrorColResFailed;
            }
            memcpy(&data->uid[data->uid_len], &sel_req.nfcid[1], 3);
            data->uid_len += 3;
            cascade_level++;
        } else {
            memcpy(&data->uid[data->uid_len], &sel_req.nfcid[0], 4);
            data->uid_len += 4;
            return Iso14443_3aErrorNone;
        }
    }
}

int main(void) {
#if defined(DEMO_HOSTILE_TAG_OLD)
    // Exakt 14 Byte, wie das echte Iso14443_3aData-Layout (uid[10]+uid_len+atqa[2]+sak) --
    // ASan muss den Schreibzugriff der 4. Kaskadenstufe (Byte 9,10,11 relativ zum uid[]-Anfang)
    // als heap-buffer-overflow melden.
    Iso14443_3aData* data = malloc(sizeof(Iso14443_3aData));
    Iso14443_3aError err = old_resolve_collision(data, 3, true);
    printf("ALT (hostile Tag): err=%d uid_len=%u (sollte hier nie ankommen)\n", err, data->uid_len);
    free(data);
    return 0;
#elif defined(DEMO_HOSTILE_TAG_NEW)
    Iso14443_3aData* data = malloc(sizeof(Iso14443_3aData));
    Iso14443_3aError err = new_resolve_collision(data, 3, true);
    if(err != Iso14443_3aErrorColResFailed) {
        fprintf(stderr, "FEHLER: gefixte Variante haette ColResFailed liefern muessen\n");
        free(data);
        return 1;
    }
    printf("NEU (hostile Tag): sauber mit Iso14443_3aErrorColResFailed abgebrochen, kein Overflow\n");
    free(data);
    return 0;
#else
    // Normalbetrieb, beide Varianten muessen identisch funktionieren: 4-, 7- und 10-Byte-UID
    // (1, 2, 3 Kaskadenstufen), jeweils korrekt terminiert.
    for(int total_steps = 1; total_steps <= 3; total_steps++) {
        Iso14443_3aData old_data, new_data;
        memset(&old_data, 0, sizeof(old_data));
        memset(&new_data, 0, sizeof(new_data));

        Iso14443_3aError old_err = old_resolve_collision(&old_data, total_steps, false);
        Iso14443_3aError new_err = new_resolve_collision(&new_data, total_steps, false);
        if(old_err != Iso14443_3aErrorNone || new_err != Iso14443_3aErrorNone ||
           new_data.uid_len != old_data.uid_len ||
           memcmp(new_data.uid, old_data.uid, old_data.uid_len) != 0) {
            fprintf(
                stderr,
                "FEHLER bei total_steps=%d: ALT und NEU weichen im Normalbetrieb voneinander ab\n",
                total_steps);
            return 1;
        }
        printf(
            "total_steps=%d (UID-Laenge %u): ALT und NEU identisch, kein Verhaltensunterschied\n",
            total_steps,
            new_data.uid_len);
    }
    printf("OK: Normalbetrieb unveraendert, Obergrenze greift nur beim hostile-Tag-Fall.\n");
    return 0;
#endif
}
