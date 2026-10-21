// Host repro for app-improvements/usb-army-penetrator/set-setting-string-whitespace-value-fix
//
// Reproduces, verbatim, Ducky::SplitString() from
// i-am-shodan/DuckScriptInterpreter@23aa3ba87e95f402ccab53beb9ead54602643dd7 (src/DuckyParse.h)
// and startsWith()/handleSetSetting() from
// G4MEOVER18/usb-army-penetrator@411b153d749f66a5c0059c39a3933c61971fcb2f
// (src/Attacks/Ducky/Extensions.h), in ALT (current) and NEU (fixed) form.
//
// No Arduino/Preferences/USB objects are needed: handleSetSetting()'s own argument
// parsing is pure string logic. setSettingValue() is stubbed to record what it was
// called with, mirroring how the previous raw-hid-stoi-uncaught-exception-crash-fix
// and settings-int-uint16-stoi-uncaught-exception-crash-fix repros isolated handler
// logic from hardware.

#include <cassert>
#include <cctype>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

// ---- verbatim from DuckyParse.h ----
namespace Ducky
{
    static std::vector<std::string> SplitString(const std::string &input)
    {
        std::vector<std::string> words;
        std::string word;

        for (char c : input)
        {
            if (std::isspace(c))
            {
                if (!word.empty())
                {
                    words.push_back(word);
                    word.clear();
                }
            }
            else
            {
                word += c;
            }
        }

        if (!word.empty())
        {
            words.push_back(word);
        }

        return words;
    }
}

// ---- verbatim from Extensions.h ----
bool startsWith(const std::string &str, const std::string &prefix)
{
    return str.rfind(prefix, 0) == 0;
}

// ---- stand-in for setSettingValue(Preferences&, name, value) ----
// Records the last call instead of touching NVS/Preferences.
static int totalErrors = 0;
static std::string lastSetName;
static std::string lastSetValue;
static bool setSettingValueCalled = false;

static bool setSettingValue(const std::string &name, const std::string &value)
{
    lastSetName = name;
    lastSetValue = value;
    setSettingValueCalled = true;
    return true;
}

static void resetState()
{
    totalErrors = 0;
    lastSetName.clear();
    lastSetValue.clear();
    setSettingValueCalled = false;
}

// ---- ALT: handleSetSetting() exactly as in Extensions.h ----
static bool handleSetSetting_ALT(const std::string &str)
{
    bool ret = false;

    do
    {
        const auto split = Ducky::SplitString(str);
        if (split.size() != 3)
        {
            totalErrors++;
            break;
        }

        const auto settingName = split[1];
        auto settingValue = split[2];

        if (startsWith(str, "SET_SETTING_UINT16") && settingValue.rfind("0x", 0) != 0)
        {
            settingValue = "0x" + settingValue;
        }

        if (startsWith(str, "SET_SETTING_BOOL"))
        {
            if (settingValue == "0" || settingValue == "1" || settingValue == "TRUE" || settingValue == "FALSE")
            {
                if (settingValue == "TRUE" || settingValue == "1")
                {
                    settingValue = "1";
                }
                else
                {
                    settingValue = "0";
                }
            }
            else
            {
                totalErrors++;
                break;
            }
        }

        ret = setSettingValue(settingName, settingValue);

    } while (false);

    return ret;
}

