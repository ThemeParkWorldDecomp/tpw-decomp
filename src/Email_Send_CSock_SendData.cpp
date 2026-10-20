// CSock::CSock 0x10201f50 and CSock::SendData 0x10202058 (unit v5304).
#include <cstdint>
#include <cstring>

#include "email.h"

CSock::CSock() {
    csock_callee::SockCtor(this);
    mVtable = 0x102b1638;   // TOC[-0x5f10]: the CSock vtable's address
    std::memset(mF08, 0, 0x10);
}

int32_t CSock::SendData(const char* data, int32_t count) {
    if (data == nullptr) return 1;
    while (count > 0) {
        int32_t chunk = count;
        if (chunk > 0x2000) chunk = 0x2000;
        int32_t sent = csock_callee::Send(mSocket, data, chunk, 0);
        if (sent < 0) return 1;
        data += sent;
        count -= sent;
    }
    return 0;
}
