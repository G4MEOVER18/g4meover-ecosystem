// Host-Repro fuer den #_FILE_INDEX_FILE_NAME_-Bounds-Bug in Extensions.h.
//
// Bildet drei Dinge 1:1 nach:
//  1. handleFileIndex() / FILE_INDEX_VALID() -- setzt curFileIndexVariable aus der
//     Skriptvariable $FILE_INDEX, OHNE es bei ungueltigem Index zurueckzusetzen.
//  2. Die dynamische Konstante fuer #_FILE_INDEX_FILE_NAME_ -- ALT prueft nur, ob
//     die Dateiliste nicht leer ist, NEU prueft zusaetzlich den Index selbst.
//  3. DuckyInterpreter::getLineAndProcess() (DuckScriptInterpreter, DuckyParse.cpp,
//     Commit 23aa3ba, Zeile ~1321) -- ruft JEDE registrierte UserDefinedConstants-
//     Funktion für JEDE Zeile des Skripts auf, unabhaengig davon, ob die Zeile
//     #_FILE_INDEX_FILE_NAME_ ueberhaupt referenziert.
//
// std::vector::operator[] mit einem Index >= size() (oder negativ, nach Konversion
// zu size_t) ist laut Standard undefiniertes Verhalten -- kein garantierter Crash,
// sondern ein stiller Zugriff auf Heap-Speicher jenseits des Vektors. Damit das
// hier reproduzierbar und ohne UB laeuft, verwendet die ALTE Funktion .at() statt
// operator[] und faengt die daraus resultierende Exception als Nachweis der
// versuchten Out-of-Bounds-Zugriffsstelle ab -- im echten Code (Extensions.h)
// steht an dieser Stelle operator[], das denselben ungueltigen Index ohne jede
// Pruefung akzeptieren wuerde.

#include <cassert>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

static std::vector<std::string> curListOfFiles;
static int curFileIndexVariable = 0;

// Nachbau von handleFileIndex()s FILE_INDEX_VALID()-Zweig (Extensions.h):
// setzt curFileIndexVariable aus der Skriptvariable, meldet Gueltigkeit zurueck,
// laesst curFileIndexVariable bei Ungueltigkeit aber unveraendert auf dem
// (ungueltigen) Wert stehen -- exakt wie im Original.
static bool fileIndexValid(int scriptVarFileIndex)
{
    curFileIndexVariable = scriptVarFileIndex;
    if (curFileIndexVariable < 0 || curFileIndexVariable >= (int)curListOfFiles.size())
    {
        return false;
    }
    return true;
}

// ALT: konst.emplace_back(...)-Closure vor dem Fix -- prueft nur "Liste nicht leer".
static std::pair<std::string, std::string> constFileIndexFilenameOld()
{
    if (curListOfFiles.size() != 0)
    {
        // Im echten Code: curListOfFiles[curFileIndexVariable] (operator[], UB bei
        // ungueltigem Index). Hier .at(), um die Out-of-Bounds-Stelle sichtbar zu
        // machen statt sie tatsaechlich auszuloesen.
        try
        {
            return {"#_FILE_INDEX_FILE_NAME_", curListOfFiles.at(curFileIndexVariable)};
        }
        catch (const std::out_of_range &)
        {
            return {"#_FILE_INDEX_FILE_NAME_", "<<OOB-ZUGRIFF: operator[] waere hier UB>>"};
        }
    }
    return {"#_FILE_INDEX_FILE_NAME_", std::string()};
}

// NEU: mit Indexpruefung.
static std::pair<std::string, std::string> constFileIndexFilenameNew()
{
    if (curFileIndexVariable >= 0 && curFileIndexVariable < (int)curListOfFiles.size())
    {
        return {"#_FILE_INDEX_FILE_NAME_", curListOfFiles.at(curFileIndexVariable)};
    }
    return {"#_FILE_INDEX_FILE_NAME_", std::string()};
}

static void resetState()
{
    curListOfFiles = {"a.ds", "b.ds", "c.ds"}; // LOAD_DS_FILES_FROM_SD() findet 3 Dateien
    curFileIndexVariable = 0;
}

