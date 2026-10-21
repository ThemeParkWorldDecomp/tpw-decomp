// ChallengeHQ.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of ChallengeHQ code, named after it.

#include "analyser.h"
#include "ChallengeBalance.h"
#include "CMessageCentre.h"
#include "DateMessages.h"
#include "Debug.h"
#include "FS_Online.h"
#include "MainBalance.h"
#include "ParkMessages.h"
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "ThingMessages.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// One challenge (main app, SimTheme Park 2.04b3; unit w174, the original's Entertainer.cpp range): the parameter constructor 0x100f5a78, the
// serialiser 0x100f5b80, the win test DidThisWinIt 0x100f60d8, the four message id getters (GetOfferId 0x100f8d7c, GetActiveId 0x100f8ee4,
// GetWinId 0x100f904c, GetLoseId 0x100f91b0) and the LOADVAR / SAVEVAR instances of ChallengeType (0x100f93e8, 0x100f9640) and StaffType
// (0x100f9518, 0x100f9788).
//
// Quirks kept: the constructor does not write the padding bytes 0x1e, 0x1f and 0x2d; Serialise writes mIndependent (+0x1d) after mHaveWon (+0x2c);
// DidThisWinIt reads mCheckAtEndOnly as a byte (a loaded byte of 2 counts), fetches the park analyser again before every analyser call, compares
// GetCoasterWithMostLoops' unsigned answer as a signed number, writes the value in/out argument only in the cases 9, 10 and 29, and records the
// follow-up thing id only in the cases 14, 15 and 18; type 19 compares with the constant 3 and not with mTargetVal. For a staff type outside 0 to 4
// the original traces and then passes whatever r28 held as the model (undefined on the Mac); the host passes 0.

namespace {

using ObjectId = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

// divw. The PowerPC leaves the quotient of a division by zero and of INT_MIN / -1 undefined; the tests' Mac side (Unicorn) answers the
// dividend for both. The host never traps.
int32_t PpcDivw(int32_t a, int32_t b) {
    if (b == 0) return a;
    if (a == INT32_MIN && b == -1) return a;
    return a / b;
}

// The names the serialiser writes (the Mac's string pool at 0x10240c9c and following).
constexpr const char* kNameType = "mType";
constexpr const char* kNameTargetTime = "mTargetTime";
constexpr const char* kNameTargetVal = "mTargetVal";
constexpr const char* kNameTargetObj = "mTargetObj";
constexpr const char* kNameTargetObj2 = "mTargetObj2";
constexpr const char* kNameTargetStaffType = "mTargetStaffType";
constexpr const char* kNamePrize = "mPrize";
constexpr const char* kNameFollowup = "mDoubleOrNothingFollowup";
constexpr const char* kNameCheckAtEndOnly = "mCheckAtEndOnly";
constexpr const char* kNameOffered = "mChallengeOffered";
constexpr const char* kNameLost = "mChallengeLost";
constexpr const char* kNameDeclined = "mChallengeDeclined";
constexpr const char* kNameHaveWon = "mHaveWon";
constexpr const char* kNameIndependent = "mIndependent";
constexpr const char* kNameThingIdForFollowup = "mThingIdForFollowup";

// The staff model the original passes when the staff type is not 0 to 4 (it leaves r28 unset there: the host passes 0).
constexpr CThing::ThingModel kModelUnset = static_cast<CThing::ThingModel>(0);

// The offer message id of each challenge type 1 to 33 (index 0 is type 1). The other three getters add 0x21, 0x42 and 0x63 (each original is its own
// switch of constants, the listings agree with this table).
constexpr int32_t kOfferIds[33] = {0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0x102, 0x103, 0x104, 0x108, 0x109, 0x10b, 0x10c, 0x10e, 0xff,
                                   0x111, 0x112, 0x113, 0x114, 0x117, 0x100, 0x101, 0x107, 0x10a, 0x106, 0x10d, 0x10f, 0x110, 0x115, 0x116, 0x105};
constexpr int32_t kNoMessageId = 0x17f;   // type 0 and every type above 33

// The id of type `type` shifted by `delta`: an unsigned switch with the bound 0x21.
int32_t MessageId(int32_t type, int32_t delta) {
    const uint32_t index = static_cast<uint32_t>(type);
    if (index == 0 || index > 0x21) return kNoMessageId;
    return kOfferIds[index - 1] + delta;
}

}  // namespace

// 0x100f5a78 (88 bytes). The stores are in the original's order; the padding bytes are not written.
CChallenge::CChallenge(ChallengeType type, int32_t targetTime, int32_t prize, ChallengeType followup, bool checkAtEndOnly, int32_t targetVal,
                       ObjectId targetObj, ObjectId targetObj2, StaffType staffType, bool independent)
    : mType(type),
      mTargetTime(targetTime),
      mTargetVal(targetVal),
      mTargetObj(targetObj.mIndex),
      mTargetObj2(targetObj2.mIndex),
      mTargetStaffType(staffType),
      mPrize(prize),
      mDoubleOrNothingFollowup(followup),
      mCheckAtEndOnly(checkAtEndOnly),
      mIndependent(independent),
      mChallengeOffered(0),
      mChallengeLost(0),
      mChallengeDeclined(0),
      mHaveWon(false),
      mThingIdForFollowup(0) {}

