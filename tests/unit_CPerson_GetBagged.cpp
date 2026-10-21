// Test library for the CPerson balloon, happiness and catch helpers (unit v5293; libtpw_unit_CPerson_GetBagged.dylib, tests/test_CPerson_GetBagged.py), main app:
//   src/game/people/Person.cpp
//
// The callees outside the unit are recording fakes; gMainBalance is raw storage under the program's symbol name (its constructor lives in files this
// library does not compile).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"

using namespace calllog;

alignas(16) char gMainBalanceStorage[32768] __asm__("_gMainBalance");

namespace {
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}
}  // namespace

void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }

void ESP_ForceGoto(int32_t handle, void* code) { ev("ESP_ForceGoto", {handle, lab(code)}); }

void SCR_SetScriptPosition(int32_t handle, float x, float y, float z, int32_t mode) {
    ev("SCR_SetScriptPosition", {handle, CanonBits(x), CanonBits(y), CanonBits(z), mode});
}

void CDelayedScript::KillSpriteScript() { ev("KillSpriteScript", {lab(this)}); }

void CPerson::SetState(PersonState state) { ev("SetState", {lab(this), static_cast<int32_t>(state)}); }

void CTRACE(uint32_t level, const char* format, ...) {
    ev("CTRACE", {static_cast<int32_t>(level)}, {format});
    if (std::strstr(format, "%d") != nullptr) {
        va_list args;
        va_start(args, format);
        int32_t value = va_arg(args, int32_t);
        va_end(args);
        ev("CTRACEarg", {value});
    }
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

extern "C" {

uint64_t tpw_gb_addr(const char* name) {
    if (std::strcmp(name, "bal") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gMainBalanceStorage[0]));
    if (std::strcmp(name, "target") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gBalloonPopTarget));
    return 0;
}

uint64_t tpw_gb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gMainBalanceStorage) >= sizeof(CMainBalance));
    return n;
}

void tpw_gb_getbagged(uint64_t p) { reinterpret_cast<CPerson*>(p)->GetBagged(); }
void tpw_gb_lower(uint64_t p, int32_t a) { reinterpret_cast<CPerson*>(p)->LowerHappiness(static_cast<CPerson::Amount>(a)); }
int32_t tpw_gb_catch(uint64_t p) { return reinterpret_cast<CPerson*>(p)->IsCatchable(); }
void tpw_gb_raise(uint64_t p, int32_t a) { reinterpret_cast<CPerson*>(p)->RaiseHappiness(static_cast<CPerson::Amount>(a)); }
uint32_t tpw_gb_amount(uint64_t p, int32_t a) {
    return reinterpret_cast<CPerson*>(p)->GetHappinessAmount(static_cast<CPerson::Amount>(a));
}
void tpw_gb_pop(uint64_t p) { reinterpret_cast<CPerson*>(p)->MakeBalloonPop(); }
void tpw_gb_draw(uint64_t p, float x, float y, float z, int32_t mode) { reinterpret_cast<CPerson*>(p)->DrawBalloon(x, y, z, mode); }
void tpw_gb_losing(uint64_t p) { reinterpret_cast<CPerson*>(p)->LosingMods(nullptr); }

}  // extern "C"
