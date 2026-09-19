// Host-Repro: MIFARE-Classic-Dictionary-Angriff -- fruehzeitiger Abbruch bei Schluesselwiederverwendung
//
// Bildet zwei unabhaengige Stellen der eigenen mntm-012-Basis nach, exakt nach ihrer realen
// Semantik (Quellstudium gegen lib/nfc/protocols/mf_classic/mf_classic.c und
// lib/nfc/protocols/mf_classic/mf_classic_poller.c):
//
//   1) mf_classic_get_read_sectors_and_keys() / mf_classic_is_card_read() -- unveraendert
//      uebernommen, dient hier nur als Referenzimplementierung fuer beide Tests.
//   2) Poller-Ebene: mf_classic_poller_handler_request_key() fragt bei ALT bei jedem Sektor
//      erneut das Woerterbuch nach Schluesseln, auch wenn die Karte durch Schluesselwiederverwendung
//      (ein Schluessel oeffnet mehrere Sektoren) bereits vollstaendig gelesen ist. NEU prueft
//      vorab mf_classic_poller_is_card_read() und bricht sofort ab.
//   3) Szenen-Ebene: nfc_scene_mf_classic_dict_attack_on_event() startet bei ALT nach Abschluss
//      einer Woerterbuch-Phase unbedingt die naechste Phase (CUID->User->System) bzw. tut dies
//      beim Skip-Pfad, auch wenn die Karte bereits vollstaendig gelesen ist. NEU prueft die
//      Karten-Vollstaendigkeit vorab und springt stattdessen direkt zu ReadSuccess.
//
// Kompilieren/Ausfuehren:
//   gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro && ./repro

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX_SECTORS 16

// ---------------------------------------------------------------------------------------------
// 1) Referenzsemantik aus mf_classic.c, unveraendert (nur auf ein Test-Card-Modell umgelegt)
// ---------------------------------------------------------------------------------------------

typedef struct {
    uint8_t sectors_total;
    bool key_a_found[MAX_SECTORS];
    bool key_b_found[MAX_SECTORS];
    bool sector_read[MAX_SECTORS]; // vereinfachend: "alle Bloecke des Sektors gelesen"
} TestCard;

static void get_read_sectors_and_keys(const TestCard* c, uint8_t* sectors_read, uint8_t* keys_found) {
    *sectors_read = 0;
    *keys_found = 0;
    for(uint8_t i = 0; i < c->sectors_total; i++) {
        if(c->key_a_found[i]) *keys_found += 1;
        if(c->key_b_found[i]) *keys_found += 1;
        if(c->sector_read[i]) *sectors_read += 1;
    }
}

static bool is_card_read(const TestCard* c) {
    uint8_t sectors_read = 0, keys_found = 0;
    get_read_sectors_and_keys(c, &sectors_read, &keys_found);
    return (sectors_read == c->sectors_total) && (keys_found == c->sectors_total * 2);
}

// Schluesselwiederverwendung: ein an Sektor 0 gefundener Schluessel oeffnet auch alle anderen
// Sektoren (realer, haeufiger Fall bei Werks-/Testkarten mit demselben Schluessel je Sektor).
static void apply_key_reuse_at_sector_zero(TestCard* c) {
    for(uint8_t i = 0; i < c->sectors_total; i++) {
        c->key_a_found[i] = true;
        c->key_b_found[i] = true;
        c->sector_read[i] = true;
    }
}

static int checks_total = 0;
static int checks_failed = 0;
#define CHECK(cond, desc)                                                          \
    do {                                                                           \
        checks_total++;                                                            \
        if(!(cond)) {                                                              \
            checks_failed++;                                                       \
            printf("  FEHLGESCHLAGEN: %s (Zeile %d)\n", desc, __LINE__);           \
        }                                                                           \
    } while(0)

