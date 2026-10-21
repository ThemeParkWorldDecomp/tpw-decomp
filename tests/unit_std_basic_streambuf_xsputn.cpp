// Test library for basic_streambuf<wchar_t>::xsputn / xsgetn (unit v5185; libtpw_unit_std_basic_streambuf_xsputn.dylib), main app:
//   src/game/MslSupport.cpp
//
// wmemcpy and the two virtual calls (uflow, overflow) are recording fakes that answer from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_streambuf_wchar.h"

using namespace calllog;

char16_t* wmemcpy(char16_t* dst, const char16_t* src, size_t n) {
    ev("wmemcpy", {static_cast<int64_t>(n)});
    std::memcpy(dst, src, n * 2);
    return dst;
}

namespace {
class TestBuf : public MslWStreambuf {
public:
    uint32_t uflow() override {
        ev("uflow", {1});
        return static_cast<uint32_t>(ret("uflow"));
    }
    uint32_t overflow(uint32_t c) override {
        ev("overflow", {1, static_cast<int64_t>(c)});
        return static_cast<uint32_t>(ret("overflow"));
    }
};
}  // namespace

extern "C" {

uint64_t tpw_sb_addr(const char*) { return 0; }

uint64_t tpw_sb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_sb_newobj() {
    void* m = tpw::ArenaAllocZeroed(sizeof(TestBuf));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (m) TestBuf()));
}

int32_t tpw_sb_layout(int64_t* out) {
    TestBuf o;
    const char* b = reinterpret_cast<const char*>(&o);
    int32_t n = 0;
    out[n++] = reinterpret_cast<const char*>(&o.mGNext) - b;
    out[n++] = reinterpret_cast<const char*>(&o.mGEnd) - b;
    out[n++] = reinterpret_cast<const char*>(&o.mPBeg) - b;
    out[n++] = reinterpret_cast<const char*>(&o.mPNext) - b;
    out[n++] = reinterpret_cast<const char*>(&o.mPEnd) - b;
    return n;
}

int32_t tpw_sb_xsputn(uint64_t obj, uint64_t s, int32_t n) {
    return reinterpret_cast<TestBuf*>(obj)->xsputn(reinterpret_cast<const char16_t*>(s), n);
}

int32_t tpw_sb_xsgetn(uint64_t obj, uint64_t s, int32_t n) {
    return reinterpret_cast<TestBuf*>(obj)->xsgetn(reinterpret_cast<char16_t*>(s), n);
}

}  // extern "C"
