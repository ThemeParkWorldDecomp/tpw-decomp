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
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "Serialise.h"
#include "ThingParts2.h"
#include "World.h"
#include "analyser.h"

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

int32_t CChallenge::GetOfferId() const { return MessageId(mType, 0); }       // 0x100f8d7c, 312 bytes
int32_t CChallenge::GetActiveId() const { return MessageId(mType, 0x21); }   // 0x100f8ee4, 312 bytes
int32_t CChallenge::GetWinId() const { return MessageId(mType, 0x42); }      // 0x100f904c, 312 bytes
int32_t CChallenge::GetLoseId() const { return MessageId(mType, 0x63); }     // 0x100f91b0, 312 bytes
