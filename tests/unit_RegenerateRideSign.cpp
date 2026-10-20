// Test library for the ride sign regeneration and the object instance table (unit v5934; libtpw_unit_RegenerateRideSign.dylib,
// tests/test_RegenerateRideSign.py), main app:
//   src/RideInstance_RegenerateRideSign.cpp   RegenerateRideSign (0x101eeb9c), RegenerateRideSigns, SetRideScriptID, GetAnimStats,
//                                             AllocateObjectInstanceToTable, RemoveObjectInstanceFromTable, gRideSignListEnd
//
// The callees outside the unit are recording fakes (GetDBInfo, GetSignNameA/B, GetLocalPath, the string template's constructors and
// destructors, UniStringToString, GenerateRideSign, SetCoasterScriptID, GAME_ASSERTMSG). Texts are logged as FNV hashes so one event
// carries as many as it needs. The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "FS_System.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "TPCoaster.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_main.h"

using namespace calllog;

namespace {
uint32_t HashWide(const char16_t* text);
}

// the string template's constructors and destructors (imports on the Mac): defined before the globals that construct strings
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    mData = const_cast<char16_t*>(text);
    ev("WCtor", {HashWide(text)});
}
template <>
TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {
    ev("WDtor", {0});
}
// the other virtuals: in the vtable only, never called
#define TB_VTABLE_FAKES(C)                                                                                     \
    template <> void TbDynamicStringTemplate<C>::Format(const C*, ...) {}                                      \
    template <> void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}                                         \
    template <> void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}                                      \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) { return *this; }   \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate&) { return *this; } \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C*) { return *this; }  \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) { return *this; }  \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>&) { return *this; }   \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) { return *this; } \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) { return *this; }
TB_VTABLE_FAKES(char16_t)
TB_VTABLE_FAKES(char)
template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate() {
    ev("CCtor", {0});
}
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {
    ev("CDtor", {0});
}

// ---- globals of other files
CSYS_TPFileInfo gTPFileInfo;
tag_sRideAnimSet* gRideAnimSets[16];
CPlayerSettings gPlayerSettings;
CThingArrayEntry gThingArray[kThingArraySize];
alignas(16) static char gWorldBuf[sizeof(CWorld)];
CWorld* gGameWorld = reinterpret_cast<CWorld*>(gWorldBuf);
alignas(16) static char gPathObject[64];
static uint64_t gInfoP = 0;
static uint64_t gNameP = 0;

void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}

namespace {

uint32_t HashBytes(const char* text) {
    if (text == nullptr) return 0xffffffffu;
    uint32_t h = 0x811c9dc5u;
    for (; *text != 0; ++text) h = (h ^ static_cast<uint8_t>(*text)) * 0x01000193u;
    return h;
}

uint32_t HashWide(const char16_t* text) {
    if (text == nullptr) return 0xffffffffu;
    uint32_t h = 0x811c9dc5u;
    for (; *text != 0; ++text) h = (h ^ static_cast<uint32_t>(*text)) * 0x01000193u;
    return h;
}

void FillSign(char16_t* buffer, int64_t k) {
    buffer[0] = static_cast<char16_t>(0x41 + (k & 15));
    buffer[1] = static_cast<char16_t>(0x61 + (k & 15));
    buffer[2] = 0;
}

}  // namespace

// ---- the fakes
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {0});
    return reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(gInfoP));
}

void CObject::GetSignNameA(char16_t* buffer, uint32_t size) {
    ev("SignA", {static_cast<int64_t>(*reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(this) + 0x1c)), size});
    FillSign(buffer, ret("SignA"));
}

void CObject::GetSignNameB(char16_t* buffer, uint32_t size) {
    ev("SignB", {static_cast<int64_t>(*reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(this) + 0x1c)), size});
    FillSign(buffer, ret("SignB"));
}

const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetLocalPath() const {
    ev("GetLocalPath", {0});
    return *reinterpret_cast<const TbDynamicStringTemplate<char>*>(gPathObject);
}


int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    ev("Uni", {HashWide(in.mData)});
    out.mData = reinterpret_cast<char*>(static_cast<uintptr_t>(gNameP));
    return static_cast<int32_t>(ret("Uni"));
}

int32_t GenerateRideSign(tag_sRideAnimSet* set, char* folder, char* name, char* fontPath, const char16_t* sign1, const char16_t* sign2) {
    ev("GenerateRideSign", {lab(set), HashBytes(folder), HashBytes(name), HashBytes(fontPath), HashWide(sign1), HashWide(sign2)});
    return static_cast<int32_t>(ret("GenerateRideSign"));
}

void SetCoasterScriptID(uint32_t entrance, int32_t id) { ev("SetCoaster", {entrance, id}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("Assert", {condition}, {format}); }

extern "C" {

uint64_t tpw_rs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"rideanimsets", gRideAnimSets},
        {"world", gWorldBuf},
        {"things", gThingArray},
        {"listend", &gRideSignListEnd},
        {"settings", &gPlayerSettings},
        {"pathobj", gPathObject},
        {"info", &gInfoP},
        {"namep", &gNameP},
        {"worldptr", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mFolder));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mItemName));
    out[n++] = static_cast<int64_t>(sizeof(RideInfo));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mAnimations));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mGroups));
    out[n++] = static_cast<int64_t>(sizeof(sRideAnimGroup));
    out[n++] = static_cast<int64_t>(offsetof(sRideAnimGroup, mCount));
    out[n++] = static_cast<int64_t>(offsetof(sRideAnimGroup, mEntries));
    out[n++] = static_cast<int64_t>(sizeof(sRideAnimEntry));
    out[n++] = static_cast<int64_t>(offsetof(sRideAnimEntry, mAnimation));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFD4));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA746));
    out[n++] = static_cast<int64_t>(sizeof(CWorld));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings, mCurrent));
    out[n++] = static_cast<int64_t>(offsetof(TbDynamicStringTemplate<char>, mData));
    return n;
}

int32_t tpw_rs_RegenerateRideSign(uint64_t object, uint64_t a, uint64_t b) {
    return RegenerateRideSignResult(reinterpret_cast<void*>(static_cast<uintptr_t>(object)),
                              reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(a)),
                              reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(b)));
}

void tpw_rs_RegenerateRideSigns() { RegenerateRideSigns(); }

void tpw_rs_SetRideScriptID(uint32_t instance, int32_t script) { SetRideScriptID(instance, static_cast<uint32_t>(script)); }

uint64_t tpw_rs_GetAnimStats(uint64_t set, uint32_t index, uint64_t kind, uint64_t place, uint64_t other) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(GetAnimStatsName(
        reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set)), index, reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kind)),
        reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(place)), reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(other)))));
}

uint32_t tpw_rs_Allocate(uint64_t counters, uint32_t flags, uint64_t table, uint64_t object, uint32_t extra) {
    uint32_t* c = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(counters));
    return AllocateObjectInstanceToTable(c[0], c[1], c[2], flags, reinterpret_cast<void**>(static_cast<uintptr_t>(table)),
                                         reinterpret_cast<void*>(static_cast<uintptr_t>(object)), extra);
}

void tpw_rs_Remove(uint64_t counters, uint64_t table, uint32_t id) {
    uint32_t* c = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(counters));
    RemoveObjectInstanceFromTable(c[0], c[1], c[2], reinterpret_cast<void**>(static_cast<uintptr_t>(table)), id);
}

}  // extern "C"
