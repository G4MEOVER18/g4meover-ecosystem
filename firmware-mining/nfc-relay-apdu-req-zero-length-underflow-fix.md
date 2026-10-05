# NFC Relay (leommxj/nfc_relay, RogueMaster-Bündel) — Längen-Unterlauf im ApduReq-Empfang

## Was
Ich habe den seit mehreren Sitzungen als letzten verbliebenen Kandidaten im NFC-Werkzeug-Cluster
zurückgestellten `nfc_relay`-Fall (`ideas/pentest-app-katalog.md`, bisher nur als
"Fremd-/Zweitgerät-Fall, zwei eigene Flipper-Geräte nötig" eingeordnet, nicht quelltextlich
geprüft) jetzt doch gelesen — wie beim bereits dokumentierten `chameleon`-Fund zeigt sich: auch
ein Werkzeug, das zwingend zwei eigene Geräte braucht, hat einen eigenen Protokoll-Parser für die
Relaisverbindung, der unabhängig vom Zweitgerät-Status geprüft werden kann.

Die App (`applications/external/nfc_relay/`, `fap_author` "leommxj", Quelle
`leommxj/nfc_relay`) lässt ein Flipper-Pärchen eine ISO14443-4A-Karte relayen: ein Gerät liest
als "AsReader" eine echte Karte, schickt deren Antworten über eine serielle Relaisverbindung an
das zweite Gerät, das sich als "AsCard" gegenüber dem eigentlichen Leser als diese Karte ausgibt
(und umgekehrt für die APDU-Anfragen des echten Lesers). Laut eigenem `README.md` ist die
Relaisverbindung dabei nicht zwingend ein direktes UART-Kabel zwischen zwei nebeneinander
liegenden Geräten — der Autor dokumentiert dort explizit ein Setup mit zwei ESP-AT-WLAN-Modulen,
die die UART-Bytes über das offene Internet zu einem `socat`-Relay-Server brücken. Die
Relaisverbindung ist damit, je nach Aufbau, potenziell weit exponierter als ein kurzes Kabel
zwischen zwei Geräten auf dem Tisch.

`as_reader_worker.c::as_reader_worker_task()` behandelt ein über diese Verbindung empfangenes
`NfcRelayPacketApduReq`-Paket (die vom "AsCard"-Gerät weitergereichte APDU-Anfrage des echten
Lesers) so:

```c
// as_reader_worker.c, ALT — as_reader_worker_task(), Zustand AsReaderWorkerStateInteractive:
recv_packet = comm_wait_packet(as_reader_worker->comm, NfcRelayPacketApduReq);
if(!recv_packet) { continue; }
FURI_LOG_D(TAG, "Recv NfcRelayPacketApduReq");
// drop PCB
bit_buffer_copy_bytes(
    as_reader_worker->bitbuffer_tx, &recv_packet->buf[1], recv_packet->len - 1);
```

`comm_wait_packet()` (`comm.c`) prüft beim Entgegennehmen ausschließlich den Pakettyp, nicht
dessen Länge. `recv_packet->len` ist ein `uint8_t`-Feld, das vollständig von der Gegenseite der
Relaisverbindung bestimmt wird — `uart_worker_task()` (`uart_worker.c`) erzeugt für `len == 0`
einen vollkommen gültigen Null-Byte-Payload-Rahmen (eigener `if(len == 0)`-Zweig, `malloc()`
reserviert dafür nur den Paket-Header, kein zusätzliches `buf[]`). Ein `NfcRelayPacketApduReq`
mit `len = 0` ist also ein gültiges, drei Byte langes Wire-Paket (`typ`, `len=0`), kein
Sonderfall, der irgendeine Prüfung verletzt.

Trifft ein solches Paket ein, wertet der Code `recv_packet->len - 1` aus: `len` ist `uint8_t`,
die Subtraktion promotet nach `int` (`0 - 1 = -1`), und dieser negative `int`-Wert wird
anschließend dem `size_t size_bytes`-Parameter von `bit_buffer_copy_bytes()`
(`lib/toolbox/bit_buffer.c`) übergeben — auf der eigentlichen Zielarchitektur (STM32WB55,
Cortex-M4, `size_t == uint32_t`) wird daraus `0xFFFFFFFF`. `bit_buffer_copy_bytes()` selbst
prüft vor dem eigentlichen `memcpy()`:

