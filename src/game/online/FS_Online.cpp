// FS_Online.cpp: name proven by __sinit_FS_Online_cpp in the main application.

#define TPW_V5532_STRONG 1
#include "bfl_FileSystem.h"
#include "CBalanceTable.h"
#include "Debug.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "helpers/FS_OnlineHelpers.h"
#include "helpers/FS_OnlineListHelpers.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbTime.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "OnlineBalance.h"
#include "platform/indirectx.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "std_basic_filebuf.h"
#include "std_exception_what.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include "Text.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPString.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// Unit v5917: std::__tree<unsigned short> find 0x1016d1f8, insert_one 0x1016e9c4, insert_node_at 0x1016f164; IPSystem::Startup 0x1016f3c8
// and Shutdown 0x1016f448; ccParkInfo::~ccParkInfo 0x1016ff48.

// 0x1016f3c8
bool IPSystem::Startup() {
    uint8_t data[404];
    int32_t error = v5917_callee::WSAStartup(0x101, data);
    if (error != 0) {
        ShowError();
        return true;
    }
    return false;
}

// 0x1016f448
bool IPSystem::Shutdown() {
    int32_t error = v5917_callee::WSACleanup();
    if (error != 0) {
        ShowError();
        return true;
    }
    return false;
}

// FS_Online.cpp (main app). PARTIAL: OnlineConfig's constructor (0x1016f4c0), and CFSOnlineSystem's Tick and
// StopProcessing; the rest of the file (CFSOnlineSystem) is not translated yet.

// The defaults before online.sam is read (SetInitialOnlineValues replaces them). The
// vote port is left alone, as in the original (zero in the global gOnlineConfig).
OnlineConfig::OnlineConfig()   // 0x1016f4c0
    : mMailServer("gatekeeper.ea.com"),
      mChatServer("britain.bullfrog.ea.com"),
      mLoginServer("chorlton"),
      mCityServer("chorlton"),
      mNewsServer("chorlton"),
      mVoteServer(),
      mMailPort(25),
      mChatPort(6002),
      mLoginPort(7598),
      mCityPort(7591),
      mNewsPort(7597),
      mComplainEmail("support@feral.co.uk") {}

// CGameType (main app 0x1016f5d8..0x1016f79c; the inventory files it under FS_Online.cpp; unit w40): the game's type (normal, online
// or "easy mode" master-save user) and the three bits of the game option word that mirror it. The one object is TheGameType()
// (include/FS_Online.h), made on first use.

// 0x1016f5d8. The type starts as 0 and is then set from the option bits already in the word: easy mode wins over online, online over
// normal, and none of the three is the normal game. Every path goes through SetGameType, so the word ends with exactly one bit.
CGameType::CGameType() : mType(kGameTypeNormal) {
    const uint32_t options = gGameOptions;
    if ((options & kGameOptionEasyGame) != 0) {
        SetGameType(kGameTypeEasy);
    } else if ((options & kGameOptionOnlineGame) != 0) {
        SetGameType(kGameTypeOnline);
    } else if ((options & kGameOptionNormalGame) != 0) {
        SetGameType(kGameTypeNormal);
    } else {
        SetGameType(kGameTypeNormal);
    }
}

// 0x1016f6a0. The assertion tests `type < 3` as a signed number (-1 passes) and the type is stored before the switch whatever it is,
// so 3 and up, or negative, change the object but not the option word. Each of the three leaves its own bit set and the others clear.
void CGameType::SetGameType(GameType type) {
    GAME_ASSERTMSG(type < 3, "Invalid GameType in SetGameType\n");
    mType = type;
    switch (type) {
    case kGameTypeNormal:
        gGameOptions |= kGameOptionNormalGame;
        gGameOptions &= ~kGameOptionOnlineGame;
        gGameOptions &= ~kGameOptionEasyGame;
        break;
    case kGameTypeOnline:
        gGameOptions &= ~kGameOptionNormalGame;
        gGameOptions |= kGameOptionOnlineGame;
        gGameOptions &= ~kGameOptionEasyGame;
        break;
    case kGameTypeEasy:
        gGameOptions &= ~kGameOptionNormalGame;
        gGameOptions &= ~kGameOptionOnlineGame;
        gGameOptions |= kGameOptionEasyGame;
        break;
    default:
        break;
    }
}

// FS_Online_init.cpp (main app, unit w68): the online system's constructor, the Init* members that put each online job back to its
// start, the Release*Connection members they call, and TickLogin. The state tickers (TickNews, TickCity, TickUpload,
// TickFTPDownload), CompleteLogin, the list requests and the destructor are not translated yet.
//
//   0x1016f7d0 CFSOnlineSystem::CFSOnlineSystem (1356)     0x101719f8 InitLogin (176)         0x10171b68 ReleaseLoginConnection (96)
//   0x10172238 InitNews (196)                              0x1017249c ReleaseNewsConnection (96)
//   0x101735cc InitCity (1280)                             0x10174ac8 ReleaseCityServerConnection (96)
//   0x10175a18 InitUpload (180)                            0x1017598c ReleaseUploadServerConnection (68)
//   0x10176cec InitFTPDownload (128)                       0x101770dc ReleaseFTPDownloadServerConnection (124)
//   0x10171adc TickLogin (88)

// The strings are built in the order of their offsets (the compiler's member order on the Mac), the inline string list (0x3c8) between
// two of them; then the connections are cleared and every job started from scratch. The nine lists and the park are made with new
// (the Mac tests each pointer operator new answered; here new throws instead).
CFSOnlineSystem::CFSOnlineSystem() {   // 0x1016f7d0
    new (&mLoginName) TbDynamicStringTemplate<char16_t>();
    new (&mLoginPassword) TbDynamicStringTemplate<char16_t>();
    new (&mLoginEmail) TbDynamicStringTemplate<char16_t>();
    new (&mNewsText) TbDynamicStringTemplate<char>();
    new (&mF98) TbDynamicStringTemplate<char16_t>();
    new (&mFB4) TbDynamicStringTemplate<char16_t>();
    new (&mF10C) TbDynamicStringTemplate<char>();
    new (&mF330) TbDynamicStringTemplate<char>();
    new (&mF348) TbDynamicStringTemplate<char16_t>();
    new (&mF35C) TbDynamicStringTemplate<char16_t>();
    new (&mF3AC) TbDynamicStringTemplate<char>();
    new (&mF3C8) TbPtrList<TbDynamicStringTemplate<char16_t>>();   // a union member since v5921 (its destructor runs by hand), so built here
    new (&mF3DC) TbDynamicStringTemplate<char>();
    mLogin = nullptr;
    InitLogin();
    mNews = nullptr;
    mF70 = 0;
    InitNews();
    mCity = nullptr;
    mCityList = new CFSOnlineSystemCityList;
    mParksTop10 = new CFSOnlineSystemParkList_InCityTop10;
    mParksInCity = new CFSOnlineSystemParkList_InCity;
    mParksByPlayer = new CFSOnlineSystemParkList_InWorldByPlayerName;
    mParksByID = new CFSOnlineSystemParkList_InWorldByParkID;
    mParksInCity2 = new CFSOnlineSystemParkList_InCity;
    mThemeList = new CFSOnlineSystemThemeList;
    mServerList = new CFSOnlineSystemServerList;
    mRideList = new CFSOnlineSystemRideList;
    InitCity();
    mUpload = nullptr;
    mF108 = 0;
    InitUpload();
    mF39C = nullptr;
    mFTP = nullptr;
    mFTPPark = new CFSOnlinePark;
    InitFTPDownload();
}

// CFSOnlineSystemParkList::~CFSOnlineSystemParkList (0x1016fd48, 300 B). The Mac's destructor has the base class's destructor inlined:
// the stored answer is released (the count drops by one and at zero the infos are destroyed and freed, then the answer itself freed;
// the pointer is cleared whatever the count was), the typed list is emptied head first (each park deleted through its virtual
// destructor), the two words at 0x24 and 0x28 are cleared and the list itself is deleted. The request id and the other words stay.
// The host's base destructor (CFSOnlineSystemList, out of line elsewhere) must therefore do nothing after this body: the list pointer is
// cleared after the delete (host only, v5L17), and the base skips its work on a NULL list. Without that the base read the freed list
// and deleted it again (the crash in CFSOnlineSystem::~CFSOnlineSystem when the park is left).
// The assembly reads the list at 0x2c without a NULL test (a list is always made by the constructor); `this` NULL is not reachable.

CFSOnlineSystemParkList::~CFSOnlineSystemParkList() {
    ccParkList* received = mReceived;
    if (received != nullptr) {
        received->mRefCount = received->mRefCount - 1;
        if (received->mRefCount == 0) {
            if (received->mInfos != nullptr) delete[] received->mInfos;
            received->mInfos = nullptr;
            ::operator delete(static_cast<void*>(received));
        }
        mReceived = nullptr;
    }
    while (mList->GetCount() != 0) {
        CFSOnlinePark* park = mList->RemoveHead();
        if (park != nullptr) delete park;
    }
    mF24 = 0;
    mF28 = 0;
    delete mList;
    mList = nullptr;   // host only: the base destructor runs after this one (see above)
}

// 0x1016ff48. The five blocks are 32-bit words of the library's record (offsets 4, 0xc, 8, 0x44, 0x4c), freed when set and cleared.
ccParkInfo* ccParkInfo::Destruct(ccParkInfo* self, int16_t deleteFlag) {
    if (self != nullptr) {
        static const uint32_t kSlots[5] = {0x4, 0xc, 0x8, 0x44, 0x4c};
        for (uint32_t slot : kSlots) {
            Ptr32<void>* word = reinterpret_cast<Ptr32<void>*>(self->mF00 + slot);
            if (word->mOffset != 0) v5917_callee::Free(word->get());
            word->mOffset = 0;
        }
        if (deleteFlag > 0) v5917_callee::OperatorDelete(self);
    }
    return self;
}

ccParkInfo::~ccParkInfo() { Destruct(this, 0); }

// Unit v5921: three destructors of the online system (main app).
//   CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::~CFSOnlineSystemList  0x1017004c (288)
//   ccCityInfo::~ccCityInfo                                                          0x101703e4 (164)
//   CFSOnlineSystem::~CFSOnlineSystem                                                0x1017055c (684)
// The Mac's hidden "delete storage" flag (r4) is the compiler's deleting destructor here: the caller deletes when it is above 0.
// The Mac's stores of the vtable words (list 0x102a7310 at +0, 0x102a738c at +0x3c8) are the compiler's work.

// The park list's destructor: the stored answer is released (count down; at zero the infos are destroyed and freed and the answer
// freed), then the parks are removed from the list and deleted, the two words cleared and the list itself deleted.
// The Mac reads the list without a NULL test. Host only (v5L17): on the host this runs after every park list's own destructor
// (CFSOnlineSystemParkList::~ does the same work inline, as on the Mac, and clears the pointer), so a NULL list skips the drain.
template<>
CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::~CFSOnlineSystemList() {
    ccParkList* received = mReceived;
    if (received != nullptr) {
        received->mRefCount = received->mRefCount - 1;
        if (received->mRefCount == 0) {
            if (received->mInfos != nullptr) delete[] received->mInfos;   // __destroy_new_array
            received->mInfos = nullptr;
            ::operator delete(static_cast<void*>(received));
        }
        mReceived = nullptr;
    }
    while (mList != nullptr && mList->mCount != 0) {
        CFSOnlinePark* park = mList->RemoveHead();
        if (park != nullptr) delete park;
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) delete mList;
}

