// bullfrog_CPU.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3,
// functions at 0x100156b0..0x10015788; original file name unknown, see bullfrog_CPU.h).
#include "bullfrog_CPU.h"

#include "platform/interfacelib.h"

namespace bullfrog {

int32_t gCPUClockMHz;

void CPU::detect() {
    if (gCPUClockMHz == 0) {
        int32_t hz;
        if (Gestalt(0x70636c6b /* 'pclk' */, &hz) == 0) gCPUClockMHz = hz / 1000000;
    }
    BlockMoveData("FunkyPowerPC", mName, 13);
    mType = 6;
    mHasRDTSC = 1;
    mFeatures = 0;
}

int32_t CPU::clocks_per_millisec(int32_t unused) {
    return gCPUClockMHz * 1000;
}

}  // namespace bullfrog
