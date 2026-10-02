# WiFi-Recon (heltec-ukfe-rx) — Probe-Request-SSID-Parse liest ein Byte über das
# erfasste 802.11-Frame hinaus, wenn ein Probe Request ohne jede IE eintrifft

## Was

`src/wifi_recon.cpp::promisc_cb()` verarbeitet jedes per Promiscuous-Callback
empfangene 802.11-Frame. Der Längen-Guard am Funktionsanfang lässt Frames mit
genau 24 Byte (der reine MAC-Header, keine einzige Information-Element) passieren:

```c
int len = p->rx_ctrl.sig_len;
if(len < 24) return;   // len == 24 passiert diesen Guard
```

Im `RC_PROBE`-Zweig (Probe-Request-SSID-Erkennung) wird das SSID-Tag-Byte an
Offset 24 bisher ungeprüft gelesen:

```c
uint8_t slen = (len > 25) ? fr[25] : 0;   // SSID-Tag: fr[24]=0x00, fr[25]=len
char ssid[33]; uint8_t n = slen > 32 ? 32 : slen;
if(fr[24] == 0x00 && 26 + n <= len) { memcpy(ssid, &fr[26], n); ssid[n] = 0; }
else { ssid[0] = 0; }
```

Der `len > 25`-Guard vor `fr[25]` ist korrekt, aber `fr[24]` selbst wird
unabhängig von `len` gelesen. Bei `len == 24` (ein Probe Request, der nach dem
MAC-Header sofort endet — kein gültiges, aber ein über die Luft von jedem
Angreifer frei erzeugbares Frame, da `promisc_cb()` jedes empfangene 802.11-Frame
ungefiltert durchreicht) ist Offset 24 bereits ein Byte hinter dem von
`rx_ctrl.sig_len` gemeldeten Frame-Ende — ein Lesezugriff außerhalb der
gemeldeten Framegrenze. Mein Host-Repro (unten) bestätigt das als klassischen
Heap-Buffer-Overflow-READ unter AddressSanitizer.

Die restlichen Zweige (`RC_HANDSHAKE`, `RC_PACKETMON`, `RC_PWNA`, `RC_RID`) habe
ich beim selben Review gegengeprüft: deren Schleifen prüfen die Längengrenze
jeweils direkt in der Schleifenbedingung (`i + 1 < len`, `i + 4 < len`,
`i + 2 < len` mit zusätzlichem `i + 2 + ilen > len`-Break), bevor sie den
jeweiligen Index lesen — dort kein vergleichbarer Fund.

## Nutzen

`wifi_recon_probe()` läuft typischerweise minutenlang im Dauerbetrieb über alle
13 Kanäle und verarbeitet dabei jedes eintreffende Frame ungefiltert — die
Eingabe steht vollständig unter der Kontrolle jedes Geräts in Funkreichweite.
Der Fix schließt den einzigen in diesem Modul gefundenen Fall, in dem ein ganz
regulär aussehendes, aber absichtlich oder zufällig extrem kurzes Probe-Request-
Frame einen Zugriff außerhalb der gemeldeten Framegrenze auslöst — unabhängig
davon, ob das in der konkreten ESP-IDF-Pufferallokierung aktuell zu einem
Absturz führt oder nicht, ist das Verhalten damit korrekt statt zufällig richtig.

## Betroffene Dateien

- `heltec-ukfe-rx/src/wifi_recon.cpp` (`promisc_cb()`, `RC_PROBE`-Zweig)

(`lilygo-ukfe-rx` und `heltec-v4-hub` besitzen kein `wifi_recon.cpp` — reine
ESP-NOW-/USB-HID-Satelliten bzw. -Hub ohne eigenes WiFi-Promiscuous-Sniffing —
der Fund betrifft ausschließlich `heltec-ukfe-rx`.)

## Risiko

Reiner Bounds-Check-Fix, keine Verhaltensänderung für jedes Frame mit
mindestens einer IE (`len > 24`) — das Host-Repro zeigt für vier weitere
Szenarien (nur Tag-Byte ohne Längen-Byte, benannte SSID, Broadcast-Probe mit
SSID-Länge 0, maximale SSID-Länge 32) byte-identisches Verhalten vor und nach
dem Fix. Der Fix ist rein additiv (eine zusätzliche Bedingung in einem
bestehenden `if`), keine Protokoll-/ESP-NOW-Berührung.

## Fix

```c
if(len > 24 && fr[24] == 0x00 && 26 + n <= len) { memcpy(ssid, &fr[26], n); ssid[n] = 0; }
else { ssid[0] = 0; }
```

Patch in `wifi-recon-probe-ssid-tag-oob-read-fix/wifi_recon_cpp.patch`
(`git apply --check` gegen einen frischen, unabhängigen Klon verifiziert, real
angewendet und byte-für-byte mit der beabsichtigten Endfassung abgeglichen).

## Testhinweis (lokal)

1. Patch anwenden (`git apply --check` vorab, dann real), `pio run` für
   `heltec-ukfe-rx` — reine Ein-Zeilen-Änderung in einer bestehenden Bedingung,
   kein neuer Include/keine neue Abhängigkeit.
2. Host-Repro (`repro-wifi-recon-probe-ssid-tag-oob-read/repro.cpp`) deckt die
   reine SSID-Tag-Parse-Logik ab (kein WiFi/Arduino nötig, `malloc()` statt des
   ESP-IDF-Promiscuous-Puffers, damit AddressSanitizer jeden Zugriff über die
   gemeldete Framelänge hinaus zuverlässig erkennt): fünf Szenarien — der
   Fund-Grenzfall `len == 24` (löst im unveränderten Code einen von
   AddressSanitizer erkannten Heap-Buffer-Overflow-READ aus, im gefixten Code
   nicht), `len == 25` mit nur dem Tag-Byte, eine normale benannte SSID, eine
   Broadcast-Probe (SSID-Länge 0) und eine maximale 32-Byte-SSID. Für die vier
   Nicht-Grenzfall-Szenarien zusätzlich Byte-für-Byte-Diff zwischen unveränderter
   und gefixter Logik bestätigt: identische Ausgabe. Unter
   `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined` bei `-O0` und
   `-O1`: gefixte Logik — ALLE CHECKS BESTANDEN, keine ASan-/UBSan-Meldungen,
   keine Compiler-Warnungen; unveränderte Logik — AddressSanitizer meldet beim
   Grenzfall-Szenario zuverlässig `heap-buffer-overflow READ of size 1`.
3. Funktionstest auf Hardware (optional, da bereits über das Host-Repro
   abgedeckt): mit einem zweiten WiFi-fähigen Gerät einen Probe Request mit
   leerer/fehlender SSID-IE senden (die meisten Treiber fügen immer eine
   SSID-IE ein, daher ist dafür typischerweise ein eigenes Rohframe-Tool wie
   `scapy` mit `dot11.addfield`-Manipulation auf einem zweiten Gerät nötig,
   um das `len == 24`-Frame gezielt zu erzeugen) — vor dem Fix ggf. Absturz/
   Reset des Heltec V3 während `wifi_recon_probe()` läuft, danach nicht mehr.
