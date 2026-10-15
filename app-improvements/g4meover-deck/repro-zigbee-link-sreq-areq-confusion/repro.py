#!/usr/bin/env python3
"""Host-Repro: g4meover-deck/zigbee_link.py::ZigbeeLink._sreq() AREQ/SRSP-Verwechslung.

ALT/NEU 1:1 aus der echten Logik von `_sreq()` (deck/zigbee_link.py) uebernommen --
ALT ist der unveraenderte Stand vor dem Fix, NEU der gefixte Stand (Antwort-Frame
wird gegen das erwartete SRSP-CMD0/CMD1 geprueft statt den ersten SOF-Frame
blind als Antwort zu nehmen). Kein echtes CC2531/Serial noetig -- ein
Mock-"Serial"-Objekt liefert einen vorab zusammengesetzten Byte-Strom.

Hintergrund: TI Z-Stack-Koordinatoren senden jederzeit unaufgeforderte AREQ-Frames
(z.B. ZDO_STATE_CHANGE_IND, SYS_RESET_IND) -- unabhaengig von einem laufenden
SREQ/SRSP-Austausch. ALT liest nach dem Senden eines SREQ einfach den naechsten
SOF-Frame vom Draht und haelt ihn fuer die SRSP-Antwort, ohne CMD0/CMD1 zu pruefen.
"""
from __future__ import annotations
import time

SOF = 0xFE
SYS_PING = (0x21, 0x01)
SYS_VERSION = (0x21, 0x02)


def _frame(cmd0: int, cmd1: int, data: bytes = b"") -> bytes:
    body = bytes([len(data), cmd0, cmd1]) + data
    fcs = 0
    for b in body:
        fcs ^= b
    return bytes([SOF]) + body + bytes([fcs])


class MockSerial:
    """Simuliert exakt das von `_sreq()` benutzte pyserial-Teilinterface."""

    def __init__(self, incoming: bytes = b""):
        self._buf = bytearray(incoming)
        self.written: list[bytes] = []

    def write(self, data: bytes) -> None:
        self.written.append(bytes(data))

    def read(self, n: int = 1) -> bytes:
        out = bytes(self._buf[:n])
        del self._buf[:n]
        return out

    def reset_input_buffer(self) -> None:
        # Im echten Treiber wird hier Alt-Rauschen VOR dem Senden verworfen --
        # der hier vorbereitete Antwort-Strom kommt danach an, bleibt also unberuehrt.
        pass


# ---------------------------------------------------------------------------
# ALT: 1:1 der Stand vor dem Fix.
# ---------------------------------------------------------------------------
def sreq_alt(s: MockSerial, cmd, data: bytes = b"", timeout: float = 0.05):
    s.reset_input_buffer()
    s.write(_frame(cmd[0], cmd[1], data))
    end = time.time() + timeout
    while time.time() < end:
        b = s.read(1)
        if b == bytes([SOF]):
            hdr = s.read(3)
            if len(hdr) < 3:
                return None
            ln = hdr[0]
            payload = s.read(ln + 1)
            if len(payload) < ln + 1:
                return None
            return payload[:ln]
    return None


# ---------------------------------------------------------------------------
# NEU: 1:1 der gefixte Stand.
# ---------------------------------------------------------------------------
def sreq_neu(s: MockSerial, cmd, data: bytes = b"", timeout: float = 0.05):
    s.reset_input_buffer()
    s.write(_frame(cmd[0], cmd[1], data))
    expect_cmd0 = (cmd[0] & 0x1F) | 0x60
    end = time.time() + timeout
    while time.time() < end:
        b = s.read(1)
        if b == bytes([SOF]):
            hdr = s.read(3)
            if len(hdr) < 3:
                return None
            ln = hdr[0]
            payload = s.read(ln + 1)
            if len(payload) < ln + 1:
                return None
            if hdr[1] != expect_cmd0 or hdr[2] != cmd[1]:
                continue
            return payload[:ln]
    return None


def ping_cap(d: bytes | None) -> int | None:
    return (d[0] | (d[1] << 8)) if d and len(d) >= 2 else None


def check(label: str, cond: bool) -> None:
    status = "OK" if cond else "FEHLER"
    print(f"[{status}] {label}")
    if not cond:
        raise SystemExit(1)


