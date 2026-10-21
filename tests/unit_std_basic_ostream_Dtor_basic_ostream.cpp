// Test library for std::basic_ostream<char> ctor and dtor (unit v5116; libtpw_unit_std_basic_ostream_Dtor_basic_ostream.dylib,
// tests/test_std_basic_ostream_Dtor_basic_ostream.py), main app:
//   src/game/MslSupport.cpp   BasicOstreamCtor, BasicOstreamDtor and the three vtable data blocks
//
// The callees (ios_base::~ios_base, ios_base::init, basic_ios::widen, operator delete) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"

using namespace calllog;

void OstreamIosBaseDtor(MacOstreamIos* ios, int32_t flags) { ev("IosBaseDtor", {lab(ios), flags}); }
void OstreamIosInit(MacOstreamIos* ios, uint32_t streambuf) { ev("IosInit", {lab(ios), static_cast<int64_t>(streambuf)}); }
char OstreamWiden(MacOstreamIos* ios, char c) {
    ev("Widen", {lab(ios), static_cast<int64_t>(c)});
    return static_cast<char>(ret("Widen"));
}
void OstreamDelete(void* object) { ev("Delete", {lab(object)}); }

extern "C" {

uint64_t tpw_os_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"g1", gOstreamVt1},
        {"g2", gOstreamVt2},
        {"vt", gIosBaseVt},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_os_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_os_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_os_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MacOstream, mIos));
    out[n++] = static_cast<int64_t>(offsetof(MacOstream, mVtbl));
    out[n++] = static_cast<int64_t>(offsetof(MacOstream, mIosPart));
    out[n++] = static_cast<int64_t>(offsetof(MacOstreamIos, mF3c));
    out[n++] = static_cast<int64_t>(offsetof(MacOstreamIos, mFill));
    out[n++] = static_cast<int64_t>(offsetof(MacOstreamIos, mVbaseOffset));
    out[n++] = static_cast<int64_t>(sizeof(MacOstreamIos));
    out[n++] = static_cast<int64_t>(sizeof(MacOstream));
    return n;
}

uint64_t tpw_os_ctor(uint64_t self, int16_t flag, uint32_t streambuf) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(BasicOstreamCtor(reinterpret_cast<MacOstream*>(static_cast<uintptr_t>(self)), flag, streambuf)));
}

uint64_t tpw_os_dtor(uint64_t self, int16_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(BasicOstreamDtor(reinterpret_cast<MacOstream*>(static_cast<uintptr_t>(self)), flag)));
}

}  // extern "C"
