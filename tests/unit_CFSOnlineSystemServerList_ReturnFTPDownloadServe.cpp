// Test library for the server and ride lists' destructors, search members and LoggedInGetList (unit v51110;
// libtpw_unit_CFSOnlineSystemServerList_ReturnFTPDownloadServe.dylib, tests/test_CFSOnlineSystemServerList_ReturnFTPDownloadServe.py), main app:
//   src/CFSOnlineSystemServerList_RequestListFromServer_CFSOnlineSystemServerList_ReturnFTPDownloadServe.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues: the list's RemoveHead, the
// destructors of records and lists, __destroy_new_array, operator delete, the online system (constructor, Instance, Logout, city
// connection), AddUIError, the list callback, the string constructor and __register_global_object. The globals of other files
// (gOnlineSystem, gOnlineListCallbackCaller) are defined here. Host objects are raw arena blocks (no vtable) except the two Live classes,
// which carry the virtual slots LoggedInGetList calls.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "ui_mainmenu.h"

using namespace calllog;

// ---- the string constructor, faked (it is a glue import on the Mac)
template<>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("StringCtor", {lab(this)}, {narrow(text).c_str()});
}

// ---- globals of other files
CFSOnlineSystem* gOnlineSystem = nullptr;
void (*gOnlineListCallbackCaller)(int32_t callback, int32_t error, int32_t context, void* list) = nullptr;

namespace {
void CallbackFake(int32_t callback, int32_t error, int32_t context, void* list) {
    ev("Callback", {static_cast<int64_t>(static_cast<uint32_t>(callback)), error, context, lab(list)});
}
struct Init {
    Init() { gOnlineListCallbackCaller = CallbackFake; }
} gInit;

void* gItems[8];

using ServerBase = CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>;
using RideBase = CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>;
using ServerNode = TbTList<CFSOnlineServer*, CFSOnlineServer*>;

// The two instances share one layout; the tests treat both through the server one.
ServerBase* B(uint64_t o) { return reinterpret_cast<ServerBase*>(o); }
void*& ListPtr(uint64_t o) { return *reinterpret_cast<void**>(&B(o)->mList); }
ServerNode* L(uint64_t o) { return static_cast<ServerNode*>(ListPtr(o)); }

struct LiveServer : ServerBase {
    int32_t ChildLocked() override {
        ev("ChildLocked", {lab(this)});
        return static_cast<int32_t>(ret("ChildLocked"));
    }
    int32_t RequestListFromServer(City* city, uint32_t arg) override {
        ev("Request", {lab(this), lab(static_cast<void*>(city)), arg});
        return static_cast<int32_t>(ret("Request"));
    }
};
struct LiveRide : RideBase {
    int32_t ChildLocked() override {
        ev("ChildLocked", {lab(this)});
        return static_cast<int32_t>(ret("ChildLocked"));
    }
    int32_t RequestListFromServer(City* city, uint32_t arg) override {
        ev("Request", {lab(this), lab(static_cast<void*>(city)), arg});
        return static_cast<int32_t>(ret("Request"));
    }
};

void* Pop(void* list) {
    ev("RemoveHead", {lab(list)});
    int64_t index = ret("RemoveHead");
    static_cast<ServerNode*>(list)->mCount--;
    return index != 0 ? gItems[index] : nullptr;
}
}  // namespace

// ---- the fakes of the unit's callees
CFSOnlineRide::~CFSOnlineRide() {}
CFSOnlineServer::~CFSOnlineServer() {}
ccRideInfo::~ccRideInfo() {}
ccServerInfo::~ccServerInfo() {}

CFSOnlineSystem::CFSOnlineSystem() { ev("SystemCtor", {newlab(this)}); }
void CFSOnlineSystem::Logout() { ev("Logout", {lab(this)}); }
CFSOnlineSystem* CFSOnlineSystem::Instance() {
    ev("Instance", {});
    return gOnlineSystem;
}
City* CFSOnlineSystem::ReturnConnectionToCityServer() {
    ev("CityConn", {lab(this)});
    return reinterpret_cast<City*>(static_cast<intptr_t>(ret("CityConn")));
}
void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {error, lab(static_cast<void*>(table))}); }

// only the vtable of the faked string refers to these
template<> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }
void UIMain_DestroyWString(void*) {}   // the destructor glue, only its address is passed on

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    (void)destructor;
    ev("Register", {lab(object), lab(node)});
}

