# Recherche-Auftrag abgearbeitet: PicoPass/Seader API-87-Status

Aus `pentest-app-katalog.md`-Zeile "Access Audit" sowie mehreren Firmware-Mining- und
Ideen-Sitzungen als offener Punkt mitgeschleppt: "PicoPass/Seader (ASN.1/mbedTLS-Aufwand)" stand
seit der ersten RogueMaster-CAN-Bus-Sitzung als "größerer Portaufwand" zurückgestellt, ohne dass
Quelle, Pflegestand oder tatsächlicher Aufwand je konkret geprüft wurden. Diese Sitzung holt das
nach.

## Kernfund: beide Apps liegen bereits in meinem eigenen Firmware-Baum

`applications/external` in `Next-Flip/Momentum-Firmware` ist ein Submodul auf
`Next-Flip/Momentum-Apps.git`, beim Tag `mntm-012` gepinnt auf Commit `85b0cdb4e7dec…` — exakt
derselbe Commit, den ich bereits in der Subbrute-Sitzung für den `subghz_bruteforcer`-Fund
verifiziert habe (`ideas/subghz-bruteforcer-app-status.md`). Genau in diesem Commit existieren
**beide** Ordner bereits vollständig:

- `picopass/` — App-ID `picopass`, `fap_category="NFC"`, Version 1.22, Autor Eric Betts
  (`bettse`), Zielplattform laut `application.fam` explizit `FlipperAppId=picopass` /
  `targets=["f7"]`.
- `seader/` — eigenständige App (App-ID `seader`), ebenfalls von `bettse`, mit eigenem
  `readme.md`/`LICENSE` (GPLv3)/`Makefile`.

Damit ist der bisher unterstellte "größere Portaufwand wegen ASN.1/mbedTLS-Abhängigkeiten" **kein
Portvorhaben mehr**, sondern ein bereits im eigenen `mntm-012`-Firmware-Baum vorhandener
App-Baustein — dieselbe Situation wie zuvor bei `subghz_bruteforcer`. Beleg zusätzlich über den
`mntm-012`-Changelog selbst: der Release-Eintrag nennt unter "Updated" wörtlich "Picopass: Removed
wiegand plugin, support for changing CN during emulation for some formats (by @bettse)" — bettse
pflegt PicoPass aktiv direkt im Momentum-Kontext, nicht nur im eigenen Repo.

## Was die beiden Apps tun

**PicoPass** (`picopass/`, Kategorie NFC): Lesen/Schreiben/Emulieren von HID-iClass/PicoPass-
Zutrittskarten über die eingebaute NFC-Antenne des Flipper — **keine zusätzliche Hardware**
nötig. Bringt die `loclass`-Portierung aus dem Proxmark3-Iceman-Fork mit (Elite-/Custom-Key-
Berechnung aus dem Leserverhalten heraus, `picopass_elite_keygen.c`) sowie einen eigenen
`asn1`/`sio.asn1`-Parser für Secure Identity Objects (SIO) und `mbedtls` für die DES3-
Krypto-Operationen — beides als **private, im App-Ordner mitgelieferte Libraries** gebaut
(`asn1`, `loclass` laut `application.fam` mit eigenen `fap_private_libs`-Einträgen), nicht als
zusätzliche Firmware-Abhängigkeit, die ich selbst einpflegen müsste. `mbedtls` selbst ist ohnehin
bereits fester Firmware-Bestandteil (eigenes Submodul `lib/mbedtls` in `Momentum-Firmware`, siehe
`.gitmodules`).

**Seader** (`seader/`): liest zusätzlich HID-iClass-**SE**, Desfire EV1/EV2 und Seos-Credentials
— Formate, bei denen die Daten (SIO) verschlüsselt sind und ohne einen echten HID-SAM (Security
Access Module) nicht entschlüsselt werden können. Anders als PicoPass **braucht Seader
zusätzliche Fremd-Hardware**: ein HID-SAM plus UART-/Mini-SIM-Adapter zwischen Flipper und SAM
(kommerzielle Optionen laut eigenem Readme: NARD-Addon, SAMAdams, Flippermeister, oder ein
"Smart Card 2 Click"-Board mit separat zu beschaffendem HID-SAM plus Eigenverkabelung) — dieselbe
Kategorie wie der bereits als "Fremdgerät, größerer separater Prüfpunkt" zurückgestellte
`can_commander`-Fund. Ohne SAM bleibt Seader auf das Auslesen der unverschlüsselten Teile
beschränkt.

## Nutzen für mich

