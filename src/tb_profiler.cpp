// tb_profiler.cpp (engine library of SimTheme Park 2.04b3).
// PARTIAL: the profiler object, its static initialiser (0x1001fcec, the inline
// constructor) and CProfiler::GetClockCycleCount10h (0x1001fc8c), translated with
// tb_matrix.cpp, which uses them. The rest of the file is still to do.
#include "tb_profiler.h"

#include "platform/samsutils.h"

CProfiler gProfiler;

int32_t CProfiler::GetClockCycleCount10h() {
    return static_cast<int32_t>(static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsoluteCycles()));
}
