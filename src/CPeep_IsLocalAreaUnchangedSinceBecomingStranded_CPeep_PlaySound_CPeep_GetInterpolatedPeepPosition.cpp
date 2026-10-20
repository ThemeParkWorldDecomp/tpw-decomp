// CPeep::GetInterpolatedPeepPosition (0x101147e8) and CPeep::Serialise (0x10113138), both from Peep.cpp.
#include <cstdint>
#include <cstring>
#include "analyser.h"
#include "Debug.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "World.h"
#include "WorldSavvy.h"
#include "ppc_math.h"
#include "Serialise.h"
#include "ESP_sprites.h"

// Runtime glue 0x10239940, not translated anywhere yet: a weak empty stand-in so the launcher links (the Peep.cpp static initialiser needs it).
extern "C" __attribute__((weak)) void __register_global_object(void*, void*, void*) {}

namespace {

uint8_t* PeepBytes(CPeep* peep) { return reinterpret_cast<uint8_t*>(peep); }

// The Mac's int -> single conversion of the magic-number trick: the exact int, rounded to single.
float IntToSingle(int32_t value) { return static_cast<float>(static_cast<double>(value)); }

}  // namespace

// 0x101147e8: the share is clamped (to -1..2, or to 0..1 when `ranged` is not 0; a NaN becomes the low end), the position is the pilot's
// current (x, y) mixed with the last one at +0x190 / +0x194 by a fused multiply-add, cut to an int, and scaled by 10 / 65536.
// (A NaN share is not clamped.)
void CPeep::GetInterpolatedPeepPosition(float t, int32_t ranged, float& x, float& y, float& z, uint32_t& stamp) {
    uint8_t* self = PeepBytes(this);
    float low, high;
    if (ranged != 0) {
        low = 0.0f;
        high = 1.0f;
    } else {
        low = -1.0f;
        high = 2.0f;
    }
    if (t < low) t = low;          // bge / ble test the LT / GT bits: a NaN passes both and stays a NaN
    else if (t > high) t = high;

    int32_t curX, curY;
    reinterpret_cast<Pilot*>(self + 0xd4)->get_pos(curX, curY);

    int32_t lastX, lastY, word;
    std::memcpy(&lastX, self + 0x190, 4);
    std::memcpy(&lastY, self + 0x194, 4);
    const float dx = IntToSingle(static_cast<int32_t>(static_cast<uint32_t>(curX) - static_cast<uint32_t>(lastX)));
    const float ax = IntToSingle(lastX);
    const float dy = IntToSingle(static_cast<int32_t>(static_cast<uint32_t>(curY) - static_cast<uint32_t>(lastY)));
    const float ay = IntToSingle(lastY);
    const int32_t ix = ppc_fctiwz(static_cast<double>(ppc_fmadds(t, dx, ax)));
    const int32_t iy = ppc_fctiwz(static_cast<double>(ppc_fmadds(t, dy, ay)));
    x = (10.0f * IntToSingle(ix)) / 65536.0f;
    y = 0.0f;
    z = (10.0f * IntToSingle(iy)) / 65536.0f;
    std::memcpy(&word, self + 0x1c, 4);
    stamp = static_cast<uint32_t>(word);
}

