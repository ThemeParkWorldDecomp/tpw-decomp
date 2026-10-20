// Test library for AnimatingPumpMsg (unit v5197; libtpw_unit_AnimatingPumpMsg.dylib, tests/test_AnimatingPumpMsg.py).
// The InterfaceWindow callees are recording fakes; the window is a fake address that is never dereferenced.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"

using namespace calllog;

int32_t AnimatingPumpMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

static int32_t W(void* p) { return static_cast<int32_t>(reinterpret_cast<intptr_t>(p)); }

void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("get", {lab(this), index});
    const char* key = index == 0 ? "gep0" : index == 1 ? "gep1" : index == 2 ? "gep2" : "gep3";
    return reinterpret_cast<void*>(static_cast<intptr_t>(static_cast<int32_t>(ret(key))));
}

int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("set", {lab(this), index, W(value)});
    return 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("getwin", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(0x7000));
}

void InterfaceWindow::SetFrame(int32_t frame) { ev("setframe", {lab(this), frame}); }

uint32_t IveGotTicks() {
    ev("ticks", {});
    return static_cast<uint32_t>(ret("ticks"));
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("default", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("default"));
}

extern "C" {

uint32_t tpw_apm_call(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(AnimatingPumpMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}

}  // extern "C"