// LOADVAR / SAVEVAR of the two 32-bit enums: the same bodies as the int instances.
template<> int32_t LOADVAR<ChallengeType>(void* file, ChallengeType* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f93e8, 236 bytes
}
template<> int32_t LOADVAR<StaffType>(void* file, StaffType* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f9518, 236 bytes
}
template<> int32_t SAVEVAR<ChallengeType>(void* file, ChallengeType* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f9640, 260 bytes
}
template<> int32_t SAVEVAR<StaffType>(void* file, StaffType* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f9788, 260 bytes
}

// 0x100f5b80 (1316 bytes). A version of 2 or more, or below 0, answers 0 at once; the first failing field answers 1. The unused argument is
// the base class's flag in the other serialisers.
int32_t CChallenge::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (version >= 2 || version < 0) return 0;
    ChallengeType* const type = reinterpret_cast<ChallengeType*>(&mType);
    StaffType* const staffType = reinterpret_cast<StaffType*>(&mTargetStaffType);
    ChallengeType* const followup = reinterpret_cast<ChallengeType*>(&mDoubleOrNothingFollowup);
    ObjectId* const targetObj = reinterpret_cast<ObjectId*>(&mTargetObj);
    ObjectId* const targetObj2 = reinterpret_cast<ObjectId*>(&mTargetObj2);
    ThingId* const thingId = reinterpret_cast<ThingId*>(&mThingIdForFollowup);
    if (save == 0) {
        if (LOADVAR(file, type, kNameType, 0) != 0) return 1;
        if (LOADVAR(file, &mTargetTime, kNameTargetTime, 0) != 0) return 1;
        if (LOADVAR(file, &mTargetVal, kNameTargetVal, 0) != 0) return 1;
        if (LOADVARSTRUCT(file, targetObj, kNameTargetObj, 0) != 0) return 1;
        if (LOADVARSTRUCT(file, targetObj2, kNameTargetObj2, 0) != 0) return 1;
        if (LOADVAR(file, staffType, kNameTargetStaffType, 0) != 0) return 1;
        if (LOADVAR(file, &mPrize, kNamePrize, 0) != 0) return 1;
        if (LOADVAR(file, followup, kNameFollowup, 0) != 0) return 1;
        if (LOADVAR(file, &mCheckAtEndOnly, kNameCheckAtEndOnly, 0) != 0) return 1;
        if (LOADVAR(file, &mChallengeOffered, kNameOffered, 0) != 0) return 1;
        if (LOADVAR(file, &mChallengeLost, kNameLost, 0) != 0) return 1;
        if (LOADVAR(file, &mChallengeDeclined, kNameDeclined, 0) != 0) return 1;
        if (LOADVAR(file, &mHaveWon, kNameHaveWon, 0) != 0) return 1;
        if (LOADVAR(file, &mIndependent, kNameIndependent, 0) != 0) return 1;   // (after mHaveWon, though it lives at +0x1d)
        if (LOADVARSTRUCT(file, thingId, kNameThingIdForFollowup, 0) != 0) return 1;
    } else {
        if (SAVEVAR(file, type, kNameType, 0) != 0) return 1;
        if (SAVEVAR(file, &mTargetTime, kNameTargetTime, 0) != 0) return 1;
        if (SAVEVAR(file, &mTargetVal, kNameTargetVal, 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, targetObj, kNameTargetObj, 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, targetObj2, kNameTargetObj2, 0) != 0) return 1;
        if (SAVEVAR(file, staffType, kNameTargetStaffType, 0) != 0) return 1;
        if (SAVEVAR(file, &mPrize, kNamePrize, 0) != 0) return 1;
        if (SAVEVAR(file, followup, kNameFollowup, 0) != 0) return 1;
        if (SAVEVAR(file, &mCheckAtEndOnly, kNameCheckAtEndOnly, 0) != 0) return 1;
        if (SAVEVAR(file, &mChallengeOffered, kNameOffered, 0) != 0) return 1;
        if (SAVEVAR(file, &mChallengeLost, kNameLost, 0) != 0) return 1;
        if (SAVEVAR(file, &mChallengeDeclined, kNameDeclined, 0) != 0) return 1;
        if (SAVEVAR(file, &mHaveWon, kNameHaveWon, 0) != 0) return 1;
        if (SAVEVAR(file, &mIndependent, kNameIndependent, 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, thingId, kNameThingIdForFollowup, 0) != 0) return 1;
    }
    return 0;
}

