# ESP-NOW-Mesh (heltec-ukfe-rx, lilygo-ukfe-rx) — ACK-Absender-MAC wird global
# gehalten und erst nach dem blockierenden act()-Aufruf gelesen

## Was

Beide ESP-NOW-Satelliten (`heltec-ukfe-rx`, `lilygo-ukfe-rx`) merken sich die Absender-MAC
eines eingetroffenen `ukfe_rf`-Frames ausschließlich in der globalen Mailbox-Variable
`enowSenderMac`, gesetzt im Empfangs-Callback:

```c
// heltec-ukfe-rx/src/main.cpp:74-81 (lilygo-ukfe-rx/src/main.cpp:53-60 identisch)
void onEspNowRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if(enowFlag) return;             // vorheriger Frame noch nicht verarbeitet
    if(len <= 0 || len > (int)UKFE_RF_MAX_FRAME) return;
    memcpy(enowSenderMac, mac, 6);   // Absender fuer den ACK merken
    memcpy(enowBuf, data, len);
    enowLen = len;
    enowFlag = true;
}
```

In `loop()` wird der eigentliche Frame (`enowBuf`→`frame[]`) korrekt **vor** dem Freigeben
der Mailbox (`enowFlag = false`) in eine lokale Kopie übernommen — die Absender-MAC dagegen
nicht:

```c
// heltec-ukfe-rx/src/main.cpp:424-441
if(enowFlag) {
    int len = enowLen;
    uint8_t frame[UKFE_RF_MAX_FRAME];
    memcpy(frame, enowBuf, len);     // Frame: lokale Kopie VOR dem Freigeben
    enowFlag = false;                // Puffer wieder freigeben
    enowRx++;
    ...
    if(ukfe_rf_parse_frame(UKFE_SECRET, frame, real_len, &msg, &enowCounter)) {
        enowOk++;
        ...
        act(&msg);                                      // blockierend, bis zu mehrere Sekunden
        send_espnow_ack(enowSenderMac, msg.cmd, 0);      // liest die GLOBALE MAC JETZT
    } else { ... }
}
```

(`lilygo-ukfe-rx/src/main.cpp:260-283`, dieselbe Struktur: `frame[]` wird vor `enowFlag =
false` kopiert, `enowSenderMac` erst nach `act(&msg)` für den `send_espnow_ack()`-Aufruf
gelesen.)

Da `enowFlag` schon **vor** `act(&msg)` wieder auf `false` gesetzt wird, ist die Mailbox
während der gesamten (teils mehrsekündigen) Laufzeit von `act()` erneut scharf: ein
WiFi-Scan (laut eigenem Kommentar "blockierend ~2 s"), ein BLE-Scan (Default 3000 ms), ein
HID-Ducky-/HID-Stream-Lauf (Keyboard-Tippen Zeichen für Zeichen) oder ein Wardrive-Scan
lassen `onEspNowRecv()` in dieser Zeit anstandslos einen neuen Frame annehmen — inklusive
Überschreiben von `enowSenderMac` mit der MAC des **neuen** Absenders. Trifft während eines
laufenden `act()` ein Frame von einem anderen Sender ein (z. B. einem zweiten, parallel
betriebenen Hub — die eigene Topologie sieht sowohl `heltec-v4-hub` als auch das ältere
`g4meover-wifi-relay` als ESP-NOW-Broadcast-Quelle vor, beide potenziell gleichzeitig in
Betrieb), geht der anschließend gesendete `send_espnow_ack()` für den **ursprünglichen**
Befehl an die MAC des **zweiten, zwischenzeitlich eingetroffenen** Senders — nicht an den,
dessen Befehl gerade quittiert werden soll. Der eigentliche Frame (`frame[]`/`msg`) ist
davon nicht betroffen (bereits sauber lokal kopiert) — der Befehl selbst wird also immer
korrekt ausgeführt, nur die Quittung landet beim falschen Gerät.

