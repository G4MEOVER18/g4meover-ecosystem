# BACKLOG — priorisiert

Stand: initial. Wird laufend abgehakt und ergänzt; lokal wird gemerged/gebaut/geflasht.

## Firmware-Feature-Mining
- [x] Unleashed **unlshd-090** vs. mntm-012: **Telcoma/Cardin EDGE**-SubGHz-Protokoll fehlt mir —
      Port-Vorschlag in `firmware-mining/telcoma-cardin-edge-subghz.md`.
- [ ] Unleashed **unlshd-090+** vs. mntm-012: weitere neue/erweiterte **SubGHz-Protokolle** prüfen
      (unlshd-090 selbst brachte darüber hinaus primär Stabilitätsfixes, kein zweites neues
      Protokoll — bei künftigen unlshd-Releases erneut gegenprüfen).
- [ ] RogueMaster App-Bündel durchsehen: welche nützlichen Apps fehlen uns, sind API-87-kompatibel?
- [ ] Official-dev seit mntm-012 (2025-12-31): Bugfixes/Sicherheits-/Stabilitäts-Commits, die wir übernehmen sollten.
- [ ] NFC/RFID-Tooling-Deltas (Dictionaries, Parser, Magic-Support).

## App-Verbesserungen (reine Logik, ohne HW testbar)
- [ ] **g4meover-magspoof**: Card-String→MagDevice-Parser (aus astro-cyberpaws-Fork übernehmen), Track-3-Encode. Siehe dortige ROADMAP.md.
- [ ] Eigene Apps (RollForge/ProtoPirate/RollJam/RollLab/FlipperJammer/lora-ukfe): Encode/Parse/State-Refactors, Tests.

## Ideen-Recherche
- [ ] Kuratierte Liste starker Pentest-Apps (SubGHz/NFC/RFID/BadUSB/2.4G) mit API-87-Status.
- [ ] Nützliche `.sub`-/NFC-/RFID-**Konzepte** (keine echten Captures) + eigene App-Ideen.
- [ ] Companion-Tool-Ideen: PC-Helfer für Flipper-Apps, danach Raspberry-Pi-Deck.

## Kompatibilität (Kontext, lokal umgesetzt)
- CLI-Kompat-Shim (`cli_session_open/close`, `cli_vcp`) für Alt-Apps + cli_bridge — Firmware-seitig, lokal.
- 49 Alt-Apps laufen bereits unter `api_mismatch_autoload` (bewiesen per Symbol-Analyse).
