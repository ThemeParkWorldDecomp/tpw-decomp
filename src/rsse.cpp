// The rsse.cpp range of the inventory (decompiled from the main app of SimTheme Park 2.04b3).
// PARTIAL: only BuildGeometryTables (0x100c30a8) is translated so far; see rsse.h for the
// rest of the range.
#include "rsse.h"

#include <cmath>

#include "TPGlobals.h"

namespace {
constexpr float kTwoPi = 6.2831855f;   // 0x40c90fdb
}

void BuildGeometryTables(uint32_t size) {
    gTPGlobals.mSinTableSize = size;
    gTPGlobals.mSinTableMask = size - 1;
    gTPGlobals.mSinTableScale = static_cast<float>(size) / kTwoPi;
    for (uint32_t i = 0; i < gTPGlobals.mSinTableSize; ++i) {
        const float angle = kTwoPi * (static_cast<float>(i) / static_cast<float>(gTPGlobals.mSinTableSize));
        gTPGlobals.mSinTable[i] = static_cast<float>(std::sin(static_cast<double>(angle)));
    }
}
