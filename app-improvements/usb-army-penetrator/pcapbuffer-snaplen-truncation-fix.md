# usb-army-penetrator — PcapBuffer: fehlender Snaplen-Clamp beim Capture

## Was
`src/Utilities/PcapBuffer.cpp` ist der gemeinsame Pcap-Writer, den ich für
jede Paket-Aufzeichnung nutze (aktuell verdrahtet über `USBNCM::startPacketCollection()`
für USB-NCM-Ethernet-Traffic, `src/Devices/USB/USBNCM.cpp`; als generische
Utility-Klasse ist sie der naheliegende Ansatzpunkt für jede künftige
WiFi-/BLE-Paket-Aufzeichnung aus dem Marauder-Modul). Der Konstruktor schreibt
den globalen Pcap-Header mit einer festen Snaplen-Zusage:

```cpp
#define MAX_PACKET_LEN 2324

write(m_file, uint32_t(MAX_PACKET_LEN)); // max length of captured packets, in octets
```

`append()` nimmt aber jede beliebige Paketlänge `len` ohne jeden Vergleich
gegen `MAX_PACKET_LEN` an, alloziert genau `len` Bytes und kopiert sie
vollständig:

```cpp
bool PcapBuffer::append(uint8_t *packet, uint32_t len)
{
    if (m_blocks.size() > MAX_SIZE) { return false; }
    uint8_t* buffer = (uint8_t*) malloc(len);   // <-- kein Vergleich mit MAX_PACKET_LEN
    if (!buffer) { return false; }
    memcpy(buffer, packet, len);
    PcapBufferBlock block { .time = curTime, .buffer = buffer, .length = len };
    m_blocks.push(block);
    return true;
}
```

`flushToDisk()` schreibt anschließend `block.length` unverändert sowohl als
`incl_len` als auch als `orig_len` in den Record-Header:

```cpp
write(m_file, block.length); // incl_len
write(m_file, block.length); // orig_len
write(m_file, block.buffer, block.length); // packet payload
```

Für ein Paket mit `len > MAX_PACKET_LEN` entsteht damit ein `.pcap`, dessen
`incl_len` den im globalen Header selbst zugesagten `snaplen` überschreitet —
ein Verstoß gegen das eigene Dateiformat-Kontrakt (`incl_len`/`orig_len`
existieren im Pcap-Format gerade dafür, dass ein zu langes Paket auf
`snaplen` gekappt aufgezeichnet wird, `orig_len` aber die echte
Ursprungslänge behält, analog zu `tcpdump -s`). Es handelt sich um keinen
Speicherfehler in `PcapBuffer` selbst (der Puffer ist exakt `len` Bytes groß,
kein Overflow), sondern um eine fehlerhafte, das eigene Format verletzende
Aufzeichnung, die downstream-Werkzeuge (Wireshark/tcpdump) als ungültig
markieren bzw. bei `incl_len > snaplen` mit "invalid" abweisen/abschneiden
können.

## Nutzen
- `.pcap`-Aufzeichnungen respektieren wieder ihre eigene Snaplen-Zusage —
  jedes gecapturte Paket ist mit Standard-Tools ohne Format-Warnung
  auswertbar.
- `orig_len` bleibt bei Kappung die echte Ursprungslänge (Standard-Snaplen-
  Semantik), statt beim gekürzten Paket denselben (falschen) Wert wie
  `incl_len` zu tragen.
- Härtet die einzige gemeinsame Capture-Utility für alle künftigen
  Aufzeichnungspfade ab (aktuell USB-NCM; naheliegender Wiederverwendungs-
  punkt für eine künftige WiFi-/BLE-Paket-Aufzeichnung aus dem
  Marauder-Modul), ohne dass jeder Aufrufer selbst auf `MAX_PACKET_LEN`
  prüfen muss.

## Betroffene Dateien
- `src/Utilities/PcapBuffer.cpp` — `PcapBufferBlock`/`append()`/`flushToDisk()`.
- `src/Utilities/PcapBuffer.h` — `PcapBufferBlock` um `origLength`-Feld
  erweitert.

## Risiko
- Minimal. Der Fix ändert nur das interne Speicherverhalten für den (in der
  aktuellen USB-NCM-Nutzung seltenen bis nicht auftretenden) Fall
  `len > MAX_PACKET_LEN`; normale Pakete (`len <= MAX_PACKET_LEN`, der
  Alltagsfall) sind byteidentisch zum bisherigen Verhalten — mit Host-Repro
  verifiziert. Kein API-Bruch: `append(uint8_t*, uint32_t)`-Signatur bleibt
  unverändert, nur ein zusätzliches internes Feld in der (privaten)
  `PcapBufferBlock`-Struktur.

