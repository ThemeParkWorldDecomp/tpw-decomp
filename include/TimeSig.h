// CTimeSig inline members, emitted after Text.cpp's static initializer.
#pragma once
#include "Serialise.h"
#include "WorldSavvy.h"
class CTimeSig : public CWorldSavvy {
public:
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t flags) { // 0x1015ef5c
        if (version < 0 || version >= 2) return 0;
        return (save ? SAVEVAR(file, reinterpret_cast<MacULong*>(&mTimeSig), "mTimeSig", 0)
                     : LOADVAR(file, reinterpret_cast<MacULong*>(&mTimeSig), "mTimeSig", 0)) != 0;
    }
    void SetTimeSig() { mTimeSig = gWorld->mTurn; } // 0x1015f02c
    int32_t TurnsSinceLastUpdate() const {         // 0x1015f070
        const CWorld& world = GetWorld();
        uint32_t stamp = mTimeSig; // re-read after GetWorld, as in the listing
        return static_cast<int32_t>(world.mTurn - stamp);
    }
    uint32_t GetTimeSecond() const { return mTimeSig >> 2; } // 0x1015f0e8
    int32_t SecondsSinceLastUpdate() const {       // 0x1015f124
        const CWorld& world = GetWorld();
        uint32_t stamp = mTimeSig;
        return static_cast<int32_t>((world.mTurn >> 2) - (stamp >> 2));
    }
    uint32_t mTimeSig; // 0x00, four turns per second
};