// 0x10113138: the thing, the script, then (when the version is 0 or 1) the peep's own fields in the same order saving and loading, the vehicle and
// the thoughts; any failure answers 1. A load wraps the sprite id into the sprite set. At the end (also when the version is out of range) the peep
// walks to its saved destination again.
int32_t CPeep::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    uint8_t* self = PeepBytes(this);
    if (reinterpret_cast<CThing*>(this)->Serialise(file, save, version, unused) != 0) return 1;
    if (reinterpret_cast<CDelayedScript*>(self + 0xc)->Serialise(file, save, version, unused) != 0) return 1;

    auto* destX = reinterpret_cast<int16_t*>(self + 0x18);
    auto* destY = reinterpret_cast<int16_t*>(self + 0x1a);
    auto* adjustorSpeed = reinterpret_cast<uint16_t*>(self + 0xc4);
    auto* baseSpeed = reinterpret_cast<uint16_t*>(self + 0xc0);
    auto* count = reinterpret_cast<uint8_t*>(self + 0x2c);
    auto* sprite = reinterpret_cast<ESP_Sprites*>(self + 0x24);
    auto* lastMap = reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>*>(self + 0xcc);
    auto* previousSpeed = reinterpret_cast<float*>(self + 0xc8);
    auto* previousX = reinterpret_cast<MacLong*>(self + 0x190);
    auto* previousY = reinterpret_cast<MacLong*>(self + 0x194);
    auto* strandedTime = reinterpret_cast<MacULong*>(self + 0x198);
    auto* purposeSpeed = reinterpret_cast<uint16_t*>(self + 0xc2);
    auto* setDestOk = reinterpret_cast<int32_t*>(self + 0xd0);
    auto* spriteAngle = reinterpret_cast<MacULong*>(self + 0x1c);
    auto* spriteId = reinterpret_cast<uint32_t*>(self + 0x20);
    auto* underRide = reinterpret_cast<int32_t*>(self + 0x28);
    auto* vehicle = reinterpret_cast<Vehicle*>(self + 0xd4);
    auto* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);

    if (save != 0) {
        if (version < 2 && version >= 0) {
            if (SAVEVAR(file, destX, "mAccurateDestX", 0) != 0) return 1;
            if (SAVEVAR(file, destY, "mAccurateDestY", 0) != 0) return 1;
            if (SAVEVAR(file, adjustorSpeed, "mAdjustorSpeed", 0) != 0) return 1;
            if (SAVEVAR(file, baseSpeed, "mBaseSpeed", 0) != 0) return 1;
            if (SAVEVAR(file, count, "mCount", 0) != 0) return 1;
            if (SAVEVAR(file, sprite, "mESPSprite", 0) != 0) return 1;
            if (SAVEVARSTRUCT(file, lastMap, "mLastRecordedMapId", 0) != 0) return 1;
            if (vehicle->Serialise(file, save, version, unused) != 0) return 1;
            if (SAVEVAR(file, previousSpeed, "mPreviousSpeed", 0) != 0) return 1;
            if (SAVEVAR(file, previousX, "mPreviousX", 0) != 0) return 1;
            if (SAVEVAR(file, previousY, "mPreviousY", 0) != 0) return 1;
            if (SAVEVAR(file, strandedTime, "mStrandedTime", 0) != 0) return 1;
            if (SAVEVAR(file, purposeSpeed, "mPurposeSpeed", 0) != 0) return 1;
            if (SAVEVAR(file, setDestOk, "mSetDestSuccessfully", 0) != 0) return 1;
            if (SAVEVAR(file, spriteAngle, "mSpriteAngle", 0) != 0) return 1;
            if (SAVEVAR(file, spriteId, "mSpriteID", 0) != 0) return 1;
            if (SAVEVAR(file, underRide, "mSpriteUnderRideCtrl", 0) != 0) return 1;
            if (thoughts->Serialise(file, save, version, unused) != 0) return 1;
        }
    } else if (version < 2 && version >= 0) {
        if (LOADVAR(file, destX, "mAccurateDestX", 0) != 0) return 1;
        if (LOADVAR(file, destY, "mAccurateDestY", 0) != 0) return 1;
        if (LOADVAR(file, adjustorSpeed, "mAdjustorSpeed", 0) != 0) return 1;
        if (LOADVAR(file, baseSpeed, "mBaseSpeed", 0) != 0) return 1;
        if (LOADVAR(file, count, "mCount", 0) != 0) return 1;
        if (LOADVAR(file, sprite, "mESPSprite", 0) != 0) return 1;
        if (LOADVARSTRUCT(file, lastMap, "mLastRecordedMapId", 0) != 0) return 1;
        if (vehicle->Serialise(file, 0, version, unused) != 0) return 1;
        if (LOADVAR(file, previousSpeed, "mPreviousSpeed", 0) != 0) return 1;
        if (LOADVAR(file, previousX, "mPreviousX", 0) != 0) return 1;
        if (LOADVAR(file, previousY, "mPreviousY", 0) != 0) return 1;
        if (LOADVAR(file, strandedTime, "mStrandedTime", 0) != 0) return 1;
        if (LOADVAR(file, purposeSpeed, "mPurposeSpeed", 0) != 0) return 1;
        if (LOADVAR(file, setDestOk, "mSetDestSuccessfully", 0) != 0) return 1;
        if (LOADVAR(file, spriteAngle, "mSpriteAngle", 0) != 0) return 1;
        if (LOADVAR(file, spriteId, "mSpriteID", 0) != 0) return 1;
        const uint32_t sprites = static_cast<uint32_t>(ESP_GetNumberOfSprites(*sprite));
        *spriteId = *spriteId - (*spriteId / sprites) * sprites;
        if (LOADVAR(file, underRide, "mSpriteUnderRideCtrl", 0) != 0) return 1;
        if (thoughts->Serialise(file, 0, version, unused) != 0) return 1;
    }

    MapPos x, y;
    std::memcpy(&x, self + 0x18, sizeof(MapPos));
    std::memcpy(&y, self + 0x1a, sizeof(MapPos));
    if (SetDest(x, y) == 0) CTRACE(2, "Couldn't renav to my accurate dest!!");
    return 0;
}
