// The weather and challenge messages (unit w124, src/game/ui/MessageCentre.cpp, src/game/park/Weather.cpp, src/game/park/ChallengeHQ.cpp): CMsgWeather (type 1), CMsgWeatherWarning (type 2) and CMsgChallenge (type 0x19),
// made by CWeather::UpdateTurn and CChallengeHQ::UpdateTurn. Each has the constructor, GetType and destructor of CMsgDeath (include/CMsgDeath.h): the
// Mac's destructor only stores the vtable and, for a positive (signed 16 bit) delete flag, calls operator delete. The Mac vtables (base 0x102a10ec,
// CMsgWeather 0x102a45ec, CMsgWeatherWarning 0x102a45c8, CMsgChallenge 0x102a08d4) are never used by the host. The payload names are inferred.
#pragma once
#include <cstdint>

#include "AdvisorScoring.h"
#include "CMsg.h"
#include "ThingParts2.h"
#include "analyser.h"

class CMsgWeather : public CMsg {
public:
    CMsgWeather() = default;   // test subclasses only
    CMsgWeather(int32_t quality);   // 0x101518c4 (32 bytes): flags 0 and the quality
    int32_t GetType() const override;   // 0x101399d0 (8 bytes), answers 1; native vtable key function
    ~CMsgWeather() override;            // 0x10138b9c (76 bytes)
    int32_t mQuality;   // 0x08
};

class CMsgWeatherWarning : public CMsg {
public:
    CMsgWeatherWarning() = default;   // test subclasses only
    CMsgWeatherWarning(int32_t days, int32_t quality);   // 0x10151870 (36 bytes): flags 0, the quality (+8) and the days (+0xc)
    int32_t GetType() const override;   // 0x10139994 (8 bytes), answers 2
    ~CMsgWeatherWarning() override;     // 0x10138b20 (76 bytes)
    int32_t mQuality;   // 0x08
    int32_t mDays;      // 0x0c
};

class CMsgChallenge : public CMsg {
public:
    CMsgChallenge() = default;   // test subclasses only
    // 0x10152008 (88 bytes). The object ids ride in the first two bytes of their stack words, the staff type is a whole word. The names of the five
    // numbers (a to e: the prize, the target time, the target value, the days left and the value so far) are inferred from CChallengeHQ::UpdateTurn.
    CMsgChallenge(int32_t tagId, AdvisorMessage message, bool flag, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e,
                  tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj2,
                  StaffType staff);
    int32_t GetType() const override;   // 0x100f93b0 (8 bytes), answers 0x19
    ~CMsgChallenge() override;          // 0x100f65c8 (76 bytes)
    int32_t mTagId;          // 0x08
    AdvisorMessage mMessage; // 0x0c
    uint8_t mFlag;           // 0x10  (0x11 to 0x13: never written)
    int32_t mPrize;          // 0x14  "a"
    int32_t mTargetTime;     // 0x18  "b"
    int32_t mTargetVal;      // 0x1c  "c"
    int32_t mDays;           // 0x20  "d"
    int32_t mValue;          // 0x24  "e"
    uint16_t mObj;           // 0x28
    uint16_t mObj2;          // 0x2a
    int32_t mStaff;          // 0x2c
};
