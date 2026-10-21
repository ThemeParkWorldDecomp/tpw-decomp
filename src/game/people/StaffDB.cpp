// StaffDB.cpp: name proven by __sinit_StaffDB_cpp in the main application.

#include "Debug.h"
#include "helpers/StaffDBHelpers.h"
#include "HLSprite.h"
#include "MainBalance.h"
#include "Mouse.h"
#include "platform/msl.h"
#include "Serialise.h"
#include "StaffDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "UIInterface.h"
#include "World.h"
#include <cstdint>
#include <cstring>
#include <new>

// CStaffDB::CStaffDB and CStaffInfo::CStaffInfo (main app, SimTheme Park 2.04b3, StaffDB.cpp; unit w113).

// 0x10128ed8 (108 bytes). The Mac's __construct_array runs CStaffInfo::CStaffInfo on records 0 to 0x1f in ascending order (the
// cleanup descriptor ~CStaffInfo is only used if a constructor throws). mPeopleInCat (+0x280) and the padding at +0x29e are not written.
CStaffDB::CStaffDB() {
    CStaffInfo* const records = reinterpret_cast<CStaffInfo*>(mRecords);
    for (int i = 0; i < 0x20; ++i) {
        new (&records[i]) CStaffInfo;
    }
    mF294 = 0;
    mOpeningStaffPoolGenerated = 0;
    std::memset(mStopProducing, 0, 5);
}

// CStaffDB::~CStaffDB (main app, SimTheme Park 2.04b3, StaffDB.cpp; unit w81).

// 0x10128f68 (104 bytes). The Mac's destructor takes a flag in r4: with `this` not NULL it runs __destroy_arr over the 0x20 records of
// 0x14 bytes (descriptor 0x10266c88, whose code is CStaffInfo::~CStaffInfo 0x101283d4; the runtime goes from the last element to the first),
// then frees `this` when the flag, as a SIGNED 16-bit value, is above 0, and answers `this`. On the host the flag and the NULL test have no
// form: the callers of the non-deleting form call the destructor, a delete-expression gives the deleting one (and calls it for the block's
// 0x280 bytes, which the object's one-byte size does not show).
CStaffDB::~CStaffDB() {
    CStaffInfo* const records = reinterpret_cast<CStaffInfo*>(this);
    for (int i = 0x20; i > 0; --i) {
        records[i - 1].~CStaffInfo();
    }
}

// StaffDB.cpp, main app, unit w226: staff records and the available staff pool.

char16_t gStaffNameScratch[0x80]; // 0x1034c4e0, original name unknown
namespace {
constexpr uint16_t kNoStaffThing = 0; // 0x102a11b8
constexpr uint16_t kNoNamedThing = 0; // 0x102a11ba
constexpr const char* kBadThing = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
CStaffInfo* Record(const CStaffDB* db, uint32_t slot) {
    return reinterpret_cast<CStaffInfo*>(const_cast<uint8_t*>(db->mRecords) + 0x14 * slot);
}
CMouse* MakeMouse() {
    CMouse* mouse = new (std::nothrow) CMouse;
    if (mouse != nullptr) {
        CMMDefault* mode = new (std::nothrow) CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
    }
    return mouse;
}
}

// 0x10128ff4, 628 bytes. The category count and stop byte are interleaved.
int32_t CStaffDB::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (version < 0 || version >= 2) return 0;
    for (uint32_t slot = 0; slot < 32; ++slot) {
        if (Record(this, slot)->Serialise(file, save, version, unused)) return 1;
    }
    for (int32_t type = 0; type < 5; ++type) {
        if (Field(file, save, &mPeopleInCat[type], "mPeopleInCat[i]")) return 1;
        if (Field(file, save, reinterpret_cast<bool*>(&mStopProducing[type]), "mStopProducing[i]")) return 1;
    }
    if (reinterpret_cast<CTimeSig*>(&mF294)->Serialise(file, save, version, unused)) return 1;
    if (Field(file, save, reinterpret_cast<bool*>(&mOpeningStaffPoolGenerated), "mOpeningStaffPoolGenerated")) return 1;
    return 0;
}