Kein Sicherheitsproblem (beide möglichen Empfänger sind eigene, bereits per MAC bekannte
Geräte im selben Mesh, keyed MAC + CRC16 bleiben unberührt), aber ein eigenständiger
Verfügbarkeits-/Korrektheitsfund: das Deck/der Hub, dessen Befehl tatsächlich lief, sieht
nie eine Bestätigung, während ein anderes Gerät eine ihm nicht zustehende ACK-Antwort auf
einen Befehl erhält, den es nie gesendet hat.

## Nutzen

Die Zusage "Signierter ACK ... zurück an den Absender" (siehe Kommentar über
`send_espnow_ack()` in beiden Dateien) gilt nach dem Fix auch dann noch, wenn während eines
blockierenden `act()`-Laufs ein zweiter Sender dasselbe Gerät anspricht — ohne dafür den
bereits bewährten Mailbox-Mechanismus (ein Frame gleichzeitig, Rest wird bis zur nächsten
`loop()`-Iteration zurückgehalten) zu verändern.

## Betroffene Dateien

- `heltec-ukfe-rx/src/main.cpp` (ESP-NOW-Verarbeitungsblock in `loop()`) — Patch:
  `espnow-ack-sender-mac-stale-after-blocking-act-fix/heltec-ukfe-rx_main_cpp.patch`
- `lilygo-ukfe-rx/src/main.cpp` (ESP-NOW-Verarbeitungsblock in `loop()`) — Patch:
  `espnow-ack-sender-mac-stale-after-blocking-act-fix/lilygo-ukfe-rx_main_cpp.patch`
- `heltec-v4-hub` und `g4meover-wifi-relay` sind **nicht** betroffen: beide senden selbst
  keinen `send_espnow_ack()` als Antwort auf einen empfangenen ESP-NOW-Frame (der Hub
  verarbeitet nur eingehende Satelliten-ACKs weiter zum Flipper, sendet aber keine eigenen).
  `ukfe_rf.c`/`.h` bleiben unverändert — reiner Kontrollfluss-Fix in den beiden Satelliten,
  kein Protokolleingriff.

## Fix-Skizze

In beiden Dateien wird die Absender-MAC — genau wie der Frame selbst bereits — als lokale
Kopie gesichert, **bevor** die Mailbox freigegeben wird:

```c
uint8_t frame[UKFE_RF_MAX_FRAME];
memcpy(frame, enowBuf, len);
uint8_t ackMac[6];
memcpy(ackMac, enowSenderMac, 6);  // Absender-MAC zusammen mit dem Frame sichern
enowFlag = false;
...
act(&msg);
send_espnow_ack(ackMac, msg.cmd, 0);   // liest jetzt die lokale Kopie, nicht mehr die globale
```

Ein zwischenzeitlich eintreffender zweiter Frame überschreibt weiterhin `enowSenderMac`/
`enowBuf`/`enowLen` — das ist unverändert gewollt (Mailbox-Semantik, nächste `loop()`-
Iteration verarbeitet ihn regulär) — aber er überschreibt nicht mehr die MAC, die für den
bereits in Bearbeitung befindlichen ACK gebraucht wird, da diese schon vorher in `ackMac`
kopiert wurde.

Patches gegen zwei frische, unabhängige Zweitkopien der jeweiligen Repos per `git apply
--check` sowie per `patch -p1` verifiziert, Ergebnis laut `md5sum` in beiden Fällen
byte-identisch.

## Host-Repro

`repro-espnow-ack-sender-mac-stale/repro.c` bildet die Mailbox-Felder (`enowFlag`/
`enowBuf`/`enowLen`/`enowSenderMac`) sowie den `loop()`-Kontrollfluss beider Dateien 1:1
nach; `act()` ist durch einen Hook ersetzt, der währenddessen gezielt einen zweiten
`onEspNowRecv()`-Aufruf von einem anderen Absender auslöst (bildet nach, was auf echter
Hardware bei einem ESP-NOW-Interrupt während eines mehrsekündigen WiFi-Scans/BLE-Spams/
HID-Ducky-Laufs passieren würde). Drei Szenarien:

1. **ALT** (Ist-Zustand): Sender A schickt einen Frame, währenddessen trifft einer von
   Sender B ein — der anschließende ACK für A's Befehl geht an B. Reproduziert den Fund.
