# MIFARE Ultralight/NTAG: SAK/ATQA-Gate vor dem Detect-Probe-Read + ATQA-Lockerung

## Was

Beim UL-AES-Phase-2-Port hatte ich in Unleasheds `mf_ultralight.c`-Diff eine gelockerte
`mf_ultralight_detect_protocol()`-ATQA-Prüfung als eigenständigen, zurückgestellten Fund notiert
(`firmware-mining/mfultralight-aes-phase2-struct-serialization.md`). Beim genaueren Nachgehen
stellt sich heraus, dass diese Funktion bei Unleashed inzwischen nicht mehr nur eine unbenutzte
Hilfsfunktion ist, sondern tatsächlich in `mf_ultralight_poller_detect()`
(`mf_ultralight_poller.c`) als Gate vor dem eigentlichen Sonden-Lesevorgang eingebaut wurde — bei
mir fehlt dieses Gate komplett. Zwei zusammengehörige Änderungen:

**1. `mf_ultralight_poller_detect()` (`mf_ultralight_poller.c`) — neues Gate**

Meine eigene Basis (`main`-Branch):

```c
static bool mf_ultralight_poller_detect(NfcGenericEvent event, void* context) {
    ...
    if(iso14443_3a_event->type == Iso14443_3aPollerEventTypeReady) {
        MfUltralightPageReadCommandData read_page_cmd_data = {};
        MfUltralightError error = mf_ultralight_poller_read_page(instance, 0, &read_page_cmd_data);
        protocol_detected = (error == MfUltralightErrorNone);
        iso14443_3a_poller_halt(instance->iso14443_3a_poller);
    }
    return protocol_detected;
}
```

Sobald ein ISO14443-3A-Tag in den `Ready`-Zustand kommt, schickt diese Funktion **bedingungslos**
einen unauthentifizierten `READ` (0x30) auf Seite 0 — unabhängig davon, was für ein Tag es
tatsächlich ist. Der Rückgabewert entscheidet, ob `nfc_scanner.c`s Protokoll-Iteration das Tag als
Ultralight/NTAG einstuft.

Unleashed (`dev`-Branch), mit Gate:

```c
if(mf_ultralight_detect_protocol(
       iso14443_3a_poller_get_data(instance->iso14443_3a_poller))) {
    MfUltralightPageReadCommandData read_page_cmd_data = {};
    MfUltralightError error = mf_ultralight_poller_read_page(instance, 0, &read_page_cmd_data);
    protocol_detected = (error == MfUltralightErrorNone);
}
iso14443_3a_poller_halt(instance->iso14443_3a_poller);
```

Der Sonden-Read läuft nur noch, wenn SAK/ATQA vorher überhaupt zu Ultralight/NTAG passen; `halt`
läuft weiterhin in jedem Fall.

**Warum das ein echter Fund ist, keine reine Kosmetik:** einige magische MIFARE-Classic-Klone
(Gen1a/Gen2, verbreitet bei generischen "UID-changeable"-Karten) beantworten einen
unauthentifizierten `READ` auf Block 0 mit Daten, statt ihn (wie ein echter MIFARE-Classic-Chip)
mit einem NAK abzulehnen — der reale Klon-Chip verhält sich dabei wie ein Ultralight. Ohne das
Gate liest meine Basis bei jedem ISO14443-3A-Tag, das den `Ready`-Zustand erreicht, diesen Probe
und stuft einen solchen Klon fälschlich als Ultralight ein, bevor `nfc_scanner.c` überhaupt den
MIFARE-Classic-Poller an die Reihe kommen lässt (Base-Protokolle werden nacheinander
durchprobiert, siehe `lib/nfc/nfc_scanner.c`). Mit echter MIFARE-Classic-Hardware (SAK 0x08/0x18,
ATQA 0x0004/0x0002) tritt das nicht auf, weil das Gate genau diese SAK/ATQA-Kombination ablehnt.

**2. `mf_ultralight_detect_protocol()` (`mf_ultralight.c`) — ATQA-Low-Byte-Lockerung**

Meine Basis:

```c
bool mfu_detected = (iso14443_3a_data->atqa[0] == 0x44) &&
                    (iso14443_3a_data->atqa[1] == 0x00) && (iso14443_3a_data->sak == 0x00);
```

Unleashed:

```c
// Ultralight/NTAG advertise SAK 0x00 with ATQA high byte 0x00. The ATQA low byte is deliberately
// NOT checked: it encodes UID size (0x44 for 7-byte, 0x04 for 4-byte), so a 4-byte-UID UL is
// still UL. Matches PM3 (rejects only atqa[1] != 0x00 || sak != 0x00); non-zero SAK (Classic and
// its magic clones) is excluded by the SAK term.
return (iso14443_3a_data->atqa[1] == 0x00) && (iso14443_3a_data->sak == 0x00);
```