// 0x100f60d8 (1212 bytes). `value` is the running figure the caller keeps (written in cases 9, 10 and 29); the answer is whether the challenge is
// won. The switch is on mType (unsigned, bound 0x21): see the jump table at 0x102a05e0.
bool CChallenge::DidThisWinIt(int32_t& value, bool atEnd) {
    uint8_t checkAtEndOnly;
    std::memcpy(&checkAtEndOnly, &mCheckAtEndOnly, 1);   // lbz, cmplwi: a byte of 2 counts
    if (checkAtEndOnly != 0 && !atEnd) return false;
    const uint32_t type = static_cast<uint32_t>(mType);
    if (type > 0x21) return false;
    switch (type) {
        case 0:
        case 23:
        case 26:
            return false;
        case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
        case 11: case 12: case 13:
        case 16: case 17:
        case 20:
        case 22:
            return value >= mTargetVal;
        case 24:
        case 25:
        case 27:
            return PpcDivw(value, mTargetTime) >= mTargetVal;
        case 9: {   // balloons: the percentage of the visitors in the park
            const int32_t visitors = (*gGameWorldRef).GetParkAnalyser()->NumberOfVisitorsInPark();
            if (visitors <= 0) return false;
            const int32_t holding = (*gGameWorldRef).GetParkAnalyser()->NumberOfVisitorsWithBalloons();
            value = static_cast<int32_t>(static_cast<uint32_t>(holding) * 100u) / visitors;
            return value >= mTargetVal;
        }
        case 10: {   // costumes
            const int32_t visitors = (*gGameWorldRef).GetParkAnalyser()->NumberOfVisitorsInPark();
            if (visitors <= 0) return false;
            const int32_t wearing = (*gGameWorldRef).GetParkAnalyser()->NumberOfVisitorsWithCostumes();
            value = static_cast<int32_t>(static_cast<uint32_t>(wearing) * 100u) / visitors;
            return value >= mTargetVal;
        }
        case 29: {   // staff quality
            CThing::ThingModel model = kModelUnset;
            switch (mTargetStaffType) {
                case 0: model = static_cast<CThing::ThingModel>(5); break;
                case 1: model = static_cast<CThing::ThingModel>(4); break;
                case 2: model = static_cast<CThing::ThingModel>(6); break;
                case 3: model = static_cast<CThing::ThingModel>(7); break;
                case 4: model = static_cast<CThing::ThingModel>(8); break;
                default: CTRACE(3, "There really ought to be a staff type here!!!!!\n"); break;   // 0x10240d6f
            }
            value = (*gGameWorldRef).GetParkAnalyser()->AverageQualityOfStaffType(model);
            return value >= mTargetVal;
        }
        case 28: {
            const ObjectId object{mTargetObj};
            return (*gGameWorldRef).GetParkAnalyser()->GetHighestUpgradeLevelForObject(object) >= mTargetVal;
        }
        case 19: {   // a coaster with at least 3 loops (not mTargetVal)
            ThingId found;
            const ObjectId object{mTargetObj};
            return static_cast<int32_t>((*gGameWorldRef).GetParkAnalyser()->GetCoasterWithMostLoops(found, object)) >= 3;
        }
        case 18: {
            ThingId found;
            const ObjectId object{mTargetObj};
            if (static_cast<int32_t>((*gGameWorldRef).GetParkAnalyser()->GetCoasterWithMostLoops(found, object)) >= mTargetVal) {
                mThingIdForFollowup = found.mIndex;
                return true;
            }
            return false;
        }
        case 14: {   // any coaster (object type 0)
            ThingId found;
            const ObjectId object{0};
            if (static_cast<int32_t>((*gGameWorldRef).GetParkAnalyser()->GetCoasterWithMostLoops(found, object)) >= mTargetVal) {
                mThingIdForFollowup = found.mIndex;
                return true;
            }
            return false;
        }
        case 15: {
            ThingId found;
            if ((*gGameWorldRef).GetParkAnalyser()->GetGokartWithMostCrossRoads(found) >= mTargetVal) {
                mThingIdForFollowup = found.mIndex;
                return true;
            }
            return false;
        }
        case 21: {
            ThingId found;
            const ObjectId object{mTargetObj};
            return (*gGameWorldRef).GetParkAnalyser()->GetLongestTrackRide(found, object) >= mTargetVal;
        }
        case 30:
        case 31:
        case 32: {
            const ObjectId object{mTargetObj};
            return (*gGameWorldRef).GetParkAnalyser()->NumberOfObjectInPark(object) > 0;
        }
        case 33: {
            const ObjectId first{mTargetObj};
            if ((*gGameWorldRef).GetParkAnalyser()->NumberOfObjectInPark(first) <= 0) return false;
            const ObjectId second{mTargetObj2};
            return (*gGameWorldRef).GetParkAnalyser()->NumberOfObjectInPark(second) > 0;
        }
    }
    return false;
}

// The weather and challenge messages (main app, unit w124): the three constructors 0x101518c4, 0x10151870 and 0x10152008, the GetType functions
// 0x101399d0, 0x10139994 and 0x100f93b0, and the destructors 0x10138b9c, 0x10138b20 and 0x100f65c8. They live together because the native vtable
// needs a key function (GetType); the pattern is src/MsgDeath.cpp's.

CMsgChallenge::~CMsgChallenge() {}

// CChallengeHQ::CChallengeHQ, main app 0x100f6640 (416 bytes), unit w122b.

namespace {
// Lazy centre block at 0x100f66c0, repeated before each subscription, including after allocation failure.
CMessageCentre* MessageCentreForSubscribe() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}
}

CChallengeHQ::CChallengeHQ()
    : mBase(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{1}, static_cast<CThing::ThingModel>(0x13)) {
    mCurrentChallenge = -1;
    mNextChallengeEventTime[1] = 0;
    mNextChallengeEventTime[0] = 0;
    mSystemSwitchedOn = false;
    mChallengeOn = false;
    mWaitingForReply = false;
    mValue = 0;
    mActualThing = 0;
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0x1a));
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0xb));
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0x11));
    Init();
}

// The challenge hall's list and offers (main app 0x100f680c range; unit w130): ClearInvalidFollowups 0x100f680c (188 bytes), AddChallenge 0x100f7498
// (164 bytes), IsSystemOn 0x100f7c98 (272 bytes) and OfferNextChallenge 0x100f7dd8 (412 bytes). Reads *gWorld; the funny time is asked of it again
// for the span.

