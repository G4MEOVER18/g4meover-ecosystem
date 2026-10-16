"""Host-Repro: deck/flipper_link.py::find_flipper() ALT vs NEU.

ALT/NEU 1:1 aus dem Quelltext transkribiert. device_discovery und
serial.tools.list_ports werden pro Szenario als Fake-Module in sys.modules
eingehaengt bzw. absichtlich weggelassen (ModuleNotFoundError) -- kein echter
Flipper, kein pyserial noetig.
"""
import sys
import types

FLIPPER_VID_PID = (0x0483, 0x5740)

# ---------- ALT (Originalstand deck/flipper_link.py) ----------
def find_flipper_alt():
    import glob
    try:
        import device_discovery
        port = device_discovery.find_port("Flipper Zero")
        if port:
            return port
    except Exception:  # noqa: BLE001
        pass
    ports = sorted(glob.glob("/dev/ttyACM*"))
    return ports[0] if ports else None


# ---------- NEU (mit Patch) ----------
def find_flipper_neu():
    import glob
    try:
        import device_discovery
        port = device_discovery.find_port("Flipper Zero")
        if port:
            return port
    except Exception:  # noqa: BLE001
        pass
    try:
        import serial.tools.list_ports
        for p in serial.tools.list_ports.comports():
            if (p.vid, p.pid) == FLIPPER_VID_PID:
                return p.device
    except Exception:  # noqa: BLE001
        pass
    ports = sorted(glob.glob("/dev/ttyACM*"), key=lambda s: (len(s), s))
    return ports[0] if ports else None


# ---------- Test-Infrastruktur ----------
class FakePort:
    def __init__(self, device, vid, pid):
        self.device = device
        self.vid = vid
        self.pid = pid


def install_fake_glob(paths):
    import glob
    glob.glob = lambda pattern, _p=list(paths): list(_p) if pattern == "/dev/ttyACM*" else []


def restore_glob():
    import glob
    import importlib
    importlib.reload(glob)


def install_fake_device_discovery(raises, result):
    mod = types.ModuleType("device_discovery")
    if raises:
        def find_port(name):
            raise RuntimeError("device_discovery kaputt/nicht vendored")
    else:
        def find_port(name):
            return result
    mod.find_port = find_port
    sys.modules["device_discovery"] = mod


def install_fake_serial(ports):
    serial_mod = types.ModuleType("serial")
    tools_mod = types.ModuleType("serial.tools")
    list_ports_mod = types.ModuleType("serial.tools.list_ports")
    list_ports_mod.comports = lambda: list(ports)
    tools_mod.list_ports = list_ports_mod
    serial_mod.tools = tools_mod
    sys.modules["serial"] = serial_mod
    sys.modules["serial.tools"] = tools_mod
    sys.modules["serial.tools.list_ports"] = list_ports_mod


def clear_fakes():
    sys.modules.pop("device_discovery", None)
    sys.modules.pop("serial.tools.list_ports", None)
    sys.modules.pop("serial.tools", None)
    sys.modules.pop("serial", None)


results = []


def check(name, alt_val, neu_val, expected_alt, expected_neu):
    ok = (alt_val == expected_alt) and (neu_val == expected_neu)
    results.append(ok)
    status = "OK" if ok else "FEHLER"
    print(f"[{status}] {name}")
    print(f"       ALT={alt_val!r} (erwartet {expected_alt!r})  NEU={neu_val!r} (erwartet {expected_neu!r})")


def scenario(name, *, dd_raises, dd_result, serial_ports, glob_ports, expected_alt, expected_neu):
    clear_fakes()
    install_fake_device_discovery(dd_raises, dd_result)
    if serial_ports is not None:
        install_fake_serial(serial_ports)
    install_fake_glob(glob_ports)
    try:
        alt = find_flipper_alt()
        neu = find_flipper_neu()
    finally:
        restore_glob()
        clear_fakes()
    check(name, alt, neu, expected_alt, expected_neu)


