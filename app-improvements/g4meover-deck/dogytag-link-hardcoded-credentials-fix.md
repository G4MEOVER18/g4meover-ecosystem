# g4meover-deck — dogytag_link.py liest Klartext-Default-Zugangsdaten aus dem Quelltext

## Was
Ich habe den beim letzten Cross-Repo-Heartbeat bereits gefundenen, aber nur in der
Masterplan-Dokumentation vermerkten Punkt jetzt als konkreten Patch ausgearbeitet:
`deck/dogytag_link.py` liest die Zugangsdaten für den lokalen mosquitto-Broker bisher so:

```python
BROKER_USER = os.getenv("DOGYTAG_MQTT_USER", "yanis")
BROKER_PASS = os.getenv("DOGYTAG_MQTT_PASS", "12345678")
...
def _client():
    ...
    if BROKER_USER:
        c.username_pw_set(BROKER_USER, BROKER_PASS)
    return c
```

Ist keine der beiden Umgebungsvariablen gesetzt — der Normalfall, wenn das Modul
direkt (CLI-Selbsttest, Import durch `deck_daemon`/künftige Dashboard-Hooks) ohne
vorherige Konfiguration läuft —, fallen `BROKER_USER`/`BROKER_PASS` auf die
hartkodierten Platzhalter `"yanis"`/`"12345678"` zurück, und `_client()` sendet diese
unbedingt per `username_pw_set()`. Das Repo ist öffentlich (`G4MEOVER18/g4meover-deck`
auf GitHub), der Quelltext damit für jeden einsehbar.

Das steht im Widerspruch zum eigenen, an zwei anderen Stellen im selben Repo
etablierten Muster: `lorix_link.py` (`_key()`) und `ukfe_rf.py`
(`DEFAULT_SECRET`-Override) lesen ein echtes Secret ausschließlich aus der Umgebung
oder aus `secret_local.py` — einer lokalen, über `.gitignore` (`secret_local.py`,
`deck/secret_local.py`) bewusst ungetrackten Datei — und fallen ohne beides auf einen
erkennbar unechten Platzhalter zurück (`DEFAULT_SECRET` ist als "PUBLIC" markiert) bzw.
auf einen klaren Fehlerstatus (`lorix_link.detect()`: `"kein TTN-Key"`). `dogytag_link.py`
ist die einzige der drei `*_link.py`-Dateien mit einer eigenen Zugangsdaten-Variable, die
dieses Muster nicht nutzt — stattdessen ein Wortpaar, das wie ein reales, benutztes
Credential aussieht (kein offensichtlicher Platzhalter wie `DEFAULT_SECRET`s
Byte-Folge), direkt im Quelltext.

Konkrete Risiken dieser Reihenfolge:
- Jeder, der den öffentlichen Quelltext liest, erhält ein Benutzername/Passwort-Paar,
  das — falls es (wie der Variablenname `BROKER_USER` und das Passwortformat
  nahelegen) zufällig mit echten, lokal konfigurierten Zugangsdaten übereinstimmt oder
  falls der Broker nie umkonfiguriert wurde — tatsächlich funktioniert.
- Selbst wenn die echten Zugangsdaten inzwischen andere sind: ein künftiger lokaler
  Lauf ohne gesetzte Umgebungsvariablen (frischer Checkout, neuer Service-Account, ein
  vergessenes `.env`) verbindet sich stillschweigend mit `yanis`/`12345678` statt mit
  einem klaren Fehler — genau das Symptom, das `lorix_link.detect()` für den
  TTN-Key-Fall bereits bewusst vermeidet.

## Nutzen
- Entfernt das Klartext-Credential-Paar vollständig aus dem Quelltext — nichts, was im
  öffentlichen Repo sichtbar ist, sieht mehr wie ein echtes, benutztes Zugangsdatenpaar
  aus.
- Vereinheitlicht `dogytag_link.py` mit dem bereits etablierten
  Umgebungsvariable-zuerst/`secret_local.py`-danach-Muster aus `lorix_link.py` — eine
  neue `_creds()`-Funktion liest `DOGYTAG_MQTT_USER`/`DOGYTAG_MQTT_PASS` aus der Umgebung
  oder (gitignored) `MQTT_USER`/`MQTT_PASS` aus `secret_local.py`, exakt analog zu
  `lorix_link._key()`.
- Ohne jede Konfiguration verbindet sich `_client()` jetzt ohne Zugangsdaten
  (`username_pw_set()` wird gar nicht aufgerufen) statt mit einem festen Rateversuch —
  schlägt der Broker einen anonymen Verbindungsversuch ab, erscheint das als normaler,
  von `detect()`/`snapshot()` bereits abgefangener Verbindungsfehler, nicht als stiller
  Erfolg mit falschen Annahmen über die tatsächlich verwendeten Zugangsdaten.
- Rein additiv: `BROKER_HOST`/`BROKER_PORT`/`TOPIC` (keine Zugangsdaten, nur
  Netzwerk-Ziel) bleiben unverändert über Umgebungsvariablen konfigurierbar.

## Betroffene Dateien
- `deck/dogytag_link.py` — Modul-Docstring um einen Hinweis auf das
  Zugangsdaten-Muster ergänzt; `BROKER_USER`/`BROKER_PASS`-Konstanten entfernt; neue
  Funktion `_creds()`; `_client()` ruft jetzt `_creds()` statt der beiden Konstanten ab.

