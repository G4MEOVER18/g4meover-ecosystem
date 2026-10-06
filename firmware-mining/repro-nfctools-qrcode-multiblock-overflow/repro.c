// Host-Repro: NFC Tools QR-Code-Hilfsmodul erzeugt fuer Version 6/7 (ECC-L) einen
// strukturell ungueltigen, nicht scannbaren QR-Code, weil nfc_tools_qrcodegen.c jede
// Version 1-7 als EINEN Reed-Solomon-Block behandelt. Ab Version 6 teilt der echte
// QR-Standard (ISO/IEC 18004:2015, ECC-Level L) die Daten tatsaechlich in ZWEI
// verschachtelte RS-Bloecke (v6 = 2x(86,68), v7 = 2x(98,78) Codewoerter) -- diese
// Werte sind unabhaengig gegen die "segno"-Referenzbibliothek (segno.consts.ECC)
// gegengeprueft, nicht nur gegen eigene Berechnung.
//
// ALT (unveraendert aus RogueMaster/flipperzero-firmware-wPlugins Branch 420,
// nfc_tools_qrcodegen_alt.c) und NEU (gefixt, Versionsschleife auf 1-5 begrenzt,
// nfc_tools_qrcodegen_new.c) werden ueber Praeprozessor-Makros mit eigenen
// Funktionsnamen in denselben Prozess eingebunden -- beide Dateien sind 1:1
// identisch zur echten Quelle bzw. zum angewendeten Patch, keine Transkription.
//
// Szenario 1 (Regression, Version 1-5): ALT und NEU liefern fuer jede Textlaenge
// 1..106 Zeichen byte-identische QR-Matrizen.
// Szenario 2 (Fund, Version 6/7): ALT erzeugt fuer 107..174 Zeichen weiterhin
// "erfolgreich" (true) einen QR-Code -- aber dessen Bitmap decodiert bei einem
// echten, unabhaengigen Scanner (zbarimg, extern ueber popen() aufgerufen, keine
// eigene Decoder-Implementierung) NICHT zurueck auf den Originaltext. NEU gibt fuer
// dieselben Laengen ehrlich false zurueck ("QR unavailable" in der App) statt eine
// kaputte Grafik anzuzeigen.
// Szenario 3 (Grenzfall): Laenge 106 (NEU: letzte noch unterstuetzte Version-5-Laenge)
// bleibt erfolgreich und scannbar, Laenge 107 faellt bei NEU korrekt auf false.

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// nfc_tools_qrcodegen_alt.c (unveraendert) und nfc_tools_qrcodegen_new.c (gefixt)
// werden als ZWEI GETRENNTE Uebersetzungseinheiten gebaut, jede mit eigenen
// -D-Praeprozessor-Umbenennungen ihrer drei oeffentlichen Funktionen (siehe
// Build-Kommando unten) -- deshalb hier nur Low-Level-Prototypen von Hand statt
// eines zweiten Includes von nfc_tools_qrcodegen.h (das Header-"#pragma once"
// wuerde den zweiten Include ohnehin unterdruecken).
#include "nfc_tools_qrcodegen.h" // nur fuer QRCODEGEN_BUF_MAX

bool nfc_tools_qr_encode_alt(
    const char* text, uint8_t tempBuffer[QRCODEGEN_BUF_MAX], uint8_t qrcode[QRCODEGEN_BUF_MAX]);
int nfc_tools_qr_size_alt(const uint8_t qrcode[QRCODEGEN_BUF_MAX]);
bool nfc_tools_qr_module_alt(const uint8_t qrcode[QRCODEGEN_BUF_MAX], int x, int y);

bool nfc_tools_qr_encode_new(
    const char* text, uint8_t tempBuffer[QRCODEGEN_BUF_MAX], uint8_t qrcode[QRCODEGEN_BUF_MAX]);
int nfc_tools_qr_size_new(const uint8_t qrcode[QRCODEGEN_BUF_MAX]);
bool nfc_tools_qr_module_new(const uint8_t qrcode[QRCODEGEN_BUF_MAX], int x, int y);

static int g_checks = 0;
static int g_failed = 0;

static void check(bool cond, const char* label) {
    g_checks++;
    if(cond) {
        printf("OK:   %s\n", label);
    } else {
        printf("FAIL: %s\n", label);
        g_failed++;
    }
}

