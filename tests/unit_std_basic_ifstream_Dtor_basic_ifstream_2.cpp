// Test library for std::basic_ifstream<char>::~basic_ifstream at 0x1020448c (unit v5642; libtpw_unit_std_basic_ifstream_Dtor_basic_ifstream_2.dylib),
// main app: src/game/MslSupport.cpp
//
// The callees (filebuf close, locale dtor, ios_base dtor, operator delete) are recording fakes. The vtable globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"

using namespace calllog;

uint32_t gFilebufCharVtblA[4];
uint32_t gFilebufCharVtblB[4];
char gOstreamVt1[0x28];
char gIfstreamVtbl[0x40];
char gIfstreamVtblC[0x40];
uint8_t gIfsWFiles[0xfc];

void OfstreamClose(void* filebuf) { ev("OfstreamClose", {lab(filebuf)}); }
void OfstreamLocaleDtor(void* locale, int32_t flag) { ev("LocaleDtor", {lab(locale), flag}); }
void OstreamIosBaseDtor(MacOstreamIos* ios, int32_t flags) { ev("IosBaseDtor", {lab(ios), flags}); }
void OstreamDelete(void* object) { ev("delete", {lab(object)}); }

extern "C" {

uint64_t tpw_ifs2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ostream", gOstreamVt1},       {"fbA", gFilebufCharVtblA}, {"fbB", gFilebufCharVtblB},
        {"vtblC", gIfstreamVtblC},      {"vtbl", gIfstreamVtbl},    {"files", gIfsWFiles},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ifs2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_ifs2_base() { tpw::ArenaBase(); return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_ifs2_layout(int64_t*) { return 0; }

uint64_t tpw_ifs2_dtor(uint64_t self, int32_t flags) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(BasicIfstreamDtorEmail(reinterpret_cast<void*>(static_cast<uintptr_t>(self)), static_cast<int16_t>(flags))));
}

}  // extern "C"
