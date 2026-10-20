// extern "C" wrappers for HoardingsBalance.cpp; pointer-bearing fields use accessors.
#include "HoardingsBalance.h"
#include "balance_shim_support.h"

extern "C" {
void tpw_HoardingsBalance_seed(CHoardingsBalance* p, uint32_t rows, const uint32_t* values) { BalanceSeed(p, rows, values, 130); }
void tpw_HoardingsBalance_read(CHoardingsBalance* p, uint64_t* rows, uint64_t* values) { BalanceRead(p, rows, values, 130); }
void* tpw_HoardingsBalance_create(uint32_t rows, const uint32_t* values) { return BalanceCreate<CHoardingsBalance>(rows, values, 130); }
int64_t tpw_HoardingsBalance_description(CHoardingsBalance* p, int32_t row) {
    return reinterpret_cast<intptr_t>(p->CHoardingsBalance::GetBalanceDescription(row)) - reinterpret_cast<intptr_t>(gHoardingsBalanceDescriptions);
}
const BalanceDescription* tpw_HoardingsBalance_table() { return gHoardingsBalanceDescriptions; }
int32_t tpw_HoardingsBalance_values(CHoardingsBalance* p) {
    auto* result = p->CHoardingsBalance::GetBalanceValuesStart();
    return 8 + static_cast<int32_t>((reinterpret_cast<uintptr_t>(result) - reinterpret_cast<uintptr_t>(p->mValues)) / sizeof(BalanceValue)) * 4;
}
void* tpw_HoardingsBalance_destroy(CHoardingsBalance* p, int16_t flag, uint64_t* rows, uint64_t* values) { return BalanceDestroy(p, flag, rows, values, 130); }
void tpw_HoardingsBalance_release(void* p) { ::operator delete(p); }
}
