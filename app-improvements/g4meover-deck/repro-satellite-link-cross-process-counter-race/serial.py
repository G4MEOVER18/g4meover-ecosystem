"""Minimaler Platzhalter fuer pyserial, nur fuer dieses Host-Repro.

satellite_link.py importiert `serial` nur fuer `serial.Serial(port, baud, timeout=...)`
mit `.write()`/`.flush()`/`.close()`. Dieses Modul bildet exakt diese Oberflaeche nach,
ohne echte Hardware/einen echten UART zu brauchen. Liegt im selben Verzeichnis wie
satellite_link_orig.py/_fixed.py und wird nur gefunden, wenn dieses Verzeichnis vor
einem echten pyserial auf sys.path steht (siehe repro.py).
"""
from __future__ import annotations


class SerialException(Exception):
    pass


class Crash(Exception):
    """Simuliert einen Prozessabbruch (Kill/Stromausfall) exakt an dieser Stelle."""


class Serial:
    def __init__(self, port: str, baud: int, timeout: float = 0.5):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.writes: list[bytes] = []
        # Vom Test gesetzt: wird in write() aufgerufen, BEVOR ein optionaler Crash greift —
        # bildet nach, dass der Byte-Strom bereits beim Satelliten ankommt, auch wenn der
        # sendende Prozess direkt danach stirbt (die Bytes liegen schon im UART-FIFO/auf der
        # Leitung, das ist unabhaengig vom weiteren Schicksal des Host-Prozesses).
        self.on_write = None
        self.crash_after_write = False

    def write(self, data: bytes) -> None:
        self.writes.append(bytes(data))
        if self.on_write is not None:
            self.on_write(bytes(data))
        if self.crash_after_write:
            raise Crash("Prozess stirbt direkt nach dem UART-Write")

    def flush(self) -> None:
        pass

    def close(self) -> None:
        pass
