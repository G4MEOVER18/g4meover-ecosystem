# Idee-Status: SubGHz-Bruteforcer-App ("Subbrute / BruteForcer")

Katalog-Eintrag "Subbrute / BruteForcer" (`ideas/pentest-app-katalog.md`, bislang ❓) geprüft.
Auslöser war eine Verwechslungsgefahr mit meinem eigenen Konzeptdokument
`ideas/subghz-fixed-code-bruteforce-konzept.md`: dort geht es um ein host-seitiges
Python-Skript, das `.sub`-RAW-Dateien für die Stock-Sub-GHz-App erzeugt. Der Katalog-Eintrag
meint aber eine eigenständige On-Device-**App**, die ich bisher nicht konkret identifiziert
hatte.

## Fund

**Quelle:** `DarkFlippers/flipperzero-subbrute` (App-ID `subghz_bruteforcer`, Maintainer u. a.
`xMasterX`, derselbe Maintainer wie beim bereits geprüften `ibutton-fuzzer`), zuletzt am
2026-07-21 aktiv (fbt-Formatierung, davor `fsk12k support` am 2026-01-05) — aktiv gepflegt.

**Wichtigstes Ergebnis: Die App ist bereits Teil meiner Basis-Firmware, kein Port nötig.**
Momentum bindet externe Apps über das Submodul `applications/external` →
`Next-Flip/Momentum-Apps` ein. Der bei Tag `mntm-012` gepinnte Submodul-Commit
(`85b0cdb`) enthält bereits das Verzeichnis `subghz_bruteforcer/` mit identischer
`application.fam` (App-ID, Entry-Point, `requires=["gui","dialogs"]` identisch zum
Upstream-Repo) — verifiziert per direktem Abruf von
`raw.githubusercontent.com/Next-Flip/Momentum-Apps/85b0cdb/subghz_bruteforcer/application.fam`
(HTTP 200). Die App ist unter Sub-GHz → Bruteforcer bereits auf meinem Gerät vorhanden,
sobald ich G4MEOVER-FW v2 (Basis mntm-012) geflasht habe — keine SD-Karten-Installation,
kein eigener Build nötig.

## Was die App tut

On-Device-Menü: Protokoll + Frequenz auswählen (Standard: CAME 12bit 433 MHz, laut
Upstream-README das verbreitetste), Wiederholungszahl pro Code per Links/Rechts einstellbar
(mehr Wiederholungen = zuverlässiger bei störrischen Empfängern, aber langsamer). Deckt laut
`subbrute_protocols.c` (an der `mntm-012`-gepinnten Version geprüft) folgende Fixed-Code-Familien
ab: CAME (12 Bit, 6 Frequenzvarianten 303–868 MHz), NICE (12 Bit, 433/868 MHz), Ansonic
(12 Bit, 3 Frequenzvarianten), Chamberlain (7/8/9 Bit, je mehrere Frequenzvarianten), Linear
(10 Bit + Delta-8-Bit-Variante), Unilarm (24 Bit), SMC5326 (24 Bit), PT2260 (24 Bit,
4 Frequenzvarianten), PT2262 (24 Bit, 5 Frequenzvarianten), Holtek (12 Bit inkl. AM-Varianten).
Zusätzlich ein `load_file`-Eintrag für eigene/benutzerdefinierte Protokoll-Timings.

## Verhältnis zu meinem eigenen Bruteforce-Konzept

Ergänzt statt ersetzt `ideas/subghz-fixed-code-bruteforce-konzept.md`: Das eigene Konzept
(host-seitiges Skript, Binärsuch-Aufteilung des Coderaums in verschachtelte `.sub`-Dateien) bleibt
sinnvoll für Protokolle/Bitlängen, die die App nicht direkt unterstützt (z. B. PT-2240,
individuell rekonstruierte Timings), sowie für den Fall, dass ich den vollen Coderaum ohne
manuelle Menübedienung durcharbeiten will. Für alle in der Liste oben abgedeckten Standard-
Protokolle ersetzt die bereits vorhandene App den Umweg über Host-Skript + Datei-Kopieren
vollständig — sie iteriert den Coderaum on-device selbst durch, kein manuelles Datei-für-Datei-
Senden über die Stock-Sub-GHz-App nötig.

## API-87-Status

✅ Bereits im Firmware-Baum enthalten (siehe oben, Submodul-Commit-Verifikation gegen `mntm-012`),
kein Kompatibilitätsrisiko zu prüfen — die App wird zusammen mit dem Rest von
`applications/external` gegen dieselbe API 87.1 gebaut wie meine übrige Firmware. Einzige
offene Frage aus dem Upstream-Changelog (ein Issue erwähnt "Preload failed API version mismatch"
bei einem alten Release-Build) betrifft nur veraltete, separat heruntergeladene `.fap`-Releases —
entfällt hier, da die App fest im Firmware-Image mitgebaut wird statt als eigenständiges `.fap`
nachinstalliert zu werden.

## Sicherheitsrahmen

Ausschließlich gegen eigene, für Tests vorgesehene Fixed-Code-Empfänger (Torantriebe,
Garagentore, Schranken) einsetzbar — kein Einsatz gegen fremde oder nicht ausdrücklich
freigegebene Anlagen. Laufzeiten bei größeren Bitlängen (24 Bit: Unilarm/SMC5326/PT2260/PT2262)
können lang werden; dabei lokale Regularien zu Sendedauer/Duty-Cycle auf 433/868 MHz beachten.
Kein Bezug zu Massenangriffen oder Detection-Evasion.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

Nach dem nächsten Flash von G4MEOVER-FW v2 unter Sub-GHz → Bruteforcer prüfen, dass die App
erscheint und startet; an einem eigenen Fixed-Code-Torantrieb (bekanntes Protokoll, z. B. per
Read-RAW vorab identifiziert) einen Testlauf mit niedriger Wiederholungszahl durchführen und mit
dem eigenen Host-Skript-Ansatz aus `ideas/subghz-fixed-code-bruteforce-konzept.md` hinsichtlich
Laufzeit vergleichen.
