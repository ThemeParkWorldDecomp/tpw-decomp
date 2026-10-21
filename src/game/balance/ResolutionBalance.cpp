// ResolutionBalance.cpp: name proven by __sinit_ResolutionBalance_cpp in the main application.

#include "ResolutionBalance.h"

// ResolutionBalance.cpp (SimTheme Park 2.04b3).
// Static initializer 0x1023779c; emitted members 0x10237824, 0x10237874, 0x102378bc.

// Typed description rows at 0x102b2a10, ending with type 12.
const BalanceDescription gResolutionBalanceDescriptions[4] = {
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "RESOLUTION", 0, 8, 0.0f, 0.0f, 0, 0u},
    {1, "Res", 0, 0, 0.0f, 0.0f, 0, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* CResolutionBalance::GetBalanceDescription(int32_t row) {
    return &gResolutionBalanceDescriptions[row];
}
BalanceValue* CResolutionBalance::GetBalanceValuesStart() { return mValues; }
CResolutionBalance::~CResolutionBalance() { UnInit(); }

CResolutionBalance gResolutionBalance; // __sinit_ResolutionBalance_cpp at 0x1023779c, object at 0x103bc640
