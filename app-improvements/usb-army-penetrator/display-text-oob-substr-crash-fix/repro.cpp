// Host-Repro: DISPLAY_TEXT-Argumentparsing aus
// src/Attacks/Ducky/Extensions.h (handleDisplayText()), 1:1 die reine
// String-Logik ohne Hardware-Abhaengigkeit (Devices::TFT/asciiOrVariableToInt
// durch einfache Stand-ins ersetzt, Steuerfluss unveraendert).
//
// ALT = Originalzustand (zwei ungeprueft Aufrufe von std::string::substr()
// mit einer pos, die > remainingArgs.size() sein kann -> std::out_of_range,
// in Execute() nirgends gefangen -> std::terminate()/abort()).
// NEU = Zustand nach dem Fix (Grenzen vor jedem substr() geprueft).
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <unordered_map>

static int asciiOrVariableToInt(const std::string &input, const std::unordered_map<std::string, int> &variables)
{
    if (variables.find(input) == variables.cend())
        return atoi(input.c_str());
    return variables.at(input);
}

// ---- ALT: byte-identisch zur Parsing-Logik in handleDisplayText() vor dem Fix ----
static bool handleDisplayText_ALT(const std::string &str, const std::unordered_map<std::string, int> &variables,
                                   int *outX, int *outY, std::string *outText)
{
    std::string remainingArgs = str.substr(str.find(' ') + 1);

    auto xPosStr = remainingArgs.substr(0, remainingArgs.find(' '));
    int xPos = asciiOrVariableToInt(xPosStr, variables);

    auto yStartPos = xPosStr.length() + 1;
    auto yCount = remainingArgs.find(' ', xPosStr.length() + 1) - yStartPos;

    auto yPosStr = remainingArgs.substr(yStartPos, yCount); // (1) kann werfen
    int yPos = asciiOrVariableToInt(yPosStr, variables);

    auto text = remainingArgs.substr(xPosStr.length() + yPosStr.length() + 2); // (2) kann werfen

    *outX = xPos;
    *outY = yPos;
    *outText = text;
    return true;
}

// ---- NEU: Fix aus display-text-oob-substr-crash-fix.md ----
static bool handleDisplayText_NEU(const std::string &str, const std::unordered_map<std::string, int> &variables,
                                   int *outX, int *outY, std::string *outText)
{
    std::string remainingArgs = str.substr(str.find(' ') + 1);

    const auto xEnd = remainingArgs.find(' ');
    if (xEnd == std::string::npos)
    {
        return false; // "DISPLAY_TEXT needs x, y and text arguments"
    }

    auto xPosStr = remainingArgs.substr(0, xEnd);
    int xPos = asciiOrVariableToInt(xPosStr, variables);

    const auto yStartPos = xEnd + 1;
    const auto yEnd = remainingArgs.find(' ', yStartPos);
    if (yEnd == std::string::npos)
    {
        return false; // "DISPLAY_TEXT needs x, y and text arguments"
    }

    auto yPosStr = remainingArgs.substr(yStartPos, yEnd - yStartPos);
    int yPos = asciiOrVariableToInt(yPosStr, variables);

    auto text = remainingArgs.substr(yEnd + 1);

    *outX = xPos;
    *outY = yPos;
    *outText = text;
    return true;
}

struct Case
{
    const char *cmd;
    bool wellFormed; // erwartet: keine Ausnahme, Felder gesetzt
};

