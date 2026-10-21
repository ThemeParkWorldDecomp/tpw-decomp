// Test library for the small CFSOnlineSystem members, the park/server/theme CompleteGetList instances, two std::__tree helpers and two
// status panel members (unit v51123; libtpw_unit_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba.dylib,
// tests/test_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba.py), main app:
//   src/game/online/FS_Online.cpp, src/game/save/FS_MasterSave.cpp, src/game/ui/ui_FeatureStatus.cpp, src/game/ui/ui_bogStatus.cpp
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). The globals of
// other files (gOnlineSystem, gOnlineListCallbackCaller) are defined here. Every object the host side builds lives in static buffers,
// rebuilt by the entry point tpw_v51123_do for each case (the host layout differs from the Mac's: the module passes values, not words).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CFSOnlineSystem* gOnlineSystem;
void (*gOnlineListCallbackCaller)(int32_t callback, int32_t error, int32_t context, void* list);

namespace {
bool gTrackNew = false;
}

// ---- the allocator: the online system's operator new is logged (the Mac object is 0x404 bytes); an object the test labelled is only logged
// when deleted (its storage is static).

// ---- the base destructors of the list instances (declared, defined elsewhere)
template<> CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::~CFSOnlineSystemList() {}
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::~CFSOnlineSystemList() {}
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>::~CFSOnlineSystemList() {}

// ---- the info destructors: one event per destroyed element (the test builds an array of one)
ccParkInfo::~ccParkInfo() { ev("DestroyInfos", {lab(this)}); }
ccServerInfo::~ccServerInfo() { ev("DestroyInfos", {lab(this)}); }
ccThemeInfo::~ccThemeInfo() { ev("DestroyInfos", {lab(this)}); }

// ---- functions the unit's callbacks name (their addresses are compared)
void FSOnlineLoginRASFailureCallback() {}
void GotoPlayerFTPPostGetParkInfoCallbackFn(int32_t, const void*, TbPtrList<CFSOnlinePark>*) {}

// ---- the fakes
void CFSOnlineSystem::CompleteFTPDownload(int32_t failed) { ev("CompleteFTPDownload", {lab(this), failed}); }
void CFSOnlineSystem::LoggedInRequestFTPDownload(const CFSOnlinePark* park) { ev("LoggedInRequestFTPDownload", {lab(this), lab(park)}); }
void CFSOnlineSystem::CompleteLogin(int32_t result) { ev("CompleteLogin", {lab(this), result}); }
void CFSOnlineSystem::InitLogin() { ev("InitLogin", {lab(this)}); }
void CFSOnlineSystem::StopProcessing() { ev("StopProcessing", {lab(this)}); }
CFSOnlineSystem::CFSOnlineSystem() { ev("SystemCtor", {newlab(this)}); }
void CFSOnlineSystemParkList_InWorldByParkID::GetList(int32_t parkID, Callback callback, const void* context) {
    ev("GetList", {lab(this), parkID, callback == &GotoPlayerFTPPostGetParkInfoCallbackFn ? 1 : 0, lab(context)});
}
void CreateOnlineLoginPanel(void (*onFailure)()) { ev("CreateOnlineLoginPanel", {onFailure == &FSOnlineLoginRASFailureCallback ? 1 : 0}); }
void UIStatusPanel::CloseWindow() { ev("CloseWindow", {lab(this)}); }
void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {error, lab(table)}); }

