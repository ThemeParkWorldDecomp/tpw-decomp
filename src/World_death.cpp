// CWorld::AddToPendingDeleteList 0x1013b154 (48 bytes) and CWorld::GetRand 0x1013b1e4 (56 bytes), main app, unit w123c (the inventory files
// both under World.cpp).
#include "World.h"
#include "WorldThings.h"
#include "analyser.h"

// The id goes on the global pending-delete list (gPendingDeleteList, not a member of the world: the world pointer is never read). The
// list's AddHead 0x10141ad4 is an external callee (its specialization is declared in WorldThings.h, so the generic body is not built).
void CWorld::AddToPendingDeleteList(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { gPendingDeleteList.AddHead(id); }

// The generator is a linear congruence on the word at +0x1da708 (mStartTime, which SetupNewLevel seeds with time(NULL)): the new state is
// state * 0x19660d + 0x3c6ef35f in 32 bits. The original stores it twice (the same word) and reads it back. The answer is the state's
// absolute value taken as a signed word, but the stored state stays negative; 0x80000000 is its own negation. Unsigned arithmetic throughout:
// the signed overflow of the original is wrapping.
uint32_t CWorld::GetRand() {
    const uint32_t state = mStartTime * 0x0019660du + 0x3c6ef35fu;
    mStartTime = state;   // (stored twice and read back by the original: the repeat cannot show)
    return static_cast<int32_t>(state) >= 0 ? state : 0u - state;
}
