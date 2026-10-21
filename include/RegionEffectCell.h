// CRegionEffectCell (Researcher.cpp in the inventory, unreliable; main app): five shorts, "mpEffect[i]" in its serialiser
// (0x10120cfc). PARTIAL: the constructor (unit w113).
#pragma once
#include <cstdint>

class CRegionEffectCell {
public:
    CRegionEffectCell();    // 0x10120c10 (60 bytes, src/game/people/Researcher.cpp)
    // Unit w188 (src/game/people/Researcher.cpp, where the inventory puts them): Serialise 0x10120cfc (256 bytes), the five shorts; operator== 0x10120c7c (60 bytes), memcmp of the ten bytes == 0.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);
    bool operator==(const CRegionEffectCell& other);   // (the mangled name has no const)

    int16_t mpEffect[5];    // 0x00  zero
};
static_assert(sizeof(CRegionEffectCell) == 10, "a region effect cell is 10 bytes");
