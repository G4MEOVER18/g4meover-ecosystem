// Host-Repro: Rolling-Counter-Reserve-Mechanismus fuer die G4MEOVER RF Console
// (lora-ukfe, Flipper-Seite, rf/rf_comm.c). Reine Zustandslogik, kein Storage-/
// CC1101-Zugriff -- bildet den in rf_comm.c neu eingefuehrten Reserve-Mechanismus
// 1:1 nach (rf_counter_initial_reserve/rf_counter_needs_bump aus dem Patch) sowie
// die Heltec-seitige Anti-Replay-Pruefung aus ukfe_rf_parse_frame()
// ("counter <= last_counter -> ablehnen").
//
// Zeigt:
//   1) BUG (Ist-Zustand, kein Reserve-Mechanismus): nach jedem Flipper-App-Neustart
//      lehnt der Heltec so viele Kommandos ab, wie in der vorherigen Sitzung
//      gesendet wurden (lokaler Counter startet immer wieder bei 0, Heltecs
//      last_counter bleibt bestehen).
//   2) FIX: nach Neustart wird ohne eine einzige Ablehnung sofort weitergesendet,
//      weil der lokale Counter aus einer bereits vorreservierten, persistierten
//      oberen Schranke fortgesetzt wird statt bei 0.
//   3) FIX uebersteht auch einen unsauberen Absturz (kein finales Speichern beim
//      Beenden) dank Vorreservierung mit Sicherheitsspanne.
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define RF_COUNTER_MARGIN 200u

// ---- Reine Logik aus rf_comm.c (Patch), unveraendert uebernommen ----
static uint32_t rf_counter_initial_reserve(uint32_t loaded, uint32_t margin) {
    return loaded + margin;
}
static bool rf_counter_needs_bump(uint32_t counter, uint32_t reserved_up_to, uint32_t margin, uint32_t* new_reserved_up_to) {
    if(counter + margin / 2 < reserved_up_to) return false;
    *new_reserved_up_to = counter + margin;
    return true;
}

// ---- Heltec-seitige Pruefung aus ukfe_rf_parse_frame() (nur die Counter-Regel) ----
static bool heltec_accept(uint32_t counter, uint32_t* last_counter) {
    if(counter <= *last_counter) return false;
    *last_counter = counter;
    return true;
}

// ---- Modell "Flipper-Sitzung", BUG-Variante: kein Storage, Counter startet immer bei 0 ----
static uint32_t bug_session_rejections(uint32_t sends_this_session, uint32_t* heltec_last_counter) {
    uint32_t local_counter = 0;
    uint32_t rejected = 0;
    for(uint32_t i = 0; i < sends_this_session; i++) {
        local_counter++;
        if(!heltec_accept(local_counter, heltec_last_counter)) rejected++;
    }
    return rejected;
}

// ---- Modell "Flipper-Sitzung", FIX-Variante: mit Reserve-Persistenz ----
typedef struct {
    uint32_t counter;
    uint32_t reserved_up_to;
} FixSession;

// simuliert rf_comm_init(): laedt "gespeicherten" Stand, reserviert sofort vor
static FixSession fix_session_init(uint32_t loaded_from_storage, uint32_t* storage_out) {
    FixSession s;
    s.counter = loaded_from_storage;
    s.reserved_up_to = rf_counter_initial_reserve(loaded_from_storage, RF_COUNTER_MARGIN);
    *storage_out = s.reserved_up_to;
    return s;
}

// simuliert rf_comm_send(): inkrementiert, bumpt+speichert bei Bedarf
static uint32_t fix_session_send(FixSession* s, uint32_t* storage_out) {
    s->counter++;
    uint32_t new_reserve;
    if(rf_counter_needs_bump(s->counter, s->reserved_up_to, RF_COUNTER_MARGIN, &new_reserve)) {
        s->reserved_up_to = new_reserve;
        *storage_out = new_reserve;
    }
    return s->counter;
}

