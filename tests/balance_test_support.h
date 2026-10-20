// Ordered observations and controls for untranslated balance/string callees.
#pragma once
#include <cstdint>

struct BalanceTestEvent {
    int32_t mKind;              // 1 Init, 2 UnInit, 4 string constructor
    int32_t mType;              // 0 Hoardings, 1 Main, 2 Online, 3 Options, 4 Sound, 5 Resolution
    int32_t mAllocBefore;
    const void* mObject;
    uintptr_t mRows;
    char mText[64];
};
extern "C" {
extern int32_t tpw_balance_log_on;
extern int32_t tpw_balance_log_n;
extern BalanceTestEvent tpw_balance_log[64];
extern uint32_t tpw_balance_mask;
}
