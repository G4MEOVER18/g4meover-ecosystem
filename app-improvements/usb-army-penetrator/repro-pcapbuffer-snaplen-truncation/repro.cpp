// Host-Repro (kein Hardware-/Arduino-Code): modelliert PcapBuffer::append()/
// flushToDisk() aus src/Utilities/PcapBuffer.cpp 1:1 (Byte-Layout des globalen
// Headers und der Record-Header identisch nachgebaut, fs::File durch einen
// std::vector<uint8_t>-Sink ersetzt), einmal als IST (kein Snaplen-Clamp,
// aktueller Code) und einmal als SOLL (Fix: Payload auf MAX_PACKET_LEN
// gekappt, orig_len bleibt die echte Ursprungslänge). Reiner Host-Test,
// g++ -O2 -Wall -Wextra -o repro repro.cpp && ./repro
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <queue>
#include <vector>

#define MAX_SIZE 15
#define MAX_PACKET_LEN 2324u

using Sink = std::vector<uint8_t>;

static void writeBuf(Sink& file, const uint8_t* buf, uint32_t len) {
    file.insert(file.end(), buf, buf + len);
}
static void writeU16(Sink& file, uint16_t n) {
    uint8_t buf[2] = { (uint8_t)n, (uint8_t)(n >> 8) };
    writeBuf(file, buf, 2);
}
static void writeI32(Sink& file, int32_t n) {
    uint8_t buf[4] = { (uint8_t)n, (uint8_t)(n >> 8), (uint8_t)(n >> 16), (uint8_t)(n >> 24) };
    writeBuf(file, buf, 4);
}
static void writeU32(Sink& file, uint32_t n) {
    uint8_t buf[4] = { (uint8_t)n, (uint8_t)(n >> 8), (uint8_t)(n >> 16), (uint8_t)(n >> 24) };
    writeBuf(file, buf, 4);
}

// ---- IST: Nachbau des aktuellen PcapBuffer (kein Snaplen-Clamp in append()) ----
namespace Ist {

struct Block {
    unsigned long time;
    uint8_t* buffer;
    uint32_t length; // == angefragte len, ungeprueft
};

class PcapBuffer {
public:
    PcapBuffer(Sink& sink, uint32_t dataLinkType = 1) : m_file(sink) {
        writeU32(m_file, 0xa1b2c3d4);
        writeU16(m_file, 2);
        writeU16(m_file, 4);
        writeI32(m_file, 0);
        writeU32(m_file, 0);
        writeU32(m_file, MAX_PACKET_LEN); // snaplen
        writeU32(m_file, dataLinkType);
    }
    ~PcapBuffer() {
        while (!m_blocks.empty()) { free(m_blocks.front().buffer); m_blocks.pop(); }
    }
    bool append(const uint8_t* packet, uint32_t len) {
        if (m_blocks.size() > MAX_SIZE) return false;
        uint8_t* buffer = (uint8_t*)malloc(len); // <-- kein Clamp auf MAX_PACKET_LEN
        if (!buffer) return false;
        memcpy(buffer, packet, len);
        m_blocks.push(Block{ 0, buffer, len });
        return true;
    }
    void flushToDisk() {
        while (!m_blocks.empty()) {
            const auto& block = m_blocks.front();
            writeU32(m_file, 0);             // ts_sec
            writeU32(m_file, 0);             // ts_usec
            writeU32(m_file, block.length);  // incl_len
            writeU32(m_file, block.length);  // orig_len (identisch zu incl_len)
            writeBuf(m_file, block.buffer, block.length);
            free(block.buffer);
            m_blocks.pop();
        }
    }
private:
    Sink& m_file;
    std::queue<Block> m_blocks;
};

} // namespace Ist

// ---- SOLL: Fix - Payload wird beim Capture auf MAX_PACKET_LEN gekappt ----
// (Standard-Snaplen-Semantik wie bei tcpdump -s: incl_len <= snaplen IMMER,
// orig_len traegt weiterhin die echte, ungekappte Laenge.)
namespace Soll {

struct Block {
    unsigned long time;
    uint8_t* buffer;
    uint32_t length;     // incl_len: tatsaechlich gespeicherte (<= snaplen) Laenge
    uint32_t origLength; // orig_len: echte, ungekappte Paketlaenge
};

class PcapBuffer {
public:
    PcapBuffer(Sink& sink, uint32_t dataLinkType = 1) : m_file(sink) {
        writeU32(m_file, 0xa1b2c3d4);
        writeU16(m_file, 2);
        writeU16(m_file, 4);
        writeI32(m_file, 0);
        writeU32(m_file, 0);
        writeU32(m_file, MAX_PACKET_LEN); // snaplen
        writeU32(m_file, dataLinkType);
    }
    ~PcapBuffer() {
        while (!m_blocks.empty()) { free(m_blocks.front().buffer); m_blocks.pop(); }
    }
    bool append(const uint8_t* packet, uint32_t len) {
        if (m_blocks.size() > MAX_SIZE) return false;
        uint32_t storedLen = len > MAX_PACKET_LEN ? MAX_PACKET_LEN : len; // <-- Clamp
        uint8_t* buffer = (uint8_t*)malloc(storedLen);
        if (!buffer) return false;
        memcpy(buffer, packet, storedLen);
        m_blocks.push(Block{ 0, buffer, storedLen, len });
        return true;
    }
    void flushToDisk() {
        while (!m_blocks.empty()) {
            const auto& block = m_blocks.front();
            writeU32(m_file, 0);                 // ts_sec
            writeU32(m_file, 0);                 // ts_usec
            writeU32(m_file, block.length);      // incl_len (<= snaplen)
            writeU32(m_file, block.origLength);  // orig_len (echte Laenge)
            writeBuf(m_file, block.buffer, block.length);
            free(block.buffer);
            m_blocks.pop();
        }
    }
private:
    Sink& m_file;
    std::queue<Block> m_blocks;
};

} // namespace Soll

