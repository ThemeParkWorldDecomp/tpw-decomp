// StaffDB.cpp (main app): the staff database. The world starts with one: CGame::ExitLevel destroys the world
// through the destructor (non-deleting, flag -1) and frees the block itself. Unit w81 translated the destructor, unit w113 the
// constructors. Unit w226 translates the records and the pool in StaffDB_pool.cpp.
#pragma once
#include <cstdint>

// One staff record (CStaffInfo, 0x14 bytes). Its destructor is the element destructor of the database's array. The field names
// come from CStaffInfo::Serialise 0x10128440; the constructor clears the ones marked.
class CStaffInfo {
public:
    CStaffInfo();    // 0x10128390 (28 bytes, src/game/people/StaffInfo.cpp)
    // Unit w226 (src/game/people/StaffInfo.cpp).
    ~CStaffInfo();   // 0x101283d4, 68 bytes
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused); // 0x10128440, 756 bytes
    void GetName(char16_t* buffer, int32_t size) const; // 0x10128768, 200 bytes
    void GenerateName(); // 0x10128860, 636 bytes
    // 0x10128b0c, 728 bytes: makes the staff member of slot `slot`.
    void Generate(int32_t category, int32_t slot);
    uint32_t GetSalary() const; // 0x10128e1c, 48 bytes
    void SetValid(bool valid, int32_t slot); // 0x10128e7c, 48 bytes

    uint32_t mType;          // 0x00  StaffType (cleared)
    uint32_t mName;          // 0x04  an int in the serialiser (not written)
    uint8_t mPayGrade;       // 0x08  (not written)
    uint8_t mSubType;        // 0x09  (cleared)
    uint8_t mValid;          // 0x0a  (cleared)
    uint8_t mOnPointer;      // 0x0b  (cleared)
    uint32_t mTimeSig;       // 0x0c  a CTimeSig (4 bytes; TimeSig.h cannot be included from here), cleared
    uint32_t mTimeoutTime;   // 0x10  (not written)
};
static_assert(sizeof(CStaffInfo) == 0x14, "a staff record is 0x14 bytes");

// The database: 0x20 staff records from +0, then the members CStaffDB::Serialise 0x10128ff4 names. The records are kept as bytes (the
// constructor and StaffDB_dtor.cpp view them through CStaffInfo): a member array of CStaffInfo would run ~CStaffInfo a second time
// after the destructor's own loop. The database is the first member of the world (CWorld::CWorld passes `this` itself).
class CStaffDB {
public:
    // Unit w143: CMMPlaceStaff::OnRelease passes 0 once placed, 1 if not.
    enum StaffFate : int32_t {};
    CStaffDB();     // 0x10128ed8 (108 bytes, src/game/people/StaffDB.cpp)
    // 0x10128f68 (104 bytes, src/game/people/StaffDB.cpp): destroys the 0x20 staff records, last first; the Mac's flag (r4, a positive 16-bit
    // value also frees `this`) has no host form.
    ~CStaffDB();

    // Unit w129 (src/game/people/StaffDB.cpp).
    // 0x10129298 (60 bytes): makes the opening pool of staff, then stamps the top-up clock (mF294).
    void Init();
    // 0x1012a10c (856 bytes): retires the valid, unclaimed staff whose time ran out (shifted left by 2, as turns), and when the top-up interval has gone by
    // makes new staff of the categories that are below their targets (chosen at random in proportion to how far below), up to the pool's room and the
    // balance's maximum, then refills the minimum pool and stamps the clock.
    void Update();
    uint8_t mRecords[0x280];            // 0x000  the 0x20 CStaffInfo records
    int32_t mPeopleInCat[5];            // 0x280  never written by the constructor
    uint32_t mF294;                     // 0x294  a CTimeSig (4 bytes), cleared
    uint8_t mOpeningStaffPoolGenerated; // 0x298  cleared
    uint8_t mStopProducing[5];          // 0x299  cleared (memset)
                                        // 0x29e and 0x29f: padding, never written

    // Unit w226 (src/game/people/StaffDB.cpp).
    // 0x10128ff4, 628 bytes: 0 on success, 1 on failure.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);
    CStaffInfo* GetStaff(uint32_t slot) const; // 0x101292f8, 36 bytes
    int32_t HireStaff(uint32_t slot); // 0x10129348, 996 bytes
    void EnforceCap(); // 0x10129758, 172 bytes
    void CountStaffTypes(); // 0x10129830, 436 bytes
    void EnsureMinimumAmountInPool(); // 0x10129a14, 356 bytes
    int32_t GenerateStaff(int32_t type); // 0x10129bb4, 128 bytes
    int32_t GetNumStaffOfType(int32_t type); // 0x10129c6c, 68 bytes
    int32_t GetNumStaff(); // 0x10129cec, 56 bytes
    void FateOfPickedUpStaff(StaffFate fate); // 0x10129d50, 136 bytes
    bool IsNameInUse(char16_t* name) const; // 0x10129e20, 700 bytes
    uint32_t GetFirstFreeSlot(); // 0x1012a48c, 56 bytes
    void GenerateOpeningStaffPool(); // 0x1012a4f8, 648 bytes
};
static_assert(sizeof(CStaffDB) == 0x2a0, "the staff database is 0x2a0 bytes");

// Unit w226 (src/game/people/StaffDB.cpp, src/game/people/StaffInfo.cpp), original names unknown.
extern char16_t gStaffNameScratch[0x80]; // 0x1034c4e0
extern char16_t gStaffGeneratedName[0x80]; // 0x1034c5e0
extern int32_t gStaffDBWord6E0; // 0x1034c6e0
extern int32_t gStaffDBWord6E4; // 0x1034c6e4
