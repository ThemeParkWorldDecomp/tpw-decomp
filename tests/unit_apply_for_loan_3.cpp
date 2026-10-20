// Test library for apply_for_loan_3 and apply_for_loan_4 (unit v5036), src/ui_Postcard_panel_spawn_handyman_apply_for_loan_3.cpp.
// Callees (GetBankAccount, ApplyForLoan, CTRACE) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "keytable_actions.h"
#include "World.h"
#include "ThingReceivers.h"
#include "Debug.h"

using namespace calllog;

CWorld* gGameWorld;

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(0x1000));
}

bool CBankAccount::ApplyForLoan(uint32_t loan) {
    ev("ApplyForLoan", {lab(this), static_cast<int64_t>(loan)});
    return ret("ApplyForLoan") != 0;
}

void CTRACE(uint32_t level, const char* format, ...) {
    ev("CTRACE", {static_cast<int64_t>(level)}, {format});
}

extern "C" {

uint64_t tpw_afl_addr(const char* name) {
    if (std::strcmp(name, "gGameWorld") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gGameWorld));
    return 0;
}

uint64_t tpw_afl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_afl_layout(int64_t* out) {
    (void)out;
    return 0;
}

uint32_t tpw_afl_3() { return static_cast<uint32_t>(apply_for_loan_3()); }
uint32_t tpw_afl_4() { return static_cast<uint32_t>(apply_for_loan_4()); }

}  // extern "C"
