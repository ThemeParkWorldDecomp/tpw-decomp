// extern "C" wrappers for ResolutionBalance.cpp; pointer-bearing fields use accessors.
#include "ResolutionBalance.h"
#include "balance_shim_support.h"

extern "C" {
void tpw_ResolutionBalance_seed(CResolutionBalance* p, uint32_t rows, const uint32_t* values) { BalanceSeed(p, rows, values, 2); }
void tpw_ResolutionBalance_read(CResolutionBalance* p, uint64_t* rows, uint64_t* values) { BalanceRead(p, rows, values, 2); }
void* tpw_ResolutionBalance_create(uint32_t rows, const uint32_t* values) { return BalanceCreate<CResolutionBalance>(rows, values, 2); }
int64_t tpw_ResolutionBalance_description(CResolutionBalance* p, int32_t row) {
    return reinterpret_cast<intptr_t>(p->CResolutionBalance::GetBalanceDescription(row)) - reinterpret_cast<intptr_t>(gResolutionBalanceDescriptions);
}
const BalanceDescription* tpw_ResolutionBalance_table() { return gResolutionBalanceDescriptions; }
int32_t tpw_ResolutionBalance_values(CResolutionBalance* p) {
    auto* result = p->CResolutionBalance::GetBalanceValuesStart();
    return 8 + static_cast<int32_t>((reinterpret_cast<uintptr_t>(result) - reinterpret_cast<uintptr_t>(p->mValues)) / sizeof(BalanceValue)) * 4;
}
void* tpw_ResolutionBalance_destroy(CResolutionBalance* p, int16_t flag, uint64_t* rows, uint64_t* values) { return BalanceDestroy(p, flag, rows, values, 2); }
void tpw_ResolutionBalance_release(void* p) { ::operator delete(p); }
void* tpw_ResolutionBalance_global() { return &gResolutionBalance; }
void tpw_sinit_ResolutionBalance() { new (&gResolutionBalance) CResolutionBalance; }
}
