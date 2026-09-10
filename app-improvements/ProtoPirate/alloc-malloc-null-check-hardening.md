# ProtoPirate — `*_alloc()`: `malloc()`-Rückgabewert wird an neun Stellen ungeprüft verwendet

## Was

Ich habe den seit mehreren Sitzungen mitgeschleppten Rückstellpunkt "`scher_khan.c`s
`malloc()`-ohne-`furi_check()`-Muster als übergreifender Prüfpunkt" jetzt vollständig
abgearbeitet: alle `*_alloc()`-Funktionen unter `protocols/*.c` systematisch danach
durchsucht, ob der `malloc()`-Rückgabewert vor der ersten Dereferenzierung geprüft wird.

Elf der insgesamt zwanzig Allokationsfunktionen sichern sich bereits ab — entweder mit
`furi_check(instance)` direkt nach `malloc()` (`ford_v3.c`, `honda_static.c` (beide),
`honda_v1.c` (beide), `kia_v0.c` (beide), `kia_v3_v4.c` (beide), `kia_v5.c`,
`kia_v6.c`-Decoder, `star_line.c` (beide)) oder mit einem expliziten `if(instance)`-
Zweig, der bei `NULL` kontrolliert `NULL` zurückgibt statt zu schreiben (`kia_v6.c`-Encoder,
`psa.c` (beide)). Neun Stellen in fünf Dateien tun das nicht — der `malloc()`-Rückgabewert
wird in der jeweils nächsten Zeile ungeprüft dereferenziert:

```c
// protocols/scher_khan.c — subghz_protocol_decoder_scher_khan_alloc()
SubGhzProtocolDecoderScherKhan* instance = malloc(sizeof(SubGhzProtocolDecoderScherKhan));
instance->base.protocol = &subghz_protocol_scher_khan;      // Schreibzugriff ungeprüft
instance->generic.protocol_name = instance->base.protocol->name;
```

Identisches Muster (Allokation, danach unmittelbar zwei bis mehr Schreibzugriffe ohne
Zwischenprüfung) an folgenden acht weiteren Stellen:

- `protocols/ford_v0.c`: `subghz_protocol_encoder_ford_v0_alloc()` (Zeile 360),
  `subghz_protocol_decoder_ford_v0_alloc()` (Zeile 610)
- `protocols/kia_v1.c`: `kia_protocol_encoder_v1_alloc()` (Zeile 150, nur unter
  `ENABLE_EMULATE_FEATURE`), `kia_protocol_decoder_v1_alloc()` (Zeile 350)
- `protocols/kia_v2.c`: `kia_protocol_encoder_v2_alloc()` (Zeile 158, nur unter
  `ENABLE_EMULATE_FEATURE`), `kia_protocol_decoder_v2_alloc()` (Zeile 259)
- `protocols/subaru.c`: `subghz_protocol_encoder_subaru_alloc()` (Zeile 275),
  `subghz_protocol_decoder_subaru_alloc()` (Zeile 426)

Jede dieser neun Funktionen wird vom SubGHz-Framework bei jedem Szenenwechsel in den
Empfänger/Emulator dieses Protokolls aufgerufen (Decoder-`alloc()` bei jedem Start einer
Empfangs-/Ladesitzung, Encoder-`alloc()` bei jedem Emulations-/Sendeversuch). Auf dem
Flipper Zero ist der Heap klein und geteilt (SubGHz-Keystore-Laden, NFC-Wörterbücher,
gleichzeitig offene Apps/Szenen) — ein `malloc()`-Fehlschlag bei Speicherdruck ist selten,
aber nicht ausgeschlossen, und genau dafür nutzt der Rest desselben Repos durchgängig
`furi_check()` direkt nach der Allokation als einheitliches Sicherheitsnetz. An den neun
genannten Stellen fehlt dieses Netz: bei `malloc() == NULL` schreibt der nächste Befehl
über einen NULL-Pointer, statt kontrolliert mit einer Diagnosemeldung abzubrechen.

## Nutzen

Vereinheitlicht das Fehlerverhalten bei Speichermangel auf das im übrigen Repo bereits
etablierte Muster (`furi_check()` unmittelbar nach jeder Allokation, wie es die anderen elf
Allokationsfunktionen bereits vormachen). Ein NULL-Pointer-Schreibzugriff ist undefiniertes
Verhalten; auf dem Gerät bedeutet das einen unkontrollierten Hardfault ohne die sonst übliche,
diagnostizierbare `furi_check()`-Absturzmeldung — und im ungünstigsten Fall (je nach
Speicherlayout an der jeweiligen Adresse) keinen sofortigen Fehler, sondern eine stille
Beschädigung von Speicher, der zufällig an der geschriebenen Adresse liegt, bevor irgendein
Fehler überhaupt sichtbar wird. Der Fix macht das Verhalten in allen zwanzig
Allokationsfunktionen einheitlich und sofort diagnostizierbar.

## Betroffene Dateien

- `protocols/ford_v0.c` — `subghz_protocol_encoder_ford_v0_alloc()`,
  `subghz_protocol_decoder_ford_v0_alloc()`
- `protocols/kia_v1.c` — `kia_protocol_encoder_v1_alloc()`,
  `kia_protocol_decoder_v1_alloc()`
- `protocols/kia_v2.c` — `kia_protocol_encoder_v2_alloc()`,
  `kia_protocol_decoder_v2_alloc()`
