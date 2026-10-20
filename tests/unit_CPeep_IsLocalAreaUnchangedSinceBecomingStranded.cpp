// Test library for the CPeep stranded-area check, particle effect and event report (unit v5048;
// libtpw_unit_CPeep_IsLocalAreaUnchangedSinceBecomingStranded.dylib, tests/test_CPeep_IsLocalAreaUnchangedSinceBecomingStranded.py), main app:
//   src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded.cpp   CPeep::IsLocalAreaUnchangedSinceBecomingStranded, SpawnParticleEffect, ReportAllEvents
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues or from a pure function of
// their arguments; the globals of other files the unit reads are defined here (zero at start like the Mac's bss).
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint16_t gStrandedQueueEnd;
uint16_t gNoThoughtThing;
CNeighbourStep gNeighbourSteps[9];
CMap* gWorldMap;
CWorld* gWorld;
CThingArrayEntry gThingArray[kThingArraySize];
static uintptr_t gEventBase;   // the block of events GetRecentEvent answers from
static int32_t gStampSeed;     // GetTimeStamp's stamp is a function of the cell and this

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Hash(const char* s) {
    uint32_t h = 0;
    for (; *s; ++s) h = h * 31u + static_cast<uint8_t>(*s);
    return h & 0x7fffffffu;
}

float FloatOf(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

int64_t Offset(const void* p, const void* base) { return static_cast<int64_t>(static_cast<const char*>(p) - static_cast<const char*>(base)); }

}  // namespace

// ---- the fakes
int32_t CMapCell::IsQueue() const {
    ev("IsQueue", {Offset(this, gWorldMap) / 0x44});
    return static_cast<int32_t>(ret("IsQueue"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CObject::GetQCellBehind(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("GetQCellBehind", {static_cast<int64_t>(cell.mIndex)});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> out;
    out.mIndex = static_cast<uint16_t>(cell.mIndex - 1);
    return out;
}

uint32_t CMap::GetTimeStamp(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("GetTimeStamp", {Offset(this, gWorld), static_cast<int64_t>(cell.mIndex)});
    return static_cast<uint32_t>((gStampSeed + static_cast<int32_t>(cell.mIndex) * 7) % 50);
}

int32_t CWorld::GetHeightfieldXStart() const {
    ev("HeightfieldXStart", {Offset(this, gWorld)});
    return static_cast<int32_t>(ret("HX"));
}

int32_t CWorld::GetHeightfieldYStart() const {
    ev("HeightfieldYStart", {Offset(this, gWorld)});
    return static_cast<int32_t>(ret("HY"));
}

float DetermineHFieldHeightQuick(float x, float z) {
    ev("DetermineHFieldHeightQuick", {CanonBits(x), CanonBits(z)});
    return FloatOf(ret("HEIGHT"));
}

void SCR_GetScriptPosition(int32_t handle, float* x, float* y, float* z, int32_t* mode) {
    ev("SCR_GetScriptPosition", {static_cast<int64_t>(handle), static_cast<int64_t>(reinterpret_cast<uintptr_t>(mode))});
    *x = FloatOf(ret("SPx"));
    *y = FloatOf(ret("SPy"));
    *z = FloatOf(ret("SPz"));
}

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {static_cast<int64_t>(type), static_cast<int64_t>(a), static_cast<int64_t>(b), static_cast<int64_t>(c)});
    return 0;
}

void CTRACE(uint32_t level, const char* format, ...) {
    int32_t n = 0;
    for (const char* p = format; *p; ++p)
        if (*p == '%') {
            if (p[1] == '%') ++p; else ++n;
        }
    va_list ap;
    va_start(ap, format);
    int64_t v[3] = {0, 0, 0};
    for (int32_t i = 0; i < n && i < 3; ++i) v[i] = va_arg(ap, int32_t);
    va_end(ap);
    int64_t h = Hash(format);
    int64_t l = level;
    switch (n) {
    case 0: ev("CTRACE", {l, h, 0}); break;
    case 1: ev("CTRACE", {l, h, 1, v[0]}); break;
    case 2: ev("CTRACE", {l, h, 2, v[0], v[1]}); break;
    default: ev("CTRACE", {l, h, 3, v[0], v[1], v[2]}); break;
    }
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {static_cast<int64_t>(condition != 0), Hash(format)});
}

CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {Offset(this, nullptr) != 0});
    return *gWorld;
}

const CThoughtEvent* CThoughts::GetRecentEvent(uint32_t index) const {
    ev("GetRecentEvent", {Offset(this, nullptr) != 0, static_cast<int64_t>(index)});
    return reinterpret_cast<const CThoughtEvent*>(gEventBase) + index;
}

extern "C" {

uint64_t tpw_v5048_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorldMap", &gWorldMap},       {"gWorld", &gWorld},     {"steps", gNeighbourSteps}, {"qend", &gStrandedQueueEnd},
        {"none", &gNoThoughtThing},      {"evbase", &gEventBase}, {"seed", &gStampSeed},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v5048_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v5048_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CNeighbourStep));
    out[n++] = static_cast<int64_t>(offsetof(CNeighbourStep, mDy));
    out[n++] = static_cast<int64_t>(sizeof(CThoughtEvent));
    return n;
}

void tpw_v5048_setthing(uint32_t index, uint64_t thing) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(thing)); }

uint32_t tpw_v5048_islocal(uint64_t peep) {
    return reinterpret_cast<CPeep*>(static_cast<uintptr_t>(peep))->IsLocalAreaUnchangedSinceBecomingStranded() ? 1u : 0u;
}

void tpw_v5048_spawn(uint64_t peep, int32_t effect) { reinterpret_cast<CPeep*>(static_cast<uintptr_t>(peep))->SpawnParticleEffect(effect); }

void tpw_v5048_report(uint64_t peep) { reinterpret_cast<const CPeep*>(static_cast<uintptr_t>(peep))->ReportAllEvents(); }

}  // extern "C"
