// Unit v51110: the server and ride lists' small members, their destructors and LoggedInGetList, the FS_OnlineList static initialiser and
// three one-liners (CFSOnlineRide::GetFilePath, CFSOnlineSystem::GetAvatarID, UI_ByeByeBankrupt).
#include <new>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"
#include "ui_mainmenu.h"

// ---- the callees of the destructors: weak, so that a test library can replace them with fakes
namespace onlinelist2_callee {
__attribute__((weak, noinline)) CFSOnlineRide* RideRemoveHead(TbPtrList<CFSOnlineRide>* list) { return list->RemoveHead(); }
__attribute__((weak, noinline)) CFSOnlineServer* ServerRemoveHead(TbPtrList<CFSOnlineServer>* list) { return list->RemoveHead(); }
__attribute__((weak, noinline)) void DestroyRide(CFSOnlineRide* ride) { delete ride; }
__attribute__((weak, noinline)) void DestroyServer(CFSOnlineServer* server) { delete server; }
__attribute__((weak, noinline)) void DestroyRideList(TbPtrList<CFSOnlineRide>* list) { delete list; }
__attribute__((weak, noinline)) void DestroyServerList(TbPtrList<CFSOnlineServer>* list) { delete list; }
__attribute__((weak, noinline)) void DestroyRideInfos(ccRideInfo* infos) { delete[] infos; }
__attribute__((weak, noinline)) void DestroyServerInfos(ccServerInfo* infos) { delete[] infos; }
__attribute__((weak, noinline)) void FreeBlock(void* block) { ::operator delete(block); }
}  // namespace onlinelist2_callee

// ---- the server list's search members
namespace ftpdl_callee {
// 0x1017b0b4 (52): the last server of the list whose word at 0xc is 1 (no early exit), or NULL.
CFSOnlineServer* ReturnFTPDownloadServer(CFSOnlineSystemServerList* list) {
    CFSOnlineServer* found = nullptr;
    TbPtrList<CFSOnlineServer>::CNode* node = list->mList->mHead;
    while (node != nullptr) {
        CFSOnlineServer* server = node->mData;
        node = node->mNext;
        if (server->mF0C == 1) found = server;
    }
    return found;
}
}  // namespace ftpdl_callee

namespace ftpreq_callee {
// 0x1017b134 (52): the same for the word 0x64.
CFSOnlineServer* ReturnUploadServer(CFSOnlineSystemServerList* list) {
    CFSOnlineServer* found = nullptr;
    TbPtrList<CFSOnlineServer>::CNode* node = list->mList->mHead;
    while (node != nullptr) {
        CFSOnlineServer* server = node->mData;
        node = node->mNext;
        if (server->mF0C == 0x64) found = server;
    }
    return found;
}

// 0x1017bacc (64): the last ride whose words at 0x44 and 0x64 equal the two numbers (signed compares), or NULL.
CFSOnlineRide* ReturnRideInfo(CFSOnlineSystemRideList* list, int32_t theme, int32_t index) {
    CFSOnlineRide* found = nullptr;
    TbPtrList<CFSOnlineRide>::CNode* node = list->mList->mHead;
    while (node != nullptr) {
        CFSOnlineRide* ride = node->mData;
        node = node->mNext;
        if (theme == ride->mF44 && index == ride->mF64) found = ride;
    }
    return found;
}

// 0x10176c90 (40): a copy of the wide string the ride keeps at 0x54 (the data pointer of mText4C), returned by value.
TbDynamicStringTemplate<char16_t> GetFilePath(const CFSOnlineRide* ride) {
    return TbDynamicStringTemplate<char16_t>(ride->mText4C.mData);
}
}  // namespace ftpreq_callee

