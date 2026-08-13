# NFC-Konzept: Mifare-Classic-Nested-Attack-Automatisierung (kein neues Firmware-Feature)

Backlog-Punkt „NFC-/RFID-seitige Konzepte (analog, z. B. Mifare-Classic-Nested-Attack-
Automatisierung)" aus der Ideen-Recherche aufgegriffen — in einer früheren Sitzung schon
recherchiert, hier erstmals kuratiert. Meine bisherigen NFC-Zugangskontroll-Funde
(`access-audit-app.md`, `wiegand-reader-app.md`) decken die physische/Wiegand-Seite und das
Risikoscoring von Zutrittsmedien ab; für Mifare-Classic-Karten selbst nutze ich bislang nur die
im Stock-NFC-Flow vorhandenen Mittel (Dictionary-Angriff mit Standard-/gefundenen Schlüsseln,
sowie MFKey32 bei Reader-Interaktion). Beide decken nicht den Fall ab, bei dem ich nur **einen**
Sektorschlüssel einer eigenen Karte kenne (z. B. aus dem Dictionary-Treffer auf Sektor 0) und
die übrigen Sektoren ohne erneute Reader-Interaktion prüfen will.

## Konzept

**Quellen:** github.com/AloneLiberty/FlipperNested (Haupt-App, GPL-3.0, 380 Sterne, aktive
`dev`-Branch), github.com/AloneLiberty/FlipperNested/wiki (FAQ, Usage-Guide),
github.com/AloneLiberty/FlipperNestedRecovery (PC-seitiges Begleit-Skript für die eigentliche
Schlüsselberechnung), github.com/noproto/FlipperMfkey (MFKey32-Referenz, offizielle
App-Hub-App von Flipper Devices) — reine Konzept-/Tooling-Recherche, keine der Quellen liefert
echte fremde Kartendaten.

Mifare Classic sichert seine Sektoren mit dem gebrochenen CRYPTO1-Stromchiffre. Drei bei mir
bislang ungenutzte Angriffsvarianten aus der Proxmark3-Iceman-Fork sind über FlipperNested auf
den Flipper portiert:

- **Nested Attack:** Klassischer Angriff, nutzt einen *bereits bekannten* Schlüssel eines
  Sektors, um über verschachtelte (nested) Authentifizierungsversuche mit vorhersagbaren
  Nonces die Schlüssel weiterer Sektoren zu berechnen — deutlich schneller als erneutes
  Dictionary-/Reader-Cracking pro Sektor.
- **Static Nested Attack:** Variante für Karten mit statischem (nicht zufälligem) Nonce —
  noch schneller, aber nur bei entsprechend schwachen/älteren Chargen anwendbar.
- **Hard Nested Attack:** Fallback für Karten mit besserer (aber weiterhin CRYPTO1-basierter)
  Nonce-Verteilung, rechenintensiver.

