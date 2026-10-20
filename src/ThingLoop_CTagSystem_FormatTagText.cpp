// CTagSystem::FormatTagText (unit v5626), cut from the neighbourhood of src/ThingLoop.cpp.
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <new>
#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "FS_Online.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "World.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingMessages.h"
#include "platform/msl.h"

uint16_t gTagNoThingId;    // 0x102a1214
uint16_t gTagNoRideType;   // 0x102a1216

namespace {

// The texts of the string pool at 0x10246451 (narrow) and 0x1024659a (wide).
const char kThingAssert[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";               // 0x1024650a
const char kNoRideTrace[] = "Object id is IDNULL in CTagSystem::FormatTagText()   (TVT_UNICODE_OBJNAME)\n";       // 0x1024654d
const char16_t kEmptyText[] = u"";                                                                                // 0x1024659a
const char16_t kThingText[] = u"A string needs to be put in here";                                               // 0x1024659c
const char16_t kPlainText[] = u"String and number goes here";                                                    // 0x102465de

// gText's resource pointers 5 (Mac 0x560) and 17 (Mac 0x590): the tag texts and the staff names.
const TbITable* TagStrings() { return static_cast<const TbITable*>(gText.mResourcePointers[5]); }
const TbITable* StaffStrings() { return static_cast<const TbITable*>(gText.mResourcePointers[17]); }

// Integer balance values by their Mac offsets in gMainBalance (0x750, 0x754, 0x760, 0x764, 0x768).
int32_t Balance(uint32_t macOffset) { return gMainBalance.mValues[(macOffset - 8) / 4].mInteger; }

// The staff name of the challenge's staff type (0 to 4: string 1, 3, 5, 7 or 9); another type leaves `name` as it was
// (uninitialised, as on the Mac).
void StaffName(const CMsgChallenge* challenge, char16_t* name) {
    switch (challenge->mStaff) {
    case 0: StaffStrings()->FormatString(name, 0x40, 1); break;
    case 1: StaffStrings()->FormatString(name, 0x40, 3); break;
    case 2: StaffStrings()->FormatString(name, 0x40, 5); break;
    case 3: StaffStrings()->FormatString(name, 0x40, 7); break;
    case 4: StaffStrings()->FormatString(name, 0x40, 9); break;
    default: break;
    }
}

TbVTag* V(TbVTag& tag) { return &tag; }

}  // namespace

// 0x1012cc3c, 4308 bytes. The integer tags carry the number's magnitude and sign on the Mac (TbIntegerVTag's constructor);
// the host tag keeps the signed number.
int32_t CTagSystem::FormatTagText(const TagTableElement* element, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing,
                                  uint32_t value, const CMsgChallenge* challenge) {
    char16_t* out = reinterpret_cast<char16_t*>(mF0C);
    if (element == nullptr) {
        swprintf(out, 0x400, kEmptyText);
        return 0;
    }
    const CMsgChallenge* c = challenge;
    switch (element->mFormat) {
    case 0:   // 0x1012ccc4: the text alone
        TagStrings()->FormatString(out, 0x400, element->mTextId);
        break;
    case 1: {   // 0x1012cce0: a thing that still exists
        if (thing.mIndex == gTagNoThingId) return 0;
        uint16_t index = thing.mIndex;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingAssert);
        if (gThingArray[index].mThing == nullptr) return 0;
        swprintf(out, 0x400, kThingText);
        break;
    }
    case 2: {   // 0x1012cd60: the ride type in the low half of `value`
        uint16_t type = static_cast<uint16_t>(value);
        if (type == gTagNoRideType) {
            TRACE(kNoRideTrace);
            return 0;
        }
        TbUniStringVTag name(0, gRideDatabase.GetTypeName(type));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(name));
        break;
    }
    case 3: {   // 0x1012cdf4: `value` as a number
        TbIntegerVTag number(1, static_cast<int32_t>(value));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(number));
        break;
    }
    case 4:   // 0x1012cddc
        swprintf(out, 0x400, kPlainText);
        break;
    case 5: {   // 0x1012ce54
        TbIntegerVTag number(1, Balance(0x750));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(number));
        break;
    }
    case 6: {   // 0x1012ceb8
        TbIntegerVTag number(1, Balance(0x754));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(number));
        break;
    }
    case 7: {   // 0x1012cf1c
        TbIntegerVTag number(1, Balance(0x760));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(number));
        break;
    }
    case 8: {   // 0x1012cf80
        TbIntegerVTag first(1, Balance(0x764));
        TbIntegerVTag second(2, Balance(0x768));
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(first), V(second));
        break;
    }
    case 9:
    case 11: {   // 0x1012d030
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag time(2, c->mTargetTime);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(time));
        break;
    }
    case 10:
    case 14: {   // 0x1012d12c
        char16_t ride[0x40];
        gRideDatabase.GetTypeName(c->mObj, ride, 0x40);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag time(2, c->mTargetTime);
        TbUniStringVTag name(6, ride);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(time), V(name));
        break;
    }
    case 12:
    case 13: {   // 0x1012d274
        char16_t staff[0x40];
        StaffName(c, staff);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag time(2, c->mTargetTime);
        TbUniStringVTag name(6, staff);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(time), V(name));
        break;
    }
    case 15: {   // 0x1012d450
        char16_t ride[0x40];
        char16_t ride2[0x40];
        gRideDatabase.GetTypeName(c->mObj, ride, 0x40);
        gRideDatabase.GetTypeName(c->mObj2, ride2, 0x40);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag time(2, c->mTargetTime);
        TbUniStringVTag second(7, ride2);
        TbUniStringVTag first(6, ride);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(time), V(first), V(second));
        break;
    }
    case 16:
    case 18: {   // 0x1012d594
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag sofar(4, c->mValue);
        TbIntegerVTag days(5, c->mDays);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(sofar), V(days));
        break;
    }
    case 17:
    case 21: {   // 0x1012d6dc
        char16_t ride[0x40];
        gRideDatabase.GetTypeName(c->mObj, ride, 0x40);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag sofar(4, c->mValue);
        TbIntegerVTag days(5, c->mDays);
        TbUniStringVTag name(6, ride);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(sofar), V(days), V(name));
        break;
    }
    case 19:
    case 20: {   // 0x1012d874
        char16_t staff[0x40];
        StaffName(c, staff);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag sofar(4, c->mValue);
        TbIntegerVTag days(5, c->mDays);
        TbUniStringVTag name(6, staff);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(target), V(sofar), V(days), V(name));
        break;
    }
    case 22: {   // 0x1012daa0
        char16_t ride[0x40];
        char16_t ride2[0x40];
        gRideDatabase.GetTypeName(c->mObj, ride, 0x40);
        gRideDatabase.GetTypeName(c->mObj2, ride2, 0x40);
        TbIntegerVTag prize(3, c->mPrize);
        TbIntegerVTag days(5, c->mDays);
        TbUniStringVTag second(7, ride2);
        TbUniStringVTag first(6, ride);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize), V(days), V(first), V(second));
        break;
    }
    case 23: case 24: case 25: case 26: case 27: case 28: case 29: {   // 0x1012dbe4
        TbIntegerVTag prize(3, c->mPrize);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(prize));
        break;
    }
    case 30: case 31: case 32: case 33: case 34: case 35: case 36: {   // 0x1012dc4c
        TbIntegerVTag target(1, c->mTargetVal);
        TbIntegerVTag sofar(4, c->mValue);
        TagStrings()->FormatString(out, 0x400, element->mTextId, V(target), V(sofar));
        break;
    }
    default:   // above 0x24 (unsigned)
        break;
    }
    return 1;
}
