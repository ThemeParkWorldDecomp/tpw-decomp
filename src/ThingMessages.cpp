// The weather and challenge messages (main app, unit w124): the three constructors 0x101518c4, 0x10151870 and 0x10152008, the GetType functions
// 0x101399d0, 0x10139994 and 0x100f93b0, and the destructors 0x10138b9c, 0x10138b20 and 0x100f65c8. They live together because the native vtable
// needs a key function (GetType); the pattern is src/MsgDeath.cpp's.
#include "ThingMessages.h"

// The Mac writes the base vtable, a zero flags word, the message's own vtable and then the payload, in that order.
CMsgWeather::CMsgWeather(int32_t quality) {
    mFlags = 0;
    mQuality = quality;
}

CMsgWeatherWarning::CMsgWeatherWarning(int32_t days, int32_t quality) {
    mFlags = 0;
    mQuality = quality;
    mDays = days;
}

CMsgChallenge::CMsgChallenge(int32_t tagId, AdvisorMessage message, bool flag, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e,
                             tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj2,
                             StaffType staff) {
    mFlags = 0;
    mTagId = tagId;
    mMessage = message;
    mFlag = flag;
    mPrize = a;
    mTargetTime = b;
    mTargetVal = c;
    mDays = d;
    mValue = e;
    mObj = obj.mIndex;
    mObj2 = obj2.mIndex;
    mStaff = staff;
}

int32_t CMsgWeather::GetType() const { return 1; }
int32_t CMsgWeatherWarning::GetType() const { return 2; }
int32_t CMsgChallenge::GetType() const { return 0x19; }

// Each Mac destructor resets the vtable and deletes the object for a positive flag (the C++ delete-expression does both).
CMsgWeather::~CMsgWeather() {}
CMsgWeatherWarning::~CMsgWeatherWarning() {}
CMsgChallenge::~CMsgChallenge() {}
