// FS_OnlineList.cpp: name proven by __sinit_FS_OnlineList_cpp in the main application.

#define TPW_SERVERLIST_STRONG 1
#define TPW_V51039_STRONG
#define TPW_V5532_STRONG 1
#define TPW_V5718_STRONG
#define TPW_V5762_STRONG
#define TPW_V5928_STRONG
#include "Advisor.h"
#include "analyser.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "helpers/FS_OnlineHelpers.h"
#include "helpers/FS_OnlineListHelpers.h"
#include "HLSprite.h"
#include "InterfaceFontWrapper.h"
#include "LbTime.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "Postcard.h"
#include "Serialise.h"
#include "std_basic_streambuf_wchar.h"
#include "tb_ddraw.h"
#include "tb_resource_texture.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbResource.h"
#include "TbTimeDiff.h"
#include "TbTList.h"
#include "Text.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_Postcard.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <new>

// Unit v51065: the online lists' small members, the park lists' GetList for the top ten and the by-park-id lists, the login callbacks,
// two destructors (the city list's and the theme list's base), the park instance of LoggedInGetList and the FTP download getters.

// The lists' RemoveHead instances (0x10177e2c city, 0x10177904 theme) are callees outside the unit: explicit specializations
// declared here, not in FS_OnlineList.h, so only this file calls them out of line (src/v5_twins.cpp and src/std_exception_what.cpp
// define them; recording fakes in the test library). In the header they turned every includer's inlined RemoveHead into an
// external call, which the other units' test libraries could not link.
template<> CFSOnlineCity* TbTList<CFSOnlineCity*, CFSOnlineCity*>::RemoveHead();
template<> CFSOnlineTheme* TbTList<CFSOnlineTheme*, CFSOnlineTheme*>::RemoveHead();

// 0x10178b3c (52): the last city of the list whose id (Mac 0x08) is `city`, or NULL. The walk does not stop at a match.
namespace park_info_callee {
CFSOnlineCity* ReturnCityInfo(CFSOnlineSystemCityList* list, int32_t city) {
    CFSOnlineCity* result = nullptr;
    POSITION position = list->mList->GetHeadPosition();
    while (position != nullptr) {
        CFSOnlineCity* record = list->mList->GetNext(position);
        if (city == record->mCityID) result = record;
    }
    return result;
}
}  // namespace park_info_callee

// 0x101783a4 (60): the same for the city list.
void PostLoginCallbackFn_City(int32_t error, const void* context) {
    CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>* list =
        const_cast<CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>*>(
            static_cast<const CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>*>(context));
    if (error != 0) {
        list->CompleteGetList(1);
    } else {
        list->LoggedInGetList();
    }
}

// The FTP download job of the online system: its end (CompleteFTPDownload), the download client's callback and the request of the next
// file, and the accessor of the one system object (unit v5655). Companion of src/FS_Online_init.cpp.

// 0x10178618. The one online system, made on first use.
CFSOnlineSystem* CFSOnlineSystem::Instance() {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    return gOnlineSystem;
}

// Unit v5961: seven small members of the online system.
//   UploadPostLoginCallbackFn 0x10174f2c, GotoPlayerFTPPostGetParkInfoCallbackFn 0x1017620c, CFSOnlineFTPControl::CallbackFn 0x1017769c:
//     each makes the online system when there is none (operator new(0x404) and the constructor) and forwards its arguments.
//   CFSOnlineSystem::ReturnConnectionToCityServer 0x1017346c, CFSOnlineSystemCityList::GetList 0x10178718,
//   ccThemeInfo::~ccThemeInfo 0x101732e4, ccRideInfo::~ccRideInfo 0x10173110.