```c
// lib/toolbox/bit_buffer.c — bit_buffer_copy_bytes() (bereits vorhandene, korrekte Prüfung):
furi_check(buf->capacity_bytes >= size_bytes);
```

`bitbuffer_tx` ist mit `bit_buffer_alloc(256)` angelegt (256 Byte Kapazität), die Prüfung
`256 >= 0xFFFFFFFF` schlägt also fehl — `furi_check()` bricht den Prozess kontrolliert ab
(`furi_crash()` im echten Gerät: sofortiger Geräteabsturz/-neustart). Der Abbruch passiert
*vor* dem `memcpy()` selbst; es handelt sich damit um einen sauberen, aber aus der Ferne über
die Relaisverbindung auslösbaren Denial-of-Service (Geräteabsturz des "AsReader"-Geräts durch ein
einzelnes, drei Byte kurzes, ansonsten unauffälliges Paket), nicht um eine Speicherkorruption.

Ich habe dabei gezielt gegengeprüft, ob derselbe Fundtyp (ungeprüfte Subtraktion eines
peer-kontrollierten `uint8_t`-Längenfelds) anderswo im selben Cluster vorkommt: die
Geschwisterstelle in `as_card_worker.c` (`as_card_worker->apdu_buf_len = recv_packet->len + 1;`,
gefolgt von `memcpy(&as_card_worker->apdu_buf[1], recv_packet->buf, recv_packet->len);`) nutzt
nur eine Addition (`len + 1`, maximal 256 bei `len = 255`, passt unverändert in das 260 Byte
große `apdu_buf`) und ist von diesem Unterlauf nicht betroffen — geprüft, kein zweiter Fund. Den
zunächst vermuteten Kandidaten in `as_card_worker.c::as_card_worker_task()`
(`recv_packet->len == ((SerializedIso14443_4a*)recv_packet->buf)->len_t1_tk +
sizeof(SerializedIso14443_4a)`, `NfcRelayPacketNfcDevData`-Pfad) habe ich ebenfalls auf einen
mutmaßlichen Ganzzahlüberlauf-Bypass hin durchgerechnet und wieder verworfen: die Prüfung
verlangt vorab `recv_packet->len > sizeof(SerializedIso14443_4a)`, wodurch die Differenz
`recv_packet->len - sizeof(SerializedIso14443_4a)` bereits klein und positiv ist — die
Kongruenzgleichung `len_t1_tk + sizeof(SerializedIso14443_4a) ≡ recv_packet->len (mod 2^32)` hat
dann in der vollen 32-Bit-Wertemenge genau eine Lösung für `len_t1_tk`, nämlich exakt diese
kleine, legitime Differenz — kein zweiter, großer Lösungswert, der die Prüfung mit einem
bösartig großen `len_t1_tk` umgehen würde. Diese Stelle ist entgegen meiner ersten Vermutung
tatsächlich bereits korrekt.

## Nutzen
- Schließt den einzigen noch offenen Codereview-Rückstand im NFC-Werkzeug-Cluster
  (`ideas/pentest-app-katalog.md`): nach `chameleon` ist jetzt auch `nfc_relay` als
  "Fremd-/Zweitgerät, aber quelltextlich geprüft" eingeordnet, der Cluster ist damit
  vollständig durchgesehen.
- Der Fund betrifft die Robustheit des eigenen "AsReader"-Geräts gegenüber der Gegenseite der
  Relaisverbindung — relevant sowohl bei einer fehlerhaften/inkompatiblen Firmware auf dem
  zweiten eigenen Gerät (z. B. während eigener Weiterentwicklung an `as_card_worker.c`) als auch,
  laut der eigenen Vorgabe in `README.md`, bei dem vom Autor selbst dokumentierten
  Internet-Relay-Aufbau über zwei ESP-AT-Module — dort reicht ein einzelnes verlorenes/
  verstümmeltes Byte auf der UDP-Brücke, um aus einem beliebigen anderen Paket zufällig ein
  gültiges `len=0`-ApduReq-Paket entstehen zu lassen, oder ein absichtlich eingeschleustes
  Paket eines Dritten mit Zugriff auf die Relay-Strecke.
