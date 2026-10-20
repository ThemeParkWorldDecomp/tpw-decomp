// Test library for Connection::RecvResponse and Connection::GetLocalIPAddress (unit v5371; libtpw_unit_Connection_RecvResponse.dylib,
// tests/test_Connection_RecvResponse.py), main app: src/Email_Send_CSock_SendData_Connection_RecvResponse.cpp.
//
// The callees (atoi, strncpy, memmove, recv, gethostname, gethostbyname, socket, inet_addr, connect, getsockname, closesocket) are fakes
// that record what the real ones read; the answers come from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"

using namespace calllog;

namespace {
int64_t gObj = 0;
int64_t gOut = 0;
const uint8_t* gBlob = nullptr;
int32_t gBlobLen = 0;
int32_t gBlobOff = 0;
uint32_t gHeIp = 0;
uint32_t* gHeList[2];
FTPHostent gHe;
bool gHeOn = false;

int64_t Addr(const void* p) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(p)); }
}  // namespace

namespace ftp_callee {
int32_t Atoi(const char* text) {
    const uint8_t* b = reinterpret_cast<const uint8_t*>(text);
    ev("Atoi", {Addr(text) - gObj, b[0], b[1], b[2], b[3]});
    return static_cast<int32_t>(ret("Atoi"));
}
char* Strncpy(char* dest, const char* src, int32_t n) {
    ev("Strncpy", {Addr(dest) - gOut, Addr(src) - gObj, n});
    bool ended = false;
    for (int32_t i = 0; i < n; i++) {
        if (!ended && src[i] == 0) ended = true;
        dest[i] = ended ? 0 : src[i];
    }
    return dest;
}
int32_t Recv(int32_t sock, void* buffer, int32_t size, int32_t flags) {
    ev("Recv", {sock, Addr(buffer) - gObj, size, flags});
    int64_t q = ret("Recv");
    int32_t n = static_cast<int32_t>(q);
    if (n > size) n = size;
    if (n > 0) {
        uint8_t* d = static_cast<uint8_t*>(buffer);
        for (int32_t i = 0; i < n; i++) d[i] = (gBlobOff + i < gBlobLen) ? gBlob[gBlobOff + i] : 0;
        gBlobOff += n;
    }
    return n;
}
int32_t Socket(int32_t family, int32_t type, int32_t protocol) {
    ev("Socket", {family, type, protocol});
    return static_cast<int32_t>(ret("Socket"));
}
uint32_t InetAddr(const char* text) {
    int64_t v[12];
    for (int i = 0; i < 12; i++) v[i] = static_cast<uint8_t>(text[i]);
    ev("InetAddr", {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11]});
    return static_cast<uint32_t>(ret("InetAddr"));
}
int32_t GetSockName(int32_t sock, void* addr, int32_t* length) {
    ev("GetSockName", {sock, *length});
    uint32_t ip = static_cast<uint32_t>(ret("GetSockName"));
    std::memcpy(static_cast<uint8_t*>(addr) + 4, &ip, 4);
    return 0;
}
int32_t SockConnect(int32_t sock, const void* addr, int32_t length) {
    uint16_t f, p;
    uint32_t ip;
    std::memcpy(&f, addr, 2);
    std::memcpy(&p, static_cast<const uint8_t*>(addr) + 2, 2);
    std::memcpy(&ip, static_cast<const uint8_t*>(addr) + 4, 4);
    ev("Connect", {sock, f, p, ip, length});
    return static_cast<int32_t>(ret("Connect"));
}
FTPHostent* GetHostByName(const char* name) {
    int64_t v[8];
    for (int i = 0; i < 8; i++) v[i] = static_cast<uint8_t>(name[i]);
    ev("GetHostByName", {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]});
    return gHeOn ? &gHe : nullptr;
}
void CloseSocket(int32_t sock) { ev("CloseSocket", {sock}); }
}  // namespace ftp_callee

namespace conn_callee {
int32_t Memmove(void* dest, const void* src, int32_t n) {
    ev("Memmove", {Addr(dest) - gObj, Addr(src) - gObj, n});
    std::memmove(dest, src, static_cast<size_t>(n));
    return 0;
}
int32_t Gethostname(char* name, int32_t size) {
    ev("Gethostname", {size});
    std::memcpy(name, "tpwhost", 8);
    return static_cast<int32_t>(ret("Gethostname"));
}
}  // namespace conn_callee

extern "C" {

uint64_t tpw_conn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_conn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Connection, mSocket));
    out[n++] = static_cast<int64_t>(offsetof(Connection, mBuf));
    out[n++] = static_cast<int64_t>(offsetof(Connection, mLen));
    out[n++] = static_cast<int64_t>(sizeof(Connection));
    return n;
}

// The case's world for the fakes: the object and output addresses (the log names offsets), the bytes recv delivers, the hostent.
void tpw_conn_ctx(uint64_t obj, uint64_t out, const uint8_t* blob, int32_t blobLen, int32_t heOn, uint32_t heIp) {
    gObj = static_cast<int64_t>(obj);
    gOut = static_cast<int64_t>(out);
    gBlob = blob;
    gBlobLen = blobLen;
    gBlobOff = 0;
    gHeOn = heOn != 0;
    gHeIp = heIp;
    gHeList[0] = &gHeIp;
    gHeList[1] = nullptr;
    gHe.mAddrList = gHeList;
}

int32_t tpw_conn_recv(uint64_t obj, uint64_t out, int32_t n, uint32_t flag) {
    return reinterpret_cast<Connection*>(static_cast<uintptr_t>(obj))->RecvResponse(reinterpret_cast<char*>(static_cast<uintptr_t>(out)), n, flag != 0);
}

uint32_t tpw_conn_ip() {
    return reinterpret_cast<Connection*>(static_cast<uintptr_t>(gObj))->GetLocalIPAddress();
}

}  // extern "C"
