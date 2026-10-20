// ResolutionBalance.cpp: main app CResolutionBalance. Original offsets are PowerPC offsets.
#pragma once
#include "CBalanceTable.h"

class CResolutionBalance : public CBalanceTable {
public:
    CResolutionBalance() { Init(); }  // inline in 0x1023779c
    ~CResolutionBalance();  // 0x102378bc, nonvirtual
    const BalanceDescription* GetBalanceDescription(int32_t row) override; // 0x10237824
    BalanceValue* GetBalanceValuesStart() override; // 0x10237874
    BalanceValue mValues[2];  // 0x08, includes reserved slot zero
};
extern const BalanceDescription gResolutionBalanceDescriptions[4]; // 0x102b2a10
extern CResolutionBalance gResolutionBalance; // 0x103bc640, original symbol name not recovered
