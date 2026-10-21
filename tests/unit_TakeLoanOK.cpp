// Test library for TakeLoanOK (unit v5754; libtpw_unit_TakeLoanOK.dylib, tests/test_TakeLoanOK.py), main app:
//   src/game/ui/ui_loansPanel.cpp
// Fakes: CWorld::GetBankAccount, CBankAccount::ApplyForLoan, InterfaceWindow::GetWindow, FillLenderList. Pointers are recorded as raw values.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "LoansPanelMsg.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_controls.h"

using namespace calllog;

CWorld* gGameWorld;
InterfaceWindow* gLoansPanel;

static int64_t P(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {P(this)});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(ret("GetBankAccount")));
}

bool CBankAccount::ApplyForLoan(uint32_t loan) {
    ev("ApplyForLoan", {P(this), static_cast<int64_t>(loan)});
    return ret("ApplyForLoan") != 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {P(this), static_cast<int64_t>(id)});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(ret("GetWindow")));
}

void FillLenderList(InterfaceListControl2* list) { ev("FillLenderList", {P(list)}); }

extern "C" {

uint64_t tpw_tlo_addr(const char* name) {
    if (std::strcmp(name, "world") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gGameWorld));
    if (std::strcmp(name, "panel") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gLoansPanel));
    return 0;
}

int32_t tpw_tlo_layout(int64_t* out) {
    (void)out;
    return 0;
}

uint32_t tpw_tlo_run(uint32_t parameter) {
    return static_cast<uint32_t>(TakeLoanOK(reinterpret_cast<void*>(static_cast<uintptr_t>(parameter))));
}

}  // extern "C"
