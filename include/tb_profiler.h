// tb_profiler.h: the engine profiler (engine library, original tb_profiler.cpp).
//
// PARTIAL: the profiler's records (0x0000..0x10ff) are not recovered yet; the size is a
// lower bound (the next known global starts 0x111c bytes later).
#pragma once
#include <cstdint>

class CProfiler {
public:
    // Inline in the original (__sinit_tb_profiler_cpp, 0x1001fcec, stores these four).
    CProfiler() : mF1100(1), mF1104(1), mF1108(1), mF110C(1.0f) {}

    // Low 32 bits of SamsUtilities::UTimer::GetAbsoluteCycles().
    int32_t GetClockCycleCount10h();

    uint8_t mRecords[0x1100];   // 0x0000  not recovered yet
    int32_t mF1100;             // 0x1100
    int32_t mF1104;             // 0x1104
    int32_t mF1108;             // 0x1108
    float mF110C;               // 0x110c
};

extern CProfiler gProfiler;   // engine data 0x100b050c (not exported; name invented)
