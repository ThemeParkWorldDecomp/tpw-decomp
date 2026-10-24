// Test library for CPeep::GetInterpolatedPeepPosition and CPeep::Serialise (unit v5533; libtpw_unit_CPeep_GetInterpolatedPeepPosition.dylib,
// tests/test_CPeep_GetInterpolatedPeepPosition.py), main app:
//   src/game/people/Peep.cpp
//
// Every callee is a fake that records the arguments the real one reads (a pointer into the peep as its offset from the peep) and answers from
// the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "Serialise.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

namespace {

uintptr_t gBase = 0;   // the peep under test

int64_t Off(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - gBase); }

template<class V> int32_t Var(const char* kind, void* file, V* value, const char* name, int32_t noLog) {
    std::string event = std::string(kind) + ":" + name;
    ev(event.c_str(), {lab(file), Off(value), noLog});
    return static_cast<int32_t>(ret("R"));
}

int32_t Sub(const char* kind, const void* self, void* file, int32_t save, int32_t version, uint32_t unused) {
    ev(kind, {Off(self), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("R"));
}

}  // namespace

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

template<> int32_t SAVEVAR<int16_t>(void* f, int16_t* v, const char* n, int32_t l) { return Var("SAVEVAR<s>", f, v, n, l); }
template<> int32_t LOADVAR<int16_t>(void* f, int16_t* v, const char* n, int32_t l) { return Var("LOADVAR<s>", f, v, n, l); }
template<> int32_t SAVEVAR<uint16_t>(void* f, uint16_t* v, const char* n, int32_t l) { return Var("SAVEVAR<Us>", f, v, n, l); }
template<> int32_t LOADVAR<uint16_t>(void* f, uint16_t* v, const char* n, int32_t l) { return Var("LOADVAR<Us>", f, v, n, l); }
template<> int32_t SAVEVAR<uint8_t>(void* f, uint8_t* v, const char* n, int32_t l) { return Var("SAVEVAR<Uc>", f, v, n, l); }
template<> int32_t LOADVAR<uint8_t>(void* f, uint8_t* v, const char* n, int32_t l) { return Var("LOADVAR<Uc>", f, v, n, l); }
template<> int32_t SAVEVAR<ESP_Sprites>(void* f, ESP_Sprites* v, const char* n, int32_t l) { return Var("SAVEVAR<ESP>", f, v, n, l); }
template<> int32_t LOADVAR<ESP_Sprites>(void* f, ESP_Sprites* v, const char* n, int32_t l) { return Var("LOADVAR<ESP>", f, v, n, l); }
template<> int32_t SAVEVARSTRUCT<MapId>(void* f, MapId* v, const char* n, int32_t l) { return Var("SAVEVARSTRUCT<map>", f, v, n, l); }
template<> int32_t LOADVARSTRUCT<MapId>(void* f, MapId* v, const char* n, int32_t l) { return Var("LOADVARSTRUCT<map>", f, v, n, l); }
template<> int32_t SAVEVAR<float>(void* f, float* v, const char* n, int32_t l) { return Var("SAVEVAR<f>", f, v, n, l); }
template<> int32_t LOADVAR<float>(void* f, float* v, const char* n, int32_t l) { return Var("LOADVAR<f>", f, v, n, l); }
template<> int32_t SAVEVAR<MacLong>(void* f, MacLong* v, const char* n, int32_t l) { return Var("SAVEVAR<l>", f, v, n, l); }
template<> int32_t LOADVAR<MacLong>(void* f, MacLong* v, const char* n, int32_t l) { return Var("LOADVAR<l>", f, v, n, l); }
template<> int32_t SAVEVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) { return Var("SAVEVAR<Ul>", f, v, n, l); }
template<> int32_t LOADVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) { return Var("LOADVAR<Ul>", f, v, n, l); }
template<> int32_t SAVEVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Var("SAVEVAR<i>", f, v, n, l); }
template<> int32_t LOADVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Var("LOADVAR<i>", f, v, n, l); }
template<> int32_t SAVEVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l) { return Var("SAVEVAR<Ui>", f, v, n, l); }
template<> int32_t LOADVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l) { return Var("LOADVAR<Ui>", f, v, n, l); }

int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return Sub("CThing::Serialise", this, file, save, version, unused); }
int32_t CDelayedScript::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    return Sub("CDelayedScript::Serialise", this, file, save, version, unused);
}
int32_t Vehicle::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return Sub("Vehicle::Serialise", this, file, save, version, unused); }
int32_t CThoughts::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    return Sub("CThoughts::Serialise", this, file, save, version, unused);
}

int32_t ESP_GetNumberOfSprites(ESP_Sprites set) {
    ev("ESP_GetNumberOfSprites", {static_cast<int64_t>(static_cast<int32_t>(set))});
    return static_cast<int32_t>(ret("N"));
}

int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("SetDest", {Off(this), x.mCell, x.mOffset, y.mCell, y.mOffset});
    return static_cast<int32_t>(ret("D"));
}

void Pilot::get_pos(int32_t& x, int32_t& y) {
    ev("Pilot::get_pos", {Off(this)});
    x = static_cast<int32_t>(ret("px"));
    y = static_cast<int32_t>(ret("py"));
}

void CTRACE(uint32_t level, const char* format, ...) {
    std::string event = std::string("CTRACE:") + format;
    ev(event.c_str(), {level});
}

extern "C" {

uint64_t tpw_peep_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_peep_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MapPos));
    out[n++] = static_cast<int64_t>(sizeof(MapId));
    return n;
}

int32_t tpw_peep_serialise(uint64_t peep, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    gBase = static_cast<uintptr_t>(peep);
    return reinterpret_cast<CPeep*>(gBase)->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}

// out: x, y, z as float bits, then the stamp
void tpw_peep_interp(uint64_t peep, float t, int32_t ranged, uint64_t out) {
    gBase = static_cast<uintptr_t>(peep);
    float* o = reinterpret_cast<float*>(static_cast<uintptr_t>(out));
    reinterpret_cast<CPeep*>(gBase)->GetInterpolatedPeepPosition(t, ranged, o[0], o[1], o[2], *reinterpret_cast<uint32_t*>(o + 3));
}

int32_t tpw_peep_walkfix() {
    return 0;
}

}  // extern "C"
