# CONSTRAINTS — verbindlich für jeden Beitrag

1. **Commit-Stil:** Commits ausschließlich als **Yanis / G4MEOVER18 <g4me.over.18@gmail.com>**. **Kein** `Co-Authored-By`-Trailer, keine Fremd-Signatur, kein zusätzlicher Autor. Vor dem ersten Commit die Git-Identität entsprechend setzen.
2. **Niemals pushen:** sensible SubGHz-Captures `B1.sub`–`B9.sub` und generell rohe eigene Capture-Dateien (`.sub`, `.nfc`, `.rfid` mit echten Kartendaten). Nur Code, Docs, Ideen, Vorschläge.
3. **Keine Fremd-Kartendaten / keine echten Zugangsdaten** in dieses Repo.
4. **Sprache/Stil in Dateien:** Alles in erster Person als Projektinhaber, nur Sachinhalt. Keine Meta-Angaben zur Entstehung oder zum Werkzeug der Arbeit in Dateien, Commits oder Reports.
5. **Sicherheitsrahmen:** Alles ist defensives/autorisiertes Pentest-Werkzeug (eigene Geräte, autorisierte Tests, CTF/Lehre). Keine Anleitungen für Massenangriffe, DoS, Detection-Evasion zu bösartigen Zwecken.
6. **Nicht destruktiv:** Bestehende Dateien werden nicht ohne klaren Grund gelöscht/überschrieben — nur ergänzen und vorschlagen. Firmware-Code-Änderungen kommen als **Vorschlag/Patch** (dieses Repo oder PR), nicht als Blind-Merge in die lokale Firmware.
7. **Basis-Fakten:** Firmware = G4MEOVER-FW v2 auf Momentum **mntm-012**, API **87.1**, Target f7 (STM32WB55). Build lokal via `fbt`/`ufbt`. Bauen/Flashen/Hardware-Test passiert lokal — hier nur prüfbare Artefakte.
