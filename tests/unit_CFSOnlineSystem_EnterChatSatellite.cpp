// Test library for the online system's chat satellite, publishing, upload and FTP download entry points and AreWeProcessing (unit v5130;
// libtpw_unit_CFSOnlineSystem_EnterChatSatellite.dylib, tests/test_CFSOnlineSystem_EnterChatSatellite.py), main app:
//   src/game/online/FS_Online.cpp   the eight CFSOnlineSystem members
//
// The callees outside the unit are recording fakes (the same event names and arguments as the PowerPC stubs). The virtual calls (the
// string's operator= and the globe's slot 0x50) go through hand-made vtables whose slot is found from a pointer to the member. A callback
// the system keeps in a 32-bit word is the lower half of the function's address. Globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <sys/mman.h>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "lobby.h"

using namespace calllog;

CFSOnlineSystem* gOnlineSystem;
CLBScreenSystem* gLobbyScreens;

namespace {

uint32_t HashBytes(const char* text) {
    uint32_t h = 0;
    for (; *text != 0; ++text) h = h * 31u + static_cast<uint8_t>(*text);
    return h;
}

int64_t U32(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }

typedef TbDynamicStringTemplate<char> Str;

TbDynamicStringTemplate<char>* FakeStringAssign(TbDynamicStringTemplate<char>* self, const char* text) {
    ev("StringAssign", {lab(self), static_cast<int64_t>(std::strlen(text)), static_cast<int64_t>(HashBytes(text))});
    return self;
}

void FakeGlobeSlot50(CLBScreenComponentDownloadGlobe* self) { ev("GlobeSlot50", {lab(self)}); }

void FakeCallback(int32_t error, const void* context) { ev("Callback", {error, U32(context)}); }

size_t SlotOf(const void* pointerToMember) {
    uintptr_t words[2];
    std::memcpy(words, pointerToMember, sizeof(words));
    return words[0] / sizeof(void*);   // the arm64 member pointer to a virtual: the vtable offset, and bit 0 of the adjustment set
}

// The word a function pointer is kept as (its lower half; the program's code sits in one 4 GB window).
uint32_t MakeThunk(void* target) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target)); }

}  // namespace

// ---- the fakes
CFSOnlineSystem::CFSOnlineSystem() { ev("CFSOnlineSystem", {newlab(this)}); }
void CFSOnlineSystem::StopProcessing() { ev("StopProcessing", {lab(this)}); }
void CFSOnlineSystem::Logout() { ev("Logout", {lab(this)}); }
void CFSOnlineSystem::LoggedInRequestUpload() { ev("LoggedInRequestUpload", {lab(this)}); }
void CFSOnlineSystem::CompleteFTPDownload(int32_t failed) { ev("CompleteFTPDownload", {lab(this), failed}); }
void CFSOnlineSystem::RequestFTPPostLoginCallback(int32_t result, const void* context) {
    ev("RequestFTPPostLoginCallback", {lab(this), result, U32(context)});
}
void CFSOnlineSystem::GotoPlayerByIDFTPPostLoginCallback(int32_t result, const void* context) {
    ev("GotoPlayerByIDFTPPostLoginCallback", {lab(this), result, U32(context)});
}
void CFSOnlineSystem::LoggedInRequestFTPDownload(const CFSOnlinePark* park) { ev("LoggedInRequestFTPDownload", {lab(this), lab(park)}); }

CFSOnlinePark* CFSOnlineSystemParkList_InCity::ReturnParkInfo(int32_t index) {
    ev("ReturnParkInfo", {lab(this), index});
    return reinterpret_cast<CFSOnlinePark*>(static_cast<uintptr_t>(0x123400u + static_cast<uint32_t>(index) * 16u));
}

CFSOnlinePark& CFSOnlinePark::operator=(const CFSOnlinePark& other) {
    ev("ParkAssign", {lab(this), lab(&other)});
    return *this;
}

void Upload::Release() { ev("UploadRelease", {lab(this)}); }
void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {error, U32(table)}); }

void FSOnlineLoginRASFailureCallback() {}
void SatelliteOnlineParkInfoPanelCancelCallback() {}
void GeneralParkDownloadCompleteCallback(int32_t, const void*) {}
void RequestFTPPostLoginCallbackFn(int32_t, void*) {}
void GotoPlayerByIDFTPPostLoginCallbackFn(int32_t, void*) {}

void CreateOnlineLoginPanel(void (*onFailure)()) {
    ev("CreateOnlineLoginPanel", {onFailure == &FSOnlineLoginRASFailureCallback ? 1 : 0});
}

void CreateOnlineParkInfoPanel(const CFSOnlinePark* park, InterfaceWindow* parent, void (*onCancel)(),
                               void (*onDownload)(int32_t, const void*), void* context) {
    ev("CreateOnlineParkInfoPanel", {U32(park), lab(parent), onCancel == &SatelliteOnlineParkInfoPanelCancelCallback ? 1 : 0,
                                     onDownload == &GeneralParkDownloadCompleteCallback ? 1 : 0, U32(context)});
}

