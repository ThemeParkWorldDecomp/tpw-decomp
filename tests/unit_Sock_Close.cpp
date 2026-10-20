// Test library for Sock::Close (unit v5765; libtpw_unit_Sock_Close.dylib, tests/test_Sock_Close.py), main app:
//   src/COnlineParser_ParseGoto_COnlineParser_ParseIgnore_Sock_Close.cpp   Sock::Close
//
// shutdown, recv and closesocket are fakes that record their arguments; recv answers from the queue "Recv".
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"

using namespace calllog;

namespace ftp_callee {

void Shutdown(int32_t sock, int32_t how) { ev("Shutdown", {sock, how}); }

int32_t Recv(int32_t sock, void* buffer, int32_t size, int32_t flags) {
    (void)buffer;
    ev("Recv", {sock, size, flags});
    return static_cast<int32_t>(ret("Recv"));
}

void CloseSocket(int32_t sock) { ev("CloseSocket", {sock}); }

}  // namespace ftp_callee

extern "C" {

uint64_t tpw_sock_addr(const char* name) {
    (void)name;
    return 0;
}

uint64_t tpw_sock_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sock_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Sock, mSocket));
    out[n++] = static_cast<int64_t>(sizeof(Sock));
    return n;
}

void tpw_sock_close(uint64_t self) { reinterpret_cast<Sock*>(static_cast<uintptr_t>(self))->Close(); }

}  // extern "C"
