// Unit v51123: small CFSOnlineSystem members (the FTP post-login callbacks, the login getters, Login, Logout), the park, server and theme
// instances of CFSOnlineSystemList::CompleteGetList, two std::__tree helpers and two status panel members.
#include <new>
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "LbTime.h"
#include "ui_main.h"
#include "platform/samsutils.h"

// ---- the post-login callbacks of the FTP download jobs
// 0x10175f24: on an error the download is completed as failed, else the park download starts (no park).
void CFSOnlineSystem::RequestFTPPostLoginCallback(int32_t result, const void* context) {
    (void)context;
    if (result != 0) {
        CompleteFTPDownload(1);
    } else {
        LoggedInRequestFTPDownload(nullptr);
    }
}

// 0x10176180: the same for the go-to-player download.
void CFSOnlineSystem::GotoPlayerByIDFTPPostLoginCallback(int32_t result, const void* context) {
    (void)context;
    if (result != 0) {
        CompleteFTPDownload(1);
    } else {
        LoggedInGotoPlayerRequestFTPDownloadByID();
    }
}

// 0x10176390: asks the by-park-id list for the park at mF3A8; the callback is GotoPlayerFTPPostGetParkInfoCallbackFn (TVector 0x10267450).
void CFSOnlineSystem::LoggedInGotoPlayerRequestFTPDownloadByID() {
    mParksByID->GetList(mF3A8, GotoPlayerFTPPostGetParkInfoCallbackFn, nullptr);
}

// ---- the getters (the strings' data words are at the string + 8)
const char16_t* CFSOnlineSystem::GetLoginCustomerName() { return mLoginName.mData; }      // 0x10171834: word 0x14
const char16_t* CFSOnlineSystem::GetLoginPassword() { return mLoginPassword.mData; }      // 0x10171878: word 0x28
const char16_t* CFSOnlineSystem::GetLoginEmailAddress() { return mLoginEmail.mData; }     // 0x101718b8: word 0x44
int32_t CFSOnlineSystem::GetLoginPlayerID() { return mF34; }                              // 0x101718fc
int32_t CFSOnlineSystem::GetLoginCustomerID() { return mF38; }                            // 0x1017193c
uint32_t CFSOnlineSystem::GetChildLock() {                                                // 0x10171980: the word at 0x58
    uint32_t word;
    __builtin_memcpy(&word, mF54 + 4, sizeof(word));
    return word;
}

// 0x101717cc: always when not logged in; else when bit 5 (0x20) of the child lock word is set.
int32_t CFSOnlineSystem::CanWeSendEmail() {
    if (mLoggedIn == 0) return 1;
    return (GetChildLock() & 0x20) != 0 ? 1 : 0;
}

// 0x10171398
void CFSOnlineSystem::Logout() {
    InitLogin();
    StopProcessing();
}

// 0x10171114
void CFSOnlineSystem::LoginRASCancelled() { CompleteLogin(1); }

// 0x101710ac: keeps the caller's callback and context, then raises the login panel (failure callback TVector 0x102674b0).
void CFSOnlineSystem::Login(void (*callback)(int32_t, const void*), const void* context) {
    mLoginCallback = reinterpret_cast<OnlineLoginCallback>(callback);
    mLoginContext = const_cast<void*>(context);
    CreateOnlineLoginPanel(FSOnlineLoginRASFailureCallback);
}

// ---- CompleteGetList of the park, server and theme lists (0x101741d4, 0x1017464c, 0x1017488c): the ride instance's code with their
// own record type, deleter and removal.
__attribute__((weak, noinline)) CFSOnlinePark* ParkRemoveHead(TbPtrList<CFSOnlinePark>* list) { return list->RemoveHead(); }
__attribute__((weak, noinline)) CFSOnlineTheme* ThemeRemoveHead(TbPtrList<CFSOnlineTheme>* list) { return list->RemoveHead(); }
using onlinelist2_callee::ServerRemoveHead;

void (*gParkRecordDeleter)(CFSOnlinePark* park) = [](CFSOnlinePark* park) { delete park; };
void (*gServerRecordDeleter)(CFSOnlineServer* server) = [](CFSOnlineServer* server) { delete server; };
void (*gThemeRecordDeleter)(CFSOnlineTheme* theme) = [](CFSOnlineTheme* theme) { delete theme; };

