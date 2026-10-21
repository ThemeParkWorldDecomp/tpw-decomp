// Test library for the research panel bar messages (unit v5029; libtpw_unit_RideMsg.dylib, tests/test_RideMsg.py), main app:
//   src/game/ui/ui_researchPanel.cpp   RideMsg, UpgMsg, ShopMsg, ShowMsg, FeatureMsg
//
// TankButton and AnimatingPumpMsg are fakes that record their arguments; AnimatingPumpMsg answers from the queue.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "ui_main.h"

using namespace calllog;

void TankButton(int32_t which, InterfaceWindow* window, int32_t flag) {
    ev("TankButton", {which, lab(window), flag});
}

int32_t AnimatingPumpMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("AnimatingPumpMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("AnimatingPumpMsg"));
}

extern "C" {

uint64_t tpw_rmsg_addr(const char*) { return 0; }

uint64_t tpw_rmsg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rmsg_layout(int64_t*) { return 0; }

uint32_t tpw_rmsg_call(int32_t op, uint32_t window, int32_t m, int32_t a, int32_t b) {
    InterfaceWindow* w = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window));
    switch (op) {
        case 0: return static_cast<uint32_t>(RideMsg(w, m, a, b));
        case 1: return static_cast<uint32_t>(UpgMsg(w, m, a, b));
        case 2: return static_cast<uint32_t>(ShopMsg(w, m, a, b));
        case 3: return static_cast<uint32_t>(ShowMsg(w, m, a, b));
        default: return static_cast<uint32_t>(FeatureMsg(w, m, a, b));
    }
}

}  // extern "C"
