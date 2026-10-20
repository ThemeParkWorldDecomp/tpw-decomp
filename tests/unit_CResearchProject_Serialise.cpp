// Test library for CResearchProject::Serialise (libtpw_unit_CResearchProject_Serialise.dylib, tests/test_CResearchProject_Serialise.py), main app:
//   src/ResearchLab_projects_CResearchProject_Serialise.cpp
//
// The eight SAVEVAR / LOADVAR instances it calls are fakes that record the field offset, a hash of the name and the noLog flag, and answer from a queue.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Serialise.h"
#include "ThingParts.h"
#include "ThingReceivers.h"
#include "analyser.h"

using namespace calllog;

namespace {

uintptr_t gBase = 0;

int64_t NameHash(const char* name) {
    int64_t h = 0;
    for (const char* p = name; *p; ++p) h = (h * 31 + static_cast<unsigned char>(*p)) & 0xffffff;
    return h;
}

int32_t Fake(const char* event, const void* value, const char* name, int32_t noLog) {
    ev(event, {static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gBase), NameHash(name), noLog});
    return static_cast<int32_t>(ret("answer"));
}

typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short> ObjIdTag;

}  // namespace

template<> int32_t SAVEVARSTRUCT<ObjIdTag>(void*, ObjIdTag* v, const char* n, int32_t f) { return Fake("SAVEVARSTRUCT", v, n, f); }
template<> int32_t LOADVARSTRUCT<ObjIdTag>(void*, ObjIdTag* v, const char* n, int32_t f) { return Fake("LOADVARSTRUCT", v, n, f); }
template<> int32_t SAVEVAR<ResearchCat>(void*, ResearchCat* v, const char* n, int32_t f) { return Fake("SAVEVARCat", v, n, f); }
template<> int32_t LOADVAR<ResearchCat>(void*, ResearchCat* v, const char* n, int32_t f) { return Fake("LOADVARCat", v, n, f); }
template<> int32_t SAVEVAR<unsigned char>(void*, unsigned char* v, const char* n, int32_t f) { return Fake("SAVEVARByte", v, n, f); }
template<> int32_t LOADVAR<unsigned char>(void*, unsigned char* v, const char* n, int32_t f) { return Fake("LOADVARByte", v, n, f); }
template<> int32_t SAVEVAR<float>(void*, float* v, const char* n, int32_t f) { return Fake("SAVEVARFloat", v, n, f); }
template<> int32_t LOADVAR<float>(void*, float* v, const char* n, int32_t f) { return Fake("LOADVARFloat", v, n, f); }

extern "C" {

uint64_t tpw_crs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_crs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CResearchProject, mCategory));
    out[n++] = static_cast<int64_t>(offsetof(CResearchProject, mF08));
    out[n++] = static_cast<int64_t>(offsetof(CResearchProject, mUpgradeLevel));
    out[n++] = static_cast<int64_t>(offsetof(CResearchProject, mScore));
    out[n++] = static_cast<int64_t>(offsetof(CResearchProject, mProgress));
    return n;
}

int32_t tpw_crs_serialise(uint64_t object, int32_t save, int32_t version, uint32_t unused) {
    gBase = static_cast<uintptr_t>(object);
    return reinterpret_cast<CResearchProject*>(object)->Serialise(nullptr, save, version, unused);
}

}  // extern "C"
