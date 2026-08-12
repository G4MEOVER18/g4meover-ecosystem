# SubGHz-Konzept: Fixed-Code-Bruteforce (kein neues Firmware-Feature)

Backlog-Punkt „Nützliche `.sub`-/NFC-/RFID-Konzepte (keine echten Captures) + eigene App-Ideen"
aufgegriffen. Meine bisherigen SubGHz-Werkzeuge (RollJam, RollForge, der Telcoma/Cardin-Port-
Vorschlag) decken ausschließlich **Rolling-Code**-Schwächen ab (Capture+Replay, Jam+Capture). Fixed-
Code-Systeme (feste, unveränderliche Sendercodes ohne Rolling-Mechanismus) sind eine eigene, bislang
bei mir unabgedeckte Geräteklasse mit einer ganz anderen Angriffsfläche: erschöpfendes Durchprobieren
des gesamten Coderaums.

## Konzept

**Quellen:** github.com/tobiabocchi/flipperzero-bruteforce (README, MIT-artig/offen, Python-
Generator), github.com/UberGuidoZ/Flipper (Sub-GHz/Garages/deBruijn, kuratierte Community-Sammlung),
gist.github.com/jinschoi (De-Bruijn-.sub-Beispiel für NSCD-Garagentore) — reine Konzept-/Tooling-
Recherche, keine der Quellen liefert echte fremde Codes, die ich übernehmen würde.

Fixed-Code-Sender (u. a. CAME, Nice, Chamberlain, Linear, PT-2240, Ansonic, Holtek) senden bei jedem
Tastendruck denselben, festen Bitcode ohne Rolling-Mechanismus. Wer den gesamten Coderaum eines
solchen Senders systematisch durchprobiert, findet zwangsläufig den gültigen Code — die einzige Frage
ist die Laufzeit. Der naheliegende Ansatz ist, alle 2^n Codes einzeln zu senden; De-Bruijn-Sequenzen
versprechen dafür theoretisch eine Kompression (überlappende Substrings decken alle Kombinationen mit
deutlich weniger Gesamtbits ab, klassisches Cracking-Konzept aus Zahlenschloss-/PIN-Bruteforce-
Kontexten).

**Wichtige Einschränkung aus der Recherche:** Das o. g. Tool merkt in seiner eigenen Dokumentation
ausdrücklich an, dass reine De-Bruijn-Sequenzen für die meisten realen Fixed-Code-Protokolle in der
Praxis kaum nutzbar sind, weil Stop-/Pilot-Bits und Protokoll-Framing (Präambel, feste Bitlänge pro
Übertragung) das nahtlose Überlappen verhindern, das die Kompression erst bringt. Der praktisch
funktionierende Ansatz dort ist stattdessen ein binäres Suchverfahren: Der Coderaum wird in
verschachtelte, zunehmend kleinere Verzeichnisse/Dateien aufgeteilt (z. B. erst obere/untere Hälfte
des Coderaums als große Sammel-Datei senden, je nachdem welche das Garagentor öffnet die passende
Hälfte weiter unterteilen, bis eine einzelne kleine Datei mit dem Code übrig bleibt) — dadurch sinkt
die Zahl der nötigen Einzelübertragungen von 2^n auf ungefähr n Schritte plus die Größe der jeweils
kleinsten Dateien.

**Laufzeiten laut Quelle (Referenzwerte, keine Zusicherung):** stark protokoll-/bitlängen-abhängig —
von rund 123 Sekunden (Chamberlain, 9 Bit) bis zu mehreren hundert Stunden (PT-2240, 24 Bit) für den
vollständigen Coderaum am Stück; das Binärsuchverfahren verkürzt die Zeit bis zum ersten Treffer
gegenüber sequenziellem Durchprobieren erheblich, ohne dass der volle Coderaum je gesendet werden
muss.

## Nutzen für mich

Deckt eine Geräteklasse ab, die RollJam/RollForge/Telcoma-Cardin bewusst nicht abdecken (die sind auf
Rolling Code ausgelegt) — an einem eigenen Fixed-Code-Torantrieb/Garagentor kann ich damit prüfen, ob
er in vertretbarer Zeit durchprobierbar ist, bevor ich ihn produktiv nutze oder empfehle. Kein neues
Firmware-Feature nötig: `.sub`-RAW-Dateien lassen sich mit der bereits vorhandenen Stock-App
Sub-GHz → Gespeichert → Senden abspielen: reines Datei-Erzeugungsproblem (Host-Skript), keine
Firmware-Änderung.

## Aufwand

Klein. Kein Firmware-Patch, kein neuer App-Port. Ein Host-seitiges Python-Skript (analog zum
referenzierten Generator) erzeugt `.sub`-RAW-Dateien für ein konkretes Zielprotokoll/Bitlänge, ich
kopiere sie auf die SD-Karte und spiele sie über die bestehende Sub-GHz-App ab. Für eigene
Zielgeräte müsste ich zusätzlich Trägerfrequenz, Bitlänge und Timing-Parameter (Pulsbreite,
Pause-Verhältnis) meines konkreten Fixed-Code-Senders bestimmen (z. B. per Read-RAW-Mitschnitt eines
eigenen Tastendrucks) — das ist der eigentliche Rechercheaufwand pro Zielgerät, nicht das
Datei-Generieren selbst.

## API-87-Status

Entfällt — keine Firmware-Änderung. Nutzt ausschließlich die bereits vorhandene, unveränderte
Sub-GHz-Sende-Funktion (`.sub`-RAW-Wiedergabe) aus mntm-012, kein API-Kontakt nötig. Ein möglicher
späterer Schritt (Binärsuch-Ablauf direkt on-device automatisieren, statt Dateien einzeln manuell
auszuwählen) wäre ein eigenes App-/RollForge-Modul und bräuchte dann einen echten Port-Vorschlag mit
API-Prüfung — hier bewusst nicht vorweggenommen, da noch ungeprüft, ob der manuelle Datei-für-Datei-
Ablauf über die Stock-App für meinen Bedarf schon ausreicht.

## Sicherheitsrahmen

Ausschließlich gegen eigene, für Tests vorgesehene Fixed-Code-Geräte einsetzbar — kein Einsatz gegen
fremde oder nicht ausdrücklich für Tests freigegebene Anlagen. Codes werden lokal für ein konkretes
Zielprotokoll/Bitlänge generiert, keine Sammlung/Weitergabe echter fremder Codes. Laufzeiten bei
größeren Bitlängen (16+ Bit) können in den Stunden-/Tage-Bereich gehen — dabei die lokal geltenden
Regularien für Sendedauer/Duty-Cycle auf 433/868 MHz beachten, nicht unbeaufsichtigt über die erlaubte
Sendezeit hinaus senden. Kein Bezug zu Massenangriffen oder Detection-Evasion — reiner
Erschöpfungstest auf physisch zugänglichen eigenen Geräten.

## Nächster Schritt (lokal, nicht Teil dieses Repos)

Für ein konkretes eigenes Fixed-Code-Zielgerät (Trägerfrequenz, Protokoll, Bitlänge) per Read-RAW
eigenen Tastendruck aufzeichnen, daraus Timing-Parameter ableiten, Bruteforce-Dateien lokal erzeugen
und über die Stock-Sub-GHz-App testen. Bei Bedarf danach als eigenes RollForge-Modul (Binärsuch-Ablauf
on-device statt manueller Dateiauswahl) zum vollen Port-Vorschlag ausbauen.