2. **FIX**: derselbe Ablauf mit lokalem `ackMac`-Snapshot — der ACK geht weiterhin korrekt
   an A; B's Frame bleibt dabei unverändert für die nächste `loop()`-Iteration in der
   Mailbox erhalten (kein Datenverlust durch den Fix).
3. **Regression**: ohne einen zweiten, dazwischenkommenden Sender liefern ALT und FIX
   exakt dasselbe Ergebnis — keine Verhaltensänderung im Normalfall.

```
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O0 -o repro repro.c && ./repro
gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -O1 -o repro repro.c && ./repro
```

Unter `-O0` und `-O1`: ALLE CHECKS BESTANDEN, identischer Output beider Stufen, keine
Compiler-Warnungen, keine ASan-/UBSan-Meldungen.

## Risiko

Verfügbarkeits-, kein Sicherheitsrisiko. Keyed MAC + CRC16 (`ukfe_rf.c`, unverändert)
bleiben für die eigentliche Befehlsausführung vollständig maßgeblich — der Fund betrifft
ausschließlich, an welche (ohnehin bereits bekannte, eigene) Geräte-MAC die Erfolgs-Quittung
nach einem bereits korrekt ausgeführten Befehl adressiert wird. Der Patch ändert nur die
Quelle der an `send_espnow_ack()` übergebenen MAC (lokale Kopie statt globale Variable) —
keine Änderung an Frame-Aufbau, Counter-Fenster oder MAC/CRC-Prüfung, rückwirkungsfrei zum
bisherigen Verhalten im Nicht-Race-Fall (siehe Szenario 3 im Repro).

## Testhinweis

Lokal (PlatformIO, beide betroffenen Projekte):
1. Jeweiligen Patch anwenden, bauen, auf Heltec V3 (`heltec-ukfe-rx`) bzw. LilyGo T-Dongle
   S3 (`lilygo-ukfe-rx`) flashen.
2. Funktionstest braucht zwei unterscheidbare ESP-NOW-Sender auf demselben Kanal (z. B.
   `heltec-v4-hub` und zusätzlich `g4meover-wifi-relay`, beide gleichzeitig aktiv, oder
   ersatzweise ein zweites Testgerät, das rohe `ukfe_rf`-Broadcast-Frames mit einer anderen
   MAC sendet): einen Befehl mit mehrsekündiger Laufzeit auslösen (z. B. `UkfeRfCmdWifiScan`
   oder `UkfeRfCmdBleScan`), währenddessen vom zweiten Sender einen weiteren Befehl senden —
   vor dem Fix bekommt der zweite Sender die ACK für den Befehl des ersten, nach dem Fix
   bekommt jeder Sender die ACK für seinen eigenen Befehl.
3. Reine Logik bereits hier per Host-Repro verifiziert (siehe oben) — nicht gegen die
   ESP32-S3-Toolchain kompiliert.

## Offen

- Lokaler Zweisender-Funktionstest wie oben beschrieben (braucht zwei gleichzeitig aktive
  ESP-NOW-Quellen, bisher nur per Host-Repro nachvollzogen).
- Verwandter, bewusst nicht mitbehobener Punkt: ob und wie das Deck auf eine *fehlende*
  ACK (statt einer falsch adressierten) zeitlich reagiert — reine Darstellungsfrage auf der
  Deck-Seite, nicht Teil dieses Funds.

## Quelle

Eigener Soucecode-Durchgang durch `heltec-ukfe-rx/src/main.cpp` und
`lilygo-ukfe-rx/src/main.cpp` (ESP-NOW-Verarbeitungsblock in `loop()`), angestoßen durch den
in der vorletzten Sitzung als wenig durchgesehen vermerkten Bereich
(`ukfe-esp-now-mesh/espnow-origin-tag-counter-window-split-fix.md` war bisher der einzige
Fund in diesem Verzeichnis). Unabhängig von diesem früheren Fund (der betraf die
Zähler-Fenster-Trennung zwischen Hub-Broadcasts und relayten Flipper-Frames) — hier geht es
um die ACK-Rückadressierung selbst, nicht um das Anti-Replay-Fenster.