namespace onlinelist2_callee {
CFSOnlineRide* RideRemoveHead(TbPtrList<CFSOnlineRide>* list) { return static_cast<CFSOnlineRide*>(Pop(list)); }
CFSOnlineServer* ServerRemoveHead(TbPtrList<CFSOnlineServer>* list) { return static_cast<CFSOnlineServer*>(Pop(list)); }
void DestroyRide(CFSOnlineRide* ride) { ev("DestroyItem", {lab(ride), 1}); }
void DestroyServer(CFSOnlineServer* server) { ev("DestroyItem", {lab(server), 1}); }
void DestroyRideList(TbPtrList<CFSOnlineRide>* list) { ev("DestroyList", {lab(list), 1}); }
void DestroyServerList(TbPtrList<CFSOnlineServer>* list) { ev("DestroyList", {lab(list), 1}); }
void DestroyRideInfos(ccRideInfo* infos) { ev("DestroyArray", {lab(infos), 0x10267488}); }
void DestroyServerInfos(ccServerInfo* infos) { ev("DestroyArray", {lab(infos), 0x10267490}); }
void FreeBlock(void* block) { ev("Delete", {lab(block)}); }
}  // namespace onlinelist2_callee

extern "C" {

uint64_t tpw_svl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"str1", &gOnlineListString1},
        {"str2", &gOnlineListString2},
        {"node1", gOnlineListNode1},
        {"node2", gOnlineListNode2},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_svl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_svl_layout(int64_t* out) {
    int32_t n = 0;
    CFSOnlineSystem* sys = static_cast<CFSOnlineSystem*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystem)));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(sys->mF54 + 8) - reinterpret_cast<char*>(sys));
    return n;
}

// ---- the world
// kind 0 is the server list, 1 the ride list; live makes an object with the virtual slots (its constructor makes the TbPtrList).
uint64_t tpw_svl_new_list(int32_t kind, int32_t live) {
    if (live != 0) {
        if (kind == 0) return reinterpret_cast<uintptr_t>(new (tpw::ArenaAllocZeroed(sizeof(LiveServer))) LiveServer());
        return reinterpret_cast<uintptr_t>(new (tpw::ArenaAllocZeroed(sizeof(LiveRide))) LiveRide());
    }
    size_t size = kind == 0 ? sizeof(CFSOnlineSystemServerList) : sizeof(CFSOnlineSystemRideList);
    uint64_t o = reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size));
    ListPtr(o) = tpw::ArenaAllocZeroed(sizeof(ServerNode));
    return o;
}
uint64_t tpw_svl_blk(uint64_t o) { return reinterpret_cast<uintptr_t>(ListPtr(o)); }
void tpw_svl_set_fields(uint64_t o, const int32_t* w) {
    ServerBase* b = B(o);
    b->mRequestID = w[0];
    b->mF08 = w[1];
    b->mF0C = w[2];
    b->mF10 = w[3];
    b->mF14 = w[4];
    b->mF18 = w[5];
    b->mF1C = w[6];
    b->mF20 = w[7];
    b->mF24 = w[8];
    b->mF28 = w[9];
}
void tpw_svl_get_fields(uint64_t o, int32_t* w) {
    ServerBase* b = B(o);
    w[0] = b->mRequestID;
    w[1] = b->mF08;
    w[2] = b->mF0C;
    w[3] = b->mF10;
    w[4] = b->mF14;
    w[5] = b->mF18;
    w[6] = b->mF1C;
    w[7] = b->mF20;
    w[8] = b->mF24;
    w[9] = b->mF28;
}
void tpw_svl_set_count(uint64_t o, int32_t n) { L(o)->mCount = n; }
int32_t tpw_svl_get_count(uint64_t o) { return L(o)->mCount; }

void tpw_svl_set_nodes(uint64_t o, int32_t n, const uint64_t* items) {
    ServerNode* list = L(o);
    ServerNode::CNode* prev = nullptr;
    list->mHead = nullptr;
    for (int32_t i = 0; i < n; i++) {
        ServerNode::CNode* node = static_cast<ServerNode::CNode*>(tpw::ArenaAllocZeroed(sizeof(ServerNode::CNode)));
        node->mData = reinterpret_cast<CFSOnlineServer*>(static_cast<uintptr_t>(items[i]));
        node->mNext = nullptr;
        node->mPrev = prev;
        if (prev != nullptr) prev->mNext = node;
        else list->mHead = node;
        prev = node;
    }
    list->mTail = prev;
    list->mCount = n;
}

