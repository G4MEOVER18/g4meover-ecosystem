# NFC Tools — Fortsetzung des NFC-Werkzeug-Clusters: CC-Lesefehler beim NDEF-Schreiben wird als "Tag too small" fehlgemeldet

## Was
Ich habe den NFC-Tools-Cluster (`applications/external/nfctools/`, Autor `wakdev`, `fap_version`
1.3, Quelle `github.com/wakdev/nfctools-fz`) fortgesetzt und diesmal den zentralen NDEF-Schreib-Worker
geprüft, über den alle ~35 Schreib-Szenen (Kontakt/Bitcoin/WLAN/Social/URL/Text/etc.) am Ende
laufen: `scenes/write/nfc_tools_scene_write_scan.c` (304 Zeilen,
`nfc_tools_ndef_write_worker()`). Die eigentliche NDEF-Kodierung
(`nfc_tools_ndef_build()` in `core/nfc_tools_ndef.c`) mitgeprüft — feste Puffergrößen
(`ndef_buf1..3`) durchgängig per `strlen()`/`malloc()` auf die exakt benötigte Länge alloziert,
abschließender `furi_check((size_t)(b - buf) == total)` bestätigt die Baugröße; kein
Speicherzugriffsfehler gefunden. Der Fund liegt in der Kapazitätsprüfung für MIFARE Ultralight/NTAG
kurz vor dem eigentlichen Seiten-Schreiben.

## Fund: Ein einziger, nicht wiederholter CC-Lesefehler wird als "Tag zu klein" statt als Kommunikationsfehler gemeldet
`nfc_tools_ndef_write_worker()` liest vor dem MFUL/NTAG-Schreibpfad die Capability Container
(Seite 3), um die tatsächliche Nutzerkapazität des Tags zu bestimmen (`nfc_tools_scene_write_scan.c:186-207`):

```c
// Phase 4: read the Capability Container (page 3) to determine the user
// capacity of the tag. This is a deterministic gate — a "tag too small"
// error is not a transient failure and is not retried.
MfUltralightPage cc_page;
uint16_t capacity = 48; // conservative fallback (6 × 8 bytes)
if(mf_ultralight_poller_sync_read_page(app->nfc, 3, &cc_page) == MfUltralightErrorNone) {
    if(cc_page.data[0] == 0xE1) {
        capacity = (uint16_t)cc_page.data[2] * 8;
    }
}

if(ndef_size > capacity) {
    notification_message(app->notifications, &sequence_error);
    furi_string_printf(
        app->info_str,
        "Tag too small!\n%u bytes required\n%u bytes available",
        (unsigned)ndef_size,
        (unsigned)capacity);
    free(ndef);
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
    return 0;
}
```