// CStaffDB::Init (0x10129298, 60 bytes) and CStaffDB::Update (0x1012a10c, 856 bytes), main app, unit w129. The staff database is the first member of the
// world (the world's CWorldSavvy::GetWorld answers gWorld, which CTimeSig's inline members and the dice use).

namespace {
CTimeSig* Stamp(uint32_t* word) { return reinterpret_cast<CTimeSig*>(word); }
}  // namespace

// The opening pool is made, then the top-up clock is stamped with the world's turn.
void CStaffDB::Init() {
    GenerateOpeningStaffPool();
    Stamp(&mF294)->SetTimeSig();
}

// 0x101292f8, 36 bytes.
CStaffInfo* CStaffDB::GetStaff(uint32_t slot) const {
    return slot < 32 && Record(this, slot)->mValid != 0 ? Record(this, slot) : nullptr;
}

// 0x10129348, 996 bytes. The mouse takes the placement mode before the cap is enforced.
int32_t CStaffDB::HireStaff(uint32_t slot) {
    if (slot >= 32 || Record(this, slot)->mValid == 0) return 0;
    CStaffInfo* record = Record(this, slot);
    if (ValidType(record->mType)) {
        constexpr int32_t models[5] = {5, 4, 6, 7, 8};
        CMMPlaceStaff* place = new (std::nothrow) CMMPlaceStaff(models[record->mType], record->mPayGrade, record->mSubType, record->mName);
        if (gMouse == nullptr) gMouse = MakeMouse();
        gMouse->AssignMouseMode(place);
    }
    record->mOnPointer = 1;
    EnforceCap();
    return 1;
}

// 0x10129758, 172 bytes. The original does not bound-check the type used for the stop byte.
void CStaffDB::EnforceCap() {
    CountStaffTypes();
    for (uint32_t slot = 0; slot < 32; ++slot) {
        CStaffInfo* record = Record(this, slot);
        if (record->mValid != 0 && mStopProducing[record->mType] != 0 && record->mOnPointer == 0) {
            record->mValid = 0;
            gUIInterface.StaffDBChange(slot);
        }
    }
}

// 0x10129830, 436 bytes.
void CStaffDB::CountStaffTypes() {
    std::memset(mPeopleInCat, 0, 0x14);
    std::memset(mStopProducing, 0, 5);
    auto id = gWorld->GetUsedThingHead();
    while (id.mIndex != kNoStaffThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kBadThing);
        CThing* thing = static_cast<CThing*>(gThingArray[id.mIndex].mThing);
        int32_t type = -1;
        switch (thing->mKind) {
            case 5: type = 0; break;
            case 4: type = 1; break;
            case 6: type = 2; break;
            case 7: type = 3; break;
            case 8: type = 4; break;
        }
        if (type >= 0) {
            ++mPeopleInCat[type];
            if (mPeopleInCat[type] >= gMainBalance.mValues[kMainBalanceStaffMaxInPark + type].mInteger) mStopProducing[type] = 1;
        }
        id = gWorld->GetNextUsedThing(thing);
    }
}

// 0x10129a14, 356 bytes. A pool that cannot grow can leave this loop running forever.
void CStaffDB::EnsureMinimumAmountInPool() {
    constexpr int32_t order[5] = {1, 0, 2, 3, 4};
    bool shortfall;
    do {
        shortfall = false;
        for (int32_t type : order) {
            const int32_t available = GetNumStaffOfType(type);
            const int32_t total = static_cast<int32_t>(static_cast<uint32_t>(mPeopleInCat[type]) + static_cast<uint32_t>(available));
            if (total < gMainBalance.mValues[kMainBalanceStaffMinInPool + type].mInteger) {
                GenerateStaff(type);
                shortfall = true;
            }
        }
    } while (shortfall);
}

