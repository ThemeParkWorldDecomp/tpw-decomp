// CPeep (Peep.cpp), unit v5048: the stranded-area check, the particle effect and the event report.
#include <cstdint>

#include "Debug.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

char* Bytes(void* p) { return static_cast<char*>(p); }
const char* Bytes(const void* p) { return static_cast<const char*>(p); }

}  // namespace

// 0x101156fc: the area around the cell the peep got stranded in (the back of its queue when it stands in one) has not been touched since the peep
// became stranded (+0x198): the newest stamp of the nine blocks around the cell is still below that turn.
bool CPeep::IsLocalAreaUnchangedSinceBecomingStranded() {
    const uint8_t cellX = static_cast<uint8_t>(Bytes(this)[4]);
    const uint8_t cellY = static_cast<uint8_t>(Bytes(this)[6]);
    const uint16_t cell = static_cast<uint16_t>(cellX + (cellY << 7) + 1);
    const CMapCell* mapCell = reinterpret_cast<const CMapCell*>(Bytes(gWorldMap) + (static_cast<int32_t>(cell) - 1) * 0x44);
    uint16_t start;
    if (mapCell->IsQueue()) {
        uint16_t walk = static_cast<uint16_t>(cellX + (cellY << 7) + 1);
        uint16_t last = 0;   // the listing leaves it unwritten when the walk starts at the end cell (never the case: that cell id is 0)
        while (walk != gStrandedQueueEnd) {
            last = walk;
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> in{walk};
            walk = CObject::GetQCellBehind(in).mIndex;
        }
        start = last;
    } else {
        start = static_cast<uint16_t>(cellX + (cellY << 7) + 1);
    }
    const uint32_t stranded = *reinterpret_cast<const uint32_t*>(Bytes(this) + 0x198);
    uint32_t newest = 0;
    for (int32_t i = 0; i < 9 && newest <= stranded; ++i) {
        const uint16_t neighbour = static_cast<uint16_t>(gNeighbourSteps[i].mDx + (gNeighbourSteps[i].mDy << 7) + start);
        tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id{neighbour};
        CMap* map = reinterpret_cast<CMap*>(Bytes(gWorld) + 0x2d8);
        uint32_t stamp = map->GetTimeStamp(id);
        if (newest > stamp) stamp = newest;
        newest = stamp;
    }
    return stranded > newest;
}

// 0x101152a4: starts the emitter of an effect at the script position of the peep (x and z shifted by ten per height field start cell, the height
// from the height field), the position scaled by 1024.
void CPeep::SpawnParticleEffect(int32_t effect) {
    float x, y, z;
    SCR_GetScriptPosition(*reinterpret_cast<const int32_t*>(Bytes(this) + 0xc), &x, &y, &z, nullptr);
    const float ten = 10.0f;
    x = ppc_fmadds(ten, static_cast<float>(gWorld->GetHeightfieldXStart()), x);
    z = ppc_fmadds(ten, static_cast<float>(gWorld->GetHeightfieldYStart()), z);
    y = DetermineHFieldHeightQuick(x, z);
    const float scale = 1024.0f;
    const float sx = scale * x;
    const float sy = scale * y;
    const float sz = scale * z;
    PAR_InitEmitter(effect, ppc_fctiwz(sx), ppc_fctiwz(sy), ppc_fctiwz(sz));
}

namespace {

// The texts at 0x102439f5 + offset.
const char kRule[] = "------------------------------------------------";                       // 0x244
const char kHeader[] = "This is what this person did (most recent last):";                      // 0x275
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x2a6
const char kDeleted[] = "I did something with a ride that has been deleted: ";                  // 0x2e9

}  // namespace