namespace {
int32_t Slot(int slot) { return gMainBalance.mValues[slot].mInteger; }
constexpr int kDaysBeforeSystemSlot = 480;   // the balance word at 0x788: the day the system switches on
constexpr int kDeclinedLimitSlot = 481;      // the balance word at 0x78c
constexpr int32_t kChallengeSlots = 0x14;

// 0x10237e1c: the signed 64 bit division of the span in ticks by 864,000,000,000 (a day); the Mac keeps the low word.
int32_t DaysOf(const TbTimeDiff& span) { return static_cast<int32_t>(span.mTicks / 864000000000LL); }
}  // namespace

// A follow-up that no other challenge in the list has the type of is dropped: the owning challenge's type is cleared (at once, so later
// rows see it).
void CChallengeHQ::ClearInvalidFollowups() {
    for (int32_t i = 0; i < kChallengeSlots; ++i) {
        if (mChallenges[i].mType != 0 && mChallenges[i].mDoubleOrNothingFollowup != 0) {
            const int32_t followup = mChallenges[i].mDoubleOrNothingFollowup;
            bool found = false;
            for (int32_t j = 0; j < kChallengeSlots; ++j) {
                if (j != i && mChallenges[j].mType != 0 && mChallenges[j].mType == followup) {
                    found = true;
                    break;
                }
            }
            if (!found) mChallenges[i].mType = 0;
        }
    }
}

// CChallengeHQ::Init, main app 0x100f6904 (412 bytes; Entertainer.cpp in the inventory, probably ChallengeHQ.cpp), unit w123b. The description table of
// challenges.sam that its CChallengeBalance reads is in src/ChallengeBalance.cpp (unit w177).

// Loads the level's challenge descriptions and offers the ones the level asks for. Only the twenty type words of the challenge slots are cleared first
// (no other state of the HQ is touched). A local CChallengeBalance (the Mac builds its table by hand: the base's vptr, then the derived one, then
// CBalanceTable::Init) loads data:challenges.sam fresh; a failed load only traces (the text is the original's, with no final newline) and carries on
// with whatever the table holds. Then for each of the twenty signed row choices of the main balance (slot 483 on; the count slot 503 is not read), a
// choice <= 0 is skipped and a positive one j selects the row of table slots 1 + 10 * j .. 10 + 10 * j, with no test against the sixty rows or the
// loaded count. The row's columns, in the order of the descriptions: 0 Type, 1 FollowupType, 2 TargetTime, 3 TargetVal, 4 TargetObj, 5 TargetObj2,
// 6 TargetStaffType, 7 Prize, 8 CheckAtEndOnly, 9 Independent. The two object ids are the low 16 bits of columns 4 and 5, the two flags are "!= 0".
// One CChallenge is built over the SAME stack storage each time (never default constructed first, as the Mac's temporary) with the parameter
// constructor (a stand-in) and handed to AddChallenge (a stand-in). After the loop ClearInvalidFollowups runs once, and the table's destructor (the Mac inlines it:
// the derived vptr put back, then UnInit) runs at the end of the scope.
void CChallengeHQ::Init() {
    for (int32_t i = 0; i < 0x14; ++i) mChallenges[i].mType = 0;
    CChallengeBalance table;
    table.Init();
    if (table.Load("data:challenges.sam", CBalanceTable::kLoadFresh) == 0) {
        CTRACE(0, "!!!!!\nLoading challenges.sam went horribly wrong! Shout at Bjarne!\n!!!!!");
    }
    alignas(CChallenge) unsigned char temporary[sizeof(CChallenge)];
    for (int32_t i = 0; i < kMainBalanceChallengesInThisLevelCount; ++i) {
        const int32_t choice = gMainBalance.mValues[kMainBalanceChallengesInThisLevel + i].mInteger;
        if (choice <= 0) continue;
        const BalanceValue* row = table.mValues + 1 + static_cast<std::ptrdiff_t>(choice) * 10;
        const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> targetObj{static_cast<uint16_t>(row[4].mInteger)};
        const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> targetObj2{static_cast<uint16_t>(row[5].mInteger)};
        CChallenge* challenge = new (temporary) CChallenge(
            static_cast<ChallengeType>(row[0].mInteger), row[2].mInteger, row[7].mInteger, static_cast<ChallengeType>(row[1].mInteger),
            row[8].mInteger != 0, row[3].mInteger, targetObj, targetObj2, static_cast<StaffType>(row[6].mInteger), row[9].mInteger != 0);
        AddChallenge(*challenge);
    }
    ClearInvalidFollowups();
}

// CChallengeBalance (main app, unit w177): the table of data:challenges.sam that CChallengeHQ::Init builds on its stack. The destructor 0x100f6acc (100 bytes)
// and the two virtuals GetBalanceDescription 0x100f9318 (16) and GetBalanceValuesStart 0x100f9368 (8), and the description table at 0x1028e908.

