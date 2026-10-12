# usb-army-penetrator — `USBMSC::end()`: verkehrte Bedingung, Flush trifft im Hauptfall gar nicht und im Rohzugriff-Fall das falsche Ziel

## Was
`src/Devices/USB/USBMSC.cpp`, `USBMSC::end()`:

```cpp
void USBMSC::end()
{
    if (!mscFile)
    {
        usb_msc.setUnitReady(false);
        mscFile.flush();
    }
}
```

`mscFile` ist das statische `File`-Handle der Datei, die über
`USB_MOUNT_DISK_READ_ONLY`/`USB_MOUNT_CDROM_READ_ONLY` (Ducky-Befehl →
`handleUSBMode()` → `USBMSC::mountDiskImage()` → `open_msc()`) als Disk-Image
gemountet wurde. Alle drei TinyUSB-Callbacks in genau dieser Datei
(`msc_read_cb`, `msc_write_cb`, `msc_flush_cb`) prüfen konsistent
`mscFile && strlen(mscFile.name()) != 0`, um zwischen zwei Modi zu
unterscheiden:

- **Image-Modus**: ein Disk-Image ist über `mscFile` offen (die Prüfung ist
  wahr).
- **Rohzugriff-Modus**: kein Image offen, `USBMSC::mountSD()` exponiert die
  SD-Karte direkt über `Devices::Storage` (die Prüfung ist falsch).

`msc_flush_cb()` behandelt beide Fälle richtig:

```cpp
static void msc_flush_cb()
{
    if (mscFile && strlen(mscFile.name()) != 0)
    {
        mscFile.flush();
    }
    else
    {
        Devices::Storage.flush();
    }
}
```

`USBMSC::end()` benutzt an derselben Stelle **dieselbe Bedingung mit
umgedrehtem Vorzeichen** (`if (!mscFile)` statt `if (mscFile && ...)`) und
flusht dabei zusätzlich im übrig bleibenden Zweig das falsche Objekt:

- **Image-Modus** (der in der Praxis häufigste Fall, ausgelöst durch
  `USB_MOUNT_DISK_READ_ONLY`/`_CDROM_READ_ONLY`): `mscFile` ist "truthy",
  `!mscFile` ist falsch → der gesamte Funktionskörper wird **übersprungen**.
  Kein `setUnitReady(false)`, kein Flush.
- **Rohzugriff-Modus** (`USBMSC::mountSD()`): `mscFile` ist ungültig,
  `!mscFile` ist wahr → der Zweig läuft, aber er ruft `mscFile.flush()` auf
  einem ungültigen/nie geöffneten `File`-Objekt auf statt — wie
  `msc_flush_cb()` es im identischen Fall richtig macht — auf
  `Devices::Storage.flush()`, dem tatsächlich aktiven Backing-Store.

`USBMSC::end()` wird von `USBCore::end()` aufgerufen, und die ist wiederum
der zentrale Aufräum-Schritt direkt vor jedem Soft-Reset der App — in
`DuckyPayload.cpp`, `DuckyPayload::loop()`:

```cpp
else if (requiresReset)
{
    Devices::USB::Core.end();
#ifdef ARDUINO_ARCH_ESP32
    ESP.restart();
#else
    watchdog_reboot(0, SRAM_END, 0);
#endif
}
```

`requiresReset` wird über die `reset`-Callback des `DuckyInterpreter` gesetzt
(z. B. nach dem entsprechenden Ducky-Reset-Befehl mitten in einem laufenden
Skript) — also genau der Pfad, über den ein Skript den Controller neu
startet, während ein Mass-Storage-Gerät (Image oder Rohzugriff) aktiv am
Host angemeldet ist.

## Nutzen
- **Hauptfall (Image gemountet) ist komplett unbehandelt**: der Host wird vor
  dem Reboot nie per `setUnitReady(false)` informiert, dass das Medium
  verschwindet, und das offene Image-`File` wird nicht geflusht. Für ein rein
  lesend gemountetes Image (`mountAsCD`/`_READ_ONLY`) ist das Flush-Fehlen
  selbst unkritisch (`msc_write_cb()` lehnt Schreibzugriffe auf Images ohnehin
  ab), aber das fehlende `setUnitReady(false)` bedeutet, dass der Host das
  Gerät beim abrupten USB-Disconnect/Reset als unsauber entferntes
  Wechselmedium sieht statt als ordentlich ausgehängt — auf Windows typischer
  Auslöser für "Gerät wurde nicht ordnungsgemäß entfernt"/verzögerte
  Neuerkennung beim nächsten Enumerieren.
- **Rohzugriff-Fall flusht das falsche Ziel**: hier kann der Host tatsächlich
  schreiben (`msc_write_cb()` leitet an `Devices::Storage.writeRawSectors()`
  weiter). Bricht das Skript genau hier per Reset ab, flusht `USBMSC::end()`
  ein ungültiges `File`-Objekt statt der SD-Karte selbst — bereits vom Host
  geschriebene, aber noch nicht physisch committete Sektoren bleiben
  ungeflusht im Cache, während der Controller sofort neu startet. Das ist der
  in diesem Projekt bereits mehrfach dokumentierte Fehlerklassen-Typ
  "fehlender Flush vor Reset/Reboot" (vgl. `PcapBuffer`/`UkfeRfControl`), hier
  für den SD-Rohzugriffspfad.