// ---------------------------------------------------------------------------------------------
// 2) Poller-Ebene: mf_classic_poller_handler_request_key(), ALT vs NEU
// ---------------------------------------------------------------------------------------------
//
// Vereinfachtes Modell des Zustandsautomaten: pro "Tick" fragt der Handler das Woerterbuch nach
// dem naechsten Schluessel fuer den aktuellen Sektor. Nach D erfolglosen Versuchen (oder einem
// Treffer) rueckt der Cursor zum naechsten Sektor vor. ALT prueft den Kartenzustand dabei nirgends;
// NEU prueft ihn vor jeder Anfrage (Portierung der realen Guard-Bedingung).

typedef struct {
    TestCard card;
    uint8_t dict_size; // Eintraege je simuliertem Woerterbuchdurchlauf
    uint8_t current_sector;
    uint32_t dict_lookups; // Zaehler: wie oft wurde das Woerterbuch nach einem Schluessel gefragt
    bool stopped;
} PollerSim;

// ALT: keine Vorabpruefung -- fragt bedingungslos weiter, bis der Sektor-Cursor das Ende erreicht.
static void poller_tick_old(PollerSim* p) {
    if(p->stopped) return;
    if(p->current_sector >= p->card.sectors_total) {
        p->stopped = true;
        return;
    }
    // Eine Anfrage kostet einen vollen Woerterbuchdurchlauf, wenn der Sektor schon
    // (durch Wiederverwendung) freigeschaltet ist -- kein Treffer wird je gebraucht, das
    // Woerterbuch wird trotzdem komplett durchsucht, bevor zum naechsten Sektor gewechselt wird.
    p->dict_lookups += p->dict_size;
    p->current_sector++;
}

// NEU: portierte Guard-Bedingung aus mf_classic_poller_handler_request_key() --
// mf_classic_poller_is_card_read() zuerst pruefen, sonst identisch zu ALT.
static void poller_tick_new(PollerSim* p) {
    if(p->stopped) return;
    if(is_card_read(&p->card)) {
        p->stopped = true; // instance->state = MfClassicPollerStateSuccess; return command;
        return;
    }
    if(p->current_sector >= p->card.sectors_total) {
        p->stopped = true;
        return;
    }
    p->dict_lookups += p->dict_size;
    p->current_sector++;
}

