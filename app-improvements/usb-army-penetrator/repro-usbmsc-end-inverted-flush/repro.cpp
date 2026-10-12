// Host repro for the usb-army-penetrator USBMSC::end() inverted-condition fix.
//
// Mocks just enough of the Arduino File/Storage surface used by
// src/Devices/USB/USBMSC.cpp to run both the ALT (as shipped) and NEU (fixed)
// versions of USBMSC::end() side by side, and to observe which flush target
// (the open disk-image File vs. the raw-SD-backing Devices::Storage) and
// which setUnitReady() call each version actually performs.
//
// Build: g++ -std=c++17 -Wall -Wextra repro.cpp -o repro && ./repro

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

// ---- Minimal mock of the Arduino File type used by USBMSC.cpp ----------
// Real File::operator bool() returns true iff the file handle is open/valid;
// File::name() returns "" for an unopened/invalid handle. USBMSC.cpp's own
// msc_read_cb/msc_write_cb/msc_flush_cb already rely on exactly this pair of
// checks ("mscFile && strlen(mscFile.name()) != 0") to mean "a disk image is
// currently open" -- that is the ground truth this repro holds fixed.
struct MockFile
{
    bool valid = false;
    int flushCalls = 0;

    explicit operator bool() const { return valid; }
    const char *name() const { return valid ? "image.img" : ""; }
    void flush() { flushCalls++; }
};

// ---- Minimal mock of Devices::Storage's flush(), the raw-SD backing store
struct MockStorage
{
    int flushCalls = 0;
    void flush() { flushCalls++; }
};

// ---- Minimal mock of the TinyUSB MSC wrapper's setUnitReady() ----------
struct MockUsbMsc
{
    int setUnitReadyFalseCalls = 0;
    void setUnitReady(bool ready)
    {
        if (!ready) setUnitReadyFalseCalls++;
    }
};

static MockFile mscFile;
static MockStorage Storage;
static MockUsbMsc usb_msc;

static void reset_mocks(bool fileMounted)
{
    mscFile = MockFile{};
    mscFile.valid = fileMounted;
    Storage = MockStorage{};
    usb_msc = MockUsbMsc{};
}

// ---- ALT: USBMSC::end() exactly as shipped (src/Devices/USB/USBMSC.cpp:158) --
static void end_ALT()
{
    if (!mscFile)
    {
        usb_msc.setUnitReady(false);
        mscFile.flush();
    }
}

// ---- NEU: fixed USBMSC::end(), matching the existing msc_flush_cb() pattern --
static void end_NEU()
{
    usb_msc.setUnitReady(false);

    if (mscFile && strlen(mscFile.name()) != 0)
    {
        mscFile.flush();
    }
    else
    {
        Storage.flush();
    }
}

struct Scenario
{
    const char *label;
    bool fileMounted; // USB_MOUNT_DISK_READ_ONLY/_CDROM with a .img on mscFile
};

int main()
{
    const Scenario scenarios[] = {
        {"Disk-Image gemountet (mscFile offen) -- der Hauptfall von USB_MOUNT_DISK_READ_ONLY/_CDROM_READ_ONLY", true},
        {"Kein Image gemountet (mscFile ungueltig) -- Rohzugriff-Modus (mountSD()) oder nie gemountet", false},
    };

    int failures = 0;

    for (const auto &scenario : scenarios)
    {
        std::cout << "== " << scenario.label << " ==\n";

        // ALT
        reset_mocks(scenario.fileMounted);
        end_ALT();
        const int altUnitNotReady = usb_msc.setUnitReadyFalseCalls;
        const int altFileFlush = mscFile.flushCalls;
        const int altStorageFlush = Storage.flushCalls;

        // NEU
        reset_mocks(scenario.fileMounted);
        end_NEU();
        const int neuUnitNotReady = usb_msc.setUnitReadyFalseCalls;
        const int neuFileFlush = mscFile.flushCalls;
        const int neuStorageFlush = Storage.flushCalls;

        std::cout << "  ALT: setUnitReady(false)=" << altUnitNotReady
                   << " mscFile.flush()=" << altFileFlush
                   << " Storage.flush()=" << altStorageFlush << "\n";
        std::cout << "  NEU: setUnitReady(false)=" << neuUnitNotReady
                   << " mscFile.flush()=" << neuFileFlush
                   << " Storage.flush()=" << neuStorageFlush << "\n";

        if (scenario.fileMounted)
        {
            // Hauptfall: ALT tut nichts (mscFile ist "truthy", die Bedingung
            // !mscFile ist false) -- der Host erfaehrt nie, dass das Medium
            // entfernt wird, und das offene Image-File wird nicht geflusht.
            if (altUnitNotReady != 0 || altFileFlush != 0)
            {
                std::cerr << "  UNERWARTET: ALT hat im Hauptfall doch etwas getan\n";
                failures++;
            }
            // NEU tut in genau diesem Fall das Richtige.
            if (neuUnitNotReady != 1 || neuFileFlush != 1 || neuStorageFlush != 0)
            {
                std::cerr << "  FEHLER: NEU hat im Hauptfall nicht korrekt reagiert\n";
                failures++;
            }
        }
        else
        {
            // Rohzugriff-Fall: ALT setzt zwar setUnitReady(false), flusht aber
            // das falsche Ziel (ein ungueltiges File-Objekt statt Devices::Storage).
            if (altUnitNotReady != 1 || altFileFlush != 1 || altStorageFlush != 0)
            {
                std::cerr << "  UNERWARTET: ALT-Verhalten im Rohzugriff-Fall abweichend\n";
                failures++;
            }
            // NEU setzt setUnitReady(false) und flusht das tatsaechlich genutzte
            // Backing-Store (Devices::Storage), nicht das ungueltige File-Objekt.
            if (neuUnitNotReady != 1 || neuFileFlush != 0 || neuStorageFlush != 1)
            {
                std::cerr << "  FEHLER: NEU hat im Rohzugriff-Fall nicht korrekt reagiert\n";
                failures++;
            }
        }

        std::cout << "\n";
    }

    if (failures == 0)
    {
        std::cout << "Alle Szenarien bestanden: ALT verfehlt setUnitReady(false)+Flush im "
                     "Hauptfall (Image gemountet) komplett und flusht im Rohzugriff-Fall das "
                     "falsche Ziel; NEU liefert in beiden Faellen das korrekte Paar "
                     "(setUnitReady(false) + Flush des tatsaechlich aktiven Backing-Stores).\n";
        return 0;
    }
    else
    {
        std::cerr << failures << " Pruefung(en) fehlgeschlagen.\n";
        return 1;
    }
}
