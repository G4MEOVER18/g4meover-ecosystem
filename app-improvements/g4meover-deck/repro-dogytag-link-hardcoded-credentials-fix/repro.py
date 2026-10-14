"""Host-Repro: dogytag_link._creds() / _client()-Credential-Auswahl, ALT vs. NEU.

dogytag_link_orig.py / dogytag_link_fixed.py sind 1:1-Stand vor/nach dem Patch
(dogytag_link_py.patch), unveraendert. Reine Logik -- kein paho-mqtt, kein Netzwerk,
keine Hardware notwendig.
"""
import importlib.util
import os
import sys
import types

_HERE = os.path.dirname(os.path.abspath(__file__))


def load_module(path, modname):
    spec = importlib.util.spec_from_file_location(modname, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[modname] = mod
    spec.loader.exec_module(mod)
    return mod


def fake_paho():
    """Minimaler paho.mqtt.client-Stand-in: username_pw_set() zeichnet Aufrufe auf."""
    calls = []

    class FakeClient:
        def __init__(self, *a, **kw):
            pass

        def username_pw_set(self, user, pw):
            calls.append((user, pw))

    mqtt_mod = types.ModuleType("paho.mqtt.client")
    mqtt_mod.Client = FakeClient
    mqtt_mod.CallbackAPIVersion = types.SimpleNamespace(VERSION1=1)
    paho_mod = types.ModuleType("paho.mqtt")
    paho_mod.client = mqtt_mod
    paho_pkg = types.ModuleType("paho")
    paho_pkg.mqtt = paho_mod
    sys.modules["paho"] = paho_pkg
    sys.modules["paho.mqtt"] = paho_mod
    sys.modules["paho.mqtt.client"] = mqtt_mod
    return calls


def clear_env():
    for k in ("DOGYTAG_MQTT_USER", "DOGYTAG_MQTT_PASS"):
        os.environ.pop(k, None)


def run_case(path, modname, label):
    print(f"--- {label} ---")
    calls = fake_paho()

    # Szenario 1: kein Env, kein secret_local.py -> keine Zugangsdaten im Quelltext?
    clear_env()
    sys.path_importer_cache.clear()
    mod = load_module(path, modname + "_s1")
    c = mod._client()
    print(f"  S1 (kein Env/secret_local): username_pw_set() aufgerufen: {bool(calls)}"
          + (f" mit user={calls[0][0]!r}" if calls else ""))

    # Szenario 2: Env setzt echte Zugangsdaten -> muss verwendet werden
    calls.clear()
    os.environ["DOGYTAG_MQTT_USER"] = "realuser"
    os.environ["DOGYTAG_MQTT_PASS"] = "realpass"
    mod2 = load_module(path, modname + "_s2")
    c2 = mod2._client()
    print(f"  S2 (Env gesetzt): username_pw_set()-Aufruf: {calls}")
    clear_env()

    # Szenario 3 (nur NEU relevant): secret_local.py-Fallback wie bei lorix_link.py
    if hasattr(mod, "_creds"):
        calls.clear()
        fake_secret_local = types.ModuleType("secret_local")
        fake_secret_local.MQTT_USER = "secretuser"
        fake_secret_local.MQTT_PASS = "secretpass"
        sys.modules["secret_local"] = fake_secret_local
        mod3 = load_module(path, modname + "_s3")
        c3 = mod3._client()
        print(f"  S3 (secret_local.py-Fallback): username_pw_set()-Aufruf: {calls}")
        del sys.modules["secret_local"]


print("=== ALT (vor dem Fix) ===")
run_case(os.path.join(_HERE, "dogytag_link_orig.py"), "dogytag_old", "ALT")

print()
print("=== NEU (nach dem Fix) ===")
run_case(os.path.join(_HERE, "dogytag_link_fixed.py"), "dogytag_new", "NEU")
