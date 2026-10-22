// Host-compilable isolation of handleRunPayload()'s file-extension dispatch
// (src/Attacks/Ducky/Extensions.h, RUN_PAYLOAD Ducky handler).
//
// ALT = exact pre-fix logic: arg.ends_with(".ds") / arg.ends_with(".png"),
//       case-sensitive, no logging on no-match.
// NEU = post-fix logic: uppercase-transform comparison before the
//       ends_with() check, plus an explicit "unsupported" log branch.
//
// Side effects (setPayload()/setPayloadCmdLine()/log) are replaced with
// recorded calls so each branch taken can be asserted on without any
// Arduino/ESP32/USB/SD dependency.

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>

enum class Branch
{
    None,
    SetPayload,
    DisplayPng,
    Unsupported
};

struct Recorder
{
    Branch branch = Branch::None;
    std::string arg;
    std::string logMsg;
};

// --- ALT: byte-for-byte transcription of the pre-fix handleRunPayload() body ---
static Recorder handleRunPayload_ALT(const std::string &str)
{
    Recorder rec;
    const std::string arg = str.substr(str.find(' ') + 1);
    if (arg.ends_with(".ds"))
    {
        rec.branch = Branch::SetPayload;
        rec.arg = arg;
        // duckyFileParser.Restart() dropped here too -- DuckyInterpreter::Restart()
        // (vendored DuckScriptInterpreter@23aa3ba, src/DuckyParse.h) only resets
        // _lineNumber/_whileLoopLineNumbers/_callstack/_linesToIgnore, so the
        // duplicate call in the original was side-effect free either way.
    }
    else if (arg.ends_with(".png"))
    {
        rec.branch = Branch::DisplayPng;
        rec.arg = arg;
    }

    return rec;
}

// --- NEU: transcription of the post-fix handleRunPayload() body ---
static Recorder handleRunPayload_NEU(const std::string &str)
{
    Recorder rec;
    const std::string arg = str.substr(str.find(' ') + 1);

    std::string argUpper = arg;
    std::transform(argUpper.begin(), argUpper.end(), argUpper.begin(), ::toupper);

    if (argUpper.ends_with(".DS"))
    {
        rec.branch = Branch::SetPayload;
        rec.arg = arg;
    }
    else if (argUpper.ends_with(".PNG"))
    {
        rec.branch = Branch::DisplayPng;
        rec.arg = arg;
    }
    else
    {
        rec.branch = Branch::Unsupported;
        rec.logMsg = "RUN_PAYLOAD unsupported file type: " + arg;
    }

    return rec;
}

static const char *branchName(Branch b)
{
    switch (b)
    {
    case Branch::None: return "None (silent no-op)";
    case Branch::SetPayload: return "SetPayload";
    case Branch::DisplayPng: return "DisplayPng";
    case Branch::Unsupported: return "Unsupported (logged)";
    }
    return "?";
}

int totalChecks = 0;
int failedChecks = 0;

static void expect(const std::string &label, Branch actual, Branch expected)
{
    totalChecks++;
    const bool ok = (actual == expected);
    if (!ok) failedChecks++;
    std::cout << "[" << (ok ? "OK" : "FAIL") << "] " << label
              << " -> " << branchName(actual)
              << (ok ? "" : (std::string(" (expected ") + branchName(expected) + ")"))
              << "\n";
}

