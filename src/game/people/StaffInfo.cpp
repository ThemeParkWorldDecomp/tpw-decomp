// StaffInfo.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of StaffInfo code, named after it.

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
#include <cstring>
#include <new>

// CStaffDB::CStaffDB and CStaffInfo::CStaffInfo (main app, SimTheme Park 2.04b3, StaffDB.cpp; unit w113).

// 0x10128390 (28 bytes), a leaf. Not written: mName (+4), mPayGrade (+8) and mTimeoutTime (+0x10).
CStaffInfo::CStaffInfo() {
    mTimeSig = 0;
    mType = 0;
    mSubType = 0;
    mValid = 0;
    mOnPointer = 0;
}

// StaffDB.cpp, main app, unit w226: staff records and the available staff pool.

char16_t gStaffGeneratedName[0x80]; // 0x1034c5e0, original name unknown
// __sinit_StaffDB_cpp, 0x1012a7b8 (28 bytes): constant initialisation on the host.
int32_t gStaffDBWord6E0 = 0x10000; // 0x1034c6e0, original name unknown
int32_t gStaffDBWord6E4 = 0; // 0x1034c6e4, original name unknown

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