int main(void) {
    // ---- Szenario 1: BUG -- 50 Kommandos in Sitzung 1, dann Flipper-App-Neustart ----
    {
        uint32_t heltec_last = 0;
        uint32_t rejected_s1 = bug_session_rejections(50, &heltec_last);
        assert(rejected_s1 == 0);          // Sitzung 1 (frische Paarung) laeuft fehlerfrei
        assert(heltec_last == 50);

        uint32_t rejected_s2 = bug_session_rejections(60, &heltec_last);
        // Erste 50 Sends (Counter 1..50) werden abgelehnt (<= 50), erst ab 51 wieder akzeptiert
        assert(rejected_s2 == 50);
        printf("BUG:  Sitzung 2 nach Neustart: %u von 60 Kommandos abgelehnt (Geraet effektiv "
               "unbenutzbar bis Counter aufgeholt hat)\n", rejected_s2);
    }

    // ---- Szenario 2: FIX -- gleiche Abfolge, jetzt mit Reserve-Persistenz ----
    {
        uint32_t heltec_last = 0;
        uint32_t storage = 0;   // "Flash-Datei", Startzustand: nichts gespeichert

        FixSession s1 = fix_session_init(storage, &storage);
        assert(storage == RF_COUNTER_MARGIN);
        uint32_t rejected_s1 = 0;
        for(int i = 0; i < 50; i++) {
            uint32_t c = fix_session_send(&s1, &storage);
            if(!heltec_accept(c, &heltec_last)) rejected_s1++;
        }
        assert(rejected_s1 == 0);
        assert(heltec_last == 50);

        // Sitzung 2 nach Neustart: laedt den zuletzt gespeicherten Reserve-Stand (200),
        // NICHT den zuletzt tatsaechlich gesendeten Wert (50) -> garantiert > heltec_last
        FixSession s2 = fix_session_init(storage, &storage);
        assert(s2.counter == 200);
        uint32_t rejected_s2 = 0;
        for(int i = 0; i < 60; i++) {
            uint32_t c = fix_session_send(&s2, &storage);
            if(!heltec_accept(c, &heltec_last)) rejected_s2++;
        }
        assert(rejected_s2 == 0);   // KEIN einziges Kommando wird nach dem Neustart abgelehnt
        printf("FIX:  Sitzung 2 nach Neustart: %u von 60 Kommandos abgelehnt\n", rejected_s2);
    }

    // ---- Szenario 3: FIX ueberlebt unsauberen Absturz (kein finales Speichern) ----
    {
        uint32_t heltec_last = 0;
        uint32_t storage = 0;

        FixSession s1 = fix_session_init(storage, &storage);   // reserved_up_to=200, gespeichert
        uint32_t max_counter_used = 0;
        for(int i = 0; i < 150; i++) {                          // ueberschreitet die Bump-Schwelle (100)
            max_counter_used = fix_session_send(&s1, &storage);
            heltec_accept(max_counter_used, &heltec_last);
        }
        // Bump muss bei Counter>=100 ausgeloest haben (margin/2=100 vor reserved_up_to=200)
        assert(storage > 200);
        assert(storage >= max_counter_used + RF_COUNTER_MARGIN / 2);
        uint32_t storage_at_crash = storage;                    // "Stromausfall" hier, kein weiteres Save

        // Neustart: laedt storage_at_crash (garantiert > max_counter_used == heltec_last)
        FixSession s2 = fix_session_init(storage_at_crash, &storage);
        assert(s2.counter > max_counter_used);
        uint32_t c = fix_session_send(&s2, &storage);
        assert(heltec_accept(c, &heltec_last));
        printf("FIX:  nach unsauberem Absturz bei Counter %u (Reserve war bei %u): "
               "erstes Kommando nach Neustart (Counter %u) sofort akzeptiert\n",
               max_counter_used, storage_at_crash, c);
    }

    printf("Alle Szenarien bestaetigt (exit code 0)\n");
    return 0;
}