namespace {

// A callback or context the Mac keeps in a 32-bit word.
int32_t PointerWord(const void* p) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// 0x10178718: starts a city list request. The request words are reset, the callback and its context kept (32-bit words), the stored
// answer released (its count drops; at zero the infos are destroyed and freed and the answer freed), then the post login step runs at
// once when logged in, or after the login.
void CFSOnlineSystemCityList::GetList(OnlineListCallback<TbPtrList<CFSOnlineCity>>::Type callback, const void* context) {
    mRequestID = -1;
    mF08 = 0;
    mF0C = 0;
    mF10 = 0;
    mF14 = 0;
    mF18 = 0;
    mF1C = PointerWord(reinterpret_cast<const void*>(callback));
    mF20 = PointerWord(context);
    ccCityList* received = mReceived;
    if (received != nullptr) {
        int32_t count = received->mRefCount - 1;
        received->mRefCount = count;
        if (count == 0) {
            if (received->mInfos != nullptr) delete[] received->mInfos;
            received->mInfos = nullptr;
            v5917_callee::OperatorDelete(received);   // operator delete only, no destructor
        }
        mReceived = nullptr;
    }
    if (OnlineSystem()->AreWeLoggedIn() != 0) {
        PostLoginCallbackFn_City(0, this);
    } else {
        mF08 = 1;
        OnlineSystem()->Login(PostLoginCallbackFn_City, this);
    }
}

// FS_OnlineList.cpp (main app): the city list's overrides of the base list's virtual slots.
//   CFSOnlineSystemCityList::ChildLocked 0x101788d8, RequestListFromServer 0x101789d8, StoreReceivedList 0x10178a78

// 0x101788d8 (196): the lobby's child lock. Answers 1 (locked out) when no lobby screen is wanted (word 0x108 is 0), or when the system's
// child lock lacks bit 0x10; 0 otherwise. The system is made on first use (the original tests the global again before the second use).
int32_t CFSOnlineSystemCityList::ChildLocked() {
    int32_t result = 1;
    if (gOnlineSystem == nullptr) {
        void* memory = ::operator new(0x404);
        gOnlineSystem = new (memory) CFSOnlineSystem();
    }
    if (gOnlineSystem->mF108 != 0) {
        if (gOnlineSystem == nullptr) {
            void* memory = ::operator new(0x404);
            gOnlineSystem = new (memory) CFSOnlineSystem();
        }
        if ((gOnlineSystem->GetChildLock() & 0x10) != 0) result = 0;
    } else {
        result = 0;
    }
    return result;
}

// 0x101789d8 (84): logs, then asks the city for the list (no context, the caller's second argument, flag 0); answers its answer.
int32_t CFSOnlineSystemCityList::RequestListFromServer(City* city, uint32_t unused) {
    SamsUtilities::UDebug("ONLINELIST: Getting city list\n");
    return city->GetCityList(nullptr, unused, 0);
}

// 0x10178a78 (116): makes a CFSOnlineCity (0x68 bytes) for each info of the received list; the infos are 0x30 bytes apart on the Mac
// (the host's ccCityInfo has no size of its own). The cities are not kept here (the constructor registers them with the list).
void CFSOnlineSystemCityList::StoreReceivedList(const ccCityList* list) {
    const char* info = reinterpret_cast<const char*>(list->mInfos);
    for (int32_t i = 0; i < list->mCount; ++i) {
        void* memory = ::operator new(0x68);
        new (memory) CFSOnlineCity(reinterpret_cast<const ccCityInfo*>(info), this);
        info += 0x30;
    }
}

// Unit v51039 (main app): std::list<std::wstring>::clear and the destructors of the swear filter and of the theme map, the font
// wrapper's GetCharWidth, the park list's StoreReceivedList and four small UI helpers. The Mac's strings, heap and virtual calls are
// the callees of the headers' *_callee namespaces (weak definitions in the _callees file; the test library fakes them).

// ---- the weak definitions of the callees the host cannot call as the Mac does (the Mac's strings and heap, new plus constructor,
// virtual calls through __ptr_glue); the unit's test library defines them again as recording fakes.
#define TPW_WEAK __attribute__((weak))
namespace parklist_callee {
// CFSOnlinePark's constructor (0x1017c0d8) is found by its symbol at run time: the test library does not compile its file.
TPW_WEAK void NewPark(const ccParkInfo* info, CFSOnlineSystemParkList* list) {
    typedef void (*Ctor)(void*, const ccParkInfo*, CFSOnlineSystemParkList*);
    static Ctor ctor = reinterpret_cast<Ctor>(dlsym(RTLD_DEFAULT, "_ZN13CFSOnlineParkC1EPK10ccParkInfoP23CFSOnlineSystemParkList"));
    void* park = ::operator new(0xc0);
    if (ctor != nullptr) ctor(park, info, list);
}
}  // namespace parklist_callee

// 0x10178bb0 (116): the signed count is compared with cmpw; the stride is the info's 0x66 bytes.
void CFSOnlineSystemParkList::StoreReceivedList(const ccParkList* list) {
    const ccParkInfo* info = list->mInfos;
    for (int32_t i = 0; i < list->mCount; ++i) {
        parklist_callee::NewPark(info, this);
        ++info;
    }
}

// 0x10178c74 (60): an error completes the list with 1, otherwise the list asks the server.
void PostLoginCallbackFn_Park(int32_t error, const void* context) {
    CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>* list =
        const_cast<CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>*>(
            static_cast<const CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>*>(context));
    if (error != 0) {
        list->CompleteGetList(1);
    } else {
        list->LoggedInGetList();
    }
}

// CFSOnlineSystemParkList_InCity::GetList (0x10178f54, 352 bytes).

// 0x10178f54 (352): stores the city id, clears the request words, keeps the caller's callback and context (32-bit words), releases the
// stored answer, then asks the online system (made on first use) either to answer at once (logged in) or to log in first.
void CFSOnlineSystemParkList_InCity::GetList(int32_t cityID, Callback callback, const void* context) {
    mF34 = cityID;
    mRequestID = -1;
    mF08 = 0;
    mF0C = 0;
    mF10 = 0;
    mF14 = 0;
    mF18 = 0;
    mF1C = static_cast<int32_t>(reinterpret_cast<uintptr_t>(callback));
    mF20 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(context));

    if (mReceived != nullptr) {
        --mReceived->mRefCount;
        if (mReceived->mRefCount == 0) {
            if (mReceived->mInfos != nullptr) delete[] mReceived->mInfos;
            mReceived->mInfos = nullptr;
            ::operator delete(static_cast<void*>(mReceived));
        }
        mReceived = nullptr;
    }

    if (gOnlineSystem == nullptr) {
        void* memory = ::operator new(0x404);
        gOnlineSystem = new (memory) CFSOnlineSystem();
    }
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        PostLoginCallbackFn_Park(0, this);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) {   // (the original tests again; the call above cannot clear it)
            void* memory = ::operator new(0x404);
            gOnlineSystem = new (memory) CFSOnlineSystem();
        }
        gOnlineSystem->Login(&PostLoginCallbackFn_Park, this);
    }
}

// 0x1017911c (104): logs, then asks the city server for the parks of the city in word 0x34; the answer is the call's (r3 is left as is).
int32_t CFSOnlineSystemParkList_InCity::RequestListFromServer(City* city, uint32_t unused) {
    SamsUtilities::UDebug("ONLINELIST: Getting park in city list\n");
    return city->GetParkInCityList(nullptr, mF34, unused, 0);
}