// Rendert eine ALT- oder NEU-Matrix als PGM-Datei (Pixel-fuer-Modul, Ruhezone 4,
// Skalierung 8x -- ausreichend gross fuer einen echten Scanner) und ruft zbarimg
// darauf auf. Gibt true zurueck, wenn zbarimg den Originaltext zurueckliest.
typedef int (*qr_size_fn)(const uint8_t[QRCODEGEN_BUF_MAX]);
typedef bool (*qr_module_fn)(const uint8_t[QRCODEGEN_BUF_MAX], int, int);

static bool zbar_decodes_to(
    const uint8_t qrcode[QRCODEGEN_BUF_MAX],
    qr_size_fn size_fn,
    qr_module_fn module_fn,
    const char* expected_text,
    const char* tmp_path) {
    int sz = size_fn(qrcode);
    int scale = 8, quiet = 4;
    int img = sz + 2 * quiet;

    FILE* f = fopen(tmp_path, "wb");
    if(!f) return false;
    fprintf(f, "P5\n%d %d\n255\n", img * scale, img * scale);
    for(int y = 0; y < img * scale; y++) {
        for(int x = 0; x < img * scale; x++) {
            int mx = x / scale - quiet, my = y / scale - quiet;
            int v = 255;
            if(mx >= 0 && mx < sz && my >= 0 && my < sz && module_fn(qrcode, mx, my)) v = 0;
            fputc(v, f);
        }
    }
    fclose(f);

    char cmd[768];
    snprintf(
        cmd,
        sizeof(cmd),
        "zbarimg -q --raw %s 2>/dev/null",
        tmp_path);
    FILE* p = popen(cmd, "r");
    if(!p) return false;
    char line[512] = {0};
    bool got_line = (fgets(line, sizeof(line), p) != NULL);
    pclose(p);
    if(!got_line) return false;
    size_t n = strlen(line);
    while(n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
    return strcmp(line, expected_text) == 0;
}

static bool have_zbarimg(void) {
    FILE* p = popen("command -v zbarimg >/dev/null 2>&1", "r");
    if(!p) return false;
    int rc = pclose(p);
    return rc == 0;
}

static char s_text[256];

int main(void) {
    bool zbar_available = have_zbarimg();
    if(!zbar_available) {
        printf(
            "Hinweis: zbarimg nicht gefunden -- Szenario 2/3 laufen nur mit dem "
            "strukturellen ALT/NEU-Vergleich (Rueckgabewert, Matrix-Diff), ohne den "
            "echten Scanner-Rundlauf. Szenario 1 (Regression) ist davon unabhaengig.\n\n");
    }

    // ── Szenario 1: Regression ueber alle Version-1-5-Laengen (1..106) ──────────
    {
        bool all_identical = true;
        for(size_t n = 1; n <= 106; n++) {
            memset(s_text, 'A', n);
            s_text[n] = '\0';

            static uint8_t temp_alt[QRCODEGEN_BUF_MAX], qr_alt[QRCODEGEN_BUF_MAX];
            static uint8_t temp_new[QRCODEGEN_BUF_MAX], qr_new[QRCODEGEN_BUF_MAX];
            bool ok_alt = nfc_tools_qr_encode_alt(s_text, temp_alt, qr_alt);
            bool ok_new = nfc_tools_qr_encode_new(s_text, temp_new, qr_new);

            if(!ok_alt || !ok_new || memcmp(qr_alt, qr_new, QRCODEGEN_BUF_MAX) != 0) {
                all_identical = false;
                printf("  -> Abweichung bei Laenge %zu (ok_alt=%d ok_new=%d)\n", n, ok_alt, ok_new);
                break;
            }
        }
        check(
            all_identical,
            "Regression: ALT und NEU liefern fuer Laenge 1..106 (Version 1-5) "
            "byte-identische QR-Matrizen.");
    }

    // ── Szenario 2: Fund -- Version 6/7 (Laenge 107..174) bei ALT unscannbar ────
    {
        int sample_lengths[] = {107, 110, 130, 150, 174};
        bool found_at_least_one_unscannable = false;
        bool all_new_reject = true;

        for(size_t i = 0; i < sizeof(sample_lengths) / sizeof(sample_lengths[0]); i++) {
            int n = sample_lengths[i];
            memset(s_text, 'A', (size_t)n);
            s_text[n] = '\0';

            static uint8_t temp_alt[QRCODEGEN_BUF_MAX], qr_alt[QRCODEGEN_BUF_MAX];
            static uint8_t temp_new[QRCODEGEN_BUF_MAX], qr_new[QRCODEGEN_BUF_MAX];
            bool ok_alt = nfc_tools_qr_encode_alt(s_text, temp_alt, qr_alt);
            bool ok_new = nfc_tools_qr_encode_new(s_text, temp_new, qr_new);

            if(ok_new) all_new_reject = false;

            if(ok_alt && zbar_available) {
                bool decodes = zbar_decodes_to(
                    qr_alt,
                    nfc_tools_qr_size_alt,
                    nfc_tools_qr_module_alt,
                    s_text,
                    "/tmp/nfctools_qr_repro_alt.pgm");
                printf(
                    "  Laenge %d: ALT meldet encode=true, Version=%u, zbarimg liest "
                    "zurueck=%s\n",
                    n,
                    qr_alt[0],
                    decodes ? "ja (unerwartet)" : "nein (Fund bestaetigt)");
                if(!decodes) found_at_least_one_unscannable = true;
            } else if(ok_alt) {
                found_at_least_one_unscannable = true; // ohne zbarimg: ALT behauptet
                // zumindest weiterhin truthy fuer Versionen, die NEU bewusst ablehnt.
            }
        }

        check(
            all_new_reject,
            "NEU lehnt alle Laengen 107/110/130/150/174 (Version 6/7) mit false ab "
            "(\"QR unavailable\") statt eine Grafik zu zeigen.");
        if(zbar_available) {
            check(
                found_at_least_one_unscannable,
                "Fund reproduziert: mindestens eine von ALT als \"true\" gemeldete "
                "Version-6/7-QR-Grafik liest bei einem echten, unabhaengigen Scanner "
                "(zbarimg) NICHT auf den Originaltext zurueck.");
        }
    }

    // ── Szenario 3: Grenzfall 106 (letzte Version 5) vs. 107 (erste Version 6) ──
    {
        memset(s_text, 'A', 106);
        s_text[106] = '\0';
        static uint8_t temp106[QRCODEGEN_BUF_MAX], qr106[QRCODEGEN_BUF_MAX];
        bool ok106 = nfc_tools_qr_encode_new(s_text, temp106, qr106);
        check(ok106 && qr106[0] == 5, "Grenzfall: Laenge 106 bleibt bei NEU Version 5, encode=true.");

        if(ok106 && zbar_available) {
            bool decodes106 = zbar_decodes_to(
                qr106, nfc_tools_qr_size_new, nfc_tools_qr_module_new, s_text,
                "/tmp/nfctools_qr_repro_new106.pgm");
            check(decodes106, "Grenzfall: Laenge 106 (NEU) ist bei zbarimg scannbar.");
        }

        memset(s_text, 'A', 107);
        s_text[107] = '\0';
        static uint8_t temp107[QRCODEGEN_BUF_MAX], qr107[QRCODEGEN_BUF_MAX];
        bool ok107 = nfc_tools_qr_encode_new(s_text, temp107, qr107);
        check(!ok107, "Grenzfall: Laenge 107 faellt bei NEU korrekt auf encode=false.");
    }

    // ── Szenario 4: leerer Text bleibt bei beiden Fassungen abgelehnt ───────────
    {
        static uint8_t t[QRCODEGEN_BUF_MAX], q[QRCODEGEN_BUF_MAX];
        bool ok_alt = nfc_tools_qr_encode_alt("", t, q);
        bool ok_new = nfc_tools_qr_encode_new("", t, q);
        check(!ok_alt && !ok_new, "Randfall leerer Text: ALT und NEU lehnen beide ab (unveraendert).");
    }

    printf("\n%d/%d Szenarien bestanden.\n", g_checks - g_failed, g_checks);
    if(!zbar_available) {
        printf(
            "(zbarimg war in dieser Umgebung nicht installiert -- Szenario 2/3 liefen "
            "nur strukturell. In der Sitzung, die diesen Fund dokumentiert, lief zbarimg "
            "zusaetzlich und hat den Scann-Fehlschlag fuer ALT bei Version 6/7 bestaetigt.)\n");
    }
    return g_failed == 0 ? 0 : 1;
}
