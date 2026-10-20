// CStaffDB::CStaffDB and CStaffInfo::CStaffInfo (main app, SimTheme Park 2.04b3, StaffDB.cpp; unit w113).
#include "StaffDB.h"

#include <cstring>
#include <new>

// 0x10128390 (28 bytes), a leaf. Not written: mName (+4), mPayGrade (+8) and mTimeoutTime (+0x10).
CStaffInfo::CStaffInfo() {
    mTimeSig = 0;
    mType = 0;
    mSubType = 0;
    mValid = 0;
    mOnPointer = 0;
}

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