// Frees the three blocks the answer carries (each cleared after).
ccCityInfo::~ccCityInfo() {
    if (mBlock04.get() != nullptr) free(mBlock04.get());
    mBlock04 = nullptr;
    if (mBlock08.get() != nullptr) free(mBlock08.get());
    mBlock08 = nullptr;
    if (mBlock1C.get() != nullptr) free(mBlock1C.get());
    mBlock1C = nullptr;
}

// Logs out, releases the news connection, deletes the nine lists, the upload and FTP connections, the FTP park, then the strings and the
// string list from the last to the first member.
CFSOnlineSystem::~CFSOnlineSystem() {
    Logout();
    ReleaseNewsConnection();
    if (mCityList != nullptr) delete mCityList;
    if (mParksTop10 != nullptr) delete mParksTop10;
    if (mParksInCity != nullptr) delete mParksInCity;
    if (mParksByPlayer != nullptr) delete mParksByPlayer;
    if (mParksByID != nullptr) delete mParksByID;
    if (mParksInCity2 != nullptr) delete mParksInCity2;
    if (mThemeList != nullptr) delete mThemeList;
    if (mServerList != nullptr) delete mServerList;
    if (mRideList != nullptr) delete mRideList;
    ReleaseUploadServerConnection();
    ReleaseFTPDownloadServerConnection();
    if (mFTPPark != nullptr) delete mFTPPark;
    mF3DC.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
    mF3C8.TbPtrList<TbDynamicStringTemplate<char16_t>>::~TbPtrList();
    mF3AC.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
    mF35C.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mF348.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mF330.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
    mF10C.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
    mFB4.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mF98.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mNewsText.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
    mLoginEmail.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mLoginPassword.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mLoginName.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
}

// Unit v5L17: the online list destructors the online system's teardown reaches (main app, FS_Online.cpp).
//   CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::~CFSOnlineSystemList  0x101701c8 (288)
//   CFSOnlineSystemParkList_InWorldByParkID::~CFSOnlineSystemParkList_InWorldByParkID 0x10170834 (312)
//   CFSOnlineSystemParkList_InCityTop10::~CFSOnlineSystemParkList_InCityTop10        0x10170ca8 (312)
// The Mac's derived destructors have the base's work inline (only the vtable stores differ) and never call the base's out-of-line copy;
// in host C++ the base destructor always runs after the derived one. The park lists' destructors are empty here (as v5532 and v51110 did
// for the InCity, server and ride lists): their work is CFSOnlineSystemParkList::~ (0x1016fd48), which clears the list pointer, after
// which the park instance of the base (0x1017004c) finds a NULL list and does nothing. The city list's destructor (0x10170e20, unit
// v51065) has the body itself and clears the pointer the same way, so the city base below does nothing after it either; on its own it
// does the whole work. The same bodies with the Mac's delete flag are the stdexc:: helpers of unit v51125 (src/std_exception_what.cpp).
// The Mac's hidden "delete storage" flag (r4) is the compiler's deleting destructor here: the caller deletes when it is above 0.

// The city list's base: the stored answer is released (count down; at zero the infos are destroyed and freed and the answer freed), then
// the cities are removed from the list and destroyed (the Mac's virtual destructor, flag 1, through gCityRecordDeleter: the host's
// CFSOnlineCity has no vtable), the two words cleared and the list itself deleted. The Mac reads the list without a NULL test; host only
// (v5L17), the drain is skipped on a NULL list: this runs after the city list's own destructor, which clears the pointer.
template<>
CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::~CFSOnlineSystemList() {
    ccCityList* received = mReceived;
    if (received != nullptr) {
        received->mRefCount = received->mRefCount - 1;
        if (received->mRefCount == 0) {
            if (received->mInfos != nullptr) delete[] received->mInfos;   // __destroy_new_array
            received->mInfos = nullptr;
            ::operator delete(static_cast<void*>(received));
        }
        mReceived = nullptr;
    }
    while (mList != nullptr && mList->mCount != 0) {
        CFSOnlineCity* city = mList->RemoveHead();
        if (city != nullptr) gCityRecordDeleter(city);
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) delete mList;
    mList = nullptr;
}

CFSOnlineSystemParkList_InWorldByParkID::~CFSOnlineSystemParkList_InWorldByParkID() {}    // 0x10170834: CFSOnlineSystemParkList::~'s work

// The park list "in the world by player name" (unit v5063): its destructor and GetList.

// 0x101709b0 (316). The Mac's destructor inlines its bases': the string member is destroyed first (the call at the top), then the park
// list's and the list's work (the stored answer released, the typed list emptied and deleted); C++ runs the same steps in that order
// (member, then bases), and the deleting form's "operator delete(this)" for a positive 16-bit flag is the delete expression.
CFSOnlineSystemParkList_InWorldByPlayerName::~CFSOnlineSystemParkList_InWorldByPlayerName() {}

// 0x10170b34 (312): the Mac inlines the base destructors here (vtable stores, the stored answer released, the parks drained from the
// list, the words 0x24 and 0x28 cleared, the list deleted); the host's base destructor
// CFSOnlineSystemParkList::~CFSOnlineSystemParkList does exactly that, so the derived one adds nothing.
CFSOnlineSystemParkList_InCity::~CFSOnlineSystemParkList_InCity() {}

CFSOnlineSystemParkList_InCityTop10::~CFSOnlineSystemParkList_InCityTop10() {}            // 0x10170ca8: CFSOnlineSystemParkList::~'s work

// The lobby calls this every frame: the five online tickers, in this order.
void CFSOnlineSystem::Tick() {   // 0x10170f80
    TickLogin();
    TickNews();
    TickCity();
    TickUpload();
    TickFTPDownload();
}

// The online system's login callbacks: each makes the global system on first use, then forwards to it.

void FSOnlineLoginRASFailureCallback() {   // 0x1017100c
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->LoginRASCancelled();
}

// Unit v51123: small CFSOnlineSystem members (the FTP post-login callbacks, the login getters, Login, Logout), the park, server and theme
// instances of CFSOnlineSystemList::CompleteGetList, two std::__tree helpers and two status panel members.

// 0x101710ac: keeps the caller's callback and context, then raises the login panel (failure callback TVector 0x102674b0).
void CFSOnlineSystem::Login(void (*callback)(int32_t, const void*), const void* context) {
    mLoginCallback = reinterpret_cast<OnlineLoginCallback>(callback);
    mLoginContext = const_cast<void*>(context);
    CreateOnlineLoginPanel(FSOnlineLoginRASFailureCallback);
}

// 0x10171114
void CFSOnlineSystem::LoginRASCancelled() { CompleteLogin(1); }

void FSOnlineLoginConnectionCallback(int32_t a, int32_t b, void* c, void* d) {   // 0x10171178
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->LoginConnectionCallback(a, b, c, d);
}

// FS_Online.cpp (main app), unit v5924: the login and news jobs' starters and callbacks, and the news job's end.
//   LoginConnectionCallback 0x101715a0, DoLogin 0x10171234, LoggedInGetNews 0x10171eb8, NewsConnectionCallback 0x10172030,
//   CompleteNews 0x1017253c.

// 0x102a7248: the GUID the news requests carry. Only its first word (0x5c200a41, bytes 5c 20 0a 41) was read; the rest is not known.
NS_MacDoze::_GUID gOnlineNewsGuid = {{0x5c, 0x20, 0x0a, 0x41}};

namespace online_callee {
__attribute__((weak)) void Free(void* p) {
    std::free(p);
}
}  // namespace online_callee

// The empty wide text the news words are reset to (the literal at 0x1024d390; its content is checked by the test).
static const char16_t kNewsResetText[] = u"";

// 0x10171234. Keeps the name and the password, drops the old connection, makes a new one and connects to the login server; when
// that works it logs in (with the text language), otherwise error 0x4f is reported through CompleteLogin(0).
void CFSOnlineSystem::DoLogin(const char16_t* name, const char16_t* password) {
    mLoginName = name;
    mLoginPassword = password;
    ReleaseLoginConnection();
    mLogin = new Auth();
    SamsUtilities::UDebug("ONLINE LOGIN: About to connect\n");
    int32_t connected = mLogin->Connect(const_cast<char*>(gOnlineConfig.mLoginServer.c_str()), nullptr,
                                        static_cast<uint16_t>(gOnlineConfig.mLoginPort), FSOnlineLoginConnectionCallback);
    if (connected == 1) {
        SamsUtilities::UDebug("ONLINE LOGIN: Connected - Logging in\n");
        char16_t* passwordText = mLoginPassword.mData;
        char16_t* nameText = mLoginName.mData;
        mLogin->Login(static_cast<uint16_t>(gText.GetLanguageId()), nameText, passwordText, nullptr);
    } else {
        mLoginErrorID = 0x4f;
        CompleteLogin(0);
    }
}

// 0x10171398
void CFSOnlineSystem::Logout() {
    InitLogin();
    StopProcessing();
}

// Called as an island screen opens: every online job back to its start, then the progress bar goes (the login job is left alone).
void CFSOnlineSystem::StopProcessing() {   // 0x10171404
    InitNews();
    InitCity();
    InitUpload();
    InitFTPDownload();
    FuckOffUIProgressBar();
}

// Unit v5130 (main app, FS_Online.cpp): the online system's chat satellite, publishing words, the upload and FTP download entry points
// and AreWeProcessing.

namespace {

// The Mac keeps callbacks and contexts as 32-bit words in the system's int32 members; they come back as unsigned words.
int32_t PointerWord(const void* pointer) { return static_cast<int32_t>(reinterpret_cast<intptr_t>(pointer)); }
uintptr_t WordPointer(int32_t word) { return static_cast<uintptr_t>(static_cast<uint32_t>(word)); }

// A function pointer kept in such a word (PointerWord drops its upper half): the program's code sits in one 4 GB window, so the upper half is
// that of any function of this file.
void AnchorFunction() {}
uintptr_t WordFunction(int32_t word) {
    return (reinterpret_cast<uintptr_t>(&AnchorFunction) & ~static_cast<uintptr_t>(0xFFFFFFFFu)) | static_cast<uint32_t>(word);
}

// The literal the original reads at 0x1024cc03 + 0x167 (a pooled string next to "ONLINE LOGIN: About to connect").
const char kDefaultPublishText[] = "";

}  // namespace

// 0x10171498. 1 when any connection is open, any of the nine lists has a request out, or the upload or FTP download runs.
int32_t CFSOnlineSystem::AreWeProcessing() {
    int32_t result = 0;
    if (mLogin != nullptr || mNews != nullptr || mCityList->mF0C != 0 || mParksTop10->mF0C != 0 || mParksInCity->mF0C != 0 ||
        mParksByPlayer->mF0C != 0 || mParksByID->mF0C != 0 || mParksInCity2->mF0C != 0 || mThemeList->mF0C != 0 ||
        mServerList->mF0C != 0 || mRideList->mF0C != 0 || mUploadActive != 0 || mFTPActive != 0)
        result = 1;
    return result;
}

