// Host-side repro for the Marauder command-mailbox bug in
// src/Attacks/Marauder/Marauder.cpp (ESP32Marauder::run() / ESP32Marauder::loop()).
//
// Firmware code being modeled (unchanged logic, just renamed for a host build):
//
//   static std::string nextMarauderCommandToRun;
//   static bool marauderActivated = false;
//
//   void run(const std::string &cmd) {                 // called from:
//     marauderActivated = true;                         //  - WebServer.cpp /marauder handler
//     nextMarauderCommandToRun = cmd;                    //    (runs on the AsyncTCP task)
//   }                                                    //  - Ducky Extensions.h MARAUDER line
//                                                         //  - EvilCrowWind::loop() thermal
//                                                         //    safety cutoff: run("stopscan")
//   void loop() {                                        // called every Arduino loop()
//     if (marauderActivated) {                           // iteration from the main loop task
//       if (!nextMarauderCommandToRun.empty()) {
//         cli_obj.runCommand(nextMarauderCommandToRun.c_str());
//         nextMarauderCommandToRun.clear();
//       }
//     }
//   }
//
// Two separate, independently confirmable problems:
//
// TEST A (deterministic, no threads needed): the mailbox holds exactly one
// pending command. Any run() call that lands before the previous one has
// been drained by loop() overwrites it outright -- the earlier command is
// dropped and NEVER executed, with no error, no log line, nothing. This
// matters concretely for EvilCrowWind::loop(), which calls
// run("stopscan") as the *only* action taken to stop an overheating scan;
// if a web request or a running Ducky script issues another MARAUDER
// command in the same window, the safety cutoff can be silently discarded.
//
// TEST B (needs ThreadSanitizer): nextMarauderCommandToRun/marauderActivated
// are plain globals, read in ESP32Marauder::loop() (Arduino main loop task)
// and written in ESP32Marauder::run() -- which is reachable from the
// AsyncWebServer request handler. ESPAsyncWebServer/AsyncTCP process
// requests from their own FreeRTOS task (see platformio.ini:
// CONFIG_ASYNC_TCP_RUNNING_CORE=1), a genuinely different task than the
// Arduino loop() task, and FreeRTOS can preempt either task at any point.
// There is no mutex/critical section around the shared std::string, so a
// std::string assignment on one task can be observed mid-copy by the other
// -- undefined behaviour on a non-trivial object, and worse than the plain
// data loss in TEST A. Build with -fsanitize=thread to see it flagged.
//
// Build + run:
//   g++ -std=c++17 -O1 -g repro.cpp -o repro_a && ./repro_a          # TEST A (always deterministic)
//   g++ -std=c++17 -O1 -g -fsanitize=thread repro.cpp -o repro_b -DTHREAD_TEST -lpthread && ./repro_b   # TEST B, current (UNSAFE) mailbox
//   g++ -std=c++17 -O1 -g -fsanitize=thread repro.cpp -o repro_c -DTHREAD_TEST -DFIXED -lpthread && ./repro_c  # TEST B, with the proposed critical-section fix

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#ifdef FIXED
#include <mutex>
// Host stand-in for the firmware fix, which uses a portMUX_TYPE spinlock
// (portENTER_CRITICAL/portEXIT_CRITICAL) around the same three lines --
// a std::mutex gives the same "one accessor at a time" guarantee on host
// and is enough to make ThreadSanitizer stop flagging the access.
static std::mutex g_mailboxMutex;
#endif

// -------- exact mirror of the firmware statics + run()/loop() bodies --------
static std::string nextMarauderCommandToRun;
static bool marauderActivated = false;

static std::vector<std::string> executedCommands; // what actually reached cli_obj.runCommand()

static void run(const std::string &cmd)
{
#ifdef FIXED
    std::lock_guard<std::mutex> lock(g_mailboxMutex);
#endif
    marauderActivated = true;
    nextMarauderCommandToRun = cmd;
}

static void loopOnce()
{
#ifdef FIXED
    std::lock_guard<std::mutex> lock(g_mailboxMutex);
#endif
    if (marauderActivated)
    {
        if (!nextMarauderCommandToRun.empty())
        {
            executedCommands.push_back(nextMarauderCommandToRun); // stand-in for cli_obj.runCommand(...)
            nextMarauderCommandToRun.clear();
        }
    }
}
// ------------------------------------------------------------------------

#ifndef THREAD_TEST

// TEST A: two run() calls land before loop() ever drains the mailbox --
// exactly what happens if e.g. a web request and the thermal safety cutoff
// both fire inside the same Arduino loop() iteration, before
// ESP32Marauder::loop() itself runs.
int main()
{
    run("scanap");    // e.g. attacker/operator kicks off a scan via the web UI
    run("stopscan");  // e.g. EvilCrowWind::loop() thermal cutoff fires right after

    loopOnce(); // only ONE command is ever visible to cli_obj.runCommand()

    printf("commands actually executed: %zu\n", executedCommands.size());
    for (auto &c : executedCommands) printf("  - %s\n", c.c_str());

    assert(executedCommands.size() == 1);
    assert(executedCommands[0] == "stopscan"); // "scanap" was silently dropped, never ran

    // Bug confirmed either way round: whichever run() call happens to be
    // second always wins, with zero visibility into what got dropped.
    // In the thermal-cutoff case this can just as easily go the other way:
    executedCommands.clear();
    nextMarauderCommandToRun.clear();
    marauderActivated = false;

    run("stopscan");  // thermal cutoff fires first this time
    run("scanap");    // then a queued web request runs before loop() drains

    loopOnce();

    assert(executedCommands.size() == 1);
    assert(executedCommands[0] == "scanap"); // "stopscan" -- the safety cutoff -- was dropped

    printf("TEST A: confirmed -- single-slot mailbox silently drops whichever "
           "run() call is NOT the most recent one before the next loop() drain.\n");
    return 0;
}

#else

#include <atomic>
#include <thread>

// TEST B: run() (writer, models the AsyncTCP task handling /marauder and
// the main-loop-task callers) and loopOnce() (reader, models the Arduino
// loop() task) hammering the shared mailbox concurrently from two real
// OS threads, with no ordering between them beyond what the (missing, in
// the UNSAFE build) lock provides. Run under -fsanitize=thread: the
// UNSAFE build reports a data race on nextMarauderCommandToRun; the FIXED
// build (critical section around every access, mirroring the proposed
// portMUX_TYPE fix) does not.
int main()
{
    std::atomic<bool> stop{false};

    std::thread writer([&]() {
        for (int i = 0; i < 20000 && !stop; ++i)
        {
            run(i % 2 == 0 ? "scanap" : "stopscan");
        }
    });

    std::thread reader([&]() {
        for (int i = 0; i < 20000 && !stop; ++i)
        {
            loopOnce();
        }
    });

    writer.join();
    stop = true;
    reader.join();

    printf("TEST B finished, %zu commands observed by loop() "
           "(run under ThreadSanitizer to see the race report on the UNSAFE build)\n",
           executedCommands.size());
    return 0;
}

#endif
