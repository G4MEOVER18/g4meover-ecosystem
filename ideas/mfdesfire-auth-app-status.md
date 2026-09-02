# Idee-Status: MFDesfire Auth (RogueMaster-Bündel)

Backlog-Auftrag "RogueMaster-Bündel weiter durchsehen" (restlicher NFC-Werkzeug-Cluster)
fortgesetzt. Sparse-Checkout von `applications/external` gegen den echten Baum (Branch `420`,
Commit `6a8a4863`) gezogen und den kompletten NFC/RFID-App-Bestand aufgelistet (34 Ordner).
Kandidat `mfdesfire_auth` geprüft, weil er im Namen und in der Beschreibung
("Application for legacy authentication with Mifare Desfire cards") genau an das offene
Firmware-Mining-Vorhaben zu schlüsselbasierter MIFARE-Authentifizierung erinnert
(`MfPlusKey`/`MfUltralightAesKey`, BACKLOG-Punkt "MIFARE Plus / Ultralight AES") — betrifft aber
tatsächlich eine andere Kartenfamilie (DESFire DES/2K3DES statt Plus/Ultralight-C AES) und ist
damit kein Kandidat für dieses Vorhaben.

## Fund

**Quelle:** `applications/external/mfdesfire_auth`, Autor Gatorixx/NullPageGh0st, Ursprung
`Gatorixx/flipper-zero-legacy-authentication`, Version 1.0. Kompletter Quellbestand (520 Zeilen
über 7 Dateien: `mfdesfire_auth.c`, sechs `scenes/*.c`) im Volltext geprüft.

**Was die App verspricht:** Karte aus `/ext/nfc` auswählen (Dateibrowser, `.nfc`-Filter) →
8-Byte-Initial-Vector und 24-Byte-Schlüssel (Länge passt zu 2K3DES/3K3DES) per Byte-Input setzen,
persistiert in einer eigenen Settings-Datei (`mfdesfire_auth.save`, pro Kartenpfad) → Menüpunkt
"Legacy Authetication" (sic) soll damit eine DESFire-Legacy-Authentifizierung gegen die Karte
durchführen.

**Was die App tatsächlich tut:** Nichts davon. `desfire_app_scene_emulate_on_enter()`
(`scenes/scene_emulate.c`, komplette Funktion 14 Zeilen) baut ausschließlich ein `Popup` mit dem
Text "Emulating... / Please wait", startet einen 3-Sekunden-Timeout und springt danach ins Menü
zurück — keine einzige NFC-HAL-Aufrufstelle (`nfc_alloc`/`nfc_poller_*`/`nfc_listener_*`/
`furi_hal_nfc_*`) im gesamten Repo, keine DES/2K3DES-Implementierung, kein APDU-Austausch. Der
zuvor eingegebene `instance->key`/`instance->initial_vector` wird in der Emulate-Szene an keiner
Stelle gelesen — die einzigen beiden Verwendungsstellen dieser Felder sind der Byte-Input-Schreib-
und der Save/Load-Pfad selbst. Die App speichert also einen Schlüssel und einen IV in eine Datei
und zeigt danach für drei Sekunden eine Ladeanimation, ohne die Karte je anzusprechen — ein reiner
UI-Platzhalter für ein nie implementiertes Feature, kein funktionsfähiges Auth-Werkzeug.

## API-87-Status

Nicht relevant — es gibt keine NFC-API-Aufrufstelle zu bewerten (siehe Fund). Für die
Vollständigkeit: `application.fam` deklariert `targets=["f7"]`, keine exotischen
Abhängigkeiten außerhalb `gui`/`storage`/`flipper_format` — ein Rebuild gegen `mntm-012` wäre
technisch trivial, würde aber nur den beschriebenen Platzhalter liefern.

## Fazit

Katalog-Status: kein Port-Kandidat. Die App liefert keine der im Namen versprochenen
DESFire-Legacy-Authentifizierungsfunktion — ein Port würde ausschließlich eine funktionslose
Attrappe reproduzieren. Für echte MIFARE-DESFire-Interaktion decke ich bereits über Access Audit
(`firmware-mining/access-audit-app.md`, Protokoll-Erkennung inkl. DESFire) und die native
NFC-App ab; ein eigenständiges DESFire-Key-Authentifizierungswerkzeug bleibt eine offene Lücke,
für die dieser Kandidat keine Lösung ist. Das ursprünglich vermutete MIFARE-Plus/Ultralight-AES-
Vorhaben (BACKLOG) bleibt davon unberührt und weiterhin ohne identifizierte Quelle.

## Sicherheitsrahmen

Nicht relevant — App führt keine sicherheitsrelevante Funktion aus (siehe Fund). Kein
Klon-/Schreib-/Bruteforce-Verhalten vorhanden, das zu bewerten wäre.
