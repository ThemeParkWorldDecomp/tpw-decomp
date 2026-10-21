// Test library for the park-goto callbacks (unit v5276; libtpw_unit_GotoPark_OK.dylib, tests/test_GotoPark_OK.py), main app:
//   src/game/online/online.cpp   GotoPark_OK, GotoCallbackFn
//
// The callees are recording fakes (operator new, the constructor, GotoFailed, GotoStoredPerson, UDebug, GS_SetCurrentGameStatus).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "GameSys.h"
#include "online.h"
#include "platform/samsutils.h"

using namespace calllog;

COnlineControl* gOnlineControl;

namespace online_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
}  // namespace online_callee

COnlineControl::COnlineControl() { ev("COnlineControl", {newlab(this)}); }

void COnlineControl::GotoFailed() { ev("GotoFailed", {lab(this)}); }
void COnlineControl::GotoStoredPerson() { ev("GotoStoredPerson", {lab(this)}); }

void GS_SetCurrentGameStatus(GS_GAME_STATES status) { ev("GS_SetCurrentGameStatus", {static_cast<int64_t>(status)}); }

void SamsUtilities::UDebug(const char* text, ...) { ev("UDebug", {}, {text}); }

extern "C" {

uint64_t tpw_gpo_addr(const char* name) {
    if (std::strcmp(name, "gOnlineControl") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gOnlineControl));
    return 0;
}

uint64_t tpw_gpo_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gpo_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl*));
    return n;
}

uint32_t tpw_gpo_park(uint32_t context) { return static_cast<uint32_t>(GotoPark_OK(reinterpret_cast<void*>(static_cast<uintptr_t>(context)))); }
void tpw_gpo_callback(int32_t status, uint32_t data) { GotoCallbackFn(status, reinterpret_cast<const void*>(static_cast<uintptr_t>(data))); }

}  // extern "C"
