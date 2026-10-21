// Test library for _RefCountedPtr<__locale_imp>::operator= (unit v5196; libtpw_unit_std__RefCountedPtr_std___locale_imp_std__Single_.dylib).
//   src/game/MslSupport.cpp
//
// The callees (vector size/operator[], facet virtual delete, CharArray removeRef/destroy/deallocate, vector dtor, operator delete) are
// recording fakes; operator[] answers the slot at host offset 0x40 + 8 * i of the impl (Mac: 0x20 + 4 * i).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"

using namespace calllog;

namespace locale_imp_callee {

uint32_t VecSize(void* vec) {
    ev("size", {lab(vec)});
    return static_cast<uint32_t>(ret("size"));
}
LocaleFacetRef** VecIndex(void* vec, uint32_t i) {
    ev("index", {lab(vec), static_cast<int64_t>(i)});
    return reinterpret_cast<LocaleFacetRef**>(static_cast<uint8_t*>(vec) + 0x40 + 8 * i);
}
void FacetDelete(void* facet, int32_t flag) { ev("facetdel", {lab(facet), flag}); }
void* CharArrayRemoveRef(void* arr) {
    ev("removeRef", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(arr))});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("removeRef")));
}
void AllocDestroy(void* alloc, void* arr) {
    ev("destroy", {lab(static_cast<uint8_t*>(alloc) - offsetof(LocaleImpRef, mCharArray)), static_cast<int64_t>(reinterpret_cast<uintptr_t>(arr))});
}
void AllocDeallocate(void* alloc, void* arr, uint32_t n) {
    ev("dealloc", {lab(static_cast<uint8_t*>(alloc) - offsetof(LocaleImpRef, mCharArray)),
                   static_cast<int64_t>(reinterpret_cast<uintptr_t>(arr)), static_cast<int64_t>(n)});
}
void VecDtor(void* vec, int32_t flag) { ev("vecdtor", {lab(vec), flag}); }
void Delete(void* p) { ev("delete", {lab(p)}); }

}  // namespace locale_imp_callee

extern "C" {

uint64_t tpw_lrp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lrp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(LocaleRefPtr, mCount));
    out[n++] = static_cast<int64_t>(sizeof(LocaleRefPtr));
    out[n++] = static_cast<int64_t>(offsetof(LocaleFacetRef, mRefs));
    out[n++] = static_cast<int64_t>(offsetof(LocaleImpRef, mCharArray));
    return n;
}

uint64_t tpw_lrp_assign(uint64_t self, uint64_t other) {
    LocaleRefPtr* t = reinterpret_cast<LocaleRefPtr*>(static_cast<uintptr_t>(self));
    const LocaleRefPtr* o = reinterpret_cast<const LocaleRefPtr*>(static_cast<uintptr_t>(other));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&(*t = *o)));
}

}  // extern "C"