## Patch
Unified-Diff gegen die unveränderte `G4MEOVER18/g4meover-deck`-Basis (`main`, Commit
`b030e45`) in
[`dogytag-link-hardcoded-credentials-fix/dogytag_link_py.patch`](dogytag-link-hardcoded-credentials-fix/dogytag_link_py.patch).
Gegen zwei frische, unabhängige Zweitkopien mit `git apply --check` verifiziert, real
angewendet, beide Endstände byte-für-byte mit der Arbeitsfassung abgeglichen —
identisch.

## Testhinweis (ohne Hardware)
[`repro-dogytag-link-hardcoded-credentials-fix/repro.py`](repro-dogytag-link-hardcoded-credentials-fix/repro.py)
lädt `dogytag_link.py` (ALT und NEU, Stand vor/nach dem Patch) direkt von der Platte
und ersetzt `paho.mqtt.client` durch einen minimalen Stand-in, dessen
`username_pw_set()` seine Aufrufe nur aufzeichnet — kein echter Broker, kein
Netzwerk-Zugriff nötig:

```
$ python3 repro.py
=== ALT (vor dem Fix) ===
--- ALT ---
  S1 (kein Env/secret_local): username_pw_set() aufgerufen: True mit user='yanis'
  S2 (Env gesetzt): username_pw_set()-Aufruf: [('realuser', 'realpass')]

=== NEU (nach dem Fix) ===
--- NEU ---
  S1 (kein Env/secret_local): username_pw_set() aufgerufen: False
  S2 (Env gesetzt): username_pw_set()-Aufruf: [('realuser', 'realpass')]
  S3 (secret_local.py-Fallback): username_pw_set()-Aufruf: [('secretuser', 'secretpass')]
```

Bestätigt: ALT sendet ohne jede Konfiguration unbedingt das hartkodierte
`yanis`/`12345678`-Paar (S1) an den Broker; NEU verbindet sich im selben Fall ohne
Zugangsdaten (kein `username_pw_set()`-Aufruf). Beide Stände verwenden echte, per
Umgebungsvariable gesetzte Zugangsdaten korrekt (S2) — reine Logikänderung, kein
Verhaltensunterschied im konfigurierten Fall. NEU zusätzlich mit dem
`secret_local.py`-Fallback geprüft (S3, analog `lorix_link.py`).

Lokaler Funktionstest (mit echtem Broker): `deck/secret_local.py` mit
`MQTT_USER`/`MQTT_PASS` anlegen (oder `DOGYTAG_MQTT_USER`/`DOGYTAG_MQTT_PASS` exportieren),
`python3 deck/dogytag_link.py` ausführen — `detect()` muss wie bisher `{"ready": True,
...}` liefern. Ohne jede Konfiguration muss der Verbindungsversuch je nach
Broker-Policy entweder anonym gelingen oder mit einer Fehlermeldung in `detect()`
scheitern — in keinem Fall darf `yanis`/`12345678` mehr verschickt werden (mit
`mosquitto`-Logging/`tcpdump` auf Port 1883 prüfbar).

## Risiko
- Niedrig: betrifft ausschließlich die Zugangsdaten-Auswahl beim Aufbau des
  MQTT-Clients, keine Nachrichtenverarbeitung (`on_message`/`summarize()` unverändert).
- Verhaltensänderung nur im bisher unkonfigurierten Fall (kein Env, kein
  `secret_local.py`): dort verbindet sich `_client()` jetzt ohne Zugangsdaten statt mit
  dem festen Rateversuch. Lief der Broker bisher tatsächlich mit `yanis`/`12345678`
  (unwahrscheinlich, aber ohne Einsicht in die reale `mosquitto`-Konfiguration nicht
  ausschließbar), muss lokal einmalig `secret_local.py` angelegt oder die
  Umgebungsvariablen gesetzt werden, damit `detect()`/`snapshot()` weiterhin
  `{"ready": True}` liefern — dieselbe einmalige Migration, die `lorix_link.py`s
  TTN-Key-Muster bereits von jedem Nutzer verlangt.
- Löst nicht das in `lorix_link.py` dokumentierte Hardware-/Betriebs-Risiko einer
  tatsächlich kompromittierten Broker-Instanz — reine Quelltext-Hygiene, keine
  Härtung des Brokers selbst.

## Quelle
- `G4MEOVER18/g4meover-deck` (main, Commit `b030e45`): `deck/dogytag_link.py`
  (`BROKER_USER`/`BROKER_PASS`/`_client()`, Fund und Fix), `deck/lorix_link.py`
  (`_key()`, als Vorbild für `_creds()` übernommenes Muster), `deck/ukfe_rf.py`
  (`DEFAULT_SECRET`-Override, zweites Beispiel desselben Musters), `.gitignore`
  (`secret_local.py`/`deck/secret_local.py`).
- `G4MEOVER18/g4meover-ecosystem`: der ursprüngliche Fund entstand beim letzten
  Cross-Repo-Heartbeat (Masterplan-Commit "Cross-Repo-Vertiefung (Cloud-Heartbeat
  2026-10-14)") und wurde dort nur dokumentiert/als Risiko vermerkt, nicht als Patch
  ausgeliefert — das holt dieser Eintrag nach.