// 0x101715a0. The connection answer `a`: 0x3c is success (the context `d` holds the account: two words, the e-mail text at 0x30, four
// words at 0x58..0x64 and the use count at 0x68, which is the context's to free); -2 / -4 / -5 / -6 are the errors 0x4b / 0x4c /
// 0x4d / 0x4e; anything else is 0x37. A failure clears the logged-in word; both end by setting the login-done word.
void CFSOnlineSystem::LoginConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    int32_t error;
    if (a == -3) {
        error = 0x37;
    } else if (a < -3) {
        if (a == -5) {
            error = 0x4d;
        } else if (a > -5) {
            error = 0x4c;
        } else if (a > -7) {
            error = 0x4e;
        } else {
            error = 0x37;
        }
    } else {
        if (a == 0x3c) {
            SamsUtilities::UDebug("ONLINE LOGIN: Authorization OK\n");
            uint8_t* ctx = static_cast<uint8_t*>(d);
            std::memcpy(&mF34, ctx + 0x00, 4);
            std::memcpy(&mF38, ctx + 0x04, 4);
            mLoginEmail = reinterpret_cast<const char16_t*>(ctx + 0x30);
            std::memcpy(&mF50, ctx + 0x58, 4);
            std::memcpy(mF54 + 0, ctx + 0x5c, 4);
            std::memcpy(mF54 + 4, ctx + 0x60, 4);
            std::memcpy(mF54 + 8, ctx + 0x64, 4);
            uint32_t count;
            std::memcpy(&count, ctx + 0x68, 4);
            count = count - 1;
            std::memcpy(ctx + 0x68, &count, 4);
            if (count == 0 && d != nullptr) ftp_callee::OperatorDelete(d);
            mLoginErrorID = 0;
            mLoggedIn = 1;
            mLoginDone = 1;
            return;
        }
        if (a < 0x3c && a < -1) {
            error = 0x4b;
        } else {
            error = 0x37;
        }
    }
    mLoginErrorID = error;
    mLoggedIn = 0;
    mLoginDone = 1;
}

// FS_Online_tick.cpp (main app, unit w69): the online system's four state tickers (news, city, upload, FTP download), CompleteLogin and
// AreWeLoggedIn. Each ticker follows one online job while it runs (the interface's progress bar shows its request) and hands the job to its
// Complete* member once it is done; the job completions themselves are not translated yet.
//
//   0x1017232c CFSOnlineSystem::TickNews (320)          0x10173afc TickCity (1132)
//   0x10175b00 TickUpload (236)                         0x10176da4 TickFTPDownload (180)
//   0x10171c08 CompleteLogin (444)                      0x1017178c AreWeLoggedIn (8)

namespace {
// done x 100 / total as the Mac computes it (mulli, then divw: the product wraps in 32 bits); the callers have tested total > 0.
int32_t Percent(int32_t done, int32_t total) {
    return static_cast<int32_t>(static_cast<uint32_t>(done) * 100u) / total;
}

// TickCity's step for one list: when a request is out (word 0xc) and its answer is in (word 0x10) the request is cleared and completed
// (with 0). The answer is the list's word 0xc as it stands afterwards (CompleteGetList may start another request).
template<class LIST> int32_t CompleteIfAnswered(LIST* list) {
    if (list->mF0C != 0 && list->mF10 != 0) {
        list->mF0C = 0;
        list->CompleteGetList(0);
    }
    return list->mF0C;
}
}  // namespace

// 0x1017178c.
int32_t CFSOnlineSystem::AreWeLoggedIn() {
    return mLoggedIn;
}

// 0x101717cc: always when not logged in; else when bit 5 (0x20) of the child lock word is set.
int32_t CFSOnlineSystem::CanWeSendEmail() {
    if (mLoggedIn == 0) return 1;
    return (GetChildLock() & 0x20) != 0 ? 1 : 0;
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

// Unit v51110: the server and ride lists' small members, their destructors and LoggedInGetList, the FS_OnlineList static initialiser and
// three one-liners (CFSOnlineRide::GetFilePath, CFSOnlineSystem::GetAvatarID, UI_ByeByeBankrupt).

namespace ftpreq_callee {
// 0x10176c90 (40): a copy of the wide string the ride keeps at 0x54 (the data pointer of mText4C), returned by value.
TbDynamicStringTemplate<char16_t> GetFilePath(const CFSOnlineRide* ride) {
    return TbDynamicStringTemplate<char16_t>(ride->mText4C.mData);
}
}  // namespace ftpreq_callee

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

// 0x101719f8. The connection goes first, then the login words and the three login strings (the Mac's order).
void CFSOnlineSystem::InitLogin() {
    ReleaseLoginConnection();
    mLoginDone = 0;
    mLoggedIn = 0;
    mLoginErrorID = 0;
    mLoginCallback = nullptr;
    mLoginContext = nullptr;
    mLoginName = u"";
    mLoginPassword = u"";
    mF34 = 0;
    mF38 = 0;
    mLoginEmail = u"";
    mF50 = 0;
}

// 0x10171adc. A connected login that has finished is completed once (CompleteLogin(0)) and its flag cleared.
void CFSOnlineSystem::TickLogin() {
    if (mLogin != nullptr && mLoginDone != 0) {
        CompleteLogin(0);
        mLoginDone = 0;
    }
}
// 0x10171b68. The library's Release, then its destructor with the delete flag (the deleting form: the object is freed too), then the
// pointer is cleared and the message logged.
void CFSOnlineSystem::ReleaseLoginConnection() {
    if (mLogin != nullptr) {
        mLogin->Release();
        delete mLogin;
        mLogin = nullptr;
        SamsUtilities::UDebug("ONLINE LOGIN: Releasing connection\n");
    }
}

// 0x10171c08. A pending error id goes to the interface first. Error 0: when the player is logged in, the connection is released, the
// login panel removed, the callback and its context kept (words 0xc8 and 0xcc), and the outbox panel made; if it made none the news
// are fetched. Any other error: the callback (if any) hears the error and is forgotten, the connection is released and the login words
// and strings are put back to nothing.
void CFSOnlineSystem::CompleteLogin(int32_t error) {
    if (mLoginErrorID != 0) {
        AddUIError(mLoginErrorID, nullptr);
        mLoginErrorID = 0;
    }
    if (error == 0) {
        if (mLoggedIn != 0) {
            ReleaseLoginConnection();   // (the Mac has this inlined: Release, delete, clear, log)
            RemoveOnlineLoginPanel(0);
            mLoginCallbackKept = mLoginCallback;
            mLoginContextKept = mLoginContext;
            if (CreateOnlineOutboxPanel() == 0) LoggedInGetNews();
        }
    } else {
        if (mLoginCallback != nullptr) {
            mLoginCallback(error, mLoginContext);
            mLoginCallback = nullptr;
            mLoginContext = nullptr;
        }
        ReleaseLoginConnection();
        mLoginDone = 0;
        mLoggedIn = 0;
        mLoginErrorID = 0;
        mLoginCallback = nullptr;
        mLoginContext = nullptr;
        mLoginName = u"";
        mLoginPassword = u"";
        mF34 = 0;
        mF38 = 0;
        mLoginEmail = u"";
        mF50 = 0;
    }
}

// Unit v51117: FS_MasterSave.cpp's static initialiser, the online callbacks that make the online system on first use, and keyboard actions.

// 0x10171dfc (also defined weakly in src/FS_Online_init_CFSOnlineSystem_LoginConnectionCallback.cpp: same body)
__attribute__((weak)) void FSOnlineNewsConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->NewsConnectionCallback(a, b, c, d);
}

// 0x10171eb8. Starts the news job: a new connection, the progress bar (text 0xf4), and when connected the three requests (server
// details, game news, server news), each id kept in its word. A failed connection is error 0x4f and ends the job.
void CFSOnlineSystem::LoggedInGetNews() {
    ReleaseNewsConnection();
    mNews = new News();
    SamsUtilities::UDebug("ONLINE NEWS: About to connect\n");
    CreateUIProgressBar(0xf4);
    int32_t connected = mNews->Connect(const_cast<char*>(gOnlineConfig.mNewsServer.c_str()), nullptr,
                                       static_cast<uint16_t>(gOnlineConfig.mNewsPort), FSOnlineNewsConnectionCallback);
    if (connected == 1) {
        SamsUtilities::UDebug("ONLINE NEWS: Connected - Getting News\n");
        mNewsRequest1 = mNews->GetGameServerDetails(static_cast<uint16_t>(gText.GetLanguageId()), &gOnlineNewsGuid, nullptr);
        mNewsRequest2 = mNews->GetGameNews(static_cast<uint16_t>(gText.GetLanguageId()), &gOnlineNewsGuid, nullptr);
        mNewsRequest3 = mNews->GetServerNews(static_cast<uint16_t>(gText.GetLanguageId()), nullptr);
    } else {
        SamsUtilities::UDebug("ONLINE NEWS: Could not connect\n");
        mNewsFinished = 0x4f;
        CompleteNews();
    }
}

// 0x10172030. The news answer `a`: -2 / -4 / -5 / -6 are the errors 0x4b / 0x4c / 0x4d / 0x4e (the finished word); 0x33 is the server
// details (when that request was made, its text is kept); 0x32 is a news text for the request `b` (the game news or the server news;
// a byte order mark is skipped). The context `d` (a buffer, a word, a use count at 8) is freed with its last use.
void CFSOnlineSystem::NewsConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    OnlineUploadContext* ctx = static_cast<OnlineUploadContext*>(d);
    if (a == -2) {
        mNewsFinished = 0x4b;
    } else if (a < -2) {
        if (a == -5) {
            mNewsFinished = 0x4d;
        } else if (a < -5) {
            if (a > -7) mNewsFinished = 0x4e;
        } else if (a < -3) {
            mNewsFinished = 0x4c;
        }
    } else if (a == 0x33) {
        if (mNewsRequest1 != 0) {
            SamsUtilities::UDebug("ONLINE NEWS: Got server details\n");
            mNewsText = static_cast<const char*>(ctx->mBuffer);
            mNewsDone1 = 1;
        }
    } else if (a < 0x33 && a > 0x31) {
        const char16_t* text = static_cast<const char16_t*>(ctx->mBuffer);
        if (text[0] == 0xfffe || text[0] == 0xfeff) text = text + 1;
        if (static_cast<uint32_t>(b) == mNewsRequest2) {
            SamsUtilities::UDebug("ONLINE NEWS: Got game news\n");
            mF98 = text;
            mNewsDone2 = 1;
        } else if (static_cast<uint32_t>(b) == mNewsRequest3) {
            SamsUtilities::UDebug("ONLINE NEWS: Got server news\n");
            mFB4 = text;
            mNewsDone3 = 1;
        } else {
            SamsUtilities::UDebug("ONLINE NEWS: Got ???? news\n");
        }
        int32_t count = ctx->mRefCount - 1;
        ctx->mRefCount = count;
        if (count == 0 && ctx != nullptr) {
            if (ctx->mBuffer != nullptr) online_callee::Free(ctx->mBuffer);
            ftp_callee::OperatorDelete(ctx);
        }
    }
}

// 0x10172238. The news words back to "nothing received" (-1 for the three ids), the text and strings emptied.
void CFSOnlineSystem::InitNews() {
    ReleaseNewsConnection();
    mLoginCallbackKept = nullptr;
    mLoginContextKept = nullptr;
    mNewsRequest1 = -1;
    mNewsDone1 = 0;
    mNewsText = "";
    mNewsRequest2 = -1;
    mNewsDone2 = 0;
    mF98 = u"";
    mNewsRequest3 = -1;
    mNewsDone3 = 0;
    mFB4 = u"";
    mNewsFinished = 0;
}

