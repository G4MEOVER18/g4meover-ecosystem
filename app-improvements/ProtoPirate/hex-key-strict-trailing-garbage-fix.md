# ProtoPirate — Trailing-Garbage-Fix in `pp_parse_hex_u64_strict`

## Was
Meine ProtoPirate-App liest den 64-Bit-Schlüssel jedes Protokoll-Decoders (KIA v0–v7,
Ford v0–v3, PSA, Fiat v0/v1, Honda, Star Line, VAG, ...) über eine zentrale Hilfsfunktion in
`protocols/protocols_common.c`:

```c
bool pp_parse_hex_u64_strict(const char* str, uint64_t* out_key) {
    if(!str || !out_key) {
        return false;
    }

    uint64_t key = 0;
    uint8_t hex_pos = 0;
    for(size_t i = 0; str[i] != '\0' && hex_pos < 16; i++) {
        if(str[i] == ' ') {
            continue;
        }

        uint8_t nibble = 0;
        if(!pp_hex_nibble(str[i], &nibble)) {
            return false;
        }
        key = (key << 4) | nibble;
        hex_pos++;
    }

    if(hex_pos != 16) {
        return false;
    }
    *out_key = key;
    return true;
}
```

Die Schleife bricht ab, sobald 16 Hex-Nibbles eingelesen sind (`hex_pos < 16` in der
Schleifenbedingung). Alles, was danach im String noch folgt, wird nie angeschaut — auch dann
nicht, wenn dort weitere Hex-Zeichen oder sonstiger Text steht. `pp_flipper_read_hex_u64()`
ruft diese Funktion für den `"Key"`-String jedes `.sub`-Capture-Files auf
(`protocols_common.c:78-91`), bevor der Wert an den jeweiligen Protokoll-Decoder als
Klon-/Emulations-Schlüssel weitergereicht wird.

Konkretes Beispiel: `"0123456789ABCDEF99"` (16 gültige Hex-Ziffern + zwei überzählige) liefert
`true` und `out_key = 0x0123456789ABCDEF` — als hätte der String exakt 16 Ziffern gehabt. Der
Funktionsname `_strict` verspricht aber, genau das zu verhindern: entweder ein exakt
16-stelliger (durch Leerzeichen unterbrechbarer) Hex-String, oder Ablehnung.

Diese Diskrepanz betrifft nicht nur selbst mit ProtoPirate erzeugte Dateien (die schreiben
den Key immer exakt 16-stellig und lösen den Bug nie aus), sondern jede `.sub`-Datei mit
einem `Key`-Feld, die ich aus einer anderen Quelle importiere — z. B. Captures/Schlüssel-
Dumps aus anderen Tools, Community-Sammlungen oder manuell nachbearbeitete Dateien, wie sie
bei einer autorisierten Bewertung fremder Rolling-Code-Systeme anfallen können.

## Nutzen
- Schließt eine Lücke in der eigenen Eingabevalidierung: eine `.sub`-Datei mit
  fehlerhaftem/verlängertem `Key`-Feld wird aktuell **stillschweigend mit einem falschen,
  abgeschnittenen Schlüssel** akzeptiert, statt korrekt als ungültig abgelehnt zu werden. Bei
  einem defensiven Audit-Tool ist "leise falsches Ergebnis" schlechter als "klare Ablehnung
  mit Fehler-Log" — Letzteres macht mich sofort auf die kaputte Eingabedatei aufmerksam,
  Ersteres könnte einen Emulations-/Analyse-Fehlschlag verursachen, der aussieht wie ein
  Problem im Protokoll-Decoder statt in der Eingabedatei.
- Betrifft die zentrale Hilfsfunktion, die **alle** Key-lesenden Protokoll-Decoder gemeinsam
  nutzen (`kia_v0/v1/v2/v3_v4`, `ford_v0`, `psa`/`psa_bf_core`, `fiat_v0`, `honda_static`,
  `star_line`) — ein einziger Fix statt Einzel-Patches pro Decoder.
