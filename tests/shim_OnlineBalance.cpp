// extern "C" wrappers for OnlineBalance.cpp; pointer-bearing fields use accessors.
#include "OnlineBalance.h"
#include "balance_shim_support.h"

extern "C" {
void tpw_OnlineBalance_seed(COnlineBalance* p, uint32_t rows, const uint32_t* values) { BalanceSeed(p, rows, values, 14); }
void tpw_OnlineBalance_read(COnlineBalance* p, uint64_t* rows, uint64_t* values) { BalanceRead(p, rows, values, 14); }
void* tpw_OnlineBalance_create(uint32_t rows, const uint32_t* values) { return BalanceCreate<COnlineBalance>(rows, values, 14); }
int64_t tpw_OnlineBalance_description(COnlineBalance* p, int32_t row) {
    return reinterpret_cast<intptr_t>(p->COnlineBalance::GetBalanceDescription(row)) - reinterpret_cast<intptr_t>(gOnlineBalanceDescriptions);
}
const BalanceDescription* tpw_OnlineBalance_table() { return gOnlineBalanceDescriptions; }
int32_t tpw_OnlineBalance_values(COnlineBalance* p) {
    auto* result = p->COnlineBalance::GetBalanceValuesStart();
    return 8 + static_cast<int32_t>((reinterpret_cast<uintptr_t>(result) - reinterpret_cast<uintptr_t>(p->mValues)) / sizeof(BalanceValue)) * 4;
}
void* tpw_OnlineBalance_destroy(COnlineBalance* p, int16_t flag, uint64_t* rows, uint64_t* values) { return BalanceDestroy(p, flag, rows, values, 14); }
void tpw_OnlineBalance_release(void* p) { ::operator delete(p); }
void* tpw_OnlineBalance_global() { return &gOnlineBalance; }
void tpw_sinit_OnlineBalance() { new (&gOnlineBalance) COnlineBalance; }
}
