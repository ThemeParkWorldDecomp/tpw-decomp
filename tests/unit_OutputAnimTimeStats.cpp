// Test library for OutputAnimTimeStats and RestoreRideInstance (unit v5933; libtpw_unit_OutputAnimTimeStats.dylib,
// tests/test_OutputAnimTimeStats.py), main app: src/RideTypes_OutputAnimTimeStats.cpp.
// Fakes: CTRACE, CRideDatabase::GetRideInfo, SetCoasterCObject; the ride tables are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "SaveLoad.h"
#include "TPCoaster.h"
#include "Debug.h"

using namespace calllog;

tag_sRideAnimSet* gRideAnimSets[16];
uint32_t gRideCountA;
uint32_t gRideCountB;
tag_sRideAnimSet* gRideTypes[400];
uint32_t gRideTypeCount;
CRideDatabase gRideDatabase;

static uint32_t gInfoBlock[16];

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {static_cast<int64_t>(id.mIndex)});
    return ret("GetRideInfo") ? reinterpret_cast<RideInfo*>(gInfoBlock) : nullptr;
}

void SetCoasterCObject(uint32_t entrance, void* object) {
    ev("SetCoasterCObject", {static_cast<int64_t>(entrance), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(object)))});
}

extern "C" {

uint64_t tpw_ots_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sets", &gRideAnimSets[0]}, {"countA", &gRideCountA}, {"countB", &gRideCountB},
        {"types", &gRideTypes[0]},   {"typeCount", &gRideTypeCount}, {"info", gInfoBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ots_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ots_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mSelf));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mTimeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mTimeControls));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFE4));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFE8));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFEC));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFF0));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFF4));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mFF8));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF10));
    out[n++] = static_cast<int64_t>(offsetof(sFixedRideParams, mF0C));
    return n;
}

uint32_t tpw_ots_out(uint64_t control, uint64_t set) {
    OutputAnimTimeStats(reinterpret_cast<sAnimTimeControl*>(control), reinterpret_cast<tag_sRideAnimSet*>(set));
    return 0;
}

uint32_t tpw_ots_restore(uint32_t instance, uint32_t type, uint32_t flags, uint64_t params, uint32_t a, uint64_t c, int32_t d, int32_t e,
                         int32_t f) {
    return RestoreRideInstance(instance, type, flags, reinterpret_cast<sFixedRideParams*>(params), a, reinterpret_cast<void*>(c), d, e, f);
}

}  // extern "C"
