// Test library for std::__putnumber<char, ostreambuf_iterator<char>> (unit v5068), main app:
//   src/game/MslSupport.cpp   PutNumberChar
//
// The streambuf overflow (via __ptr_glue) is a fake that records whether it got the test's streambuf and the character.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_istream.h"

using namespace calllog;

namespace {
MacPutStreamBuf* gTestSb = nullptr;
}

int32_t MslPutOverflow(MacPutStreamBuf* sb, uint32_t c) {
    ev("overflow", {sb == gTestSb ? 1 : 0, static_cast<int64_t>(c)});
    return static_cast<int32_t>(ret("overflow"));
}

extern "C" {

uint64_t tpw_putnum_addr(const char*) { return 0; }

// The arena base: Ptr32 fields hold offsets from it.
uint64_t tpw_putnum_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

uint64_t tpw_putnum_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_putnum_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MacPutStreamBuf, mPptr));
    out[n++] = static_cast<int64_t>(offsetof(MacPutStreamBuf, mEpptr));
    out[n++] = static_cast<int64_t>(offsetof(MacPutIos, mWidth));
    out[n++] = static_cast<int64_t>(offsetof(MacPutIos, mFlags));
    return n;
}

uint32_t tpw_putnum_run(uint64_t sb, uint64_t ios, int32_t fill, uint64_t prefix, int32_t prefixLen, uint64_t suffix, int32_t suffixLen) {
    gTestSb = reinterpret_cast<MacPutStreamBuf*>(static_cast<uintptr_t>(sb));
    MacPutStreamBuf* r = PutNumberChar(gTestSb, reinterpret_cast<MacPutIos*>(static_cast<uintptr_t>(ios)), static_cast<char>(fill),
                                       reinterpret_cast<const char*>(static_cast<uintptr_t>(prefix)), prefixLen,
                                       reinterpret_cast<const char*>(static_cast<uintptr_t>(suffix)), suffixLen);
    return r == nullptr ? 0 : (r == gTestSb ? 1 : 2);
}

}  // extern "C"