// 0x10115c3c: traces the peep's 32 most recent events, the oldest first. x, y and the ride type are kept from one event to the next (registers of the
// original that are only rewritten for an event with a ride): they start at 0 here, the original's start unwritten.
void CPeep::ReportAllEvents() const {
    CTRACE(2, kRule);
    CTRACE(2, kHeader);
    uint32_t x = 0, y = 0, type = 0;
    const CThoughts* thoughts = reinterpret_cast<const CThoughts*>(Bytes(this) + 0x30);
    for (int32_t i = 31; i >= 0; --i) {
        const CThoughtEvent* event = thoughts->GetRecentEvent(static_cast<uint32_t>(i));
        const uint16_t thing = event->mThing;
        if (thing != gNoThoughtThing) {
            reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
            char* ride = Bytes(gThingArray[thing].mThing);
            if (ride == nullptr) {
                CTRACE(2, kDeleted);
                x = 0;
                y = 0;
                type = 0;
            } else {
                if (static_cast<uint8_t>(ride[2]) == 3) {
                    const uint16_t again = event->mThing;
                    reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                    type = *reinterpret_cast<const uint16_t*>(Bytes(gThingArray[again].mThing) + 0xe);
                }
                const uint16_t id1 = event->mThing;
                reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                {
                    const char* t = Bytes(gThingArray[id1].mThing);
                    const uint16_t cell = static_cast<uint16_t>(static_cast<uint8_t>(t[4]) + (static_cast<uint8_t>(t[6]) << 7) + 1);
                    x = static_cast<uint32_t>(static_cast<int32_t>(cell) - 1) & 0x7f;
                }
                const uint16_t id2 = event->mThing;
                reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                {
                    const char* t = Bytes(gThingArray[id2].mThing);
                    const uint16_t cell = static_cast<uint16_t>(static_cast<uint8_t>(t[4]) + (static_cast<uint8_t>(t[6]) << 7) + 1);
                    y = static_cast<uint32_t>(static_cast<int32_t>(cell) - 1) >> 7;
                }
            }
        }
        switch (event->mType) {
        case 1: CTRACE(2, "I found nothing to do. "); break;
        case 2: CTRACE(2, "I decided to visit ride at (%d, %d) of type %d. ", x, y, type); break;
        case 3: CTRACE(2, "I couldn't reach ride at (%d, %d) of type %d. ", x, y, type); break;
        case 4: CTRACE(2, "The ride at (%d, %d) of type %d was too exciting. ", x, y, type); break;
        case 5: CTRACE(2, "The ride at (%d, %d) of type %d was too boring. ", x, y, type); break;
        case 6: CTRACE(2, "The ride at (%d, %d) of type %d dismissed me from its queue. ", x, y, type); break;
        case 7: CTRACE(2, "I left the queue of ride at (%d, %d) of type %d, I got bored. ", x, y, type); break;
        case 8: CTRACE(2, "The ride at (%d, %d) of type %d was great!. ", x, y, type); break;
        case 9: CTRACE(2, "The ride at (%d, %d) of type %d was no good!. ", x, y, type); break;
        case 10: CTRACE(2, "The ride at (%d, %d) of type %d was too expensive! ", x, y, type); break;
        case 11: CTRACE(2, "I got a great costume from the ride at (%d, %d) of type %d. ", x, y, type); break;
        case 12: CTRACE(2, "I got a nice balloon from the ride at (%d, %d) of type %d. ", x, y, type); break;
        case 13: CTRACE(2, "The nasty man on the ride at (%d, %d) of type %d told me to get off! ", x, y, type); break;
        case 14: CTRACE(2, "I watched entertainer %d, he was top notch entertainment for sure. ", event->mThing); break;
        case 15: CTRACE(2, "I pranked. "); break;
        case 16: CTRACE(2, "I watched entertainer %d, he was top notch entertainment for sure. ", event->mThing); break;
        case 17: CTRACE(2, "I went to the toilet at (%d, %d). ", x, y); break;
        case 18: CTRACE(2, "I was sick! "); break;
        case 19: CTRACE(2, "It was time to go home. "); break;
        case 20: CTRACE(2, "Getting onto ride (%d, %d) of type %d. ", x, y, type); break;
        case 21: CTRACE(2, "There was no free queue space on ride (%d, %d) of type %d. ", x, y, type); break;
        case 22: CTRACE(2, "The queue for ride (%d, %d) of type %d moved, and I couldn't get to it any more. ", x, y, type); break;
        case 23: CTRACE(2, "On the way to my chosen ride I decided to pop into the ride at (%d, %d) of type %d", x, y, type); break;
        case 24: CTRACE(2, "I won on the sideshow at (%d, %d) of type %d! :D ", x, y, type); break;
        case 25: CTRACE(2, "I lost on the sideshow at (%d, %d) of type %d :( ", x, y, type); break;
        default: break;
        }
    }
    CTRACE(2, kRule);
}