static void test_poller_level(void) {
    printf("== Test 1: Poller-Ebene (mf_classic_poller_handler_request_key) ==\n");

    const uint8_t sectors_total = 8;
    const uint8_t dict_size = 40;

    // ALT: Schluesselwiederverwendung tritt am Ende von Sektor 0 ein (die Karte ist danach
    // vollstaendig gelesen), aber der Cursor laeuft trotzdem alle 8 Sektoren komplett durch.
    PollerSim alt = {0};
    alt.card.sectors_total = sectors_total;
    alt.dict_size = dict_size;
    poller_tick_old(&alt); // Sektor 0: Treffer, danach Wiederverwendung ausgeloest
    apply_key_reuse_at_sector_zero(&alt.card);
    while(!alt.stopped) poller_tick_old(&alt);

    // NEU: identischer Ablauf, aber mit der portierten Vorabpruefung.
    PollerSim neu = {0};
    neu.card.sectors_total = sectors_total;
    neu.dict_size = dict_size;
    poller_tick_new(&neu); // Sektor 0: Treffer, danach Wiederverwendung ausgeloest
    apply_key_reuse_at_sector_zero(&neu.card);
    while(!neu.stopped) poller_tick_new(&neu);

    printf("  ALT: %u Woerterbuch-Abfragen fuer %u Sektoren (voller Durchlauf trotz geloester Karte)\n",
           alt.dict_lookups, alt.card.sectors_total);
    printf("  NEU: %u Woerterbuch-Abfragen (bricht sofort nach der geloesten Karte ab)\n",
           neu.dict_lookups);

    CHECK(alt.dict_lookups == (uint32_t)dict_size * sectors_total,
          "ALT durchsucht bei jedem der verbleibenden Sektoren erneut das komplette Woerterbuch");
    CHECK(neu.dict_lookups == dict_size,
          "NEU stoppt nach dem einen Treffer, der die Karte bereits vollstaendig geloest hat");

    // Regressionsfall: keine Schluesselwiederverwendung -- ALT und NEU muessen identisch
    // arbeiten (jeder Sektor braucht seinen eigenen Woerterbuchdurchlauf).
    PollerSim alt_reg = {0}, neu_reg = {0};
    alt_reg.card.sectors_total = sectors_total;
    alt_reg.dict_size = dict_size;
    neu_reg.card.sectors_total = sectors_total;
    neu_reg.dict_size = dict_size;
    for(uint8_t i = 0; i < sectors_total; i++) {
        poller_tick_old(&alt_reg);
        alt_reg.card.key_a_found[i] = true;
        alt_reg.card.key_b_found[i] = true;
        alt_reg.card.sector_read[i] = true;

        poller_tick_new(&neu_reg);
        neu_reg.card.key_a_found[i] = true;
        neu_reg.card.key_b_found[i] = true;
        neu_reg.card.sector_read[i] = true;
    }
    while(!alt_reg.stopped) poller_tick_old(&alt_reg);
    while(!neu_reg.stopped) poller_tick_new(&neu_reg);
    CHECK(alt_reg.dict_lookups == neu_reg.dict_lookups,
          "Regression: ohne Schluesselwiederverwendung liefern ALT und NEU dieselbe Abfrageanzahl");
    printf("  Regression (keine Wiederverwendung): ALT=%u NEU=%u Abfragen (gleich)\n\n",
           alt_reg.dict_lookups, neu_reg.dict_lookups);
}

// ---------------------------------------------------------------------------------------------
// 3) Szenen-Ebene: nfc_scene_mf_classic_dict_attack_on_event(), ALT vs NEU
// ---------------------------------------------------------------------------------------------
//
// Modelliert die vier betroffenen Uebergaenge (Complete: CUID->User, User->System; Skip:
// CUID->User, User->System). ALT loest den Uebergang unbedingt aus (neuer Poller, neuer
// Woerterbuch-Scan), NEU nur wenn die Karte noch nicht vollstaendig ist.

typedef enum { PhaseCUID, PhaseUser, PhaseSystem, PhaseReadSuccess } ScenePhase;

typedef struct {
    ScenePhase phase;
    int phase_starts; // Zaehler: wie oft wurde eine neue Woerterbuch-Phase (Poller+Dict) gestartet
} SceneSim;

static void scene_on_complete_old(SceneSim* s, bool /*card_read (ignoriert bei ALT)*/ unused) {
    (void)unused;
    if(s->phase == PhaseCUID) {
        s->phase = PhaseUser;
        s->phase_starts++;
    } else if(s->phase == PhaseUser) {
        s->phase = PhaseSystem;
        s->phase_starts++;
    } else {
        s->phase = PhaseReadSuccess;
    }
}

static void scene_on_complete_new(SceneSim* s, bool card_read) {
    if(s->phase == PhaseCUID && !card_read) {
        s->phase = PhaseUser;
        s->phase_starts++;
    } else if(s->phase == PhaseUser && !card_read) {
        s->phase = PhaseSystem;
        s->phase_starts++;
    } else {
        s->phase = PhaseReadSuccess;
    }
}