namespace {
unsigned char gSys[sizeof(CFSOnlineSystem) + 64] __attribute__((aligned(16)));
unsigned char gParksBuf[64] __attribute__((aligned(16)));
unsigned char gListBuf[512] __attribute__((aligned(16)));
unsigned char gRecvBuf[64] __attribute__((aligned(16)));
unsigned char gInfosBuf[256] __attribute__((aligned(16)));
unsigned char gRecBuf[3][64] __attribute__((aligned(16)));
unsigned char gPanel[128] __attribute__((aligned(16)));
void* gRecvPtr = nullptr;
void* gInfosPtr = nullptr;
void* gNodePtr = nullptr;
unsigned char gTree[64] __attribute__((aligned(16)));
unsigned char gNode[64] __attribute__((aligned(16)));

void* Record(int64_t k) { return k >= 1 && k <= 3 ? static_cast<void*>(gRecBuf[k - 1]) : nullptr; }

template<class T> T* RemoveFake(TbPtrList<T>* list) {
    ev("RemoveHead", {lab(list)});
    int64_t k = ret("RemoveHead");
    list->mCount -= 1;
    return static_cast<T*>(Record(k));
}
}  // namespace

CFSOnlinePark* ParkRemoveHead(TbPtrList<CFSOnlinePark>* list) { return RemoveFake(list); }
CFSOnlineTheme* ThemeRemoveHead(TbPtrList<CFSOnlineTheme>* list) { return RemoveFake(list); }
CFSOnlineServer* onlinelist2_callee::ServerRemoveHead(TbPtrList<CFSOnlineServer>* list) { return RemoveFake(list); }

namespace {
// The list under test: the base instance with a recording StoreReceivedList (the derived lists' own bodies are other units').
template<class L, class R> struct HostList : CFSOnlineSystemList<L, R> {
    void StoreReceivedList(const R* received) override { ev("StoreReceivedList", {lab(this), lab(received)}); }
};
typedef HostList<TbPtrList<CFSOnlinePark>, ccParkList> ParkHost;
typedef HostList<TbPtrList<CFSOnlineServer>, ccServerList> ServerHost;
typedef HostList<TbPtrList<CFSOnlineTheme>, ccThemeList> ThemeHost;

void FakeCallbackCaller(int32_t callback, int32_t error, int32_t context, void* list) {
    ev("ListCallback", {callback, error, context, lab(list)});
}

template<class H, class R> void Build(const int64_t* p) {
    std::memset(gListBuf, 0, sizeof(gListBuf));
    H* list = new (gListBuf) H();
    list->mF08 = static_cast<int32_t>(p[1]);
    list->mF14 = static_cast<int32_t>(p[2]);
    list->mF18 = static_cast<int32_t>(p[3]);
    list->mF1C = static_cast<int32_t>(p[4]);
    list->mF20 = static_cast<int32_t>(p[5]);
    list->mF24 = static_cast<int32_t>(p[6]);
    list->mF28 = static_cast<int32_t>(p[7]);
    list->mList->mCount = static_cast<int32_t>(p[8]);
    std::memset(gRecvBuf, 0, sizeof(gRecvBuf));
    std::memset(gInfosBuf, 0, sizeof(gInfosBuf));
    reinterpret_cast<std::size_t*>(gInfosBuf + 16)[-1] = 1;   // the array cookie: one element
    // the received list and its info array are heap blocks (the unit frees them with the global operators)
    typedef std::remove_pointer_t<decltype(static_cast<R*>(nullptr)->mInfos)> Info;
    R* received = static_cast<R*>(::operator new(sizeof(R)));
    std::memset(static_cast<void*>(received), 0, sizeof(R));
    gRecvPtr = received;
    gInfosPtr = new Info[1];
    if (p[9] != 0) {
        received->mCount = static_cast<int32_t>(p[10]);
        received->mInfos = p[11] != 0 ? static_cast<Info*>(gInfosPtr) : nullptr;
        received->mF08 = static_cast<int32_t>(p[12]);
        received->mRefCount = static_cast<int32_t>(p[13]);
        list->mReceived = received;
    } else {
        list->mReceived = nullptr;
    }
    std::memset(gSys, 0, sizeof(gSys));
    gOnlineSystem = p[14] != 0 ? reinterpret_cast<CFSOnlineSystem*>(gSys) : nullptr;
}

template<class H, class R> int64_t State(int64_t* out) {
    H* list = reinterpret_cast<H*>(gListBuf);
    R* received = static_cast<R*>(gRecvPtr);
    int n = 0;
    out[n++] = list->mF18;
    out[n++] = list->mF24;
    out[n++] = list->mF28;
    out[n++] = lab(list->mReceived);
    out[n++] = received->mRefCount;
    out[n++] = lab(received->mInfos);
    out[n++] = received->mF08;
    out[n++] = list->mList->mCount;
    out[n++] = lab(gOnlineSystem);
    return n;
}

template<class H> void Call(int32_t error) {
    gParkRecordDeleter = [](CFSOnlinePark* record) { ev("DeleteRecord", {lab(record), 1}); };
    gServerRecordDeleter = [](CFSOnlineServer* record) { ev("DeleteRecord", {lab(record), 1}); };
    gThemeRecordDeleter = [](CFSOnlineTheme* record) { ev("DeleteRecord", {lab(record), 1}); };
    gOnlineListCallbackCaller = FakeCallbackCaller;
    gTrackNew = true;
    reinterpret_cast<H*>(gListBuf)->CompleteGetList(error);
    gTrackNew = false;
}

void BuildSys(int32_t f3a8, int havePark) {
    std::memset(gSys, 0, sizeof(gSys));
    CFSOnlineSystem* sys = reinterpret_cast<CFSOnlineSystem*>(gSys);
    sys->mF3A8 = f3a8;
    sys->mParksByID = havePark != 0 ? reinterpret_cast<CFSOnlineSystemParkList_InWorldByParkID*>(gParksBuf) : nullptr;
}
}  // namespace