Diese beiden Änderungen gehören zusammen: sobald `detect_protocol()` als Gate vor jedem
Ultralight-Sonden-Read steht (Änderung 1), muss die Prüfung selbst auch alle real vorkommenden
Ultralight-kompatiblen SAK/ATQA-Kombinationen abdecken — sonst würde das neue Gate ausgerechnet
die 4-Byte-UID-Klonkarten aussperren, die unter meiner alten, ungegateten Basis (durch das
fehlende Gate) bisher zufällig trotzdem erkannt wurden. Siehe Testgruppe 3 unten, die genau diese
Wechselwirkung belegt.

## Nutzen

- Schließt den einzigen der vier beim UL-AES-Phase-2-Port zurückgestellten Funde ab, der beim
  genaueren Prüfen tatsächlich ein aktives Verhalten betrifft (nicht nur eine unbenutzte
  Hilfsfunktion wie zuletzt `is_pwd_pack_read()`).
- Behebt eine reale Fehlerkennung: ein magischer MIFARE-Classic-Klon, der einen
  unauthentifizierten `READ` auf Block 0 beantwortet, wird bei mir aktuell versehentlich als
  Ultralight eingestuft, bevor die eigentliche Classic-Erkennung überhaupt zum Zug kommt — mit
  entsprechend falscher weiterer Behandlung (Seitenanzahl, Kartentyp, UI) bei einem für einen
  autorisierten Pentest genutzten Klon-Tag.
- Erweitert gleichzeitig die Erkennung um echte 4-Byte-UID-Ultralight-kompatible Tags, die meine
  Basis aktuell an dieser Stelle fälschlich ablehnt (unabhängig vom neuen Gate, siehe Testgruppe
  1) — spiegelt laut Unleashed-Kommentar das Proxmark3-Verhalten.
- Beide Teile sind klein, lokal und klar abgegrenzt (ein Predicate-Ausdruck, ein `if`-Wrapper um
  bereits vorhandenen Code) — kein neuer Zustand, keine neue Datenstruktur.

## Betroffene Dateien

**Jetzt geliefert:**
- `lib/nfc/protocols/mf_ultralight/mf_ultralight.c` — `mf_ultralight_detect_protocol()`: ATQA-
  Low-Byte-Prüfung entfernt, Begründungskommentar ergänzt.
- `lib/nfc/protocols/mf_ultralight/mf_ultralight_poller.c` — `mf_ultralight_poller_detect()`: der
  bestehende Sonden-Read wird in ein `if(mf_ultralight_detect_protocol(...))` gewrappt; `halt`
  bleibt unverändert außerhalb der Bedingung.
- Als Unified-Diffs gegen meine unveränderte `main`-Basis:
  `mfultralight-detect-protocol-atqa-gate/detect_protocol_c.patch`,
  `mfultralight-detect-protocol-atqa-gate/poller_gate.patch` — `patch --dry-run -p1` vorab gegen
  frische Kopien beider Dateien verifiziert, danach real angewendet und Byte-für-Byte gegen die
  host-getestete Fassung abgeglichen (identisch).
- Keine Header-Änderung: `mf_ultralight_detect_protocol()`s Signatur bleibt unverändert, kein
  neuer Include nötig (`mf_ultralight_poller.h` inkludiert `mf_ultralight.h` bereits;
  `iso14443_3a_poller_get_data()` wird an anderer Stelle in derselben Datei bereits identisch
  verwendet).

**Nicht Teil dieses Patches:**
- Kein Eingriff in `mf_ultralight_poller_read_page()` oder den eigentlichen Lese-Zustandsautomat
  — nur der bereits vorhandene Aufruf wird bedingt gemacht.
- Der noch offene Typ-Erkennungs-Umbau (NTAG210/212) und die
  `get_write_end_page()`-Schreibbereichs-Anpassung — weiterhin eigene künftige Kandidaten (siehe
  "Offen" in `firmware-mining/mf-ultralight-is-pwd-pack-read-helper.md`).

## Verifikation (auf dem Host, ohne Hardware)

`firmware-mining/repro-mfultralight-detect-protocol-atqa-gate/repro.c` kombiniert zwei Ebenen:

**Ebene 1 — echte, kompilierte Funktion:** kompiliert die tatsächlich gepatchte, unveränderte
`mf_ultralight.c`/`.h` (einzige Host-Anpassung: derselbe Include-Stand-in wie bei den vorherigen
UL-Ports) gegen die etablierten `stubs/`. `new_detect_protocol()` im Repro ruft die reale,
kompilierte `mf_ultralight_detect_protocol()` auf; `old_detect_protocol()` ist eine reine,
unveränderte Kopie meiner bisherigen Basis-Prädikat-Zeile, ausschließlich für den ALT-vs-NEU-
Vergleich (kein Teil des Patches).

