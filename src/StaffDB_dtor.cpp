// CStaffDB::~CStaffDB (main app, SimTheme Park 2.04b3, StaffDB.cpp; unit w81).
#include "StaffDB.h"

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