int main()
{
    std::cout << "=== ALT (vor Fix): case-sensitives ends_with(\".ds\")/(\" .png\") ===\n";

    // Fund: real-world mixed/upper-case filenames (as actually uploaded by
    // handleUpload() in WebServer.cpp, which writes the filename exactly as
    // supplied by the browser -- no lowercasing anywhere in the upload path)
    // silently fall through BOTH branches: ALT does nothing, no log line.
    expect("ALT RUN_PAYLOAD payload.ds",   handleRunPayload_ALT("RUN_PAYLOAD payload.ds").branch,   Branch::SetPayload);
    expect("ALT RUN_PAYLOAD payload.DS",   handleRunPayload_ALT("RUN_PAYLOAD payload.DS").branch,   Branch::None);
    expect("ALT RUN_PAYLOAD Payload.Ds",   handleRunPayload_ALT("RUN_PAYLOAD Payload.Ds").branch,   Branch::None);
    expect("ALT RUN_PAYLOAD image.png",    handleRunPayload_ALT("RUN_PAYLOAD image.png").branch,    Branch::DisplayPng);
    expect("ALT RUN_PAYLOAD image.PNG",    handleRunPayload_ALT("RUN_PAYLOAD image.PNG").branch,    Branch::None);
    expect("ALT RUN_PAYLOAD IMAGE.Png",    handleRunPayload_ALT("RUN_PAYLOAD IMAGE.Png").branch,    Branch::None);
    // Regression: already-valid lowercase names and an unrelated extension
    // (never any branch, with or without the fix -- must stay unchanged)
    expect("ALT RUN_PAYLOAD notes.txt",    handleRunPayload_ALT("RUN_PAYLOAD notes.txt").branch,    Branch::None);

    std::cout << "\n=== NEU (nach Fix): Grossschreibungs-unabhaengiger Vergleich ===\n";

    expect("NEU RUN_PAYLOAD payload.ds",   handleRunPayload_NEU("RUN_PAYLOAD payload.ds").branch,   Branch::SetPayload);
    expect("NEU RUN_PAYLOAD payload.DS",   handleRunPayload_NEU("RUN_PAYLOAD payload.DS").branch,   Branch::SetPayload);
    expect("NEU RUN_PAYLOAD Payload.Ds",   handleRunPayload_NEU("RUN_PAYLOAD Payload.Ds").branch,   Branch::SetPayload);
    expect("NEU RUN_PAYLOAD image.png",    handleRunPayload_NEU("RUN_PAYLOAD image.png").branch,    Branch::DisplayPng);
    expect("NEU RUN_PAYLOAD image.PNG",    handleRunPayload_NEU("RUN_PAYLOAD image.PNG").branch,    Branch::DisplayPng);
    expect("NEU RUN_PAYLOAD IMAGE.Png",    handleRunPayload_NEU("RUN_PAYLOAD IMAGE.Png").branch,    Branch::DisplayPng);
    expect("NEU RUN_PAYLOAD notes.txt",    handleRunPayload_NEU("RUN_PAYLOAD notes.txt").branch,    Branch::Unsupported);

    // Argument content itself must stay byte-identical (NOT upper-cased) --
    // only the comparison uses the uppercase copy, Ducky.setPayload()/
    // setPayloadCmdLine() must still receive the real on-disk filename.
    {
        totalChecks++;
        const auto rec = handleRunPayload_NEU("RUN_PAYLOAD Payload.Ds");
        const bool ok = (rec.arg == "Payload.Ds");
        if (!ok) failedChecks++;
        std::cout << "[" << (ok ? "OK" : "FAIL")
                  << "] NEU preserves original filename case for setPayload() arg: '"
                  << rec.arg << "'\n";
    }

    // Edge case already covered by a previous session's substr()-pattern
    // review: RUN_PAYLOAD with no argument at all. str.find(' ') == npos,
    // +1 wraps to 0, substr(0) returns the whole string "RUN_PAYLOAD" --
    // well-defined (substr(0) is always valid), not a crash, and correctly
    // falls into the new Unsupported branch instead of a silent no-op.
    expect("NEU RUN_PAYLOAD (kein Argument)", handleRunPayload_NEU("RUN_PAYLOAD").branch, Branch::Unsupported);

    std::cout << "\n" << totalChecks << " Pruefungen, " << failedChecks << " fehlgeschlagen.\n";
    if (failedChecks == 0)
    {
        std::cout << "ALLE CHECKS BESTANDEN\n";
    }
    assert(failedChecks == 0);
    return failedChecks == 0 ? 0 : 1;
}