**Ebene 2 — Kontrollfluss-Modell für `mf_ultralight_poller_detect()`:** die echte Funktion hängt
am vollen ISO14443-3A-Polling-Zustandsautomat, FuriHal und der NFC-Generic-Event-Verdrahtung und
ist auf dem Host nicht realistisch kompilierbar. Das Repro bildet stattdessen exakt ihren
Kontrollfluss nach (bei `Ready`: bedingt sondieren, danach immer halten) mit einem simulierten
Sonden-Read (Aufrufzähler + injizierbares Ergebnis) und Halt-Zähler — als Gate-Funktion wird dabei
die echte, in Ebene 1 kompilierte `mf_ultralight_detect_protocol()` eingesetzt (kein zweites
Modell), sodass das Zusammenspiel beider Patches mit echtem Code geprüft wird.

Build: `gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -I . -I stubs repro.c
mf_ultralight.c -o repro && ./repro` → `ALL CHECKS PASSED`, exit code 0.

Fünf Testgruppen:
1. **`detect_protocol()` ATQA-Varianten:** Standard-UL (ATQA 0x0044, SAK 0x00) auf beiden Seiten
   `true`; 4-Byte-UID-Klon (ATQA 0x0004, SAK 0x00) — Basis `false` (der eigentliche Fehler),
   gepatcht `true` (der Fix); beliebiger ATQA-Low-Byte bei ATQA-High 0x00/SAK 0x00 weiterhin
   `true` (belegt, dass der Low-Byte wirklich ungeprüft ist, nicht nur um einen zweiten festen
   Wert erweitert); ATQA-High ungleich 0 sowie MIFARE-Classic-SAK (0x08) auf beiden Seiten
   weiterhin `false`.
2. **Gate gegen magischen Classic-Klon:** simulierter Klon mit SAK 0x08/ATQA 0x0004, der den
   Sonden-Read positiv beantwortet (`probe_result = true`) — ungegatet (meine Basis)
   fälschlicherweise als Ultralight erkannt, Sonden-Read läuft genau einmal; gegatet (gepatcht)
   korrekt abgelehnt, Sonden-Read läuft **nicht**, `halt` läuft in beiden Fällen genau einmal.
3. **Gate gegen echtes Ultralight:** ATQA 0x0044/SAK 0x00 — ungegatet und gegatet erkennen beide
   korrekt, Sonden-Read läuft in beiden Fällen genau einmal (kein Regressions-Fehlalarm für den
   Normalfall).
4. **Wechselwirkung der beiden Patches:** derselbe 4-Byte-UID-UL-Klon aus Testgruppe 1, diesmal
   durchs Gate geschickt — einmal mit der alten, strikten `old_detect_protocol()` als Gate-
   Funktion (Sonden-Read läuft **nicht** — genau die Regression, die entstünde, wenn nur das
   Gate ohne die ATQA-Lockerung eingebaut würde), einmal mit der echten, gepatchten
   `mf_ultralight_detect_protocol()` (Sonden-Read läuft, Karte weiterhin erkannt).
5. **Gegenprobe:** ein absichtlich kaputtes Gate ohne SAK-Prüfung lässt den Classic-Klon aus
   Testgruppe 2 bewusst durch (`CHECK` erwartet hier explizit das fehlerhafte Verhalten) — belegt,
   dass Testgruppe 2 tatsächlich die SAK-Prüfung testet und nicht unabhängig vom Gate durchläuft.

Zusätzlich gegen die reale, unveränderte Basis-Zeile (`iso14443_3a_data->atqa[0] == 0x44 && ...`)
zurückgeprüft (`mf_ultralight.c` ohne den Patch neu kompiliert): vier der oben genannten Checks
schlagen dann gezielt fehl (4-Byte-UID-Erkennung selbst, sowie die davon abhängige
Wechselwirkungsprüfung aus Gruppe 4) — die Suite ist also nicht vom eigentlichen Fix unabhängig
grün.

**Nicht** durch diesen Host-Test abgedeckt: das tatsächliche Verhalten von
`nfc_scanner.c`s Protokoll-Iteration bei einem echten Tag am Gerät (Reihenfolge der
Base-Protokolle, reales Timing des `Ready`-Events) — dafür wäre ein echter magischer
MIFARE-Classic-Klon nötig, siehe "Testhinweis".

## Risiko

- **Niedrig.** Beide Änderungen sind lokal auf je eine Funktion begrenzt und ändern keine
  Datenstruktur, keine Signatur, keinen Include.
