// Host-side repro for the USBHID::IsQueueEmpty() blind spot in
// src/Devices/USB/USBHID.cpp / src/Attacks/Ducky/DuckyPayload.cpp.
//
// Firmware code being modeled (unchanged logic, just reduced to plain
// counters so it builds on a host without TinyUSB/Arduino):
//
//   static std::queue<KEY_EVENT> eventsToProcess;               // keyboard
//   static std::queue<std::pair<uint8_t,uint16_t>> cdcEventsToProcess; // consumer control
//   static std::queue<MOUSE_EVENT> mouseEventsToProcess;        // mouse
//
//   bool USBHID::IsQueueEmpty() { return eventsToProcess.empty(); }   // <-- BUG: ignores the other two
//
//   void USBHID::loop(...) {
//     // sends at most ONE report per call, in this fixed priority:
//     // cdc first, then mouse, then keyboard -- exactly mirrors the real
//     // if/else-if chain in USBHID::loop()
//   }
//
//   void DuckyPayload::loop(...) {
//     if (timeToWait != 0) { /* waiting */ }
//     else if (!Devices::USB::HID.IsQueueEmpty()) { /* yield, do nothing */ }
//     else { /* execute + advance one more script line */ }
//   }
//
// A Ducky "MOUSE_JIGGLE" line (src/Attacks/Ducky/Extensions.h,
// handleMouseJiggle()) pushes two entries straight into
// mouseEventsToProcess and never touches eventsToProcess. Because
// IsQueueEmpty() only looks at eventsToProcess, the interpreter's yield
// gate is permanently blind to the mouse (and consumer-control) queues:
// a script consisting of consecutive MOUSE_JIGGLE lines (the natural way
// to write a continuous anti-idle jiggler, or the same lines expanded by
// a Ducky REPEAT) advances one line per interpreter tick regardless of
// how backed up mouseEventsToProcess already is, while USBHID::loop()
// only ever drains one report per tick -- the queue backlog grows
// without bound for as long as the script keeps producing faster than
// USB can physically drain (each HID report needs a full low-speed poll
// interval plus HID_WAIT_TIME on the device side).
//
// TEST A: buggy gate -- mouse queue backlog grows roughly linearly with
// script length/tick count, unbounded.
// TEST B: fixed gate (checks all three queues) -- backlog never exceeds
// one line's worth of production (<=2 mouse events), because the
// interpreter blocks until USBHID::loop() has drained everything before
// advancing to the next script line, restoring the "yield to other event
// processing jobs" contract the comment at the call site already claims.
//
// Build + run:
//   g++ -std=c++17 -O1 -g repro.cpp -o repro && ./repro

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <queue>
#include <utility>

struct KeyEvent { int dummy = 0; };
struct MouseEvent {
    int8_t xDelta = 0;
    int8_t yDelta = 0;
    MouseEvent() = default;
    MouseEvent(int8_t x, int8_t y) : xDelta(x), yDelta(y) {}
};

struct USBHIDModel
{
    std::queue<KeyEvent> eventsToProcess;
    std::queue<std::pair<uint8_t, uint16_t>> cdcEventsToProcess;
    std::queue<MouseEvent> mouseEventsToProcess;

#ifdef FIXED
    bool IsQueueEmpty() const
    {
        return eventsToProcess.empty() && cdcEventsToProcess.empty() && mouseEventsToProcess.empty();
    }
#else
    bool IsQueueEmpty() const
    {
        return eventsToProcess.empty(); // BUG: cdc/mouse queues ignored
    }
#endif

    void mouseMove(int8_t x, int8_t y)
    {
        mouseEventsToProcess.push({x, y});
    }

    // Mirrors USBHID::loop(): sends at most one report per tick,
    // priority cdc > mouse > keyboard, matching the real if/else-if chain.
    void loopTick()
    {
        if (!cdcEventsToProcess.empty())
        {
            cdcEventsToProcess.pop();
        }
        else if (!mouseEventsToProcess.empty())
        {
            mouseEventsToProcess.pop();
        }
        else if (!eventsToProcess.empty())
        {
            eventsToProcess.pop();
        }
    }

    size_t totalPending() const
    {
        return eventsToProcess.size() + cdcEventsToProcess.size() + mouseEventsToProcess.size();
    }
};

// Mirrors DuckyPayload::loop(): one interpreter step per call, gated on
// IsQueueEmpty() exactly like the real "else if (!IsQueueEmpty()) { yield }".
struct DuckyModel
{
    USBHIDModel &hid;
    size_t nextLine = 0;
    size_t totalLines;

    bool tick()
    {
        if (!hid.IsQueueEmpty())
        {
            return false; // yielded, no line executed
        }
        if (nextLine >= totalLines)
        {
            return false; // script finished
        }
        // MOUSE_JIGGLE handler body (handleMouseJiggle in Extensions.h)
        hid.mouseMove(1, 1);
        hid.mouseMove(-1, -1);
        nextLine++;
        return true;
    }
};

int main()
{
    const size_t SCRIPT_LINES = 50;
    const size_t TICKS = 200; // generous upper bound, script would finish quickly if throttled

    USBHIDModel hid;
    DuckyModel ducky{hid, 0, SCRIPT_LINES};

    size_t maxBacklog = 0;
    for (size_t t = 0; t < TICKS; ++t)
    {
        ducky.tick();
        hid.loopTick();
        maxBacklog = std::max(maxBacklog, hid.totalPending());
    }

    printf("%s gate: script lines executed=%zu/%zu, max queue backlog=%zu\n",
#ifdef FIXED
           "FIXED",
#else
           "BUGGY",
#endif
           ducky.nextLine, SCRIPT_LINES, maxBacklog);

#ifdef FIXED
    // Each jiggle line produces 2 events, drained 1/tick -- with the gate
    // correctly blocking, backlog never exceeds one line's own production.
    assert(maxBacklog <= 2);
    assert(ducky.nextLine == SCRIPT_LINES); // script still completes, just paced
    printf("OK: fixed IsQueueEmpty() keeps the mouse queue bounded.\n");
#else
    // Buggy gate: every tick executes a new line regardless of backlog,
    // production (2/tick) outruns draining (1/tick) -- backlog grows
    // roughly linearly and blows well past what one line could produce.
    assert(ducky.nextLine == SCRIPT_LINES); // whole script races ahead immediately
    assert(maxBacklog >= SCRIPT_LINES);     // unbounded growth, far beyond the <=2 the fixed gate holds to
    printf("CONFIRMED BUG: mouse queue backlog grows with script length (%zu, vs <=2 with the fix).\n",
           maxBacklog);
#endif

    return 0;
}