# 1) Regression: device_discovery findet den Flipper direkt -> Fallback wird nie
#    erreicht, ALT=NEU unveraendert.
scenario(
    "1) Regression: device_discovery findet Flipper direkt",
    dd_raises=False, dd_result="/dev/ttyACM1",
    serial_ports=[FakePort("/dev/ttyACM0", 0x0451, 0x16A8), FakePort("/dev/ttyACM1", 0x0483, 0x5740)],
    glob_ports=["/dev/ttyACM0", "/dev/ttyACM1"],
    expected_alt="/dev/ttyACM1", expected_neu="/dev/ttyACM1",
)

# 2) Der Fund: device_discovery nicht vendored/kaputt, CC2531-Zigbee-Koordinator
#    (VID 0x0451, eigener ttyACM*-Eintrag in device_discovery.KNOWN) liegt
#    alphabetisch VOR dem Flipper -> ALT liefert faelschlich den Zigbee-Port, NEU
#    findet den Flipper trotzdem per direktem VID:PID-Abgleich.
scenario(
    "2) Fund: device_discovery fehlt, CC2531 vor Flipper auf ttyACM*",
    dd_raises=True, dd_result=None,
    serial_ports=[FakePort("/dev/ttyACM0", 0x0451, 0x16A8), FakePort("/dev/ttyACM1", 0x0483, 0x5740)],
    glob_ports=["/dev/ttyACM0", "/dev/ttyACM1"],
    expected_alt="/dev/ttyACM0", expected_neu="/dev/ttyACM1",
)

# 3) Fund-Variante: LilyGo T-Dongle S3 (natives USB-CDC, laut device_discovery.py
#    eigener "S3-Quirk"-Hinweis) als Stoerer statt CC2531.
scenario(
    "3) Fund-Variante: device_discovery fehlt, LilyGo T-Dongle S3 vor Flipper",
    dd_raises=True, dd_result=None,
    serial_ports=[FakePort("/dev/ttyACM0", 0x303A, 0x1001), FakePort("/dev/ttyACM1", 0x0483, 0x5740)],
    glob_ports=["/dev/ttyACM0", "/dev/ttyACM1"],
    expected_alt="/dev/ttyACM0", expected_neu="/dev/ttyACM1",
)

# 4) Grenzfall: weder device_discovery noch pyserial verfuegbar (echtes
#    ModuleNotFoundError, kein Fake installiert) -> beide fallen gleich auf den
#    blinden Glob-Fallback zurueck (dokumentierte Grenze bei <=9 Geraeten).
scenario(
    "4) Grenzfall: weder device_discovery noch pyserial verfuegbar",
    dd_raises=True, dd_result=None,
    serial_ports=None,
    glob_ports=["/dev/ttyACM0"],
    expected_alt="/dev/ttyACM0", expected_neu="/dev/ttyACM0",
)

# 5) Nebenfund: reiner Glob-Fallback (Schritt 4) bei zweistelligen Portnummern --
#    ALT sortiert lexikografisch (ttyACM10 vor ttyACM2), NEU laengenbewusst
#    (ttyACM2 vor ttyACM10).
scenario(
    "5) Nebenfund: Glob-Fallback bei zweistelligen ttyACM-Nummern",
    dd_raises=True, dd_result=None,
    serial_ports=None,
    glob_ports=["/dev/ttyACM10", "/dev/ttyACM2"],
    expected_alt="/dev/ttyACM10", expected_neu="/dev/ttyACM2",
)

# 6) Kein Geraet angeschlossen -> beide liefern None.
scenario(
    "6) Kein Geraet angeschlossen",
    dd_raises=False, dd_result=None,
    serial_ports=[],
    glob_ports=[],
    expected_alt=None, expected_neu=None,
)

# 7) Aequivalenztest: device_discovery liefert in vielen Faellen direkt das
#    richtige Ergebnis -- NEU darf das nie veraendern.
for i in range(20):
    port = f"/dev/ttyACM{i}"
    scenario(
        f"7.{i}) Aequivalenz: device_discovery liefert direkt {port}",
        dd_raises=False, dd_result=port,
        serial_ports=[],
        glob_ports=[port],
        expected_alt=port, expected_neu=port,
    )

print()
if all(results):
    print(f"ALLE CHECKS BESTANDEN ({len(results)}/{len(results)})")
else:
    print(f"FEHLER: {results.count(False)}/{len(results)} Checks fehlgeschlagen")
    sys.exit(1)
