# NFC ISO14443-3A: Kaskadenfortsetzung falsch am NFCID-Byte statt am SAK-Bit erkannt

Quelle: Unleashed-Firmware-Commit `38ba133f3b20232451cfdcd9d40719384afdfe95`
("[NFC] Fix reading 4-byte UIDs that start with 0x88 (cascade tag)", PR #1075,
Fixes #1074, hardware-verifiziert von @mishamyte, 2026-08-15), gefunden beim
Weiterverfolgen des `unlshd-091`-Fensters — der andere der beiden noch offenen
Restfunde aus derselben Sitzung, in der ich bereits die Kaskadenstufen-Obergrenze
(`4e8b746f`) portiert habe (siehe `firmware-mining/iso14443-3a-cascade-uid-overflow.md`).
Diese Änderung ist unabhängig davon: sie behebt einen Korrektheitsbug (falsches
Leseergebnis auf echter Hardware), keinen Speicherzugriffsfehler.

## Was

`iso14443_3a_poller_i.c`, derselbe `Iso14443_3aPollerColResStateStateSelectCascade`-
Zweig wie beim bereits portierten Overflow-Fix: nach jeder SEL-Antwort entscheidet der
Code, ob die UID vollständig ist oder eine weitere Kaskadenstufe folgt. Bei mir lautet
die Entscheidung:

```c
if(instance->col_res.sel_req.nfcid[0] == ISO14443_3A_POLLER_SDD_CL) { // 0x88
    // UID unvollständig -> naechste Kaskadenstufe
    ...
} else {
    // UID vollstaendig
    ...
}
```

Das geprüfte Byte ist `nfcid[0]`, das erste Byte der SDD-Antwort des Tags — bei einer
*echten* Kaskadenfortsetzung setzt eine spezifikationskonforme Karte hier den
festgelegten Kaskadierungs-Tag-Wert `0x88`. Das Problem: `0x88` ist gleichzeitig ein
gültiger erster Byte-Wert für eine ganz normale, **finale** 4-Byte-UID — auf manchen
MIFARE-Classic- und Magic-Karten kommt das laut Unleashed-Issue #1074 real vor
(nicht spezifikationskonform, aber real vorhandene Hardware). Für diese Karten
entscheidet die Schleife bei mir fälschlich "unvollständig" und fragt eine zweite
Kaskadenstufe ab, die die Karte gar nicht mehr sinnvoll beantwortet — der Lesevorgang
schlägt fehl oder liefert eine falsche/verstümmelte UID, obwohl die Karte korrekt
geantwortet hätte. Nach eigenem Antritt (`iso14443_3a_poller_i.h`,
`ISO14443_3A_POLLER_SEL_PAR`/`ISO14443_3A_POLLER_SEL_CMD`) und ISO14443-3 ist dafür
eigentlich das **Kaskadenbit im SAK** (`sel_resp.sak`, Bit 2 = `0x04`) die vorgesehene
Instanz: eine Karte, die noch eine weitere Kaskadenstufe erwartet, setzt dieses Bit in
ihrer SEL-Antwort explizit — unabhängig davon, welchen Wert `nfcid[0]` zufällig trägt.

Betrifft denselben gemeinsamen Aktivierungspfad wie der bereits portierte
Overflow-Fix (Iso14443-3A ist die Basis für Classic/Ultralight/DESFire/Plus/
ISO14443-4A/EMV) — jeder NFC-A-Lesevorgang durchläuft diese Entscheidung mindestens
einmal.

**Sicherheitsrelevanz:** keine. Reiner Korrektheitsbug (falsches Leseergebnis für
reale, aber nicht spezifikationskonforme Karten), kein Speicherzugriffsfehler und
keine neue Angriffsfläche. Das SAK-Byte ist wie `nfcid[0]` vollständig
tag-/angreiferkontrolliert — die Kaskadenstufen-Obergrenze aus dem bereits portierten
`iso14443-3a-cascade-uid-overflow.md`-Fix bleibt davon unabhängig notwendig und wird
durch diesen Fix weder ersetzt noch überflüssig. Beide Fixes sind unabhängig
voneinander anwendbar und ändern unterschiedliche Zeilen derselben Funktion — siehe
Port-Skizze unten für die kombinierte Fassung.

## Betroffene Dateien (Portumfang)

- `lib/nfc/protocols/iso14443_3a/iso14443_3a_poller_i.h` — Makro
  `ISO14443_3A_POLLER_SDD_CL (0x88U)` durch ein neues Makro
  `ISO14443_3A_POLLER_SAK_CASCADE_BIT (0x04U)` ersetzen (der `0x88`-Konstantenwert
  selbst wird weiterhin gebraucht, um `sel_req.nfcid[0]` beim Aufbau der SEL-Anfrage
  zu setzen — dieser Teil bleibt unverändert, nur die Fortsetzungs-*Entscheidung*
  wechselt die Quelle).
- `lib/nfc/protocols/iso14443_3a/iso14443_3a_poller_i.c` —
  `Iso14443_3aPollerColResStateStateSelectCascade`-Zweig: Bedingung von
  `sel_req.nfcid[0] == ISO14443_3A_POLLER_SDD_CL` auf
  `sel_resp.sak & ISO14443_3A_POLLER_SAK_CASCADE_BIT` ändern.

Zwei Zeilen geändert, ein Makro ersetzt, keine neuen Abhängigkeiten, kein API-Bruch.

## Risiko

Sehr gering. Die Bedingung wechselt lediglich die Datenquelle für dieselbe
Ja/Nein-Entscheidung; für jede spezifikationskonforme Karte liefern beide Kriterien
dasselbe Ergebnis (siehe Testhinweis), weil eine echte Kaskadenfortsetzung `nfcid[0]`
UND das SAK-Kaskadenbit gemeinsam setzt. Der einzige Verhaltensunterschied betrifft
genau die Karten, die vorher falsch gelesen wurden — dort ist jede Änderung eine
Verbesserung. Kombinierbar mit dem bereits portierten Kaskadenstufen-Obergrenze-Fix
ohne Interaktion (unterschiedliche Prüfungen an derselben Stelle, siehe Port-Skizze).

## Port-Skizze

Kombiniert mit dem bereits vorgeschlagenen Kaskadenstufen-Limit
(`iso14443-3a-cascade-uid-overflow.md`) sieht der Zweig nach beiden Fixes so aus:

```c
// iso14443_3a_poller_i.h
#define ISO14443_3A_POLLER_SEL_CMD(cascade_lvl) (0x93 + 2 * (cascade_lvl))
#define ISO14443_3A_POLLER_SEL_PAR(bytes, bits) (((bytes) << 4 & 0xf0U) | ((bits) & 0x0fU))
#define ISO14443_3A_POLLER_SDD_CL               (0x88U)
// SAK-Kaskadenbit (b3): gesetzt => UID unvollstaendig, naechste Kaskadenstufe folgt
#define ISO14443_3A_POLLER_SAK_CASCADE_BIT      (0x04U)
```

```c
// iso14443_3a_poller_i.c, Iso14443_3aPollerColResStateStateSelectCascade-Zweig
FURI_LOG_T(TAG, "Sel resp: %02X", instance->col_res.sel_resp.sak);
// Das SAK-Kaskadenbit ist die eigentliche Instanz fuer UID-Vollstaendigkeit, nicht
// das erste NFCID-Byte: eine echte 4-Byte-UID kann zufaellig mit dem
// Kaskadierungs-Tag-Wert 0x88 beginnen (nicht spezifikationskonform, aber auf
// manchen MIFARE-Classic-/Magic-Karten real vorhanden).
if(instance->col_res.sel_resp.sak & ISO14443_3A_POLLER_SAK_CASCADE_BIT) {
    // Bereits portierte Kaskadenstufen-Obergrenze (siehe
    // iso14443-3a-cascade-uid-overflow.md), unveraendert kombinierbar:
    if(instance->col_res.cascade_level >= 2) {
        FURI_LOG_E(TAG, "Too many cascade levels");
        instance->state = Iso14443_3aPollerStateColResFailed;
        ret = Iso14443_3aErrorColResFailed;
        break;
    }
    // UID unvollstaendig: die 3 Byte nach dem Kaskadierungs-Tag-Byte uebernehmen
    memcpy(
        &instance->data->uid[instance->data->uid_len],
        &instance->col_res.sel_req.nfcid[1],
        3);
    instance->data->uid_len += 3;
    instance->col_res.cascade_level++;
    instance->col_res.state = Iso14443_3aPollerColResStateStateNewCascade;
} else {
    FURI_LOG_T(TAG, "Col resolution complete");
    instance->data->sak = instance->col_res.sel_resp.sak;
    memcpy(
        &instance->data->uid[instance->data->uid_len],
        &instance->col_res.sel_req.nfcid[0],
        4);
    instance->data->uid_len += 4;
    instance->col_res.state = Iso14443_3aPollerColResStateStateSuccess;
    instance->state = Iso14443_3aPollerStateActivated;
}
```

Wer nur diesen Fix ohne die Kaskadenstufen-Obergrenze einpflegen will: einfach den
inneren `if(cascade_level >= 2) { ... break; }`-Block weglassen, Rest identisch — beide
Fixes sind unabhängig voneinander anwendbar.

## Testhinweis (lokal, ohne Hardware für die Kernlogik)

Host-kompilierbares Modell beider Entscheidungskriterien
(`firmware-mining/repro-iso14443-3a-sak-cascade-bit-fix/repro.c`, 1:1 aus
`iso14443_3a_poller_i.c`/`.h` übernommen):

```
cd firmware-mining/repro-iso14443-3a-sak-cascade-bit-fix
gcc -O0 -Wall -Wextra -fsanitize=address,undefined -o repro repro.c && ./repro
```

Vergleicht ALT- (`nfcid[0] == 0x88`) gegen NEU-Entscheidung (`sak & 0x04`) über sechs
realistische `(nfcid[0], sak)`-Antwortpaare (4-/7-/10-Byte-UID, jeweils Zwischen- und
Finalstufe, plus der pathologische 0x88-Magic-Karten-Fall). Ergebnis: NEU entscheidet
in allen sechs Fällen richtig; ALT irrt sich in genau einem Fall — der 4-Byte-UID-Karte,
die zufällig mit `0x88` beginnt (der reale, hardware-bestätigte Unleashed-Fund #1074).
In allen fünf spezifikationskonformen Fällen liefern ALT und NEU dasselbe Ergebnis, da
`nfcid[0] == 0x88` und `sak & 0x04` bei einer echten Kaskadenfortsetzung stets gemeinsam
gesetzt sind — kein Verhaltensunterschied für normale Karten.

Für die Hardware-Verifikation nach dem Einpflegen: falls eine 4-Byte-UID-Karte
verfügbar ist, deren UID mit `0x88` beginnt (z. B. bestimmte Magic-Karten, per
vorherigem Lesevorgang mit einem anderen Reader oder Zufall zu identifizieren) — vor
dem Fix schlägt der Lesevorgang fehl oder liefert eine falsche UID, nach dem Fix wird
die korrekte 4-Byte-UID gelesen. Ohne eine solche Karte lässt sich der Effekt nur über
Codereview und den Host-Repro oben nachvollziehen; alle regulären eigenen Karten
(4-/7-Byte-UID, die nicht mit `0x88` beginnen) müssen vor und nach dem Fix identisch
gelesen werden — mit ausreichend eigenen Testkarten stichprobenartig gegenprüfen.
