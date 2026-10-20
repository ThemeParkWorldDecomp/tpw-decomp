#include <cstdint>
#include <cstdlib>
#include <new>
#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "MainBalance.h"
#include "RideDB.h"
#include "Serialise.h"
#include "Text.h"
#include "TbITable.h"
#include "TPSound.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

// The halfword constants the mechanic HQ compares ids with (Mac 0x102a0b9a to 0x102a0bae, all zero; names invented).
uint16_t gMechHQId102a0b9a = 0;
uint16_t gMechHQId102a0b9c = 0;
uint16_t gMechHQId102a0b9e = 0;
uint16_t gMechHQId102a0ba0 = 0;
uint16_t gMechHQId102a0ba2 = 0;
uint16_t gMechHQId102a0ba4 = 0;
uint16_t gMechHQId102a0ba6 = 0;
uint16_t gMechHQId102a0ba8 = 0;
uint16_t gMechHQId102a0baa = 0;
uint16_t gMechHQId102a0bac = 0;
uint16_t gMechHQId102a0bae = 0;

namespace {

constexpr const char* kThingZeroMsg = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

CObject* ObjectAt(uint16_t index) {
    return static_cast<CObject*>(gThingArray[index].mThing);
}

// The object row of a mechanic (CMechanic is plain data at the Mac's offsets).
CMechanic* MechanicAt(uint16_t index) {
    return static_cast<CMechanic*>(gThingArray[index].mThing);
}

void QueueAdvice(int32_t message, uint16_t id, uint16_t type) {
    CWorld* world = gWorld;
    CAdvisorRequest request(static_cast<AdvisorMessage>(message), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{id}, type, -1, false, -1);
    CAdvisor* advisor = world->GetAdvisor();
    advisor->QueueRequest(request);
}

}  // namespace

// 0x10105348
int32_t CMechanicHQ::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    auto* value = reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>*>(&mF0C);
    if (save != 0) {
        if (version < 2 && version >= 0 && SAVEVARSTRUCT(file, value, "mNextObject", 0) != 0) return 1;
    } else {
        if (version < 2 && version >= 0 && LOADVARSTRUCT(file, value, "mNextObject", 0) != 0) return 1;
    }
    return 0;
}

// 0x10105450
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMechanicHQ::GetNewJob(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mechanic) {
    if (mF0C == gMechHQId102a0b9a) {
        mF0C = gWorld->mF1DA746;
    } else {
        uint16_t cur = gWorld->mF1DA746;
        while (cur != gMechHQId102a0b9c && cur != mF0C) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
            cur = ObjectAt(cur)->mF0C;
        }
        if (cur == gMechHQId102a0b9e) {
            mF0C = gWorld->mF1DA746;
        } else {
            const uint16_t at = mF0C;
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
            mF0C = ObjectAt(at)->mF0C;
        }
    }
    const uint16_t start = mF0C;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
    CThing* mech = &ObjectAt(mechanic.mIndex)->mBase;
    uint32_t best = 0xffffffffu;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> found{0};
    if (mF0C == gMechHQId102a0ba0) return found;
    do {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
        CObject* ride = ObjectAt(mF0C);
        mF0C = ride->mF0C;
        if (mF0C == gMechHQId102a0ba2) mF0C = gWorld->mF1DA746;
        bool wanted = false;
        if (ride->IsBrokenDown()) {
            wanted = true;
        } else if (ride->IsFlaggedForMaintenance() != 0 && (ride->mFlags & 1) == 0) {
            wanted = true;
        } else if (ride->RequiresUpgrade() != 0) {
            wanted = true;
        }
        if (wanted) {
            bool mine = ride->GetAssignedStaffMember().mIndex == gMechHQId102a0ba4;
            if (!mine) mine = ride->GetAssignedStaffMember().mIndex == mechanic.mIndex;
            if (mine) {
                const uint32_t a = static_cast<uint16_t>(mech->mCellX + mech->mCellY * 0x80 + 1) - 1u;
                const uint32_t b = static_cast<uint16_t>(ride->mBase.mCellX + ride->mBase.mCellY * 0x80 + 1) - 1u;
                const int32_t dx = static_cast<int32_t>(labs(static_cast<int32_t>((a & 0x7f) - (b & 0x7f))));
                const int32_t dy = static_cast<int32_t>(labs(static_cast<int32_t>((a >> 7) - (b >> 7))));
                const uint32_t d = static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx) + static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy);
                if (d < best) {
                    best = d;
                    found = ride->mBase.GetThingId();
                }
            }
        }
    } while (mF0C != start);
    return found;
}

// 0x10105818
void CMechanicHQ::ReassignJobsAfterDrop(CMechanic& mechanic, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    uint16_t cur = gWorld->mF1DA746;
    while (cur != gMechHQId102a0ba6) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
        CObject* ride = ObjectAt(cur);
        bool wanted = false;
        if (ride->IsBrokenDown()) wanted = true;
        else if (ride->IsFlaggedForMaintenance() != 0) wanted = true;
        else if (ride->RequiresUpgrade() != 0) wanted = true;
        if (wanted) {
            if (ride->GetAssignedStaffMember().mIndex != gMechHQId102a0ba8) ride->UnassignStaffMember();
        }
        cur = ride->mF0C;
    }
}

// 0x10105988
void CMechanicHQ::ReceiveMessage(const CMsg& message) {
    if (message.GetType() != 7) {
        CTRACE(0x17, "Don't know how to handle message type %d\n", message.GetType());
        return;
    }
    const uint16_t id = static_cast<const CMsgRideBrokenDown&>(message).mThingId;
    uint16_t cur = gWorld->mF1DA73E;
    if (cur == gMechHQId102a0baa) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
        QueueAdvice(0x44, id, ObjectAt(id)->mObjectType);
        return;
    }
    bool none = true;
    while (cur != gMechHQId102a0bac) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
        CMechanic* mech = MechanicAt(cur);
        if (mech->mObjectToRepair.mIndex == id) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
            if (ObjectAt(id)->IsFlaggedForMaintenance() == 0) {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
                QueueAdvice(0x46, id, ObjectAt(id)->mObjectType);
            }
            none = false;
            break;
        }
        if (mech->mObjectToRepair.mIndex == gMechHQId102a0bae) {
            CStaff* staff = reinterpret_cast<CStaff*>(mech);
            if (staff->GetGeneralStaffState() != 3 && staff->GetGeneralStaffState() != 4) {
                none = false;
                break;
            }
        }
        cur = mech->mNext.mIndex;
    }
    if (none) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
        QueueAdvice(0x45, id, ObjectAt(id)->mObjectType);
    }
}
