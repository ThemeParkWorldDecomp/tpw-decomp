// extern "C" wrappers for OptionsBalance.cpp; pointer-bearing fields use accessors.
#include "OptionsBalance.h"
#include "balance_shim_support.h"

extern "C" {
void tpw_OptionsBalance_seed(COptionsBalance* p, uint32_t rows, const uint32_t* values) { BalanceSeed(p, rows, values, 28); }
void tpw_OptionsBalance_read(COptionsBalance* p, uint64_t* rows, uint64_t* values) { BalanceRead(p, rows, values, 28); }
void* tpw_OptionsBalance_create(uint32_t rows, const uint32_t* values) { return BalanceCreate<COptionsBalance>(rows, values, 28); }
int64_t tpw_OptionsBalance_description(COptionsBalance* p, int32_t row) {
    return reinterpret_cast<intptr_t>(p->COptionsBalance::GetBalanceDescription(row)) - reinterpret_cast<intptr_t>(gOptionsBalanceDescriptions);
}
const BalanceDescription* tpw_OptionsBalance_table() { return gOptionsBalanceDescriptions; }
int32_t tpw_OptionsBalance_values(COptionsBalance* p) {
    auto* result = p->COptionsBalance::GetBalanceValuesStart();
    return 8 + static_cast<int32_t>((reinterpret_cast<uintptr_t>(result) - reinterpret_cast<uintptr_t>(p->mValues)) / sizeof(BalanceValue)) * 4;
}
void* tpw_OptionsBalance_destroy(COptionsBalance* p, int16_t flag, uint64_t* rows, uint64_t* values) { return BalanceDestroy(p, flag, rows, values, 28); }
void tpw_OptionsBalance_release(void* p) { ::operator delete(p); }
void* tpw_OptionsBalance_global() { return &gOptionsBalance; }
void tpw_sinit_OptionsBalance() { new (&gOptionsBalance) COptionsBalance; }
}
