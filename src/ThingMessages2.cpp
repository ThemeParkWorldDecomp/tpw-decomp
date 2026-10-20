// The ride messages (main app, unit w200): the constructors 0x10151998, 0x10151a14, 0x10151a98, 0x10151b1c and 0x10151e48, the GetType functions
// 0x10110f6c, 0x10110e84, 0x10110ec0, 0x10110e44, 0x10110f34 and 0x10110efc, and the deleting destructors 0x101064fc, 0x1010e524, 0x1010e5a0, 0x1010e4a4,
// 0x10107b28 and 0x1010aa34. They live together because the native vtable needs a key function (GetType); the pattern is src/ThingMessages.cpp's.
#include "ThingMessages2.h"

// The Mac writes the base vtable, a zero flags word, the message's own vtable and the thing id (the HIGH halfword of its register argument), in that order.
CMsgNewRide::CMsgNewRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

CMsgRideBrokenDown::CMsgRideBrokenDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

CMsgRideCondemned::CMsgRideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

CMsgRideWantsUpgrade::CMsgRideWantsUpgrade(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

CMsgBlockedExit::CMsgBlockedExit(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}

// CMsgRideDeleted's constructor (unit w210) is at the end of this file.
int32_t CMsgNewRide::GetType() const { return 6; }
int32_t CMsgRideBrokenDown::GetType() const { return 7; }
int32_t CMsgRideCondemned::GetType() const { return 8; }
int32_t CMsgRideWantsUpgrade::GetType() const { return 9; }
int32_t CMsgRideDeleted::GetType() const { return 10; }
int32_t CMsgBlockedExit::GetType() const { return 0x14; }

// Each Mac destructor resets the vtable and deletes the object for a positive flag (the C++ delete-expression does both).
CMsgNewRide::~CMsgNewRide() {}
CMsgRideBrokenDown::~CMsgRideBrokenDown() {}
CMsgRideCondemned::~CMsgRideCondemned() {}
CMsgRideWantsUpgrade::~CMsgRideWantsUpgrade() {}
CMsgRideDeleted::~CMsgRideDeleted() {}
CMsgBlockedExit::~CMsgBlockedExit() {}

// Unit w210: CObject::FinalDestroy builds this message on its stack and posts it; the id (the HIGH halfword of the register argument) goes to +8 as the others do.
CMsgRideDeleted::CMsgRideDeleted(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}