// ---- the destructors. The server list's copy (0x1017a6e4) and the ride list's (0x1017b288) of the base destructor; the derived
// destructors 0x1017a908 and 0x1017b4a4 are byte for byte the same code, so they are empty and run the base's.
// Order: the stored answer is released, the records are removed and destroyed, the two words cleared, the list freed.
template<>
CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::~CFSOnlineSystemList() {
    ccServerList* received = mReceived;
    if (received != nullptr) {
        received->mRefCount = received->mRefCount - 1;
        if (received->mRefCount == 0) {
            if (received->mInfos != nullptr) onlinelist2_callee::DestroyServerInfos(received->mInfos);
            received->mInfos = nullptr;
            onlinelist2_callee::FreeBlock(received);
        }
        mReceived = nullptr;
    }
    while (mList->mCount != 0) {
        CFSOnlineServer* server = onlinelist2_callee::ServerRemoveHead(mList);
        if (server != nullptr) onlinelist2_callee::DestroyServer(server);
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) onlinelist2_callee::DestroyServerList(mList);
}

template<>
CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::~CFSOnlineSystemList() {
    ccRideList* received = mReceived;
    if (received != nullptr) {
        received->mRefCount = received->mRefCount - 1;
        if (received->mRefCount == 0) {
            if (received->mInfos != nullptr) onlinelist2_callee::DestroyRideInfos(received->mInfos);
            received->mInfos = nullptr;
            onlinelist2_callee::FreeBlock(received);
        }
        mReceived = nullptr;
    }
    while (mList->mCount != 0) {
        CFSOnlineRide* ride = onlinelist2_callee::RideRemoveHead(mList);
        if (ride != nullptr) onlinelist2_callee::DestroyRide(ride);
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) onlinelist2_callee::DestroyRideList(mList);
}

CFSOnlineSystemServerList::~CFSOnlineSystemServerList() {}   // 0x1017a908: the base's work, inline on the Mac
CFSOnlineSystemRideList::~CFSOnlineSystemRideList() {}       // 0x1017b4a4

// ---- LoggedInGetList (0x1017ab30 server, 0x1017b6c8 ride; the same code as the city instance)
namespace {
template<class LIST>
void LoggedInGetListBody(LIST* self) {
    if (self->ChildLocked() == 0) {
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
        City* city = gOnlineSystem->ReturnConnectionToCityServer();
        if (city != nullptr) {
            self->mF0C = 1;
            self->mRequestID = self->RequestListFromServer(city, static_cast<uint32_t>(self->mF24));
            return;
        }
        self->mF18 = 0x4f;
    } else {
        self->mF18 = 0x4e;
    }
    // The failure: the same tail twice in the original.
    if (self->mF18 != 0) {
        AddUIError(self->mF18, nullptr);
        self->mF18 = 0;
    }
    int32_t callback = self->mF1C;
    int32_t context = self->mF20;
    if (self->mF08 != 0) CFSOnlineSystem::Instance()->Logout();
    if (callback != 0) gOnlineListCallbackCaller(callback, 1, context, self->mList);
}
}  // namespace

template<>
void CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::LoggedInGetList() { LoggedInGetListBody(this); }

template<>
void CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::LoggedInGetList() { LoggedInGetListBody(this); }

// ---- __sinit_FS_OnlineList_cpp (0x1017ddb4): the two wide string globals
OnlineWideStringGlobal gOnlineListString1;   // 0x10380b14
OnlineWideStringGlobal gOnlineListString2;   // 0x10380af4
uint8_t gOnlineListNode1[16];                // 0x10380b28
uint8_t gOnlineListNode2[16];                // 0x10380b08

static const char16_t kLiteral1[] = u"upload";    // 0x1024d840
static const char16_t kLiteral2[] = u"gms.dat";    // 0x1024d84e

void sinit_FS_OnlineList_cpp() {
    new (&gOnlineListString1.mString) TbDynamicStringTemplate<char16_t>(kLiteral1);
    __register_global_object(&gOnlineListString1, reinterpret_cast<void*>(&UIMain_DestroyWString), gOnlineListNode1);
    new (&gOnlineListString2.mString) TbDynamicStringTemplate<char16_t>(kLiteral2);
    __register_global_object(&gOnlineListString2, reinterpret_cast<void*>(&UIMain_DestroyWString), gOnlineListNode2);
}

// ---- the one-liners
uint32_t CFSOnlineSystem::GetAvatarID() {   // 0x101719bc (8)
    uint32_t value;
    __builtin_memcpy(&value, mF54 + 8, sizeof(value));
    return value;
}

void UI_ByeByeBankrupt() {}   // 0x10187220 (4): blr
