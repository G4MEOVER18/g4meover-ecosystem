# NFC ISO14443-3A: Unbegrenzte Kaskadenstufen überschreiben uid[]-Puffer

Quelle: Unleashed-Firmware-Commit `4e8b746fd6c2ba7ed566247dbd8a0768add997ba`
("[NFC] Bound ISO14443-3A UID cascade to 3 levels (fixes potential uid[] overflow)",
PR #1078, Fixes #1077, 2026-08-15), gefunden beim Gegenprüfen von `unlshd-091` gegen
`unlshd-090` (letzter zuvor geprüfter Stand). Der Fund ist unabhängig von der
Official-dev-Mining-Reihe (`dev` steht weiterhin unverändert auf `a55e3939`, siehe
BACKLOG) — die Unleashed-eigene Anticollision-Implementierung hat einen Bug, den ich
bei mir (mntm-012, API 87.1) in abgewandelter, eher noch schwerer ausnutzbarer Form
wiederfinde.

## Was

`iso14443_3a_poller_i.c`, Kollisionsauflösungsschleife
(`Iso14443_3aPollerColResStateStateSelectCascade`-Zweig in
`iso14443_3a_poller_activate()`): nach jeder erfolgreichen SEL-Antwort entscheidet der
Code, ob eine weitere Kaskadenstufe folgt, und hängt im Fortsetzungsfall 3 weitere
Byte an `data->uid[]` an — ohne jede Obergrenze für die Anzahl der Stufen.

Bei mir lautet die Fortsetzungsbedingung:

```c
if(instance->col_res.sel_req.nfcid[0] == ISO14443_3A_POLLER_SDD_CL) { // 0x88
    memcpy(&instance->data->uid[instance->data->uid_len],
           &instance->col_res.sel_req.nfcid[1], 3);
    instance->data->uid_len += 3;
    instance->col_res.cascade_level++;
    instance->col_res.state = Iso14443_3aPollerColResStateStateNewCascade;
}
```

`nfcid[0]` ist das erste Byte der SDD-Antwort des Tags (`sdd_resp.nfcid`) — vollständig
tag-/angreiferkontrolliert. Ein präparierter oder emulierter Tag, der bei **jeder**
Kaskadenstufe erneut `0x88` als erstes NFCID-Byte liefert, hält die Schleife beliebig
lange am Laufen: `cascade_level` und `uid_len` wachsen unbegrenzt, und jede Runde
schreibt 3 weitere, vom Tag gewählte Byte in `data->uid[]`.

`data->uid[]` ist nach `iso14443_3a.h` genau 10 Byte groß
(`ISO14443_3A_MAX_UID_SIZE = ISO14443_3A_UID_10_BYTES = 10U`), unmittelbar gefolgt von
`uid_len`, `atqa[2]` und `sak` im selben `Iso14443_3aData`-Struct
(`iso14443_3a_alloc()`, Heap-Allokation). ISO14443-3 selbst begrenzt die
Kollisionsauflösung auf höchstens 3 Kaskadenstufen (4+7+10-Byte-UIDs) — ab der 4.
Stufe (`cascade_level == 3`) liegt `uid_len` bereits bei 9, und der bedingungslose
`memcpy(..., 3)` schreibt in `uid[9]`, `uid[10]`, `uid[11]` — die letzten beiden Byte
liegen bereits hinter dem Array, in `uid_len` und `atqa[0]`. Jede weitere Stufe schreibt
3 zusätzliche, vom Tag frei wählbare Byte weiter in den Heap dahinter. Das ist ein
echter Speicherzugriffsfehler mit angreiferkontrolliertem Inhalt und wachsender
Reichweite, ausgelöst durch jeden NFC-A-Lesevorgang (`iso14443_3a_poller_activate()`
läuft bei jedem Read/Detect eines ISO14443-A-Tags, nicht nur in einem Sonderfall) — mit
Host-Repro unter ASan als `heap-buffer-overflow` beim vierten Kaskadenschritt
verifiziert (siehe Testhinweis).

Eine verwandte, aber unabhängige Unleashed-Änderung (`38ba133f`, unmittelbar vor
`4e8b746f`) ersetzt die 0x88-Erkennung durch eine Prüfung des SAK-Kaskadenbits
(`sel_resp.sak & 0x04`), weil ein *echter* 4-Byte-UID, der zufällig mit `0x88` beginnt,
sonst fälschlich als unvollständig gilt und der Read fehlschlägt (Korrektheitsbug, kein
Speicherfehler). Ich portiere hier bewusst **nur** die Kaskadenstufen-Obergrenze und
lasse die 0x88-Erkennung selbst unverändert: die Obergrenze behebt den
Speicherzugriffsfehler unabhängig davon, welche Bedingung über die Fortsetzung
entscheidet, und ändert nichts am bestehenden Leseverhalten für reale Karten (siehe
Testhinweis, Normalbetrieb-Fall). Die 0x88-vs-SAK-Korrektheitsfrage bleibt als eigener,
kleinerer Folgepunkt offen (betrifft nur die seltenen Karten mit UID-Präfix `0x88`,
kein Sicherheitsproblem).

## Betroffene Dateien (Portumfang)

- `lib/nfc/protocols/iso14443_3a/iso14443_3a_poller_i.c` —
  `Iso14443_3aPollerColResStateStateSelectCascade`-Zweig: vor dem bestehenden
  `memcpy`/`cascade_level++` im Fortsetzungsfall eine Prüfung `cascade_level >= 2`
  ergänzen, die bei Überschreitung `instance->state = Iso14443_3aPollerStateColResFailed`
  setzt und mit `Iso14443_3aErrorColResFailed` abbricht (exakt der bereits vorhandene
  Fehlerpfad der anderen Prüfungen in derselben Funktion, z. B. "Sel response wrong
  length").

Eine einzige, lokal begrenzte Änderung (eine zusätzliche Bedingung), kein API-Bruch,
keine neuen Abhängigkeiten. Betrifft jeden NFC-A-Poller-Nutzer indirekt (Iso14443-3A ist
die gemeinsame Basis für Classic/Ultralight/DESFire/Plus/ISO14443-4A/EMV), da die
Kollisionsauflösung an dieser einen Stelle für alle Protokolle gemeinsam läuft.

## Risiko

Gering für den Fix selbst: rein additive Grenzprüfung, die im regulären Betrieb (jede
reale Karte terminiert spätestens nach 3 Kaskadenstufen) nie greift — mit Repro
verifiziert, dass 4-, 7- und 10-Byte-UID-Karten (1, 2, 3 Stufen) vor und nach dem Fix
byteidentisch gelesen werden. Der Bug selbst ist dagegen hoch: ein hostiler/emulierter
Tag kann während eines autorisierten Zutritts-/NFC-Pentests einen laufenden Read mit
tag-kontrolliertem Speicherzugriffsfehler zum Absturz bringen oder — je nach
Heap-Layout zum Zeitpunkt des Reads — potenziell benachbarten Heap-Speicher mit
tag-gewählten Bytes überschreiben. Kein Fixed-vs-Overflow-Tradeoff: die Obergrenze
kostet nichts an Funktionalität, die die Spezifikation vorsieht.

## Port-Skizze

```c
} else if(instance->col_res.state == Iso14443_3aPollerColResStateStateSelectCascade) {
    ...
    FURI_LOG_T(TAG, "Sel resp: %02X", instance->col_res.sel_resp.sak);
    if(instance->col_res.sel_req.nfcid[0] == ISO14443_3A_POLLER_SDD_CL) {
        // ISO14443-3 kennt hoechstens 3 Kaskadenstufen (10-Byte-UID); ein Tag, der
        // darueber hinaus immer wieder 0x88 liefert, wuerde data->uid[10] ueberschreiben.
        if(instance->col_res.cascade_level >= 2) {
            FURI_LOG_E(TAG, "Too many cascade levels");
            instance->state = Iso14443_3aPollerStateColResFailed;
            ret = Iso14443_3aErrorColResFailed;
            break;
        }
        // Copy part of UID
        memcpy(
            &instance->data->uid[instance->data->uid_len],
            &instance->col_res.sel_req.nfcid[1],
            3);
        instance->data->uid_len += 3;
        instance->col_res.cascade_level++;
        instance->col_res.state = Iso14443_3aPollerColResStateStateNewCascade;
    } else {
        ...
```

Einzige Änderung: der neue `if(cascade_level >= 2) { ... break; }`-Block direkt vor dem
bestehenden `memcpy`. Rest der Funktion unverändert.

## Testhinweis (lokal, ohne Hardware für die Kernlogik)

Host-kompilierbares Modell der Kollisionsauflösungsschleife
(`firmware-mining/repro-iso14443-3a-cascade-uid-overflow/repro.c`, ALT/NEU 1:1 aus dem
Entscheidungs-/Kopierzweig von `iso14443_3a_poller_i.c` übernommen, Struct-Layout 1:1
aus `iso14443_3a.h`):

```
cd firmware-mining/repro-iso14443-3a-cascade-uid-overflow
gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
gcc -O0 -Wall -Wextra -fsanitize=address,undefined -DDEMO_HOSTILE_TAG_OLD -o repro_bug repro.c && ./repro_bug
gcc -O0 -Wall -Wextra -fsanitize=address,undefined -DDEMO_HOSTILE_TAG_NEW -o repro_fixed repro.c && ./repro_fixed
```

Ergebnis:
- `./repro` (Normalbetrieb, 1/2/3 Kaskadenstufen, korrekt terminierender Tag): ALT und
  NEU liefern identische UIDs, kein Verhaltensunterschied.
- `./repro_bug` (hostiler Tag, liefert immer `0x88`, ALT-Variante auf einem exakt
  14 Byte großen Heap-Puffer — Größe von `Iso14443_3aData` reduziert auf die relevanten
  Felder): ASan bricht beim 4. Kaskadenschritt mit `heap-buffer-overflow` (`WRITE of
  size 3`, 176 Byte hinter der 14-Byte-Region) ab.
- `./repro_fixed` (derselbe hostile Tag, NEU-Variante): bricht sauber mit
  `Iso14443_3aErrorColResFailed` nach der 3. Kaskadenstufe ab, kein ASan-Fund.

Für die Hardware-Verifikation nach dem Einpflegen: eigene 4-, 7- und 10-Byte-UID-Karten
(z. B. eine MIFARE-Classic-Karte mit 4-Byte-UID sowie eine mit 7-Byte-UID, falls
vorhanden) lesen — Verhalten muss vor/nach dem Patch identisch bleiben. Ein hostiler
Tag, der endlos `0x88` liefert, lässt sich mangels passendem Emulations-Werkzeug für
beliebig viele Kaskadenstufen nicht ohne Weiteres mit eigener Hardware nachstellen;
Verifikation erfolgt daher primär über Codereview und den Host-Repro oben.