typedef CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList> PARKBASE;
typedef CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList> SERVERBASE;
typedef CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList> THEMEBASE;

#define TPW_COMPLETE_GET_LIST(LISTTYPE, RECTYPE, RECEIVEDTYPE, REMOVEHEAD, DELETER)                              \
    template<> void LISTTYPE::CompleteGetList(int32_t error) {                                                    \
        bool hadError = mF18 != 0;                                                                                \
        if (hadError) {                                                                                           \
            AddUIError(mF18, nullptr);                                                                            \
            mF18 = 0;                                                                                             \
        }                                                                                                         \
        int32_t callback = mF1C;                                                                                  \
        int32_t context = mF20;                                                                                   \
        if (error == 0 && !hadError && mF14 != 0) {                                                               \
            while (mList->GetCount() != 0) {                                                                      \
                RECTYPE* record = REMOVEHEAD(mList);                                                              \
                if (record != nullptr) DELETER(record);                                                           \
            }                                                                                                     \
            mF24 = 0;                                                                                             \
            mF28 = 0;                                                                                             \
            mF28 = 1;                                                                                             \
            if (mReceived != nullptr) {                                                                           \
                StoreReceivedList(mReceived);                                                                     \
                mF24 = mReceived->mF08;                                                                           \
                RECEIVEDTYPE* received = mReceived;                                                               \
                if (received != nullptr) {                                                                        \
                    --received->mRefCount;                                                                        \
                    if (received->mRefCount == 0) {                                                               \
                        if (received->mInfos != nullptr) delete[] received->mInfos;                               \
                        received->mInfos = nullptr;                                                               \
                        ::operator delete(static_cast<void*>(received));                                          \
                    }                                                                                             \
                    mReceived = nullptr;                                                                          \
                }                                                                                                 \
            }                                                                                                     \
        }                                                                                                         \
        int32_t result = 0;                                                                                       \
        if (error != 0 || hadError || mF28 == 0) {                                                                \
            if (mF08 != 0) {                                                                                      \
                if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();                              \
                gOnlineSystem->Logout();                                                                          \
            }                                                                                                     \
            result = 1;                                                                                           \
        }                                                                                                         \
        if (callback != 0) gOnlineListCallbackCaller(callback, result, context, mList);                           \
    }

#define TPW_CALL_DELETER_PARK(record) gParkRecordDeleter(record)
#define TPW_CALL_DELETER_SERVER(record) gServerRecordDeleter(record)
#define TPW_CALL_DELETER_THEME(record) gThemeRecordDeleter(record)

TPW_COMPLETE_GET_LIST(PARKBASE, CFSOnlinePark, ccParkList, ParkRemoveHead, TPW_CALL_DELETER_PARK)
TPW_COMPLETE_GET_LIST(SERVERBASE, CFSOnlineServer, ccServerList, ServerRemoveHead, TPW_CALL_DELETER_SERVER)
TPW_COMPLETE_GET_LIST(THEMEBASE, CFSOnlineTheme, ccThemeList, ThemeRemoveHead, TPW_CALL_DELETER_THEME)

// ---- std::__tree<unsigned short> helpers
// 0x1016eaf0: the header words: a copy of the global word, a cleared word, and a pointer to the cleared word (the comparator byte the Mac
// reads and stores at 8 is overwritten at once).
namespace macstd {
void* gTreeUShortWord = nullptr;   // 0x102a722c (original name unknown)
TreeUShort::TreeUShort(const LessUShort& less, const AllocUShort& alloc) {
    (void)less;
    (void)alloc;
    mF00 = gTreeUShortWord;
    mF04 = nullptr;
    mF08 = &mF04;
}
// 0x1016e8b0: gives the node back (the count is unused).
void AllocUShortNode::deallocate(void* node, unsigned long count) {
    (void)count;
    ::operator delete(node);
}
}  // namespace macstd

// ---- the status panels
// 0x1019ad1c
void BogStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }
// 0x1019e2d4: nothing.
void FeatureStatusPanel::InitViewWindow() {}
