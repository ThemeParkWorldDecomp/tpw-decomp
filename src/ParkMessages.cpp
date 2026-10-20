// The park analyser's three messages (main app, unit w161): the constructors 0x1015210c (CMsgAddToChallenge), 0x10151cfc (CMsgResearchCompleted) and
// 0x10151f14 (CMsgStaffInfo), the GetType functions 0x100ef1f4, 0x101244d0 and 0x1012c244, the destructors 0x100e4584, 0x1012412c and 0x1012b4f4, and
// CBankAccount::GetBalance 0x100f32b4. They live together because the native vtable needs a key function (GetType); the pattern is src/ThingMessages.cpp's.
#include "ParkMessages.h"

#include "ThingReceivers.h"

// The Mac writes the base vtable, a zero flags word, the message's own vtable and then the payload, in that order.
CMsgAddToChallenge::CMsgAddToChallenge(ChallengeType type, int32_t value, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj,
                                       tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    mFlags = 0;
    mType = type;
    mValue = value;
    mObj = obj.mIndex;
    mThing = thing.mIndex;
}

CMsgResearchCompleted::CMsgResearchCompleted(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj, ResearchCat category) {
    mFlags = 0;
    mObj = obj.mIndex;
    mCategory = category;
}

CMsgStaffInfo::CMsgStaffInfo(StaffType staff, StaffSituation situation) {
    mFlags = 0;
    mStaff = staff;
    mSituation = situation;
}

int32_t CMsgAddToChallenge::GetType() const { return 0x1a; }
int32_t CMsgResearchCompleted::GetType() const { return 0x11; }
int32_t CMsgStaffInfo::GetType() const { return 0x17; }

// Each Mac destructor resets the vtable and deletes the object for a positive flag (the C++ delete-expression does both).
CMsgAddToChallenge::~CMsgAddToChallenge() {}
CMsgResearchCompleted::~CMsgResearchCompleted() {}
CMsgStaffInfo::~CMsgStaffInfo() {}

// 0x100f32b4 (8 bytes): the word at +0xc.
int32_t CBankAccount::GetBalance() const { return mBalance; }
