// World.cpp (main app): CWorld::GetAdvisor, 0x1013eab0.
#include <cstddef>

#include "Debug.h"
#include "World.h"

static_assert(offsetof(CWorld, mAdvisorThing) == 0x1da71c, "the advisor's row index sits at +0x1da71c");

// The advisor is a thing: the world keeps its row of the thing table in a short (read before the check, as the Mac does). Entry 0 must stay
// free (an index of 0 means "no thing"); the check only reports. The answer is the row's first word, which is 0 when the row is free
// (a world whose advisor was never made answers NULL), and the index is not checked against the table's size.
CAdvisor* CWorld::GetAdvisor() {
    const uint16_t index = mAdvisorThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    return static_cast<CAdvisor*>(gThingArray[index].mThing);
}