Der eigene Kopfkommentar behauptet, die Prüfung sei "deterministic" und ein Fehlschlag sei "not a
transient failure" — das stimmt aber nur für den Fall, dass der CC-Lesevorgang selbst *gelingt* und
eine echte, zu kleine Kapazität liefert. Schlägt der Lesevorgang von Seite 3 dagegen fehl (schwache
Feldkopplung während des kurzen Antippens, exakt dieselbe Fehlerklasse wie bei jedem der
nachfolgenden Seiten-Schreibzugriffe), greift der `if(... == MfUltralightErrorNone)`-Zweig nicht,
`capacity` bleibt beim Fallback-Wert `48` — und dieser Fallback wird direkt im Anschluss als wäre er
die echte, gemessene Kapazität in der Fehlermeldung ausgegeben ("Tag too small! X bytes required, 48
bytes available"). Ein Kommunikationsfehler wird damit als Kapazitätsurteil über den Tag
ausgegeben, mit einer erfundenen Zahl.

Das ist besonders inkonsistent, weil dieselbe Funktion für exakt dieselbe Fehlerklasse (transiente
Kommunikationsfehler beim Zugriff auf denselben Tag in derselben RF-Sitzung) sowohl davor
(ISO15693/MFC/DESFire-Pfade) als auch direkt danach (`write_mful_pages()`) ein eigenes
`RETRY_WRITE(...)`-Makro verwendet: ein einziger stiller Wiederholungsversuch nach 200 ms, bevor
endgültig ein Fehler gemeldet wird. Nur der CC-Lesevorgang — dieselbe Art von Tag-Zugriff, nur lesend
statt schreibend — bekommt diese Behandlung nicht, obwohl das umgebende Makro zum Zeitpunkt dieser
Prüfung bereits im Gültigkeitsbereich der Funktion definiert ist.

Praktische Auswirkung: Bei jedem NDEF-Inhalt, der die 48-Byte-Fallback-Schwelle überschreitet
(realistisch bei den meisten Szenen außer reinem Text/URL — z. B. WLAN-Zugangsdaten, Kontaktdaten
mit mehreren Feldern, Bitcoin-Adresse+Betrag+Nachricht), zeigt ein einziger kurzer
Kommunikationsaussetzer beim CC-Lesen dem Nutzer "Tag too small!\n<N> bytes required\n48 bytes
available" — obwohl der tatsächlich angetippte Tag (z. B. NTAG216 mit 888 Byte Nutzkapazität) mehr
als genug Platz hätte. Der Nutzer bekommt eine irreführende Ursache ("mein Tag ist zu klein")
statt der zutreffenden ("nochmal antippen") und wird die Schreibaktion mangels erneutem Versuch
plausibel ganz aufgeben, obwohl ein zweiter Tap genügt hätte.

Das ist dieselbe Fundklasse wie der bereits dokumentierte NFC-Tools-"Remove Password"-Reihenfolgen-
Fund und die RollForge-/RollJam-Meldungswidersprüche: eine gemeldete Fehlerursache, die dem
tatsächlichen Gerätezustand widerspricht — hier nicht als Erfolg/Fehler-Widerspruch, sondern als
falsch zugeordnete Fehlerursache mit einer erfundenen Kapazitätszahl.

## Host-Repro
Eigenständiges Zustandsmodell der Kapazitätsprüfung (kein NFC-/Flipper-Bezug), das für vier Szenarien
(transienter CC-Lesefehler mit anschließendem Erfolg, dauerhafter CC-Lesefehler, sofortiger Erfolg
mit echter kleiner Kapazität, sofortiger Erfolg mit ausreichender Kapazität) vergleicht, was die App
meldet — einmal ohne Retry auf den CC-Read (ALT, wie ausgeliefert) und einmal mit demselben Retry-
Muster wie die Seiten-Writes (NEU):

```
gcc -O1 -Wall -Wextra -fsanitize=address,undefined repro.c -o repro
./repro
```

Tatsächliche Ausgabe (lokal ausgeführt):
```
== ALT (CC-Read ohne Retry, Fallback als echte Kapazitaet gemeldet) ==
ALT transient CC-read blip, tag has room (144B, writing 100B)  | write=0 too_small=1 (meldet 48B) comm_err=0 <-- FALSCH: Kommunikationsfehler als 'zu klein' gemeldet
ALT permanent CC-read failure (tag pulled away, writing 100B)  | write=0 too_small=1 (meldet 48B) comm_err=0 <-- FALSCH: Kommunikationsfehler als 'zu klein' gemeldet
ALT immediate success, genuinely tiny tag (32B, writing 100B)  | write=0 too_small=1 (meldet 32B) comm_err=0
ALT immediate success, tag has room (144B, writing 100B)       | write=1 too_small=0 (meldet 0B) comm_err=0

== NEU (CC-Read mit demselben Retry wie die Seiten-Writes) ==
NEU transient CC-read blip, tag has room (144B, writing 100B)  | write=1 too_small=0 (meldet 0B) comm_err=0
NEU permanent CC-read failure (tag pulled away, writing 100B)  | write=0 too_small=0 (meldet 0B) comm_err=1
NEU immediate success, genuinely tiny tag (32B, writing 100B)  | write=0 too_small=1 (meldet 32B) comm_err=0
NEU immediate success, tag has room (144B, writing 100B)       | write=1 too_small=0 (meldet 0B) comm_err=0

Bestaetigt: ALT meldet in 2 von 4 Faellen eine falsche 'Tag too small'-Ursache, NEU in 0 von 4.
```
Die beiden Regressionsfälle (sofortiger Erfolg, egal ob Kapazität zu klein oder ausreichend) sind für
ALT und NEU identisch — der Fix ändert nichts am bereits korrekten Verhalten, nur an den beiden
Fällen, in denen der CC-Read selbst fehlschlägt.

## Betroffene Dateien
- `scenes/write/nfc_tools_scene_write_scan.c` — `nfc_tools_ndef_write_worker()`, Phase 4
  (Zeile ~186–207), einziger Fix-Ort. Nutzt das in derselben Funktion bereits vorhandene
  `RETRY_WRITE(...)`-Makro (Zeile ~112–121), keine neue API nötig.

## Risiko
Gering. Reine Ablauf-/Meldungskorrektur, keine neue Hardware-Interaktion — der zusätzliche
Wiederholungsversuch ist exakt derselbe `furi_delay_ms(WRITE_RETRY_DELAY_MS)`/Retry-Mechanismus,
den dieselbe Funktion für alle vier Schreibpfade (ISO15693/MFC/DESFire/MFUL-Seiten) bereits nutzt,
nur ein paar Zeilen früher angewendet. Der einzige Verhaltensunterschied: ein Tag, dessen CC-Read
beim ersten Versuch fehlschlägt, aber danach entweder erfolgreich gelesen wird oder endgültig
fehlschlägt, bekommt jetzt entweder seine echte Kapazität geprüft (statt der zu niedrigen
Fallback-Annahme von 48 Byte) oder eine ehrliche "Write error"-Meldung (statt einer erfundenen
Kapazitätsangabe). Der bereits korrekte Erfolgsfall (CC-Read gelingt sofort) bleibt unverändert,
mit Host-Repro als Regressionsfall bestätigt.

## Port-Skizze / Fix
CC-Read mit demselben Retry-Muster wie die nachfolgenden Seiten-Writes versehen; nur ein
*erfolgreicher* Read darf noch über "Tag too small" entscheiden, ein endgültig fehlgeschlagener
Read wird als das gemeldet, was er ist — ein Schreib-/Kommunikationsfehler:

```c
// Phase 4: read the Capability Container (page 3) to determine the user
// capacity of the tag. Reading the CC is a tag-communication operation
// like the page writes below it, so it gets the same silent one-retry
// treatment; only a *successful* read that reports a real capacity
// smaller than ndef_size is treated as "tag too small".
MfUltralightPage cc_page;
bool cc_ok = (mf_ultralight_poller_sync_read_page(app->nfc, 3, &cc_page) ==
              MfUltralightErrorNone);
RETRY_WRITE(
    cc_ok,
    mf_ultralight_poller_sync_read_page(app->nfc, 3, &cc_page) == MfUltralightErrorNone);

if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
    free(ndef);
    return 0;
}

if(!cc_ok) {
    notification_message(app->notifications, &sequence_error);
    furi_string_set(app->info_str, NTS_ERR_WRITE);
    free(ndef);
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
    return 0;
}

uint16_t capacity = 48; // conservative fallback only if CC lacks the NDEF magic byte
if(cc_page.data[0] == 0xE1) {
    capacity = (uint16_t)cc_page.data[2] * 8;
}

if(ndef_size > capacity) {
    notification_message(app->notifications, &sequence_error);
    furi_string_printf(
        app->info_str,
        "Tag too small!\n%u bytes required\n%u bytes available",
        (unsigned)ndef_size,
        (unsigned)capacity);
    free(ndef);
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
    return 0;
}
```

`RETRY_WRITE` ist an dieser Stelle bereits nutzbar, da das Makro zu Beginn der Funktion definiert
und erst nach deren Ende (`#undef RETRY_WRITE`, Zeile 233) wieder entfernt wird — keine
Verschiebung des Makros nötig. `NTS_ERR_WRITE` ("Write error") ist dieselbe Konstante, die
`write_mful_pages()` bei einem tatsächlichen Schreibfehler bereits verwendet — konsistente
Fehlermeldung für dieselbe Fehlerklasse.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfctools-write-scan-cc-read-retry/repro.c`, host-kompilierbar, keine
Flipper-/NFC-Abhängigkeiten (Befehl und tatsächliche Ausgabe siehe Host-Repro oben).

Für den lokalen Funktionstest an echter Hardware: Fix einbauen (`ufbt launch_app` gegen `nfctools`),
mit einem eigenen NTAG213/215/216-Tag eine Schreib-Szene mit größerem NDEF-Inhalt wählen (z. B.
"Write WiFi", SSID+Passwort zusammen deutlich über 48 Byte). Regressionsfall: normaler Tap, Tag
bleibt während des gesamten Vorgangs stabil im Feld — Schreibvorgang muss wie vor dem Fix
gelingen. Fehlerfall: Tag beim Antippen bewusst kurz kippen/leicht abheben, sodass genau der erste
Lesezugriff (CC, Seite 3) fehlschlägt, der Tag aber im Feld bleibt — vor dem Fix erscheint "Tag too
small!" mit der festen 48-Byte-Angabe unabhängig von der echten Tag-Kapazität; nach dem Fix greift
der stille Retry, und bei stabil im Feld verbleibendem Tag gelingt der Schreibvorgang trotz des
kurzen Aussetzers. Zusatzcheck: mit einem tatsächlich sehr kleinen/unformatierten Tag (CC fehlt oder
`data[2]` klein) weiterhin "Tag too small!" mit der jetzt echten (nicht mehr fallback-48)
Kapazitätsangabe erwartet — Regressionsfall aus dem Host-Repro.

## Zusätzlich geprüft, kein Fund
- `write_mful_pages()`: Seitenweises Schreiben ab Seite 4, letzte Teilseite über die
  Nullinitialisierung von `MfUltralightPage page = {.data = {0, 0, 0, 0}}` korrekt mit Nullen
  aufgefüllt (`chunk < MF_ULTRALIGHT_PAGE_SIZE` im letzten Durchlauf) — kein Fund.
- ISO15693-/MFC-/DESFire-Schreibpfade in derselben Datei: nutzen das `RETRY_WRITE`-Makro bereits
  korrekt, kein zusätzlicher Fund in dieser Sitzung (die jeweiligen Ziel-Schreibfunktionen selbst
  liegen in anderen, bereits bzw. noch nicht geprüften Dateien).
- `nfc_tools_ndef_build()` (`core/nfc_tools_ndef.c`, alle NDEF-Typen inkl. WLAN/Bitcoin/Mail/
  Social/Contact): jede Payload-Länge wird vor `malloc()` exakt berechnet, abschließender
  `furi_check((size_t)(b - buf) == total)` bestätigt die tatsächlich geschriebene Menge —
  kein Speicherzugriffsfehler gefunden; `payload`/`ndef` können nur bei `malloc()`-Fehlschlag
  `NULL` werden (Host-OOM, kein Logikfehler), `write_mful_pages(app, NULL, 0)` bliebe dabei
  ohnehin unkritisch (`num_pages = 0`).
- Alle 32 kleinen Eingabe-Szenen unter `scenes/write/` (Kontakt/Bitcoin/Mail/Social/etc., je
  ~1,1–1,5 KB) stichprobenhaft gegen ihr jeweiliges `ndef_bufN`-Zielfeld geprüft — reine
  Tastatur-Eingabe-Wrapper ohne eigene Fund-Fläche, die eigentliche Logik liegt vollständig in
  `nfc_tools_ndef_build()` (oben geprüft) und `write_scan` (Fund oben).

## Quelle
- `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`, dieselbe Quelle wie alle bisherigen
  NFC-Tools-Funde dieser Sitzungsreihe): `applications/external/nfctools/`, `fap_author` "wakdev",
  Original-Quelle `github.com/wakdev/nfctools-fz`, `fap_version` 1.3.
- API-87-Einordnung: nutzt ausschließlich die bereits im Cluster verifizierten
  `mf_ultralight_poller_sync_read_page()`/`mf_ultralight_poller_sync_write_page()`-Signaturen
  (`lib/nfc/protocols/mf_ultralight/mf_ultralight_poller_sync.h`) — gegen meinen eigenen
  `mntm-012`-Stand erneut direkt verifiziert (Signaturen identisch). Kein
  firmwareversionsabhängiges Kompatibilitätsrisiko, keine neue API-Fläche; der Fix nutzt
  ausschließlich ein in derselben Datei bereits vorhandenes Makro.
- Eigener Fund: `scenes/write/nfc_tools_scene_write_scan.c`, `nfc_tools_ndef_write_worker()`,
  Phase 4 (CC-Read ohne Retry), mit Host-Repro bestätigt (siehe
  `firmware-mining/repro-nfctools-write-scan-cc-read-retry/repro.c`).
- Restlicher Umfang des Clusters: `scenes/write/nfc_tools_scene_felica_write.c`/
  `_felica_write_input.c` (FeliCa-Schreibpfad, noch nicht geprüft), Tag-Info-/QR-Code-Detail-
  Lesepfade sowie `chameleon`/`nfc_relay` (Fremdgerät/Zweitgerät) bleiben für künftige Sitzungen
  offen.
