# g4meover-companion / magtool — `build` schreibt ungeprüft, auch wenn das Ergebnis die eigenen Validierungsregeln verletzt

## Was

`magtool/magtool.py` ist mein PC-Begleittool zur G4MEOVER-MagSpoof-App: `build`
erzeugt aus PAN/Name/Expiry/Service-Code eine `.mag`-Datei, `validate` prüft
eine vorhandene `.mag`-Datei strukturell (Start-/Endsentinel, Zeichensatz,
Feldtrenner-Anzahl, Maximallänge) gegen `validate_track1()`/`validate_track2()`.

`cmd_build()` validiert vor dem Schreiben ausschließlich die drei reinen
Eingabefelder PAN/Expiry/Service-Code per Regex:

```python
if not re.fullmatch(r"\d{12,19}", args.pan): ...
if not re.fullmatch(r"\d{4}", args.expiry): ...
if not re.fullmatch(r"\d{3}", args.service): ...
t1 = build_track1(args.pan, args.name, args.expiry, args.service)
t2 = build_track2(args.pan, args.expiry, args.service)
t3 = args.track3 or ""
content = (...)
with open(args.out, "w", newline="\n") as f:
    f.write(content)
```

`args.name` und `args.track3` laufen ungeprüft in `build_track1()`/den
Track-3-String ein. `build_track1()` fügt den Namen unverändert zwischen zwei
`^`-Feldtrennern ein (`f"%B{pan}^{name}^{expiry}{service}?"`); ich rufe die im
selben File bereits vorhandenen `validate_track1()`/`validate_track2()` an
dieser Stelle nie auf. Enthält `--name` selbst ein `^` (z. B. ein Name, den
man versehentlich schon im Track-1-Rohformat statt als Klartext übergibt),
entsteht ein **dritter** Feldtrenner — Track 1 hat danach die laut ISO 7811-2
falsche Feldstruktur, und `build` schreibt die Datei trotzdem anstandslos mit
Exit-Code 0 und "Geschrieben: ...". Derselbe ungeprüfte Weg gilt für jedes
Zeichen außerhalb von `T1_CHARSET` (auch `name.upper()` rettet z. B. Umlaute
nicht: `"ü".upper() == "Ü"`, weiterhin kein ISO-7811-Alphanumerikzeichen) und
für `--track3`, das 1:1 ungeprüft in Track 3 übernommen wird.

Der Fehler fällt nur auf, wenn man die geschriebene Datei zusätzlich noch von
Hand mit `magtool.py validate` prüft — `build` ruft `validate` nicht
automatisch auf, obwohl exakt die dafür nötigen Funktionen schon im selben
Modul stehen.

## Nutzen des Fixes

`build` und `validate` desselben Tools widersprechen sich sonst: `build`
meldet Erfolg für eine Datei, die `validate` im selben Atemzug als `FEHLER`
zurückweist. Praktisch betrifft das jeden Namen mit einem versehentlichen
`^`/Sonderzeichen (Copy-Paste aus einer bereits codierten Track-1-Zeile,
ein Name mit einem im ISO-7811-Alphabet nicht vorhandenen Zeichen) sowie
jeden händisch zusammengebauten `--track3`-Rohstring. Der Fix lässt `build`
seine eigenen Validierungsfunktionen vor dem Schreiben anwenden und bricht
mit einer klaren Fehlermeldung ab, statt eine Datei zu erzeugen, die das Tool
selbst als ungültig einstuft. Für jede Eingabe, die bereits heute eine gültige
`.mag`-Datei ergibt, ändert sich nichts — Track-1/Track-2/Track-3-Aufbau
(`build_track1()`/`build_track2()`) bleibt unverändert, nur der
Schreibvorgang wird jetzt davon abhängig gemacht, dass das Ergebnis besteht.

## Betroffene Dateien

- `magtool/magtool.py` — nur `cmd_build()`, zwischen Track-Aufbau und
  Datei-Schreiben; `validate_track1()`/`validate_track2()` werden
  unverändert wiederverwendet, nicht dupliziert.

## Patch

`connective-patches/magtool_py.patch` (unified diff gegen `magtool/magtool.py`,
Commit `ceb068bf278feae4a8180d24330111e0b911301d` von
`G4MEOVER18/g4meover-companion`, `main`). Per `git apply --check`/real sowie
`patch -p1 --dry-run`/real gegen zwei frische, unabhängige Zweitkopien
angewendet — beide Ergebnisse laut `md5sum` byte-identisch zueinander und zur
beigelegten Zieldatei (`magtool_py.target`, `f671c6e142f08f59d4000f4a7cc17af1`)
bestätigt. `python3 -m py_compile` lief auf allen drei Kopien ohne Fehler.

Einzige Änderung (Ausschnitt aus `cmd_build()`):