- Kleinstmöglicher Fix: dieselbe, im selben File bereits etablierte
  Fallunterscheidung (`msc_flush_cb()`) wird 1:1 auf `USBMSC::end()`
  übertragen — keine neue Logik, keine geänderte Semantik der beiden Modi.

## Betroffene Dateien
- `src/Devices/USB/USBMSC.cpp` — nur `USBMSC::end()`. Keine Änderung an
  `msc_read_cb()`, `msc_write_cb()`, `msc_flush_cb()`, `open_msc()`,
  `mountSD()` oder `mountDiskImage()` nötig — deren bereits korrekte
  Fallunterscheidung ist die Vorlage für den Fix.

## Patch
```cpp
void USBMSC::end()
{
    usb_msc.setUnitReady(false);

    if (mscFile && strlen(mscFile.name()) != 0)
    {
        mscFile.flush();
    }
    else
    {
        Devices::Storage.flush();
    }
}
```

Vollständiger Patch in `usbmsc-end-inverted-flush-fix/usbmsc-end-inverted-flush.patch`,
gegen zwei frische, unabhängige Zweitkopien von `G4MEOVER18/usb-army-penetrator`
(`411b153`) mit `git apply --check` verifiziert, real angewendet, Ergebnis
byte-für-byte abgeglichen — identisch.

## Testhinweis (ohne Hardware)
Reine Zustands-/Kontrollfluss-Logik (welcher Zweig läuft, welches Objekt
geflusht wird) — kein USB-, SD- oder Arduino-Header nötig. Repro-Programm
(`g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
-fno-sanitize-recover=undefined repro.cpp -o repro && ./repro`) in
`repro-usbmsc-end-inverted-flush/repro.cpp`. Es bildet `File` (`operator
bool()`, `name()`, `flush()`), `Devices::Storage.flush()` und
`usb_msc.setUnitReady()` als schlanke Zähler-Mocks nach und baut beide
Fassungen von `USBMSC::end()` (ALT/NEU) im selben Programm nach, dann zwei
Szenarien:

1. **Disk-Image gemountet** (`mscFile` offen) — der Hauptfall nach
   `USB_MOUNT_DISK_READ_ONLY`/`_CDROM_READ_ONLY`.
2. **Kein Image gemountet** (`mscFile` ungültig) — Rohzugriffs-Modus
   (`mountSD()`) oder nie gemountet.

Ergebnis (identisch bei `-O0`/`-O1`, keine Compiler-Warnungen, keine
ASan/UBSan-Meldungen): in Szenario 1 tut ALT nichts
(`setUnitReady(false)`=0, `mscFile.flush()`=0), NEU ruft korrekt beides auf;
in Szenario 2 ruft ALT zwar `setUnitReady(false)` auf, flusht aber das
ungültige `File`-Objekt statt `Devices::Storage` (`Storage.flush()`=0), NEU
flusht korrekt `Devices::Storage`.

Funktionaler Test danach lokal am Gerät: `USB_MOUNT_DISK_READ_ONLY
<image>.img` ausführen, danach ein Ducky-Skript mit einem Reset-Befehl mitten
im Mount; vor dem Fix bleibt das Gerät für den Host bis zum nächsten manuellen
Replug als unsauber entfernt markiert, danach meldet der Reset-Pfad das
Medium vor dem Neustart korrekt ab. Für den Rohzugriffs-Pfad:
`mountSD()`-Variante mit aktivem Schreibzugriff vom Host, dann Reset mitten im
Schreibvorgang — vor dem Fix riskiert das ungeflushte `Devices::Storage`
Datenverlust auf der SD-Karte, danach ist der letzte Schreibzugriff vor dem
Neustart geflusht.

## Risiko
- Sehr niedrig: übernimmt nur die im selben File bereits etablierte,
  korrekte Fallunterscheidung von `msc_flush_cb()` — keine neue
  Kontrollflusslogik.
- `usb_msc.setUnitReady(false)` wird jetzt unconditional aufgerufen (auch
  wenn MSC nie gestartet wurde); das ist bereits exakt das Verhalten, das der
  ALT-Code im Rohzugriffs-Zweig ebenfalls unconditional ausführt, also keine
  neue Annahme.
- `Devices::Storage.flush()` unconditional im Else-Zweig aufzurufen (auch
  wenn nie im Rohzugriffsmodus gemountet wurde) ist derselbe bereits
  bestehende Aufrufmusterfall wie in `msc_flush_cb()` — kein neues Risiko.
- Kein zusätzliches Sicherheits-/Missbrauchsrisiko; der Fix schließt eine
  Datenverlust-/Host-Verwirrungslücke vor einem Reset, ändert aber nichts an
  Lese-/Schreibrechten oder Mount-Berechtigungen.

## Quelle
- `G4MEOVER18/usb-army-penetrator` (main, `411b153`):
  `src/Devices/USB/USBMSC.cpp` (`USBMSC::end()`, `msc_flush_cb()`,
  `msc_read_cb()`, `msc_write_cb()`, `open_msc()`), aufgerufen von
  `src/Devices/USB/USBCore.cpp` (`USBCore::end()`), aufgerufen von
  `src/Attacks/Ducky/DuckyPayload.cpp` (`DuckyPayload::loop()`,
  `requiresReset`-Zweig).
