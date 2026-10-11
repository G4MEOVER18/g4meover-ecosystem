// Schlanker Host-Mock fuer USBHIDKeyboard.h — zeichnet press()/write()/print()/releaseAll()
// in ein globales Log auf, statt echten USB-HID-Traffic zu senden. KEY_*-Werte sind
// Platzhalter (nur Eindeutigkeit zaehlt), keine echten USB-HID-Usage-IDs noetig.
#pragma once
#include "Arduino.h"
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#define KEY_LEFT_CTRL    0x80
#define KEY_LEFT_SHIFT   0x81
#define KEY_LEFT_ALT     0x82
#define KEY_LEFT_GUI     0x83
#define KEY_RETURN       0xB0
#define KEY_ESC          0xB1
#define KEY_BACKSPACE    0xB2
#define KEY_TAB          0xB3
#define KEY_CAPS_LOCK    0xC1
#define KEY_F1           0xC2
#define KEY_INSERT       0xD1
#define KEY_HOME         0xD2
#define KEY_PAGE_UP      0xD3
#define KEY_DELETE       0xD4
#define KEY_END          0xD5
#define KEY_PAGE_DOWN    0xD6
#define KEY_RIGHT_ARROW  0xD7
#define KEY_LEFT_ARROW   0xD8
#define KEY_DOWN_ARROW   0xD9
#define KEY_UP_ARROW     0xDA

extern std::vector<std::string> g_log;

class USBHIDKeyboard {
public:
    void press(uint8_t k) {
        char b[32]; snprintf(b, sizeof(b), "PRESS:0x%02X", k);
        g_log.push_back(b);
    }
    void write(uint8_t k) {
        char b[32]; snprintf(b, sizeof(b), "WRITE:0x%02X", k);
        g_log.push_back(b);
    }
    void print(const String& s) {
        g_log.push_back(std::string("PRINT:") + s.c_str());
    }
    void println(const String& s) {
        g_log.push_back(std::string("PRINTLN:") + s.c_str());
    }
    void releaseAll() {
        g_log.push_back("RELEASE_ALL");
    }
};