// Row 0 opens the array group (type 2, no name), rows 1..10 are its integer members, row 11 closes it (type 3, named Challenges, sixty elements), row 12
// ends the table. Every range and flag is zero: nothing is range checked.
const BalanceDescription gChallengeBalanceDescriptions[13] = {
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "Type", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "FollowupType", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "TargetTime", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "TargetVal", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "TargetObj", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "TargetObj2", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "TargetStaffType", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "Prize", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "CheckAtEndOnly", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "Independent", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "Challenges", 0, 0, 0.0f, 0.0f, 60, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

// 0x100f6acc (100 bytes). The Mac's destructor, when `this` is not NULL, stores the base's vtable word (0x102a08a8, TOC -0x6a48) at +4, calls
// CBalanceTable::UnInit and, when the halfword flag it is given is above 0, operator delete(this); it answers `this`. The NULL test, the vptr
// store and the delete are the compiler's (the host's destructor and delete expression); the body is the UnInit.
CChallengeBalance::~CChallengeBalance() { UnInit(); }

// The challenge hall's progress (main app 0x100f6b60 range; unit w175): FinalDestroy 0x100f6b60 (4 bytes), Serialise 0x100f6fe0 (784), AddToCounter
// 0x100f7324 (216), GetCurrentChallenge 0x100f757c (48), ReceiveMessage 0x100f75e4 (1,656), OfferFollowup 0x100f7fac (436), TellPlayerChallengeOffered
// 0x100f8194 (492), ChallengeAccepted 0x100f83cc (312), ChallengeDeclined 0x100f853c (356), EndCurrentChallenge 0x100f86d8 (196), WonCurrentChallenge
// 0x100f87d4 (788) and LostCurrentChallenge 0x100f8b20 (544). The flags are read as bytes (the Mac tests a byte against 0; a saved game can hold 2).

namespace {
// The names the serialiser writes (the Mac's string pool at 0x10240dfd and following).
constexpr const char* kNameCurrentChallenge = "mCurrentChallenge";
constexpr const char* kNameNextEventTime = "mNextChallengeEventTime";
constexpr const char* kNameSystemSwitchedOn = "mSystemSwitchedOn";
constexpr const char* kNameChallengeOn = "mChallengeOn";
constexpr const char* kNameWaitingForReply = "mWaitingForReply";
constexpr const char* kNameValue = "mValue";
constexpr const char* kNameActualThing = "mActualThing";

constexpr const char* kStaffText = "There really ought to be a staff type here!!!!!\n";   // 0x10240d6f
constexpr const char* kUnknownText = "Unknown message\n";                                  // 0x10240e6b
constexpr const char* kFollowupText = "Followup challenge %d is being offered\n";          // 0x10240e9c
constexpr const char* kNullText = "GetCurrentChallenge() returned NULL in EndCurrentChallenge()\n";   // 0x10240ec4
constexpr const char* kWonText = "Won the challenge!!!\n";                                 // 0x10240f02
constexpr const char* kLostText = "Lost the challenge!!!\n";                               // 0x10240f18

constexpr int kDaysAfterCompletedSlot = 478;   // the balance word at 0x780
constexpr int kDaysAfterDeclinedSlot = 479;    // the balance word at 0x784
constexpr int kDeclinesToForfeitSlot = 481;    // the balance word at 0x78c
// The halfword at 0x102a0668, which AddToCounter and ReceiveMessage read and nothing writes: its initial value, 0.
constexpr uint16_t kNoThing = 0;

constexpr int32_t kMsgEndOfDay = 0xb;
constexpr int32_t kMsgResearchCompleted = 0x11;
constexpr int32_t kMsgAddToChallenge = 0x1a;
constexpr int32_t kTypeResearch = 0x10;
constexpr int32_t kTypeToilets = 0x18;
constexpr int32_t kTypeStaffHappiness = 0x19;
constexpr int32_t kTypeStaffTypeHappiness = 0x1b;
// The staff model ReceiveMessage hands the analyser when the challenge's staff type is none of 0 to 4: the Mac passes r29, which nothing sets (the
// caller's value; the test makes it 0).
constexpr int32_t kModelLeftOver = 0;

// A flag byte tested against 0 (a bool with the byte 2 would be read as true by the Mac).
bool Flag(const bool& flag) { return *reinterpret_cast<const uint8_t*>(&flag) != 0; }

CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

TbTimeStamp FunnyTimeIn(CWorld* world) {
    return world->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

// The span of a balance slot's days: the Mac takes the low 64 bits of the sign-extended count times 864,000,000,000 (mulhwu, mullw), 0xc9 2a69c000.
TbTimeDiff DaysSpan(int32_t days) {
    TbTimeDiff span;
    span.mTicks = static_cast<int64_t>(static_cast<uint64_t>(static_cast<int64_t>(days)) * 864000000000ULL);
    return span;
}

// The funny time now plus the span, stored as the hall's next event.
void SetNextEvent(TbTimeStamp* next, int32_t days) {
    const TbTimeDiff span = DaysSpan(days);
    const TbTimeStamp when = FunnyTimeIn(gWorld) + span;
    *next = when;
}

template <class V>
int32_t Var(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVAR<V>(file, value, name, 0) : LOADVAR<V>(file, value, name, 0);
}

template <class V>
int32_t VarStruct(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVARSTRUCT<V>(file, value, name, 0) : LOADVARSTRUCT<V>(file, value, name, 0);
}

}  // namespace

void CChallengeHQ::FinalDestroy() {}

// CChallengeHQ::UpdateTurn, main app 0x100f6b98 (1048 bytes), unit w124: offers the next challenge, reports its progress every fourth turn, ends it
// when its time is up, and warns when little time is left. Reads *gWorld; the first clock read after the turn word uses the world pointer read once
// after the GetCurrentChallenge test, the others read the global again.

void CChallengeHQ::UpdateTurn() {
    if (!IsSystemOn()) return;
    if (!mChallengeOn) {
        if (mWaitingForReply) return;
        TbTimeStamp now = FunnyTimeIn(gWorld);
        if (now > *NextEventTime()) OfferNextChallenge(false);
        return;
    }
    if (GetCurrentChallenge() == nullptr) return;
    CWorld* world = gWorld;
    if ((world->mTurn & 3) == 0) {
        TbTimeStamp now = FunnyTimeIn(world);
        TbTimeDiff left = *NextEventTime() - now;
        int32_t days = DaysOf(left);
        int32_t value = mValue;
        CChallenge* challenge = GetCurrentChallenge();
        int32_t staff = challenge->mTargetStaffType;
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj{challenge->mTargetObj};
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj2{challenge->mTargetObj2};
        int32_t targetVal = challenge->mTargetVal;
        int32_t targetTime = challenge->mTargetTime;
        int32_t prize = challenge->mPrize;
        int32_t id = challenge->GetActiveId();
        CMsgChallenge message(id, kAdvisorNoMessage, false, prize, targetTime, targetVal, days, value, obj, obj2, static_cast<StaffType>(staff));
        MessageCentreForPost()->PostMessage(message);
        return;
    }
    {
        TbTimeStamp now = FunnyTimeIn(world);
        if (now > *NextEventTime()) {
            EndCurrentChallenge();
            return;
        }
    }
    if (GetCurrentChallenge()->DidThisWinIt(mValue, false)) {
        WonCurrentChallenge();
        return;
    }
    if (GetCurrentChallenge()->mTargetTime == 0) return;
    TbTimeStamp now = FunnyTimeIn(gWorld);
    TbTimeDiff left = *NextEventTime() - now;
    int32_t days = DaysOf(left);
    int32_t targetTime = GetCurrentChallenge()->mTargetTime;
    int32_t percent = static_cast<int32_t>(static_cast<uint32_t>(days) * 100u) / targetTime;
    if (percent < gMainBalance.mValues[kMainBalanceShortTimeLeftWarningAt].mInteger) {
        CMsgChallenge message(0x17f, static_cast<AdvisorMessage>(0x10f), false, 0, 0, 0, 0, 0, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0},
                              tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0}, static_cast<StaffType>(0));
        MessageCentreForPost()->PostMessage(message);
    }
}

