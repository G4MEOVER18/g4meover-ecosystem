// Host-Repro: protopirate_history_add_to_history() (protopirate_history.c) prueft die
// Verlaufsgroesse/Auto-Purge-Eviction VOR der Duplikat-Entprellung ("code_last_hash_data" +
// "< 500 ms" -Fenster). Wird bei vollem Verlauf mit aktiviertem Auto-Purge ein wirklich neues
// Signal aufgenommen, wird dafuer korrekt der aelteste Eintrag verdraengt. Redekodiert der
// SubGHz-Empfaenger dasselbe Signal danach noch einmal innerhalb von 500 ms -- der Normalfall,
// da ein gehaltener Fernbedienungs-Taster dieselbe Bitfolge ueber die gesamte Druckdauer
// mehrfach sendet und jede vollstaendige Dekodierung einen eigenen add_to_history()-Aufruf
// ausloest --, greift die Limit-Pruefung ein zweites Mal (der Verlauf ist ja durch die erste
// Aufnahme wieder voll) und verdraengt einen ZWEITEN aeltesten Eintrag, bevor die anschliessende
// Entprellung erkennt, dass es sich um eine Wiederholung handelt und ueberhaupt nichts Neues
// hinzugefuegt wird. Ergebnis: ein einzelner Tastendruck bei vollem Verlauf verdraengt zwei
// echte, unabhaengige Captures, obwohl nur ein einziger neuer Eintrag hinzugekommen ist.
//
// ALT/NEU unten bilden die Kontrollstruktur von protopirate_history_add_to_history() 1:1 nach
// (Limit-Pruefung+Auto-Purge-Eviction, dann Duplikat-Entprellung, dann Hinzufuegen), reduziert
// auf die fuer den Bug relevante Buchhaltung (Anzahl/Reihenfolge der Eintraege). Storage-I/O,
// FlipperFormat-Serialisierung und der Text-Arena-Teil sind fuer den Fehler irrelevant und
// deshalb hier durch ein einfaches Array aus Sequenz-IDs ersetzt -- "Loeschen von Index 0"
// entspricht 1:1 ProtoPirateHistoryItemArray_pop_at(..., 0) in protopirate_history_delete_item().
//
// Build & run:
//   gcc -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
//
// Erwartete Ausgabe: ALT verliert bei vollem Verlauf + Auto-Purge genau einen zusaetzlichen,
// unbeteiligten Eintrag pro tatsaechlich neu aufgenommenem Signal, sobald dieses innerhalb von
// 500 ms erneut dekodiert wird; NEU bleibt in derselben Situation stabil (keine Loeschung ohne
// einen tatsaechlich neuen Eintrag).

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HIST_CAP 16

typedef struct {
    int seq[HIST_CAP]; // Reihenfolge = Alter, seq[0] ist immer der aelteste vorhandene Eintrag
    size_t count;
    uint8_t limit;
    bool auto_purge;
    bool has_last_hash;
    uint32_t code_last_hash_data;
    uint32_t last_update_timestamp;
} Hist;

static void hist_init(Hist* h, uint8_t limit, bool auto_purge) {
    memset(h, 0, sizeof(*h));
    h->limit = limit;
    h->auto_purge = auto_purge;
}

// Entspricht protopirate_history_delete_item(instance, 0): Item an Index 0 entfernen,
// Rest nach vorne schieben (ProtoPirateHistoryItemArray_pop_at).
static void hist_delete_oldest(Hist* h) {
    assert(h->count > 0);
    for(size_t i = 0; i + 1 < h->count; i++) {
        h->seq[i] = h->seq[i + 1];
    }
    h->count--;
}

// ALT: 1:1 Kontrollfluss von protopirate_history_add_to_history() vor dem Fix --
// Limit-Pruefung/Eviction VOR der Duplikat-Entprellung.
static bool hist_add_ALT(Hist* h, uint32_t hash, uint32_t now_tick, int seq) {
    if(h->count >= h->limit) {
        if(h->auto_purge) {
            hist_delete_oldest(h);
        } else {
            return false;
        }
    }

    if(h->has_last_hash && h->code_last_hash_data == hash &&
       (now_tick - h->last_update_timestamp) < 500) {
        h->last_update_timestamp = now_tick;
        return false;
    }

    h->code_last_hash_data = hash;
    h->has_last_hash = true;
    h->last_update_timestamp = now_tick;
    h->seq[h->count++] = seq;
    return true;
}

// NEU: Duplikat-Entprellung zuerst, Limit-Pruefung/Eviction nur noch fuer tatsaechlich neue
// Signale. Einzige Aenderung gegenueber ALT ist die Reihenfolge der beiden Bloecke.
static bool hist_add_NEW(Hist* h, uint32_t hash, uint32_t now_tick, int seq) {
    if(h->has_last_hash && h->code_last_hash_data == hash &&
       (now_tick - h->last_update_timestamp) < 500) {
        h->last_update_timestamp = now_tick;
        return false;
    }

    if(h->count >= h->limit) {
        if(h->auto_purge) {
            hist_delete_oldest(h);
        } else {
            return false;
        }
    }

    h->code_last_hash_data = hash;
    h->has_last_hash = true;
    h->last_update_timestamp = now_tick;
    h->seq[h->count++] = seq;
    return true;
}