static void test_scene_level(void) {
    printf("== Test 2: Szenen-Ebene (nfc_scene_mf_classic_dict_attack_on_event, "
           "NfcCustomEventDictAttackComplete) ==\n");

    // Fall A: Karte wird bereits waehrend der CUID-Phase durch Schluesselwiederverwendung
    // vollstaendig geloest (card_read=true), der CUID-Woerterbuchdurchlauf endet aber trotzdem
    // regulaer (NfcCustomEventDictAttackComplete).
    SceneSim alt_a = {PhaseCUID, 0};
    scene_on_complete_old(&alt_a, true);
    CHECK(alt_a.phase == PhaseUser && alt_a.phase_starts == 1,
          "ALT startet die User-Woerterbuch-Phase auch auf einer bereits geloesten Karte");

    SceneSim neu_a = {PhaseCUID, 0};
    scene_on_complete_new(&neu_a, true);
    CHECK(neu_a.phase == PhaseReadSuccess && neu_a.phase_starts == 0,
          "NEU springt bei bereits geloester Karte direkt zu ReadSuccess, ohne die User-Phase zu starten");

    printf("  Fall A (Karte bereits bei CUID-Abschluss geloest): "
           "ALT startet Phase %d (phase_starts=%d), NEU springt direkt zu ReadSuccess (phase_starts=%d)\n",
           alt_a.phase, alt_a.phase_starts, neu_a.phase_starts);

    // Fall B: Regressionsfall -- Karte ist am Ende der CUID-Phase noch NICHT vollstaendig
    // geloest (normaler Fall ohne Schluesselwiederverwendung). ALT und NEU muessen hier
    // identisch in die User-Phase wechseln.
    SceneSim alt_b = {PhaseCUID, 0};
    scene_on_complete_old(&alt_b, false);
    SceneSim neu_b = {PhaseCUID, 0};
    scene_on_complete_new(&neu_b, false);
    CHECK(alt_b.phase == neu_b.phase && alt_b.phase_starts == neu_b.phase_starts,
          "Regression: unvollstaendige Karte wechselt bei ALT und NEU identisch in die naechste Phase");
    printf("  Fall B (Karte am CUID-Ende noch unvollstaendig, Regression): "
           "ALT und NEU wechseln beide in Phase %d (phase_starts=%d)\n\n",
           alt_b.phase, alt_b.phase_starts);

    // Fall C: dieselbe Situation eine Stufe weiter -- Karte wird erst waehrend der User-Phase
    // vollstaendig geloest, kein Nested-Angriff gelaufen (ran_nested_dict=false).
    SceneSim alt_c = {PhaseUser, 0};
    scene_on_complete_old(&alt_c, true);
    SceneSim neu_c = {PhaseUser, 0};
    scene_on_complete_new(&neu_c, true);
    CHECK(alt_c.phase == PhaseSystem && alt_c.phase_starts == 1,
          "ALT startet die System-Woerterbuch-Phase auch auf einer bereits geloesten Karte");
    CHECK(neu_c.phase == PhaseReadSuccess && neu_c.phase_starts == 0,
          "NEU springt bei bereits geloester Karte direkt zu ReadSuccess, ohne die System-Phase zu starten");
    printf("  Fall C (Karte bereits bei User-Abschluss geloest): "
           "ALT startet Phase %d (phase_starts=%d), NEU springt direkt zu ReadSuccess (phase_starts=%d)\n\n",
           alt_c.phase, alt_c.phase_starts, neu_c.phase_starts);
}

// Skip-Pfad: identische Guard-Struktur, zusaetzlich an die vorhandene is_card_present-Bedingung
// gekoppelt (siehe nfc_scene_mf_classic_dict_attack_on_event(), NfcCustomEventDictAttackSkip).
static void scene_on_skip_old(SceneSim* s, bool card_present, bool card_read) {
    (void)card_read;
    if(s->phase == PhaseCUID) {
        if(card_present) {
            s->phase = PhaseUser;
            s->phase_starts++;
        } else {
            s->phase = PhaseReadSuccess;
        }
    } else if(s->phase == PhaseUser) {
        if(card_present) {
            s->phase = PhaseSystem;
            s->phase_starts++;
        } else {
            s->phase = PhaseReadSuccess;
        }
    }
}

