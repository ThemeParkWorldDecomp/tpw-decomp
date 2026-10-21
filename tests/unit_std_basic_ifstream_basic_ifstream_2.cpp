// Test library for the std::basic_ifstream<wchar_t> constructor (unit v5724; libtpw_unit_std_basic_ifstream_basic_ifstream_2.dylib), main app:
//   src/game/MslSupport.cpp   BasicIfstreamWchar::construct
//
// The callees (basic_istream and basic_filebuf constructors) are fakes recording their arguments as tags; the vtable globals are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_ifstream.h"

using namespace calllog;

uint32_t gIfsWVtblB[4];
uint32_t gIfsWVtblE[4];
char gIosBaseVt[16];

namespace {
char* gObj = nullptr;

// 0 the object, 1 the filebuf part (Mac this + 0xc, host + 0x10), 2 NULL, 3 anything else
int64_t Tag(const void* p) {
    const char* c = static_cast<const char*>(p);
    if (c == gObj) return 0;
    if (c == gObj + offsetof(BasicIfstreamWchar, mFilebufVt)) return 1;
    if (c == nullptr) return 2;
    return 3;
}
}  // namespace

namespace ifstream_callee {
void IstreamCtor(void* self, int32_t flag, void* streambuf) { ev("IstreamCtor", {Tag(self), flag, Tag(streambuf)}); }
void FilebufCtor(void* self, void* file) { ev("FilebufCtor", {Tag(self), Tag(file)}); }
}  // namespace ifstream_callee

extern "C" {

uint64_t tpw_ifs2_addr(const char* name) {
    if (std::strcmp(name, "vtB") == 0) return reinterpret_cast<uintptr_t>(gIfsWVtblB);
    return 0;
}

uint64_t tpw_ifs2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ifs2_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicIfstreamWchar, mVbase));
    out[n++] = static_cast<int64_t>(offsetof(BasicIfstreamWchar, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(BasicIfstreamWchar, mFilebufVt));
    out[n++] = static_cast<int64_t>(offsetof(BasicIfstreamWchar, mIosVtable));
    out[n++] = static_cast<int64_t>(sizeof(BasicIfstreamWchar));
    out[n++] = static_cast<int64_t>(offsetof(IfstreamWVbTable, mBaseOffset));
    return n;
}

uint64_t tpw_ifs2_ctor(uint64_t obj, int32_t flag) {
    gObj = reinterpret_cast<char*>(obj);
    BasicIfstreamWchar* r = reinterpret_cast<BasicIfstreamWchar*>(obj)->construct(static_cast<int16_t>(flag));
    return reinterpret_cast<uintptr_t>(r) == obj ? 1 : 0;
}

}  // extern "C"