static void print_state(const char* label, Hist* h) {
    printf("%s: count=%zu items=[", label, h->count);
    for(size_t i = 0; i < h->count; i++) {
        printf("%s%d", i ? "," : "", h->seq[i]);
    }
    printf("]\n");
}

int main(void) {
    // Szenario: Verlaufslimit = 3, Auto-Purge aktiviert. Drei unabhaengige Signale fuellen den
    // Verlauf vollstaendig (grosser zeitlicher Abstand, damit die Entprellung hier nicht
    // greift). Danach trifft ein viertes, wirklich neues Signal (hash=100, seq=4) ein --
    // korrekt wird der aelteste Eintrag (seq=1) verdraengt. Anschliessend "redekodiert" der
    // Empfaenger DASSELBE vierte Signal noch zweimal innerhalb von 500 ms (typisch fuer einen
    // gehaltenen Taster) -- dabei darf NICHTS mehr geloescht werden, da kein neuer Eintrag
    // entsteht.
    printf("=== ALT (Limit-Check vor Entprellung) ===\n");
    {
        Hist h;
        hist_init(&h, 3, true);
        assert(hist_add_ALT(&h, 1, 0, 1) == true);
        assert(hist_add_ALT(&h, 2, 1000, 2) == true);
        assert(hist_add_ALT(&h, 3, 2000, 3) == true);
        print_state("nach 3 Einzelsignalen", &h);
        assert(h.count == 3);

        bool added4 = hist_add_ALT(&h, 100, 10000, 4);
        print_state("nach neuem Signal (seq=4, verdraengt seq=1)", &h);
        assert(added4 == true);
        assert(h.count == 3);

        bool added_dup1 = hist_add_ALT(&h, 100, 10100, 4);
        print_state("nach 1. Redekodierung desselben Signals (100ms spaeter)", &h);
        assert(added_dup1 == false); // Entprellung greift korrekt: nichts Neues hinzugefuegt
        assert(h.count == 2); // BUG: trotzdem wurde seq=2 zusaetzlich geloescht

        // Der freigewordene Platz ist nach dem Bug-Verlust nicht mehr "voll" (count < limit),
        // die naechste Redekodierung desselben Signals loest deshalb keine weitere Eviction
        // mehr aus -- die Entprellung greift jetzt wie vorgesehen, ohne zusaetzlichen Verlust.
        // Der Schaden bleibt trotzdem real: ein einzelner gehaltener Tastendruck bei vollem
        // Verlauf hat bereits ZWEI echte Eintraege verdraengt (seq=1 UND seq=2), obwohl nur
        // EIN neuer Eintrag (seq=4) hinzugekommen ist.
        bool added_dup2 = hist_add_ALT(&h, 100, 10200, 4);
        print_state("nach 2. Redekodierung desselben Signals (200ms spaeter)", &h);
        assert(added_dup2 == false);
        assert(h.count == 2); // kein weiterer Verlust mehr, aber seq=2 bleibt weg
        assert(h.seq[0] == 3 && h.seq[1] == 4);
    }

    printf("\n=== NEU (Entprellung vor Limit-Check) ===\n");
    {
        Hist h;
        hist_init(&h, 3, true);
        assert(hist_add_NEW(&h, 1, 0, 1) == true);
        assert(hist_add_NEW(&h, 2, 1000, 2) == true);
        assert(hist_add_NEW(&h, 3, 2000, 3) == true);
        print_state("nach 3 Einzelsignalen", &h);
        assert(h.count == 3);

        bool added4 = hist_add_NEW(&h, 100, 10000, 4);
        print_state("nach neuem Signal (seq=4, verdraengt seq=1)", &h);
        assert(added4 == true);
        assert(h.count == 3);

        bool added_dup1 = hist_add_NEW(&h, 100, 10100, 4);
        print_state("nach 1. Redekodierung desselben Signals (100ms spaeter)", &h);
        assert(added_dup1 == false);
        assert(h.count == 3); // FIX: keine zusaetzliche Loeschung, Verlauf bleibt stabil
        assert(h.seq[0] == 2 && h.seq[1] == 3 && h.seq[2] == 4);

        bool added_dup2 = hist_add_NEW(&h, 100, 10200, 4);
        print_state("nach 2. Redekodierung desselben Signals (200ms spaeter)", &h);
        assert(added_dup2 == false);
        assert(h.count == 3); // FIX: weiterhin stabil, auch nach mehreren Wiederholungen
        assert(h.seq[0] == 2 && h.seq[1] == 3 && h.seq[2] == 4);
    }

    printf("\nOK: ALT verliert bei der ersten Redekodierung einen weiteren echten Eintrag "
           "(zwei Verdraengungen fuer nur einen neuen Eintrag), NEU bleibt stabil.\n");
    return 0;
}
