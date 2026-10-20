// Test library for the std::basic_ofstream<wchar_t> constructor (unit v5755; libtpw_unit_std_basic_ofstream_basic_ofstream.dylib), main app:
//   src/std_basic_ofstream_basic_ofstream.cpp   OfstreamCtor 0x10221f20
//
// The callees (basic_ostream<wchar_t> ctor, basic_filebuf<wchar_t> ctor) are recording fakes. The vtable globals are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "std_basic_ifstream.h"

using namespace calllog;

char gIosBaseVt[16];
uint32_t gOfsWVtbl[4];
uint32_t gIfsWVtblE[4];

namespace ignore_callee {
void WostreamCtor(void* self, int32_t flag, void* streambuf) {
    ev("WostreamCtor", {lab(self), flag, lab(streambuf)});
}
}  // namespace ignore_callee

namespace ifstream_callee {
void FilebufCtor(void* self, void* file) {
    ev("FilebufCtor", {lab(self), lab(file)});
}
}  // namespace ifstream_callee

extern "C" {

uint64_t tpw_ofs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOfsWVtbl", &gOfsWVtbl},
        {"gIfsWVtblE", &gIfsWVtblE},
        {"gIosBaseVt", &gIosBaseVt},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ofs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ofs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreStream, mSlot));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mBaseOffset));
    return n;
}

void tpw_ofs_ctor(uint64_t stream, int32_t flag) {
    ignore_callee::OfstreamCtor(reinterpret_cast<ignore_callee::IgnoreStream*>(static_cast<uintptr_t>(stream)), flag);
}

}  // extern "C"