- `protocols/scher_khan.c` — `subghz_protocol_decoder_scher_khan_alloc()`
- `protocols/subaru.c` — `subghz_protocol_encoder_subaru_alloc()`,
  `subghz_protocol_decoder_subaru_alloc()`

## Port-/Patch-Skizze

Für jede der neun Stellen genügt eine Zeile direkt nach dem `malloc()`-Aufruf, exakt im
bereits an elf anderen Stellen dieses Repos verwendeten Stil. Beispiel `scher_khan.c`:

```c
void* subghz_protocol_decoder_scher_khan_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderScherKhan* instance = malloc(sizeof(SubGhzProtocolDecoderScherKhan));
    furi_check(instance);                                        // neu
    instance->base.protocol = &subghz_protocol_scher_khan;
    instance->generic.protocol_name = instance->base.protocol->name;

    return instance;
}
```

Dieselbe Ein-Zeilen-Ergänzung (`furi_check(instance);` unmittelbar nach der
`malloc()`-Zeile, vor dem ersten Schreibzugriff) an den übrigen acht Stellen:

- `ford_v0.c:360` → nach `SubGhzProtocolEncoderFordV0* instance = malloc(...);`
- `ford_v0.c:610` → nach `SubGhzProtocolDecoderFordV0* instance = malloc(...);`
- `kia_v1.c:150` → nach `SubGhzProtocolEncoderKiaV1* instance = malloc(...);`
- `kia_v1.c:350` → nach `SubGhzProtocolDecoderKiaV1* instance = malloc(...);`
- `kia_v2.c:158` → nach `SubGhzProtocolEncoderKiaV2* instance = malloc(...);`
- `kia_v2.c:259` → nach `SubGhzProtocolDecoderKiaV2* instance = malloc(...);`
- `subaru.c:275` → nach `SubGhzProtocolEncoderSubaru* instance = malloc(...);`
- `subaru.c:426` → nach `SubGhzProtocolDecoderSubaru* instance = malloc(...);`

`furi_check` ist in allen fünf Dateien bereits eingebunden und wird an anderer Stelle
derselben Datei bereits verwendet (`free()`/`reset()`/`feed()` prüfen `context` durchgängig
per `furi_check(context)`) — der Fix zieht keine neuen Includes nach sich.

## Risiko

Sehr gering. `furi_check()` ist im normalen Betrieb (Allokation gelingt) ein reiner
No-Op-Pfad — die Funktion verhält sich für den Erfolgsfall byte-identisch zu vorher. Die
einzige Verhaltensänderung betrifft den Fehlerfall (`malloc() == NULL`), der vorher
undefiniertes Verhalten auslöste und jetzt kontrolliert mit `furi_check()`s
Standard-Absturzverhalten abbricht — exakt das bereits an den anderen elf Allokationsstellen
etablierte Verhalten dieses Repos.

## Testhinweis

Lokal an allen neun Stellen die Zeile `furi_check(instance);` direkt nach dem jeweiligen
`malloc()`-Aufruf einfügen, bauen/flashen. Da ein echter Speichermangel auf dem Gerät nicht
gezielt herbeigeführt werden kann, ist der Fixnachweis Code-Review (Diff enthält
ausschließlich die neun `furi_check(instance);`-Zeilen, keine sonstige Verhaltensänderung)
plus Regressionstest im Normalfall (jedes der fünf betroffenen Protokolle je einmal
empfangen/senden — Ford V0, Kia V1, Kia V2, Scher-Khan, Subaru —, Verhalten muss
unverändert bleiben). Zusätzlich Host-Repro in
`repro-alloc-malloc-null-check-hardening/repro.c`: das in allen neun Fundstellen
identische Strukturmuster (Allokation, danach zwei Schreibzugriffe auf `base.protocol`/
`generic.protocol_name`) isoliert nachgebaut, `malloc()` durch einen Stub ersetzt, der
Speichermangel simuliert (liefert `NULL`, wie es echtes `malloc()` bei erschöpftem Heap
ebenfalls täte), beide Fassungen (ALT ohne Prüfung, NEU mit `furi_check()`-Simulation) je in
einem eigenen `fork()`-Kindprozess ausgeführt, unter `gcc -O0 -Wall -Wextra` (bewusst ohne
AddressSanitizer, damit das reale `SIGSEGV`-Signal des Betriebssystems sichtbar wird statt
ASan's abgefangenem Bericht) gebaut und verifiziert: ALT beendet den Kindprozess
unkontrolliert per `SIGSEGV` (Schreibzugriff über NULL-Pointer), NEU beendet ihn kontrolliert
mit eigenem Exit-Code und Diagnosemeldung auf `stderr`, *bevor* irgendein Schreibzugriff
stattfindet.

## Quelle

Eigener Fund, cross-cutting: seit mehreren Sitzungen als "`scher_khan.c`s
`malloc()`-ohne-`furi_check()`-Muster" vorgemerkter Rückstellpunkt (offen "an 2-3 weiteren
Decodern zu verifizieren"), jetzt für den gesamten `protocols/`-Baum (`G4MEOVER18/ProtoPirate`,
`main`, Commit `e3d9da5`) systematisch nachgeprüft (`grep`-gestützte Vollerhebung aller
`= malloc(` in `protocols/*.c`, jede Fundstelle einzeln gegen die jeweils folgenden Zeilen
geprüft) — neun statt der vermuteten zwei bis drei Stellen bestätigt.