// 0x101791d8 (60): the park at position `index` (the last one when the walk counts it twice: it cannot), or NULL.
CFSOnlinePark* CFSOnlineSystemParkList_InCity::ReturnParkInfo(int32_t index) {
    CFSOnlinePark* result = nullptr;
    int32_t counter = 0;
    POSITION position = mList->GetHeadPosition();
    while (position != nullptr) {
        CFSOnlinePark* record = mList->GetNext(position);
        if (counter == index) result = record;
        counter++;
    }
    return result;
}

// Unit v5928: the theme list's GetList, RequestListFromServer, StoreReceivedList and ReturnOnlineThemeID, the login callback
// ThemeListPostLoginCallbackFn, and the by-park-id and top-10 park lists' RequestListFromServer.

// 0x10179428 (104)
int32_t CFSOnlineSystemParkList_InCityTop10::RequestListFromServer(City* city, uint32_t arg) {
    SamsUtilities::UDebug("ONLINELIST: Getting top 10 park in city list\n");
    return city->GetParkInCityListTop10(nullptr, mF34, arg, 0);
}

// The park list "in the world by player name" (unit v5063): its destructor and GetList.

// 0x101794e8 (364): remembers the player name (the string's virtual assignment, slot 0x20), clears the request words, keeps the
// caller's callback and context (the host's words are 32 bits: the header holds them as int32_t), releases the stored answer, then
// asks the online system (made on first use) either to answer at once (logged in) or to log in first.
void CFSOnlineSystemParkList_InWorldByPlayerName::GetList(const char16_t* playerName, Callback callback, const void* context) {
    mPlayerName = playerName;
    mRequestID = -1;
    mF08 = 0;
    mF0C = 0;
    mF10 = 0;
    mF14 = 0;
    mF18 = 0;
    mF1C = static_cast<int32_t>(reinterpret_cast<uintptr_t>(callback));
    mF20 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(context));

    if (mReceived != nullptr) {
        --mReceived->mRefCount;
        if (mReceived->mRefCount == 0) {
            if (mReceived->mInfos != nullptr) delete[] mReceived->mInfos;
            mReceived->mInfos = nullptr;
            ::operator delete(static_cast<void*>(mReceived));
        }
        mReceived = nullptr;
    }

    if (gOnlineSystem == nullptr) {
        void* memory = ::operator new(0x404);
        gOnlineSystem = new (memory) CFSOnlineSystem();
    }
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        PostLoginCallbackFn_Park(0, this);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) {   // (the original tests again; the call above cannot clear it)
            void* memory = ::operator new(0x404);
            gOnlineSystem = new (memory) CFSOnlineSystem();
        }
        gOnlineSystem->Login(&PostLoginCallbackFn_Park, this);
    }
}

// 0x101796cc (104): logs, then asks the city for the park in the world by the stored player name (the string's text pointer, Mac
// +0x3c); the context is NULL, the flag 0, and the city's answer is passed back.
int32_t CFSOnlineSystemParkList_InWorldByPlayerName::RequestListFromServer(City* city, uint32_t unused) {
    SamsUtilities::UDebug("ONLINELIST: Getting park in world by player name\n");
    return city->GetParkInWorldByName(nullptr, mPlayerName.mData, unused, 0);
}

// 0x10179964 (104)
int32_t CFSOnlineSystemParkList_InWorldByParkID::RequestListFromServer(City* city, uint32_t arg) {
    SamsUtilities::UDebug("ONLINELIST: Getting park in world by park id\n");
    return city->GetParkByID(nullptr, mF34, arg, 0);
}

// FS_OnlineList.cpp (main app, unit w68): the constructors of the theme, server and ride lists and of an online park (default). The
// base CFSOnlineSystemList's constructor (include/FS_OnlineList.h) does the Mac's stores of the two vtable words, the zeroed words and
// the new list (0x10 bytes) that each of the three constructors repeats.
//
//   0x10179a28 CFSOnlineSystemThemeList::CFSOnlineSystemThemeList (168)
//   0x1017a604 CFSOnlineSystemServerList::CFSOnlineSystemServerList (168)
//   0x1017b1ac CFSOnlineSystemRideList::CFSOnlineSystemRideList (168)
//   0x1017bf30 CFSOnlinePark::CFSOnlinePark (380)

// The Mac inlines this constructor into CFSOnlineSystem's; it lives here so that the string is not instantiated in the header.
CFSOnlineSystemParkList_InWorldByPlayerName::CFSOnlineSystemParkList_InWorldByPlayerName() { mPlayerName = u""; }

CFSOnlineSystemThemeList::CFSOnlineSystemThemeList() {}   // 0x10179a28

// 0x10179e8c (140): makes the online system if there is none, then asks the theme list: CompleteGetList(1) when error is nonzero,
// else LoggedInGetList. The context is unused.
void ThemeListPostLoginCallbackFn(int32_t error, const void* context) {
    (void)context;
    if (gOnlineSystem == nullptr) {
        gOnlineSystem = new CFSOnlineSystem();   // 0x404 bytes
    }
    CFSOnlineSystemThemeList* list = gOnlineSystem->mThemeList;
    if (error != 0) {
        list->CompleteGetList(1);
    } else {
        list->LoggedInGetList();
    }
}

// 0x1017a1c0 (348): clears the request state, releases the stored answer, then logs in (or goes straight on when logged in).
void CFSOnlineSystemThemeList::GetList() {
    Reset();
    if (gOnlineSystem == nullptr) {
        gOnlineSystem = new CFSOnlineSystem();
    }
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        ThemeListPostLoginCallbackFn(0, nullptr);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) {
            gOnlineSystem = new CFSOnlineSystem();
        }
        gOnlineSystem->Login(ThemeListPostLoginCallbackFn, nullptr);
    }
}