// 0x1017232c. While the news job has requests out (the finished word is 0 and one of the three answers is missing) the bar shows the
// request in flight (the current message, or the waiting one when there is none) with the text of the first of the three requests it
// matches, 0xf5 to 0xf7, and no bar when it matches none; when everything is answered the bar goes and CompleteNews runs.
void CFSOnlineSystem::TickNews() {
    if (mNews == nullptr) return;
    if (mNewsFinished == 0 && (mNewsDone1 == 0 || mNewsDone2 == 0 || mNewsDone3 == 0)) {
        uint32_t id = mNews->GetCurrentMessage(0);
        if (id == 0) id = mNews->GetCurrentMessage(1);
        if (id == mNewsRequest1) {
            CreateUIProgressBar(0xf5);
        } else if (id == mNewsRequest2) {
            CreateUIProgressBar(0xf6);
        } else if (id == mNewsRequest3) {
            CreateUIProgressBar(0xf7);
        } else {
            FuckOffUIProgressBar();
        }
        int32_t done = 0;
        int32_t total = 0;
        mNews->MonitorMessage(id, &done, &total);
        if (0 < total) UpdateUIProgressBar(Percent(done, total));
    } else {
        FuckOffUIProgressBar();
        CompleteNews();
    }
}

// 0x1017249c, as ReleaseLoginConnection on the news connection.
void CFSOnlineSystem::ReleaseNewsConnection() {
    if (mNews != nullptr) {
        mNews->Release();
        delete mNews;
        mNews = nullptr;
        SamsUtilities::UDebug("ONLINE NEWS: Releasing connection\n");
    }
}

// 0x1017253c. Ends the news job. A callback kept by the login is called with 0 and forgotten. With no error the news text is written
// to <data path><separator><override file> and loaded as the online balance overlay (SetInitialOnlineValues when it loaded), the
// connection is dropped and the news panel is made once; with an error the error dialog is shown, the connection dropped and
// the news words and texts put back to their start state.
void CFSOnlineSystem::CompleteNews() {
    if (mLoginCallbackKept != nullptr) {
        mLoginCallbackKept(0, mLoginContextKept);
        mLoginCallbackKept = nullptr;
        mLoginContextKept = nullptr;
    }
    if (mNewsFinished == 0) {
        TbDynamicStringTemplate<char> path(gTPFileInfo.GetDataPath());
        path += gTPGlobals.mSeparator;
        path += gTPGlobals.mF413C;
        bfl::FileSystem::CDiscFile file;
        file.Open(path.mData, 0xC0000010);
        int32_t length = mNewsText.Length();
        int32_t written = file.Write(mNewsText.mData, length);
        bool ok = written >= 0;
        file.Close();
        if (ok) {
            if (gOnlineBalance.Load(path.mData, CBalanceTable::kLoadOverlay) == 1) SetInitialOnlineValues();
        }
        if (mNews != nullptr) {
            mNews->Release();
            delete mNews;
            mNews = nullptr;
            SamsUtilities::UDebug("ONLINE NEWS: Releasing connection\n");
        }
        if (mF70 == 0) {
            CreateOnlineNewsPanel();
            mF70 = 1;
        }
    } else {
        AddUIError(mNewsFinished, nullptr);
        if (mNews != nullptr) {
            mNews->Release();
            delete mNews;
            mNews = nullptr;
            SamsUtilities::UDebug("ONLINE NEWS: Releasing connection\n");
        }
        mLoginCallbackKept = nullptr;
        mLoginContextKept = nullptr;
        mNewsRequest1 = 0xffffffffu;
        mNewsDone1 = 0;
        mNewsText = "";
        mNewsRequest2 = 0xffffffffu;
        mNewsDone2 = 0;
        mF98 = kNewsResetText;
        mNewsRequest3 = 0xffffffffu;
        mNewsDone3 = 0;
        mFB4 = kNewsResetText;
        mNewsFinished = 0;
    }
}

// CFSOnlineSystem::CityServerConnectionCallback 0x101727f8 (2252): the city server's answer to one of the nine list requests.

namespace {

// The release of one reference to a received answer (inlined on the Mac): the count drops, and at zero the infos are destroyed (last to
// first) and freed and the answer itself freed with operator delete only.
template <class R>
void DropReceived(R* received) {
    --received->mRefCount;
    if (received->mRefCount == 0) {
        if (received->mInfos != nullptr) delete[] received->mInfos;
        received->mInfos = nullptr;
        ::operator delete(static_cast<void*>(received));
    }
}

// The list takes the answer: the old one is released first (the pointer cleared), then the answer stored, the answer flags set.
template <class LIST, class R>
void Store(LIST* list, R* received) {
    R* old = list->mReceived;
    if (old != nullptr) {
        DropReceived(old);
        list->mReceived = nullptr;
    }
    list->mReceived = received;
    list->mF14 = 1;
    list->mF10 = 1;
}

// The failure: the list whose request id is the one answered gets the error code.
template <class LIST>
bool Fail(LIST* list, int32_t requestID, int32_t code) {
    if (requestID != list->mRequestID) return false;
    list->mF18 = code;
    list->mF14 = 0;
    list->mF10 = 1;
    return true;
}

}  // namespace

// a is the answer code (-10 to 8), b the request id, d the received answer (a counted block).
void CFSOnlineSystem::CityServerConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    (void)c;
    int32_t failed = 0;
    int32_t code = 0;
    uint32_t index = static_cast<uint32_t>(a + 10);
    if (index <= 0x12) {
        switch (a) {
        case 2: {   // the city list
            ccReceivedList<ccCityInfo>* received = static_cast<ccReceivedList<ccCityInfo>*>(d);
            if (b == static_cast<int32_t>(mCityList->mRequestID)) {
                SamsUtilities::UDebug("ONLINE CITY: City data downloaded ok\n");
                Store(mCityList, received);
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Strange unknown city list ignored\n");
                if (received != nullptr) DropReceived(received);
            }
            break;
        }
        case 3: {   // the park lists
            ccReceivedList<ccParkInfo>* received = static_cast<ccReceivedList<ccParkInfo>*>(d);
            if (b == mParksTop10->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Top 10 parks data downloaded ok\n");
                Store(mParksTop10, received);
            } else if (b == mParksInCity->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: All parks data downloaded ok\n");
                Store(mParksInCity, received);
            } else if (b == mParksByPlayer->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Player parks data downloaded ok\n");
                Store(mParksByPlayer, received);
            } else if (b == mParksByID->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: ParkInfo parks data downloaded ok\n");
                Store(mParksByID, received);
            } else if (b == mParksInCity2->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Chat Satellites parks data downloaded ok\n");
                Store(mParksInCity2, received);
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Strange unknown park data downloaded - ignoring\n");
                if (received != nullptr) DropReceived(received);
            }
            break;
        }
        case 5: {   // the theme list
            ccReceivedList<ccThemeInfo>* received = static_cast<ccReceivedList<ccThemeInfo>*>(d);
            if (b == mThemeList->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Theme list downloaded OK\n");
                Store(mThemeList, received);
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Strange unknown theme list ignored\n");
                if (received != nullptr) DropReceived(received);
            }
            break;
        }
        case 4: {   // the server list
            ccReceivedList<ccServerInfo>* received = static_cast<ccReceivedList<ccServerInfo>*>(d);
            if (b == mServerList->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Server list downloaded OK\n");
                Store(mServerList, received);
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Strange unknown server list ignored\n");
                if (received != nullptr) DropReceived(received);
            }
            break;
        }
        case 6: {   // the ride list
            ccReceivedList<ccRideInfo>* received = static_cast<ccReceivedList<ccRideInfo>*>(d);
            if (b == mRideList->mRequestID) {
                SamsUtilities::UDebug("ONLINE CITY: Ride list downloaded OK\n");
                Store(mRideList, received);
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Strange unknown ride list ignored\n");
                if (received != nullptr) DropReceived(received);
            }
            break;
        }
        case -10: code = 0x56; failed = 1; break;
        case -6: code = 0x4e; failed = 1; break;
        case -5: code = 0x4d; failed = 1; break;
        case -4: code = 0x4c; failed = 1; break;
        case -2: code = 0x4b; failed = 1; break;
        case -1: code = 0x37; failed = 1; break;
        case 7:
            SamsUtilities::UDebug("ONLINE CITY: CRC match\n");
            failed = 1;
            break;
        case 8:
            SamsUtilities::UDebug("ONLINE CITY: No CRC match\n");
            break;
        default:
            break;
        }
    }
    if (failed != 0) {
        if (Fail(mCityList, b, code)) return;
        if (Fail(mParksTop10, b, code)) return;
        if (Fail(mParksInCity, b, code)) return;
        if (Fail(mParksByPlayer, b, code)) return;
        if (Fail(mParksByID, b, code)) return;
        if (Fail(mParksInCity2, b, code)) return;
        if (Fail(mThemeList, b, code)) return;
        if (Fail(mServerList, b, code)) return;
        Fail(mRideList, b, code);
    }
}

// ccServerInfo::~ccServerInfo (0x101731dc, 220 bytes): frees the five strings (+0x08, +0x0c, +0x16, +0x1a, +0x22), each when not NULL, and
// clears the field after each. The Mac's destructor also takes the hidden "delete storage" short and calls operator delete when it is
// positive; the host's `delete` does that itself, so only the member work is here.

ccServerInfo::~ccServerInfo() {
    if (mStr08.mOffset != 0) free(const_cast<char16_t*>(mStr08.get()));
    mStr08.mOffset = 0;
    if (mStr0C.mOffset != 0) free(const_cast<char16_t*>(mStr0C.get()));
    mStr0C.mOffset = 0;
    if (mStr16.mOffset != 0) free(const_cast<char16_t*>(mStr16.get()));
    mStr16.mOffset = 0;
    if (mStr1A.mOffset != 0) free(const_cast<char16_t*>(mStr1A.get()));
    mStr1A.mOffset = 0;
    if (mStr22.mOffset != 0) free(const_cast<char16_t*>(mStr22.get()));
    mStr22.mOffset = 0;
}

// FSOnlineCityServerConnectionCallback 0x101733b0 (124): makes the online system when there is none (operator new(0x404) and the
// constructor, the pointer stored), then hands the four arguments to its CityServerConnectionCallback.

void FSOnlineCityServerConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    if (gOnlineSystem == nullptr) {
        gOnlineSystem = new CFSOnlineSystem();
    }
    gOnlineSystem->CityServerConnectionCallback(a, b, c, d);
}

// Unit v5961: seven small members of the online system.
//   UploadPostLoginCallbackFn 0x10174f2c, GotoPlayerFTPPostGetParkInfoCallbackFn 0x1017620c, CFSOnlineFTPControl::CallbackFn 0x1017769c:
//     each makes the online system when there is none (operator new(0x404) and the constructor) and forwards its arguments.
//   CFSOnlineSystem::ReturnConnectionToCityServer 0x1017346c, CFSOnlineSystemCityList::GetList 0x10178718,
//   ccThemeInfo::~ccThemeInfo 0x101732e4, ccRideInfo::~ccRideInfo 0x10173110.

// 0x1017346c: the city server connection, made and connected on first use. A failed connect releases it again (the answer is then
// the cleared word). The password text is the pointer at 0x28 (the string's data).
City* CFSOnlineSystem::ReturnConnectionToCityServer() {
    if (mCity == nullptr) {
        ReleaseCityServerConnection();
        mCity = new City;
        SamsUtilities::UDebug("ONLINE CITY: About to connect\n");
        int32_t connected = mCity->Connect(const_cast<char*>(gOnlineConfig.mCityServer.c_str()), nullptr,
                                           static_cast<uint16_t>(gOnlineConfig.mCityPort), FSOnlineCityServerConnectionCallback);
        if (connected == 1) {
            SamsUtilities::UDebug("ONLINE CITY: Connected\n");
            mCityClock = LbTime_GetClock();
            char16_t* password = mLoginPassword.mData;
            uint16_t language = static_cast<uint16_t>(gText.GetLanguageId());
            mCity->SetSessionInfo(language, mF34, mF38, password);
            mCityListsWanted = 1;
        } else {
            SamsUtilities::UDebug("ONLINE CITY: Could not connect\n");
            ReleaseCityServerConnection();
        }
    }
    return mCity;
}

