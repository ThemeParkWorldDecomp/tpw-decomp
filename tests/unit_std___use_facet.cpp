// Test library for std::__use_facet<F> (unit v5665; libtpw_unit_std___use_facet.dylib, tests/test_std___use_facet.py), main app:
//   src/game/MslSupport.cpp   the four functions UseFacet*; the id globals are defined here.
//
// The callees outside the unit are recording fakes: operator new, the facet constructors, the vector resize/insert/erase and
// the facet delete through __ptr_glue. The vector fakes only change the count (they do not fill the new slots), so a stale
// pointer beyond the count reaches the release path like it would in a vector whose slot was left behind.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"

using namespace calllog;

uint32_t gLocaleIdCount;
uint32_t gIdCtypeChar;
uint32_t gIdCodecvtCharChar;
uint32_t gIdCtypeWchar;
uint32_t gIdCodecvtWcharChar;

namespace use_facet_callee {

void* New(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}

void CtypeCharCtor(void* p, const void* table, bool del, uint32_t n) {
    ev("ctype_char_ctor", {newlab(p), table == nullptr ? 0 : 1, del ? 1 : 0, static_cast<int64_t>(n)});
}

void CodecvtCharCtor(void* p, uint32_t n) { ev("codecvt_char_ctor", {newlab(p), static_cast<int64_t>(n)}); }

void CodecvtWcharCtor(void* p, uint32_t n) { ev("codecvt_wchar_ctor", {newlab(p), static_cast<int64_t>(n)}); }

void VecResize(UseFacetImp* vec, uint32_t n, const void* value) {
    ev("resize", {static_cast<int64_t>(n), value == nullptr ? 0 : 1});
    if (n > vec->mCount) vec->mCount = n;
}

void VecInsert(UseFacetImp* vec, void** pos, uint32_t count, void* const* value) {
    ev("insert", {static_cast<int64_t>(pos - vec->mData), static_cast<int64_t>(count), *value == nullptr ? 0 : 1});
    vec->mCount += count;
}

void VecErase(UseFacetImp* vec, void** first, void** last) {
    ev("erase", {static_cast<int64_t>(first - vec->mData), static_cast<int64_t>(last - vec->mData)});
    vec->mCount = static_cast<uint32_t>(first - vec->mData);
}

void FacetDelete(void* facet, int32_t flag) { ev("facet_delete", {lab(facet), flag}); }

}  // namespace use_facet_callee

extern "C" {

uint64_t tpw_uf_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"count", &gLocaleIdCount},
        {"id_ctype_char", &gIdCtypeChar},
        {"id_codecvt_cc", &gIdCodecvtCharChar},
        {"id_ctype_wchar", &gIdCtypeWchar},
        {"id_codecvt_wc", &gIdCodecvtWcharChar},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_uf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_uf_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(UseFacetObj));
    out[n++] = static_cast<int64_t>(offsetof(UseFacetObj, mRefs));
    out[n++] = static_cast<int64_t>(offsetof(UseFacetImp, mCount));
    out[n++] = static_cast<int64_t>(offsetof(UseFacetImp, mData));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

#define LOC(x) reinterpret_cast<const UseFacetLocale*>(static_cast<uintptr_t>(x))
uint64_t tpw_uf_ctype_char(uint64_t locale) { return reinterpret_cast<uintptr_t>(UseFacetCtypeChar(LOC(locale))); }
uint64_t tpw_uf_codecvt_cc(uint64_t locale) { return reinterpret_cast<uintptr_t>(UseFacetCodecvtCharChar(LOC(locale))); }
uint64_t tpw_uf_ctype_wchar(uint64_t locale) { return reinterpret_cast<uintptr_t>(UseFacetCtypeWchar(LOC(locale))); }
uint64_t tpw_uf_codecvt_wc(uint64_t locale) { return reinterpret_cast<uintptr_t>(UseFacetCodecvtWcharChar(LOC(locale))); }

}  // extern "C"
