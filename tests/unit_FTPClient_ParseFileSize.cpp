// Test library for the FTPClient members of unit v5273 (libtpw_unit_FTPClient_ParseFileSize.dylib, tests/test_FTPClient_ParseFileSize.py), main app:
//   src/FTPClient_RecvData_FTPClient_ParseFileSize.cpp
//
// The callees outside the unit are recording fakes (the same event names and arguments as the PowerPC stubs). _Download and _threadfn
// and the destructor (the vtable's key function) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"

using namespace calllog;

namespace {

FTPClient* gObj = nullptr;
const char* gText = nullptr;

// The hash both sides compute over at most `limit` bytes up to the first NUL.
int64_t Hash(const char* s, int64_t limit) {
    uint32_t h = 0;
    for (int64_t i = 0; i < limit && s[i] != 0; ++i) h += static_cast<uint32_t>(static_cast<uint8_t>(s[i])) * static_cast<uint32_t>(i + 1);
    return h;
}

}  // namespace

FTPClient::~FTPClient() {}
void* FTPClient::_threadfn(void*) { return nullptr; }
int32_t FTPClient::_Download() {
    ev("_Download", {lab(this)});
    return static_cast<int32_t>(ret("_Download"));
}

namespace ftp_callee {

void ConnectionCtor(FTPClient* self) { ev("ConnectionCtor", {lab(self)}); }
void StringCtor(void* str) { ev("StringCtor", {lab(str)}); }
void CondCtor(void* cond) { ev("CondCtor", {lab(cond)}); }
int32_t WSAStartup(int32_t version, void*) {
    ev("WSAStartup", {version});
    return 0;
}
char* Strncpy(char* dest, const char* src, int32_t n) {
    ev("Strncpy", {lab(dest), Hash(src, n), n});
    return std::strncpy(dest, src, static_cast<size_t>(n));
}
int32_t Atoi(const char* text) {
    ev("Atoi", {static_cast<int64_t>(text - gText), Hash(text, 0x400)});
    return static_cast<int32_t>(ret("Atoi"));
}
int32_t BthreadCreate(void* (*fn)(void*), FTPClient* arg) {
    ev("BthreadCreate", {lab(reinterpret_cast<void*>(fn)), lab(arg)});
    return static_cast<int32_t>(ret("BthreadCreate"));
}
void Sleep(uint32_t ms) {
    ev("Sleep", {static_cast<int64_t>(ms)});
    if (ret("Sleep") != 0) gObj->mReady = 1;
}
void CondSignal(void* cond) { ev("CondSignal", {lab(cond)}); }
void SendCommandLine(FTPClient* self, const char* line) { ev("SendCommandLine", {lab(self), Hash(line, 0x400)}); }
int32_t RecvResponse(FTPClient* self, char* buffer, int32_t size, bool flag) {
    ev("RecvResponse", {lab(self), static_cast<int64_t>(reinterpret_cast<uintptr_t>(buffer)), size, flag ? 1 : 0});
    return static_cast<int32_t>(ret("RecvResponse"));
}
void SockClose(FTPClient* self) { ev("SockClose", {lab(self)}); }
void Shutdown(int32_t sock, int32_t how) { ev("Shutdown", {sock, how}); }
int32_t Recv(int32_t sock, void* buffer, int32_t size, int32_t flags) {
    ev("Recv", {sock, buffer == static_cast<void*>(gObj->mBuffer) ? 1 : 0, size, flags});
    return static_cast<int32_t>(ret("Recv"));
}
void CloseSocket(int32_t sock) { ev("CloseSocket", {sock}); }

}  // namespace ftp_callee

extern "C" {

uint64_t tpw_ftp_addr(const char* name) {
    if (std::strcmp(name, "threadfn") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&FTPClient::_threadfn));
    return 0;
}

uint64_t tpw_ftp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the members the module compares (in the module's HOST_FIELDS order), then sizeof.
int32_t tpw_ftp_layout(int64_t* out) {
    alignas(16) static char raw[sizeof(FTPClient) + 16];
    FTPClient* p = reinterpret_cast<FTPClient*>(raw);
    auto off = [&](const void* m) { return static_cast<int64_t>(static_cast<const char*>(m) - raw); };
    int32_t n = 0;
    out[n++] = off(p->mF04);
    out[n++] = off(&p->mDataSocket);
    out[n++] = off(&p->mListenSocket);
    out[n++] = off(&p->mFamily);
    out[n++] = off(p->mBuffer);
    out[n++] = off(p->mPath);
    out[n++] = off(&p->mControl);
    out[n++] = off(&p->mServer);
    out[n++] = off(p->mCondA);
    out[n++] = off(p->mCondB);
    out[n++] = off(&p->mF276c);
    out[n++] = off(&p->mReady);
    out[n++] = off(&p->mThreadStarted);
    out[n++] = off(&p->mThreaded);
    out[n++] = static_cast<int64_t>(sizeof(FTPClient));
    return n;
}

// Makes the object (the vptr) and tells the fakes which object and text block are in use.
void tpw_ftp_setup(uint64_t obj, uint64_t text) {
    gObj = reinterpret_cast<FTPClient*>(static_cast<uintptr_t>(obj));
    gText = reinterpret_cast<const char*>(static_cast<uintptr_t>(text));
    new (gObj) FTPClient;
}

static FTPClient* O(uint64_t obj) { return reinterpret_cast<FTPClient*>(static_cast<uintptr_t>(obj)); }

int32_t tpw_ftp_parse(uint64_t obj, uint64_t text) { return O(obj)->ParseFileSize(reinterpret_cast<char*>(static_cast<uintptr_t>(text))); }
void tpw_ftp_closed(uint64_t obj, uint64_t sock) { O(obj)->_CloseDSocket(*reinterpret_cast<int32_t*>(static_cast<uintptr_t>(sock))); }
int32_t tpw_ftp_download(uint64_t obj, uint64_t a2, uint64_t a3) {
    return O(obj)->Download(reinterpret_cast<char*>(static_cast<uintptr_t>(a2)), reinterpret_cast<char*>(static_cast<uintptr_t>(a3)));
}
int32_t tpw_ftp_disconnect(uint64_t obj) { return O(obj)->Disconnect(); }
int32_t tpw_ftp__disconnect(uint64_t obj) { return O(obj)->_Disconnect(); }
void tpw_ftp_ctor(uint64_t obj, uint32_t control, int32_t threaded) {
    O(obj)->Construct(reinterpret_cast<CFTPControl*>(static_cast<uintptr_t>(control)), threaded != 0);
}

}  // extern "C"