**Wichtige Einschränkung aus der Recherche:** Alle drei Varianten benötigen laut FAQ
zwingend mindestens einen bereits bekannten Schlüssel als Startpunkt („Scan tag and find at
least one key to start"); ohne vorhandenen Schlüssel muss zuerst ein Dictionary- oder
Darkside-/Reader-Angriff laufen — Letzterer ist laut FAQ „already implemented in Flipper
firmware" (Stock-Funktionalität, kein zusätzlicher Port nötig). Die eigentliche
Schlüsselberechnung aus den auf dem Gerät gesammelten Nonces läuft nicht on-device, sondern
über das separate PC-Skript `FlipperNestedRecovery` — zweistufiger Ablauf wie bei MFKey32
(Gerät sammelt Rohdaten, PC rechnet), nur mit anderem Algorithmus/Angriffsziel: MFKey32 braucht
eine *echte Reader-Interaktion* (Karte wird gegen einen legitimen Reader emuliert, Nonces aus
dessen Auth-Versuchen abgegriffen), während Nested/Static-Nested/Hard-Nested ausschließlich
*card-only* arbeiten — kein legitimer Reader nötig, nur die eigene Karte und ein bereits
bekannter Schlüssel.

## Nutzen für mich

Schließt eine Lücke zwischen den beiden bereits vorhandenen NFC-Zugangs-Funden
(Access-Audit-Risikoscoring, Wiegand-Reader-Physical-Audit): für eine eigene Mifare-Classic-
Zugangskarte, bei der der Stock-Dictionary-Angriff nur den Standard-/Transportschlüssel eines
einzelnen Sektors findet, kann ich mit dem gefundenen Schlüssel als Startpunkt alle übrigen
Sektoren ohne erneute physische Reader-Interaktion prüfen — praxisrelevant für den
Vollständigkeits-Check, ob eine eigene Karte konsistent starke, nicht wiederverwendete
Sektorschlüssel verwendet, statt nur den einen per Dictionary gefundenen schwachen Schlüssel zu
kennen.

## Aufwand

Mittel. Kein Firmware-Patch nötig (Stock-Dictionary-/Darkside-Angriff liefert den
Startschlüssel bereits ohne Zusatz-App). Für die Nested-Attack-Kette selbst: FlipperNested als
externe `.fap`-App installieren (App-Hub oder `ufbt build`/`ufbt launch` gegen eine geklonte
Kopie des Repos — kein Firmware-Fork-Merge, reine Zusatz-App analog zu ProtoPirate/RollJam/
RollForge/g4meover-magspoof) plus das PC-seitige `FlipperNestedRecovery`-Skript für die
Schlüsselberechnung aus den gesammelten Nonces. Laut Entwickler „early development" mit
möglichen Abstürzen/Hängern — vor Produktiveinsatz an einer eigenen Testkarte statt einer
tatsächlich genutzten Zugangskarte ausprobieren.

## API-87-Status

Ungeprüft — FlipperNested dokumentiert im README/FAQ keine explizite API-Versionsangabe oder
Firmware-Kompatibilitätsliste. Da es sich um eine reine Zusatz-App (`.fap`, kein Firmware-Fork)
handelt, ist die relevante Prüfung nicht API-87-Kompatibilität des gesamten Repos, sondern ob
`ufbt build` gegen meinen mntm-012-SDK-Stand (API 87.1) fehlerfrei durchläuft — das ist lokal
zu verifizieren, bevor die App auf dem Gerät installiert wird. Der Entwickler nennt zudem einen
offenen technischen Punkt: PRNG-Sprünge auf dem Flipper-NFC-HAL verhalten sich anders als auf
dem Proxmark3, was App-eigene Timing-Workarounds nötig macht — ein möglicher Quell für
API-Versions-abhängige Instabilität, die beim lokalen Build-/Testlauf mitzuprüfen ist.

## Sicherheitsrahmen

Ausschließlich gegen eigene Mifare-Classic-Karten/Testkarten einsetzbar, deren
Sektorschlüssel bereits per Dictionary-/Darkside-Angriff (Stock-Firmware) teilweise bekannt
sind — kein Einsatz gegen fremde oder nicht ausdrücklich für Tests freigegebene Karten. Card-
only-Charakter (keine Reader-Emulation gegen ein fremdes Lesegerät nötig) reduziert die
Angriffsfläche gegenüber MFKey32 zusätzlich auf die eigene Karte als einziges Ziel. Ergebnisse
(gefundene Schlüssel) verbleiben lokal auf Gerät/PC, keine Sammlung oder Weitergabe echter
Kartendaten in dieses Repo. Kein Bezug zu Massenangriffen oder Detection-Evasion — reiner
Vollständigkeits-Check der eigenen Kartensicherheit.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

FlipperNested per `ufbt` gegen den lokalen mntm-012-SDK-Stand bauen und auf einer eigenen
Testkarte mit bereits per Dictionary-Angriff bekanntem Sektor-0-Schlüssel ausprobieren; bei
stabilem Lauf `FlipperNestedRecovery` lokal einrichten und die Nonce→Schlüssel-Berechnung für
die restlichen Sektoren gegenprüfen. Bei Bedarf danach als eigener, ausführlicher Port-Vorschlag
in `firmware-mining/` (falls dauerhaft in die eigene App-Sammlung integriert statt separat
gebaut) weiterverfolgen.