// 0x1017a354 (88)
int32_t CFSOnlineSystemThemeList::RequestListFromServer(City* city, uint32_t arg) {
    SamsUtilities::UDebug("ONLINELIST: Getting theme list\n");
    return city->GetThemeList(nullptr, arg, 0);
}

// 0x1017a3fc (116): one CFSOnlineTheme (0x48 bytes on the Mac) per info; each adds itself to the list.
void CFSOnlineSystemThemeList::StoreReceivedList(const ccThemeList* list) {
    const ccThemeInfo* info = list->mInfos;
    for (int32_t i = 0; i < list->mCount; i++) {
        new CFSOnlineTheme(info, this);   // 0x48 bytes on the Mac (the host's is larger)
        info++;
    }
}

// 0x1017a4c0 (128): the last theme whose name matches wins; 1 when none does.
uint8_t CFSOnlineSystemThemeList::ReturnOnlineThemeID(const char* name) {
    int32_t id = 1;
    TbTList<CFSOnlineTheme*, CFSOnlineTheme*>::CNode* node = mList->mHead;
    while (node != nullptr) {
        CFSOnlineTheme* theme = node->mData;
        node = node->mNext;
        if (NS_MacDoze::_stricmp(theme->mText0C.mData, name) == 0) {
            id = theme->mThemeID;
        }
    }
    return static_cast<uint8_t>(id);
}

// 0x1017a588 (52): the name of the last theme whose id is `theme` (the string's data pointer, Mac 0x14), or NULL.
char* CFSOnlineSystemThemeList::ReturnOnlineThemeName(int32_t theme) {
    char* result = nullptr;
    POSITION position = mList->GetHeadPosition();
    while (position != nullptr) {
        CFSOnlineTheme* record = mList->GetNext(position);
        if (theme == record->mThemeID) result = record->mText0C.mData;
    }
    return result;
}

// 0x10178ce4 (412, park instance; the city instance is 0x10178414): asks the city server for the list once logged in, or reports the
// failure (answer 1) to the callback. 0x4e: the child lock forbids it; 0x4f: no connection to the city server.
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::LoggedInGetList() {
    int32_t error;
    if (ChildLocked() == 0) {
        if (gOnlineSystem == nullptr) {
            void* memory = ::operator new(0x404);
            gOnlineSystem = new (memory) CFSOnlineSystem();
        }
        City* city = gOnlineSystem->ReturnConnectionToCityServer();
        if (city != nullptr) {
            mF0C = 1;
            mRequestID = RequestListFromServer(city, static_cast<uint32_t>(mF24));
            return;
        }
        error = 0x4f;
    } else {
        error = 0x4e;
    }
    mF18 = error;
    if (mF18 != 0) {
        AddUIError(mF18, nullptr);
        mF18 = 0;
    }
    int32_t logout = mF08;
    int32_t callback = mF1C;
    int32_t context = mF20;
    if (logout != 0) CFSOnlineSystem::Instance()->Logout();
    if (callback == 0) return;
    gOnlineListCallbackCaller(callback, 1, context, mList);
}

namespace {
CFSOnlineSystem* MakeOnlineSystem() {
    void* memory = ::operator new(0x404);
    return new (memory) CFSOnlineSystem();
}
}  // namespace

// 0x1017925c (352): the same as CFSOnlineSystemParkList_InCity::GetList (0x10178f54).
void CFSOnlineSystemParkList_InCityTop10::GetList(int32_t cityID, Callback callback, const void* context) {
    mF34 = cityID;
    mRequestID = -1;
    mF08 = 0;
    mF0C = 0;
    mF10 = 0;
    mF14 = 0;
    mF18 = 0;
    mF1C = static_cast<int32_t>(reinterpret_cast<uintptr_t>(callback));
    mF20 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(context));
    ReleaseReceived(this);
    if (gOnlineSystem == nullptr) gOnlineSystem = MakeOnlineSystem();
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        PostLoginCallbackFn_Park(0, this);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) gOnlineSystem = MakeOnlineSystem();   // (tested again by the original; it cannot be NULL here)
        gOnlineSystem->Login(&PostLoginCallbackFn_Park, this);
    }
}

// 0x10179794 (352): the same.
void CFSOnlineSystemParkList_InWorldByParkID::GetList(int32_t parkID, Callback callback, const void* context) {
    mF34 = parkID;
    mRequestID = -1;
    mF08 = 0;
    mF0C = 0;
    mF10 = 0;
    mF14 = 0;
    mF18 = 0;
    mF1C = static_cast<int32_t>(reinterpret_cast<uintptr_t>(callback));
    mF20 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(context));
    ReleaseReceived(this);
    if (gOnlineSystem == nullptr) gOnlineSystem = MakeOnlineSystem();
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        PostLoginCallbackFn_Park(0, this);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) gOnlineSystem = MakeOnlineSystem();
        gOnlineSystem->Login(&PostLoginCallbackFn_Park, this);
    }
}

// 0x10179b08 (288): the same for the theme list's base destructor (a named function, see FS_OnlineList.h).
void online_list_base::ThemeListBaseDtor(CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>* self) {
    ReleaseReceived(self);
    while (self->mList->GetCount() != 0) {
        CFSOnlineTheme* theme = self->mList->RemoveHead();
        if (theme != nullptr) delete theme;
    }
    self->mF24 = 0;
    self->mF28 = 0;
    if (self->mList != nullptr) delete self->mList;
}

CFSOnlineSystemServerList::CFSOnlineSystemServerList() {}   // 0x1017a604

// Unit v51110: the server and ride lists' small members, their destructors and LoggedInGetList, the FS_OnlineList static initialiser and
// three one-liners (CFSOnlineRide::GetFilePath, CFSOnlineSystem::GetAvatarID, UI_ByeByeBankrupt).

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

