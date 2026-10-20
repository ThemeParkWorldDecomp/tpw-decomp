// StaffDB.cpp, main app, unit w226: staff records and the available staff pool.
#include "StaffDB.h"
#include "Debug.h"
#include "HLSprite.h"
#include "MainBalance.h"
#include "Mouse.h"
#include "Serialise.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "UIInterface.h"
#include "World.h"
#include "platform/msl.h"
#include <cstring>
#include <new>

char16_t gStaffNameScratch[0x80]; // 0x1034c4e0, original name unknown
char16_t gStaffGeneratedName[0x80]; // 0x1034c5e0, original name unknown
// __sinit_StaffDB_cpp, 0x1012a7b8 (28 bytes): constant initialisation on the host.
int32_t gStaffDBWord6E0 = 0x10000; // 0x1034c6e0, original name unknown
int32_t gStaffDBWord6E4 = 0; // 0x1034c6e4, original name unknown

namespace {
constexpr uint16_t kNoStaffThing = 0; // 0x102a11b8
constexpr uint16_t kNoNamedThing = 0; // 0x102a11ba
constexpr const char* kBadName = "Trying to get the name of a non-existent staffType!!!\n";
constexpr const char* kBadThing = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
CStaffInfo* Record(const CStaffDB* db, uint32_t slot) {
    return reinterpret_cast<CStaffInfo*>(const_cast<uint8_t*>(db->mRecords) + 0x14 * slot);
}
bool ValidType(uint32_t type) { return static_cast<int32_t>(type) >= 0 && static_cast<int32_t>(type) <= 4; }
void FormatName(const CStaffInfo* record, char16_t* buffer, int32_t size) {
    if (ValidType(record->mType)) {
        static_cast<TbITable*>(gText.mResourcePointers[11 + record->mType])->FormatString(buffer, size, record->mName);
    } else {
        TRACE(kBadName);
    }
}
template<class T> int32_t Field(void* file, int32_t save, T* value, const char* name) {
    return save != 0 ? SAVEVAR(file, value, name, 0) : LOADVAR(file, value, name, 0);
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

// 0x101283d4, 68 bytes. The Mac's positive signed halfword flag invokes delete;
// delete-expressions provide that operation on the host.
CStaffInfo::~CStaffInfo() {}

// 0x10128440, 756 bytes. Stop at the first failed field, in file order.
int32_t CStaffInfo::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (version < 0 || version >= 2) return 0;
    if (Field(file, save, reinterpret_cast<StaffType*>(&mType), "mType")) return 1;
    if (Field(file, save, reinterpret_cast<int32_t*>(&mName), "mName")) return 1;
    if (Field(file, save, &mPayGrade, "mPayGrade")) return 1;
    if (Field(file, save, &mSubType, "mSubType")) return 1;
    if (Field(file, save, reinterpret_cast<bool*>(&mValid), "mValid")) return 1;
    if (Field(file, save, reinterpret_cast<bool*>(&mOnPointer), "mOnPointer")) return 1;
    if (reinterpret_cast<CTimeSig*>(&mTimeSig)->Serialise(file, save, version, unused)) return 1;
    if (Field(file, save, reinterpret_cast<MacULong*>(&mTimeoutTime), "mTimeoutTime")) return 1;
    return 0;
}

// 0x10128768, 200 bytes.
void CStaffInfo::GetName(char16_t* buffer, int32_t size) const { FormatName(this, buffer, size); }

// 0x10128860, 636 bytes. Even a stale invalid-type buffer is checked for collisions.
void CStaffInfo::GenerateName() {
    int32_t tries = 0;
    do {
        if (ValidType(mType)) mName = gWorld->GetRand() % 35u;
        else TRACE("Trying to generate a name for a non-existent staff type!!!\n");
        FormatName(this, gStaffGeneratedName, 0x80);
        ++tries;
        if (!gWorld->mStaffDB.IsNameInUse(gStaffGeneratedName)) break;
    } while (tries < 15);
}

// 0x10128b0c, 728 bytes. The discarded draw is part of the generator's sequence.
void CStaffInfo::Generate(int32_t category, int32_t slot) {
    mType = static_cast<uint32_t>(category);
    uint8_t grade = 0; // Invalid categories use an uninitialised r26 in the Mac binary.
    if (ValidType(mType)) {
        constexpr int32_t sets[5] = {5, 6, 4, 7, 8};
        mSubType = static_cast<uint8_t>(ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(sets[category])));
        gWorld->GetRand();
        const uint32_t draw = gWorld->GetRand();
        const int8_t base = static_cast<int8_t>(gMainBalance.mValues[kMainBalanceStaffAvgGrade + category].mInteger);
        grade = static_cast<uint8_t>(draw % 3u + base - 1);
    } else {
        TRACE("Trying to generate a non-existent staff type!!!\n");
    }
    mPayGrade = grade > 4 ? 4 : grade;
    GenerateName();
    SetValid(true, slot);
    reinterpret_cast<CTimeSig*>(&mTimeSig)->SetTimeSig();
    const int32_t timeout = gMainBalance.mValues[kMainBalanceStaffTimeoutTime].mInteger;
    const uint32_t draw = gWorld->GetRand();
    mTimeoutTime = static_cast<uint32_t>(timeout) + draw % static_cast<uint32_t>(timeout >> 1);
}

// 0x10128e1c, 48 bytes.
uint32_t CStaffInfo::GetSalary() const { return CStaff::GetSalary(static_cast<StaffType>(mType), mPayGrade); }
// 0x10128e7c, 48 bytes.
void CStaffInfo::SetValid(bool valid, int32_t slot) {
    mValid = valid;
    gUIInterface.StaffDBChange(slot);
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
