// Unit v5918: CStaffHQ::Serialise 0x1012ac08, ReceiveMessage 0x1012a9e8, UpdateMonthly 0x1012ae40, FindNearestAvailableGuard 0x1012be78.
#include "Debug.h"
#include "RegionEffectCell.h"
#include "Serialise.h"
#include "ThingReceivers.h"
#include "World.h"

#include <cstdint>
#include <cstdlib>

// Halfwords of initialised data (all zero at start; names invented). The monthly check compares the world's staff list heads with
// them (index = staff type), ReceiveMessage compares the guard found with gStaffHQNoGuard, the guard walk ends at gStaffHQGuardListEnd.
uint16_t gStaffHQNoGuard;          // 0x102a11bc
uint16_t gStaffHQNoStaff[5];       // 0x102a11be, 0x102a11c0, 0x102a11c2, 0x102a11c4, 0x102a11c6
uint16_t gStaffHQGuardListEnd;     // 0x102a11dc

namespace {
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";                                     // 0x10245ff2
const char kAnySecurity[] = "Any security level will do, no probability yet - later, make prob. of detection prop. to sec. level\n";   // +0x43
const char kNoGuards[] = "No guards available to deal with pranker, he's got away with it!\n";                                      // +0xa8
const char kSiccing[] = "Siccing a guard on this pranker\n";                                                                      // +0xea
const char kNoSecurity[] = "There's no security level on this cell, this kid has got away with it!\n";                             // +0x10b
const char kParkClosed[] = "The park is closed, so I'm not considering striking\n";                                                // +0x185
const char kForceStrike[] = "mForceStrike";                                                                                       // +0x153
const char kPickedUp[] = "mStaffMemberPickedUp";                                                                                  // +0x160
const char kStrikeLevel[] = "mStrikeLevel[i]";                                                                                    // +0x175

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingIdTag;
typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> MapIdTag;

// (x + y * 128 + 1) as the 16 bit map cell id of a thing (its bytes at +4 and +6)
uint16_t CellOfThing(const CThing* thing) {
    return static_cast<uint16_t>(thing->mCellX + (static_cast<uint16_t>(thing->mCellY * 0x80) + 1));
}
}  // namespace

// 0x1012ac08 (520 bytes)
int32_t CStaffHQ::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (save != 0) {
        if (version >= 2 || version < 0) return 0;
        if (SAVEVAR(file, reinterpret_cast<int32_t*>(&mF64), kForceStrike, 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, reinterpret_cast<ThingIdTag*>(&mF60), kPickedUp, 0) != 0) return 1;
        for (int32_t i = 0; i < 5; ++i)
            if (SAVEVARSTRUCT(file, &mStrikes[i], kStrikeLevel, 0) != 0) return 1;
        if (mTraining.Serialise(file, save, version, unused) != 0) return 1;
    } else {
        if (version >= 2 || version < 0) return 0;
        if (LOADVAR(file, reinterpret_cast<int32_t*>(&mF64), kForceStrike, 0) != 0) return 1;
        if (LOADVARSTRUCT(file, reinterpret_cast<ThingIdTag*>(&mF60), kPickedUp, 0) != 0) return 1;
        for (int32_t i = 0; i < 5; ++i)
            if (LOADVARSTRUCT(file, &mStrikes[i], kStrikeLevel, 0) != 0) return 1;
        if (mTraining.Serialise(file, save, version, unused) != 0) return 1;
    }
    return 0;
}

// 0x1012a9e8 (492 bytes). Message type 12 is the monthly update, 14 a prank (the perp's thing id at +8); the rest are ignored.
void CStaffHQ::ReceiveMessage(const CMsg& message) {
    int32_t type = message.GetType();
    if (type == 13) return;
    if (type < 13) {
        if (type >= 12) UpdateMonthly();
        return;
    }
    if (type >= 15) return;
    uint16_t perp = static_cast<const CMsgStaffHQPrank&>(message).mPerp;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    MapIdTag cell;
    cell.mIndex = CellOfThing(static_cast<const CThing*>(gThingArray[perp].mThing));
    CRegionEffectCell* effect = gWorldMap->GetRegionalEffectCell(cell);
    if (effect->mpEffect[3] <= 0) {
        CTRACE(2, kNoSecurity);
        return;
    }
    CTRACE(2, kAnySecurity);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    MapIdTag where;
    where.mIndex = CellOfThing(static_cast<const CThing*>(gThingArray[perp].mThing));
    ThingIdTag guard = FindNearestAvailableGuard(where);
    if (guard.mIndex == gStaffHQNoGuard) {
        CTRACE(2, kNoGuards);
        return;
    }
    CTRACE(2, kSiccing);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    ThingIdTag perpTag;
    perpTag.mIndex = perp;
    static_cast<CGuard*>(gThingArray[guard.mIndex].mThing)->ProsecutePerp(perpTag);
}

// 0x1012ae40 (532 bytes): the park analyser is only asked when a strike is not forced
void CStaffHQ::UpdateMonthly() {
    mTraining.UpdateMonthly();
    if (mF64 == 0) {
        if (gWorld->IsParkClosed() != 0 || gWorld->GetParkAnalyser()->GetVisitorsInPark(5) != 0) {
            CTRACE(2, kParkClosed);
            return;
        }
    }
    for (int32_t i = 0; i < 5; ++i) {
        uint16_t head;
        switch (i) {
            case 0: head = gWorld->mF1DA73C; break;
            case 1: head = gWorld->mF1DA73E; break;
            case 2: head = gWorld->mF1DA740; break;
            case 3: head = gWorld->mF1DA744; break;
            default: head = gWorld->mF1DA742; break;
        }
        if (head == gStaffHQNoStaff[i]) continue;
        if (mStrikes[i].mF08 == gWorld->mTurn) continue;
        mStrikes[i].mF08 = gWorld->mTurn;
        if (IsStriking(static_cast<StaffType>(i)) != 0)
            StopStrikeInProgress(static_cast<StaffType>(i));
        else
            ConsiderStriking(static_cast<StaffType>(i));
    }
}

// 0x1012be78 (368 bytes; the result goes through a hidden pointer). The cell argument is never read: the distance is measured from
// this object's own cell bytes (+4 and +6), as the listing does.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CStaffHQ::FindNearestAvailableGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    (void)cell;
    uint32_t best = 0xffffffffu;
    uint16_t current = gWorld->mF1DA744;
    uint16_t found = 0;
    while (current != gStaffHQGuardListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CGuard* guard = static_cast<CGuard*>(gThingArray[current].mThing);
        uint32_t g = static_cast<uint32_t>(CellOfThing(&guard->mThing)) - 1u;
        uint32_t m = static_cast<uint32_t>(CellOfThing(&mBase)) - 1u;
        int32_t dx = static_cast<int32_t>(labs(static_cast<int32_t>((g & 0x7f) - (m & 0x7f))));
        int32_t dy = static_cast<int32_t>(labs(static_cast<int32_t>((g >> 7) - (m >> 7))));
        uint32_t dist = static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx) + static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy);
        bool available = guard->IsAvailable();
        if (available && dist < best) {
            found = current;
            best = dist;
        }
        current = guard->mNext.mIndex;
    }
    ThingIdTag result;
    result.mIndex = found;
    return result;
}