uint64_t tpw_svl_item(int32_t kind) {
    return reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(kind == 0 ? sizeof(CFSOnlineServer) : sizeof(CFSOnlineRide)));
}
void tpw_svl_set_slot(int32_t index, uint64_t item) { gItems[index] = reinterpret_cast<void*>(static_cast<uintptr_t>(item)); }
void tpw_svl_item_set(int32_t kind, uint64_t item, int32_t a, int32_t b) {
    if (kind == 0) {
        reinterpret_cast<CFSOnlineServer*>(item)->mF0C = a;
    } else {
        reinterpret_cast<CFSOnlineRide*>(item)->mF44 = a;
        reinterpret_cast<CFSOnlineRide*>(item)->mF64 = b;
    }
}
void tpw_svl_item_text(uint64_t item, uint64_t text) {
    CFSOnlineRide* ride = reinterpret_cast<CFSOnlineRide*>(item);
    const char16_t* pointer = reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(text));
    std::memcpy(&ride->mText4C.mData, &pointer, sizeof(pointer));
}

void tpw_svl_set_recv(uint64_t o, uint64_t recv, uint64_t infos, int32_t has, int32_t hasInfos, int32_t ref) {
    ccServerList* r = reinterpret_cast<ccServerList*>(recv);
    r->mCount = 0;
    r->mInfos = hasInfos != 0 ? reinterpret_cast<ccServerInfo*>(infos) : nullptr;
    r->mF08 = 0;
    r->mRefCount = ref;
    B(o)->mReceived = has != 0 ? r : nullptr;
}
void tpw_svl_get_recv(uint64_t o, uint64_t recv, int32_t* out) {
    ccServerList* r = reinterpret_cast<ccServerList*>(recv);
    out[0] = B(o)->mReceived != nullptr ? 1 : 0;
    out[1] = r->mRefCount;
    out[2] = r->mInfos != nullptr ? 1 : 0;
}
void tpw_svl_set_sys(uint64_t sys) { gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(sys); }
uint64_t tpw_svl_get_sys() { return reinterpret_cast<uintptr_t>(gOnlineSystem); }

// ---- the functions
uint64_t tpw_svl_ftp(uint64_t o) { return reinterpret_cast<uintptr_t>(ftpdl_callee::ReturnFTPDownloadServer(reinterpret_cast<CFSOnlineSystemServerList*>(o))); }
uint64_t tpw_svl_upload(uint64_t o) { return reinterpret_cast<uintptr_t>(ftpreq_callee::ReturnUploadServer(reinterpret_cast<CFSOnlineSystemServerList*>(o))); }
uint64_t tpw_svl_rideinfo(uint64_t o, int32_t theme, int32_t index) {
    return reinterpret_cast<uintptr_t>(ftpreq_callee::ReturnRideInfo(reinterpret_cast<CFSOnlineSystemRideList*>(o), theme, index));
}

// variant 0 the server list's destructor, 1 the ride list's, 2 the ride base's; a positive flag (16 bit, signed) also frees the object
// (the compiler's deleting destructor).
void tpw_svl_dtor(int32_t variant, uint64_t o, int32_t flag) {
    if (variant == 0) reinterpret_cast<CFSOnlineSystemServerList*>(o)->CFSOnlineSystemServerList::~CFSOnlineSystemServerList();
    else if (variant == 1) reinterpret_cast<CFSOnlineSystemRideList*>(o)->CFSOnlineSystemRideList::~CFSOnlineSystemRideList();
    else reinterpret_cast<RideBase*>(o)->RideBase::~RideBase();
    if (static_cast<int16_t>(flag) > 0) onlinelist2_callee::FreeBlock(reinterpret_cast<void*>(static_cast<uintptr_t>(o)));
}
void tpw_svl_login(int32_t kind, uint64_t o) {
    if (kind == 0) static_cast<ServerBase*>(reinterpret_cast<LiveServer*>(o))->LoggedInGetList();
    else static_cast<RideBase*>(reinterpret_cast<LiveRide*>(o))->LoggedInGetList();
}

void tpw_svl_filepath(uint64_t ride) {
    alignas(16) static char buffer[sizeof(TbDynamicStringTemplate<char16_t>) + 16];
    new (buffer) TbDynamicStringTemplate<char16_t>(ftpreq_callee::GetFilePath(reinterpret_cast<const CFSOnlineRide*>(ride)));
}
uint32_t tpw_svl_avatar(uint32_t value) {
    CFSOnlineSystem* sys = static_cast<CFSOnlineSystem*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystem)));
    std::memcpy(sys->mF54 + 8, &value, sizeof(value));
    return sys->GetAvatarID();
}
void tpw_svl_bankrupt() { UI_ByeByeBankrupt(); }
void tpw_svl_sinit() { sinit_FS_OnlineList_cpp(); }

}  // extern "C"