- Die Fix-Form ist minimal-invasiv (eine zusätzliche Längenprüfung vor der bestehenden
  Subtraktion) und ändert am gültigen Protokollverhalten (jedes Paket mit `len >= 1`) nichts.

## Betroffene Dateien (lokal)
Falls diese App künftig nach `applications_user/nfc_relay/` portiert wird (bisher nicht Teil
meines eigenen Firmware-Baums, da als Zweitgerät-Fall zurückgestellt, siehe
`ideas/pentest-app-katalog.md`):
- `applications_user/nfc_relay/as_reader_worker.c` — **mit dem unten beschriebenen Fix**
- alle übrigen Dateien unter `applications_user/nfc_relay/` unverändert aus
  `RogueMaster/flipperzero-firmware-wPlugins` (Branch `420`,
  `applications/external/nfc_relay/`)

Der Fund und Fix stehen unabhängig von einem tatsächlichen Port für sich — relevant auch für
jeden, der `leommxj/nfc_relay` direkt aus der Originalquelle nutzt.

## Risiko
- Niedrig für den Fix selbst: additive Längenprüfung an einer einzigen Stelle, lässt jedes
  Paket mit `len >= 1` (der gesamte bisher funktionierende Bereich) unverändert durch.
- **API-87/`mntm-012`-Kompatibilität**: Die App nutzt ausschließlich Standard-NFC-/Furi-APIs
  (`nfc_poller_*`, `nfc_listener_*`, `BitBuffer`, `FuriHalSerial`, Standard-GUI-Module) — gegen
  den `mntm-012`-Tag selbst nicht nachgebaut (zwei eigene Geräte für einen echten Funktionstest
  nötig), aber `bit_buffer_copy_bytes()`s Signatur und interne `furi_check`-Prüfung sind seit
  Langem stabiler Bestandteil von `lib/toolbox/bit_buffer.c` und in meinem eigenen
  `mntm-012`-Baum unverändert vorhanden (bereits für andere Ports in diesem Repo genutzt, z. B.
  `firmware-mining/uhf-rfid-ring-buffer-oob-read-fix.md`).
- **Der gefundene Fehler selbst** bleibt ein kontrollierter `furi_check()`-Abbruch (Absturz/
  Neustart), keine Speicherkorruption — die Kapazitätsprüfung in `bit_buffer_copy_bytes()`
  selbst ist bereits korrekt und fängt den unterlaufenen Wert ab, bevor er als `memcpy()`-Länge
  verwendet wird. Das eigentliche Problem ist die fehlende Validierung einer aus der Ferne
  kontrollierten Paketlänge, bevor sie in eine vorzeichenbehaftete Subtraktion einfließt.
- Restrisiko nach dem Fix: ein `len=0`-Paket wird jetzt still verworfen (nur ein
  `FURI_LOG_E`-Eintrag) und die Schleife wartet auf das nächste `NfcRelayPacketApduReq`-Paket —
  kein Status-/Fortschrittsverlust über das reine Ignorieren des einen fehlerhaften Pakets
  hinaus, da das Relay-Protokoll ohnehin verlustbehaftet ist (keine Sequenznummern/ACKs auf
  Anwendungsebene über das reine Ping/Pong hinaus).

## Port-Skizze
1. Bei einem künftigen Port: App-Ordner `nfc_relay` unverändert aus
   `RogueMaster/flipperzero-firmware-wPlugins` (`applications/external/nfc_relay/`) nach
   `applications_user/nfc_relay/` kopieren.
2. Patch `nfc-relay-apdu-req-zero-length-underflow-fix/as_reader_worker_c.patch` (unten) gegen
   `as_reader_worker.c` anwenden — fügt direkt nach dem Empfang eines
   `NfcRelayPacketApduReq`-Pakets eine Prüfung auf `len == 0` ein, die das Paket verwirft, bevor
   die bestehende `recv_packet->len - 1`-Subtraktion erreicht wird.
