// Test library for the FTP client's RecvData, destructor body, Connect and _EnterPassiveMode (unit v5101;
// libtpw_unit_FTPClient_RecvData.dylib, tests/test_FTPClient_RecvData.py), main app:
//   src/FTPClient_RecvData.cpp
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"

using namespace calllog;

namespace {
FTPClient* gObj = nullptr;
const char* const kResp[] = {
    "227 Entering Passive Mode (192,168,1,2,19,136).\r\n",
    "227 no paren here\r\n",
    "(1,2,3,4,5,6)",
    "x(300,257,-1,255,256,70000)",
    "(0,0,0,0,255,255)",
    "(10,20,30,40,1,2",
    "(127,0,0,1,-3,65)",
};
int64_t I(int64_t v) { return v; }
int32_t Which(void* p) {
    if (p == gObj->mCondA) return 1;
    if (p == gObj->mCondB) return 2;
    if (p == &gObj->mServer) return 3;
    return 0;
}
}  // namespace

void* FTPClient::_threadfn(void*) { return nullptr; }
int32_t FTPClient::_Connect() {
    ev("_Connect", {lab(this)});
    return static_cast<int32_t>(ret("_Connect"));
}

namespace ftp_callee {
void SendCommandLine(FTPClient* self, const char* line) { ev("SendCommandLine", {lab(self)}, {line}); }
int32_t RecvResponse(FTPClient* self, char* buffer, int32_t size, bool flag) {
    ev("RecvResponse", {lab(self), I(buffer != nullptr), I(size), I(flag)});
    int32_t code = static_cast<int32_t>(ret("RecvResponse"));
    int32_t id = static_cast<int32_t>(ret("respid"));
    if (buffer != nullptr) std::strcpy(buffer, kResp[id]);
    return code;
}
uint32_t GetLocalIPAddress(FTPClient* self) {
    ev("GetLocalIPAddress", {lab(self)});
    return __builtin_bswap32(static_cast<uint32_t>(ret("GetLocalIPAddress")));
}
int32_t Accept(int32_t sock, void*, int32_t* length) {
    ev("accept", {I(sock), I(*length)});
    return static_cast<int32_t>(ret("accept"));
}
void* Fopen(const char* path, const char* mode) {
    ev("fopen", {}, {path, mode});
    return reinterpret_cast<void*>(static_cast<intptr_t>(static_cast<int32_t>(ret("fopen"))));
}
int32_t Recv(int32_t sock, void* buffer, int32_t size, int32_t flags) {
    ev("recv", {I(sock), I(size), I(flags)});
    int32_t n = static_cast<int32_t>(ret("recv"));
    int32_t nl = static_cast<int32_t>(ret("nl"));
    if (n > 0) {
        uint8_t* b = static_cast<uint8_t*>(buffer);
        for (int32_t i = 0; i < n; ++i) b[i] = static_cast<uint8_t>(97 + i % 26);
        if (nl >= 0 && nl < n) b[nl] = 10;
    }
    return n;
}
void Fwrite(const void* buffer, int32_t size, int32_t count, void* file) {
    int64_t sum = 0;
    const uint8_t* b = static_cast<const uint8_t*>(buffer);
    for (int32_t i = 0; i < count; ++i) sum += b[i];
    ev("fwrite", {I(size), I(count), I(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file))), I(sum)});
}
void Fclose(void* file) { ev("fclose", {I(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file)))}); }
int32_t Socket(int32_t family, int32_t type, int32_t protocol) {
    ev("socket", {I(family), I(type), I(protocol)});
    return static_cast<int32_t>(ret("socket"));
}
uint32_t InetAddr(const char* text) {
    ev("inet_addr", {}, {text});
    return __builtin_bswap32(static_cast<uint32_t>(ret("inet_addr")));
}
int32_t Bind(int32_t sock, const void* addr, int32_t length) {
    const uint8_t* b = static_cast<const uint8_t*>(addr);
    uint16_t family, port;
    std::memcpy(&family, b, 2);
    std::memcpy(&port, b + 2, 2);
    int64_t ip = (static_cast<int64_t>(b[4]) << 24) | (b[5] << 16) | (b[6] << 8) | b[7];
    ev("bind", {I(sock), I(family), I(port), ip, I(length)});
    return static_cast<int32_t>(ret("bind"));
}
int32_t Listen(int32_t sock, int32_t backlog) {
    ev("listen", {I(sock), I(backlog)});
    return static_cast<int32_t>(ret("listen"));
}
int32_t GetSockName(int32_t sock, void* addr, int32_t* length) {
    ev("getsockname", {I(sock), I(*length)});
    uint16_t port = static_cast<uint16_t>(ret("gsn"));
    std::memcpy(static_cast<uint8_t*>(addr) + 2, &port, 2);
    return 0;
}
void Sleep(uint32_t ms) {
    ev("Sleep", {I(ms)});
    if (ret("sleepready") != 0) gObj->mReady = 1;
}
void CondSignal(void* cond) { ev("signal", {I(Which(cond))}); }
void CondWait(void* cond, int32_t timeout) { ev("wait", {I(Which(cond)), I(timeout)}); }
void CondDtor(void* cond, int16_t flag) { ev("condDtor", {I(Which(cond)), I(flag)}); }
void WSACleanup() { ev("WSACleanup", {}); }
void StringDtor(void*, int16_t flag) { ev("stringDtor", {I(flag)}); }
void StringAssign(void*, const char* text, uint32_t length) { ev("assign", {I(length)}, {text}); }
void CSockDtor(FTPClient* self, int16_t flag) { ev("CSockDtor", {lab(self), I(flag)}); }
void OperatorDelete(void* p) { ev("delete", {lab(p)}); }
int32_t BthreadCreate(void* (*fn)(void*), FTPClient* arg) {
    ev("bthread_create", {I(fn == &FTPClient::_threadfn), lab(arg)});
    return static_cast<int32_t>(ret("bthread_create"));
}
}  // namespace ftp_callee

