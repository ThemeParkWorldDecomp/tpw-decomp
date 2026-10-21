// Test library for FTPClient::_Connect and FTPClient::_Download (unit v5227; libtpw_unit_FTPClient__Download.dylib,
// tests/test_FTPClient__Download.py), main app:
//   src/game/online/FTPClient.cpp   _Connect, _Download
//
// The callees outside the unit are recording fakes: the connection's SendCommandLine and RecvResponse, the socket and C library imports,
// Sock::Close, _CloseDSocket, ParseFileSize, RecvData and _EnterPassiveMode. Each logs the arguments the real one reads and answers from
// the test's queue of the same name. Pointers into the object are logged as labels or as "is this the buffer" flags (the host layout
// differs from the Mac's).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"

using namespace calllog;

namespace {

uint64_t gHostent = 0;     // the host address of the hostent the case built (32-bit addressable), set by the test
FTPClient* gSelf = nullptr;

struct SockAddrIn {
    uint16_t family;
    uint16_t port;
    uint32_t ip;
};

}  // namespace

// ---- the fakes
namespace ftp_callee {

void SendCommandLine(FTPClient* self, const char* line) { ev("SendCommandLine", {lab(self)}, {line}); }

int32_t RecvResponse(FTPClient* self, char* buffer, int32_t size, bool flag) {
    ev("RecvResponse", {lab(self), static_cast<int64_t>(reinterpret_cast<uintptr_t>(buffer)), size, flag ? 1 : 0});
    return static_cast<int32_t>(ret("RecvResponse"));
}

int32_t Recv(int32_t sock, void* buffer, int32_t size, int32_t flags) {
    ev("recv", {sock, buffer == static_cast<void*>(gSelf->mBuffer) ? 1 : 0, size, flags});
    return static_cast<int32_t>(ret("recv"));
}

int32_t Socket(int32_t family, int32_t type, int32_t protocol) {
    ev("socket", {family, type, protocol});
    return static_cast<int32_t>(ret("socket"));
}

uint32_t InetAddr(const char* text) {
    ev("inet_addr", {}, {text});
    return static_cast<uint32_t>(ret("inet_addr"));
}

int32_t Bind(int32_t sock, const void* addr, int32_t length) {
    const SockAddrIn* sa = static_cast<const SockAddrIn*>(addr);
    ev("bind", {sock, sa->family, sa->port, static_cast<int64_t>(sa->ip), length});
    return static_cast<int32_t>(ret("bind"));
}

int32_t SockConnect(int32_t sock, const void* addr, int32_t length) {
    const SockAddrIn* sa = static_cast<const SockAddrIn*>(addr);
    ev("connect", {sock, sa->family, sa->port, static_cast<int64_t>(sa->ip), length});
    return static_cast<int32_t>(ret("connect"));
}

FTPHostent* GetHostByName(const char* name) {
    ev("gethostbyname", {}, {name});
    return ret("gethostbyname") != 0 ? reinterpret_cast<FTPHostent*>(static_cast<uintptr_t>(gHostent)) : nullptr;
}

void Shutdown(int32_t sock, int32_t how) {
    ev("shutdown", {sock, how});
    ret("shutdown");
}

void CloseSocket(int32_t sock) {
    ev("closesocket", {sock});
    ret("closesocket");
}

void SockClose(FTPClient* self) { ev("SockClose", {lab(self)}); }

void CloseDSocket(FTPClient* self, int32_t* sock) { ev("CloseDSocket", {lab(self), sock == &self->mListenSocket ? 1 : 0}); }

int32_t ParseFileSize(FTPClient* self, char* text) {
    ev("ParseFileSize", {lab(self), text == reinterpret_cast<char*>(self->mBuffer) ? 1 : 0});
    return static_cast<int32_t>(ret("ParseFileSize"));
}

}  // namespace ftp_callee

int32_t FTPClient::RecvData(bool save, int32_t count) {
    ev("RecvData", {lab(this), save ? 1 : 0, count});
    return static_cast<int32_t>(ret("RecvData"));
}

int32_t FTPClient::_EnterPassiveMode() {
    ev("EnterPassiveMode", {lab(this)});
    return static_cast<int32_t>(ret("EnterPassiveMode"));
}

extern "C" {

uint64_t tpw_ftp_addr(const char* name) {
    if (std::strcmp(name, "hostent") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gHostent));
    return 0;
}

uint64_t tpw_ftp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_ftp_layout(int64_t* out) {
    alignas(16) static char storage[0x4000];
    FTPClient* p = reinterpret_cast<FTPClient*>(storage);
    auto off = [&](const void* member) { return static_cast<int64_t>(reinterpret_cast<const char*>(member) - storage); };
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(FTPClient));
    out[n++] = off(p->mF04);
    out[n++] = off(&p->mTotal);
    out[n++] = off(&p->mDone);
    out[n++] = off(p->mBuffer);
    out[n++] = off(&p->mDataSocket);
    out[n++] = off(&p->mListenSocket);
    out[n++] = off(&p->mPort);
    out[n++] = off(&p->mIpWord);
    out[n++] = off(p->mStrings);
    out[n++] = off(p->mPath);
    out[n++] = off(&p->mServer);
    FTPStringRep* rep = reinterpret_cast<FTPStringRep*>(storage);
    out[n++] = static_cast<int64_t>(reinterpret_cast<const char*>(&rep->mData) - storage);
    out[n++] = static_cast<int64_t>(sizeof(FTPStringRep));
    return n;
}

int32_t tpw_ftp_connect(uint64_t self) {
    gSelf = reinterpret_cast<FTPClient*>(static_cast<uintptr_t>(self));
    return gSelf->_Connect();
}

int32_t tpw_ftp_download(uint64_t self) {
    gSelf = reinterpret_cast<FTPClient*>(static_cast<uintptr_t>(self));
    return gSelf->_Download();
}

}  // extern "C"
