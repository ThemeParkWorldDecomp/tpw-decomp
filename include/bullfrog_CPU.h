// bullfrog_CPU.h: the bullfrog library's own CPU description (functions at
// 0x100156b0..0x10015788; the original file name is unknown). The engine library has a
// different class of the same name (include/CPU.h: one more word before the name), so
// this one lives in namespace bullfrog. TbSystemInfo.cpp's static initialiser detects into
// its global at 0x100748ac; LbSystemInfo_CPUhasMMX reads bit 23 of mFeatures (the x86
// CPUID MMX bit) and LbSystemInfo_CPUhasRDTSC reads mHasRDTSC.
#pragma once
#include <cstdint>

namespace bullfrog {

class CPU {
public:
    // Reads the clock speed once (Gestalt 'pclk') into gCPUClockMHz, then fills in the
    // fixed description of the Mac build ("FunkyPowerPC", type 6, no CPUID features).
    void detect();
    // Clock cycles per millisecond; the argument is not used.
    int32_t clocks_per_millisec(int32_t unused);

    int32_t mType;         // 0x00  6
    uint32_t mFeatures;    // 0x04  0
    int32_t mHasRDTSC;     // 0x08  1
    char mName[16];        // 0x0c  "FunkyPowerPC" (13 bytes copied)
};
static_assert(sizeof(CPU) == 0x1c, "bullfrog CPU layout");

extern int32_t gCPUClockMHz;   // bullfrog data 0x10073e84 (name invented)

}  // namespace bullfrog