extern "C" {

uint64_t tpw_cs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineSystem", &gOnlineSystem},
        {"gLobbyScreens", &gLobbyScreens},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the members the module writes by name (the Mac's are in the module).
#pragma clang diagnostic ignored "-Winvalid-offsetof"
int64_t tpw_cs_off(const char* name) {
    struct Entry {
        const char* name;
        int64_t offset;
    };
#define SYS(n) {"sys." #n, static_cast<int64_t>(offsetof(CFSOnlineSystem, n))}
    const Entry entries[] = {
        SYS(mLogin), SYS(mLoginCallback), SYS(mLoginContext), SYS(mLoggedIn), SYS(mNews), SYS(mCityList), SYS(mParksTop10),
        SYS(mParksInCity), SYS(mParksByPlayer), SYS(mParksByID), SYS(mParksInCity2), SYS(mThemeList), SYS(mServerList), SYS(mRideList),
        SYS(mUpload), SYS(mF108), SYS(mF10C), SYS(mF120), SYS(mF320), SYS(mUploadActive), SYS(mF374), SYS(mF378), SYS(mF37C), SYS(mF380),
        SYS(mF384), SYS(mF388), SYS(mFTPActive), SYS(mFTPPark), SYS(mF3A8), SYS(mF3C0), SYS(mF3C4),
        {"sys.size", static_cast<int64_t>(sizeof(CFSOnlineSystem))},
        {"list.mF0C", static_cast<int64_t>(offsetof(CFSOnlineSystemParkList_InCity, mF0C))},
        {"list.mList", static_cast<int64_t>(offsetof(CFSOnlineSystemParkList_InCity, mList))},
        {"tl.mCount", static_cast<int64_t>(offsetof(TbPtrList<CFSOnlinePark>, mCount))},
        {"scr.mDownloadGlobe", static_cast<int64_t>(offsetof(CLBScreenSystem, mDownloadGlobe))},
    };
#undef SYS
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return entry.offset;
    return -1;
}

int32_t tpw_cs_layout(int64_t* out) {
    (void)out;
    return 0;
}

// The vtables: 0 the string's, 1 the globe's (a pointer to the table, entries at the slots the members use).
uint64_t tpw_cs_vt(int32_t which) {
    static void* stringTable[64];
    static void* globeTable[64];
    static bool ready = false;
    if (!ready) {
        TbDynamicStringTemplate<char>& (Str::*assign)(const char*) = static_cast<Str& (Str::*)(const char*)>(&Str::operator=);
        stringTable[SlotOf(&assign)] = reinterpret_cast<void*>(&FakeStringAssign);
        void (CLBScreenComponentDownloadGlobe::*slot)() = &CLBScreenComponentDownloadGlobe::Slot50;
        globeTable[SlotOf(&slot)] = reinterpret_cast<void*>(&FakeGlobeSlot50);
        ready = true;
    }
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(which == 0 ? stringTable : globeTable));
}

uint32_t tpw_cs_thunk() { return MakeThunk(reinterpret_cast<void*>(&FakeCallback)); }

uint64_t tpw_cs_fn(int32_t which) {
    void* p = which == 1 ? reinterpret_cast<void*>(&RequestFTPPostLoginCallbackFn)
                         : reinterpret_cast<void*>(&GotoPlayerByIDFTPPostLoginCallbackFn);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

int64_t tpw_cs_lab(uint64_t p) { return lab(reinterpret_cast<const void*>(static_cast<uintptr_t>(p))); }

// ---- the functions
void tpw_cs_enter(uint64_t sys, int32_t index) { reinterpret_cast<CFSOnlineSystem*>(sys)->EnterChatSatellite(index); }
void tpw_cs_setpub(uint64_t sys, int32_t publishing, uint64_t text, uint64_t words) {
    reinterpret_cast<CFSOnlineSystem*>(sys)->SetPublishing(publishing, reinterpret_cast<const char*>(text),
                                                           reinterpret_cast<const uint16_t*>(words));
}
void tpw_cs_upcb(uint64_t sys, int32_t result, uint64_t context) {
    reinterpret_cast<CFSOnlineSystem*>(sys)->RequestUploadPostLoginCallback(result, reinterpret_cast<const void*>(context));
}
void tpw_cs_complete(uint64_t sys, int32_t failed) { reinterpret_cast<CFSOnlineSystem*>(sys)->CompleteUpload(failed); }
void tpw_cs_ftp(uint64_t sys, uint64_t park, uint64_t callback, uint64_t context) {
    reinterpret_cast<CFSOnlineSystem*>(sys)->RequestFTPDownload(*reinterpret_cast<const CFSOnlinePark*>(park),
                                                                reinterpret_cast<void (*)(int32_t, const void*)>(callback),
                                                                reinterpret_cast<const void*>(context));
}
void tpw_cs_ftpid(uint64_t sys, int32_t id, uint64_t callback, uint64_t context) {
    reinterpret_cast<CFSOnlineSystem*>(sys)->GotoPlayerRequestFTPDownloadByID(
        id, reinterpret_cast<void (*)(int32_t, const void*)>(callback), reinterpret_cast<const void*>(context));
}
void tpw_cs_goto(uint64_t sys, int32_t result, uint64_t context, uint64_t park) {
    reinterpret_cast<CFSOnlineSystem*>(sys)->GotoPlayerFTPPostGetParkInfoCallback(
        result, reinterpret_cast<const void*>(context), reinterpret_cast<const CFSOnlinePark*>(park));
}
int32_t tpw_cs_are(uint64_t sys) { return reinterpret_cast<CFSOnlineSystem*>(sys)->AreWeProcessing(); }

}  // extern "C"
