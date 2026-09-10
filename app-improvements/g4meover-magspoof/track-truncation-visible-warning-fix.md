# g4meover-magspoof — sichtbare Warnung, wenn `mag_encode()` Track-Daten kappt

## Was
Setzt auf `mag-encode-fixed-buffer-overflow-fix.md` auf (dort als offener Folgepunkt im
Risiko-Abschnitt vermerkt: „für eine spätere Sitzung wäre ergänzend eine sichtbare Warnung
(Popup/Log) beim Kappen sinnvoll"). Nach diesem bereits vorgeschlagenen Fix kappt
`mag_encode()` überlange Track-Daten sauber an der tatsächlichen Puffergrenze
(`bits_t1_raw[64]`/`bits_t1_manchester[128]` usw.), statt abzustürzen — meldet die Kappung
aber an niemanden. `mag_spoof()` ruft die Funktion einfach auf und sendet, was auch immer
zurückkommt; weder der Aufrufer noch ich als Nutzer erfahre, dass ein Teil der Karte gar
nicht gesendet wurde.

Das ist bei mir über dieselben drei Eingabewege real erreichbar, die schon im Overflow-Fund
dokumentiert sind (UART-Kartenlesung, geladene `.mag`-Datei, manuelle 150-Zeichen-Eingabe):
sobald ein Track länger als 72 (Track 1) bzw. 101 (Track 2/3) Zeichen ist, sendet „Send" ein
unvollständiges Signal, ohne dass ich das am Gerät irgendwie merke — der Bildschirm zeigt
weiterhin den vollständigen (ungekappten) Track-Text aus `mag_scene_emulate_on_enter()`, nur
das tatsächlich abgestrahlte Signal ist kürzer. Bei einem Pentest-Einsatz (autorisierter Test
eines Kartenlesers) sähe das wie ein normal gesendetes, aber vom Leser abgelehntes Signal aus
— der eigentliche Grund (verworfene Endzeichen inkl. eines potenziell falschen LRC-Bytes, da
das LRC nur über die tatsächlich gesendeten Zeichen berechnet wird) bliebe unsichtbar.

Mit einem eigenständigen, hostseitigen Nachbau von `mag_encode()` (identisch zum bereits
vorgeschlagenen kapazitätsbegrenzten Stand, ohne Furi-/Flipper-Typen) verifiziert: eine
72-Zeichen- und eine 73-Zeichen-Eingabe für Track 1 erzeugen exakt denselben Bitcount/dieselbe
Bitfolge — aus dem Rückgabewert allein ist eine Kappung nicht erkennbar (siehe Testhinweis).

## Nutzen
- Macht eine bereits heute stillschweigend mögliche Signalverstümmelung für mich sichtbar,
  statt sie nur „sicher" (ohne Absturz) ablaufen zu lassen — genau der im vorherigen Fix
  offen gelassene Punkt.
- Zentral in `mag_encode()` selbst erkannt: beide Aufrufstellen (`mag_spoof()`,
  `debug_mag_string()`) profitieren automatisch von der Information, ohne dass ich an den
  drei Eingabewegen einzeln je eine eigene Längenprüfung ergänzen müsste.
- Der neue Ausgabeparameter ist optional (`NULL` weiterhin gültig) — keine der bestehenden
  Aufrufstellen muss sich ändern, wenn sie die Information nicht braucht.
- Reine Zusatzinformation, kein Verhaltensunterschied im gesendeten Signal selbst (durch
  Host-Repro bestätigt: Bitausgabe bei gleicher Eingabe unverändert zu vorher).
- Die eigentliche Signal-Logik bleibt vollständig ohne Hardware nachrechenbar; nur die
  Benachrichtigung selbst (ein bereits an anderer Stelle im selben Ökosystem verifizierter
  `notification_message(..., &sequence_error)`-Aufruf) braucht einen lokalen Bautest.

## Betroffene Dateien (lokal, in meiner MagSpoof-App)
- `helpers/mag_helpers.h` — Prototyp von `mag_encode()` um den optionalen Parameter
  `bool* out_truncated` erweitert (letzter Parameter, nach dem bereits vorgeschlagenen
  `out_raw_capacity_bytes`).
- `helpers/mag_helpers.c` — `mag_encode()`: setzt `*out_truncated` (falls nicht `NULL`) auf
  `false` beim Eintritt und auf `true` genau an der bereits vorhandenen Kappungsstelle
  (`break`). `mag_spoof()`: ruft `mag_encode()` für alle drei Tracks mit je einer eigenen
  `bool`-Variable auf, verknüpft sie per `||` und schreibt das Ergebnis in
  `mag->last_tx_truncated`; zusätzlich `FURI_LOG_W(TAG, ...)` mit den drei Einzelflags, wenn
  mindestens einer gesetzt ist.
- `mag_i.h` — `Mag`-Struct: neues Feld `bool last_tx_truncated;` (wird von `mag_alloc()` nicht
  explizit initialisiert, siehe Risiko-Abschnitt — unkritisch, da `mag_spoof()` es bei jedem
  Aufruf unbedingt neu setzt, bevor es irgendwo gelesen wird).
- `scenes/mag_scene_emulate.c` — `mag_scene_emulate_on_event()`, `GuiButtonTypeRight`-Zweig:
  nach `mag_spoof(mag)`/`sequence_blink_stop` zusätzlich
  `notification_message(mag->notifications, &sequence_error)`, wenn
  `mag->last_tx_truncated` gesetzt ist. `sequence_error` ist bereits an anderer Stelle in
  diesem Ökosystem gegen den eigenen `mntm-012`-Firmware-Stand verifiziert (siehe
  `firmware-mining/nfctools-commands-missing-protocol-guard.md`).

Keine Änderung an `add_bit()`/`add_bit_manchester()`, an `MagDevice`/`MagState` oder am
`.mag`-Dateiformat nötig.

## Patch
```c
// helpers/mag_helpers.h
uint16_t mag_encode(
    char* data,
    uint8_t* out_manchester,
    uint8_t* out_raw,
    uint8_t track_bits,
    uint8_t track_ascii_offset,
    uint16_t out_raw_capacity_bytes,
    bool* out_truncated); // NEU: optional (NULL erlaubt), auf true gesetzt bei Kappung
```

```c
// helpers/mag_helpers.c
uint16_t mag_encode(
    char* data,
    uint8_t* out_manchester,
    uint8_t* out_raw,
    uint8_t track_bits,
    uint8_t track_ascii_offset,
    uint16_t out_raw_capacity_bytes,
    bool* out_truncated) { // NEU
    uint16_t raw_bits_count = 0;
    uint16_t output_count = 0;
    int tmp, crc, lrc = 0;

    if(out_truncated) *out_truncated = false; // NEU

    const uint16_t raw_bit_capacity = (uint16_t)(out_raw_capacity_bytes * 8);
    const uint16_t max_data_bits =
        (raw_bit_capacity > track_bits) ? (uint16_t)(raw_bit_capacity - track_bits) : 0;

    for(int i = 0; *(data + i) != 0; i++) {
        if((uint16_t)(raw_bits_count + track_bits) > max_data_bits) {
            if(out_truncated) *out_truncated = true; // NEU
            break;
        }

        crc = 1;
        tmp = *(data + i) - track_ascii_offset;

        for(int j = 0; j < track_bits - 1; j++) {
            crc ^= tmp & 1;
            lrc ^= (tmp & 1) << j;
            raw_bits_count = add_bit(tmp & 0x01, out_raw, raw_bits_count);
            output_count = add_bit_manchester(tmp & 0x01, out_manchester, output_count);
            tmp >>= 1;
        }
        raw_bits_count = add_bit(crc, out_raw, raw_bits_count);
        output_count = add_bit_manchester(crc, out_manchester, output_count);
    }

    // LRC byte (unveraendert)
    tmp = lrc;
    crc = 1;
    for(int j = 0; j < track_bits - 1; j++) {
        crc ^= tmp & 0x01;
        raw_bits_count = add_bit(tmp & 0x01, out_raw, raw_bits_count);
        output_count = add_bit_manchester(tmp & 0x01, out_manchester, output_count);
        tmp >>= 1;
    }
    raw_bits_count = add_bit(crc, out_raw, raw_bits_count);
    output_count = add_bit_manchester(crc, out_manchester, output_count);

    return output_count;
}
```

```c
// mag_i.h -- Mag-Struct
typedef struct {
    ...
    MagState state;
    bool last_tx_truncated; // NEU: von mag_spoof() bei jedem Send-Aufruf gesetzt
    ...
} Mag;
```

```c
// helpers/mag_helpers.c -- mag_spoof()
bool truncated1 = false, truncated2 = false, truncated3 = false; // NEU

uint16_t bits_t1_count = mag_encode(
    data1, (uint8_t*)bits_t1_manchester, (uint8_t*)bits_t1_raw, bitlen[0], sublen[0],
    sizeof(bits_t1_raw), &truncated1); // NEU: &truncated1
uint16_t bits_t2_count = mag_encode(
    data2, (uint8_t*)bits_t2_manchester, (uint8_t*)bits_t2_raw, bitlen[1], sublen[1],
    sizeof(bits_t2_raw), &truncated2); // NEU: &truncated2
uint16_t bits_t3_count = mag_encode(
    data3, (uint8_t*)bits_t3_manchester, (uint8_t*)bits_t3_raw, bitlen[2], sublen[2],
    sizeof(bits_t3_raw), &truncated3); // NEU: &truncated3

// NEU: Ergebnis sichern und ins Log schreiben, bevor irgendetwas gesendet wird
mag->last_tx_truncated = truncated1 || truncated2 || truncated3;
if(mag->last_tx_truncated) {
    FURI_LOG_W(
        TAG,
        "Track data truncated during encode (T1:%d T2:%d T3:%d) - sent signal is incomplete",
        truncated1,
        truncated2,
        truncated3);
}
```

```c
// scenes/mag_scene_emulate.c -- mag_scene_emulate_on_event(), GuiButtonTypeRight
case GuiButtonTypeRight:
    consumed = true;
    notification_message(mag->notifications, &sequence_blink_start_cyan);
    mag_spoof(mag);
    notification_message(mag->notifications, &sequence_blink_stop);
    if(mag->last_tx_truncated) { // NEU
        notification_message(mag->notifications, &sequence_error);
    }
    break;
```

`debug_mag_string()` bleibt unverändert (ruft weiterhin ohne den neuen Parameter auf, da
`NULL` gültig ist und der Debug-Log ohnehin bereits die volle Eingabe vor dem Encode
ausgibt).

## Testhinweis (ohne Hardware)
Eigenständiges, hostseitiges Reproduktionsprogramm der reinen Bit-Pack-Logik (identisch zum
bereits kapazitätsbegrenzten `mag_encode()`-Stand, keine Furi-/Flipper-Typen nötig):
`app-improvements/g4meover-magspoof/repro-track-truncation-visible-warning/` —
`repro_old.c` (kapazitätsbegrenzt, aber ohne Kappungs-Signal) und `repro_new.c` (mit
`out_truncated`-Parameter):

```sh
gcc -g -O0 -Wall -Wextra -fsanitize=address,undefined -o repro_old repro_old.c
gcc -g -O0 -Wall -Wextra -fsanitize=address,undefined -o repro_new repro_new.c
./repro_old   # 72 vs. 73 Zeichen (Track 1): identischer Bitcount/identische Bitfolge (1022
              # Bit) -- keine Unterscheidung moeglich. Ebenso 101 vs. 102 Zeichen (Track 2/3,
              # 1020 Bit).
./repro_new   # 72 Zeichen: truncated=0. 73 Zeichen: truncated=1, Bitcount weiterhin 1022 Bit
              # (identisch zu ALT). Ebenso 101/102 Zeichen (Track 2/3): truncated=0/1, 1020
              # Bit. Regressionsfall 40 Zeichen: truncated=0, Bitcount identisch mit/ohne
              # &truncated-Argument (NULL bleibt gueltig).
```

Bei mir lokal reproduziert (beide Programme exit code 0, alle `assert()`s bestanden):
- `repro_old`: „ALT: 72- und 73-Zeichen-Eingabe erzeugen identischen Output -- keine
  Moeglichkeit, die Kappung des 73. Zeichens zu erkennen." (ebenso für Track 2/3).
- `repro_new`: „Alle Faelle bestanden: NEU unterscheidet vollstaendig <-> gekappt, ALT konnte
  es nicht."

Funktionaler Test nach dem Patch, lokal auf dem Flipper (kein Sender/Empfänger nötig):
1. `ufbt` bauen, App starten.
2. Über „Enter track data (WIP)" (oder Kartenlesung/Dateiladen) einen Track mit mehr als 72
   (Track 1) bzw. 101 (Track 2/3) Zeichen eingeben/laden.
3. „Emulate" → „Send": die App muss nach dem Senden zusätzlich zum normalen Cyan-Blink-Stop
   die `sequence_error`-Rückmeldung zeigen (spürbar/sichtbar am LED-/Vibro-Verhalten, je nach
   Gerätevariante). Per Log (`FURI_LOG_W`, Tag `MagHelpers`) zusätzlich die genauen
   Track-Flags prüfbar.
4. Regressionsfall: ein normal langer Track (< 72/101 Zeichen, wie jede reale ISO-7811-Karte)
   löst nach dem Senden **keine** zusätzliche `sequence_error`-Rückmeldung aus — nur der
   bisherige Cyan-Blink.
5. Erst danach der eigentliche Hardware-Funktionstest (Sender/Empfänger) — das ist der Teil,
   der bei mir lokal passiert.

## Risiko
- Niedrig: reine Zusatzinformation über einen optionalen Ausgabeparameter, keine bestehende
  Aufrufstelle muss sich ändern, um weiterhin zu funktionieren (`NULL` ist gültig).
- `mag->last_tx_truncated` wird von `mag_alloc()` (`malloc(sizeof(Mag))`, nicht
  zero-initialisiert) nicht explizit gesetzt — bewusst geprüft: das Feld wird ausschließlich
  in `mag_scene_emulate_on_event()` gelesen, unmittelbar nach einem `mag_spoof(mag)`-Aufruf,
  der es unbedingt (in jedem Codepfad) neu beschreibt, bevor es je gelesen wird. Kein
  Uninitialized-Read-Pfad vorhanden — trotzdem für künftige Refactors mit `= false` bei der
  Deklaration in `mag_i.h` als defensive Absicherung sinnvoll, falls das Feld später von
  woanders gelesen wird.
- Verhalten im Normalfall (Track-Daten unterhalb der Kapazitätsgrenze) bleibt unverändert —
  durch den Host-Repro-Regressionsfall (40 Zeichen, `NULL` vs. `&truncated`) bestätigt.
- `sequence_error` selbst ist bereits an anderer Stelle in diesem Ökosystem gegen den eigenen
  `mntm-012`-Firmware-Stand verifiziert (siehe Quelle) — kein neues, ungeprüftes
  Notification-Symbol.

## Quelle
- `zacharyweiss/magspoof_flipper` (main, `9e782404`): `helpers/mag_helpers.c`/`.h`
  (`mag_encode`, `mag_spoof`), `mag_i.h` (`Mag`-Struct), `mag.c` (`mag_alloc()`,
  `malloc(sizeof(Mag))` ohne Zero-Init), `scenes/mag_scene_emulate.c`
  (`mag_scene_emulate_on_event`).
- Eigener vorheriger Fund in diesem Repo:
  `app-improvements/g4meover-magspoof/mag-encode-fixed-buffer-overflow-fix.md` (führt den
  hier erweiterten `out_raw_capacity_bytes`-Parameter samt Kappungslogik ein).
- `sequence_error`-Verifikation gegen `mntm-012`:
  `firmware-mining/nfctools-commands-missing-protocol-guard.md`.
