// extern "C" wrappers for SoundBalance.cpp; pointer-bearing fields use accessors.
#include "SoundBalance.h"
#include "balance_shim_support.h"

extern "C" {
void tpw_SoundBalance_seed(CSoundBalance* p, uint32_t rows, const uint32_t* values) { BalanceSeed(p, rows, values, 30); }
void tpw_SoundBalance_read(CSoundBalance* p, uint64_t* rows, uint64_t* values) { BalanceRead(p, rows, values, 30); }
void* tpw_SoundBalance_create(uint32_t rows, const uint32_t* values) { return BalanceCreate<CSoundBalance>(rows, values, 30); }
int64_t tpw_SoundBalance_description(CSoundBalance* p, int32_t row) {
    return reinterpret_cast<intptr_t>(p->CSoundBalance::GetBalanceDescription(row)) - reinterpret_cast<intptr_t>(gSoundBalanceDescriptions);
}
const BalanceDescription* tpw_SoundBalance_table() { return gSoundBalanceDescriptions; }
int32_t tpw_SoundBalance_values(CSoundBalance* p) {
    auto* result = p->CSoundBalance::GetBalanceValuesStart();
    return 8 + static_cast<int32_t>((reinterpret_cast<uintptr_t>(result) - reinterpret_cast<uintptr_t>(p->mValues)) / sizeof(BalanceValue)) * 4;
}
void* tpw_SoundBalance_destroy(CSoundBalance* p, int16_t flag, uint64_t* rows, uint64_t* values) { return BalanceDestroy(p, flag, rows, values, 30); }
void tpw_SoundBalance_release(void* p) { ::operator delete(p); }
void* tpw_SoundBalance_global() { return &gSoundBalance; }
void tpw_sinit_SoundBalance() { new (&gSoundBalance) CSoundBalance; }
}
