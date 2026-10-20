// Test library for InterfaceListControl2::Sort and FindString (unit v5203; libtpw_unit_InterfaceListControl2_Sort.dylib,
// tests/test_InterfaceListControl2_Sort.py), main app:
//   src/InterfaceListControl2_items_InterfaceListControl2_Sort.cpp
//
// The callees outside the unit are fakes: the wide string compare, WcsLen, AdvanceIndex (the next link) and GetNonHierIndexedEntry (the
// entry `position` links along the next links), each recording what the PowerPC stubs of the module record.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "online.h"
#include "platform/arena.h"
#include "ui_controls.h"
#include "ui_ime.h"

using namespace calllog;

namespace {

int32_t Order(const char16_t* a, uint32_t n1, const char16_t* b, uint32_t n2) {
    uint32_t n = n1 < n2 ? n1 : n2;
    for (uint32_t i = 0; i < n; ++i)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    if (n1 < n2) return -1;
    if (n1 > n2) return 1;
    return 0;
}

int64_t Sum(const char16_t* a, uint32_t n) {
    int64_t sum = 0;
    for (uint32_t i = 0; i < n; ++i) sum += a[i];
    return sum;
}

}  // namespace

namespace IME {
int32_t MslWideString_compare(const MslWideString* self, uint32_t pos, uint32_t n1, const char16_t* s, uint32_t n2) {
    const LCStringRep* rep = *reinterpret_cast<LCStringRep* const*>(self);
    const char16_t* a = rep->mData + pos;
    ev("compare", {static_cast<int64_t>(pos), static_cast<int64_t>(n1), static_cast<int64_t>(n2), Sum(a, n1), Sum(s, n2)});
    return Order(a, n1, s, n2);
}
}  // namespace IME

namespace online_parser_callee {
uint32_t WcsLen(const char16_t* text) {
    uint32_t n = 0;
    while (text[n] != 0) ++n;
    ev("wcslen", {static_cast<int64_t>(n)});
    return n;
}
}  // namespace online_parser_callee

int32_t InterfaceListControl2::AdvanceIndex(int32_t index, int32_t* change) {
    ev("AdvanceIndex", {static_cast<int64_t>(index), change != nullptr ? 1 : 0});
    return index < 0 ? -1 : Words()[index].mInt;
}

int32_t InterfaceListControl2::GetNonHierIndexedEntry(int32_t position, int32_t parent) {
    ev("GetNonHier", {static_cast<int64_t>(position), static_cast<int64_t>(parent)});
    int32_t entry = mFirstEntry;
    for (int32_t i = 0; i < position && entry != -1; ++i) entry = Words()[entry].mInt;
    return entry;
}

extern "C" {

uint64_t tpw_icl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host's offsets: storage, count, first, column count, sort column, descending, columns, sizeof object, sizeof CoolRect, mBottom
// in CoolRect, sizeof LCWord, sizeof LCStringRep, mData in LCStringRep.
int32_t tpw_icl_layout(int64_t* out) {
    alignas(16) static char buf[2048];
    auto* o = reinterpret_cast<InterfaceListControl2*>(buf);
    auto off = [&](const void* p) { return static_cast<int64_t>(reinterpret_cast<const char*>(p) - buf); };
    int32_t n = 0;
    out[n++] = off(&o->mItemStorage);
    out[n++] = off(&o->mItemCount);
    out[n++] = off(&o->mFirstEntry);
    out[n++] = off(&o->mColumnCount);
    out[n++] = off(&o->mSortColumn);
    out[n++] = off(&o->mSortDescending);
    out[n++] = off(&o->mColumns);
    out[n++] = static_cast<int64_t>(sizeof(InterfaceListControl2));
    out[n++] = static_cast<int64_t>(sizeof(CoolRect));
    auto* c = reinterpret_cast<CoolRect*>(buf);
    out[n++] = off(&c->mBottom);
    out[n++] = static_cast<int64_t>(sizeof(LCWord));
    out[n++] = static_cast<int64_t>(sizeof(LCStringRep));
    auto* r = reinterpret_cast<LCStringRep*>(buf);
    out[n++] = off(&r->mData);
    return n;
}

void tpw_icl_sort(uint64_t object, int32_t unused) {
    reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(object))->Sort(unused);
}

int32_t tpw_icl_find(uint64_t object, uint64_t text, int32_t column) {
    return reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(object))
        ->FindString(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text)), column);
}

}  // extern "C"
