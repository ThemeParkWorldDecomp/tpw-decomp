// Test library for CLineList::AddLine and ~CLineList (unit v5297; libtpw_unit_CLineList_AddLine.dylib, tests/test_CLineList_AddLine.py), ltms:
//   src/ltms/TbTextFormatter.cpp
//
// The callees (vector<CLineInfo>::push_back and tear_down, operator delete) are fakes that record their arguments; pointers into the
// character block are logged as offsets from its base (set by the test).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbTextFormatter.h"

using namespace calllog;

namespace {
uintptr_t gBase = 0;
int64_t Off(const void* p) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(p) - gBase); }
}  // namespace

template <> void MslVector<CLineInfo>::push_back(const CLineInfo& e) {
    ev("push_back", {lab(this), Off(e.mBegin), Off(e.mEnd), static_cast<int64_t>(e.mWidth)});
}

template <> void MslVector<CLineInfo>::tear_down() { ev("tear_down", {lab(this)}); }


extern "C" {

uint64_t tpw_cll_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_cll_new(uint32_t size) {
    void* p = ::operator new(size);
    std::memset(p, 0, size);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

void tpw_cll_setbase(uint64_t base) { gBase = static_cast<uintptr_t>(base); }

int32_t tpw_cll_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CLineList, mMaxWidth));
    return n;
}

void tpw_cll_addline(uint64_t list, uint64_t a, uint64_t b, uint64_t c, uint32_t width) {
    reinterpret_cast<CLineList*>(list)->AddLine(reinterpret_cast<CCharacterInfo*>(a), reinterpret_cast<CCharacterInfo*>(b),
                                                reinterpret_cast<CCharacterInfo*>(c), width);
}

uint64_t tpw_cll_destruct(uint64_t list, int32_t flag) {
    return reinterpret_cast<uintptr_t>(reinterpret_cast<CLineList*>(list)->Destruct(static_cast<int16_t>(flag)));
}

}  // extern "C"
