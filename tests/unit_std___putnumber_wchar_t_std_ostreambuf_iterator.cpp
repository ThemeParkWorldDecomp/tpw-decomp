// Test library for std::__putnumber<wchar_t>, basic_ios<wchar_t>::widen and deque<vector<Argument>>::const_iterator::operator++
// (unit v5996; libtpw_unit_std___putnumber_wchar_t_std_ostreambuf_iterator.dylib), main app:
//   src/game/MslSupport.cpp
//
// The callees outside the unit are recording fakes: the streambuf overflow and the ctype widen (both __ptr_glue on the Mac), getloc,
// use_facet and the locale destructor.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Ptr32.h"
#include "online.h"
#include "std_basic_istream.h"

using namespace calllog;

uint64_t gMslMacAddressHigh = 0;

int32_t MslPutOverflow(MacPutStreamBuf*, uint32_t c) {
    ev("overflow", {static_cast<int64_t>(c)});
    return static_cast<int32_t>(ret("overflow"));
}

namespace ignore_callee {
void IosGetloc(IgnoreLocale* out, IgnoreIos* ios) {
    uint32_t first;
    std::memcpy(&first, ios, 4);
    ev("getloc", {static_cast<int64_t>(first)});
    out->mWords[0] = static_cast<uint32_t>(ret("loc0"));
    out->mWords[1] = static_cast<uint32_t>(ret("loc1"));
}
void* UseFacetCtype(IgnoreLocale* locale) {
    ev("use_facet", {static_cast<int64_t>(locale->mWords[0]), static_cast<int64_t>(locale->mWords[1])});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x12345678));
}
char16_t CtypeWiden(void* facet, char c) {
    ev("widen", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(facet) == 0x12345678 ? 1 : 0), static_cast<int64_t>(c)});
    return static_cast<char16_t>(ret("widen"));
}
void LocaleDtor(IgnoreLocale* locale, int16_t flag) {
    ev("locale_dtor", {static_cast<int64_t>(locale->mWords[0]), static_cast<int64_t>(locale->mWords[1]), static_cast<int64_t>(flag)});
}
}  // namespace ignore_callee

extern "C" {

uint64_t tpw_wp_alloc(uint32_t size) {
    uint64_t a = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size)));
    gMslMacAddressHigh = a >> 32;
    return a;
}

uint32_t tpw_wp_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_wp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MacPutStreamBuf, mPptr));
    out[n++] = static_cast<int64_t>(offsetof(MacPutStreamBuf, mEpptr));
    out[n++] = static_cast<int64_t>(offsetof(MacPutIos, mWidth));
    out[n++] = static_cast<int64_t>(offsetof(MacPutIos, mFlags));
    out[n++] = static_cast<int64_t>(sizeof(MslArgumentDequeIter));
    return n;
}

uint64_t tpw_wp_put(uint64_t iter, uint64_t ios, uint32_t fill, uint64_t prefix, int32_t plen, uint64_t suffix, int32_t slen) {
    void* r = wostream_callee::PutNumber(reinterpret_cast<void*>(static_cast<uintptr_t>(iter)), reinterpret_cast<void*>(static_cast<uintptr_t>(ios)),
                                         static_cast<char16_t>(fill), reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(prefix)), plen,
                                         reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(suffix)), slen);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(r));
}

uint32_t tpw_wp_widen(uint64_t ios, int32_t c) {
    return wistream_callee::Widen(reinterpret_cast<void*>(static_cast<uintptr_t>(ios)), static_cast<char>(c));
}

void tpw_wp_inc(uint64_t it) { argdeque_callee::IterInc(reinterpret_cast<MslArgumentDequeIter*>(static_cast<uintptr_t>(it))); }

}  // extern "C"