```python
    t1 = build_track1(args.pan, args.name, args.expiry, args.service)
    t2 = build_track2(args.pan, args.expiry, args.service)
    t3 = args.track3 or ""
    # Eigene Validierung vor dem Schreiben anwenden (sonst erzeugt z.B. ein '^' in
    # --name klaglos einen dritten Feldtrenner in Track 1 -- siehe Fund unten).
    errs = {1: validate_track1(t1), 2: validate_track2(t2)}
    if t3:
        errs[3] = validate_track2(t3, 107)
    bad = {n: es for n, es in errs.items() if es}
    if bad:
        for n, es in sorted(bad.items()):
            for e in es:
                print(f"Track {n}: {e}", file=sys.stderr)
        sys.exit(
            "Erzeugte Tracks sind ungueltig (z.B. '^' in --name oder nicht "
            "erlaubte Zeichen) -- Datei wurde NICHT geschrieben."
        )
    content = (
```

## Host-Repro

`repro-build-no-validation/repro.py`: importiert ausschließlich die
unveränderten Bausteine `build_track1()`/`build_track2()`/`validate_track1()`/
`validate_track2()` aus der echten, unmodifizierten `magtool.py` (per
`MAGTOOL_PY`-Umgebungsvariable überschreibbar, Default zeigt auf den lokalen
`g4meover-companion`-Checkout) — kein `argparse`-/Hardwarezugriff nötig, da es
ausschließlich um den Kontrollfluss von `cmd_build()` selbst geht.
`alt_cmd_build()`/`neu_cmd_build()` transkribieren 1:1 den fraglichen
Ausschnitt vor/nach dem Patch.

Fünf Fälle, `python3 repro.py`:
1. Normaler Name ohne Sonderzeichen → ALT und NEU schreiben beide
   (Regression: unveränderter Erfolgsfall).
2. **Fund:** Caret im `--name` → ALT schreibt anstandslos (dritter
   Feldtrenner in Track 1, von `validate` als Fehler erkannt), NEU verweigert
   das Schreiben.
3. **Fund:** Umlaut im Namen (bleibt nach `.upper()` außerhalb von
   `T1_CHARSET`) → ALT schreibt, NEU verweigert.
4. **Fund:** ungültiger `--track3`-Rohstring (falsche Sentinels, falscher
   Zeichensatz, falsche Feldtrenner-Anzahl) → ALT schreibt, NEU verweigert.
5. Regression: gültiger, nicht-leerer `--track3` bleibt weiterhin erlaubt →
   ALT und NEU schreiben beide.

Zusätzlich prüft jeder Fall, dass `build_track1()`/`build_track2()` selbst
zwischen ALT und NEU identische Tracks liefern (der Fix ändert nur, ob
geschrieben wird, nicht wie die Tracks aufgebaut werden).

Ergebnis: **5/5 FÄLLE BESTANDEN** — ALT reproduziert den Fund in allen drei
Fehlerfällen, NEU verweigert dort das Schreiben, ohne die beiden
Regressionsfälle zu verändern.

## Testhinweis (lokal)

Patch gegen die eigene `magtool.py`-Kopie anwenden. Dann:
`magtool.py build --pan 4111111111111111 --expiry 2512 --name "SMITH^JOHN" --out test.mag`
ausführen. Vor dem Fix: Datei wird geschrieben, Exit-Code 0, obwohl
`magtool.py validate test.mag` anschließend "FEHLER" für Track 1 meldet. Nach
dem Fix: `build` bricht sofort mit der neuen Fehlermeldung und Exit-Code 1 ab,
keine Datei entsteht. Danach Regressionsprobe mit einem normalen Namen ohne
Sonderzeichen (z. B. `--name "SMITH/JOHN"`) wiederholen — Verhalten
unverändert erfolgreich, `validate` meldet weiterhin "OK" für alle drei
Tracks. Reine PC-Seite, kein Flipper/Hardware nötig — betrifft nicht die
MagSpoof-App selbst, nur die Vorbereitung der `.mag`-Datei auf dem PC.

## Risiko

Sehr gering. Die Änderung betrifft ausschließlich den Fall, dass die bereits
im selben Modul vorhandene Validierung einen Fehler findet — ein Fall, der im
Status quo bereits eine laut `validate` fehlerhafte Datei erzeugt hätte, es
gibt also keinen bestehenden Erfolgspfad, der sich ändert (kein `--name`/
`--track3`, das bisher zu einer laut `validate` gültigen Datei führte, wird
neu abgelehnt — siehe Regressionsfälle 1 und 5 im Host-Repro). Kein Einfluss
auf `build_track1()`/`build_track2()`/`validate_track1()`/`validate_track2()`
selbst oder auf `cmd_validate()`/`cmd_info()`.

## Quelle

`G4MEOVER18/g4meover-companion`, `main`, Commit
`ceb068bf278feae4a8180d24330111e0b911301d`
(`magtool/magtool.py::cmd_build()`).