// ---- Minimaler Pcap-Reader zur Verifikation der geschriebenen Bytes ----
struct ParsedGlobalHeader { uint32_t snaplen; };
struct ParsedRecord { uint32_t inclLen; uint32_t origLen; };

static uint32_t readU32(const Sink& s, size_t off) {
    return (uint32_t)s[off] | ((uint32_t)s[off+1] << 8) | ((uint32_t)s[off+2] << 16) | ((uint32_t)s[off+3] << 24);
}

static ParsedGlobalHeader parseGlobalHeader(const Sink& s) {
    assert(s.size() >= 24);
    assert(readU32(s, 0) == 0xa1b2c3d4);
    // Layout: magic(4) major(2) minor(2) thiszone(4) sigfigs(4) snaplen(4) linktype(4)
    return ParsedGlobalHeader{ readU32(s, 16) };
}

static std::vector<ParsedRecord> parseRecords(const Sink& s, size_t startOff) {
    std::vector<ParsedRecord> out;
    size_t off = startOff;
    while (off + 16 <= s.size()) {
        uint32_t inclLen = readU32(s, off + 8);
        uint32_t origLen = readU32(s, off + 12);
        out.push_back(ParsedRecord{ inclLen, origLen });
        off += 16 + inclLen; // Reader ueberspringt exakt incl_len Payload-Bytes
    }
    assert(off == s.size()); // kein Rest / kein Ueberlauf beim Parsen
    return out;
}

int main() {
    const uint32_t normalLen = 100;                 // < MAX_PACKET_LEN, unveraendertes Verhalten
    const uint32_t oversizedLen = 5000;              // > MAX_PACKET_LEN(2324)
    std::vector<uint8_t> normalPacket(normalLen, 0xAB);
    std::vector<uint8_t> oversizedPacket(oversizedLen, 0xCD);

    // --- IST: normal-grosses Paket verhaelt sich wie erwartet ---
    {
        Sink sink;
        Ist::PcapBuffer buf(sink);
        assert(buf.append(normalPacket.data(), normalLen));
        buf.flushToDisk();
        auto gh = parseGlobalHeader(sink);
        auto recs = parseRecords(sink, 24);
        assert(recs.size() == 1);
        assert(recs[0].inclLen == normalLen && recs[0].origLen == normalLen);
        assert(recs[0].inclLen <= gh.snaplen);
    }

    // --- IST: uebergrosses Paket verletzt die eigene Snaplen-Zusage ---
    {
        Sink sink;
        Ist::PcapBuffer buf(sink);
        assert(buf.append(oversizedPacket.data(), oversizedLen));
        buf.flushToDisk();
        auto gh = parseGlobalHeader(sink);
        assert(gh.snaplen == MAX_PACKET_LEN);
        auto recs = parseRecords(sink, 24);
        assert(recs.size() == 1);
        // BUG: incl_len (== orig_len) ist groesser als der im Header verspro-
        // chene snaplen -- das .pcap verletzt sein eigenes Format-Kontrakt.
        assert(recs[0].inclLen == oversizedLen);
        assert(recs[0].origLen == oversizedLen);
        assert(recs[0].inclLen > gh.snaplen);
        printf("IST : oversized capture -> incl_len=%u > snaplen=%u (Format-Verstoss)\n",
               recs[0].inclLen, gh.snaplen);
    }

    // --- SOLL: normal-grosses Paket unveraendert ---
    {
        Sink sink;
        Soll::PcapBuffer buf(sink);
        assert(buf.append(normalPacket.data(), normalLen));
        buf.flushToDisk();
        auto gh = parseGlobalHeader(sink);
        auto recs = parseRecords(sink, 24);
        assert(recs.size() == 1);
        assert(recs[0].inclLen == normalLen && recs[0].origLen == normalLen);
        assert(recs[0].inclLen <= gh.snaplen);
    }

    // --- SOLL: uebergrosses Paket wird auf snaplen gekappt, orig_len bleibt echt ---
    {
        Sink sink;
        Soll::PcapBuffer buf(sink);
        assert(buf.append(oversizedPacket.data(), oversizedLen));
        buf.flushToDisk();
        auto gh = parseGlobalHeader(sink);
        auto recs = parseRecords(sink, 24);
        assert(recs.size() == 1);
        assert(recs[0].inclLen == MAX_PACKET_LEN);   // gekappt, respektiert snaplen
        assert(recs[0].origLen == oversizedLen);     // echte Laenge bleibt erhalten
        assert(recs[0].inclLen <= gh.snaplen);
        printf("SOLL: oversized capture -> incl_len=%u <= snaplen=%u, orig_len=%u erhalten\n",
               recs[0].inclLen, gh.snaplen, recs[0].origLen);
    }

    printf("Alle Assertions bestanden.\n");
    return 0;
}
