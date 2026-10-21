// Test library for std::basic_streambuf<char>::xsgetn / xsputn (unit v5147; tests/test_std_basic_streambuf_xsgetn.py), main app:
//   src/game/MslSupport.cpp
// The virtual uflow and overflow are recording fakes answering from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"

using namespace calllog;

namespace streambuf_io_callee {
int32_t Uflow(void*) {
    ev("Uflow", {0});
    return static_cast<int32_t>(ret("Uflow"));
}
int32_t Overflow(void*, int32_t c) {
    ev("Overflow", {c});
    return static_cast<int32_t>(ret("Overflow"));
}
}  // namespace streambuf_io_callee

extern "C" {

uint64_t tpw_sbio_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sbio_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicStreambufCharIO, mGptr));
    out[n++] = static_cast<int64_t>(offsetof(BasicStreambufCharIO, mEgptr));
    out[n++] = static_cast<int64_t>(offsetof(BasicStreambufCharIO, mPptr));
    out[n++] = static_cast<int64_t>(offsetof(BasicStreambufCharIO, mEpptr));
    out[n++] = static_cast<int64_t>(sizeof(BasicStreambufCharIO));
    return n;
}

int32_t tpw_sbio_xsgetn(uint64_t obj, uint64_t dst, int32_t count) {
    return reinterpret_cast<BasicStreambufCharIO*>(obj)->xsgetn(reinterpret_cast<char*>(dst), count);
}

int32_t tpw_sbio_xsputn(uint64_t obj, uint64_t src, int32_t count) {
    return reinterpret_cast<BasicStreambufCharIO*>(obj)->xsputn(reinterpret_cast<const char*>(src), count);
}

}  // extern "C"
