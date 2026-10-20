// Test library for the theme map's pair copy constructor (unit v5719; libtpw_unit_std_pair_pair.dylib, tests/test_std_pair_pair.py), main app:
//   src/FS_Online_gametype_std_pair_pair.cpp   ParkThemeTreeCallee::PairCopy
//
// The key's copy constructor (0x10238308) is a fake recording this and the source.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_MasterSave.h"

using namespace calllog;

template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const TbDynamicStringTemplate<char>& other) {
    ev("KeyCopy", {lab(this), lab(&other)});
}

// Referenced only through the vtable the fake constructor emits; never called.
using TbStr = TbDynamicStringTemplate<char>;
template <> void TbStr::AllocBuffer(uint32_t) {}
template <> void TbStr::Format(const char*, ...) {}
template <> void TbStr::ResizeTo(uint32_t) {}
template <> TbStr::~TbDynamicStringTemplate() {}
template <> TbStr& TbStr::operator=(const TbStringBase<char>&) { return *this; }
template <> TbStr& TbStr::operator=(const TbStr&) { return *this; }
template <> TbStr& TbStr::operator=(const char*) { return *this; }
template <> TbStr& TbStr::operator=(const char&) { return *this; }
template <> TbStr& TbStr::operator+=(const TbStringBase<char>&) { return *this; }
template <> TbStr& TbStr::operator+=(const char&) { return *this; }
template <> TbStr& TbStr::operator+=(const char*) { return *this; }

extern "C" {

uint64_t tpw_pair_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// offsetof the record pointer, sizeof the pair
int32_t tpw_pair_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ParkThemeTree::Pair, second));
    out[n++] = static_cast<int64_t>(sizeof(ParkThemeTree::Pair));
    return n;
}

void tpw_pair_copy(uint64_t dst, uint64_t src) {
    ParkThemeTreeCallee::PairCopy(reinterpret_cast<ParkThemeTree::Pair*>(static_cast<uintptr_t>(dst)),
                                  *reinterpret_cast<ParkThemeTree::Pair*>(static_cast<uintptr_t>(src)));
}

}  // extern "C"
