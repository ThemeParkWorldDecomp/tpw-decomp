// CControlManager (main app of SimTheme Park 2.04b3, ControlManager.cpp; unit w176): the table of 150 object controls the world keeps. Serialise
// 0x100f9ed0 (432 bytes), GetObjectControl 0x100fa0b8 (152) and its const twin 0x100fa1b8 (140), CheckAllControlsInitialised 0x100fa32c (304), and
// the file functions of an ObjectControl: LOADVARSTRUCT 0x100fa4a0 (272) and SAVEVARSTRUCT 0x100fa6e4 (360) with the two word swaps
// USwapInplace32<unsigned long> 0x100fa5fc and <long> 0x100fa644 (12 bytes each). SetupInitialControls (0x100fa2ac) is in ControlManager_setup.cpp.
#include <cstdint>

#include "Debug.h"
#include "RideDB.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "World.h"
#include "analyser.h"

namespace {
using ObjectIdStruct = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;

// The strings are the Mac's, from 0x10241079.
constexpr char kNameControls[] = "mObjectControls[i]";                                     // 0x10241079 (the one text of all 150 elements)
constexpr char kNameNumControls[] = "mNumObjectControls";                                  // 0x1024108c
constexpr char kNamePreviousSearchKey[] = "mPreviousSearchKey";                            // 0x1024109f
constexpr char kAssertKeyNotFound[] = "CControlManager::GetObjectControl - id key not found 8-(";   // 0x102410b2
constexpr int32_t kNumControls = 0x96;

// The Mac keeps the control found last as a pointer in the word at +0x12c8; the host's four bytes hold 0 or the control's index + 1.
// The address is plain arithmetic, so a stray word never faults.
ObjectControl* CachedControl(CControlManager* manager) {
    const uint32_t word = manager->mPreviousSearchResult;
    if (word == 0) return nullptr;
    return reinterpret_cast<ObjectControl*>(reinterpret_cast<uint8_t*>(manager) + static_cast<uintptr_t>(word - 1) * sizeof(ObjectControl));
}
const ObjectControl* CachedControl(const CControlManager* manager) { return CachedControl(const_cast<CControlManager*>(manager)); }

// The control at index i is cached under the id.
void CacheControl(CControlManager* manager, uint16_t id, int32_t index) {
    manager->mPreviousSearchKey = id;
    manager->mPreviousSearchResult = static_cast<uint32_t>(index) + 1;
}
}  // namespace

// 0x100fa5fc (12 bytes) and 0x100fa644 (12 bytes): SamsUtilities::USwapInplace32<unsigned long> and <long>, lwz; stwbrx.
void ControlSwap::USwapInplace32ULong(uint32_t& word) { word = serialise::SwapWord(word); }
void ControlSwap::USwapInplace32Long(int32_t& word) { word = serialise::SwapWord(word); }

// (global, not in the unnamed namespace: serialise::LoadStruct and SaveStruct find them by argument-dependent lookup)
// What the file functions do to a control between the host's order and the file's: the id halfword (Byteswap, 0x100fa688), then the five long
// words and the unsigned long, in this order.
void SwapForFile(ObjectControl& control) {
    control.mType = serialise::SwapWord(control.mType);
    ControlSwap::USwapInplace32Long(control.mUpgradeCost);
    ControlSwap::USwapInplace32Long(control.mFatigueEffect);
    ControlSwap::USwapInplace32Long(control.mRipOffOK);
    ControlSwap::USwapInplace32Long(control.mUpgradesAvailable);
    ControlSwap::USwapInplace32Long(control.mNumberInPark);
    ControlSwap::USwapInplace32ULong(control.mFirstBuiltTurn);
}
void UnswapForFile(ObjectControl& control) { SwapForFile(control); }

// 0x100fa4a0 (272 bytes) and 0x100fa6e4 (360 bytes). The saved copy keeps the five pad bytes of the control (the Mac's are stack leftovers).
template<> int32_t LOADVARSTRUCT<ObjectControl>(void* file, ObjectControl* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);
}
template<> int32_t SAVEVARSTRUCT<ObjectControl>(void* file, ObjectControl* value, const char* name, int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);
}

