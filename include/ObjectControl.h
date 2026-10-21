// ObjectControl (World.cpp, main app): 0x20 bytes, 0x96 of them in the world from +0x1d943c. The constructor is a bare blr: it
// writes nothing (unit w113), so a fresh world holds whatever its allocator gave. The fields from +0x10 are named by unit w151 (the park
// analyser's queries read them; names mine). Unit w176 (src/game/ControlManager.cpp) names the first 0x10 bytes: CControlManager::
// CheckAllControlsInitialised 0x100fa32c fills them from the ride record of the type.
#pragma once
#include <cstddef>
#include <cstdint>

class ObjectControl {
public:
    ObjectControl();    // 0x1013a10c (4 bytes, src/game/World.cpp)

    uint16_t mType;               // 0x00  the object type's id (the key GetObjectControl searches)
    uint8_t mF02[2];              // 0x02  never written
    int32_t mUpgradeCost;         // 0x04  the ride record's CostOfUpgrade (slot 107; ui_main_click.cpp reads it as the price)
    int32_t mFatigueEffect;       // 0x08  the ride record's FatigueEffect (slot 55)
    int32_t mRipOffOK;            // 0x0c  the ride record's RipOffOK (slot 88)
    uint8_t mAvailable;           // 0x10  nonzero: the object type has been researched (the record's CostOfResearch was 0)
    uint8_t mF11[3];              // 0x11
    int32_t mUpgradesAvailable;   // 0x14  above 0: upgrades of the type are available
    int32_t mNumberInPark;        // 0x18  how many of the type are in the park (CObject::CObject adds 1)
    uint32_t mFirstBuiltTurn;     // 0x1c  the turn of the first build (Object.cpp)
};
static_assert(sizeof(ObjectControl) == 0x20, "an object control is 0x20 bytes");
static_assert(offsetof(ObjectControl, mUpgradeCost) == 4 && offsetof(ObjectControl, mFatigueEffect) == 8 &&
              offsetof(ObjectControl, mRipOffOK) == 0xc, "ObjectControl first words");
static_assert(offsetof(ObjectControl, mAvailable) == 0x10 && offsetof(ObjectControl, mUpgradesAvailable) == 0x14 &&
              offsetof(ObjectControl, mNumberInPark) == 0x18 && offsetof(ObjectControl, mFirstBuiltTurn) == 0x1c, "ObjectControl layout");

// Unit w176 (src/game/ControlManager.cpp): the two SamsUtilities::USwapInplace32 instances the object control's file functions use; each swaps
// the word where it stands (lwz; stwbrx on the Mac).
namespace ControlSwap {
void USwapInplace32ULong(uint32_t& word);   // 0x100fa5fc: USwapInplace32<unsigned long> (12 bytes)
void USwapInplace32Long(int32_t& word);     // 0x100fa644: USwapInplace32<long> (12 bytes)
}
