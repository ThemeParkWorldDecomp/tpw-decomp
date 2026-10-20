// OptionsBalance.cpp: main app COptionsBalance. Original offsets are PowerPC offsets.
#pragma once
#include "CBalanceTable.h"

class COptionsBalance : public CBalanceTable {
public:
    COptionsBalance() { Init(); }  // inline in 0x100215a0
    ~COptionsBalance();  // 0x100216c0, nonvirtual
    const BalanceDescription* GetBalanceDescription(int32_t row) override; // 0x10021628
    BalanceValue* GetBalanceValuesStart() override; // 0x10021678
    BalanceValue mValues[28];  // 0x08, includes reserved slot zero
};
extern const BalanceDescription gOptionsBalanceDescriptions[32]; // 0x102990a8
extern COptionsBalance gOptionsBalance; // 0x102b4da8, original symbol name not recovered
