// CGame::Iterate (main app, 0x10149838, 168 bytes; unit w129).
#include <cstdint>

#include "Clock.h"
#include "Game.h"
#include "TPMain.h"

// A function-local static of Iterate in the original (value at 0x1037f130, guard byte at 0x1037f12c): the first call stores 1 into it, but the store
// below overwrites that in the same call, so it is a plain global here (its initial 1 is the first-call store).
int32_t gGameIterateFlag = 1;

// Whether bit 3 (value 8) of gTPWFlags was set is read BEFORE the game clock steps a frame and kept in the static AFTER it; the flags are read again for
// the restart test (a restart asks the application to quit: bit 1 of gTPWFlags, value 2, is set).
void CGame::Iterate() {
    const uint32_t flags = gTPWFlags;
    const bool eightWasSet = (flags & 8) != 0;
    gClock.mTime.NextFrame();
    gGameIterateFlag = eightWasSet;
    if (mRestart != 0) gTPWFlags |= 2;
}
