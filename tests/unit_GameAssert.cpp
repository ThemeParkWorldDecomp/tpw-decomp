// Test library for src/GameAssert.cpp (libtpw_unit_GameAssert.dylib; see
// tests/test_BalanceTable.py): GAME_ASSERTMSG called through one entry point. The original
// does nothing observable on the host, so the test only checks that the call returns and
// leaves a canary and a global alone.
#include <cstdint>

#include "Debug.h"

extern "C" {
int32_t tpw_ga_canary = 0x13572468;

// Calls GAME_ASSERTMSG with the arguments of a failed check and returns the canary.
int32_t tpw_ga_call(int32_t condition, const char* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    GAME_ASSERTMSG(condition, text, a, b, c, d, e);
    return tpw_ga_canary;
}
}