int main()
{
    int failures = 0;

    // Fall 1: typische Abbruchbedingung einer LOAD_DS_FILES_FROM_SD()-Iterationsschleife.
    // Skript zaehlt $FILE_INDEX von 0..2 hoch (jeweils gueltig), die Schleife prueft nach
    // jedem Schritt FILE_INDEX_VALID() und bricht beim ersten "false" ab -- das passiert
    // regulaer bei $FILE_INDEX == 3 (Listengroesse), nicht erst bei einem Tippfehler.
    resetState();
    for (int i = 0; i <= 3; i++)
    {
        bool valid = fileIndexValid(i);
        if (i < 3)
        {
            assert(valid);
        }
        else
        {
            assert(!valid); // 3 == size() -> korrekt als ungueltig erkannt ...
        }
    }
    // ... aber jede WEITERE Skriptzeile (auch jenseits der Schleife) laesst
    // getLineAndProcess() weiterhin #_FILE_INDEX_FILE_NAME_ ersetzen, mit dem
    // stehen gebliebenen curFileIndexVariable == 3 (Listengroesse -- ausserhalb).
    auto oldResult = constFileIndexFilenameOld();
    auto newResult = constFileIndexFilenameNew();
    printf("Fall 1 (Schleifenende, Index == Listengroesse 3):\n");
    printf("  ALT -> \"%s\"\n", oldResult.second.c_str());
    printf("  NEU -> \"%s\"\n", newResult.second.c_str());
    if (oldResult.second.rfind("<<OOB", 0) != 0)
    {
        printf("  FEHLER: ALT haette hier einen Out-of-Bounds-Zugriffsversuch zeigen muessen\n");
        failures++;
    }
    if (!newResult.second.empty())
    {
        printf("  FEHLER: NEU haette einen leeren String liefern muessen\n");
        failures++;
    }

    // Fall 2: Skript setzt $FILE_INDEX direkt auf einen negativen Wert
    // (z. B. eine fehlerhafte Dekrement-Schleife, VAR $FILE_INDEX = $FILE_INDEX - 1
    // ohne Untergrenzenpruefung). size_t-Konversion von -1 waere bei operator[]
    // ein Zugriff nahe dem Ende des Adressraums -- der schwerwiegendste Fall.
    resetState();
    bool valid = fileIndexValid(-1);
    assert(!valid);
    oldResult = constFileIndexFilenameOld();
    newResult = constFileIndexFilenameNew();
    printf("Fall 2 (negativer Index -1):\n");
    printf("  ALT -> \"%s\"\n", oldResult.second.c_str());
    printf("  NEU -> \"%s\"\n", newResult.second.c_str());
    if (oldResult.second.rfind("<<OOB", 0) != 0)
    {
        printf("  FEHLER: ALT haette hier einen Out-of-Bounds-Zugriffsversuch zeigen muessen\n");
        failures++;
    }
    if (!newResult.second.empty())
    {
        printf("  FEHLER: NEU haette einen leeren String liefern muessen\n");
        failures++;
    }

    // Fall 3 (Kontrolle): gueltiger Index -- Verhalten von ALT und NEU muss identisch
    // bleiben, keine Regression fuer den bestehenden, korrekten Anwendungsfall.
    resetState();
    valid = fileIndexValid(1);
    assert(valid);
    oldResult = constFileIndexFilenameOld();
    newResult = constFileIndexFilenameNew();
    printf("Fall 3 (gueltiger Index 1):\n");
    printf("  ALT -> \"%s\"\n", oldResult.second.c_str());
    printf("  NEU -> \"%s\"\n", newResult.second.c_str());
    if (oldResult.second != "b.ds" || newResult.second != "b.ds")
    {
        printf("  FEHLER: gueltiger Fall sollte unveraendert \"b.ds\" liefern\n");
        failures++;
    }

    if (failures == 0)
    {
        printf("\nAlle Faelle wie erwartet: ALT versucht bei ungueltigem Index einen "
               "Out-of-Bounds-Zugriff (im echten Code per operator[] = UB), NEU liefert "
               "sauber einen leeren String; der gueltige Fall ist unveraendert.\n");
        return 0;
    }
    printf("\n%d Fall/Faelle nicht wie erwartet.\n", failures);
    return 1;
}