Schließt eine Lücke, die ich bereits bei der `access_audit`-Portierung offen gelassen hatte
(`firmware-mining/access-audit-app.md`: Risikoscoring für Zutrittskontroll-Medien) — Access Audit
bewertet Medien, liest aber selbst keine iClass-Credentials aus; PicoPass ist der fehlende
Lese-/Analyse-Baustein für genau diese Kartenklasse (Physical-Access-Audit-Kategorie, die ich seit
dem Wiegand-Reader-Fund gezielt ausbaue). Für autorisierte Zutrittskontroll-Audits (eigene
Bürokarten, im Rahmen genehmigter Tests) deckt PicoPass allein bereits den Großteil ab, ohne
zusätzliche Hardware. Seader ist ein sinnvoller Ausbauschritt nur bei Bedarf an iClass-SE/Seos
(höherwertige HID-Systeme) — dafür ist die SAM-Anschaffung eine bewusste, separate
Investitionsentscheidung, kein reiner Softwareschritt.

## API-87-Status: ✅ (PicoPass), ✅ mit Zusatzhardware (Seader)

- Beide Apps liegen im exakten `Next-Flip/Momentum-Apps`-Commit, den `mntm-012` selbst als
  Submodul pinnt — kein Kompatibilitäts-Ratespiel wie bei den RogueMaster-Funden, sondern
  derselbe Firmware-Stand, den ich lokal baue.
- `bettse` ist laut `mntm-012`-Changelog aktiv an der Momentum-spezifischen Pflege beteiligt
  (Wiegand-Plugin-Entfernung, CN-Emulation-Änderungen) — kein verwaistes Drittprojekt.
- `picopass`-Repo (`github.com/bettse/picopass`) zeigt Stand 18.06.2026 (rund zwei Monate vor
  dieser Sitzung) als letzte Aktualisierung — aktiv gepflegt, deutlich über die reine
  `mntm-012`-Pin-Historie hinaus.
- Einzige Einschränkung: keine eigene Bauprobe in dieser Sitzung (Bauen/Flashen ist laut
  `WORKFLOW.md` lokale Aufgabe) — da die App aber bereits Teil des eigenen gepinnten
  Firmware-Submoduls ist, ist das Risiko eines API-Bruchs deutlich geringer als bei separat zu
  bauenden RogueMaster-Fremdordnern.

## Aufwand

**PicoPass:** keiner über das übliche Firmware-Build hinaus — die App liegt bereits im
`applications/external`-Submodul meines `mntm-012`-Checkouts; je nach lokalem Build-Setup reicht
`./fbt` (falls das Standard-App-Set sie einschließt) oder gezielt
`./fbt fap_picopass`/`--extra-int-apps=picopass`, danach Flash und Test gegen eine eigene
iClass-/PicoPass-Karte. Kein Firmware-Quellcode-Patch nötig, keine externe Library selbst
einzupflegen.

**Seader:** Softwareseitig identisch simpel (`./fbt fap_seader`), zusätzlich aber Anschaffung/Bau
eines HID-SAM-Adapters (Kostenpunkt und Lötaufwand je nach gewählter Option) — bewusst
zurückgestellt, bis konkreter Bedarf an iClass-SE/Seos-Audits besteht.

## Sicherheitsrahmen

Beide Apps bleiben im bestehenden Rahmen: ausschließlich Lesen/Analysieren eigener oder im Rahmen
autorisierter Tests bereitgestellter Zutrittskontroll-Medien, keine echten Fremd-Kartendaten in
diesem Repo, keine Anleitung zur unautorisierten Zutrittsumgehung. Die in PicoPass enthaltene
`loclass`-Elite-Key-Berechnung ist ein bekannter, seit Jahren öffentlich dokumentierter
Proxmark3-Iceman-Baustein zur Schwachstellenanalyse des HID-iClass-Systems (Reader-seitige
Schlüsselableitung) — Einsatz ausschließlich gegen eigene/autorisierte Lesegeräte.

## Katalog-Korrektur

`ideas/pentest-app-katalog.md`, Zeile "Access Audit": Status-Spalte ergänzt um Verweis auf diesen
PicoPass/Seader-Fund als medienseitige Ergänzung; neue eigene Zeile für PicoPass in der
NFC/RFID-Tabelle ergänzt.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

- `./fbt fap_picopass` (oder Standard-Build, falls bereits eingeschlossen) gegen den eigenen
  `mntm-012`-Checkout, Flash, Test gegen eine eigene iClass-/PicoPass-Karte (Lesen reicht für
  ersten Funktionsnachweis).
- Bei Bedarf an iClass-SE/Seos: HID-SAM-Hardwareoption auswählen (NARD/SAMAdams/Flippermeister/
  Smart-Card-2-Click), danach `seader`-Build gegen dieselbe Karte testen.
- Ergebnis in `ideas/pentest-app-katalog.md` von "🔧 größerer Portaufwand" endgültig auf den
  hier dokumentierten ✅-Status nachziehen (bereits in dieser Sitzung vorgenommen).
