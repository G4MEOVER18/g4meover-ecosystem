# MIFARE Ultralight: `mf_ultralight_is_pwd_pack_read()`-Helfer

## Was

Beim letzten UL-AES-Phase-2-Port (`firmware-mining/mfultralight-aes-phase2-struct-serialization.md`)
hatte ich in Unleasheds `mf_ultralight.h`/`.c`-Diff vier eigenständige, von der AES-Frage
unabhängige Funde notiert und bewusst zurückgestellt. Diese Sitzung greift den kleinsten,
klarsten davon auf: einen neuen, reinen Helfer,

```c
bool mf_ultralight_is_pwd_pack_read(const MfUltralightData* data);
```

Er meldet, ob die *tatsächlichen* Werte der PWD-/PACK-Seiten (die letzten beiden Seiten jedes
passwortfähigen Kartentyps) erfasst wurden — im Unterschied zu den maskierten Nullseiten, die
mein eigener Poller nach einem fehlgeschlagenen Default-Passwort-Versuch zurücklässt.

Hintergrund: `mf_ultralight_poller_handler_try_default_pass()` (`mf_ultralight_poller.c`,
bereits in meiner Basis vorhanden, unverändert von diesem Patch) liest bei einer vollständig
gelesenen Karte (`pages_read == pages_total`) ohne erfolgreiche Authentifizierung die letzten
zwei Seiten zurück:

```c
} else if(!instance->auth_context.auth_success) {
    instance->pages_read -= 2;
    instance->data->pages_read -= 2;
}
```

Diese zwei Seiten wurden zwar physisch von der Karte gelesen, enthalten aber nur die vom Chip
gelieferte, für nicht-authentifizierte Leser maskierte Nullantwort — kein reales PWD/PACK. Der
Poller korrigiert das, indem er sie aus `pages_read` wieder herausrechnet. Der neue Helfer
macht genau diese Unterscheidung testbar, ohne dafür den Poller oder Live-Kartenkontakt zu
brauchen:

```c
bool mf_ultralight_is_pwd_pack_read(const MfUltralightData* data) {
    furi_check(data);
    uint8_t pwd_page = mf_ultralight_get_pwd_page_num(data->type);
    return (pwd_page != 0) && (data->pages_read >= pwd_page + 2);
}
```

`mf_ultralight_get_pwd_page_num()` existiert in meiner Basis bereits unverändert (liefert
`config_page + 2`, bzw. `0` für Typen ohne Konfigurationsblock) — dieser Patch braucht also
keine weitere Infrastruktur.

## Nutzen

- Schließt einen der vier zuletzt zurückgestellten UL-Funde ab, unabhängig von den restlichen
  drei (Typ-Erkennungs-Umbau, ATQA-Lockerung, Schreibbereichs-Anpassung — bleiben eigene
  künftige Kandidaten).
- Bietet eine robustere Alternative zur bestehenden Heuristik in meiner eigenen
  `mf_ultralight_is_all_data_read()`: deren `PasswordAuth`-Zweig prüft aktuell
  `(pass != 0) || (pack != 0)` auf den *Inhalt* der Konfigurationsseite, um zu erkennen, ob ein
  Default-PWD-Versuch fehlgeschlagen ist. Das ist eine funktionierende, aber inhaltsbasierte
  Annahme (setzt voraus, dass ein echtes Passwort/PACK nie exakt `0` ist) — für den seltenen,
  aber möglichen Fall eines Kartenbesitzers, der bewusst `PWD = 0x00000000`/`PACK = 0x0000`
  gesetzt hat, würde `is_all_data_read()` fälschlich "nicht vollständig gelesen" melden, obwohl
  der reale Wert korrekt erfasst wurde. `is_pwd_pack_read()` prüft stattdessen nur die
  persistierte Seitenzahl (`pages_read`) und ist damit unabhängig vom tatsächlichen
  Passwortinhalt — ich habe das mit einem eigenen, unten dokumentierten Host-Testfall
  nachgewiesen (Gruppe 4). Ich liefere in dieser Sitzung bewusst nur den neuen Helfer selbst,
  nicht das Umstellen von `is_all_data_read()` darauf, um diesen Schritt klein und in sich
  abgeschlossen zu halten (siehe "Nicht Teil dieses Patches").
