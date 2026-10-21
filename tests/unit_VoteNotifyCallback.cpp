// Test library for VoteNotifyCallback, SwearFilter::Init, stristr, IPBase::ShowError and FTPClient::ProcessEvents (unit v51009;
// libtpw_unit_VoteNotifyCallback.dylib, tests/test_VoteNotifyCallback.py), main app:
//   src/game/online/FTPClient.cpp, src/game/online/IPBase.cpp, src/game/online/online.cpp   the five functions and the globals gSwearLowerMap, gIPErrorCodes, gIPErrorTexts
//
// The callees outside the unit are fakes that record what the PowerPC stubs record. gOnlineControl is defined here (other files define it).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "FS_Online.h"
#include "FS_MasterSave.h"
#include "WinScrn.h"

using namespace calllog;

COnlineControl* gOnlineControl;

namespace {
FTPClient* gClient;   // the object ProcessEvents runs on (the receive buffer is compared by address)
int64_t HostOffset(const void* base, const void* member) {
    return static_cast<int64_t>(static_cast<const char*>(member) - static_cast<const char*>(base));
}
}  // namespace

// ---- VoteNotifyCallback's callees
COnlineControl::COnlineControl() { ev("COnlineControl", {newlab(this)}); }
void COnlineControl::CallbackFn(int32_t id, void* arguments) {
    const uint32_t* v = static_cast<const uint32_t*>(arguments);
    ev("CallbackFn", {lab(this), id, static_cast<int64_t>(v[0]), static_cast<int64_t>(v[1]), static_cast<int64_t>(v[2])});
}
namespace online_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    if (ret("newnull") != 0) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}
void StringCtor(void* self) {
    ev("StringCtor", {});
    *static_cast<uint32_t*>(self) = 0x1111;
}
void WStringCtor(void* self) {
    ev("WStringCtor", {});
    *static_cast<uint32_t*>(self) = 0x2222;
}
void WStringDtor(void* self, int16_t flag) {
    ev("WStringDtor", {static_cast<int64_t>(*static_cast<uint32_t*>(self)), flag});
}
void StringDtor(void* self, int16_t flag) {
    ev("StringDtor", {static_cast<int64_t>(*static_cast<uint32_t*>(self)), flag});
}
void ArgumentVectorPushBack(void* vec, const void* argument) {
    uint32_t* v = static_cast<uint32_t*>(vec);
    const uint32_t* a = static_cast<const uint32_t*>(argument);
    ev("push", {static_cast<int64_t>(a[0]), static_cast<int64_t>(a[1]), static_cast<int64_t>(a[2]), static_cast<int64_t>(v[0])});
    v[0] += 1;
}
void ArgumentVectorTearDown(void* vec) {
    ev("tear_down", {static_cast<int64_t>(*static_cast<uint32_t*>(vec))});
}
}  // namespace online_callee

// ---- SwearFilter::Init's callees
namespace swear_callee {
void EraseList(std::list<std::u16string>* list) { ev("erase", {lab(list)}); }
}
bool SwearFilter::LoadWordList(const char* path, std::list<std::u16string>& words, bool flag) {
    ev("LoadWordList", {lab(this), lab(&words), flag ? 1 : 0}, {path});
    return ret("LoadWordList") != 0;
}

// ---- IPBase::ShowError's callees
namespace ip_callee {
int32_t WSAGetLastError() { return static_cast<int32_t>(ret("wsa")); }
}
namespace NS_MacDoze {
int32_t MessageBoxA(GrafPort* window, const char* text, const char* caption, uint32_t flags) {
    ev("MessageBoxA", {lab(window), static_cast<int64_t>(flags)}, {text, caption});
    return 0;
}
}  // namespace NS_MacDoze