// 0x101735cc. The clock word and the wanted flag, the city connection released, then Reset (inlined on the Mac) on the nine lists in the
// order of their offsets, 0xe0 to 0x100.
void CFSOnlineSystem::InitCity() {
    mCityClock = LbTime_GetClock();
    mCityListsWanted = 1;
    ReleaseCityServerConnection();
    mCityList->Reset();
    mParksTop10->Reset();
    mParksInCity->Reset();
    mParksByPlayer->Reset();
    mParksByID->Reset();
    mParksInCity2->Reset();
    mThemeList->Reset();
    mServerList->Reset();
    mRideList->Reset();
}

// 0x10173afc. With a city connection: the first time (the wanted flag InitCity set) the theme, server and ride lists and the second
// InCity list (city 1) are asked for. Then every list whose request is out and answered is completed (in the order theme, server, ride,
// the second InCity list, city, top ten, InCity, by player name, by park id). If any list still has a request out, the clock word is
// refreshed and the bar shows the request in flight (the text 0xf9 to 0x100 for the first of eight lists whose request id it equals;
// the park find panel also gets the percentage when it is the by-player-name list's); when none has, the bar goes and a connection that
// has been idle for more than 360000 clock ticks is released.
void CFSOnlineSystem::TickCity() {
    if (mCity == nullptr) return;
    if (mCityListsWanted != 0) {
        mThemeList->GetList();
        mServerList->GetList();
        mRideList->GetList();
        mParksInCity2->GetList(1, nullptr, nullptr);
        mCityListsWanted = 0;
    }
    int32_t pending = CompleteIfAnswered(mThemeList);
    pending |= CompleteIfAnswered(mServerList);
    pending |= CompleteIfAnswered(mRideList);
    pending |= CompleteIfAnswered(mParksInCity2);
    pending |= CompleteIfAnswered(mCityList);
    pending |= CompleteIfAnswered(mParksTop10);
    pending |= CompleteIfAnswered(mParksInCity);
    pending |= CompleteIfAnswered(mParksByPlayer);
    pending |= CompleteIfAnswered(mParksByID);
    if (pending == 0) {
        FuckOffUIProgressBar();
        const int32_t idle = static_cast<int32_t>(LbTime_GetClock() - mCityClock);
        if (360000 < idle) ReleaseCityServerConnection();
        return;
    }
    mCityClock = LbTime_GetClock();
    uint32_t id = mCity->GetCurrentMessage(0);
    if (id == 0) id = mCity->GetCurrentMessage(1);
    if (id == static_cast<uint32_t>(mCityList->mRequestID)) {
        CreateUIProgressBar(0xf9);
    } else if (id == static_cast<uint32_t>(mParksTop10->mRequestID)) {
        CreateUIProgressBar(0xfa);
    } else if (id == static_cast<uint32_t>(mParksInCity->mRequestID)) {
        CreateUIProgressBar(0xfb);
    } else if (id == static_cast<uint32_t>(mParksByPlayer->mRequestID)) {
        CreateUIProgressBar(0xfc);
    } else if (id == static_cast<uint32_t>(mParksInCity2->mRequestID)) {
        CreateUIProgressBar(0xfd);
    } else if (id == static_cast<uint32_t>(mThemeList->mRequestID)) {
        CreateUIProgressBar(0xfe);
    } else if (id == static_cast<uint32_t>(mServerList->mRequestID)) {
        CreateUIProgressBar(0xff);
    } else if (id == static_cast<uint32_t>(mRideList->mRequestID)) {
        CreateUIProgressBar(0x100);
    } else {
        FuckOffUIProgressBar();
    }
    int32_t done = 0;
    int32_t total = 0;
    mCity->MonitorMessage(id, &done, &total);
    if (0 < total) {
        const int32_t percent = Percent(done, total);
        UpdateUIProgressBar(percent);
        if (id == static_cast<uint32_t>(mParksByPlayer->mRequestID)) UpdateParkFindPanelProgress(percent);
    }
}

// 0x10174ac8, as ReleaseLoginConnection on the city connection.
void CFSOnlineSystem::ReleaseCityServerConnection() {
    if (mCity != nullptr) {
        mCity->Release();
        delete mCity;
        mCity = nullptr;
        SamsUtilities::UDebug("ONLINE CITY: Connection released\n");
    }
}

// 0x10174b6c
void SatelliteOnlineParkInfoPanelCancelCallback() { parkinfo_callee::DownloadGlobeSlot4c(gLobbyScreens->mDownloadGlobe); }

// 0x10174be8. A signed test of the index against the count of the second InCity list's park list; then the lobby's download globe is told
// through its virtual at slot 0x50 and the park info panel is raised with the satellite's two callbacks.
void CFSOnlineSystem::EnterChatSatellite(int32_t index) {
    if (index < mParksInCity2->mList->mCount) {
        CFSOnlinePark* park = mParksInCity2->ReturnParkInfo(index);
        gLobbyScreens->mDownloadGlobe->Slot50();
        CreateOnlineParkInfoPanel(park, nullptr, SatelliteOnlineParkInfoPanelCancelCallback, GeneralParkDownloadCompleteCallback, nullptr);
    }
}

// FS_Online.cpp (main app), the upload job and the city unpublish request (unit v5043):
//   UnpublishPark 0x10174ca0, RequestUpload 0x10174fe8, LoggedInRequestUpload 0x10175220,
//   UploadServerConnectionCallback 0x101753dc, ReturnUploadFilename 0x10175608.
// The words the Mac keeps as 32-bit pointers (the callback and context at 0x374 and 0x378, the text resource at 0x380) are int32_t
// members of the host class; a host pointer is truncated to them (the program's pointers are 32-bit addressable).

// 0x10174ca0. No city connection: one is made (a 0xc byte object) and connected to the city server of the online configuration; when
// that fails it is released and dropped, when it works the clock is taken and the session info (language, the two words at 0x34 and
// 0x38, the password) is sent and the lists are wanted. Then the request goes out whenever there is a connection. (The original's
// test for an existing connection ahead of the construction is dead code: it is only reached with no connection.)
void CFSOnlineSystem::UnpublishPark(int32_t park) {
    if (mCity == nullptr) {
        City* city = static_cast<City*>(::operator new(0xc));
        if (city != nullptr) new (city) City();
        mCity = city;
        SamsUtilities::UDebug("ONLINE CITY: About to connect\n");
        char* server = const_cast<char*>(gOnlineConfig.mCityServer.c_str());
        int32_t connected = mCity->Connect(server, nullptr, static_cast<uint16_t>(gOnlineConfig.mCityPort),
                                           FSOnlineCityServerConnectionCallback);
        if (connected == 1) {
            SamsUtilities::UDebug("ONLINE CITY: Connected\n");
            mCityClock = LbTime_GetClock();
            char16_t* password = mLoginPassword.mData;
            uint32_t language = gText.GetLanguageId();
            mCity->SetSessionInfo(static_cast<uint16_t>(language), mF34, mF38, password);
            mCityListsWanted = 1;
        } else {
            SamsUtilities::UDebug("ONLINE CITY: Could not connect\n");
            if (mCity != nullptr) {
                mCity->Release();
                mCity->~City();
                mCity = nullptr;
                SamsUtilities::UDebug("ONLINE CITY: Connection released\n");
            }
        }
    }
    if (mCity != nullptr) mCity->UnpublishPark(nullptr, park, mF34);
}

// 0x10174e4c. The flag is stored first; only when set, the text (or the pooled default when NULL) is assigned through the string's
// virtual operator=(const char*) (slot 0x20) and the 256 halfwords are copied to 0x120.
void CFSOnlineSystem::SetPublishing(int32_t publishing, const char* text, const uint16_t* words) {
    mF108 = publishing;
    if (publishing != 0) {
        TbDynamicStringTemplate<char>* string = &mF10C;
        asm volatile("" : "+r"(string));   // the Mac always calls through the vtable
        if (text != nullptr)
            *string = text;
        else
            *string = kDefaultPublishText;
        for (int32_t i = 0; i < 0x100; ++i) std::memcpy(mF120 + i * 2, &words[i], 2);
    }
}

// 0x10174f2c
void UploadPostLoginCallbackFn(int32_t error, void* context) {
    OnlineSystem()->RequestUploadPostLoginCallback(error, context);
}

// 0x10174fe8. Starts an upload job: the three texts go through Format (virtual slot 8), the numbers and the callback are kept. Logged
// in: the post login step runs at once on the online system (made when there is none). Not logged in: the login panel is raised with
// the upload's post login callback to run when it ends.
void CFSOnlineSystem::RequestUpload(const char* file, int32_t f344, const char16_t* name, const char16_t* description, int32_t f370,
                                    void (*callback)(int32_t, const void*), const void* context) {
    InitUpload();
    mF330.Format(file);
    mF344 = f344;
    mF348.Format(name);
    mF35C.Format(description);
    mF370 = f370;
    mF374 = PointerWord(reinterpret_cast<const void*>(callback));
    mF378 = PointerWord(context);
    if (mLoggedIn == 0) {
        mF320 = 1;
        mLoginCallback = UploadPostLoginCallbackFn;
        mLoginContext = nullptr;
        CreateOnlineLoginPanel(FSOnlineLoginRASFailureCallback);
    } else {
        if (gOnlineSystem == nullptr) {
            CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(::operator new(0x404));
            if (system != nullptr) new (system) CFSOnlineSystem();
            gOnlineSystem = system;
        }
        gOnlineSystem->RequestUploadPostLoginCallback(0, nullptr);
    }
}

// 0x10175154.
void CFSOnlineSystem::RequestUploadPostLoginCallback(int32_t result, const void* context) {
    (void)context;
    if (result != 0) {
        CompleteUpload(1);
    } else {
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
        gOnlineSystem->LoggedInRequestUpload();
    }
}

// 0x10175220. Bit 0x10 of the word at 0x58 (the child lock allows publishing) clear: error 0x4e. No upload connection and none can
// be made: error 0x4f. Otherwise the park's info is built (two texts, the 256 halfwords at 0x120, the word at 0x344, the theme id of
// the name at 0x114, the low byte of the word at 0x370) and the request goes out.
void CFSOnlineSystem::LoggedInRequestUpload() {
    uint32_t flags;
    std::memcpy(&flags, mF54 + 4, 4);
    if ((flags & 0x10) == 0) {
        SamsUtilities::UDebug("ONLINE UPLOAD: childlock forbids publishing\n");
        mF37C = 0x4e;
        mF380 = 0;
        CompleteUpload(1);
        return;
    }
    if (mUpload == nullptr && ConnectUploadServer() == 0) {
        SamsUtilities::UDebug("ONLINE UPLOAD: failed to create a connection to upload server\n");
        mF37C = 0x4f;
        mF380 = 0;
        CompleteUpload(1);
        return;
    }
    SamsUtilities::UDebug("ONLINE UPLOAD: Uploading\n");
    mUploadActive = 1;
    ccParkFileInfo info;
    info.mThemeID = 0;
    info.mFlag = 0;
    info.mWords[0] = 0;
    wcscpy(info.mName, mF348.mData);
    wcscpy(info.mDescription, mF35C.mData);
    for (int32_t i = 0; i < 0x100; i = i + 1) {
        uint16_t word;
        std::memcpy(&word, mF120 + i * 2, 2);
        info.mWords[i] = word;
    }
    info.mF00 = mF344;
    info.mThemeID = mThemeList->ReturnOnlineThemeID(mF10C.mData);
    info.mFlag = static_cast<uint8_t>(mF370);
    mUpload->UploadParkRequest(mF330.mData, &info, nullptr);
    SamsUtilities::UDebug("ONLINE UPLOAD: requesting now...\n");
}