3. `ufbt launch_app APPSRC=applications_user/nfc_relay` bzw.
   `fbt launch_app APPSRC=applications_user/nfc_relay` — deckt etwaige Header-Abweichungen als
   Compile-Fehler auf.
4. Host-seitiges Mini-Repro (`repro.c`, unten) mit `gcc -O0`/`-O1 -Wall -Wextra
   -fsanitize=address,undefined` bauen und laufen lassen, um ALT (Absturz bei `len=0`) gegen
   NEU (sauberes Verwerfen, identisches Verhalten für `len >= 1`) zu bestätigen — bereits lokal
   ausgeführt, siehe Testhinweis.
5. Funktionstest an zwei eigenen Geräten: normalen Relay-Betrieb gegen eine echte
   ISO14443-4A-Karte (z. B. Bankkarte/Zugangskarte) durchführen — jede reguläre APDU-Anfrage
   ist mindestens 1 Byte lang (PCB-Byte), bleibt vom Fix unberührt.
6. Den eigentlichen Fund (`len=0`-Paket) am Gerät nachzustellen braucht entweder eine bewusst
   modifizierte Gegenseite (zweites Gerät mit angepasster `as_card_worker.c`, die ein leeres
   `NfcRelayPacketApduReq`-Paket sendet) oder ein kleines Skript auf der seriellen/UDP-Strecke,
   das die drei Bytes `typ=0x63, len=0x00` einspielt — für den Nachweis reicht das Host-Repro,
   am Gerät nur der Normalfall zwingend zu testen.

## Testhinweis (lokal nachvollzogen)
`firmware-mining/repro-nfc-relay-apdu-req-zero-length-underflow/repro.c` transkribiert den
Paketempfang (`NfcRelayPacket`), die relevante `bit_buffer_copy_bytes()`-Kapazitätsprüfung
(`lib/toolbox/bit_buffer.c`) sowie den ApduReq-Behandlungsausschnitt aus
`as_reader_worker_task()` 1:1 (ALT) und ergänzt in einer zweiten Fassung (NEU) nur die
vorgeschlagene `len == 0`-Prüfung. `size_t` wird dabei explizit als `uint32_t` nachgebildet, um
die tatsächliche 32-Bit-Zielarchitektur (Cortex-M4) abzubilden, nicht den ggf. 64-Bit-Host.

Sechs Prüfschritte:
- Fund bei `len = 0`: ALT bricht reproduzierbar mit einem `furi_check`-Abbruch (`SIGABRT`) ab,
  in einem eigenen `fork()`-Kindprozess isoliert.
- Fix-Nachweis bei `len = 0`: NEU übersteht denselben Fall ohne Absturz (Paket wird verworfen).
- Randfall `len = 1` (nur PCB-Byte, 0 Byte Nutzinhalt danach): weder ALT noch NEU stürzen ab
  (0 Byte liegt innerhalb der 256-Byte-Kapazität).
- Sanity-Fälle `len = 5` und `len = 255`: kein Absturz in ALT oder NEU.
- Äquivalenztest über den gesamten gültigen Bereich `len = 1..255`: ALT und NEU liefern
  byte-identischen Kopierinhalt und identisches `size_bits`-Ergebnis in allen 255 Fällen (NEU
  ändert das bestehende Verhalten für gültige Pakete nicht).

Unter `gcc -std=c11 -O0`/`-O1 -Wall -Wextra -fsanitize=address,undefined` verifiziert: ALT
stürzt beim `len=0`-Fall reproduzierbar ab, NEU übersteht denselben Fall, Randfall-, Sanity- und
Äquivalenztests liefern in allen Fällen das erwartete bzw. identische Ergebnis — ALLE CHECKS
BESTANDEN, keine ASan-/UBSan-Meldungen, keine Compiler-Warnungen.

Patch (`git apply --check` vorab gegen eine frische, unabhängige Kopie von
`RogueMaster/flipperzero-firmware-wPlugins`, Branch `420`, verifiziert, real angewendet und
byte-für-byte mit der beabsichtigten Endfassung abgeglichen) in
`firmware-mining/nfc-relay-apdu-req-zero-length-underflow-fix/as_reader_worker_c.patch`.
