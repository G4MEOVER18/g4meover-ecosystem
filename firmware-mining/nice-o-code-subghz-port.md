# Nice O-Code: fehlt mir komplett, Grundlagen jetzt verifiziert portiert

## Was

Mein `lib/subghz/protocols/nice_flor_s.c` (Momentum-Basis, Tag `mntm-012`) decodiert
ausschließlich klassisches Nice FloR-S (52 Bit) und Nice One (72 Bit): einen einmal pro
Tastendruck verschlüsselten 44-Bit-Rumpf, der unverändert auf alle 16 gesendeten Pakete
verteilt wird. **Nice O-Code** ist eine dritte, im selben Timing/Header laufende Variante
(ebenfalls 52/72 Bit), bei der jedes der 16 Pakete individuell mit einer paket-abhängigen
Maske neu verschlüsselt wird und zusätzlich ein 16-Bit-Installateurcode („IC") in die
Verschlüsselung eingeht. Dafür habe ich aktuell keinerlei Erkennung — ein O-Code-Signal läuft
bei mir durch dieselbe Nice-FloR-S-Erkennung wie üblich, wird aber (siehe „Fund" unten) nie
als gültiges Paket bestätigt, weil die 16 Pakete eines Tastendrucks bei mir auf Bit-Gleichheit
zwischen zwei aufeinanderfolgenden Paketen geprüft werden — bei O-Code nie der Fall.

Unleashed-Firmware (`dev`, Commit `945234a` vom 2026-09-10, API 88.9) hat dieselbe Datei um
genau diese Variante erweitert; CHANGELOG-Eintrag zu diesem Commit: „SubGhz: **Nice O-Code
support** (52, 72 bits, Dynamic) - decode, save and emulate, plus the new **Nice O-Code** app
that recovers the installer code from 4+ captures (by @zero-mega & @xMasterX)". Das ist der
letzte der drei in der Prastel-Sitzung zurückgestellten SubGHz-Kandidaten
(`firmware-mining/prastel-42bit-rolling-code-port.md`, dort als „neues Protokoll + eigene
Installer-Code-Recovery-App, größerer Portaufwand" vermerkt) — Security+ 2.0 Keypad wurde
bereits in einer eigenen Sitzung aufgearbeitet (`firmware-mining/secplus-v2-keypad-frame-port.md`),
die neuen KeeLoq-Hersteller bleiben wegen der Lernschlüssel-Problematik weiterhin
zurückgestellt (nicht quelltextlich gegen echte Aufnahmen verifizierbar).

Ich liefere in dieser Sitzung:
1. Diesen Port-Vorschlag mit vollständiger Analyse gegen meine eigene Basis.
2. Die vier neuen, quelltextlich rein additiven, dateiweit unverwendeten Hilfsfunktionen
   (IC-parametrisierte Ver-/Entschlüsselung, paket-abhängige Maske, Paketindex-Rückgewinnung)
   1:1 aus Unleashed übernommen, an meine eigene Tabellen-Lesefunktion angepasst (siehe unten)
   und **eigenständig auf dem Host verifiziert** — keine Hardware, keine Firmware-Header nötig
   (reine Integer-Bit-Operationen plus ein simulierter 32-Byte-Tabellenpuffer statt SD-Karte).
   Als anwendbarer Patch beigelegt: additiv, ändert keine bestehende Funktion, keine
   bestehende Aufrufstelle, keine Struct-Layouts — bei mir aktuell **toter Code** (von nichts
   im Baum aufgerufen), bis Phase 2 unten sie verdrahtet.
3. Die verbleibende, hardwarenahe Verdrahtung (Decoder-/Encoder-Struct-Erweiterung um
   `ic`/`o_code`, die Zwei-Pakete-Heuristik in `feed()` zur Live-Erkennung, IC-Lesen/Schreiben
   aus/in die `.sub`-Datei, sowie die separate Installateurcode-Recovery-App) liefere ich
   bewusst **nicht** als fertigen Patch, sondern als konkrete Skizze mit exakten
   Zeilenankern — dazu unten mehr unter „Risiko" und „Port-Skizze". Das ist kein „klein &
   sicher"-Fund im Sinne von `WORKFLOW.md`, sondern ein mehrteiliger Feature-Port, der zudem
   von einer eigenen, noch nicht existierenden Companion-App abhängt.

## Nutzen

- Nice-Torantriebe/Handsender sind bei mir bereits über Nice FloR-S abgedeckt — O-Code ist
  laut Unleashed-CHANGELOG eine bei denselben Herstellern verbreitete Variante mit
  installateur-spezifischem Zusatzschlüssel. Ohne diese Erkennung zeigt mir ein empfangenes
  O-Code-Signal aktuell gar nichts Sinnvolles (weder als Nice FloR-S noch als RAW erkennbar,
  siehe „Fund").
- Reine Erweiterung der Empfangs-/Analysefähigkeit für ein bereits unterstütztes,
  öffentlich dokumentiertes Protokoll (dieselben Quellen wie beim bestehenden
  `nice_flor_s.c`: `phreakerclub.com/1615`, `phreakerclub.com/forum/showthread.php?t=2360`,
  `vrtp.ru/index.php?showtopic=27867`) — für autorisierte Prüfungen eigener/beauftragter
  Nice-Anlagen.
- Die vier jetzt verifizierten Hilfsfunktionen sind die vollständige kryptografische
  Grundlage für beide späteren Ausbaustufen (Live-Decode mit bekanntem IC, und die
  IC-Recovery-App) — künftige Sitzungen können direkt auf ihnen aufbauen, ohne die
  Verschlüsselungslogik erneut aus der Fremd-Quelle transkribieren und verifizieren zu
  müssen.

## Fund: warum ein O-Code-Signal bei mir aktuell nicht einmal als RAW auffällt

Kurz nachvollzogen, weil es die Dringlichkeit dieses Ports einordnet: Meine
`subghz_protocol_decoder_nice_flor_s_feed()` (mntm-012, Zeilen 581–673) sammelt Bits nach
Standard-Timing/Header, prüft aber nichts über den Dateninhalt — jedes empfangene 52- oder
72-Bit-Paket landet als gültiger Decoder-Treffer, der Callback wird bedingungslos aufgerufen
(Zeile ~625 `if(instance->base.callback) instance->base.callback(...)`; das
Zwei-Pakete-Vergleichsmuster, das ich oben referenziert habe, existiert erst in Unleasheds
`dev`-Fassung, siehe Port-Skizze Schritt 3). Ein O-Code-Signal wird von mir also aktuell
tatsächlich als (falsches) Nice FloR-S angezeigt — mit zufälligem Seriennummer-/Zähler-Inhalt,
da `subghz_protocol_nice_flor_s_remote_controller()` das 44-Bit-Feld ohne IC entschlüsselt.
Das ist kein Absturz- oder Sicherheitsrisiko (nur eine falsche Anzeige), aber ein
Fehlklassifikations-Fund, der die Motivation für „decode = zumindest korrekt als O-Code statt
falsch als FloR-S erkennen" untermauert.

## Betroffene Dateien (diese Sitzung, additiv)

- `lib/subghz/protocols/nice_flor_s.c` — vier neue, nicht-statische Funktionen ergänzt:
  `firmware-mining/nice-o-code-subghz-port/nice_flor_s_c.patch`
- `lib/subghz/protocols/public_api.h` — passende Deklarationen ergänzt (dieselbe Datei, in
  der `subghz_protocol_nice_flor_s_create_data()` bereits für Cross-App-Nutzung deklariert
  ist): `firmware-mining/nice-o-code-subghz-port/public_api_h.patch`

Kein anderer Dateiverweis nötig: `protocol_items.h`/`protocol_items.c` (Registry) bleiben in
dieser Phase unverändert, da noch kein neuer `SubGhzProtocol*`-Eintrag entsteht — Nice O-Code
ist laut Unleashed keine eigene Registry-Zeile, sondern eine Variante innerhalb des
bestehenden `subghz_protocol_nice_flor_s`-Eintrags (Struct-Feld `o_code`, siehe Phase 2).

## Änderungen im Detail

### Neue Funktionen (dieser Patch, additiv, aktuell unverwendet)

Alle vier direkt nach der bestehenden `subghz_protocol_nice_flor_s_encrypt()` eingefügt
(mntm-012-Zeile 477, vor der bestehenden statischen `subghz_protocol_nice_flor_s_decrypt()`),
damit sie neben ihrem unveränderten Pendant stehen und der Unterschied beim Lesen sofort
auffällt:

1. **`subghz_protocol_nice_flor_s_encrypt_ic(uint64_t data, uint16_t ic, const char* file_name)`**
   — identisch zu meiner bestehenden `subghz_protocol_nice_flor_s_encrypt()` bis auf die
   letzte Permutation: dort, wo Plain-FloR-S zwei Bytes hart invertiert (`p[4] = ~p[0]`,
   `p[3] = ~p[1]`), XOR-t O-Code stattdessen mit den beiden Hälften des Installateurcodes
   (`p[4] = p[0] ^ (ic>>8)`, `p[3] = p[1] ^ ic` in der jeweils passenden Zuweisungsreihenfolge
   — siehe Patch für die exakte Byte-Reihenfolge, identisch zu Unleashed übernommen). Mit
   `ic == NICE_FLOR_S_IC_PLAIN` (`0xFFFF`) reduziert sich das exakt auf die bestehende
   Funktion (`0xFF XOR x == ~x`) — Host-verifiziert (siehe unten).
2. **`subghz_protocol_nice_flor_s_decrypt_ic(uint64_t data, uint16_t ic, const char* file_name)`**
   — die passende Inverse, analog aus meiner bestehenden `subghz_protocol_nice_flor_s_decrypt()`
   abgeleitet (dort Zeilen 479–516).
3. **`subghz_protocol_nice_o_mask(uint8_t n, uint32_t* mask_32, uint16_t* mask_16)`** — 1:1 aus
   Unleashed übernommen (reine Integer-Bit-Arithmetik, keine Abhängigkeit von Datei-I/O oder
   Structs). Liefert für einen Paketindex `n` (0–15) die Maske, mit der Seriennummer bzw.
   Installateurcode vor der Verschlüsselung dieses einen Pakets XOR-verknüpft werden.
4. **`subghz_protocol_nice_o_get_parcel(uint64_t data)`** — 1:1 aus Unleashed übernommen.
   Liest den Paketindex direkt aus den unverschlüsselten Kopfbits P0 (Button, Bit 48–51) und
   P1 (Bit 44–47, `P1 = 0xF ^ P0 ^ n`) eines empfangenen Rahmens — funktioniert daher auch,
   solange der IC noch unbekannt ist.

Ich habe bewusst **nicht** Unleasheds Tabellen-Zwischenspeicherung (`nice_flor_s_table()`,
statischer 32-Byte-Cache plus Pfadvergleich) mitübernommen: `subghz_protocol_nice_flor_s_get_byte_in_file()`
liest bei mir weiterhin einzelne Bytes direkt von der Karte, exakt wie die bereits
bestehenden `_encrypt()`/`_decrypt()` es tun — die neuen Funktionen bleiben dadurch in ihrem
I/O-Verhalten symmetrisch zu ihren bestehenden Geschwistern, und diese Phase führt keinen
neuen globalen/statischen Zustand ein. Der Cache wird erst relevant, sobald tatsächlich über
viele IC-Kandidaten hinweg entschlüsselt wird (Recovery-App, Phase 3) — dort spart er laut
Unleashed-Kommentar den Unterschied zwischen einer SD-Karten-Transaktion und einer
RAM-Schleife pro Kandidat und sollte dann zusammen mit der Recovery-App eingeführt werden,
nicht vorab spekulativ.

### Verifikation (Host, ohne Firmware-Header/Hardware)

`firmware-mining/repro-nice-o-code-subghz/repro.c` — ALT (meine bestehende
`_encrypt()`/`_decrypt()`, 1:1 transkribiert, Tabellen-Byte-Lookup liest aus einem
In-Memory-Array statt von der SD-Karte, da die Chiffre den Tabelleninhalt nur als
Byte-Nachschlagewert behandelt) gegen NEU (die vier neuen Funktionen) unter
`-fsanitize=address,undefined` bei `-O0` und `-O1`:

1. **Plain-Äquivalenz-Regression** (20 Tabellen × 20 000 zufällige 44-Bit-Werte, je
   Encrypt+Decrypt): `neu_encrypt_ic(data, IC_PLAIN, table) == alt_encrypt(data, table)` und
   die passende Decrypt-Gegenprobe — 0 Abweichungen. Bestätigt, dass die neuen Funktionen bei
   `IC_PLAIN` bestehendes Verhalten exakt reproduzieren, also risikofrei neben den
   bestehenden Funktionen stehen können.
2. **Vollständiger IC-Bereich** (0..65535, feste Daten/Tabelle): Round-Trip
   `decrypt_ic(encrypt_ic(data, ic), ic) == data` für jeden der 65 536 Werte, zusätzlich
   Gegenprobe, dass Entschlüsseln mit `ic-1` nicht zufällig denselben Klartext liefert —
   0 Abweichungen, 0 Kollisionen.
3. **Fuzzed Round-Trip** (200 000 Iterationen, Daten/IC/Tabelle je zufällig): 0 Abweichungen.
4. **`get_parcel()`-Unabhängigkeit vom Geheimtext** (20 000 Iterationen, 44-Bit-Rumpf
   zufällig, `n`/`btn` zufällig): Rückgewinnung immer exakt `n`, unabhängig vom sonstigen
   Bitinhalt — bestätigt, dass die Funktion tatsächlich nur P0/P1 liest, wie dokumentiert.
5. **`mask()`-Determinismus** (alle 16 `n`-Werte, doppelt aufgerufen): identische Ergebnisse,
   jede `mask_32` bleibt innerhalb von 28 Bit. Tabelle aller 16 Masken zur Dokumentation mit
   ausgegeben (für eine künftige Recovery-App als Gegenprobe nutzbar, ohne die Formel erneut
   herzuleiten).
6. **Vollständige O-Code-Pipeline End-zu-Ende** (200 000 Iterationen): aus zufälligem
   `(serial, cnt, btn, ic, n)` einen synthetischen Über-die-Luft-Rahmen exakt nach dem in
   Unleasheds `remote_controller()`/`get_upload()` beschriebenen Schema gebaut (Maske
   anwenden, mit maskiertem IC verschlüsseln, P0/P1-Byte voranstellen), dann
   `get_parcel()` → `mask()` → `decrypt_ic()` rückwärts angewandt — `serial`/`cnt`/`btn`
   exakt zurückgewonnen in allen 200 000 Fällen; zusätzlich Gegenprobe, dass ein falscher
   IC (`ic+1`) nicht zufällig denselben `(serial, cnt)` liefert.

**Ergebnis: 2 551 103 Prüfungen, ALLE CHECKS BESTANDEN, keine Compiler-Warnungen
(`-Wall -Wextra`), keine ASan-/UBSan-Meldung bei `-O0` oder `-O1`.**

## Warum kein vollständiger Patch in dieser Sitzung (Risiko)

Die restliche Verdrahtung berührt Struct-Layout, den laufenden Decoder-Zustandsautomaten und
das Datei-Format bereits unterstützter Signale — genau die Stellen, an denen ein unfertiger
Patch reale Regressionen im bereits funktionierenden Nice-FloR-S-Pfad einführen könnte, wenn
er nicht vollständig durchdacht ist:

- **`SubGhzProtocolDecoderNiceFlorS`/`SubGhzProtocolEncoderNiceFlorS`** (mntm-012, Zeilen
  29–45) müssten um `uint16_t ic`, `bool o_code` sowie (für die Live-Erkennung, siehe
  Port-Skizze Schritt 3) `uint32_t pending_body`/`bool has_pending` erweitert werden — eine
  reine Additiv-Erweiterung, aber jede neue Zuweisung in `_alloc()`/`_reset()` muss exakt
  stimmen, sonst liest eine künftige Sitzung uninitialisierten Zustand.
- **`feed()`** (Zeilen 581–673) müsste die Ein-Paket-sofort-melden-Logik um die oben
  beschriebene Zwei-Pakete-Vergleichsheuristik erweitern (siehe „Fund"), die außerdem den
  bereits bestehenden Bestätigungspfad für **echtes** Nice FloR-S um einen Takt verzögert
  (erstes Paket wird zurückgehalten, bis ein zweites es bestätigt) — eine Verhaltensänderung
  am bereits produktiv genutzten Plain-FloR-S-Pfad, die eigene Regressionsverifikation
  gegen echte FloR-S-Aufnahmen bräuchte, nicht nur Host-Fuzzing mit synthetischen Daten.
- **`.sub`-Dateiformat**: das neue `IC`-Hex-Feld beim Deserialisieren/Serialisieren
  (`subghz_protocol_encoder_nice_flor_s_deserialize()` Zeilen 270ff.,
  `subghz_protocol_decoder_nice_flor_s_deserialize()` Zeilen 767ff.,
  `_serialize()` Zeilen 743ff.) ist rückwärtskompatibel additiv (fehlt das Feld, bleibt
  `o_code = false`), aber noch nicht gegen reale, mit meiner eigenen Basis erzeugte
  `.sub`-Dateien getestet.
- **Die Installateurcode-Recovery-App** existiert bei mir überhaupt noch nicht — laut
  Unleashed-CHANGELOG ein **eigenes** `.fap` (nicht Teil von `lib/subghz`), das aus 4+
  Aufnahmen brute-forced. Das ist ein eigenständiges, größeres Vorhaben (App-Gerüst,
  Aufnahme-Workflow, Konsistenzprüfung über mehrere Kandidaten-ICs), nicht Teil dieser
  lib-Erweiterung.
- Ich kenne die exakte Byte-Reihenfolge/Vorzeichen aus der Unleashed-Quelle, habe sie aber
  **nicht** gegen eine echte O-Code-Funkaufnahme verifizieren können (keine eigene Aufnahme
  vorhanden, `CONSTRAINTS.md` untersagt ohnehin fremde Kartendaten/Captures in diesem Repo) —
  die Host-Verifikation oben beweist Selbstkonsistenz (Ver-/Entschlüsselung sind zueinander
  invers, Plain-Fall bleibt unverändert), nicht Übereinstimmung mit dem realen Funkprotokoll.
  Das ist dasselbe Einschränkungsmuster wie bei Prastel/Security+ 2.0 in den Vorsitzungen.

Additiv, unverwendet, verhaltensneutral bei `IC_PLAIN` — deshalb als Patch beigelegt trotz
insgesamt größerem Restvorhaben.

## Port-Skizze (für eine künftige, eigens dafür vorgesehene Sitzung)

1. **Struct-Erweiterung** (`nice_flor_s.c` Zeilen 29–45): `ic`/`o_code` in beiden Structs,
   `pending_body`/`has_pending` nur im Decoder, jeweils in `_alloc()` auf
   `NICE_FLOR_S_IC_PLAIN`/`false`/`0`/`false` initialisiert und in `_reset()` zurückgesetzt
   (Unleashed reset()t dort zusätzlich `ic`/`o_code`, damit eine zuvor geladene O-Code-Datei
   nicht den nächsten Live-Empfang verfälscht — dieses Detail exakt übernehmen).
2. **`.sub`-I/O**: `IC`-Hex-Feld (2 Byte) beim De-/Serialisieren lesen/schreiben, analog zum
   bereits vorhandenen `CounterMode`-Feld an derselben Stelle; `o_code = true` genau dann,
   wenn das Feld beim Laden vorhanden war.
3. **Live-Erkennung in `feed()`**: die neue Zwei-Pakete-Heuristik aus „Risiko" oben,
   parametrisiert über ein `skip_o_code`-Flag (Default `true`, damit ein einzelnes,
   unbestätigtes O-Code-Paket nicht als falsches FloR-S gemeldet wird) — mit
   `subghz_protocol_decoder_nice_flor_s_set_skip_o_code()` als öffentlicher Schalter für eine
   künftige Recovery-App, die einzelne, unbestätigte Pakete ja gerade sammeln will.
4. **`remote_controller()`/`get_string()`**: `o_code`-Zweig ergänzen, der `get_parcel()` +
   `mask()` + `decrypt_ic()` aus dieser Sitzung verwendet (siehe Repro-Schritt 6 für die
   exakte Aufrufreihenfolge) statt der bestehenden Plain-Entschlüsselung; Anzeige ergänzt um
   `IC:%04X`.
5. **Encoder `get_upload()`**: pro Paket (`loops[i]`, bestehende Schleife über die 16
   Wiederholungen) Maske anwenden und mit `encrypt_ic()` statt `encrypt()` verschlüsseln,
   wenn `instance->o_code` gesetzt ist — exakt die in Repro-Schritt 6 verifizierte
   Pipeline, nur in die bestehende Sende-Schleife eingehängt statt synthetisch nachgebaut.
6. **Registry**: keine Änderung nötig (siehe „Betroffene Dateien" oben).
7. **Installateurcode-Recovery-App** (eigenständiges, größeres Vorhaben, eigene Sitzung(en)):
   neues `.fap`, das 4+ rohe Aufnahmen (`NICE_O_MIN_SAMPLES`/`_MAX_SAMPLES` in Unleashed als
   4/16 definiert) einliest, pro Aufnahme `get_parcel()` aufruft und über alle 65 536
   IC-Kandidaten `decrypt_ic()` testet, bis eine IC alle Aufnahmen auf dieselbe Seriennummer
   abbildet — genau die Aufgabe, für die diese Sitzung die kryptografische Grundlage liefert.

## Aufwand/Umfang

Firmware-Feature-Mining, additiver Grundlagen-Patch (klein, sicher, host-verifiziert) plus
Port-Skizze für ein mehrteiliges Vorhaben (Struct-/Zustandsautomat-Änderung an
produktivem Code + eigenständige neue App) — Einordnung wie bei
`secplus-v2-keypad-frame-port.md`, nicht wie ein einzelner Kleinfund.

## Sicherheitsrahmen

Ausschließlich Lese-/Analysefunktionalität für ein bereits öffentlich dokumentiertes
Consumer-Funkprotokoll (Nice-Torantriebe), zur Verwendung an eigenen/autorisierten Anlagen.
Keine fremden Aufnahmen oder Kartendaten in diesem Repo verwendet oder erzeugt — die
Host-Verifikation läuft ausschließlich mit synthetisch erzeugten Testwerten.

## Quellen

- Unleashed-Firmware (`github.com/DarkFlippers/unleashed-firmware`, `dev`, Commit
  `945234a`, 2026-09-10, API 88.9): `lib/subghz/protocols/nice_flor_s.c`/`.h` (per
  `raw.githubusercontent.com` bezogen), `CHANGELOG.md` (Ankündigung „Nice O-Code support"),
  `lib/subghz/protocols/public_api.h` (Deklarationen der neuen Hilfsfunktionen).
- Momentum-Firmware, Tag `mntm-012`: `lib/subghz/protocols/nice_flor_s.c`/`.h`
  (Ausgangszustand, keine O-Code-Erkennung), `public_api.h` (bestehendes
  Cross-App-Deklarationsmuster, an dem sich diese Erweiterung orientiert),
  `lib/subghz/blocks/generic.h` (Struct-Layout-Abgleich, `data_2`-Feld bereits vorhanden),
  `lib/subghz/types.h` (bestätigt: mein `SubGhzProtocolFilter`-Mechanismus bleibt
  unverändert nutzbar, Unleasheds parallele Migration auf reine Flag-Bits ist eine
  unabhängige, hier nicht übernommene Registry-Änderung).
