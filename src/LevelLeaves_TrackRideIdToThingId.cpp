// Unit v5124: TrackRideIdToThingId (0x10030020, 240 bytes), main app.
#include <cstdint>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "World.h"
#include "analyser.h"

// 0x1029cc10 (name invented): the thing id that ends a walk of the used list (halfword, 0 on the Mac's bss).
uint16_t gUsedListEnd;

// Walks the world's used list for the first kind 3 thing whose word at +0x24 is rideId; answers its id, or 0.
void TrackRideIdToThingId(uint16_t* out, int32_t rideId) {
    uint16_t id = gGameWorld->GetUsedThingHead().mIndex;
    while (id != gUsedListEnd) {
        uint16_t cur = id;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing->mKind == 3 && rideId == *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(thing) + 0x24)) {
            *out = thing->GetThingId().mIndex;
            return;
        }
        id = gGameWorld->GetNextUsedThing(thing).mIndex;
    }
    *out = 0;
}
