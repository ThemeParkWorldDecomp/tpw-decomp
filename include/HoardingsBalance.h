// HoardingsBalance.cpp: main app CHoardingsBalance. Original offsets are PowerPC offsets.
// PARTIAL: only the three emitted inline members; the larger owner is untranslated.
#pragma once
#include "CBalanceTable.h"
#include "platform/msl.h"

class CHoardingsBalance : public CBalanceTable {
public:
    CHoardingsBalance() = default;
    ~CHoardingsBalance();  // 0x100211c4, nonvirtual
    const BalanceDescription* GetBalanceDescription(int32_t row) override; // 0x1002112c
    BalanceValue* GetBalanceValuesStart() override; // 0x1002117c
    BalanceValue mValues[130];  // 0x08, includes reserved slot zero
};
extern const BalanceDescription gHoardingsBalanceDescriptions[5]; // 0x102948a8
// The two tables __sinit_HoardingsBalance_cpp (0x10021074) builds (src/game/balance/HoardingsBalance.cpp, unit w117): CGame::InitLevel_Load loads
// the level's "hoardings.sam" into the first and "endhoardings.sam" into the second. Original names unknown.
extern CHoardingsBalance gHoardingsBalance434C;   // 0x102b434c
extern CHoardingsBalance gHoardingsBalance4130;   // 0x102b4130
