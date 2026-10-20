// Test library for COnlineControl's Shutdown, WantToShutDown, Update, CallbackFn and first-person moves (unit v5103;
// libtpw_unit_COnlineControl_WantToShutDown.dylib, tests/test_COnlineControl_WantToShutDown.py), main app:
//   src/COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown.cpp
//
// The callees are fakes that record the arguments the real ones read and answer from the test's queue.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "UI_pump.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Pos(MapPos p) { return (p.mCell << 8) | p.mOffset; }

}  // namespace

void bthread_mutex::lock() { ev("lock", {lab(this)}); }
void bthread_mutex::unlock() { ev("unlock", {lab(this)}); }

void SamsUtilities::UDebug(const char* text, ...) { ev("UDebug", {}, {text}); }

void COnlineControl::ProcessQueue() { ev("ProcessQueue", {lab(this)}); }

void COnlineControl::Teleport(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y) {
    ev("Teleport", {lab(this), thing.mIndex, Pos(x), Pos(y)});
}

void CPeep::HideMySprite() { ev("HideMySprite", {lab(this)}); }

void CPeep::DrawMySpriteAt(float x, float y, float z, int32_t frame, CDelayedScript::ScriptIdx script) {
    ev("DrawMySpriteAt", {lab(this), CanonBits(x), CanonBits(y), CanonBits(z), frame, static_cast<int32_t>(script)});
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return t;
}

void SCR_SetFloatVariable(int32_t handle, uint32_t index, float value) {
    ev("SCR_SetFloatVariable", {handle, index, CanonBits(value)});
}

namespace online_callee {
void ResetHandles();
namespace {
void* gHandles[64];
uint32_t gHandleCount = 0;
}  // namespace
uint32_t PointerToHandle(const void* p) {
    if (p == nullptr) return 0;
    for (uint32_t i = 0; i < gHandleCount; ++i)
        if (gHandles[i] == p) return i + 1;
    gHandles[gHandleCount] = const_cast<void*>(p);
    return ++gHandleCount;
}
void ResetHandles() { gHandleCount = 0; }
void* HandleToPointer(uint32_t handle) { return handle == 0 || handle > gHandleCount ? nullptr : gHandles[handle - 1]; }
void VoteRelease(void* vote) { ev("VoteRelease", {lab(vote)}); }
void VoteDtor(void* vote, int16_t flag) { ev("VoteDtor", {lab(vote), flag}); }
void DisableVoteButton(bool flag) { ev("DisableVoteButton", {flag ? 1 : 0}); }
void ArgumentDequePushBack(void* deque, const void* vec) { ev("PushBack", {lab(deque), lab(vec)}); }
void ChatMovePlayer(void* chat, int32_t x, int32_t y, int32_t mode, void* context) {
    ev("MovePlayer", {lab(chat), x, y, mode, lab(context)});
}
}  // namespace online_callee

namespace {
MapPos MakePos(uint32_t word) {
    MapPos p;
    p.mCell = static_cast<uint8_t>(word >> 24);
    p.mOffset = static_cast<uint8_t>(word >> 16);
    return p;
}
COnlineControl* Obj(uint64_t p) { return reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(p)); }
}  // namespace

extern "C" {

uint64_t tpw_oc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_oc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MapPos));
    return n;
}

uint32_t tpw_oc_handle(uint64_t p) { return online_callee::PointerToHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }
void tpw_oc_reset() { online_callee::ResetHandles(); }

void tpw_oc_shutdown(uint64_t obj) { Obj(obj)->Shutdown(); }
uint32_t tpw_oc_want(uint64_t obj) { return Obj(obj)->WantToShutDown() ? 1 : 0; }
void tpw_oc_update(uint64_t obj) { Obj(obj)->Update(); }
void tpw_oc_callback(uint64_t obj, int32_t id, uint64_t vec) {
    Obj(obj)->CallbackFn(id, reinterpret_cast<void*>(static_cast<uintptr_t>(vec)));
}
void tpw_oc_entering(uint64_t obj, uint32_t a, uint32_t b) { Obj(obj)->EnteringFirstPerson(MakePos(a), MakePos(b)); }
void tpw_oc_leaving(uint64_t obj, uint32_t a, uint32_t b) { Obj(obj)->LeavingFirstPerson(MakePos(a), MakePos(b)); }
void tpw_oc_updatepos(uint64_t obj, uint32_t a, uint32_t b) { Obj(obj)->UpdateFirstPersonPos(MakePos(a), MakePos(b)); }
void tpw_oc_unhide(uint64_t obj, uint32_t a, uint32_t b) { Obj(obj)->UnHideAvatar(MakePos(a), MakePos(b)); }

}  // extern "C"
