// CoasterPanelEndTrack 0x1019c158 (344 bytes): makes sure the mouse exists, and unless the current mouse mode answers 8 (coaster),
// installs a new CMMCoaster as the mouse mode. The "make the mouse" sequence is inlined twice (the original's lazy gMouse creation).
#include <cstdint>
#include <new>

#include "Mouse.h"
#include "TPCoaster.h"

namespace coaster_end_callee {
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(size); }
}

// The constructors run on memory from the fake-able allocation (placement new keeps the order of the original's calls).
static CMouse* MakeMouse() {
    CMouse* mouse = static_cast<CMouse*>(coaster_end_callee::OperatorNew(1));
    CMMDefault* mode = new (coaster_end_callee::OperatorNew(8)) CMMDefault();
    mouse->AssignMouseMode(mode);
    gMouseWord1E0 = -1;
    gMouseWord1E4 = -1;
    return mouse;
}

void CoasterPanelEndTrack() {
    if (gMouse == nullptr) gMouse = MakeMouse();
    int32_t mode = 0;
    if (gMouseMode != nullptr) mode = gMouseMode->GetMode();
    if (mode != 8) {
        CMMCoaster* coaster = new (coaster_end_callee::OperatorNew(8)) CMMCoaster();
        if (gMouse == nullptr) gMouse = MakeMouse();
        gMouse->AssignMouseMode(coaster);
    }
}