extern "C" {

// The host address of a labelled object by the module's name for it.
uint64_t tpw_v51123_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sys", gSys},          {"parks", gParksBuf}, {"list", gListBuf},      {"mlist", nullptr},
        {"recv", gRecvPtr},     {"infos", gInfosPtr},{"rec0", gRecBuf[0]}, {"rec1", gRecBuf[1]},
        {"rec2", gRecBuf[2]},   {"panel", gPanel},    {"tree", gTree},         {"node", gNodePtr},
        {"gOnlineSystem", &gOnlineSystem}, {"treeword", &macstd::gTreeUShortWord},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) {
            if (entry.address == nullptr) return reinterpret_cast<uintptr_t>(reinterpret_cast<ParkHost*>(gListBuf)->mList);
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
        }
    return 0;
}

// The offsets and sizes the module relies on: none (the module passes values, not words).
int32_t tpw_v51123_layout(int64_t* out) {
    (void)out;
    return 0;
}

// One entry point for every operation (op numbers in the module); p are the inputs, out the answers.
int64_t tpw_v51123_do(int32_t op, const int64_t* p, int64_t* out) {
    CFSOnlineSystem* sys = reinterpret_cast<CFSOnlineSystem*>(gSys);
    switch (op) {
    case 0:   // GotoPlayerByIDFTPPostLoginCallback(result, context) with mF3A8 and the by-id list
        BuildSys(static_cast<int32_t>(p[2]), static_cast<int>(p[3]));
        sys->GotoPlayerByIDFTPPostLoginCallback(static_cast<int32_t>(p[0]), reinterpret_cast<const void*>(p[1]));
        return 0;
    case 1:   // LoggedInGotoPlayerRequestFTPDownloadByID
        BuildSys(static_cast<int32_t>(p[0]), static_cast<int>(p[1]));
        sys->LoggedInGotoPlayerRequestFTPDownloadByID();
        return 0;
    case 2:   // RequestFTPPostLoginCallback
        BuildSys(0, 0);
        sys->RequestFTPPostLoginCallback(static_cast<int32_t>(p[0]), reinterpret_cast<const void*>(p[1]));
        return 0;
    case 3: { // the getters and CanWeSendEmail: p = which, child lock, customer id, player id, email, password, name, logged in
        BuildSys(0, 0);
        uint32_t child = static_cast<uint32_t>(p[1]);
        std::memcpy(sys->mF54 + 4, &child, sizeof(child));
        sys->mF38 = static_cast<int32_t>(p[2]);
        sys->mF34 = static_cast<int32_t>(p[3]);
        sys->mLoginEmail.mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(p[4]));
        sys->mLoginPassword.mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(p[5]));
        sys->mLoginName.mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(p[6]));
        sys->mLoggedIn = static_cast<int32_t>(p[7]);
        switch (p[0]) {
        case 0: return sys->GetChildLock();
        case 1: return static_cast<uint32_t>(sys->GetLoginCustomerID());
        case 2: return static_cast<uint32_t>(sys->GetLoginPlayerID());
        case 3: return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sys->GetLoginEmailAddress()));
        case 4: return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sys->GetLoginPassword()));
        case 5: return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sys->GetLoginCustomerName()));
        default: return static_cast<uint32_t>(sys->CanWeSendEmail());
        }
    }
    case 4:
        BuildSys(0, 0);
        sys->Logout();
        return 0;
    case 5:
        BuildSys(0, 0);
        sys->LoginRASCancelled();
        return 0;
    case 6:   // Login(callback, context): out = the stored callback and context words
        BuildSys(0, 0);
        sys->Login(reinterpret_cast<void (*)(int32_t, const void*)>(static_cast<uintptr_t>(p[0])),
                   reinterpret_cast<const void*>(static_cast<uintptr_t>(p[1])));
        out[0] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sys->mLoginCallback));
        out[1] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sys->mLoginContext));
        return 2;
    case 7:   // build the list of kind p[0] (0 park, 1 server, 2 theme)
        if (p[0] == 0) Build<ParkHost, ccParkList>(p);
        else if (p[0] == 1) Build<ServerHost, ccServerList>(p);
        else Build<ThemeHost, ccThemeList>(p);
        return 0;
    case 8:   // CompleteGetList(p[1]) of the kind p[0]
        if (p[0] == 0) Call<ParkHost>(static_cast<int32_t>(p[1]));
        else if (p[0] == 1) Call<ServerHost>(static_cast<int32_t>(p[1]));
        else Call<ThemeHost>(static_cast<int32_t>(p[1]));
        return 0;
    case 9:   // the list's state after the call
        if (p[0] == 0) return State<ParkHost, ccParkList>(out);
        if (p[0] == 1) return State<ServerHost, ccServerList>(out);
        return State<ThemeHost, ccThemeList>(out);
    case 10: {   // the tree constructor: p = three words of garbage, the global word; out = the three words (the third as an offset)
        macstd::gTreeUShortWord = reinterpret_cast<void*>(static_cast<uintptr_t>(p[3]));
        void** words = reinterpret_cast<void**>(gTree);
        for (int i = 0; i < 3; i++) words[i] = reinterpret_cast<void*>(static_cast<uintptr_t>(p[i]));
        macstd::LessUShort less;
        macstd::AllocUShort alloc;
        macstd::TreeUShort* tree = new (gTree) macstd::TreeUShort(less, alloc);
        out[0] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tree->mF00));
        out[1] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tree->mF04));
        out[2] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(tree->mF08)) - static_cast<int64_t>(reinterpret_cast<uintptr_t>(gTree));
        return 3;
    }
    case 11: {   // the node allocator's deallocate
        macstd::AllocUShortNode allocator;
        allocator.deallocate(gNodePtr, static_cast<unsigned long>(p[0]));
        return 0;
    }
    case 14:   // a heap node for deallocate, made before the labels are given
        gNodePtr = ::operator new(16);
        return 0;
    case 12:
        reinterpret_cast<BogStatusPanel*>(gPanel)->Remove();
        return 0;
    case 13:
        reinterpret_cast<FeatureStatusPanel*>(gPanel)->InitViewWindow();
        return 0;
    }
    return -1;
}

}  // extern "C"
