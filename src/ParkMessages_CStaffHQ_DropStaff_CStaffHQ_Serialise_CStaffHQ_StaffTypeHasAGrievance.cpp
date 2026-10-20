#include "Debug.h"
#include "RegionEffectCell.h"
#include "Serialise.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"
#include <cstdint>
#include <cstdlib>

// 0x102a11c8 .. 0x102a11da (names invented): the id that ends each staff list the grievance check walks (all zero at start).
uint16_t gStaffHQGrievanceEnd[10];

namespace {

const char kAssertText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";  // 0x10245ff2
const char kFatigue[] = "%ss are striking through fatigue\n";                                       // +0x2c1
const char kType[] = "##Type";                                                                      // +0x2e3
const char kUnhappy[] = "%ss are striking through unhappiness\n";                                   // +0x2ea
const char kUnknown[] = "Unknown staff type in StaffTypeHasAGrievance\n";                           // +0x310

// Walks the list from head (linked by the halfword at +0x20c) and averages the float at `offset` as the low byte of its
// truncated value. Answers 1 when the list has more than three members and the average is below 15.
int32_t Walk(uint16_t head, uint16_t end, uint32_t offset, const char* text) {
    uint32_t count = 0;
    uint32_t sum = 0;
    for (uint16_t cur = head; cur != end;) {
        count++;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kAssertText);
        uint8_t* thing = reinterpret_cast<uint8_t*>(gThingArray[cur].mThing);
        float value = *reinterpret_cast<float*>(thing + offset);
        uint16_t next = *reinterpret_cast<uint16_t*>(thing + 0x20c);
        sum += static_cast<uint32_t>(ppc_fctiwz(value)) & 0xff;
        cur = next;
    }
    if (count > 3 && sum / count < 15) {
        CTRACE(2, text, kType);
        return 1;
    }
    return 0;
}

}  // namespace

int32_t CStaffHQ::StaffTypeHasAGrievance(StaffType type) {
    if (mF64 != 0) return 1;
    int32_t t = static_cast<int32_t>(type);
    uint16_t head;
    int32_t slot;
    switch (t) {
    case 0: {
        CParkAnalyser* analyser = gWorld->GetParkAnalyser();
        float walkable = static_cast<float>(analyser->GetNumWalkableCells(1));
        float littered = static_cast<float>(analyser->GetNumLitteredCells(1));
        if (littered / walkable > 0.2f) return 1;
        head = gWorld->mF1DA73C;
        slot = 0;
        break;
    }
    case 1: head = gWorld->mF1DA73E; slot = 2; break;
    case 2: head = gWorld->mF1DA740; slot = 4; break;
    case 3: head = gWorld->mF1DA744; slot = 6; break;
    case 4: head = gWorld->mF1DA742; slot = 8; break;
    default:
        GAME_ASSERTMSG(0, kUnknown);
        return 0;
    }
    if (Walk(head, gStaffHQGrievanceEnd[slot], 0x1f8, kFatigue)) return 1;
    if (Walk(head, gStaffHQGrievanceEnd[slot + 1], 0x1f4, kUnhappy)) return 1;
    return 0;
}