- Reine String-Logik, komplett ohne Hardware nachvollziehbar (siehe Testhinweis).

## Betroffene Dateien
- `protocols/protocols_common.c` — `pp_parse_hex_u64_strict()`.

Keine Änderung an `protocols_common.h`, an `pp_flipper_read_hex_u64()` oder an den einzelnen
Protokoll-Decodern nötig — der Fix bleibt vollständig innerhalb der Parser-Funktion und
ändert weder Signatur noch Rückgabewert-Semantik (`true`/`false` bleibt gleich benannt).

## Patch
```c
bool pp_parse_hex_u64_strict(const char* str, uint64_t* out_key) {
    if(!str || !out_key) {
        return false;
    }

    uint64_t key = 0;
    uint8_t hex_pos = 0;
    size_t i = 0;
    for(; str[i] != '\0' && hex_pos < 16; i++) {
        if(str[i] == ' ') {
            continue;
        }

        uint8_t nibble = 0;
        if(!pp_hex_nibble(str[i], &nibble)) {
            return false;
        }
        key = (key << 4) | nibble;
        hex_pos++;
    }

    if(hex_pos != 16) {
        return false;
    }

    // Nach der 16. Hex-Ziffer darf nur noch Leerraum folgen -- alles andere (weitere
    // Hex-Ziffern oder sonstige Zeichen) macht den String ungueltig statt ihn abzuschneiden.
    for(; str[i] != '\0'; i++) {
        if(str[i] != ' ') {
            return false;
        }
    }

    *out_key = key;
    return true;
}
```

Einzige Änderung: Nach der bestehenden `hex_pos != 16`-Prüfung läuft eine zweite Schleife
über den Rest des Strings ab Position `i` (dort, wo die erste Schleife stehen geblieben ist)
und lehnt ab, sobald dort ein Nicht-Leerzeichen auftaucht. Ein exakt 16-stelliger Hex-String
(mit beliebig vielen Leerzeichen dazwischen oder danach, wie bisher) verhält sich unverändert.

## Testhinweis (ohne Hardware)
Reine String-/Bit-Logik, host-kompilierbar nachvollziehbar. Reproduktionsprogramm
(`gcc strict_hex_check.c -o strict_hex_check && ./strict_hex_check`), das
`pp_hex_nibble()` und beide Varianten von `pp_parse_hex_u64_strict()` 1:1 nachbildet:

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static bool pp_hex_nibble(char c, uint8_t* nibble) {
    if(c >= '0' && c <= '9') { *nibble = (uint8_t)(c - '0'); }
    else if(c >= 'A' && c <= 'F') { *nibble = (uint8_t)(c - 'A' + 10); }
    else if(c >= 'a' && c <= 'f') { *nibble = (uint8_t)(c - 'a' + 10); }
    else { return false; }
    return true;
}

// Vorher (Bug): ignoriert alles nach der 16. Hex-Ziffer.
static bool parse_before(const char* str, uint64_t* out_key) {
    uint64_t key = 0;
    uint8_t hex_pos = 0;
    for(size_t i = 0; str[i] != '\0' && hex_pos < 16; i++) {
        if(str[i] == ' ') continue;
        uint8_t nibble = 0;
        if(!pp_hex_nibble(str[i], &nibble)) return false;
        key = (key << 4) | nibble;
        hex_pos++;
    }
    if(hex_pos != 16) return false;
    *out_key = key;
    return true;
}