extern "C" {

uint64_t tpw_ftp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets: sizeof, then the members in the module's FIELDS order, then mCondA, mCondB, mServer.
int32_t tpw_ftp_layout(int64_t* out) {
    FTPClient* p = reinterpret_cast<FTPClient*>(tpw::ArenaAllocZeroed(sizeof(FTPClient)));
    auto off = [&](const void* m) { return static_cast<int64_t>(reinterpret_cast<const char*>(m) - reinterpret_cast<const char*>(p)); };
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(FTPClient));
    out[n++] = off(&p->mTotal);
    out[n++] = off(&p->mDone);
    out[n++] = off(&p->mBuffer);
    out[n++] = off(&p->mDataSocket);
    out[n++] = off(&p->mListenSocket);
    out[n++] = off(&p->mFamily);
    out[n++] = off(&p->mPort);
    out[n++] = off(&p->mIp);
    out[n++] = off(&p->mSockZero);
    out[n++] = off(&p->mStrings);
    out[n++] = off(&p->mPath);
    out[n++] = off(&p->mF276c);
    out[n++] = off(&p->mReady);
    out[n++] = off(&p->mThreadStarted);
    out[n++] = off(&p->mThreaded);
    out[n++] = off(&p->mCondA);
    out[n++] = off(&p->mCondB);
    out[n++] = off(&p->mServer);
    return n;
}

void tpw_ftp_setrep(uint64_t obj, int32_t length, uint64_t data) {
    FTPClient* c = reinterpret_cast<FTPClient*>(static_cast<uintptr_t>(obj));
    FTPStringRep* rep = static_cast<FTPStringRep*>(tpw::ArenaAllocZeroed(sizeof(FTPStringRep)));
    rep->mLength = length;
    rep->mCapacity = 3;
    rep->mRefCount = 1;
    rep->mData = reinterpret_cast<const char*>(static_cast<uintptr_t>(data));
    c->mServer.mRep = rep;
}

int32_t tpw_ftp_recvdata(uint64_t obj, uint32_t save, int32_t count) {
    gObj = reinterpret_cast<FTPClient*>(static_cast<uintptr_t>(obj));
    return gObj->RecvData(save != 0, count);
}
uint64_t tpw_ftp_destruct(uint64_t obj, int32_t flag) {
    gObj = reinterpret_cast<FTPClient*>(static_cast<uintptr_t>(obj));
    return reinterpret_cast<uintptr_t>(gObj->Destruct(static_cast<int16_t>(flag)));
}
int32_t tpw_ftp_connect(uint64_t obj, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    gObj = reinterpret_cast<FTPClient*>(static_cast<uintptr_t>(obj));
    return gObj->Connect(reinterpret_cast<char*>(static_cast<uintptr_t>(a)), reinterpret_cast<char*>(static_cast<uintptr_t>(b)),
                         reinterpret_cast<char*>(static_cast<uintptr_t>(c)), reinterpret_cast<char*>(static_cast<uintptr_t>(d)));
}
int32_t tpw_ftp_pasv(uint64_t obj) {
    gObj = reinterpret_cast<FTPClient*>(static_cast<uintptr_t>(obj));
    return gObj->_EnterPassiveMode();
}

}  // extern "C"