int32_t CChallengeHQ::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    mBase.Serialise(file, save, version, unused);
    if (version < 0 || version >= 2) return 0;
    for (int32_t i = 0; i < kChallengeSlots; ++i) mChallenges[i].Serialise(file, save, version, unused);
    if (Var<int32_t>(save, file, &mCurrentChallenge, kNameCurrentChallenge) != 0) return 1;
    if (VarStruct<TbTimeStamp>(save, file, NextEventTime(), kNameNextEventTime) != 0) return 1;
    if (Var<bool>(save, file, &mSystemSwitchedOn, kNameSystemSwitchedOn) != 0) return 1;
    if (Var<bool>(save, file, &mChallengeOn, kNameChallengeOn) != 0) return 1;
    if (Var<bool>(save, file, &mWaitingForReply, kNameWaitingForReply) != 0) return 1;
    if (Var<int32_t>(save, file, &mValue, kNameValue) != 0) return 1;
    if (VarStruct<ThingId>(save, file, reinterpret_cast<ThingId*>(&mActualThing), kNameActualThing) != 0) return 1;
    return 0;
}

// Adds to the counter when the current challenge is on, is of this type and target object, and is for this thing (or any, when the hall's thing is
// the none value).
void CChallengeHQ::AddToCounter(ChallengeType type, int32_t amount, ObjectId obj, ThingId thing) {
    if (GetCurrentChallenge() == nullptr) return;
    if (!Flag(mChallengeOn)) return;
    if (GetCurrentChallenge()->mType != static_cast<int32_t>(type)) return;
    if (obj.mIndex != GetCurrentChallenge()->mTargetObj) return;
    if (mActualThing != kNoThing && thing.mIndex != mActualThing) return;
    mValue = static_cast<int32_t>(static_cast<uint32_t>(mValue) + static_cast<uint32_t>(amount));
}

// The challenge goes into the first free row (type 0), all 0x30 bytes copied (the padding too); a full list drops it.
void CChallengeHQ::AddChallenge(CChallenge& challenge) {
    for (int32_t i = 0; i < kChallengeSlots; ++i) {
        if (mChallenges[i].mType == 0) {
            std::memcpy(&mChallenges[i], &challenge, sizeof(CChallenge));
            return;
        }
    }
}

CChallenge* CChallengeHQ::GetCurrentChallenge() {
    if (mCurrentChallenge < 0 || mCurrentChallenge >= kChallengeSlots) return nullptr;
    return &mChallenges[mCurrentChallenge];
}

