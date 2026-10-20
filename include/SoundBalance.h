// SoundBalance.cpp: main app CSoundBalance. Original offsets are PowerPC offsets.
// PARTIAL: only the static initializer is translated. The three virtual/member
// methods below are declarations with weak test placeholders, outside this batch.
// Replace the placeholders when translating them to avoid duplicate vtable emission.
#pragma once
#include "CBalanceTable.h"

class CSoundBalance : public CBalanceTable {
public:
    CSoundBalance() { Init(); }  // inline in 0x10021750
    ~CSoundBalance();  // 0x10021864, nonvirtual
    const BalanceDescription* GetBalanceDescription(int32_t row) override; // 0x100217d4
    BalanceValue* GetBalanceValuesStart() override; // 0x10021820
    BalanceValue mValues[30];  // 0x08, includes reserved slot zero
};
extern const BalanceDescription gSoundBalanceDescriptions[28]; // 0x1029b744
extern CSoundBalance gSoundBalance; // 0x102b4e2c, original symbol name not recovered