// 0x10129bb4, 128 bytes.
int32_t CStaffDB::GenerateStaff(int32_t type) {
    if (mStopProducing[type] != 0) return 0;
    const uint32_t slot = GetFirstFreeSlot();
    if (slot == 32) return 0;
    Record(this, slot)->Generate(type, slot);
    return 1;
}
// 0x10129c6c, 68 bytes.
int32_t CStaffDB::GetNumStaffOfType(int32_t type) {
    int32_t count = 0;
    for (uint32_t slot = 0; slot < 32; ++slot) {
        const CStaffInfo* record = Record(this, slot);
        if (record->mValid != 0 && record->mType == static_cast<uint32_t>(type)) ++count;
    }
    return count;
}
// 0x10129cec, 56 bytes.
int32_t CStaffDB::GetNumStaff() {
    int32_t count = 0;
    for (uint32_t slot = 0; slot < 32; ++slot) if (Record(this, slot)->mValid != 0) ++count;
    return count;
}
// 0x10129d50, 136 bytes.
void CStaffDB::FateOfPickedUpStaff(StaffFate fate) {
    for (uint32_t slot = 0; slot < 32; ++slot) {
        CStaffInfo* record = Record(this, slot);
        if (record->mOnPointer != 0) {
            if (fate == 0) {
                record->mValid = 0;
                gUIInterface.StaffDBChange(slot);
            }
            record->mOnPointer = 0;
        }
    }
}
// 0x10129e20, 700 bytes.
bool CStaffDB::IsNameInUse(char16_t* name) const {
    for (uint32_t slot = 0; slot < 32; ++slot) {
        const CStaffInfo* record = Record(this, slot);
        if (record->mValid != 0) {
            FormatName(record, gStaffNameScratch, 0x80);
            if (wcsncmp(name, gStaffNameScratch, UINT32_MAX) == 0) return true;
        }
    }
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    auto id = savvy->GetWorld().GetUsedThingHead();
    while (id.mIndex != kNoNamedThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kBadThing);
        CThing* thing = static_cast<CThing*>(gThingArray[id.mIndex].mThing);
        const uint8_t kind = thing->mKind;
        if (kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7) {
            if (wcsncmp(name, reinterpret_cast<CStaff*>(thing)->GetName(), UINT32_MAX) == 0) return true;
        }
        id = savvy->GetWorld().GetNextUsedThing(thing);
    }
    return false;
}