- `pages_read` wird beim Laden eines Dumps mitgeladen (`MF_ULTRALIGHT_PAGES_READ_KEY`), der
  Helfer funktioniert also identisch für einen frisch gelesenen wie für einen bereits
  gespeicherten `.nfc`-Dump, ohne dass dafür ein Poller/Auth-Kontext existieren muss — rein
  aus dem Speicher-/Dateiformat ableitbar und ohne Hardware nachrechenbar.
- Schließt keine Sicherheitslücke, ist aber eine kleine, saubere Grundlage für eine spätere
  UI-Anzeige ("PWD/PACK erfasst: ja/nein", z. B. beim Anzeigen oder Exportieren eines Dumps),
  falls ich das künftig aufgreife.

## Betroffene Dateien

**Jetzt geliefert:**
- `lib/nfc/protocols/mf_ultralight/mf_ultralight.h` — eine neue Funktionsdeklaration
  (`mf_ultralight_is_pwd_pack_read`), direkt nach der bestehenden
  `mf_ultralight_is_all_data_read()`-Deklaration eingefügt (identische Position wie bei
  Unleashed).
- `lib/nfc/protocols/mf_ultralight/mf_ultralight.c` — eine neue Funktion, direkt nach
  `mf_ultralight_is_all_data_read()` eingefügt, greift ausschließlich auf bereits vorhandene
  Felder (`data->type`, `data->pages_read`) und die bereits vorhandene
  `mf_ultralight_get_pwd_page_num()` zu.
- Als Unified-Diffs gegen meine unveränderte `main`-Basis:
  `mf-ultralight-is-pwd-pack-read-helper/mf_ultralight.h.patch`,
  `mf-ultralight-is-pwd-pack-read-helper/mf_ultralight.c.patch` — `patch --dry-run -p1` vorab
  gegen frische Kopien beider Dateien verifiziert, danach real angewendet und Byte-für-Byte
  gegen die host-getestete Fassung verglichen (identisch).

**Nicht Teil dieses Patches:**
- Kein Aufruf des neuen Helfers irgendwo eingebaut — er ist reine, unbenutzte (aber vollständig
  getestete) Infrastruktur, genau wie Unleashed ihn zum Zeitpunkt dieses Diffs selbst noch nicht
  irgendwo aufruft (kein Treffer in `mf_ultralight_poller.c` oder den mir per Rohdatei
  zugänglichen NFC-Scenes/Plugins dieses Standes).
- Das oben beschriebene Umstellen von `mf_ultralight_is_all_data_read()` auf diesen Helfer —
  eigenständige Verhaltensänderung an einer bestehenden, bereits benutzten Funktion, verdient
  eine eigene Sitzung mit eigenem Regressionsfokus statt als Nebeneffekt dieses Patches.
- Die drei übrigen, beim letzten UL-AES-Phase-2-Port zurückgestellten Funde (Typ-Erkennungs-
  Umbau für NTAG210/212, gelockerte `detect_protocol()`-ATQA-Prüfung,
  `mf_ultralight_get_write_end_page()`-Anpassung für den UL-AES-Schreibbereich) — weiterhin
  eigene künftige Kandidaten.

## Verifikation (auf dem Host, ohne Hardware)

`firmware-mining/repro-mf-ultralight-is-pwd-pack-read/repro.c` kompiliert die tatsächlich
gepatchten, unveränderten `mf_ultralight.c`/`.h` (einzige Host-Anpassung: dieselben beiden
Include-Stand-ins wie beim letzten UL-AES-Port, siehe Kommentar am Dateianfang von
`mf_ultralight.h` — der reale Patch behält die echten Include-Pfade, siehe `.patch`-Dateien)
gegen dieselben, bereits etablierten `FlipperFormat`/`FuriString`/`Iso14443_3aData`/
`mbedtls_des3`-Ersatzheader aus dem letzten UL-AES-Port.

Build: `gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined -I stubs repro.c
mf_ultralight.c -o repro && ./repro` → `ALL CHECKS PASSED`, exit code 0.

