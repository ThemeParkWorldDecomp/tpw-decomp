// The ride messages (main app). Unit w126 (src/game/Object.cpp) posts four of them: CMsgNewRide (type 6, constructor 0x10151998, GetType 0x10110f6c, destructor
// 0x101064fc), CMsgRideBrokenDown (7, 0x10151a14, 0x10110e84, 0x1010e524), CMsgRideCondemned (8, 0x10151a98, 0x10110ec0, 0x1010e5a0) and CMsgRideWantsUpgrade
// (9, 0x10151b1c, 0x10110e44, 0x1010e4a4). Each constructor writes the base vtable, a zero flags word, its own vtable and the thing id at +8 (the argument's
// two bytes are the HIGH halfword of the Mac's register); nothing else. Unit w200 (src/game/Object.cpp) translates the constructors, the GetType functions
// and the deleting destructors (76 bytes each: the vtable is reset and the block freed for a positive signed 16 bit flag; they are declared after GetType, so
// GetType stays the native vtable's key function, as in include/CMsgDeath.h). The translated CObject code builds the messages as locals and posts them; the unit
// tests that do not link src/game/Object.cpp, src/game/ui/MessageCentre.cpp fake the constructor, GetType and destructor. The Mac vtables are never used by the host.
// Unit w184 adds CMsgRideDeleted (type 10, GetType 0x10110f34, destructor 0x10107b28): only the type and the id at +8, which CHandyman::ReceiveMessage reads;
// unit w210 adds its constructor (0x10151ba4, 40 bytes: CObject::FinalDestroy posts it). Unit w200 adds CMsgBlockedExit (type 0x14, constructor 0x10151e48, GetType 0x10110efc, destructor 0x1010aa34).
#pragma once
#include <cstdint>

#include "CMsg.h"
#include "analyser.h"

class CMsgNewRide : public CMsg {
public:
    CMsgNewRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);   // 0x10151998 (40 bytes)
    int32_t GetType() const override;   // 0x10110f6c, answers 6
    ~CMsgNewRide() override;            // 0x101064fc (76 bytes)
    uint16_t mThingId;                  // 0x08 on the Mac
};
class CMsgRideBrokenDown : public CMsg {
public:
    CMsgRideBrokenDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);   // 0x10151a14 (40 bytes)
    int32_t GetType() const override;   // 0x10110e84, answers 7
    ~CMsgRideBrokenDown() override;     // 0x1010e524 (76 bytes)
    uint16_t mThingId;                  // 0x08
};
class CMsgRideCondemned : public CMsg {
public:
    CMsgRideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);   // 0x10151a98 (40 bytes)
    int32_t GetType() const override;   // 0x10110ec0, answers 8
    ~CMsgRideCondemned() override;      // 0x1010e5a0 (76 bytes)
    uint16_t mThingId;                  // 0x08
};
class CMsgRideWantsUpgrade : public CMsg {
public:
    CMsgRideWantsUpgrade(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);   // 0x10151b1c (40 bytes)
    int32_t GetType() const override;   // 0x10110e44, answers 9
    ~CMsgRideWantsUpgrade() override;   // 0x1010e4a4 (76 bytes)
    uint16_t mThingId;                  // 0x08
};
// Unit w184: a ride (a toilet, for the handyman) was deleted; the id at +8 is the deleted thing. Posted by CObject::FinalDestroy (unit w210).
class CMsgRideDeleted : public CMsg {
public:
    CMsgRideDeleted(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);   // 0x10151ba4 (40 bytes), unit w210
    CMsgRideDeleted() = default;        // host only (the Mac has none): the unit tests that derive a message from it (tests/unit_Handyman.cpp, tests/unit_Staff.cpp) build it with no id
    int32_t GetType() const override;   // 0x10110f34, answers 10
    ~CMsgRideDeleted() override;        // 0x10107b28 (76 bytes), unit w200
    uint16_t mThingId;                  // 0x08
};
// Unit w200: the ride's exit is blocked (type 0x14); CObject::NotifyExitBlocked posts it for the ride whose thing id is at +8.
class CMsgBlockedExit : public CMsg {
public:
    CMsgBlockedExit(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);   // 0x10151e48 (40 bytes)
    int32_t GetType() const override;   // 0x10110efc, answers 0x14
    ~CMsgBlockedExit() override;        // 0x1010aa34 (76 bytes)
    uint16_t mThingId;                  // 0x08
};
// Unit w189 (src/game/people/staff.cpp): a staff member entered or left a room (type 15). The constructor is inline (no Mac address of its own is known); the Mac's
// destructor is 0x1012622c (the deleting form: the vtable is reset, the block freed for a positive flag).
class CMsgStaffRoomEvent : public CMsg {
public:
    CMsgStaffRoomEvent(uint16_t thing, int32_t arriving) {
        mFlags = 0;
        mThingId = thing;
        mArriving = arriving;
    }
    ~CMsgStaffRoomEvent() override;     // 0x1012622c (76 bytes)
    int32_t GetType() const override;   // 0x10127dfc, answers 15
    uint16_t mThingId;                  // 0x08
    int32_t mArriving;                  // 0x0c
};
// Unit v5631: the message of type 0xe that CAdvisor::ReceiveMessage (0x1000c874) reads; its constructor, GetType and destructor are not
// identified, so the class and field names are invented. The advisor asserts the thing table's row 0 is empty, takes the thing of row mThingId
// for its map cell id (1 + x + (y << 7)) and queues advisor message 0x7d, 0x7e or 0x7f for mKind 0, 1 or 2 (other kinds: nothing).
class CMsgThingAdvice : public CMsg {
public:
    uint16_t mThingId;                  // 0x08  the thing table row (the HIGH half of the word at +8 is the request's thing id)
    int32_t mKind;                      // 0x0c
};
