// extern "C" wrappers for MainBalance.cpp; pointer-bearing fields use accessors.
#include "MainBalance.h"
#include "balance_shim_support.h"

extern "C" {
void tpw_MainBalance_seed(CMainBalance* p, uint32_t rows, const uint32_t* values) { BalanceSeed(p, rows, values, 504); }
void tpw_MainBalance_read(CMainBalance* p, uint64_t* rows, uint64_t* values) { BalanceRead(p, rows, values, 504); }
void* tpw_MainBalance_create(uint32_t rows, const uint32_t* values) { return BalanceCreate<CMainBalance>(rows, values, 504); }
int64_t tpw_MainBalance_description(CMainBalance* p, int32_t row) {
    return reinterpret_cast<intptr_t>(p->CMainBalance::GetBalanceDescription(row)) - reinterpret_cast<intptr_t>(gMainBalanceDescriptions);
}
const BalanceDescription* tpw_MainBalance_table() { return gMainBalanceDescriptions; }
int32_t tpw_MainBalance_values(CMainBalance* p) {
    auto* result = p->CMainBalance::GetBalanceValuesStart();
    return 8 + static_cast<int32_t>((reinterpret_cast<uintptr_t>(result) - reinterpret_cast<uintptr_t>(p->mValues)) / sizeof(BalanceValue)) * 4;
}
void* tpw_MainBalance_destroy(CMainBalance* p, int16_t flag, uint64_t* rows, uint64_t* values) { return BalanceDestroy(p, flag, rows, values, 504); }
void tpw_MainBalance_release(void* p) { ::operator delete(p); }
void* tpw_MainBalance_global() { return &gMainBalance; }
void tpw_sinit_MainBalance() { new (&gMainBalance) CMainBalance; }
}