Vier Testgruppen:
1. **Kein PWD-Seite** (`MfUltralightTypeOrigin`, `config_page == 0`) — meldet für
   `pages_read == 0` UND für den Grenzfall einer vollständig gelesenen Karte dieses Typs
   (`pages_read == pages_total`) durchgehend `false`, da dieser Typ gar kein Passwort-Feature
   hat.
2. **Vier Seitenzahl-Zustände über zwei unabhängige passwortfähige Typen** (`UL11`, 20 Seiten,
   `pwd_page = 18`; `NTAG213`, 45 Seiten, `pwd_page = 43` — unterschiedliche Geometrie, um einen
   an einen einzelnen `pages_total`-Wert gebundenen Off-by-one auszuschließen):
   `pages_read == 0` (vor jedem Leseversuch) → `false`; `pages_read == pwd_page` (exakt der
   Zustand nach dem oben zitierten `try_default_pass()`-Rollback) → `false`; `pages_read ==
   pwd_page + 1` (PACK noch nicht erreicht) → `false`; `pages_read == pages_total` (PWD und PACK
   tatsächlich erfasst) → `true`.
3. **Geladener Dump** — `pages_read` wird nicht live vom Poller gesetzt, sondern wie beim Laden
   einer gespeicherten Datei direkt zugewiesen; bestätigt, dass der Helfer ausschließlich vom
   persistierten Feld abhängt, ohne jeden Auth-Kontext.
4. Eigene Gegenprobe zur oben beschriebenen "Nutzen"-Aussage (Robustheit gegenüber einem echten
   Passwort von `0`): direkt gegen die vorhandene `mf_ultralight_is_all_data_read()` verglichen,
   mit einer Konfigurationsseite, deren PWD/PACK-Inhalt auf `0` gesetzt ist, bei sonst
   vollständig gelesener Karte — `is_all_data_read()` liefert dort (durch die bestehende, von
   diesem Patch unberührte Inhaltsheuristik) `false`, während `is_pwd_pack_read()` korrekt
   `true` liefert, da `pages_read` unabhängig vom Seiteninhalt ist.

   Beim Aufbau dieser Testgruppe einen eigenständigen, unabhängigen Fund im eigenen
   Test-Setup gemacht (kein Firmware-Fund): die von der letzten UL-AES-Phase-2-Sitzung
   übernommenen Host-Stand-in-Header definieren `FURI_PACKED` nicht, wodurch
   `struct FURI_PACKED { ... }` mangels Makro als gewöhnlicher (mit diesem Namen getaggter)
   Struct ohne Packing geparst wird — `MfUltralightConfigPages` bekam auf dem Host dadurch
   Alignment-Padding und eine andere Größe/Feldoffsets als in der echten, gepackten Firmware-
   Struct. Für die bereits gelieferten Ports (UL-AES-Phase-2, MIFARE-Plus-SL3-Phase-2) folgenlos,
   da deren Tests `MfUltralightConfigPages` nicht anfassen; für diese neue Testgruppe, die
   direkt PWD/PACK-Feldinhalte setzt, hätte es falsche Testergebnisse geliefert. In der eigenen
   Kopie von `stubs/furi.h` dieses Ports behoben (`#define FURI_PACKED __attribute__((packed))`);
   die beiden älteren Repro-Verzeichnisse unverändert gelassen (nicht betroffen, siehe oben) —
   für eine künftige Sitzung, die dort ebenfalls gepackte Structs testen will, vorgemerkt.

Zur Kontrolle, dass die Tests tatsächlich etwas prüfen (nicht nur grün durchlaufen): gegen zwei
gezielt kaputte Varianten der neuen Funktion gegengeprüft (Schwelle `pwd_page + 1` statt
`pwd_page + 2`; fehlende `pwd_page != 0`-Absicherung) — beide lassen je einen eigenen,
benannten Testfall fehlschlagen.

**Nicht** durch diesen Host-Test abgedeckt: ob/wo der Helfer künftig tatsächlich aufgerufen
wird (UI-Anbindung, siehe "Nutzen") — dafür existiert aktuell noch kein Aufrufer, weder bei mir
noch (zu diesem Diff-Zeitpunkt) bei Unleashed selbst.

