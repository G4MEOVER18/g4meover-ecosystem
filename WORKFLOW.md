# WORKFLOW — Arbeitsweise

Arbeitskontext: **G4MEOVER Flipper-Zero-Pentest-Ökosystem**. Basis-Firmware: Momentum **mntm-012**, API **87.1**, Target f7. Vor jeder Sitzung `CONSTRAINTS.md` (verbindlich) und `BACKLOG.md` lesen.

Gebaut/geflasht/an Hardware getestet wird **lokal**. In diesem Repo entstehen prüfbare Artefakte: Recherche, Diffs, Port-Vorschläge, Code-Patches, kuratierte Ideen.

## Ablauf pro Sitzung (inkrementell, klein, sauber)
1. `reports/` ansehen: was wurde zuletzt gemacht? Nicht wiederholen.
2. **Einen** sinnvollen Fortschritt aus dem Backlog wählen (ein kleiner, abgeschlossener Beitrag statt vieler halber).
3. Arbeiten:
   - **Firmware-Feature-Mining:** Ein konkretes Feature/Protokoll aus Unleashed (unlshd-090+), RogueMaster oder Official-dev nehmen, gegen mntm-012 prüfen, als Port-Vorschlag in `firmware-mining/<feature>.md` dokumentieren (Was, Nutzen, betroffene Dateien, Risiko, Port-Skizze). Wenn klein & sicher: Patch beilegen.
   - **App-Verbesserung:** Für eine eigene App (z. B. g4meover-magspoof) eine konkrete Verbesserung als Patch in `app-improvements/<app>/` mit Begründung. Reine Logik, ohne Hardware verifizierbar (Encode/Parse/State).
   - **Ideen-Recherche:** Nach nützlichen Apps, `.sub`/NFC/RFID-Ideen, Tools suchen; kuratiert in `ideas/` ergänzen (Quelle, Nutzen, Aufwand, Sicherheitsrahmen). Keine fremden Kartendaten/Captures.
4. Kurzes Arbeits-Log in `reports/YYYY-MM-DD-HHMM.md` (was geprüft, was geliefert, offene Punkte).
5. Committen (siehe CONSTRAINTS) und pushen. Bei größeren Firmware-Änderungen: PR statt Direkt-Push.

## Leitlinie
Qualität vor Menge. Jeder Beitrag muss lokal geprüft, gebaut und geflasht werden können — das leicht machen (klare Dateiliste, Begründung, Testhinweis).
