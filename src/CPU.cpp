// CPU.cpp (decompiled from the engine library of SimTheme Park 2.04b3, functions at
// 0x1000006c..0x1000014c; original file name unknown).
#include "CPU.h"

#include "platform/interfacelib.h"

int32_t gCPUClockMHz;

void CPU::detect() {
    if (gCPUClockMHz == 0) {
        int32_t hz;
        if (Gestalt(0x70636c6b /* 'pclk' */, &hz) == 0) gCPUClockMHz = hz / 1000000;
    }
    BlockMoveData("FunkyPowerPC", mName, 13);
    mType = 6;
    mF08 = 1;
    mF04 = 0;
    mF0C = 0;
}

int32_t CPU::clocks_per_millisec(int32_t unused) {
    return gCPUClockMHz * 1000;
}