## Risiko

- **Sehr niedrig**: rein additiv (eine neue Funktion, eine neue Deklaration), keine Änderung an
  einer bestehenden Funktionssignatur oder an bestehendem Verhalten. Der Helfer wird von
  keinem existierenden Aufrufer referenziert, kann also auch bei einem Fehler im eigenen Code
  nichts am aktuellen Laufzeitverhalten ändern.
- Einzige Abhängigkeit ist die bereits vorhandene, unveränderte `mf_ultralight_get_pwd_page_num()`
  — kein neuer Zustand, kein geteilter Puffer.
- Commit-Metadaten von Unleashed waren wie beim letzten UL-AES-Port in dieser Sitzung nicht per
  API abrufbar (nur Rohdatei-Zugriff über `raw.githubusercontent.com`); geprüft gegen den
  aktuellen `dev`-Stand. Die Funktion selbst ist seit ihrer Einführung (gebündelt mit dem
  restlichen UL-AES-Feature) unverändert, siehe bereits dokumentierte vorherige Quellsuche.

## Port-Skizze

Kein mehrphasiger Port — einzelner, in sich abgeschlossener Helfer. Bezug zur bestehenden
UL-AES-Skizze (`firmware-mining/mfplus-mfultralight-aes-auth-port.md`): war dort als einer von
vier unabhängigen, beim Phase-2-Struct-Port zurückgestellten Funden vorgemerkt, nicht als
eigene nummerierte Phase. Mit diesem Patch sind zwei der vier offen; verbleibend: Typ-
Erkennungs-Umbau (NTAG210/212), `detect_protocol()`-ATQA-Lockerung, Schreibbereichs-Anpassung
— weiterhin eigene künftige Kandidaten. Das oben skizzierte Umstellen von
`is_all_data_read()` auf diesen Helfer bleibt ebenfalls offen, als eigenständiger,
künftig zu bewertender Folgeschritt (Verhaltensänderung an bestehendem, benutztem Code, siehe
"Nicht Teil dieses Patches").

## Testhinweis (lokal)

1. Die zwei `.patch`-Dateien in `lib/nfc/protocols/mf_ultralight/` einbauen (`patch -p1 <
   mf_ultralight.h.patch` / `... mf_ultralight.c.patch` aus dem Firmware-Wurzelverzeichnis).
2. `ufbt` (bzw. `fbt firmware_all`) bauen — reine Ergänzung, keine bestehende Signatur
   geändert, sollte ohne Warnungen durchlaufen.
3. Host-Repro laufen lassen (`gcc ... && ./repro`), `ALL CHECKS PASSED` erwarten.
4. Funktional am Gerät nicht separat prüfbar, da kein Aufrufer existiert (siehe "Risiko") —
   ein bestehender NFC-Lese-/Speicher-Workflow für eine passwortfähige Karte (z. B. eine
   eigene NTAG213) sollte sich durch diesen Patch in keiner Weise ändern; das ist der
   eigentliche Regressionstest.

## Quelle

- Unleashed-Firmware, `dev`-Branch (`DarkFlippers/unleashed-firmware`,
  `lib/nfc/protocols/mf_ultralight/mf_ultralight.h`/`.c`, per `raw.githubusercontent.com`
  bezogen, da der GitHub-API-Zugriff für Commit-Metadaten in dieser Sitzung erneut nicht
  verfügbar war — siehe "Risiko").
- Meine eigene `G4MEOVER-FW`-Basis, `main`-Branch, dieselben Dateien, sowie
  `mf_ultralight_poller.c` (nur gelesen, nicht verändert) zur Verifikation des oben zitierten
  `try_default_pass()`-Rollback-Verhaltens.
- Bereits vermerkte Vorarbeit: `firmware-mining/mfplus-mfultralight-aes-auth-port.md`
  (Gesamtbestandsaufnahme), `firmware-mining/mfultralight-aes-phase2-struct-serialization.md`
  (dort ursprünglich als einer von vier zurückgestellten Funden notiert).