int main()
{
    const std::unordered_map<std::string, int> noVars;
    int checks = 0, failed = 0;

    auto check = [&](bool cond, const char *what)
    {
        checks++;
        if (!cond)
        {
            failed++;
            printf("FAIL: %s\n", what);
        }
    };

    printf("=== ALT (vor Fix): handleDisplayText() Argumentparsing ===\n");
    {
        const char *badCmds[] = {
            "DISPLAY_TEXT 10",     // y und text fehlen komplett
            "DISPLAY_TEXT 10 20",  // text fehlt (kein trailing space)
        };
        for (const char *cmd : badCmds)
        {
            int x = 0, y = 0;
            std::string text;
            bool threw = false;
            try
            {
                handleDisplayText_ALT(cmd, noVars, &x, &y, &text);
            }
            catch (const std::out_of_range &)
            {
                threw = true;
            }
            printf("[ALT] '%s' -> %s\n", cmd, threw ? "WIRFT std::out_of_range (Absturz)" : "kein Wurf");
            check(threw, "ALT sollte bei fehlendem y/text werfen");
        }

        // Regression: wohlgeformter Befehl bleibt unveraendert korrekt
        int x = 0, y = 0;
        std::string text;
        bool ok = handleDisplayText_ALT("DISPLAY_TEXT 10 20 Hello World", noVars, &x, &y, &text);
        printf("[ALT] 'DISPLAY_TEXT 10 20 Hello World' -> x=%d y=%d text=\"%s\"\n", x, y, text.c_str());
        check(ok && x == 10 && y == 20 && text == "Hello World", "ALT wohlgeformter Fall");

        // Regression: Text mit trailing space explizit leer
        bool ok2 = handleDisplayText_ALT("DISPLAY_TEXT 10 20 ", noVars, &x, &y, &text);
        printf("[ALT] 'DISPLAY_TEXT 10 20 ' (trailing space) -> x=%d y=%d text=\"%s\"\n", x, y, text.c_str());
        check(ok2 && x == 10 && y == 20 && text.empty(), "ALT leerer Text mit trailing space");
    }

    printf("\n=== NEU (nach Fix) ===\n");
    {
        const char *badCmds[] = {
            "DISPLAY_TEXT 10",
            "DISPLAY_TEXT 10 20",
        };
        for (const char *cmd : badCmds)
        {
            int x = 0, y = 0;
            std::string text;
            bool threw = false;
            bool ret = false;
            try
            {
                ret = handleDisplayText_NEU(cmd, noVars, &x, &y, &text);
            }
            catch (const std::out_of_range &)
            {
                threw = true;
            }
            printf("[NEU] '%s' -> %s, ret=%d\n", cmd, threw ? "WIRFT (FEHLER)" : "kein Wurf", ret);
            check(!threw, "NEU darf nie werfen");
            check(ret == false, "NEU lehnt unvollstaendigen Befehl kontrolliert ab (ret=false)");
        }

        int x = 0, y = 0;
        std::string text;
        bool ok = handleDisplayText_NEU("DISPLAY_TEXT 10 20 Hello World", noVars, &x, &y, &text);
        printf("[NEU] 'DISPLAY_TEXT 10 20 Hello World' -> x=%d y=%d text=\"%s\"\n", x, y, text.c_str());
        check(ok && x == 10 && y == 20 && text == "Hello World", "NEU wohlgeformter Fall unveraendert");

        bool ok2 = handleDisplayText_NEU("DISPLAY_TEXT 10 20 ", noVars, &x, &y, &text);
        printf("[NEU] 'DISPLAY_TEXT 10 20 ' (trailing space) -> x=%d y=%d text=\"%s\"\n", x, y, text.c_str());
        check(ok2 && x == 10 && y == 20 && text.empty(), "NEU leerer Text mit trailing space unveraendert");

        // Variablen statt Literalen fuer x/y (haeufiger Anwendungsfall, $X $Y)
        std::unordered_map<std::string, int> vars = {{"$X", 5}, {"$Y", 7}};
        bool ok3 = handleDisplayText_NEU("DISPLAY_TEXT $X $Y Status: OK", vars, &x, &y, &text);
        printf("[NEU] 'DISPLAY_TEXT $X $Y Status: OK' -> x=%d y=%d text=\"%s\"\n", x, y, text.c_str());
        check(ok3 && x == 5 && y == 7 && text == "Status: OK", "NEU Variablen-Argumente unveraendert");
    }

    printf("\n%d Pruefungen, %d fehlgeschlagen.\n", checks, failed);
    if (failed == 0)
    {
        printf("ALLE CHECKS BESTANDEN\n");
        return 0;
    }
    printf("ES GAB FEHLSCHLAEGE\n");
    return 1;
}
