// Object.cpp saved ride state (unit w229).
#include "ObjectSerialise.h"
#include "TbTimeDiff.h"
#include "ppc_math.h"

// __sinit_Object_cpp, 0x10111e94 (88 bytes): ordered definitions replace its stores,
// string construction and runtime destructor registration (Mac node 0x1034c374).
int32_t gObjectStaticC384 = 0; // 0x1034c384, meaning unknown
int32_t gObjectStaticC380 = 0x10000; // 0x1034c380, meaning unknown
TbDynamicStringTemplate<char16_t> gObjectStaticC360(u"gms.dat"); // 0x1034c360

// 0x10111958, 236 bytes. Failure leaves the file's raw bytes in the destination.
template<> int32_t LOADVAR<CObject::ObjectState>(void* file, CObject::ObjectState* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}

// 0x10111bbc, 260 bytes. The swapped copy never changes the source.
template<> int32_t SAVEVAR<CObject::ObjectState>(void* file, CObject::ObjectState* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}

// 0x10106570, 4,368 bytes. Every failed call keeps its mutations and stops here.
int32_t CObject::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (version < 0 || version >= 2) return 0;
    using ObjectId = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
    using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
    using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

    // Labels are the strings at 0x1024294c + 0x39 through +0x242.
#define OBJECT_VALUE(Type, member, name) \
    if ((save != 0 ? SAVEVAR<Type>(file, reinterpret_cast<Type*>(&(member)), name, 0) \
                   : LOADVAR<Type>(file, reinterpret_cast<Type*>(&(member)), name, 0)) != 0) return 1
#define OBJECT_STRUCT(Type, member, name) \
    if ((save != 0 ? SAVEVARSTRUCT<Type>(file, reinterpret_cast<Type*>(&(member)), name, 0) \
                   : LOADVARSTRUCT<Type>(file, reinterpret_cast<Type*>(&(member)), name, 0)) != 0) return 1
#define OBJECT_HISTORY(member) \
    if (member.Serialise(file, save, version, unused) != 0) return 1

    OBJECT_VALUE(MacLong, mRotation, "mAngle");
    OBJECT_STRUCT(ObjectId, mObjectType, "mId");
    int32_t calendar[8];
    auto* stamp = reinterpret_cast<TbTimeStamp*>(mBuilt);
    if (save != 0) {
        stamp->GetTime(&calendar[0], &calendar[1], &calendar[2], &calendar[3],
                       &calendar[4], &calendar[5], &calendar[6], &calendar[7]);
    }
    for (int32_t t = 0; t < 8; ++t) {
        OBJECT_VALUE(int32_t, calendar[t], "tv[t]");
    }
    if (save == 0) {
        stamp->SetTime(calendar[0], calendar[1], calendar[2], calendar[4], calendar[5], calendar[6], calendar[7]);
    }
    OBJECT_VALUE(MacULong, mF1C, "MeshInstanceID");
    OBJECT_VALUE(uint16_t, mFlags, "mFlags");
    for (int32_t i = 0; i < 33; ++i) {
        OBJECT_VALUE(char16_t, mSignA[i], "mNameA[i]");
        OBJECT_VALUE(char16_t, mSignB[i], "mNameB[i]");
    }
    OBJECT_VALUE(MacLong, mF20, "mRideScriptHandle");
    OBJECT_VALUE(MacLong, mF24, "mTrackRideHandle");
    OBJECT_VALUE(ObjectState, mState, "mState");
    OBJECT_STRUCT(MapId, mF30, "mTopLeft");
    OBJECT_STRUCT(MapId, mF32, "mEntryPos");
    OBJECT_STRUCT(ThingId, mF0C, "mNext");
    OBJECT_STRUCT(ThingId, mF5A, "mAssignedStaffMember");
    OBJECT_STRUCT(MapId, mF36, "mBackOfQueue");
    OBJECT_VALUE(int32_t, mF64, "mCanLoad");
    OBJECT_STRUCT(MapId, mF34, "mExitPos");
    OBJECT_STRUCT(ThingId, mQueuedPerson, "mFirstInQ");
    OBJECT_VALUE(int32_t, mF28, "mIsTrackRideValid");
    OBJECT_STRUCT(ThingId, mF2C, "mUpgradeParent");
    OBJECT_HISTORY(mHistoryF4);
    OBJECT_HISTORY(mHistory6C);
    OBJECT_VALUE(uint32_t, mF19C, "mNumCustomers");
    OBJECT_HISTORY(mHistory1A4);
    OBJECT_VALUE(uint32_t, mF1A0, "mNumWalkAways");
    OBJECT_HISTORY(mHistory22C);
    OBJECT_HISTORY(mHistory2B4);
    OBJECT_HISTORY(mHistory33C);
    OBJECT_VALUE(uint8_t, mF59, "mOperatingCapacity");
    OBJECT_VALUE(uint8_t, mF58, "mOperatingDuration");
    OBJECT_VALUE(uint32_t, mF54, "mOperatingSpeed");
    OBJECT_STRUCT(ThingId, mPersonLoaded, "mPersonBeingLoaded");
    OBJECT_VALUE(MacLong, mCostOfGoods, "mCostOfGoods");
    OBJECT_VALUE(MacLong, mF188, "mQualityOfGoods");
    OBJECT_VALUE(MacLong, mChanceOfNotLoosing, "mChanceOfWinning");
    OBJECT_VALUE(MacLong, mPricePerUse, "mPricePerUse");
    if (save == 0) {
        const int32_t price = static_cast<int32_t>(mPricePerUse);
        mPricePerUse = price < 0 ? 0 : price > 500 ? 500 : price;
    }
    OBJECT_VALUE(MacLong, mF194, "mAmountOfSpecialIngredient");
    OBJECT_VALUE(uint32_t, mF3C, "mQueueSizeInCells");
    float pv;
    if (save != 0) {
        pv = static_cast<float>(static_cast<uint8_t>(ppc_fctiwz(mF48)));
        OBJECT_VALUE(float, pv, "pv");
        pv = static_cast<float>(static_cast<uint8_t>(ppc_fctiwz(mF44)));
        OBJECT_VALUE(float, pv, "pv");
        pv = static_cast<float>(static_cast<uint8_t>(ppc_fctiwz(mF40)));
        OBJECT_VALUE(float, pv, "pv");
    } else {
        OBJECT_VALUE(float, pv, "pv");
        mF48 = pv;
        OBJECT_VALUE(float, pv, "pv");
        mF44 = pv;
        OBJECT_VALUE(float, pv, "pv");
        mF40 = pv;
    }
    OBJECT_VALUE(int32_t, mF60, "mRequestedService");
    OBJECT_VALUE(MacULong, mF5C, "mTimeMarkedForMaintenance");
    OBJECT_VALUE(uint32_t, mF180, "mTotalCosts");
    OBJECT_VALUE(uint32_t, mF17C, "mTotalTakings");
    OBJECT_VALUE(MacLong, mF50, "mUpgradeBalloonSprite");
    OBJECT_VALUE(uint8_t, mF4C, "mUpgradeLevel");
#undef OBJECT_HISTORY
#undef OBJECT_STRUCT
#undef OBJECT_VALUE
    return 0;
}