## Port-Skizze
```diff
--- a/src/Utilities/PcapBuffer.h
+++ b/src/Utilities/PcapBuffer.h
@@
 struct PcapBufferBlock {
   unsigned long time;
   uint8_t* buffer;
-  uint32_t length;
+  uint32_t length;     // incl_len: tatsaechlich gespeicherte (<= snaplen) Laenge
+  uint32_t origLength; // orig_len: echte, ungekappte Paketlaenge
 };
```

```diff
--- a/src/Utilities/PcapBuffer.cpp
+++ b/src/Utilities/PcapBuffer.cpp
@@ bool PcapBuffer::append(uint8_t *packet, uint32_t len)
     unsigned long curTime = micros();

-    uint8_t* buffer = (uint8_t*) malloc(len);
+    uint32_t storedLen = len > MAX_PACKET_LEN ? MAX_PACKET_LEN : len;
+    uint8_t* buffer = (uint8_t*) malloc(storedLen);

     if (!buffer)
     {
         return false;
     }

-    memcpy(buffer, packet, len);
+    memcpy(buffer, packet, storedLen);

     PcapBufferBlock block {
         .time = curTime,
         .buffer = buffer,
-        .length = len
+        .length = storedLen,
+        .origLength = len
     };

     m_blocks.push(block);
     return true;
 }
@@ void PcapBuffer::flushToDisk()
         write(m_file, seconds); // ts_sec
         write(m_file, microSeconds); // ts_usec
         write(m_file, block.length); // incl_len
-        write(m_file, block.length); // orig_len
+        write(m_file, block.origLength); // orig_len
         write(m_file, block.buffer, block.length); // packet payload
```

## Testhinweis
- Host-Repro (ohne Hardware, in `repro-pcapbuffer-snaplen-truncation/`):
  - `g++ -O2 -Wall -Wextra -o repro repro.cpp && ./repro`
  - Baut Header-Schreiblogik (`write()`-Overloads), `append()` und
    `flushToDisk()` 1:1 nach (Sink = `std::vector<uint8_t>` statt
    `fs::File`), einmal als IST (aktueller Code, kein Clamp) und einmal als
    SOLL (Fix), inklusive eines kleinen Pcap-Parsers, der die geschriebenen
    Bytes zurückliest. Für ein Paket mit `len = 5000` (> `MAX_PACKET_LEN =
    2324`): IST liefert `incl_len = orig_len = 5000 > snaplen = 2324`
    (Format-Verstoß, mit Assertion nachgewiesen); SOLL liefert
    `incl_len = 2324 <= snaplen`, `orig_len = 5000` (echte Länge erhalten).
    Für ein normal großes Paket (`len = 100`) sind IST und SOLL
    byteidentisch. Exit 0, alle Assertions bestätigt.
- Lokaler Hardware-Test (nach Einbau des Patches):
  1. PlatformIO-Build, Gerät im USB-NCM-Modus (Ethernet-Gadget) betreiben,
     `USB_NCM_PCAP_ON` (Ducky-Kommando) bzw. Setting `pcapNcmOnStart`
     aktivieren.
  2. Traffic mit bekannt großen Paketen durch das NCM-Interface schicken
     (bei Bedarf MTU/Jumbo-Frame-fähigen Host-Traffic erzwingen).
  3. Aufgezeichnete `usbncm_N.pcap`-Datei von der SD-Karte holen und mit
     `tcpdump -r usbncm_0.pcap -v` bzw. Wireshark öffnen: erwartet keine
     "packet length is invalid"/Snaplen-Warnung mehr für über-lange Pakete,
     `orig_len` in der Paketdetailansicht zeigt bei gekapptem Paket die
     echte (größere) Ursprungslänge.

## Quelle
- `G4MEOVER18/usb-army-penetrator` (Fork, Commit
  `411b153d749f66a5c0059c39a3933c61971fcb2f`),
  `src/Utilities/PcapBuffer.cpp` (Zeilen 62–68 globaler Header/`MAX_PACKET_LEN`,
  104–129 `append()`/`flushToDisk()`), `src/Utilities/PcapBuffer.h`
  (`PcapBufferBlock`), aufgerufen aus `src/Devices/USB/USBNCM.cpp`
  (`createPcap()`/`startPacketCollection()`).
