// The park analyser's messages (main app, unit w161, src/ParkMessages.cpp): CMsgAddToChallenge (type 0x1a, posted by CParkAnalyser::PersonPaidAdmission),
// CMsgResearchCompleted (type 0x11, handled by CParkAnalyser::ReceiveMessage) and CMsgStaffInfo (type 0x17, the same). Each has the constructor, GetType and
// destructor of the pattern of include/ThingMessages.h: the Mac constructor stores the base vtable, a zero flags word, the message's own vtable and the payload; the
// destructor only resets the vtable and, for a positive (signed 16 bit) delete flag, calls operator delete. The Mac vtables (base 0x102a10ec, CMsgAddToChallenge
// 0x102a0554, CMsgResearchCompleted 0x102a1118, CMsgStaffInfo 0x102a11fc) are never used by the host. The payload names are mine (the binary has none).
#pragma once
#include <cstdint>

#include "CMsg.h"
#include "ThingParts2.h"      // ChallengeType, StaffType
#include "ThingReceivers.h"   // ResearchCat
#include "analyser.h"         // tag_IDX_TYPE

// The staff message's situation (values unknown; CParkAnalyser::ReceiveMessage tests 2, which stamps mLastStrike, so 2 is "strike" by what it stamps).
enum StaffSituation : int32_t {};

class CMsgAddToChallenge : public CMsg {
public:
    CMsgAddToChallenge() = default;   // test subclasses only
    // 0x1015210c (60 bytes): flags 0, the type (+8), the value (+0xc) and the two ids (the high halves of r6 and r7 on the Mac) at +0x10 and +0x12.
    CMsgAddToChallenge(ChallengeType type, int32_t value, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj,
                       tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing);
    int32_t GetType() const override;   // 0x100ef1f4 (8 bytes), answers 0x1a; native vtable key function
    ~CMsgAddToChallenge() override;     // 0x100e4584 (76 bytes)
    ChallengeType mType;   // 0x08
    int32_t mValue;        // 0x0c
    uint16_t mObj;         // 0x10  (tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>)
    uint16_t mThing;       // 0x12  (tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>)
};

class CMsgResearchCompleted : public CMsg {
public:
    CMsgResearchCompleted() = default;   // test subclasses only
    CMsgResearchCompleted(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj, ResearchCat category);   // 0x10151cfc (44 bytes): flags 0, the id (+8), the category (+0xc)
    int32_t GetType() const override;   // 0x101244d0 (8 bytes), answers 0x11
    ~CMsgResearchCompleted() override;  // 0x1012412c (76 bytes)
    uint16_t mObj;           // 0x08
    ResearchCat mCategory;   // 0x0c
};

class CMsgStaffInfo : public CMsg {
public:
    CMsgStaffInfo() = default;   // test subclasses only
    CMsgStaffInfo(StaffType staff, StaffSituation situation);   // 0x10151f14 (36 bytes): flags 0, the staff type (+8), the situation (+0xc)
    int32_t GetType() const override;   // 0x1012c244 (8 bytes), answers 0x17
    ~CMsgStaffInfo() override;          // 0x1012b4f4 (76 bytes)
    StaffType mStaff;             // 0x08
    StaffSituation mSituation;    // 0x0c
};