// 0x100f9ed0 (432 bytes): versions 0 and 1; a load leaves the cache empty.
int32_t CControlManager::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    ObjectIdStruct* const key = reinterpret_cast<ObjectIdStruct*>(&mPreviousSearchKey);
    if (save == 0) {
        if (version < 2 && -1 < version) {
            for (int32_t i = 0; i < kNumControls; ++i) {
                if (LOADVARSTRUCT(file, &mControls[i], kNameControls, 0) != 0) return 1;
            }
            if (LOADVAR(file, reinterpret_cast<MacLong*>(&mNumObjectControls), kNameNumControls, 0) != 0) return 1;
            if (LOADVARSTRUCT(file, key, kNamePreviousSearchKey, 0) != 0) return 1;
            mPreviousSearchResult = 0;
            mPreviousSearchKey = 0;
        }
    } else if (version < 2 && -1 < version) {
        for (int32_t i = 0; i < kNumControls; ++i) {
            if (SAVEVARSTRUCT(file, &mControls[i], kNameControls, 0) != 0) return 1;
        }
        if (SAVEVAR(file, reinterpret_cast<MacLong*>(&mNumObjectControls), kNameNumControls, 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, key, kNamePreviousSearchKey, 0) != 0) return 1;
    }
    return 0;
}

// 0x100fa0b8 (152 bytes): the id is the high halfword of the Mac's register. The control found is cached with its id.
ObjectControl* CControlManager::GetObjectControl(ObjectIdStruct type, int32_t flag) {
    const uint16_t id = type.mIndex;
    if (id == mPreviousSearchKey) return CachedControl(this);
    for (int32_t i = 0; i < mNumObjectControls; ++i) {
        if (id == mControls[i].mType) {
            CacheControl(this, id, i);
            return CachedControl(this);
        }
    }
    if (flag == 0) GAME_ASSERTMSG(0, kAssertKeyNotFound);
    return nullptr;
}

// 0x100fa1b8 (140 bytes): the same search, nothing cached.
const ObjectControl* CControlManager::GetObjectControl(ObjectIdStruct type, int32_t flag) const {
    const uint16_t id = type.mIndex;
    if (id == mPreviousSearchKey) return CachedControl(this);
    for (int32_t i = 0; i < mNumObjectControls; ++i) {
        if (id == mControls[i].mType) return &mControls[i];
    }
    if (flag == 0) GAME_ASSERTMSG(0, kAssertKeyNotFound);
    return nullptr;
}

// 0x100fa32c (304 bytes): for each ride of the database the search of GetObjectControl runs inline (flag 1: no assert); a ride with no control
// (a cached NULL included) takes the next one. The Mac does not test the count against the 150 controls.
void CControlManager::CheckAllControlsInitialised() {
    for (RideInfo* ride = gRideDatabase.GetFirstRide(); ride != nullptr; ride = gRideDatabase.GetNextRide()) {
        const uint16_t id = ride->mId;
        ObjectControl* found;
        if (id == mPreviousSearchKey) {
            found = CachedControl(this);
        } else {
            found = nullptr;
            for (int32_t i = 0; i < mNumObjectControls; ++i) {
                if (id == mControls[i].mType) {
                    CacheControl(this, id, i);
                    found = CachedControl(this);
                    break;
                }
            }
        }
        if (found == nullptr) {
            const int32_t index = mNumObjectControls;
            mNumObjectControls = index + 1;
            ObjectControl* control = reinterpret_cast<ObjectControl*>(reinterpret_cast<uint8_t*>(this) + static_cast<uintptr_t>(index) * sizeof(ObjectControl));
            control->mType = ride->mId;
            control->mUpgradeCost = ride->Value(kRideSlot_Upgrades_CostOfUpgrade).mInteger;   // ride + 0x1b8
            control->mFatigueEffect = ride->Value(kRideSlot_FatigueEffect).mInteger;          // ride + 0xe8
            control->mRipOffOK = ride->Value(kRideSlot_RipOffOK).mInteger;                    // ride + 0x16c
            control->mAvailable = ride->Value(kRideSlot_Upgrades_CostOfResearch).mInteger == 0 ? 1 : 0;   // ride + 0x180
            control->mUpgradesAvailable = 0;
            control->mNumberInPark = 0;
            control->mFirstBuiltTurn = 0;
        }
    }
}