void CChallengeHQ::ReceiveMessage(const CMsg& message) {
    const ObjectId noObject{0};
    const ThingId noThing{0};
    switch (message.GetType()) {
        case kMsgResearchCompleted: {
            const CMsgResearchCompleted& done = static_cast<const CMsgResearchCompleted&>(message);
            if (static_cast<int32_t>(done.mCategory) != 0) return;
            AddToCounter(static_cast<ChallengeType>(kTypeResearch), 1, noObject, noThing);
            return;
        }
        case kMsgAddToChallenge: {
            // The message's thing id (+0x12) is not read: the thing is passed as none.
            const CMsgAddToChallenge& add = static_cast<const CMsgAddToChallenge&>(message);
            AddToCounter(add.mType, add.mValue, ObjectId{add.mObj}, noThing);
            return;
        }
        case kMsgEndOfDay: {
            if (GetCurrentChallenge() == nullptr) return;
            const int32_t type = GetCurrentChallenge()->mType;
            if (type == kTypeToilets) {
                CParkAnalyser* analyser = gGameWorldRef->GetParkAnalyser();
                const int32_t filth = analyser->AvgFilthLevelOfToilets();
                AddToCounter(static_cast<ChallengeType>(kTypeToilets), static_cast<int32_t>(100u - static_cast<uint32_t>(filth)), noObject, noThing);
            } else if (type == kTypeStaffHappiness) {
                CParkAnalyser* analyser = gGameWorldRef->GetParkAnalyser();
                AddToCounter(static_cast<ChallengeType>(kTypeStaffHappiness), analyser->AverageHappinessOfStaff(), noObject, noThing);
            } else if (type == kTypeStaffTypeHappiness) {
                int32_t model = kModelLeftOver;
                switch (GetCurrentChallenge()->mTargetStaffType) {
                    case 0: model = 5; break;
                    case 1: model = 4; break;
                    case 2: model = 6; break;
                    case 3: model = 7; break;
                    case 4: model = 8; break;
                    default: CTRACE(3, kStaffText); break;
                }
                CParkAnalyser* analyser = gGameWorldRef->GetParkAnalyser();
                const int32_t happiness = analyser->AverageHappinessOfStaffType(static_cast<CThing::ThingModel>(model));
                AddToCounter(static_cast<ChallengeType>(kTypeStaffTypeHappiness), happiness, noObject, noThing);
            }
            return;
        }
        default:
            GAME_ASSERTMSG(0, kUnknownText);
            return;
    }
}

// Not in the online or easy modes (the game type must be 0). Once the gates are open and the days since the session started reach the
// balance word, the system switches on for good and the first challenge is offered at once.
bool CChallengeHQ::IsSystemOn() {
    if (TheGameType().mType != 0) return false;
    if (mSystemSwitchedOn) return true;
    if (gWorld->GetParkAnalyser()->GatesOpen()) {
        TbTimeDiff span = gWorld->mParkTime.GetFunnyTimeSinceSessionStart();
        if (!(DaysOf(span) < Slot(kDaysBeforeSystemSlot))) {
            mSystemSwitchedOn = true;
            OfferNextChallenge(true);
            return true;
        }
    }
    return false;
}

// Looks from the row after the current one round the ring (twenty more rows than the current index) for a row that is used, not won,
// has a declined count below the balance word and is independent; it becomes the current one and the player is told.
void CChallengeHQ::OfferNextChallenge(bool force) {
    if (mChallengeOn || mWaitingForReply) return;
    mValue = 0;
    const int32_t current = mCurrentChallenge;
    int32_t first = 0;
    if (current >= 0) first = current + 1;
    const int32_t last = static_cast<int32_t>(static_cast<uint32_t>(current) + kChallengeSlots);
    for (int32_t i = first; i <= last; ++i) {
        const int32_t index = i - (i / kChallengeSlots) * kChallengeSlots;
        bool eligible = mChallenges[index].mType != 0;
        if (eligible) {
            if (mChallenges[index].mHaveWon) {
                eligible = false;
            } else {
                eligible = mChallenges[index].mChallengeDeclined < Slot(kDeclinedLimitSlot);
            }
        }
        if (eligible && mChallenges[index].mIndependent) {
            CTRACE(3, "Challenge %d is being offered!\n", index);
            mCurrentChallenge = index;
            CChallenge* challenge = nullptr;
            if (mCurrentChallenge >= 0 && mCurrentChallenge < kChallengeSlots) challenge = &mChallenges[mCurrentChallenge];
            if (TellPlayerChallengeOffered(challenge, force)) mWaitingForReply = true;
            return;
        }
    }
}

// Offers the challenge the current one names as its follow-up: the first candidate row of that type that the player may be offered. A row the
// player refuses to be told about (the message answer false) lets the loop go on with mCurrentChallenge already changed.
bool CChallengeHQ::OfferFollowup() {
    if (Flag(mChallengeOn)) return false;
    if (GetCurrentChallenge() == nullptr) return false;
    const int32_t followup = GetCurrentChallenge()->mDoubleOrNothingFollowup;
    if (followup == 0) return false;
    for (int32_t i = 0; i < kChallengeSlots; ++i) {
        const CChallenge& row = mChallenges[i];
        const bool candidate = row.mType != 0 && !Flag(row.mHaveWon) && row.mChallengeDeclined < Slot(kDeclinesToForfeitSlot);
        if (candidate && followup == row.mType) {
            mCurrentChallenge = i;
            CTRACE(3, kFollowupText, i);
            if (TellPlayerChallengeOffered(GetCurrentChallenge(), false)) {
                mWaitingForReply = true;
                mValue = 0;
                return true;
            }
        }
    }
    return false;
}

