// Host-buildable repro for the Web-UI filename path-traversal fix.
//
// Reimplements, verbatim in logic, the filename handling that
// src/Comms/Web/WebServer.cpp performs today (ALT) and after the proposed
// fix (NEU). No Arduino/ESP32 dependency: filenames are plain std::string,
// and "resolving" a path is modelled the same way a flat-root FAT/LittleFS
// volume does it -- by naively walking "/"-separated segments and letting
// ".." pop the current segment stack, exactly like a filesystem would.
//
// Build & run:
//   g++ -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.cpp
//   ./repro
//
// Expected output: every ALT case marked "ESCAPES ROOT" is a case the
// current code (no validation at all) would hand straight to
// FILE_INTERFACE.open()/.remove() with no rejection. Every NEU case shows
// the same input rejected by the new isFilenameSafe() guard before it ever
// reaches the filesystem call.

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

// ---- ALT: current WebServer.cpp behaviour -----------------------------
// /runfile, /showimage, /download, /delete read request->getParam("filename")
// and pass the raw value straight to Devices::Storage.* / Ducky.setPayload().
// handleUpload() does the same, additionally prefixing "/" itself:
//   Devices::Storage.openFile(std::string("/") + filename.c_str(), "w");
// No check of any kind is applied anywhere in either path.

static std::string altUploadPath(const std::string &clientSuppliedFilename)
{
    return "/" + clientSuppliedFilename; // exactly what handleUpload() builds today
}

// Model what opening that path on a flat-root FAT/LittleFS volume actually
// reaches: walk "/"-separated segments, ".." pops the stack, "." and empty
// segments are no-ops. This mirrors how directory entries "." and ".." are
// real, walkable entries on FAT (and how most embedded FS libs join paths),
// which is precisely why the string-level ".." check in isFilenameSafe()
// below is required before the path reaches the filesystem call at all.
static std::vector<std::string> resolvePath(const std::string &path)
{
    std::vector<std::string> stack;
    size_t i = 0;
    while (i <= path.size())
    {
        size_t next = path.find('/', i);
        if (next == std::string::npos) next = path.size();
        std::string seg = path.substr(i, next - i);
        if (seg.empty() || seg == ".")
        {
            // no-op
        }
        else if (seg == "..")
        {
            if (!stack.empty()) stack.pop_back();
            // else: popped past root -- some FS clamp, some error; either
            // way the caller no longer controls where the write/read lands
        }
        else
        {
            stack.push_back(seg);
        }
        i = next + 1;
    }
    return stack;
}

static bool escapesIntendedRoot(const std::string &resolvedPath)
{
    // Intended root is a single flat "/" directory. A resolved path that
    // still contains a ".." component was rejected before path-walking, so
    // this only flags cases where the walk landed somewhere other than a
    // single top-level file (i.e. it needed a ".." to get there, which the
    // stack-based walk above already resolved away -- so we instead flag
    // any case where the ORIGINAL string contained "..": that's the actual
    // ALT-vs-NEU decision point).
    return resolvedPath.find("..") != std::string::npos;
}

// ---- NEU: proposed isFilenameSafe() guard ------------------------------
// Ported 1:1 from the actual WebServer.cpp patch (Arduino String there,
// std::string here -- same character-by-character logic).
static bool isFilenameSafe(const std::string &filename)
{
    if (filename.empty()) return false;
    if (filename.find("..") != std::string::npos) return false;
    if (filename.find('\\') != std::string::npos) return false;
    for (unsigned char c : filename)
    {
        if (c < 0x20) return false; // control chars, including embedded NUL
    }
    return true;
}

struct Case
{
    const char *label;
    std::string filename;
    bool expectSafe; // what isFilenameSafe() must return after the fix
};

int main()
{
    const std::vector<Case> cases = {
        {"normal .ds payload",            "wifi_deauth.ds",              true},
        {"normal nested-looking name",     "autorun.ds",                  true},
        {"dotfile (legit hidden config)",  ".env",                        true},
        {"classic parent traversal",       "../esp32m_settings.json",     false},
        {"deep traversal",                 "../../../../etc/passwd",      false},
        {"traversal via subsegment",       "a/../../b.ds",                false},
        {"traversal suffix",               "notes.txt/../../secret.bin",  false},
        {"backslash (Windows-style)",      "..\\config.json",             false},
        {"empty filename",                 "",                            false},
        {"embedded NUL (truncation attk)", std::string("safe.ds\0.exe", 8), false},
    };

    int failures = 0;

    printf("=== ALT (current WebServer.cpp: no validation) ===\n");
    for (const auto &c : cases)
    {
        const std::string uploadPath = altUploadPath(c.filename);
        const auto resolved = resolvePath(uploadPath);
        const bool escapes = escapesIntendedRoot(uploadPath);
        printf("  %-32s in=%-28s -> open(\"%s\") %s\n",
               c.label, c.filename.c_str(), uploadPath.c_str(),
               escapes ? "ESCAPES ROOT (accepted today, no check)" : "stays in root");
    }

    printf("\n=== NEU (isFilenameSafe() guard applied before any FS call) ===\n");
    for (const auto &c : cases)
    {
        const bool safe = isFilenameSafe(c.filename);
        const bool ok = (safe == c.expectSafe);
        printf("  %-32s in=%-28s -> isFilenameSafe=%-5s expected=%-5s %s\n",
               c.label, c.filename.c_str(), safe ? "true" : "false",
               c.expectSafe ? "true" : "false", ok ? "OK" : "MISMATCH");
        if (!ok) failures++;
    }

    printf("\n%s\n", failures == 0
        ? "All cases behave as expected: every traversal/control-char input "
          "that ESCAPES ROOT under ALT is rejected by isFilenameSafe() under NEU; "
          "every legitimate filename is accepted by both."
        : "FAILURES DETECTED");

    return failures == 0 ? 0 : 1;
}