// 0x101753dc. The upload server's answers. 0: the connection works. 0x50: the request was granted, so the context (use count at 8)
// loses a use and, at none, its buffer and itself; then the park file goes out. The errors are interface string ids in the word at
// 0x37c: -2 0x4b, -4 0x4c, -5 0x4d, -6 0x4e, -1 0x37, -0x14 0x55, -0x28 0xde (with the interface's resource word in 0x380); every one
// of them marks the job done and failed. Other answers do nothing.
void CFSOnlineSystem::UploadServerConnectionCallback(int32_t result, int32_t unused, void* unused2, void* context) {
    if (result == -4) {
        mF37C = 0x4c;
        mUploadFailed = 0;
        mUploadDone = 1;
    } else if (result < -4) {
        if (result == -0x14) {
            mF37C = 0x55;
            mUploadFailed = 0;
            mUploadDone = 1;
        } else if (result < -0x14) {
            if (result == -0x28) {
                mF37C = 0xde;
                mF380 = PointerWord(gText.mResourcePointers[6]);
                mUploadFailed = 0;
                mUploadDone = 1;
            }
        } else if (result == -6) {
            mF37C = 0x4e;
            mUploadFailed = 0;
            mUploadDone = 1;
        } else if (-7 < result) {
            mF37C = 0x4d;
            mUploadFailed = 0;
            mUploadDone = 1;
        }
    } else if (result == 0) {
        SamsUtilities::UDebug("ONLINE UPLOAD: file transfer ok\n");
        mUploadFailed = 1;
        mUploadDone = 1;
    } else if (result < 0) {
        if (result == -2) {
            mF37C = 0x4b;
            mUploadFailed = 0;
            mUploadDone = 1;
        } else if (-3 < result) {
            mF37C = 0x37;
            mUploadFailed = 0;
            mUploadDone = 1;
        }
    } else if (result == 0x50) {
        OnlineUploadContext* upload = static_cast<OnlineUploadContext*>(context);
        int32_t uses = upload->mRefCount - 1;
        upload->mRefCount = uses;
        if (uses == 0 && upload != nullptr) {
            if (upload->mBuffer != nullptr) free(upload->mBuffer);
            // volatile: the store is the original's, though the block is deleted right after
            *static_cast<void* volatile*>(&upload->mBuffer) = nullptr;
            ::operator delete(upload);
        }
        SamsUtilities::UDebug("ONLINE UPLOAD: upload request granted - uploading...\n");
        mUpload->UploadParkFile(mF330.mData, nullptr);
    }
}

// 0x10175608. The user's path with the theme (made from a user info made on first use), then the wide separator, the global tail
// string and the string at 0x4498 of the globals, copied to the answer.
TbDynamicStringTemplate<char16_t> CFSOnlineSystem::ReturnUploadFilename() {
    TbDynamicStringTemplate<char16_t> name;
    if (gUserInfo == nullptr) {
        CFSUserInfo* info = static_cast<CFSUserInfo*>(::operator new(0xd4));
        if (info != nullptr) new (info) CFSUserInfo();
        gUserInfo = info;
    }
    name = gUserInfo->ReturnUsersPathWithTheme();
    name += gTPGlobals.mWideSeparator;
    name += gUploadNameTail.mString;
    name += gTPGlobals.mF4498;
    return TbDynamicStringTemplate<char16_t>(name);
}

// 0x10175748
void FSOnlineUploadServerConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->UploadServerConnectionCallback(a, b, c, d);
}

// CFSOnlineSystem::ConnectUploadServer (0x10175808, 328 bytes) and CFSOnlineSystem::LoggedInRequestFTPDownload (0x10176418, 2084 bytes)
// (unit v5935).

namespace {

uintptr_t WordPointer_2(int32_t word) {
    return (reinterpret_cast<uintptr_t>(&AnchorFunction) & ~static_cast<uintptr_t>(0xFFFFFFFFu)) | static_cast<uint32_t>(word);
}

// 0x1024d220 (base + 0x61d): the Pascal string DebugStr is given for every file looked for.
const unsigned char kDebugStrText[] = {0x11, ' ', 'n', 'e', 'e', 'd', ' ', 't', 'o', ' ', 'f', 'i', 'x', ' ', 't', 'h', 'i', 's'};
// 0x1024d232 (base + 0x62f): appended to the download folder name.
const char kFolderSeparator[] = ":";
// 0x1024d392 (the wide table at 0x1024d390, plus 2): the wide literal appended after the ride's file name.
const char16_t kWideLiteral[] = u"p";

}  // namespace

int32_t CFSOnlineSystem::ConnectUploadServer() {
    ReleaseUploadServerConnection();
    mUpload = new Upload();
    SamsUtilities::UDebug("ONLINE UPLOAD: About to connect\n");
    TbDynamicStringTemplate<char16_t> wide(ftpreq_callee::ReturnUploadServer(mServerList)->mText24.mData);
    TbDynamicStringTemplate<char> narrow;
    UniStringToString(narrow, wide);
    int32_t connected = mUpload->Connect(narrow.mData, nullptr, static_cast<uint16_t>(ftpreq_callee::ReturnUploadServer(mServerList)->mF3C),
                                         ftpreq_callee::UploadConnectionCallback);
    int32_t result;
    if (connected != 1) {
        SamsUtilities::UDebug("ONLINE UPLOAD: Could not connect\n");
        result = 0;
    } else {
        SamsUtilities::UDebug("ONLINE UPLOAD: Connected\n");
        char16_t* password = mLoginPassword.mData;
        mUpload->SetSessionInfo(static_cast<int16_t>(gText.GetLanguageId()), mF34, mF38, password);
        result = 1;
    }
    return result;
}

// 0x1017598c. The library's Release only: the upload object is not deleted, only forgotten.
void CFSOnlineSystem::ReleaseUploadServerConnection() {
    if (mUpload != nullptr) {
        mUpload->Release();
        mUpload = nullptr;
    }
}

// 0x10175a18. The upload connection released (ReleaseUploadServerConnection, inlined on the Mac), the upload words and strings cleared.
void CFSOnlineSystem::InitUpload() {
    if (mUpload != nullptr) {
        mUpload->Release();
        mUpload = nullptr;
    }
    mF320 = 0;
    mUploadActive = 0;
    mUploadDone = 0;
    mUploadFailed = 0;
    mF344 = 0;
    mF348 = u"";
    mF35C = u"";
    mF370 = 0;
    mF374 = 0;
    mF378 = 0;
    mF37C = 0;
    mF380 = 0;
}

// 0x10175b00. With an upload running: until it finishes the bar shows text 0x101 and the upload's progress; when it has finished the bar
// goes, the running word is cleared and CompleteUpload runs with 1 when the result word is 0 (the upload failed) and 0 otherwise.
void CFSOnlineSystem::TickUpload() {
    if (mUpload == nullptr || mUploadActive == 0) return;
    if (mUploadDone == 0) {
        uint32_t id = mUpload->GetCurrentMessage(0);
        if (id == 0) id = mUpload->GetCurrentMessage(1);
        CreateUIProgressBar(0x101);
        int32_t done = 0;
        int32_t total = 0;
        mUpload->MonitorMessage(id, &done, &total);
        if (0 < total) UpdateUIProgressBar(Percent(done, total));
    } else {
        FuckOffUIProgressBar();
        mUploadActive = 0;
        CompleteUpload(mUploadFailed == 0);
    }
}

// 0x10175c20. A pending error (two words at 0x37c and 0x380) is shown and cleared; a failure or a shown error logs out when logged in
// (the word at 0x320); the caller's callback (0x374, context 0x378) is read before the logout and called after it; the upload connection
// is released.
void CFSOnlineSystem::CompleteUpload(int32_t failed) {
    bool errorShown = false;
    if (mF37C != 0 || mF380 != 0) {
        AddUIError(mF37C, reinterpret_cast<TbITable*>(WordPointer(mF380)));
        errorShown = true;
        mF37C = 0;
        mF380 = 0;
    }
    int32_t callback = mF374;
    int32_t context = mF378;
    if ((failed != 0 || errorShown) && mF320 != 0) Logout();
    if (static_cast<uint32_t>(callback) != 0) {
        typedef void (*Callback)(int32_t, const void*);
        Callback function = reinterpret_cast<Callback>(WordFunction(callback));
        function(failed, reinterpret_cast<const void*>(WordPointer(context)));
    }
    if (mUpload != nullptr) {
        mUpload->Release();
        mUpload = nullptr;
    }
}

// 0x10175d3c
void RequestFTPPostLoginCallbackFn(int32_t error, void* context) {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->RequestFTPPostLoginCallback(error, context);
}

// 0x10175dfc.
void CFSOnlineSystem::RequestFTPDownload(const CFSOnlinePark& park, void (*callback)(int32_t, const void*), const void* context) {
    StopProcessing();
    mF384 = 1;
    *mFTPPark = park;
    mF3C0 = PointerWord(reinterpret_cast<const void*>(callback));
    mF3C4 = PointerWord(context);
    if (mLoggedIn != 0) {
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
        gOnlineSystem->RequestFTPPostLoginCallback(0, nullptr);
    } else {
        mF388 = 1;
        mLoginCallback = RequestFTPPostLoginCallbackFn;
        mLoginContext = nullptr;
        CreateOnlineLoginPanel(FSOnlineLoginRASFailureCallback);
    }
}

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

// 0x10175fac
void GotoPlayerByIDFTPPostLoginCallbackFn(int32_t error, void* context) {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->GotoPlayerByIDFTPPostLoginCallback(error, context);
}

