// FLY ride-base accessors on gFlyBases (HLSprite.cpp): the fly handle indexes the table (1 to 99), a NULL slot is ignored.
// The base structure has no declared layout; fields are read by Mac offset (all words).
#include <cstdint>

#include "HLSprite.h"
#include "rsse.h"

namespace {

inline int32_t& W(void* base, int32_t offset) { return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(base) + offset); }

inline void* BaseOf(int32_t fly) {
    if (fly <= 0 || fly >= 100) return nullptr;
    return gFlyBases[fly];
}

}  // namespace

// 0x10078ee4: the original is void (r3 is whatever was left); the header says int32_t.
int32_t FLY_SetNumberOfCars(int32_t fly, int32_t a) {
    void* b = BaseOf(fly);
    if (b == nullptr) return 0;
    W(b, 4) = a;
    if (W(b, 0x38) == 0) return 0;
    W(b, 0) = a;
    return 0;
}

// 0x10078fb0
int32_t FLY_GetBrokenStat(int32_t fly) {
    void* b = BaseOf(fly);
    if (b != nullptr && W(b, 0x114) != 0 && W(b, 0xdc) == 1) return 1;
    return 0;
}

// 0x10079024
int32_t FLY_LaunchCar(int32_t fly) {
    void* b = BaseOf(fly);
    if (b == nullptr) return 0;
    if (W(b, 0x38) == 0 && W(b, 0x48) == 0 && W(b, 0xc) <= 1) return 0;
    if (W(b, 0x3c) != 0) {
        W(b, 0x3c) = 0;
        return 1;
    }
    return 0;
}

// 0x100791c0
int32_t FLY_RideRunning(int32_t fly) {
    void* b = BaseOf(fly);
    if (b == nullptr) return 0;
    if (W(b, 0x48) == 0 && W(b, 0x3c) != 0 && W(b, 0xc) == 1) return 0;
    return 1;
}

// 0x10078a58: void in the original.
int32_t FLY_SetOpenClosed(int32_t fly, int32_t a) {
    void* b = BaseOf(fly);
    if (b == nullptr) return 0;
    if (a != 0) {
        W(b, 0) = W(b, 4);
        W(b, 0x38) = 1;
        return 0;
    }
    if (W(b, 0x34) == 3) return 0;
    W(b, 0) = 1;
    for (int32_t i = 0; i < 0x14; ++i) {
        uint8_t* e = static_cast<uint8_t*>(b) + i * 0xe4;
        if (W(e, 0xdc) != 0) W(e, 0xe0) = 0;
        if (W(e, 0xdc) == 1) {
            int32_t v = W(e, 0xe8);
            if (v > 0) W(e, 0xe8) = -v;
        }
        W(b, 0x38) = 0;
    }
    return 0;
}

// 0x100789ec: void in the original; signed divide by 10 (mulhw trick).
void FLY_SetRunSpeed(int32_t fly, int32_t speed) {
    void* b = BaseOf(fly);
    if (b == nullptr) return;
    W(b, 0x50) = speed / 10;
}

// 0x10078958: void; time / 64 truncating toward zero, then clamps the 20 entries' word at 0xe0.
void FLY_SetRunTime(int32_t fly, int32_t time) {
    void* b = BaseOf(fly);
    if (b == nullptr) return;
    int32_t t = time / 64;
    W(b, 0x10) = t;
    for (int32_t i = 0; i < 0x14; ++i) {
        int32_t& v = W(static_cast<uint8_t*>(b) + i * 0xe4, 0xe0);
        if (v > t) v = t;
    }
}
