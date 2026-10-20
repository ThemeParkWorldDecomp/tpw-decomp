// Test library for std::basic_ifstream<wchar_t>::~basic_ifstream (unit v5201; libtpw_unit_std_basic_ifstream_Dtor_basic_ifstream.dylib), main app:
//   src/std_basic_ifstream_Dtor_basic_ifstream.cpp
//
// Callees (close, locale dtor, ios_base dtor, operator delete) are fakes recording the object as a label and the flag.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"

using namespace calllog;

uint32_t gIfsWVtblA[4];
uint32_t gIfsWVtblB[4];
uint32_t gIfsWVtblC[4];
uint32_t gIfsWVtblD[4];
uint32_t gIfsWVtblE[4];
uint8_t gIfsWFiles[0xfc];

BasicFilebufWchar* BasicFilebufWchar::close() {
    ev("Close", {lab(this)});
    return this;
}

namespace ifstream_callee {
void LocaleDtor(void* locale, int32_t flag) { ev("LocaleDtor", {lab(locale), flag}); }
void IosBaseDtor(void* ios, int32_t flag) { ev("IosBaseDtor", {lab(ios), flag}); }
void Delete(void* p) { ev("Delete", {lab(p)}); }
}  // namespace ifstream_callee

extern "C" {

uint64_t tpw_ifs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"A", gIfsWVtblA}, {"B", gIfsWVtblB}, {"C", gIfsWVtblC}, {"D", gIfsWVtblD}, {"E", gIfsWVtblE}, {"files", gIfsWFiles},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ifs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ifs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(BasicIfstreamWchar));
    out[n++] = static_cast<int64_t>(offsetof(BasicIfstreamWchar, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(BasicIfstreamWchar, mFilebufVt));
    out[n++] = static_cast<int64_t>(offsetof(BasicIfstreamWchar, mLocale));
    out[n++] = static_cast<int64_t>(offsetof(BasicIfstreamWchar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicIfstreamWchar, mIosVtable));
    out[n++] = static_cast<int64_t>(offsetof(BasicIfstreamWchar, mRest));
    out[n++] = static_cast<int64_t>(sizeof(IfstreamWVbTable));
    out[n++] = static_cast<int64_t>(offsetof(IfstreamWVbTable, mBaseOffset));
    return n;
}

uint64_t tpw_ifs_dtor(uint64_t self, int32_t flag) {
    auto* p = reinterpret_cast<BasicIfstreamWchar*>(static_cast<uintptr_t>(self));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p->destroy(static_cast<int16_t>(flag))));
}

}  // extern "C"