// ServerListPostLoginCallbackFn 0x1017aa6c (140 bytes), unit v5721.

void ServerListPostLoginCallbackFn(int32_t error, const void* context) {
    (void)context;
    if (gOnlineSystem == nullptr) {
        gOnlineSystem = new CFSOnlineSystem();   // 0x404 bytes
    }
    CFSOnlineSystemServerList* list = gOnlineSystem->mServerList;
    if (error != 0) {
        list->CompleteGetList(1);
    } else {
        list->LoggedInGetList();
    }
}

// CFSOnlineSystemServerList: GetList 0x1017ada8, RequestListFromServer 0x1017af40, StoreReceivedList 0x1017afec (main app).

// 0x1017ada8 (348): the same reset InitCity runs on each list, then logs in (or calls the callback at once when already logged in).
void CFSOnlineSystemServerList::GetList() {
    Reset();
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        ServerListPostLoginCallbackFn(0, nullptr);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
        gOnlineSystem->Login(ServerListPostLoginCallbackFn, nullptr);
    }
}

// 0x1017af40 (92): the answer is whatever the city's request answers (r3 passes through).
int32_t CFSOnlineSystemServerList::RequestListFromServer(City* city, uint32_t arg) {
    SamsUtilities::UDebug("ONLINELIST: Getting server list\n");
    return city->GetLogicalServerList(nullptr, 1, arg, 0);
}

// 0x1017afec (116): the count is read again at every step.
void CFSOnlineSystemServerList::StoreReceivedList(const ccServerList* list) {
    const ccServerInfo* info = list->mInfos;
    for (int32_t i = 0; i < list->mCount; ++i) {
        new CFSOnlineServer(info, this);   // 0x80 bytes on the Mac; registers itself in the list
        info = reinterpret_cast<const ccServerInfo*>(reinterpret_cast<const char*>(info) + 0x26);
    }
}

CFSOnlineSystemRideList::CFSOnlineSystemRideList() {}   // 0x1017b1ac

CFSOnlineSystemRideList::~CFSOnlineSystemRideList() {}       // 0x1017b4a4

// Unit v51129: std::basic_string<wchar_t>::assign (0x101b2510), the horizontal gradient's destructor (0x10189c0c), the ride list's GetList
// (0x1017b938), the server list's base destructor (0x1017a6e4), the theme list's LoggedInGetList (0x10179f50) and destructor (0x10179d28).

// 0x1017b938 (348): the ride list back to its start state (the stored answer released as InitCity does), then the login, or the callback at once.
void CFSOnlineSystemRideList::GetList() {
    mRequestID = -1;
    mF08 = 0;
    mF0C = 0;
    mF10 = 0;
    mF14 = 0;
    mF18 = 0;
    mF1C = 0;
    mF20 = 0;
    if (mReceived != nullptr) {
        --mReceived->mRefCount;
        if (mReceived->mRefCount == 0) {
            if (mReceived->mInfos != nullptr) onlinelist2_callee::DestroyRideInfos(mReceived->mInfos);
            mReceived->mInfos = nullptr;
            onlinelist2_callee::FreeBlock(mReceived);
        }
        mReceived = nullptr;
    }
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        RideListPostLoginCallbackFn(0, nullptr);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
        gOnlineSystem->Login(RideListPostLoginCallbackFn, nullptr);
    }
}

// 0x10179b08 is the base's own copy of the destructor below (untranslated): the theme list's destructor has that work inline, so it is
// empty here and ~CFSOnlineSystemThemeList does it.
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>::~CFSOnlineSystemList() {}

// Weak defaults: the real calls (the test library replaces them with recording fakes).
namespace onlinelist3_callee {
__attribute__((weak)) CFSOnlineTheme* ThemeRemoveHead(TbPtrList<CFSOnlineTheme>* list) { return list->RemoveHead(); }
__attribute__((weak)) void DestroyTheme(CFSOnlineTheme* theme) { delete theme; }
__attribute__((weak)) void DestroyThemeList(TbPtrList<CFSOnlineTheme>* list) { delete list; }
__attribute__((weak)) void DestroyThemeInfos(ccThemeInfo* infos) { delete[] infos; }
}

// 0x10179d28 (300): the same body as the server list's, with the theme callees.
CFSOnlineSystemThemeList::~CFSOnlineSystemThemeList() {
    if (mReceived != nullptr) {
        --mReceived->mRefCount;
        if (mReceived->mRefCount == 0) {
            if (mReceived->mInfos != nullptr) onlinelist3_callee::DestroyThemeInfos(mReceived->mInfos);
            mReceived->mInfos = nullptr;
            onlinelist2_callee::FreeBlock(mReceived);
        }
        mReceived = nullptr;
    }
    while (mList->GetCount() != 0) {
        CFSOnlineTheme* theme = onlinelist3_callee::ThemeRemoveHead(mList);
        if (theme != nullptr) onlinelist3_callee::DestroyTheme(theme);
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) onlinelist3_callee::DestroyThemeList(mList);
}

// 0x10179f50 (412): asks the city server for the list once logged in, or reports the failure (answer 1) to the callback.
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>::LoggedInGetList() {
    bool failed = true;
    if (ChildLocked() == 0) {
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
        City* city = gOnlineSystem->ReturnConnectionToCityServer();
        if (city != nullptr) {
            mF0C = 1;
            mRequestID = RequestListFromServer(city, static_cast<uint32_t>(mF24));
            failed = false;
        } else {
            mF18 = 0x4f;
        }
    } else {
        mF18 = 0x4e;
    }
    if (failed) {
        if (mF18 != 0) {
            AddUIError(mF18, nullptr);
            mF18 = 0;
        }
        int32_t callback = mF1C;
        int32_t context = mF20;
        if (mF08 != 0) CFSOnlineSystem::Instance()->Logout();
        if (callback != 0) gOnlineListCallbackCaller(callback, 1, context, mList);
    }
}

