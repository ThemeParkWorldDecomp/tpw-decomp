// HoardingsBalance.cpp (SimTheme Park 2.04b3).
// PARTIAL: only 0x1002112c, 0x1002117c, 0x100211c4. No other owner code.
#include "HoardingsBalance.h"
#include <new>

// Typed description rows at 0x102948a8, ending with type 12.
const BalanceDescription gHoardingsBalanceDescriptions[5] = {
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "X", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "Y", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "HoardingClicks", 0, 0, 0.0f, 0.0f, 64, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* CHoardingsBalance::GetBalanceDescription(int32_t row) {
    return &gHoardingsBalanceDescriptions[row];
}
BalanceValue* CHoardingsBalance::GetBalanceValuesStart() { return mValues; }
CHoardingsBalance::~CHoardingsBalance() { UnInit(); }