// ---- NEU: handleSetSetting() with the SET_SETTING_STRING fix ----
static bool handleSetSetting_NEU(const std::string &str)
{
    bool ret = false;

    do
    {
        const auto split = Ducky::SplitString(str);
        const bool isStringSetting = startsWith(str, "SET_SETTING_STRING");

        if (split.size() < 3 || (!isStringSetting && split.size() != 3))
        {
            totalErrors++;
            break;
        }

        const auto settingName = split[1];
        auto settingValue = split[2];

        if (isStringSetting)
        {
            // Reconstruct the value from the original line so whitespace inside it
            // (WiFi SSID/password, USB descriptor strings, ...) survives instead of
            // being cut at the first space by Ducky::SplitString() above.
            const auto nameEnd = str.find(settingName, str.find(' ') + 1) + settingName.length();
            const auto valueStart = str.find_first_not_of(" \t", nameEnd);
            settingValue = (valueStart == std::string::npos) ? std::string() : str.substr(valueStart);
        }

        if (startsWith(str, "SET_SETTING_UINT16") && settingValue.rfind("0x", 0) != 0)
        {
            settingValue = "0x" + settingValue;
        }

        if (startsWith(str, "SET_SETTING_BOOL"))
        {
            if (settingValue == "0" || settingValue == "1" || settingValue == "TRUE" || settingValue == "FALSE")
            {
                if (settingValue == "TRUE" || settingValue == "1")
                {
                    settingValue = "1";
                }
                else
                {
                    settingValue = "0";
                }
            }
            else
            {
                totalErrors++;
                break;
            }
        }

        ret = setSettingValue(settingName, settingValue);

    } while (false);

    return ret;
}

struct Case
{
    const char *label;
    const char *line;
};

static const Case kCases[] = {
    // Finding scenarios: SET_SETTING_STRING value contains whitespace.
    {"string-wifi-pwd-three-words", "SET_SETTING_STRING WIFI_AP_PWD my secret password"},
    {"string-wifi-ap-name-two-words", "SET_SETTING_STRING WIFI_AP_NAME Free WiFi"},
    {"string-usb-manufacturer-two-words", "SET_SETTING_STRING USB_DeviceManufacturer Generic Corp"},
    {"string-multiple-interior-spaces", "SET_SETTING_STRING WIFI_AP_PWD   extra   spaced   value"},
    // Regression: SET_SETTING_STRING with a single-word value (already worked before).
    {"string-single-word-value", "SET_SETTING_STRING WIFI_AP_NAME MyAP"},
    // Regression: SET_SETTING_STRING with no value at all must still be rejected.
    {"string-missing-value", "SET_SETTING_STRING WIFI_AP_NAME"},
    // Regression: non-string settings keep their exact-token-count validation.
    {"bool-valid-true", "SET_SETTING_BOOL FOO TRUE"},
    {"bool-trailing-garbage-rejected", "SET_SETTING_BOOL FOO TRUE extra"},
    {"bool-invalid-value-rejected", "SET_SETTING_BOOL FOO MAYBE"},
    {"int16-valid", "SET_SETTING_INT16 FOO 5"},
    {"int16-trailing-garbage-rejected", "SET_SETTING_INT16 FOO 5 6"},
    {"uint16-hex-prefixed", "SET_SETTING_UINT16 FOO 0x10"},
    {"uint16-prefix-auto-added", "SET_SETTING_UINT16 FOO 10"},
};

