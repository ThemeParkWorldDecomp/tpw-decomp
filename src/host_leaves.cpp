// Four small leaves of the main app (SimTheme Park 2.04b3) that the effect manager (src/effect_manager.cpp) calls
// every pass: the real-time clock that stands still while the game is paused (RealTimePauseAdjusted,
// 0x10007c68, the inventory's Advisor.cpp range) and the table of sines (HOST_Sin and HOST_Cos, 0x100218f4 and
// 0x10021928, and BMP_GenerateSinTab, 0x100259d0, the inventory's TPAdvisor.cpp range). See
// include/effect_manager.h.
#include <cmath>
#include <cstdint>

#include "Advisor.h"
#include "Clock.h"
#include "effect_manager.h"
#include "ppc_math.h"

int32_t gHostSineTable[512];   // main app data 0x102b4ec0 (not exported; the name is invented)

namespace {

// BMP_GenerateSinTab's constants (main app data 0x102645a8..0x102645c8). The "pi" is the decimal 3.14159, not
// the double nearest to pi, so the table's quarter-turn entries come out one short of 256 (255 and -255).
constexpr double kPi = 3.14159;        // 0x102645c0
constexpr double kEntryAngle = 0.001953125;   // 0x102645a8: 2^-9, so that 2 i entries make 2 pi i / 512 turns
constexpr double kAmplitude = 256.0;   // 0x102645c8

}  // namespace

// The game clock in milliseconds less the time it has spent paused, and while it is paused the moment the pause
// began: the same count as the advisor's clock (SAdvisorState keeps the pause: it is told of a pause here, on the
// first read after it, and of its end on the first read after that). The reads of the clock are in the
// original's order: the time first, then the pause flag.
uint32_t RealTimePauseAdjusted() {
    SAdvisorState& state = gAdvisorState;
    const int32_t now = gClock.ReadFixed();
    if (gClock.GetPauseFlag() != 0) {
        if (state.mClockPaused == 0) {
            state.mClockPaused = 1;
            state.mPauseStart = now;
        }
        return static_cast<uint32_t>(state.mPauseStart - state.mPausedTotal);
    }
    if (state.mClockPaused != 0) {
        state.mClockPaused = 0;
        state.mPausedTotal = state.mPausedTotal + (now - state.mPauseStart);
    }
    return static_cast<uint32_t>(now - state.mPausedTotal);
}

// The angle is in 512ths of a turn (only the low nine bits count, so negative angles and any multiple of a turn
// work); the answers are 1024 times the sine and cosine, rounded as the table's 8-bit steps are.
int32_t HOST_Sin(int32_t angle) {
    return static_cast<int32_t>(static_cast<uint32_t>(gHostSineTable[angle & 0x1ff]) << 2);
}

// The cosine is the sine a quarter turn (128 entries) on.
int32_t HOST_Cos(int32_t angle) {
    return static_cast<int32_t>(static_cast<uint32_t>(gHostSineTable[(static_cast<uint32_t>(angle) + 0x80) & 0x1ff]) << 2);
}

// Fills the table: entry i is 256 * sin(3.14159 * 2i * 2^-9) cut toward zero. (The double product is taken in
// the listing's order: pi times 2i first, then the scale.)
void BMP_GenerateSinTab() {
    for (int32_t i = 0; i < 0x200; ++i) {
        const double angle = kPi * static_cast<double>(i << 1) * kEntryAngle;
        gHostSineTable[i] = ppc_fctiwz(kAmplitude * std::sin(angle));
    }
}
