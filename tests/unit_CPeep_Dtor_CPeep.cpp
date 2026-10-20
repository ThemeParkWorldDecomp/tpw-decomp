// Test library for the CPeep destructor, HideMySprite, DrawMySpriteAt and FinalDestroy (unit v5298; libtpw_unit_CPeep_Dtor_CPeep.dylib,
// tests/test_CPeep_Dtor_CPeep.py), main app:
//   src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound_CPeep_Dtor_CPeep.cpp
//
// The callees outside the unit are fakes that record the same events as the module's PowerPC stubs (pointers as the offset from the peep block).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "World.h"
#include "TPCamera.h"
#include "analyser.h"

using namespace calllog;

namespace {

const uint8_t* gBase = nullptr;

int64_t Rel(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(p) - gBase)); }

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

Vehicle::~Vehicle() { ev("VehDtor", {Rel(this)}); }

void CDelayedScript::KillSpriteScript() { ev("Kill", {Rel(this)}); }
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("Spawn", {Rel(this), static_cast<int32_t>(script), set, index});
}
bool CDelayedScript::IsRunningThisScript(ScriptIdx script) {
    ev("IsRun", {Rel(this), static_cast<int32_t>(script)});
    return ret("IsRun") != 0;
}
void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("SetScr", {Rel(this), static_cast<int32_t>(script)}); }

void CThoughts::RemoveThoughtBubble() { ev("RemoveBubble", {Rel(this)}); }
void CThoughts::FinalDestroy() { ev("ThoughtsFinal", {Rel(this)}); }
void CPerson::MakeBalloonPop() { ev("Balloon", {Rel(this)}); }
void CThing::FinalDestroy() { ev("ThingFinal", {Rel(this)}); }
void CPeep::ReportAllEvents() const { ev("Report", {Rel(this)}); }

float DetermineHFieldHeight(float x, float z, int32_t flag) {
    ev("Height", {CanonBits(x), CanonBits(z), flag});
    uint32_t bits = static_cast<uint32_t>(ret("Height"));
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

void SCR_SetScriptPosition(int32_t handle, float x, float y, float z, int32_t mode) {
    ev("SetPos", {handle, CanonBits(x), CanonBits(y), CanonBits(z), mode});
}

extern "C" {

uint64_t tpw_cpeep_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cpeep_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CDelayedScript));
    return n;
}

static CPeep* Peep(uint64_t p) {
    CPeep* peep = reinterpret_cast<CPeep*>(static_cast<uintptr_t>(p));
    gBase = reinterpret_cast<const uint8_t*>(peep);
    return peep;
}

void tpw_cpeep_dtor(uint64_t p) { Peep(p)->~CPeep(); }
void tpw_cpeep_hide(uint64_t p) { Peep(p)->HideMySprite(); }
void tpw_cpeep_final(uint64_t p) { Peep(p)->FinalDestroy(); }
void tpw_cpeep_draw(uint64_t p, float x, float y, float z, int32_t frame, int32_t script) {
    Peep(p)->DrawMySpriteAt(x, y, z, frame, static_cast<CDelayedScript::ScriptIdx>(script));
}

}  // extern "C"
