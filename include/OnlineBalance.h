// OnlineBalance.cpp: main app COnlineBalance. Original offsets are PowerPC offsets.
#pragma once
#include "CBalanceTable.h"

class COnlineBalance : public CBalanceTable {
public:
    COnlineBalance() { Init(); }  // inline in 0x100213fc
    ~COnlineBalance();  // 0x10021510, nonvirtual
    const BalanceDescription* GetBalanceDescription(int32_t row) override; // 0x10021480
    BalanceValue* GetBalanceValuesStart() override; // 0x100214cc
    BalanceValue mValues[14];  // 0x08, includes reserved slot zero
};
extern const BalanceDescription gOnlineBalanceDescriptions[16]; // 0x10298cbc
extern COnlineBalance gOnlineBalance; // 0x102b4d5c, original symbol name not recovered