int main()
{
    int checks = 0;

    for (const auto &c : kCases)
    {
        resetState();
        bool retAlt = handleSetSetting_ALT(c.line);
        bool altAccepted = retAlt && setSettingValueCalled;
        std::string altName = lastSetName;
        std::string altValue = lastSetValue;
        int altErrors = totalErrors;

        resetState();
        bool retNeu = handleSetSetting_NEU(c.line);
        bool neuAccepted = retNeu && setSettingValueCalled;
        std::string neuName = lastSetName;
        std::string neuValue = lastSetValue;
        int neuErrors = totalErrors;

        std::printf("[%-32s] ALT accepted=%d name=%-24s value=%-28s errors=%d | NEU accepted=%d name=%-24s value=%-28s errors=%d\n",
                    c.label,
                    altAccepted, altName.c_str(), altValue.c_str(), altErrors,
                    neuAccepted, neuName.c_str(), neuValue.c_str(), neuErrors);
    }

    // --- Finding: ALT rejects every multi-word SET_SETTING_STRING value; NEU accepts it intact. ---
    {
        resetState();
        bool ret = handleSetSetting_ALT("SET_SETTING_STRING WIFI_AP_PWD my secret password");
        assert(ret == false);
        assert(setSettingValueCalled == false);
        assert(totalErrors == 1);
        checks++;
    }
    {
        resetState();
        bool ret = handleSetSetting_NEU("SET_SETTING_STRING WIFI_AP_PWD my secret password");
        assert(ret == true);
        assert(setSettingValueCalled == true);
        assert(lastSetName == "WIFI_AP_PWD");
        assert(lastSetValue == "my secret password");
        assert(totalErrors == 0);
        checks++;
    }
    {
        resetState();
        bool ret = handleSetSetting_NEU("SET_SETTING_STRING WIFI_AP_NAME Free WiFi");
        assert(ret == true);
        assert(lastSetName == "WIFI_AP_NAME");
        assert(lastSetValue == "Free WiFi");
        checks++;
    }
    {
        resetState();
        bool ret = handleSetSetting_NEU("SET_SETTING_STRING USB_DeviceManufacturer Generic Corp");
        assert(ret == true);
        assert(lastSetName == "USB_DeviceManufacturer");
        assert(lastSetValue == "Generic Corp");
        checks++;
    }
    // Interior whitespace between name and value, and inside the value, collapses
    // consistently through SplitString() tokenisation boundaries (matches ALT's
    // existing single-word behaviour for the same case, just carried over all words).
    {
        resetState();
        bool ret = handleSetSetting_NEU("SET_SETTING_STRING WIFI_AP_PWD   extra   spaced   value");
        assert(ret == true);
        assert(lastSetValue == "extra   spaced   value");
        checks++;
    }

    // --- Regression: single-word STRING value unchanged between ALT and NEU. ---
    {
        resetState();
        bool retAlt = handleSetSetting_ALT("SET_SETTING_STRING WIFI_AP_NAME MyAP");
        std::string altValue = lastSetValue;
        resetState();
        bool retNeu = handleSetSetting_NEU("SET_SETTING_STRING WIFI_AP_NAME MyAP");
        std::string neuValue = lastSetValue;
        assert(retAlt == retNeu);
        assert(altValue == neuValue);
        assert(neuValue == "MyAP");
        checks++;
    }

    // --- Regression: STRING with no value at all stays rejected in both. ---
    {
        resetState();
        bool retAlt = handleSetSetting_ALT("SET_SETTING_STRING WIFI_AP_NAME");
        int altErrors = totalErrors;
        resetState();
        bool retNeu = handleSetSetting_NEU("SET_SETTING_STRING WIFI_AP_NAME");
        int neuErrors = totalErrors;
        assert(retAlt == false && retNeu == false);
        assert(altErrors == 1 && neuErrors == 1);
        checks++;
    }

    // --- Regression: non-STRING settings keep identical ALT/NEU behaviour for every case. ---
    const char *nonStringCases[] = {
        "SET_SETTING_BOOL FOO TRUE",
        "SET_SETTING_BOOL FOO FALSE",
        "SET_SETTING_BOOL FOO 1",
        "SET_SETTING_BOOL FOO 0",
        "SET_SETTING_BOOL FOO MAYBE",
        "SET_SETTING_BOOL FOO TRUE extra",
        "SET_SETTING_INT16 FOO 5",
        "SET_SETTING_INT16 FOO -5",
        "SET_SETTING_INT16 FOO 5 6",
        "SET_SETTING_UINT16 FOO 0x10",
        "SET_SETTING_UINT16 FOO 10",
        "SET_SETTING_UINT16 FOO 10 20",
    };
    for (const auto *line : nonStringCases)
    {
        resetState();
        bool retAlt = handleSetSetting_ALT(line);
        std::string altName = lastSetName, altValue = lastSetValue;
        int altErrors = totalErrors;
        bool altCalled = setSettingValueCalled;

        resetState();
        bool retNeu = handleSetSetting_NEU(line);
        std::string neuName = lastSetName, neuValue = lastSetValue;
        int neuErrors = totalErrors;
        bool neuCalled = setSettingValueCalled;

        assert(retAlt == retNeu);
        assert(altCalled == neuCalled);
        assert(altName == neuName);
        assert(altValue == neuValue);
        assert(altErrors == neuErrors);
        checks++;
    }

    std::printf("\nAll %d checks passed.\n", checks);
    return 0;
}
