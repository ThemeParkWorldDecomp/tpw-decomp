// Test library for CResearchCategory::Serialise: LOADVAR/SAVEVAR instances are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "Serialise.h"
#include "ThingParts.h"

using namespace calllog;

namespace {
uintptr_t gObj = 0;

template <class T> int32_t Save(void* file, T* value, const char* name, int32_t noLog) {
    std::string key = std::string("SAVEVAR:") + name;
    ev(key.c_str(), {lab(file), static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gObj), static_cast<int64_t>(static_cast<uint32_t>(*value)), noLog});
    return static_cast<int32_t>(ret("rc"));
}
template <class T> int32_t Load(void* file, T* value, const char* name, int32_t noLog) {
    std::string key = std::string("LOADVAR:") + name;
    ev(key.c_str(), {lab(file), static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gObj), noLog});
    *value = static_cast<T>(ret("val"));
    return static_cast<int32_t>(ret("rc"));
}
}  // namespace

template <> int32_t SAVEVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t g) { return Save(f, v, n, g); }
template <> int32_t SAVEVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t g) { return Save(f, v, n, g); }
template <> int32_t SAVEVAR<uint8_t>(void* f, uint8_t* v, const char* n, int32_t g) { return Save(f, v, n, g); }
template <> int32_t LOADVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t g) { return Load(f, v, n, g); }
template <> int32_t LOADVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t g) { return Load(f, v, n, g); }
template <> int32_t LOADVAR<uint8_t>(void* f, uint8_t* v, const char* n, int32_t g) { return Load(f, v, n, g); }

extern "C" {

uint64_t tpw_rc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CResearchCategory, mEffort));
    out[n++] = static_cast<int64_t>(offsetof(CResearchCategory, mOutput));
    out[n++] = static_cast<int64_t>(offsetof(CResearchCategory, mF08));
    out[n++] = static_cast<int64_t>(offsetof(CResearchCategory, mF0C));
    return n;
}

int32_t tpw_rc_serialise(uint64_t obj, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    gObj = static_cast<uintptr_t>(obj);
    return reinterpret_cast<CResearchCategory*>(obj)->Serialise(reinterpret_cast<void*>(file), save, version, unused);
}

}  // extern "C"