// ---- FTPClient::ProcessEvents's callees
FTPClient::~FTPClient() {}
int32_t FTPClient::_Connect() {
    ev("_Connect", {lab(this)});
    return static_cast<int32_t>(ret("_Connect"));
}
int32_t FTPClient::_Download() {
    ev("_Download", {lab(this)});
    return static_cast<int32_t>(ret("_Download"));
}
namespace ftp_callee {
void SendCommandLine(FTPClient* self, const char* line) { ev("SendCommandLine", {lab(self)}, {line}); }
int32_t RecvResponse(FTPClient* self, char* buffer, int32_t size, bool flag) {
    ev("RecvResponse", {lab(self), buffer == nullptr ? 0 : 1, size, flag ? 1 : 0});
    return static_cast<int32_t>(ret("RecvResponse"));
}
int32_t Recv(int32_t sock, void* buffer, int32_t size, int32_t flags) {
    ev("recv", {sock, buffer == gClient->mBuffer ? 1 : 0, size, flags});
    return static_cast<int32_t>(ret("recv"));
}
void Shutdown(int32_t sock, int32_t how) { ev("shutdown", {sock, how}); }
void CloseSocket(int32_t sock) { ev("closesocket", {sock}); }
void SockClose(FTPClient* self) { ev("SockClose", {lab(self)}); }
void CondWait(void* cond, int32_t timeout) {
    // the new state goes into the object's word 0x276c (the host's mF276c)
    FTPClient* self = reinterpret_cast<FTPClient*>(static_cast<char*>(cond) - HostOffset(gClient, gClient->mCondA));
    int32_t state = static_cast<int32_t>(ret("state"));
    self->mF276c = state;
    ev("CondWait", {timeout, state});
}
void ControlCallback(CFTPControl* control, int32_t state, int32_t result) {
    ev("ControlCallback", {lab(control), state, result});
}
}  // namespace ftp_callee

extern "C" {

uint64_t tpw_vn_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineControl", &gOnlineControl},
        {"codes", &gIPErrorCodes},
        {"texts", &gIPErrorTexts},
        {"lowmap", gSwearLowerMap},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_vn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the members the module places by Mac offset: the filter's second list, then the FTP client's socket (Mac 0x04),
// receive buffer (0x20), listen socket (0x2428), condition (0x2764), control (0x275c), state (0x276c), ready byte (0x2770), size.
int32_t tpw_vn_layout(int64_t* out) {
    int32_t n = 0;
    alignas(16) static char block[0x4000];
    SwearFilter* sf = reinterpret_cast<SwearFilter*>(block);
    FTPClient* fc = reinterpret_cast<FTPClient*>(block);
    out[n++] = HostOffset(sf, &sf->mAllowed);
    out[n++] = HostOffset(fc, fc->mF04);
    out[n++] = HostOffset(fc, fc->mBuffer);
    out[n++] = HostOffset(fc, &fc->mListenSocket);
    out[n++] = HostOffset(fc, fc->mCondA);
    out[n++] = HostOffset(fc, &fc->mControl);
    out[n++] = HostOffset(fc, &fc->mF276c);
    out[n++] = HostOffset(fc, &fc->mReady);
    out[n++] = static_cast<int64_t>(sizeof(FTPClient));
    return n;
}

void tpw_vn_vote(int32_t a, int32_t b) { VoteNotifyCallback(a, b, nullptr, nullptr); }

uint32_t tpw_vn_init(uint64_t self, uint64_t wordFile, uint64_t allowedFile, uint32_t flag) {
    return reinterpret_cast<SwearFilter*>(self)->Init(reinterpret_cast<const char*>(wordFile),
                                                       reinterpret_cast<const char*>(allowedFile), flag != 0);
}

uint64_t tpw_vn_stristr(uint64_t haystack, uint64_t needle) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        stristr(reinterpret_cast<char16_t*>(haystack), reinterpret_cast<char16_t*>(needle))));
}

void tpw_vn_showerror(uint64_t self) { reinterpret_cast<IPBase*>(self)->ShowError(); }

void tpw_vn_events(uint64_t client) {
    gClient = reinterpret_cast<FTPClient*>(client);
    gClient->ProcessEvents();
}

}  // extern "C"