// 0x10176074.
void CFSOnlineSystem::GotoPlayerRequestFTPDownloadByID(int32_t parkID, void (*callback)(int32_t, const void*), const void* context) {
    StopProcessing();
    mF3A8 = parkID;
    mF3C0 = PointerWord(reinterpret_cast<const void*>(callback));
    mF3C4 = PointerWord(context);
    if (mLoggedIn != 0) {
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
        gOnlineSystem->GotoPlayerByIDFTPPostLoginCallback(0, nullptr);
    } else {
        mLoginCallback = GotoPlayerByIDFTPPostLoginCallbackFn;
        mLoginContext = nullptr;
        CreateOnlineLoginPanel(FSOnlineLoginRASFailureCallback);
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

// 0x1017620c: the park is the first one of the list (its head node's value).
void GotoPlayerFTPPostGetParkInfoCallbackFn(int32_t error, const void* context, TbPtrList<CFSOnlinePark>* list) {
    CFSOnlineSystem* system = OnlineSystem();
    system->GotoPlayerFTPPostGetParkInfoCallback(error, context, list->mHead->mData);
}

// 0x101762e8. The context is not used.
void CFSOnlineSystem::GotoPlayerFTPPostGetParkInfoCallback(int32_t result, const void* context, const CFSOnlinePark* park) {
    (void)context;
    if (result == 0 && park != nullptr)
        LoggedInRequestFTPDownload(park);
    else
        CompleteFTPDownload(1);
}

// 0x10176390: asks the by-park-id list for the park at mF3A8; the callback is GotoPlayerFTPPostGetParkInfoCallbackFn (TVector 0x10267450).
void CFSOnlineSystem::LoggedInGotoPlayerRequestFTPDownloadByID() {
    mParksByID->GetList(mF3A8, GotoPlayerFTPPostGetParkInfoCallbackFn, nullptr);
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

void CFSOnlineSystem::LoggedInRequestFTPDownload(const CFSOnlinePark* park) {
    if (park != nullptr) *mFTPPark = *park;
    ReleaseFTPDownloadServerConnection();

    // The child lock: bit 4 of the word at 0x58 allows a park with the word at 0x78 set, bit 8 the others; without the bit it is forbidden.
    uint32_t flags;
    std::memcpy(&flags, mF54 + 4, 4);
    bool allowed = false;
    if (mFTPPark->mF78 != 0) {
        if ((flags & 4) != 0) allowed = true;
    } else {
        if ((flags & 8) != 0) allowed = true;
    }
    if (!allowed) {
        SamsUtilities::UDebug("ONLINE FTPDOWNLOAD: childlock forbids downloading this park\n");
        mF398 = 0x4e;
        CompleteFTPDownload(1);
        return;
    }

    while (mF3C8.mCount != 0) {
        TbDynamicStringTemplate<char16_t>* name = ftpdl_callee::RemoveHead(&mF3C8);
        if (name != nullptr) delete name;
    }

    // The original's three character arrays on the stack are never written (zero here; the test clears the Mac's stack).
    char16_t buffer188[16] = {};
    char16_t buffer1a8[256] = {};
    char16_t buffer3a8[272] = {};

    const uint8_t* rideBits = mFTPPark->mF58;
    for (int32_t i = 0; i < 0x20; ++i) {
        for (int32_t bit = 0; bit < 8; ++bit) {
            if (((1 << bit) & 0xff & rideBits[i]) == 0) continue;
            CFSOnlineRide* ride = ftpreq_callee::ReturnRideInfo(mRideList, mFTPPark->mF4C, bit + (0x1f - i) * 8 + 1);
            TbDynamicStringTemplate<char16_t> filePath = [&]() {
                TbDynamicStringTemplate<char16_t> returned = ftpreq_callee::GetFilePath(ride);
                return TbDynamicStringTemplate<char16_t>(returned);
            }();
            DebugStr(kDebugStrText);
            TbDynamicStringTemplate<char> base(gTPFileInfo.GetLocalPath());
            FS_AddDirectorySeparator(base);
            base += gTPGlobals.mDataFolder;
            FS_AddDirectorySeparator(base);
            TbDynamicStringTemplate<char16_t> first;
            StringToUniString(first, base);
            first += buffer3a8;
            TbDynamicStringTemplate<char16_t> second(first);
            first += buffer1a8;
            first += buffer188;
            second += kWideLiteral;
            second += buffer1a8;
            second += buffer188;
            TbDynamicStringTemplate<char> firstName;
            UniStringToString(firstName, first);
            TbDynamicStringTemplate<char> secondName;
            UniStringToString(secondName, second);
            if (!LbFile_Exists(firstName.mData) && !LbFile_Exists(secondName.mData)) {
                TbDynamicStringTemplate<char16_t>* entry = new TbDynamicStringTemplate<char16_t>();
                *entry = buffer3a8;
                *entry += kWideLiteral;
                *entry += buffer1a8;
                *entry += buffer188;
                mF3C8.AddTail(entry);
            }
        }
    }

    if (gUserInfo == nullptr) {
        CFSUserInfo* info = static_cast<CFSUserInfo*>(::operator new(sizeof(CFSUserInfo)));
        ftpreq_callee::UserInfoCtor(info);
        gUserInfo = info;
    }
    TbDynamicStringTemplate<char16_t> onlinePath = [&]() {
        TbDynamicStringTemplate<char16_t> returned = gUserInfo->ReturnOnlinePath();
        return TbDynamicStringTemplate<char16_t>(returned);
    }();
    onlinePath += gTPGlobals.mWideSeparator;
    UniStringToString(mF3DC, onlinePath);
    TbDynamicStringTemplate<char> parkText(mFTPPark->mText7C.mData);
    TbDynamicStringTemplate<char> left(parkText.Left(0x13));
    TbDynamicStringTemplate<char> rest(parkText.Mid(0x13));
    mF3DC += left;
    mF3DC += kFolderSeparator;
    LbFile_CreateDirectory(mF3DC.mData);
    mF3DC += rest;
    mF3DC += gTPGlobals.mF4484;
    int32_t missing = LbFile_Exists(mF3DC.mData) ? 0 : 1;
    std::memcpy(mF3D8, &missing, 4);

    int32_t missingNow;
    std::memcpy(&missingNow, mF3D8, 4);
    if (missingNow == 0 && mF3C8.mCount == 0) {
        if (mF3C0 != 0) {
            typedef void (*Callback)(int32_t, const void*);
            Callback function = reinterpret_cast<Callback>(WordFunction(mF3C0));
            function(0, reinterpret_cast<const void*>(WordPointer_2(mF3C4)));
        }
        int32_t parkId = mFTPPark->mParkID;
        if (mCity == nullptr) {
            mCity = new City();
            SamsUtilities::UDebug("ONLINE CITY: About to connect\n");
            int32_t connected = mCity->Connect(const_cast<char*>(gOnlineConfig.mCityServer.c_str()), nullptr,
                                               static_cast<uint16_t>(gOnlineConfig.mCityPort), FSOnlineCityServerConnectionCallback);
            if (connected == 1) {
                SamsUtilities::UDebug("ONLINE CITY: Connected\n");
                mCityClock = LbTime_GetClock();
                char16_t* password = mLoginPassword.mData;
                mCity->SetSessionInfo(static_cast<uint16_t>(gText.GetLanguageId()), mF34, mF38, password);
                mCityListsWanted = 1;
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Could not connect\n");
                if (mCity != nullptr) {
                    mCity->Release();
                    delete mCity;
                    mCity = nullptr;
                    SamsUtilities::UDebug("ONLINE CITY: Connection released\n");
                }
            }
        }
        if (mCity != nullptr) mCity->MarkParkVisited(nullptr, parkId, mF34);
        mFTPParkPending = 1;
    } else {
        unsigned char* control = static_cast<unsigned char*>(::operator new(4));
        if (control != nullptr) {
            uint32_t vptr = 0x102a72c8;   // the constructor stores 0x102a72d4 first, then this
            std::memcpy(control, &vptr, 4);
        }
        mF39C = control;
        mFTP = new FTPClient();
        mFTP->Construct(static_cast<CFTPControl*>(mF39C), true);
        SamsUtilities::UDebug("ONLINE FTPDOWNLOAD: About to connect\n");
        TbDynamicStringTemplate<char16_t> serverWide(ftpdl_callee::ReturnFTPDownloadServer(mServerList)->mText24.mData);
        TbDynamicStringTemplate<char> serverName;
        UniStringToString(serverName, serverWide);
        TbDynamicStringTemplate<char16_t> emailWide(mLoginEmail.mData);
        TbDynamicStringTemplate<char> emailName;
        UniStringToString(emailName, emailWide);
        mFTPActive = 1;
        mFTP->Connect(serverName.mData, const_cast<char*>("anonymous"), emailName.mData, nullptr);
    }
}

// 0x10176cec. The FTP connection released, the download words cleared and the text emptied.
void CFSOnlineSystem::InitFTPDownload() {
    ReleaseFTPDownloadServerConnection();
    mF384 = 0;
    mF388 = 0;
    mFTPActive = 0;
    mFTPDone = 0;
    mFTPResult = 0;
    mF398 = 0;
    mF3A8 = 0;
    mF3C0 = 0;
    mF3C4 = 0;
    mF3DC = "";
    mFTPParkPending = 0;
}

// 0x10176da4. With a download running (the buffer, the FTP client and the running word all set): until it finishes the bar shows text
// 0x102 and the client's bytes received over bytes expected; when it has finished the bar goes, the running word is cleared and
// CompleteFTPDownload runs with 1 when the result word is 0 (the download failed) and 0 otherwise.
void CFSOnlineSystem::TickFTPDownload() {
    if (mF39C == nullptr || mFTP == nullptr || mFTPActive == 0) return;
    if (mFTPDone == 0) {
        CreateUIProgressBar(0x102);
        const int32_t total = mFTP->mTotal;
        const int32_t done = mFTP->mDone;
        if (0 < total) UpdateUIProgressBar(Percent(done, total));
    } else {
        FuckOffUIProgressBar();
        mFTPActive = 0;
        CompleteFTPDownload(mFTPResult == 0);
    }
}

// The FTP download job of the online system: its end (CompleteFTPDownload), the download client's callback and the request of the next
// file, and the accessor of the one system object (unit v5655). Companion of src/FS_Online_init.cpp.

// The characters the download paths append (the Mac keeps them in one literal of slashes at 0x102a72a4).
char16_t gFTPDownloadSlashA = u'/';   // 0x102a72a4
char16_t gFTPDownloadSlashB = u'/';   // 0x102a72a6
char16_t gFTPDownloadSlashC = u'/';   // 0x102a72a8

// Weak defaults of the two callees the test fakes (the Mac's functions are not translated into a file of their own).
namespace ftpdl_callee {
__attribute__((weak)) CFSOnlineServer* ReturnFTPDownloadServer(CFSOnlineSystemServerList*) { return nullptr; }
__attribute__((weak)) TbDynamicStringTemplate<char16_t>* RemoveHead(TbPtrList<TbDynamicStringTemplate<char16_t>>* list) {
    return list->RemoveHead();
}
}  // namespace ftpdl_callee

// 0x10176e90. The end of the download job. A pending error id (word 0x398) goes to the interface and is cleared. A failed job (or one
// with an error) logs the player out when the word at 0x388 says the login was made for it. A good one makes the city server
// connection if there is none (the log has the steps), marks the park visited and flags a park waiting to be played. Then the caller's
// callback gets the result, the download connection is released and the word at 0x384 cleared.
void CFSOnlineSystem::CompleteFTPDownload(int32_t failed) {
    bool errorShown = false;
    if (mF398 != 0) {
        AddUIError(mF398, nullptr);
        mF398 = 0;
        errorShown = true;
    }
    int32_t callback = mF3C0;
    int32_t context = mF3C4;
    if (failed != 0 || errorShown) {
        if (mF388 != 0) Logout();
    } else {
        int32_t park = mFTPPark->mParkID;
        if (mCity == nullptr) {
            mCity = new City();
            SamsUtilities::UDebug("ONLINE CITY: About to connect\n");
            int32_t connected = mCity->Connect(const_cast<char*>(gOnlineConfig.mCityServer.c_str()), nullptr,
                                               static_cast<uint16_t>(gOnlineConfig.mCityPort), FSOnlineCityServerConnectionCallback);
            if (connected == 1) {
                SamsUtilities::UDebug("ONLINE CITY: Connected\n");
                mCityClock = LbTime_GetClock();
                char16_t* password = mLoginPassword.mData;
                mCity->SetSessionInfo(static_cast<uint16_t>(gText.GetLanguageId()), mF34, mF38, password);
                mCityListsWanted = 1;
            } else {
                SamsUtilities::UDebug("ONLINE CITY: Could not connect\n");
                if (mCity != nullptr) {
                    mCity->Release();
                    delete mCity;
                    mCity = nullptr;
                    SamsUtilities::UDebug("ONLINE CITY: Connection released\n");
                }
            }
        }
        if (mCity != nullptr) mCity->MarkParkVisited(nullptr, park, mF34);
        mFTPParkPending = 1;
    }
    if (static_cast<uint32_t>(callback) != 0) {
        typedef void (*Callback)(int32_t, const void*);
        Callback function = reinterpret_cast<Callback>(WordFunction(callback));
        function(failed, reinterpret_cast<const void*>(WordPointer(context)));
    }
    ReleaseFTPDownloadServerConnection();
    mF384 = 0;
}

// 0x101770dc. The message is logged even when nothing is connected. The FTP object is forgotten first and then deleted through its
// virtual destructor (slot +8, flag 1); the buffer at 0x39c is freed with operator delete only.
void CFSOnlineSystem::ReleaseFTPDownloadServerConnection() {
    SamsUtilities::UDebug("ONLINE FTPDOWNLOAD: Reseting\n");
    FTPClient* client = mFTP;
    if (client != nullptr) {
        mFTP = nullptr;
        delete client;
    }
    if (mF39C != nullptr) {
        ::operator delete(mF39C);
        mF39C = nullptr;
    }
}

// 0x101771a4. What the download client reports: 0 nothing; 1 and 3 with no error ask for the next file (1 says "connected", 3 the
// other message), with an error they keep it and end the job as failed; 2 keeps an error and ends the job (the result word is left);
// every other state keeps an error and ends the job as failed. Nothing happens without a client.
void CFSOnlineSystem::FTPDownloadCallbackFn(int32_t state, int32_t error) {
    if (mFTP == nullptr) return;
    if (state == 2) {
        if (error != 0) mF398 = error;
        mFTPDone = 1;
    } else if (state < 2) {
        if (state == 0) return;
        if (state >= 0) {
            if (error == 0) {
                SamsUtilities::UDebug("ONLINE FTPDOWNLOAD: Connected\n");
                RequestNextFileForFTPDownload();
                return;
            }
            mF398 = error;
            mFTPResult = 0;
            mFTPDone = 1;
            return;
        }
        if (error != 0) mF398 = error;
        mFTPResult = 0;
        mFTPDone = 1;
    } else if (state != 4 && state < 4) {
        if (error == 0) {
            SamsUtilities::UDebug("ONLINE FTPDOWNLOAD: Download complete\n");
            RequestNextFileForFTPDownload();
            return;
        }
        mF398 = error;
        mFTPResult = 0;
        mFTPDone = 1;
    } else {
        if (error != 0) mF398 = error;
        mFTPResult = 0;
        mFTPDone = 1;
    }
}

// 0x101772ec. The next file of the download. With names waiting in the list (count at 0x3d4) the head is taken: the remote name is
// <the FTP server's name><slash><name>, the local one <local path><separator><data folder><separator><name>; the name is deleted
// after the request. With none waiting but the park's own file due (word 0x3d8) the remote name is <server><slash><city's name>
// <slash><park's file>, saved to the path at 0x3dc, and the due word is cleared. With nothing left the client is disconnected and
// the job is marked finished (result word 1).
void CFSOnlineSystem::RequestNextFileForFTPDownload() {
    if (mF3C8.mCount != 0) {
        SamsUtilities::UDebug("ONLINE FTPDOWNLOAD: Requesting next ride\n");
        TbDynamicStringTemplate<char16_t>* name = ftpdl_callee::RemoveHead(&mF3C8);
        TbDynamicStringTemplate<char16_t> remote;
        remote = ftpdl_callee::ReturnFTPDownloadServer(mServerList)->mText6C.mData;
        remote += gFTPDownloadSlashA;
        remote += *name;
        TbDynamicStringTemplate<char> remoteNarrow;
        UniStringToString(remoteNarrow, remote);
        TbDynamicStringTemplate<char> local(gTPFileInfo.GetLocalPath());
        FS_AddDirectorySeparator(local);
        local += gTPGlobals.mDataFolder;
        FS_AddDirectorySeparator(local);
        TbDynamicStringTemplate<char16_t> localWide;
        StringToUniString(localWide, local);
        localWide += *name;
        UniStringToString(local, localWide);
        mFTP->Download(remoteNarrow.mData, local.mData);
        if (name != nullptr) delete name;
    } else {
        int32_t due;
        std::memcpy(&due, mF3D8, sizeof(due));
        if (due != 0) {
            SamsUtilities::UDebug("ONLINE FTPDOWNLOAD: Requesting park\n");
            TbDynamicStringTemplate<char16_t> remote;
            remote = ftpdl_callee::ReturnFTPDownloadServer(mServerList)->mText6C.mData;
            remote += gFTPDownloadSlashB;
            remote += park_info_callee::ReturnCityInfo(mCityList, mFTPPark->mF48)->mFtpName;
            remote += gFTPDownloadSlashC;
            TbDynamicStringTemplate<char> remoteNarrow;
            UniStringToString(remoteNarrow, remote);
            remoteNarrow += mFTPPark->mText7C.mData;
            mFTP->Download(remoteNarrow.mData, mF3DC.mData);
            int32_t zero = 0;
            std::memcpy(mF3D8, &zero, sizeof(zero));
        } else {
            SamsUtilities::UDebug("ONLINE FTPDOWNLOAD: All downloads complete - disconnecting\n");
            mFTP->Disconnect();
            mFTPResult = 1;
        }
    }
}

// Unit v51065: the online lists' small members, the park lists' GetList for the top ten and the by-park-id lists, the login callbacks,
// two destructors (the city list's and the theme list's base), the park instance of LoggedInGetList and the FTP download getters.

char* CFSOnlineSystem::ReturnFTPDownloadLocalPath() { return mF3DC.mData; }

// 0x10177654 (8) and 0x10177608 (8): the word at 0x3a4 and the string data at 0x3e4 (the string at 0x3dc, its data at +8).
CFSFTPParkInfo* CFSOnlineSystem::GetFTPDownloadParkInfo() { return reinterpret_cast<CFSFTPParkInfo*>(mFTPPark); }
// 0x10170e20 (300): the city list's destructor with the base destructor inlined: the stored answer released, the list drained (each
// record destroyed through its virtual destructor with the delete flag), the two words cleared and the list freed. The Mac then frees the
// object itself for a flag above 0 (the deleting form does that here). The Mac checks `this` for NULL; a caller does that here.
// Host only (v5L17): the pointer is cleared after the delete. The host runs the base's destructor (0x101701c8, src/FS_OnlineList_dtors.cpp)
// after this one, and it skips its work on a NULL list; otherwise it read the freed list and deleted it again.
CFSOnlineSystemCityList::~CFSOnlineSystemCityList() {
    ReleaseReceived(this);
    while (mList->GetCount() != 0) {
        CFSOnlineCity* city = mList->RemoveHead();
        if (city != nullptr) gCityRecordDeleter(city);
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) delete mList;
    mList = nullptr;   // host only: the base destructor runs after this one (see above)
}

// 0x1017769c: `this` is not read.
void CFSOnlineFTPControl::CallbackFn(int32_t state, int32_t error) {
    OnlineSystem()->FTPDownloadCallbackFn(state, error);
}

// 0x101732e4 (the destructor with the hidden delete flag, r4 as a signed 16 bit value): frees the three blocks, clears the words and,
// for a flag above 0, deletes the object. NULL is allowed.
ccThemeInfo* ccThemeInfo::Destruct(ccThemeInfo* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mStr04.mOffset != 0) v5917_callee::Free(const_cast<char16_t*>(self->mStr04.get()));
        self->mStr04.mOffset = 0;
        if (self->mStr08.mOffset != 0) v5917_callee::Free(const_cast<char16_t*>(self->mStr08.get()));
        self->mStr08.mOffset = 0;
        if (self->mStr0C.mOffset != 0) v5917_callee::Free(const_cast<char16_t*>(self->mStr0C.get()));
        self->mStr0C.mOffset = 0;
        if (deleteFlag > 0) v5917_callee::OperatorDelete(self);
    }
    return self;
}
ccThemeInfo::~ccThemeInfo() { Destruct(this, 0); }

// 0x10173110: the same for the ride info (blocks at 4, 8 and 0x24).
ccRideInfo* ccRideInfo::Destruct(ccRideInfo* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mStr04.mOffset != 0) v5917_callee::Free(const_cast<char16_t*>(self->mStr04.get()));
        self->mStr04.mOffset = 0;
        if (self->mStr08.mOffset != 0) v5917_callee::Free(const_cast<char16_t*>(self->mStr08.get()));
        self->mStr08.mOffset = 0;
        if (self->mStr24.mOffset != 0) v5917_callee::Free(const_cast<char16_t*>(self->mStr24.get()));
        self->mStr24.mOffset = 0;
        if (deleteFlag > 0) v5917_callee::OperatorDelete(self);
    }
    return self;
}
ccRideInfo::~ccRideInfo() { Destruct(this, 0); }