def main() -> None:
    total = 0

    # --- Szenario 1: Regression -- keine AREQ dazwischen, ALT und NEU gleich ---
    srsp_ping = _frame(0x61, 0x01, bytes([0x79, 0x01]))  # cap = 0x0179 (live verifiziert)
    for impl, name in ((sreq_alt, "ALT"), (sreq_neu, "NEU")):
        s = MockSerial(srsp_ping)
        cap = ping_cap(impl(s, SYS_PING))
        check(f"Regression {name}: ping() liefert cap=0x0179 ohne AREQ-Stoerung", cap == 0x0179)
        total += 1

    # --- Szenario 2 (der Fund): AREQ (ZDO_STATE_CHANGE_IND-aehnlich) laeuft vor der
    #     echten SRSP ein. ALT haelt die AREQ-Nutzdaten faelschlich fuer die Antwort.
    areq = _frame(0x45, 0xC0, bytes([0x02, 0x00]))  # AREQ, Typ-Bits 0x40, Subsystem ZDO 0x05
    stream = areq + srsp_ping

    s_alt = MockSerial(stream)
    cap_alt = ping_cap(sreq_alt(s_alt, SYS_PING))
    check(
        "Fund ALT: liest die AREQ-Nutzdaten (0x0002) statt der echten SRSP (0x0179) -- "
        f"cap={'0x%04X' % cap_alt if cap_alt is not None else None}",
        cap_alt == 0x0002,
    )
    total += 1

    s_neu = MockSerial(stream)
    cap_neu = ping_cap(sreq_neu(s_neu, SYS_PING))
    check(
        "Fix NEU: verwirft die AREQ, liest weiter bis zur echten SRSP (cap=0x0179)",
        cap_neu == 0x0179,
    )
    total += 1

    # --- Szenario 3: nur AREQ, keine SRSP folgt innerhalb des Zeitbudgets.
    #     ALT liefert eine falsche "Erfolgs"-Antwort, NEU korrekt None (Timeout). ---
    s_alt2 = MockSerial(areq)
    cap_alt2 = ping_cap(sreq_alt(s_alt2, SYS_PING))
    check(
        "Fund ALT (nur AREQ, keine SRSP): liefert dennoch einen Wert statt eines Fehlers -- "
        f"cap={'0x%04X' % cap_alt2 if cap_alt2 is not None else None}",
        cap_alt2 == 0x0002,
    )
    total += 1

    s_neu2 = MockSerial(areq)
    res_neu2 = sreq_neu(s_neu2, SYS_PING)
    check("Fix NEU (nur AREQ, keine SRSP): liefert korrekt None statt einer falschen Antwort",
          res_neu2 is None)
    total += 1

    # --- Szenario 4: mehrere AREQs hintereinander vor der echten SRSP -- NEU muss
    #     alle verwerfen, nicht nur die erste. ---
    multi_stream = areq + areq + areq + srsp_ping
    s_neu3 = MockSerial(multi_stream)
    cap_neu3 = ping_cap(sreq_neu(s_neu3, SYS_PING, timeout=0.2))
    check("Fix NEU (drei AREQs vor der SRSP): findet trotzdem die echte Antwort (cap=0x0179)",
          cap_neu3 == 0x0179)
    total += 1

    # --- Szenario 5: SYS_VERSION statt SYS_PING -- erwartetes CMD0/CMD1 muss je
    #     Kommando korrekt berechnet werden, nicht nur fuer SYS_PING zufaellig passen. ---
    srsp_version = _frame(0x61, 0x02, bytes([2, 1, 3, 2, 1]))
    s_neu4 = MockSerial(areq + srsp_version)
    d = sreq_neu(s_neu4, SYS_VERSION)
    check("Fix NEU (SYS_VERSION): verwirft AREQ, liefert korrekte SRSP-Nutzdaten",
          d == bytes([2, 1, 3, 2, 1]))
    total += 1
    # NEU darf eine SRSP eines ANDEREN Kommandos (hier SYS_VERSION-Antwort waehrend
    # auf SYS_PING gewartet wird) ebenfalls nicht als Treffer werten.
    s_neu5 = MockSerial(srsp_version + srsp_ping)
    cap_neu5 = ping_cap(sreq_neu(s_neu5, SYS_PING, timeout=0.2))
    check("Fix NEU (fremde SRSP vor der eigenen): verwirft SYS_VERSION-Antwort, "
          "findet die echte SYS_PING-SRSP (cap=0x0179)", cap_neu5 == 0x0179)
    total += 1

    # --- Aequivalenztest: ohne jede Stoerung liefern ALT und NEU fuer eine Reihe
    #     echter Antwortlaengen/-werte identische Ergebnisse. ---
    for cnt in range(0, 20):
        data = bytes((i * 7) % 256 for i in range(cnt))
        frame = _frame(0x61, 0x01, data)
        d_alt = sreq_alt(MockSerial(frame), SYS_PING)
        d_neu = sreq_neu(MockSerial(frame), SYS_PING)
        check(f"Aequivalenz ohne AREQ, len={cnt}: ALT == NEU", d_alt == d_neu == data)
        total += 1

    print(f"\n{total}/{total} Einzelprüfungen bestanden -- ALLE CHECKS BESTANDEN")


if __name__ == "__main__":
    main()