bool CChallengeHQ::TellPlayerChallengeOffered(CChallenge* challenge, bool force) {
    if (challenge == nullptr) return false;
    const int32_t prize = challenge->mPrize;
    const int32_t targetTime = challenge->mTargetTime;
    const int32_t targetVal = challenge->mTargetVal;
    const ObjectId obj{challenge->mTargetObj};
    const ObjectId obj2{challenge->mTargetObj2};
    const int32_t staff = challenge->mTargetStaffType;
    // The listing builds the message twice (one copy per force value); one call with the chosen message number is the same.
    CMsgChallenge message(challenge->GetOfferId(), static_cast<AdvisorMessage>(force ? 0x158 : 0x10e), true, prize, targetTime, targetVal, 0, 0, obj, obj2,
                          static_cast<StaffType>(staff));
    MessageCentreForPost()->PostMessage(message);
    return true;
}

void CChallengeHQ::ChallengeAccepted() {
    if (!Flag(mWaitingForReply)) return;
    if (GetCurrentChallenge() == nullptr) return;
    mWaitingForReply = false;
    mChallengeOn = true;
    SetNextEvent(NextEventTime(), GetCurrentChallenge()->mTargetTime);
}

// The current challenge is read without a NULL test (the Mac reads c->mIndependent at address 0x1d with a bad index).
void CChallengeHQ::ChallengeDeclined() {
    if (!Flag(mWaitingForReply)) return;
    mWaitingForReply = false;
    mChallengeOn = false;
    if (!Flag(GetCurrentChallenge()->mIndependent)) {
        const int32_t prize = GetCurrentChallenge()->mPrize;
        CBankAccount* bank = gWorld->GetBankAccount();
        bank->MakeDeposit(static_cast<uint32_t>(prize / 2));
        mActualThing = 0;
    }
    SetNextEvent(NextEventTime(), Slot(kDaysAfterDeclinedSlot));
}

void CChallengeHQ::EndCurrentChallenge() {
    if (GetCurrentChallenge() == nullptr) {
        CTRACE(3, kNullText);
        return;
    }
    if (GetCurrentChallenge()->DidThisWinIt(mValue, true)) {
        WonCurrentChallenge();
    } else {
        LostCurrentChallenge();
    }
}

void CChallengeHQ::WonCurrentChallenge() {
    CTRACE(3, kWonText);
    mChallengeOn = false;
    SetNextEvent(NextEventTime(), Slot(kDaysAfterCompletedSlot));
    if (GetCurrentChallenge() == nullptr) return;
    if (GetCurrentChallenge()->mDoubleOrNothingFollowup != 0) {
        mActualThing = GetCurrentChallenge()->mThingIdForFollowup;
        if (OfferFollowup()) GetCurrentChallenge()->mThingIdForFollowup = mActualThing;
    } else {
        mActualThing = 0;
        const int32_t prize = GetCurrentChallenge()->mPrize;
        CBankAccount* bank = gWorld->GetBankAccount();
        bank->MakeDeposit(static_cast<uint32_t>(prize));
    }
    // The follow-up, when one was offered: the index is in range (OfferFollowup stores 0 to 19 only), so the Mac has no NULL test here.
    CChallenge* won = GetCurrentChallenge();
    won->mHaveWon = true;
    if (won->mDoubleOrNothingFollowup == 0) {
        const int32_t prize = won->mPrize;
        CMsgChallenge message(won->GetWinId(), static_cast<AdvisorMessage>(0x110), false, prize, 0, 0, 0, 0, ObjectId{0}, ObjectId{0},
                              static_cast<StaffType>(0));
        MessageCentreForPost()->PostMessage(message);
    }
}

void CChallengeHQ::LostCurrentChallenge() {
    CTRACE(3, kLostText);
    mChallengeOn = false;
    SetNextEvent(NextEventTime(), Slot(kDaysAfterCompletedSlot));
    if (GetCurrentChallenge() != nullptr) {
        const int32_t value = mValue;
        CChallenge* lost = GetCurrentChallenge();
        lost->mChallengeLost = lost->mChallengeLost + 1;
        const int32_t targetVal = lost->mTargetVal;
        const int32_t targetTime = lost->mTargetTime;
        const int32_t prize = lost->mPrize;
        CMsgChallenge message(lost->GetLoseId(), static_cast<AdvisorMessage>(0x111), false, prize, targetTime, targetVal, 0, value, ObjectId{0},
                              ObjectId{0}, static_cast<StaffType>(0));
        MessageCentreForPost()->PostMessage(message);
    }
    mActualThing = 0;
}

int32_t CChallenge::GetOfferId() const { return MessageId(mType, 0); }       // 0x100f8d7c, 312 bytes
int32_t CChallenge::GetActiveId() const { return MessageId(mType, 0x21); }   // 0x100f8ee4, 312 bytes
int32_t CChallenge::GetWinId() const { return MessageId(mType, 0x42); }      // 0x100f904c, 312 bytes
int32_t CChallenge::GetLoseId() const { return MessageId(mType, 0x63); }     // 0x100f91b0, 312 bytes

const BalanceDescription* CChallengeBalance::GetBalanceDescription(int32_t row) { return gChallengeBalanceDescriptions + row; }
BalanceValue* CChallengeBalance::GetBalanceValuesStart() { return mValues; }

int32_t CMsgChallenge::GetType() const { return 0x19; }