// CFSOnlineSystemRideList::RequestListFromServer 0x1017bb4c, StoreReceivedList 0x1017bbf0

int32_t CFSOnlineSystemRideList::RequestListFromServer(City* city, uint32_t id) {
    SamsUtilities::UDebug("ONLINELIST: Getting ride list\n");
    return city->GetRideList(nullptr, id, 0);
}

void CFSOnlineSystemRideList::StoreReceivedList(const ccRideList* list) {
    const ccRideInfo* info = list->mInfos;
    for (int32_t i = 0; i < list->mCount; i++, info++) {
        new CFSOnlineRide(info, this);
    }
}

// Unit v5994: the online city record's constructor and destructor body, the park record's assignment, the server record's destructor and
// the ride list's post-login callback (0x1017b604 .. 0x1017c96c).

namespace onlinelist_callee {
// The list calls the Mac makes through the TbTList instances. Weak, so that the test library can replace them with recording fakes.
__attribute__((weak, noinline)) void CityAddTail(TbPtrList<CFSOnlineCity>* list, CFSOnlineCity* city) { list->AddTail(city); }
__attribute__((weak, noinline)) void* CityFind(TbPtrList<CFSOnlineCity>* list, CFSOnlineCity* city) { return list->Find(city); }
__attribute__((weak, noinline)) void CityRemoveAt(TbPtrList<CFSOnlineCity>* list, void*& position) { list->RemoveAt(position); }
__attribute__((weak, noinline)) void FreeCity(void* city) { ::operator delete(city); }
__attribute__((weak, noinline)) void* ServerFind(TbPtrList<CFSOnlineServer>* list, CFSOnlineServer* server) { return list->Find(server); }
__attribute__((weak, noinline)) void ServerRemoveAt(TbPtrList<CFSOnlineServer>* list, void*& position) { list->RemoveAt(position); }
}  // namespace onlinelist_callee

namespace {
typedef TbDynamicStringTemplate<char16_t> WideString;

template <class T>
T RawWord(const void* base, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const unsigned char*>(base) + offset, sizeof(T));
    return value;
}
}  // namespace

// 0x1017bcb4 (312): the three strings made, the fields copied from the 0x30-byte info (the position scaled by 1.01), the city added to
// the list's tail.
CFSOnlineCity::CFSOnlineCity(const ccCityInfo* info, CFSOnlineSystemCityList* list) {
    WideString* name = new (mText0C) WideString;
    WideString* text20 = new (mText20) WideString;
    WideString* text44 = new (mText44) WideString;
    mList = list;
    mCityID = RawWord<int32_t>(info, 0x00);
    *name = static_cast<const char16_t*>(info->mBlock04.get());
    *text20 = static_cast<const char16_t*>(info->mBlock08.get());
    const double scale = 1.01;
    mPosX = static_cast<float>(scale * RawWord<float>(info, 0x0c));
    mPosY = static_cast<float>(scale * RawWord<float>(info, 0x14));
    mPosZ = static_cast<float>(scale * RawWord<float>(info, 0x10));
    mValue = RawWord<int32_t>(info, 0x18);
    *text44 = static_cast<const char16_t*>(info->mBlock1C.get());
    mF58 = RawWord<int32_t>(info, 0x20);
    mF5C = RawWord<int32_t>(info, 0x24);
    mF60 = RawWord<int32_t>(info, 0x28);
    mF64 = RawWord<int32_t>(info, 0x2c);
    onlinelist_callee::CityAddTail(mList->mList, this);
}

// 0x1017bf30. Every field "unset": no list, ids and numbers -1, the strings emptied, the 32 bytes at 0x58 all 0xff, the three shorts -1.
CFSOnlinePark::CFSOnlinePark() {
    mList = nullptr;
    mParkID = -1;
    mText0C = u"";
    mText20 = u"";
    mText34 = u"";
    mF48 = -1;
    mF4C = -1;
    mF50 = -1;
    mF54 = -1;
    for (int32_t i = 0; i < 0x20; ++i) mF58[i] = 0xff;
    mF78 = -1;
    mText7C = "";
    mF90 = -1;
    mText94 = u"";
    mFA8 = -1;
    mFAA = -1;
    mFAC = -1;
    mFB0 = -1;
    mFB4 = -1;
    mFB8 = -1;
    mFBC = -1;
}

// CFSOnlinePark constructor from a ccParkInfo (0x1017c0d8) and destructor (0x1017c464).

namespace {

// The ccParkInfo is a loaded 0x66 byte image: fields are read at their Mac offsets (unaligned past 0x56).
uint32_t InfoWord(const ccParkInfo* info, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(info) + offset, 4);
    return value;
}

uint16_t InfoHalf(const ccParkInfo* info, uint32_t offset) {
    uint16_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(info) + offset, 2);
    return value;
}

// A string pointer of the image: a Ptr32 (an offset from the arena base on the host), null for 0.
template <class T>
const T* InfoPtr(const ccParkInfo* info, uint32_t offset) {
    Ptr32<const T> ptr;
    ptr.mOffset = InfoWord(info, offset);
    return ptr.get();
}

}  // namespace

