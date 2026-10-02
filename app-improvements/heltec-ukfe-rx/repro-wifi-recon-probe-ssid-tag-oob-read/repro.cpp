// Host-Repro: RC_PROBE-Zweig aus heltec-ukfe-rx/src/wifi_recon.cpp::promisc_cb()
// 1:1 aus dem echten Funktionskoerper transkribiert (nur der SSID-Tag-Parse-Teil,
// unveraendert gegenueber dem Original -- s_hits/Serial.printf ausgelassen, da fuer
// den Fund irrelevant und ohne Arduino/ESP-IDF auf dem Host nicht verfuegbar).
//
// Baue zwei Varianten aus derselben Quelle:
//   g++ -DOLD_BUG ... -> Ist-Zustand (fr[24] wird IMMER gelesen)
//   g++          ... -> Fix (len > 24 Guard vor dem fr[24]-Zugriff, wie im Patch)
//
// Je fuenf Szenarien; Szenario 1 (len==24, keine IEs) ist der einzige Fall, in dem
// sich OLD_BUG und Fix unterscheiden koennen -- im OLD_BUG-Build loest er einen
// Heap-Buffer-Overflow-READ aus (ASan faengt ihn), im Fix-Build nicht. Alle anderen
// Szenarien muessen in beiden Builds byte-identische Ausgabe liefern (Regressionstest).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---- 1:1 aus promisc_cb()'s RC_PROBE-Zweig ----
static void parse_probe(const uint8_t* fr, int len, char ssid[33]) {
#ifdef OLD_BUG
    uint8_t slen = (len > 25) ? fr[25] : 0;   // SSID-Tag: fr[24]=0x00, fr[25]=len
    uint8_t n = slen > 32 ? 32 : slen;
    if(fr[24] == 0x00 && 26 + n <= len) { memcpy(ssid, &fr[26], n); ssid[n] = 0; }
    else { ssid[0] = 0; }
#else
    uint8_t slen = (len > 25) ? fr[25] : 0;   // SSID-Tag: fr[24]=0x00, fr[25]=len
    uint8_t n = slen > 32 ? 32 : slen;
    if(len > 24 && fr[24] == 0x00 && 26 + n <= len) { memcpy(ssid, &fr[26], n); ssid[n] = 0; }
    else { ssid[0] = 0; }
#endif
}

struct Scenario {
    const char* name;
    int len;
    // Baut den Frame: 24-Byte-802.11-Header (Inhalt fuer diesen Zweig irrelevant,
    // mit 0xAA gefuellt) + optionale SSID-IE ab Offset 24.
    void (*fill)(uint8_t* fr, int len);
    const char* expect_ssid;   // erwarteter ssid-Inhalt (als C-String) in BEIDEN Builds
};

static void fill_no_ie(uint8_t* fr, int len) { (void)fr; (void)len; }   // nur der 24-Byte-Header, keine IE

static void fill_tag_only(uint8_t* fr, int len) {
    (void)len;
    fr[24] = 0x00;   // SSID-Tag, aber Frame endet direkt danach (kein Laengen-Byte)
}

static void fill_named_ssid(uint8_t* fr, int len) {
    (void)len;
    const char* s = "TestAP";
    fr[24] = 0x00; fr[25] = (uint8_t)strlen(s);
    memcpy(&fr[26], s, strlen(s));
}

static void fill_broadcast_probe(uint8_t* fr, int len) {
    (void)len;
    fr[24] = 0x00; fr[25] = 0x00;   // SSID-Tag, Laenge 0 (Broadcast-Probe)
}

static void fill_max_ssid(uint8_t* fr, int len) {
    (void)len;
    fr[24] = 0x00; fr[25] = 32;
    for(int i = 0; i < 32; i++) fr[26 + i] = 'A' + (i % 26);
}

int main() {
    Scenario cases[] = {
        // len==24: GENAU der Grenzfall des Funds -- keine einzige IE-Byte vorhanden.
        { "len=24 keine IEs (Fund-Grenzfall)", 24, fill_no_ie,         "" },
        // len==25: SSID-Tag-Byte vorhanden, aber kein Laengen-Byte mehr -> bereits
        // vor dem Fix sicher (slen bleibt 0 ueber den len>25-Guard), Regressionscheck.
        { "len=25 nur Tag-Byte",               25, fill_tag_only,      "" },
        { "len=32 normale SSID 'TestAP'",       26 + 6, fill_named_ssid,   "TestAP" },
        { "len=26 Broadcast-Probe (SSID-Laenge 0)", 26, fill_broadcast_probe, "" },
        { "len=58 maximale SSID-Laenge (32)",   26 + 32, fill_max_ssid, "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEF" },
    };

    // SKIP_S1=1: Szenario 1 (der einzige Fall mit erwartetem ASan-Fund im OLD_BUG-Build)
    // auslassen, um zu zeigen, dass Szenario 2-5 in OLD_BUG und FIX byte-identisch bleiben
    // (OLD_BUG bricht sonst beim ersten ASan-Fund ab, bevor die spaeteren Szenarien laufen).
    bool skip_s1 = getenv("SKIP_S1") != nullptr;

    int failed = 0;
    for(auto& c : cases) {
        if(skip_s1 && &c == &cases[0]) continue;
        uint8_t* fr = (uint8_t*)malloc((size_t)c.len);   // exakt c.len Bytes -- ASan erkennt jeden Zugriff darueber hinaus
        memset(fr, 0xAA, (size_t)c.len);
        c.fill(fr, c.len);

        char ssid[33];
        parse_probe(fr, c.len, ssid);

        bool ok = (strcmp(ssid, c.expect_ssid) == 0);
        printf("[%s] %-45s ssid='%s' erwartet='%s' -> %s\n",
#ifdef OLD_BUG
            "OLD_BUG",
#else
            "FIX",
#endif
            c.name, ssid, c.expect_ssid, ok ? "OK" : "FEHLER");
        if(!ok) failed++;

        free(fr);
    }

    if(failed) { printf("%d Szenario(e) FEHLGESCHLAGEN\n", failed); return 1; }
    printf("ALLE CHECKS BESTANDEN\n");
    return 0;
}
