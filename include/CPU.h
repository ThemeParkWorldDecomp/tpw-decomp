// CPU.h: processor description (engine library, functions at 0x1000006c..0x1000014c, the
// first code in the engine; the original file name is unknown, the name here is the
// class's). gei_System embeds one at +0x28.
#pragma once
#include <cstdint>

class CPU {
public:
    // Reads the clock speed once (Gestalt 'pclk') into gCPUClockMHz, then fills in the
    // fixed description of the Mac build ("FunkyPowerPC", type 6).
    void detect();
    // Clock cycles per millisecond; the argument is not used.
    int32_t clocks_per_millisec(int32_t unused);

    int32_t mType;       // 0x00  6
    int32_t mF04;        // 0x04  0
    int32_t mF08;        // 0x08  1
    int32_t mF0C;        // 0x0c  0
    char mName[16];      // 0x10  "FunkyPowerPC" (13 bytes copied)
};
static_assert(sizeof(CPU) == 0x20, "CPU layout");

extern int32_t gCPUClockMHz;   // engine data 0x1005245c (name invented)
