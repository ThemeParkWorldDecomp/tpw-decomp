// Test library for std::basic_ofstream<char> ctor and dtor (unit v5527; libtpw_unit_std_basic_ofstream_Dtor_basic_ofstream_2.dylib), main app:
//   src/RideInstance_std_basic_ofstream_Dtor_basic_ofstream_2.cpp
// The callees (ostream ctor, filebuf ctor and close, locale dtor, ios_base dtor, operator delete) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"

using namespace calllog;

char gOstreamVt1[0x20];
char gOstreamVt2[0x20];
char gIosBaseVt[0x20];
uint32_t gFilebufCharVtblA[4];
uint32_t gFilebufCharVtblB[4];
uint8_t gIfsWFiles[0xfc];

static int64_t Off(const void* p) { return lab(p); }

MacOstream* BasicOstreamCtor(MacOstream* self, int16_t flag, uint32_t streambuf) {
    ev("OstreamCtor", {lab(self), flag, lab(tpw::gArenaBase + streambuf)});
    *reinterpret_cast<uint32_t*>(self) = tpw::ArenaOffset(self) + 0x48;
    return self;
}

BasicFilebufChar* BasicFilebufChar::construct(void* file) {
    ev("FilebufCtor", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(file))});
    return this;
}

void OfstreamClose(void* fb) { ev("Close", {lab(fb)}); }
void OfstreamLocaleDtor(void* loc, int32_t flag) { ev("LocaleDtor", {lab(loc), flag}); }
void OstreamIosBaseDtor(MacOstreamIos* ios, int32_t flags) { ev("IosBaseDtor", {lab(ios), flags}); }
void OstreamDelete(void* object) { ev("Delete", {lab(object)}); }

extern "C" {

uint64_t tpw_ofs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ofsvt", gOfstreamVt}, {"vt1", gOstreamVt1}, {"vt2", gOstreamVt2}, {"iosvt", gIosBaseVt},
        {"arena", tpw::gArenaBase}, {"fbA", gFilebufCharVtblA}, {"fbB", gFilebufCharVtblB}, {"files", gIfsWFiles},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ofs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ofs_layout(int64_t* out) {
    out[0] = static_cast<int64_t>(sizeof(BasicFilebufChar));
    return 1;
}

uint64_t tpw_ofs_ctor(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(BasicOfstreamCtor(reinterpret_cast<void*>(self), static_cast<int16_t>(flag))));
}
uint64_t tpw_ofs_dtor(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(BasicOfstreamDtor(reinterpret_cast<void*>(self), static_cast<int16_t>(flag))));
}

}  // extern "C"