CFSOnlinePark::CFSOnlinePark(const ccParkInfo* info, CFSOnlineSystemParkList* list) {
    // The five strings are built first (0x0c, 0x20, 0x34, 0x7c, 0x94) by the member initialisers.
    mList = list;
    mParkID = static_cast<int32_t>(InfoWord(info, 0x00));
    mText0C = InfoPtr<char16_t>(info, 0x04);
    mText20 = InfoPtr<char16_t>(info, 0x08);
    mText34 = InfoPtr<char16_t>(info, 0x0c);
    mF48 = static_cast<int32_t>(InfoWord(info, 0x10));
    mF4C = static_cast<int32_t>(InfoWord(info, 0x14));
    mF50 = static_cast<int32_t>(InfoWord(info, 0x18));
    mF54 = static_cast<int32_t>(InfoWord(info, 0x1c));
    for (int32_t i = 0; i < 0x20; i++) mF58[i] = reinterpret_cast<const uint8_t*>(info)[0x20 + i];
    mF78 = static_cast<int32_t>(InfoWord(info, 0x40));
    mText7C = InfoPtr<char>(info, 0x44);
    mF90 = static_cast<int32_t>(InfoWord(info, 0x48));
    mText94 = InfoPtr<char16_t>(info, 0x4c);
    mFA8 = static_cast<int16_t>(InfoHalf(info, 0x50));
    mFAA = static_cast<int16_t>(InfoHalf(info, 0x52));
    mFAC = static_cast<int16_t>(InfoHalf(info, 0x54));
    mFB0 = static_cast<int32_t>(InfoWord(info, 0x56));
    mFB4 = static_cast<int32_t>(InfoWord(info, 0x5a));
    mFB8 = static_cast<int32_t>(InfoWord(info, 0x5e));
    mFBC = static_cast<int32_t>(InfoWord(info, 0x62));
    mList->mList->AddTail(this);
}

// 0x1017be40 (196), the part after the NULL check.
void CFSOnlineCity::Destruct(int16_t deleteFlag) {
    CFSOnlineSystemCityList* owner = mList;
    void* position = onlinelist_callee::CityFind(owner->mList, this);
    if (position != nullptr) onlinelist_callee::CityRemoveAt(owner->mList, position);
    reinterpret_cast<WideString*>(mText44)->~WideString();
    reinterpret_cast<WideString*>(mText20)->~WideString();
    reinterpret_cast<WideString*>(mText0C)->~WideString();
    if (deleteFlag > 0) onlinelist_callee::FreeCity(this);
}

// 0x1017c2e4 (324): every field copied, the strings through their virtual assignments.
CFSOnlinePark& CFSOnlinePark::operator=(const CFSOnlinePark& other) {
    mParkID = other.mParkID;
    mText0C = other.mText0C;
    mText20 = other.mText20;
    mText34 = other.mText34;
    mF48 = other.mF48;
    mF4C = other.mF4C;
    mF50 = other.mF50;
    mF54 = other.mF54;
    for (int32_t i = 0; i < 0x20; i++) mF58[i] = other.mF58[i];
    mF78 = other.mF78;
    mText7C = other.mText7C;
    mF90 = other.mF90;
    mText94 = other.mText94;
    mFA8 = other.mFA8;
    mFAA = other.mFAA;
    mFAC = other.mFAC;
    mFB0 = other.mFB0;
    mFB4 = other.mFB4;
    mFB8 = other.mFB8;
    mFBC = other.mFBC;
    return *this;
}

// 0x1017c96c (228): leaves the list, then the members' destructors (the strings, last to first) run. The deleting form is the compiler's.
// (the destructor itself is now defined by the CFSOnlineServer unit; this is the body, which the test library's destructor calls)
void CFSOnlineServer_DestructBody(CFSOnlineServer* self) {
    CFSOnlineServer* const server = self;
    CFSOnlineSystemServerList* owner = server->mList;
    void* position = onlinelist_callee::ServerFind(owner->mList, server);
    if (position != nullptr) onlinelist_callee::ServerRemoveAt(owner->mList, position);
}

// 0x1017b604 (140)
void RideListPostLoginCallbackFn(int32_t error, const void* context) {
    (void)context;
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
    CFSOnlineSystemRideList* list = gOnlineSystem->mRideList;
    if (error != 0)
        list->CompleteGetList(1);
    else
        list->LoggedInGetList();
}

// The deleting flag is the caller's business here; the strings are destroyed after the body in reverse order, as on the Mac.
// (A NULL this, which the Mac destructor tolerates, cannot happen in C++.)
CFSOnlinePark::~CFSOnlinePark() {
    CFSOnlineSystemParkList* list = mList;
    if (list != nullptr) {
        void* position = list->mList->Find(this);
        if (position != nullptr) list->mList->RemoveAt(position);
    }
}

// CFSOnlineTheme (FS_OnlineList.cpp): 0x1017c57c the constructor, 0x1017c6e0 the destructor.

// Builds the three narrow strings, takes the id, converts the info's three wide strings and adds itself to the list's tail.
CFSOnlineTheme::CFSOnlineTheme(const ccThemeInfo* info, CFSOnlineSystemThemeList* list) {
    mList = list;
    mThemeID = static_cast<int32_t>(info->mF00);
    TbDynamicStringTemplate<char16_t> wide;
    wide = info->mStr04.get();
    UniStringToString(mText0C, wide);
    wide = info->mStr08.get();
    UniStringToString(mText20, wide);
    wide = info->mStr0C.get();
    UniStringToString(mText34, wide);
    mList->mList->AddTail(this);
}

// Leaves the list (the first node holding it), then the strings go (last to first). The delete is the caller's (flag > 0).
CFSOnlineTheme::~CFSOnlineTheme() {
    CFSOnlineSystemThemeList* list = mList;
    POSITION position = list->mList->Find(this);
    if (position != nullptr) list->mList->RemoveAt(position);
}