- Das Gate kann den Sonden-Read jetzt seltener auslösen als vorher (nur noch bei passendem
  SAK/ATQA) — für jedes Tag, das tatsächlich Ultralight/NTAG ist, bleibt das Verhalten laut
  Testgruppe 3 identisch. Ein echtes Risiko bestünde nur, wenn es reale Ultralight-kompatible
  Tags mit SAK ungleich 0x00 oder ATQA-High-Byte ungleich 0x00 gäbe — dafür habe ich in der
  Unleashed-Quelle keinen Hinweis gefunden (der Kommentar dort nennt explizit nur SAK 0x00 als
  Ultralight/NTAG-Signatur); das bleibt die einzige nicht durch den Host-Test abgesicherte
  Annahme.
- `halt` läuft in beiden Fällen (Gate erfüllt oder nicht) weiterhin garantiert genau einmal —
  kein offen gehaltenes Feld, keine hängende Poller-Transaktion, siehe Testgruppen 2–4.
- Commit-Metadaten von Unleashed waren wie bei den vorherigen UL-Ports in dieser Sitzung nicht
  per API abrufbar (nur Rohdatei-Zugriff über `raw.githubusercontent.com` sowie ein flacher,
  sparse `git clone` des `dev`-Branchs für die repo-weite Aufrufstellensuche); geprüft gegen den
  aktuellen `dev`-Stand.

## Port-Skizze

Kein mehrphasiger Port — zwei zusammengehörige, in sich abgeschlossene Ein-Funktions-Änderungen
in zwei Dateien. War als einer von vier beim UL-AES-Phase-2-Port zurückgestellten Funden notiert
(`firmware-mining/mfultralight-aes-phase2-struct-serialization.md`); mit diesem Patch sind drei
der vier ursprünglich zurückgestellten Punkte erledigt (`is_pwd_pack_read()`-Helfer, jetzt diese
Gate-/ATQA-Änderung). Verbleibend: Typ-Erkennungs-Umbau für NTAG210/212.

## Testhinweis (lokal)

1. Die zwei `.patch`-Dateien in `lib/nfc/protocols/mf_ultralight/` einbauen (`patch -p1 <
   detect_protocol_c.patch` / `... poller_gate.patch`, aus dem Firmware-Wurzelverzeichnis).
2. `ufbt` (bzw. `fbt firmware_all`) bauen — keine Signatur geändert, sollte ohne Warnungen
   durchlaufen.
3. Host-Repro laufen lassen (`gcc ... && ./repro`), `ALL CHECKS PASSED` erwarten.
4. Regressionstest am Gerät: ein bestehender NFC-Lese-Workflow für eine echte NTAG213/
   Ultralight-Karte muss sich durch diesen Patch in keiner Weise ändern (Testgruppe 3 oben deckt
   das auf Kontrollfluss-Ebene ab).
5. Falls vorhanden: einen magischen MIFARE-Classic-Klon (Gen1a/Gen2, SAK 0x08/0x18) einlesen und
   prüfen, dass `nfc_scanner` ihn weiterhin als MIFARE Classic und nicht als Ultralight/NTAG
   einstuft — das ist der eigentliche, am Gerät nachvollziehbare Effekt dieses Patches. Ohne
   einen solchen Klon zur Hand ist der Host-Kontrollfluss-Test (Testgruppe 2) der einzige
   verfügbare Nachweis.

## Quelle

- Unleashed-Firmware, `dev`-Branch (`DarkFlippers/unleashed-firmware`,
  `lib/nfc/protocols/mf_ultralight/mf_ultralight.c`/`mf_ultralight_poller.c`). Für
  `mf_ultralight.c` per `raw.githubusercontent.com` bezogen; die Aufrufstelle in
  `mf_ultralight_poller.c` wurde per flachem, sparse `git clone` (`lib/nfc`, `applications`) des
  `dev`-Branchs gefunden, da der reine Rohdatei-Zugriff auf einzelne, vermutete Dateipfade
  (`lib/nfc/nfc_device.c`, `lib/nfc/nfc.c`, `mf_ultralight_listener.c`, `nfc_scanner.c`) die
  tatsächliche Aufrufstelle nicht zeigte — GitHub-API-Zugriff für Commit-Metadaten war wie bei
  den vorherigen UL-Ports nicht verfügbar.
- Meine eigene `G4MEOVER-FW`-Basis, `main`-Branch: `mf_ultralight.c`, `mf_ultralight_poller.c`,
  `mf_ultralight_poller.h`, `mf_ultralight_poller_i.h` (nur gelesen, für den Include- und
  Aufrufkontext), `lib/nfc/nfc_scanner.c` (nur gelesen, zur Einordnung der Base-Protokoll-
  Iteration, die vom Rückgabewert dieses Gates abhängt).
- Bereits vermerkte Vorarbeit: `firmware-mining/mfplus-mfultralight-aes-auth-port.md`,
  `firmware-mining/mfultralight-aes-phase2-struct-serialization.md` (dort ursprünglich als einer
  von vier zurückgestellten Funden notiert), `firmware-mining/mf-ultralight-is-pwd-pack-read-helper.md`.
