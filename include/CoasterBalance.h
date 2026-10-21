// CCoasterBalance (main app): 0x3b80 bytes on the Mac, native value slots on the host.
#pragma once
#include "CBalanceTable.h"

class CCoasterBalance : public CBalanceTable {
public:
    ~CCoasterBalance();   // 0x10039b30, nonvirtual (src/game/rides/CoasterType.cpp, src/game/rides/TPCoaster.cpp)
    const BalanceDescription* GetBalanceDescription(int32_t row) override; // 0x1004595c
    BalanceValue* GetBalanceValuesStart() override; // 0x100459ac
    BalanceValue mValues[3806];
};
extern const BalanceDescription gCoasterBalanceDescriptions[384]; // 0x1028ec14
