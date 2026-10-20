// Unit v5121: COnlineControl's VoteForThisPark, EarmuffsOn, EarmuffsOff, GetPerson, SetDestination (two), Teleport (two).
#include <cstdint>
#include <string>

#include "analyser.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"
#include "online.h"

uint16_t gNullThingId = 0;   // 0x102b1d64

namespace {

// The three-word Argument the commands push: 0 the char string, 4 the wide string, 8 the command number (see online.h, unit v5023).
void SendCommand(COnlineControl* self, uint32_t command) {
    uint32_t vec[3] = {0, 0, 0};
    uint32_t argument[3];
    online_callee::StringCtor(&argument[0]);
    online_callee::WStringCtor(&argument[1]);
    argument[2] = command;
    online_callee::ArgumentVectorPushBack(vec, argument);
    online_callee::WStringDtor(&argument[1], -1);
    online_callee::StringDtor(&argument[0], -1);
    self->DoCommand(vec);
    online_callee::ArgumentVectorTearDown(vec);
}

const char kIndexZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10251361

}  // namespace

// 0x10218b44
void COnlineControl::VoteForThisPark() {
    SendCommand(this, 0x15);
}

// 0x10219424
void COnlineControl::EarmuffsOn() {
    if (reinterpret_cast<uint8_t*>(this)[0xe50] == 0) SendCommand(this, 0xc);
}

// 0x1021952c
void COnlineControl::EarmuffsOff() {
    if (reinterpret_cast<uint8_t*>(this)[0xe50] != 0) SendCommand(this, 0xc);
}

// 0x1021969c
COnlinePerson* COnlineControl::GetPerson(std::u16string& name) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> cur = gGameWorld->GetUsedThingHead();
    while (cur.mIndex != gNullThingId) {
        uint16_t index = cur.mIndex;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kIndexZeroMessage);
        CThing* thing = static_cast<CThing*>(gThingArray[index].mThing);
        if (thing->mKind == 0x12) {
            COnlinePerson* person = reinterpret_cast<COnlinePerson*>(thing);
            if (person->IsThisMyName(name)) return person;
        }
        cur = gGameWorld->GetNextUsedThing(thing);
    }
    return nullptr;
}

// 0x102197ec
bool COnlineControl::SetDestination(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing,
                                    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kIndexZeroMessage);
    CThing* t = static_cast<CThing*>(gThingArray[thing.mIndex].mThing);
    if (t->mKind != 0x12) return false;
    return reinterpret_cast<COnlinePerson*>(t)->SetDest(cell) != 0;
}

// 0x10219908
bool COnlineControl::SetDestination(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kIndexZeroMessage);
    CThing* t = static_cast<CThing*>(gThingArray[thing.mIndex].mThing);
    if (t->mKind != 0x12) return false;
    return reinterpret_cast<COnlinePerson*>(t)->SetDest(x, y) != 0;
}

// 0x10219a10: the cell number minus one splits into a low part of 7 bits (x) and the rest (y), each with the offset byte 0x80.
void COnlineControl::Teleport(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing,
                              tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    uint32_t v = static_cast<uint32_t>(cell.mIndex) - 1u;
    MapPos x{static_cast<uint8_t>(v & 0x7f), 0x80};
    MapPos y{static_cast<uint8_t>(v >> 7), 0x80};
    Teleport(thing, x, y);
}

// 0x10219af4
// The original answers whether the thing is an online person; the header declares void, so the answer lives in this helper.
bool OnlineControlTeleportThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kIndexZeroMessage);
    CThing* t = static_cast<CThing*>(gThingArray[thing.mIndex].mThing);
    if (t->mKind != 0x12) return false;
    reinterpret_cast<COnlinePerson*>(t)->Teleport(x, y);
    return true;
}

void COnlineControl::Teleport(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y) {
    OnlineControlTeleportThing(thing, x, y);
}
