// Test library for CSock (unit v5304; libtpw_unit_CSock_SendData.dylib, tests/test_CSock_SendData.py), main app:
//   src/game/online/Connection.cpp   CSock::CSock, CSock::SendData
//
// Sock::Sock writes two marker words into the object and records one event; send records its arguments (first byte and byte sum of
// the data) and answers from the test's queue: a negative value as is, else min(value, length).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"

using namespace calllog;

namespace csock_callee {

void SockCtor(void* self) {
    ev("SockCtor", {1});
    uint32_t* w = static_cast<uint32_t*>(self);
    w[1] = 0x5ca1ab1eu;
    w[2] = 0x11111111u;
}

int32_t Send(int32_t socket, const char* data, int32_t length, int32_t flags) {
    int64_t first = length > 0 ? static_cast<uint8_t>(data[0]) : 0;
    int64_t sum = 0;
    for (int32_t i = 0; i < length; i++) sum += static_cast<uint8_t>(data[i]);
    ev("send", {socket, length, flags, first, sum});
    int32_t q = static_cast<int32_t>(ret("send"));
    return q < 0 ? q : (q < length ? q : length);
}

}  // namespace csock_callee

extern "C" {

uint64_t tpw_cs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CSock, mSocket));
    out[n++] = static_cast<int64_t>(offsetof(CSock, mF08));
    out[n++] = static_cast<int64_t>(sizeof(CSock));
    return n;
}

uint32_t tpw_cs_ctor(uint64_t p) {
    CSock* c = new (reinterpret_cast<void*>(static_cast<uintptr_t>(p))) CSock();
    return c == reinterpret_cast<CSock*>(static_cast<uintptr_t>(p)) ? 1 : 0;
}

uint32_t tpw_cs_send(uint64_t p, uint64_t buf, int32_t len) {
    CSock* c = reinterpret_cast<CSock*>(static_cast<uintptr_t>(p));
    return static_cast<uint32_t>(c->SendData(reinterpret_cast<const char*>(static_cast<uintptr_t>(buf)), len));
}

}  // extern "C"