// Pass 1: each of the 0x20 records that is valid (byte 0xa nonzero) and has been idle for more than its timeout (byte 0x10, shifted left by 2: four turns
// a second), compared UNSIGNED, and is not on the pointer (byte 0xb zero) is retired (byte 0xa cleared) and the UI told its slot.
// Pass 2, only when the turns since the top-up clock was stamped (signed answer, compared unsigned) are more than the balance's interval shifted left by 2:
// the count of valid records; the most to make is the balance's maximum, replaced by 0x20 minus GetNumStaff when it is not below the room (unsigned
// compare); CountStaffTypes; for each of the five categories the need is 0 when its stop flag (byte 0x299 + category) is set, else the balance's target
// (read after the second of two GetNumStaffOfType calls, the first answer is ignored) minus the category's count; the total of the needs (32 bit wrap).
// Then while the most is above 0 and the total above 0 (signed): the dice answer r (gWorld), r % total picks a place in the needs, walked in category order
// (a need the pick is below, signed, is the category, otherwise the need is taken from the pick); a stopped category or no free record (0x20) ends the
// loop; else a staff member of the category is generated into the free record and the most, the total and the category's need go down by 1. The loop's
// category after a walk that finds none (only reachable when the needs wrapped) is the one of the previous round: the original keeps it in a register.
// Last, also when the loop did not run, EnsureMinimumAmountInPool and the clock stamp.
void CStaffDB::Update() {
    for (uint32_t slot = 0; slot < 0x20; ++slot) {
        CStaffInfo* record = reinterpret_cast<CStaffInfo*>(mRecords + 0x14 * slot);
        if (record->mValid != 0) {
            const int32_t turns = Stamp(&record->mTimeSig)->TurnsSinceLastUpdate();
            if (static_cast<uint32_t>(turns) > (record->mTimeoutTime << 2) && record->mOnPointer == 0) {
                record->mValid = 0;
                gUIInterface.StaffDBChange(static_cast<int32_t>(slot));
            }
        }
    }
    const uint32_t limit = static_cast<uint32_t>(gMainBalance.mValues[kMainBalanceStaffTopUpInterval].mInteger) << 2;
    const int32_t sinceTopUp = Stamp(&mF294)->TurnsSinceLastUpdate();
    if (static_cast<uint32_t>(sinceTopUp) > limit) {
        int32_t count = 0;
        for (uint32_t slot = 0; slot < 0x20; ++slot) {
            if (mRecords[0x14 * slot + 0xa] != 0) ++count;
        }
        int32_t most = gMainBalance.mValues[kMainBalanceStaffTopUpMax].mInteger;
        if (static_cast<uint32_t>(most) >= static_cast<uint32_t>(0x20 - count)) most = 0x20 - GetNumStaff();
        CountStaffTypes();
        int32_t need[5];
        for (int32_t k = 0; k < 5; ++k) {
            if (mStopProducing[k] != 0) {
                need[k] = 0;
            } else {
                GetNumStaffOfType(k);
                const int32_t have = GetNumStaffOfType(k);
                need[k] = static_cast<int32_t>(static_cast<uint32_t>(gMainBalance.mValues[kMainBalanceStaffTarget + k].mInteger) -
                                               static_cast<uint32_t>(have));
            }
        }
        int32_t total = static_cast<int32_t>(static_cast<uint32_t>(need[0]) + static_cast<uint32_t>(need[1]) + static_cast<uint32_t>(need[2]) +
                                             static_cast<uint32_t>(need[3]) + static_cast<uint32_t>(need[4]));
        int32_t category = 0;
        while (most > 0 && total > 0) {
            const uint32_t dice = gWorld->GetRand();
            int32_t pick = static_cast<int32_t>(dice % static_cast<uint32_t>(total));
            for (int32_t k = 0; k < 5; ++k) {
                if (pick < need[k]) {
                    category = k;
                    break;
                }
                pick = static_cast<int32_t>(static_cast<uint32_t>(pick) - static_cast<uint32_t>(need[k]));
            }
            if (mStopProducing[category] != 0) break;
            const uint32_t slot = GetFirstFreeSlot();
            if (slot == 0x20) break;
            reinterpret_cast<CStaffInfo*>(mRecords + 0x14 * slot)->Generate(category, static_cast<int32_t>(slot));
            --most;
            --total;
            --need[category];
        }
        EnsureMinimumAmountInPool();
        Stamp(&mF294)->SetTimeSig();
    }
}

// 0x1012a48c, 56 bytes.
uint32_t CStaffDB::GetFirstFreeSlot() {
    for (uint32_t slot = 0; slot < 32; ++slot) if (Record(this, slot)->mValid == 0) return slot;
    return 32;
}
// 0x1012a4f8, 648 bytes. The Mac returns 1 or 0, but the existing caller ignores it.
void CStaffDB::GenerateOpeningStaffPool() {
    if (mOpeningStaffPoolGenerated == 0) {
        for (int32_t type = 0; type < 5; ++type) {
            for (int32_t i = 0; i < gMainBalance.mValues[kMainBalanceStaffBeginningNumber + type].mInteger; ++i) {
                if (mStopProducing[type] == 0) {
                    const uint32_t slot = GetFirstFreeSlot();
                    if (slot != 32) Record(this, slot)->Generate(type, slot);
                }
            }
        }
        mOpeningStaffPoolGenerated = 1;
    } else {
        TRACE("Tried to generate the opening staff pool more than once!!\n");
    }
}