// Nachher (Fix): lehnt Nicht-Leerraum nach der 16. Hex-Ziffer ab.
static bool parse_after(const char* str, uint64_t* out_key) {
    uint64_t key = 0;
    uint8_t hex_pos = 0;
    size_t i = 0;
    for(; str[i] != '\0' && hex_pos < 16; i++) {
        if(str[i] == ' ') continue;
        uint8_t nibble = 0;
        if(!pp_hex_nibble(str[i], &nibble)) return false;
        key = (key << 4) | nibble;
        hex_pos++;
    }
    if(hex_pos != 16) return false;
    for(; str[i] != '\0'; i++) {
        if(str[i] != ' ') return false;
    }
    *out_key = key;
    return true;
}

static void check(const char* label, const char* input) {
    uint64_t before_key = 0, after_key = 0;
    bool before_ok = parse_before(input, &before_key);
    bool after_ok = parse_after(input, &after_key);
    printf("%-32s input=%-24s vorher=%s (0x%016llX)  nachher=%s\n",
        label, input,
        before_ok ? "OK " : "REJECT",
        (unsigned long long)before_key,
        after_ok ? "OK" : "REJECT");
}

int main(void) {
    check("exakt 16 Hex-Ziffern",      "0123456789ABCDEF");
    check("mit Leerzeichen getrennt",  "0123 4567 89AB CDEF");
    check("16 Ziffern + 2 Hex-Muell",  "0123456789ABCDEF99");
    check("16 Ziffern + Leerraum",     "0123456789ABCDEF   ");
    check("16 Ziffern + Nicht-Hex",    "0123456789ABCDEFxy");
    check("zu kurz (15 Ziffern)",      "0123456789ABCDE");
    return 0;
}
```

Erwartete Ausgabe: Die Zeilen "16 Ziffern + 2 Hex-Muell" und "16 Ziffern + Nicht-Hex" zeigen
vorher `OK` mit dem stillschweigend abgeschnittenen Schlüssel `0x0123456789ABCDEF`, nachher
`REJECT`. Alle anderen Zeilen verhalten sich vorher/nachher identisch (`OK` für exakt
16-stellige Eingaben inkl. Leerzeichen davor/dazwischen/danach, `REJECT` für zu kurze
Eingaben) — der Fix engt also ausschließlich den bisher fälschlich akzeptierten
Trailing-Garbage-Fall ein.

Funktionaler Test danach lokal auf dem Flipper: eine `.sub`-Testdatei mit einem absichtlich
um zwei Hex-Ziffern verlängerten `Key: 0123456789ABCDEF99` anlegen, im Decoder/Emulate-Menü
laden — vor dem Fix wird sie mit falschem, abgeschnittenem Schlüssel akzeptiert, danach mit
Fehler-Log abgelehnt.

## Risiko
- Sehr niedrig: reine Verschärfung einer Eingabevalidierung, keine Struct-/API-Änderung, kein
  Einfluss auf das Schreiben von `.sub`-Dateien (ProtoPirate selbst schreibt den Key immer
  exakt 16-stellig, siehe `helpers/protopirate_storage.c`) und keine Verhaltensänderung für
  alle bisher gültigen (exakt 16-stelligen) Key-Strings.
- Einzig denkbare Nebenwirkung: eine zuvor mit stillschweigend abgeschnittenem Schlüssel
  „erfolgreich" geladene, tatsächlich fehlerhafte fremde `.sub`-Datei wird nach dem Fix
  korrekt abgelehnt statt (mit falschem Schlüssel) weiterverarbeitet — das ist die
  beabsichtigte Verbesserung, kein Regressionsrisiko für reguläre Dateien.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko — der Fix macht die Validierung strenger,
  eröffnet keine neue Funktion.

## Quelle
- `G4MEOVER18/ProtoPirate` (main): `protocols/protocols_common.c`
  (`pp_parse_hex_u64_strict`, `pp_flipper_read_hex_u64`), Aufrufer u. a. in `protocols/kia_v0.c`,
  `kia_v1.c`, `kia_v2.c`, `kia_v3_v4.c`, `ford_v0.c`, `psa.c`, `psa_bf_core.c`, `fiat_v0.c`,
  `honda_static.c`, `star_line.c`.