static void scene_on_skip_new(SceneSim* s, bool card_present, bool card_read) {
    if(s->phase == PhaseCUID) {
        if(card_present && !card_read) {
            s->phase = PhaseUser;
            s->phase_starts++;
        } else {
            s->phase = PhaseReadSuccess;
        }
    } else if(s->phase == PhaseUser) {
        if(card_present && !card_read) {
            s->phase = PhaseSystem;
            s->phase_starts++;
        } else {
            s->phase = PhaseReadSuccess;
        }
    }
}

static void test_scene_skip_path(void) {
    printf("== Test 3: Szenen-Ebene, Skip-Pfad (NfcCustomEventDictAttackSkip) ==\n");

    // Karte anwesend UND bereits vollstaendig geloest (Wiederverwendungsfall) -- ALT wertet nur
    // is_card_present aus und startet die naechste Phase unnoetig, NEU beruecksichtigt zusaetzlich
    // card_read und springt direkt zu ReadSuccess.
    SceneSim alt = {PhaseCUID, 0};
    scene_on_skip_old(&alt, true, true);
    SceneSim neu = {PhaseCUID, 0};
    scene_on_skip_new(&neu, true, true);
    CHECK(alt.phase == PhaseUser && alt.phase_starts == 1,
          "ALT startet beim Skip die User-Phase, obwohl die Karte anwesend UND bereits geloest ist");
    CHECK(neu.phase == PhaseReadSuccess && neu.phase_starts == 0,
          "NEU erkennt beim Skip die bereits geloeste Karte und springt zu ReadSuccess");
    printf("  Karte anwesend + bereits geloest: ALT startet Phase %d, NEU springt zu ReadSuccess\n",
           alt.phase);

    // Regression 1: Karte anwesend, aber NOCH NICHT vollstaendig geloest -- unveraendertes
    // Verhalten (User-Phase startet in beiden Faellen).
    SceneSim alt_r1 = {PhaseCUID, 0}, neu_r1 = {PhaseCUID, 0};
    scene_on_skip_old(&alt_r1, true, false);
    scene_on_skip_new(&neu_r1, true, false);
    CHECK(alt_r1.phase == neu_r1.phase && alt_r1.phase_starts == neu_r1.phase_starts,
          "Regression: Karte anwesend, unvollstaendig -- ALT und NEU identisch");

    // Regression 2: Karte nicht (mehr) anwesend -- unveraendertes Verhalten (direkt ReadSuccess).
    SceneSim alt_r2 = {PhaseCUID, 0}, neu_r2 = {PhaseCUID, 0};
    scene_on_skip_old(&alt_r2, false, false);
    scene_on_skip_new(&neu_r2, false, false);
    CHECK(alt_r2.phase == neu_r2.phase && alt_r2.phase_starts == neu_r2.phase_starts,
          "Regression: Karte nicht anwesend -- ALT und NEU identisch (ReadSuccess)");

    printf("  Regressionsfaelle (Karte unvollstaendig / nicht anwesend): ALT und NEU identisch\n\n");
}

int main(void) {
    // Sanity-Check der Referenzsemantik selbst (unveraenderte mf_classic.c-Logik)
    printf("== Sanity: is_card_read()-Referenzsemantik ==\n");
    TestCard c = {0};
    c.sectors_total = 4;
    CHECK(!is_card_read(&c), "leere Karte ist nicht vollstaendig gelesen");
    apply_key_reuse_at_sector_zero(&c);
    CHECK(is_card_read(&c), "nach Wiederverwendung sind alle Sektoren/Schluessel vollstaendig");
    printf("  OK\n\n");

    test_poller_level();
    test_scene_level();
    test_scene_skip_path();

    printf("%d/%d Checks bestanden\n", checks_total - checks_failed, checks_total);
    if(checks_failed == 0) {
        printf("ALLE CHECKS BESTANDEN\n");
        return 0;
    } else {
        printf("%d CHECKS FEHLGESCHLAGEN\n", checks_failed);
        return 1;
    }
}