// Unit v51125: small leaves of the main app (see include/std_exception_what.h).

// ---- the callees outside the unit: weak defaults (the real calls) that the unit's test library replaces with recording fakes.
#define TPW_WEAK __attribute__((weak))
template<class T>
static T RemoveHeadBody(TbTList<T, T>* list) {
    if (list->mHead == nullptr) return nullptr;
    auto* node = list->mHead;
    list->mHead = node->mNext;
    if (list->mHead == nullptr) list->mTail = nullptr;
    else list->mHead->mPrev = nullptr;
    --list->mCount;
    T value = node->mData;
    delete node;
    return value;
}
template<> TPW_WEAK CFSOnlinePark* TbTList<CFSOnlinePark*, CFSOnlinePark*>::RemoveHead() { return RemoveHeadBody(this); }
template<> TPW_WEAK CFSOnlineCity* TbTList<CFSOnlineCity*, CFSOnlineCity*>::RemoveHead() { return RemoveHeadBody(this); }

// CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::CompleteGetList 0x10174410 (468): the ride instance of the city's 0x10173f98.

typedef CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList> RideBase;

void (*gRideRecordDeleter)(CFSOnlineRide* ride) = [](CFSOnlineRide* ride) { delete ride; };

template<> void RideBase::CompleteGetList(int32_t error) {
    bool hadError = mF18 != 0;
    if (hadError) {
        AddUIError(mF18, nullptr);
        mF18 = 0;
    }
    int32_t callback = mF1C;
    int32_t context = mF20;
    if (error == 0 && !hadError && mF14 != 0) {
        while (mList->GetCount() != 0) {
            CFSOnlineRide* ride = mList->RemoveHead();
            if (ride != nullptr) gRideRecordDeleter(ride);
        }
        mF24 = 0;
        mF28 = 0;
        mF28 = 1;
        if (mReceived != nullptr) {
            StoreReceivedList(mReceived);
            mF24 = mReceived->mF08;
            ccRideList* received = mReceived;
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

// 0x10178308. The static initialiser of FS_Online.cpp: two global wide strings are built from literals (at 0x1024d390 + 6 and + 0x14)
// and each is registered with its destructor for the exit-time teardown.
static const char16_t kLiteralA[] = u"upload";   // 0x1024d396
static const char16_t kLiteralB[] = u"gms.dat";   // 0x1024d3a4

void sinit_FS_Online_cpp() {
    new (&gUploadNameTail.mString) TbDynamicStringTemplate<char16_t>(kLiteralA);
    __register_global_object(&gUploadNameTail, reinterpret_cast<void*>(&UIMain_DestroyWString), gOnlineSinitNodeA);
    new (&gOnlineSinitString2.mString) TbDynamicStringTemplate<char16_t>(kLiteralB);
    __register_global_object(&gOnlineSinitString2, reinterpret_cast<void*>(&UIMain_DestroyWString), gOnlineSinitNodeB);
}
