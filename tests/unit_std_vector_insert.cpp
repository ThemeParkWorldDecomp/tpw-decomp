// Test library for the Metrowerks std::vector<const void*> insert / erase / resize and std::__locale_imp<__int128>::~__locale_imp
// (unit v5916; libtpw_unit_std_vector_insert.dylib, tests/test_std_vector_insert.py), main app:
//   src/GetNextWaypoint_HeightAtPos_NAV_std_vector_insert.cpp
//
// The callees outside the unit (operator new, new[], delete, strlen, strcpy, __throw, the facets' virtual destructor) are fakes that
// record what the real ones read. The host elements are 8 bytes (4 on the Mac): a vector allocation is logged at half its size.
// __throw returns in the original, but a length error would go on with a size no test can allocate, so the host fake leaves the
// function by a C++ exception (the PowerPC stub stops the emulator) and the case compares the log up to the throw.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

struct HostAbort {};

uint32_t Fnv(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}

uint32_t TagAt(const void* p, int offset) { return *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(p) + offset); }

}  // namespace

namespace IME {
size_t MslStrlen(const char* s) { return std::strlen(s); }
char* MslStrcpy(char* d, const char* s) { return std::strcpy(d, s); }
void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size == 4 ? 4 : size / 2)});
    return tpw::ArenaAllocZeroed(size);
}
void* MslNewArray(uint32_t size) {
    ev("new[]", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void MslDelete(void* p) { ev("delete", {static_cast<int64_t>(TagAt(p, -8))}); }
void MslDeleteArray(void* p) { ev("delete[]", {static_cast<int64_t>(TagAt(p, -8))}); }
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    const MslExceptionObject* exc = static_cast<const MslExceptionObject*>(object);
    ev("throw", {exc->mVtbl == gMslLengthErrorVtbl ? 1 : 0, exc->mRc.mCount ? *exc->mRc.mCount : -1,
                 dtorTV == gMslLengthErrorDtorTV ? 1 : 0, static_cast<int64_t>(Fnv(typeName)), static_cast<int64_t>(Fnv(exc->mRc.mPtr)),
                 static_cast<int64_t>(std::strlen(exc->mRc.mPtr))});
    throw HostAbort();
}
}  // namespace IME

namespace use_facet_callee {
void FacetDelete(void* facet, int32_t flag) { ev("FacetDelete", {static_cast<int64_t>(TagAt(facet, 8)), flag}); }
}  // namespace use_facet_callee

extern "C" {

uint64_t tpw_vi_addr(const char*) { return 0; }

uint64_t tpw_vi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_vi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MslPtrVector, mSize));
    out[n++] = static_cast<int64_t>(offsetof(MslPtrVector, mData));
    out[n++] = static_cast<int64_t>(sizeof(MslPtrVector));
    out[n++] = static_cast<int64_t>(offsetof(LocaleImp, mChars));
    out[n++] = static_cast<int64_t>(offsetof(LocaleCharArray, mRefs));
    out[n++] = static_cast<int64_t>(offsetof(LocaleCharArray, mData));
    out[n++] = static_cast<int64_t>(offsetof(UseFacetObj, mRefs));
    return n;
}

// Answer 1 when the function left through the fake __throw.
uint32_t tpw_vi_insert(uint64_t vec, uint64_t pos, uint32_t n, uint64_t value) {
    try {
        reinterpret_cast<MslPtrVector*>(vec)->insert(reinterpret_cast<const void**>(pos), n, reinterpret_cast<const void* const*>(value));
    } catch (const HostAbort&) {
        return 1;
    }
    return 0;
}

uint64_t tpw_vi_erase(uint64_t vec, uint64_t first, uint64_t last) {
    return reinterpret_cast<uintptr_t>(
        reinterpret_cast<MslPtrVector*>(vec)->erase(reinterpret_cast<const void**>(first), reinterpret_cast<const void**>(last)));
}

void tpw_vi_resize(uint64_t vec, uint32_t n, uint64_t value) {
    reinterpret_cast<MslPtrVector*>(vec)->resize(n, reinterpret_cast<const void*>(value));
}

uint64_t tpw_vi_dtor(uint64_t self, int16_t flag) {
    return reinterpret_cast<uintptr_t>(LocaleImpDestroy(reinterpret_cast<LocaleImp*>(self), flag));
}

}  // extern "C"