// CFSOnlineServer::CFSOnlineServer (0x1017c7d0, 324 bytes): copies a server record from the library's ccServerInfo and appends
// itself to the server list's TbPtrList.

CFSOnlineServer::CFSOnlineServer(const ccServerInfo* info, CFSOnlineSystemServerList* list) {
    // (the vtable word at 0 and the five string members are made first, in member order)
    mList = list;
    mF08 = info->mF00;
    mF0C = info->mF04;
    mText10 = info->mStr08.get();
    mText24 = info->mStr0C.get();
    mF38 = info->mF10;
    mF3C = static_cast<int16_t>(info->mF14);
    mText40 = info->mStr16.get();
    mText54 = info->mStr1A.get();
    mF68 = info->mF1E;
    mText6C = info->mStr22.get();
    mList->mList->AddTail(this);
}
// The destructor is not translated yet; this empty body is the vtable's key function so the vtable is emitted.
CFSOnlineServer::~CFSOnlineServer() {}

// CFSOnlineRide::CFSOnlineRide 0x1017ca7c (unit v5141): copies a ride record from the library's info and adds itself to the list's tail.

CFSOnlineRide::CFSOnlineRide(const ccRideInfo* info, CFSOnlineSystemRideList* list) {
    mList = list;
    mRideID = info->mF00;
    mText0C.operator=(info->mStr04.get());
    mText20.operator=(info->mStr08.get());
    mF34 = info->mF0C;
    mF38 = info->mF10;
    mF3C = info->mF14;
    mF40 = info->mF18;
    mF44 = info->mF1C;
    mF48 = info->mF20;
    mText4C.operator=(info->mStr24.get());
    mF60 = info->mF28;
    mF64 = info->mF2C;
    mF68 = info->mF30;
    mList->mList->AddTail(this);
}

// Lead 2026-10-20 (v5L02): the destructor (the key function, so the vtable too) is src/CFSOnlineRide_Dtor_CFSOnlineRide.cpp (v5741).

// CFSOnlineRide::~CFSOnlineRide 0x1017cbf4 (196): takes the ride out of its system list's ride list, then the three strings (0x4c, 0x20,
// 0x0c: the members' own destructors, in reverse order) go. The Mac stores its vtable first and, for a positive delete flag, calls
// operator delete; the host's destructor/delete expression do both. This file defines the class's vtable (the destructor is the key function).

CFSOnlineRide::~CFSOnlineRide() {
    CFSOnlineSystemRideList* list = mList;                  // r31 = this->0x04
    void* position = list->mList->Find(this);               // list at 0x2c of the system list
    if (position != nullptr) list->mList->RemoveAt(position);
}

// CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>: LoggedInGetList 0x10178414 (412) and CompleteGetList 0x10173f98 (468).

namespace {

void CallListCallbackDefault(int32_t callback, int32_t error, int32_t context, void* list) {
    typedef void (*Fn)(int32_t, const void*, void*);
    Fn fn = reinterpret_cast<Fn>(static_cast<uintptr_t>(static_cast<uint32_t>(callback)));
    const void* data = reinterpret_cast<const void*>(static_cast<uintptr_t>(static_cast<uint32_t>(context)));
    fn(error, data, list);
}

void DeleteCityRecordDefault(CFSOnlineCity* city) { delete city; }

typedef CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList> CityBase;

}  // namespace

void (*gOnlineListCallbackCaller)(int32_t, int32_t, int32_t, void*) = CallListCallbackDefault;
void (*gCityRecordDeleter)(CFSOnlineCity*) = DeleteCityRecordDefault;

template<> void CityBase::LoggedInGetList() {
    if (ChildLocked() == 0) {
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
        City* city = gOnlineSystem->ReturnConnectionToCityServer();
        if (city != nullptr) {
            mF0C = 1;
            mRequestID = RequestListFromServer(city, static_cast<uint32_t>(mF24));
            return;
        }
        mF18 = 0x4f;
    } else {
        mF18 = 0x4e;
    }
    // The failure: the same tail twice in the original.
    if (mF18 != 0) {
        AddUIError(mF18, nullptr);
        mF18 = 0;
    }
    int32_t callback = mF1C;
    int32_t context = mF20;
    if (mF08 != 0) CFSOnlineSystem::Instance()->Logout();
    if (callback != 0) gOnlineListCallbackCaller(callback, 1, context, mList);
}

template<> void CityBase::CompleteGetList(int32_t error) {
    bool hadError = mF18 != 0;
    if (hadError) {
        AddUIError(mF18, nullptr);
        mF18 = 0;
    }
    int32_t callback = mF1C;
    int32_t context = mF20;
    if (error == 0 && !hadError && mF14 != 0) {
        while (mList->GetCount() != 0) {
            CFSOnlineCity* city = mList->RemoveHead();
            if (city != nullptr) gCityRecordDeleter(city);
        }
        mF24 = 0;
        mF28 = 0;
        mF28 = 1;
        if (mReceived != nullptr) {
            StoreReceivedList(mReceived);
            mF24 = mReceived->mF08;
            ccCityList* received = mReceived;
            if (received != nullptr) {
                --received->mRefCount;
                if (received->mRefCount == 0) {
                    if (received->mInfos != nullptr) delete[] received->mInfos;
                    received->mInfos = nullptr;
                    ::operator delete(static_cast<void*>(received));
                }
                mReceived = nullptr;
            }
        }
    }
    int32_t result = 0;
    if (error != 0 || hadError || mF28 == 0) {
        if (mF08 != 0) {
            if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
            gOnlineSystem->Logout();
        }
        result = 1;
    }
    if (callback != 0) gOnlineListCallbackCaller(callback, result, context, mList);
}
