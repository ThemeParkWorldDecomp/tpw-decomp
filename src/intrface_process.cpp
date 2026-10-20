// intrface_process.cpp (main, unit w131): CIntrface::Process 0x1007db78 (24 bytes).
#include "intrface.h"

// Counts the call (the word at +0xc, read before anything is written) and clears the word at +0.
void CIntrface::Process() {
    const int32_t count = mProcessCount;
    mPointedThing = nullptr;
    mProcessCount = count + 1;
}
