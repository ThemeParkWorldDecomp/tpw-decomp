// Unit v51129: std::basic_string<wchar_t>::assign (0x101b2510), the horizontal gradient's destructor (0x10189c0c), the ride list's GetList
// (0x1017b938), the server list's base destructor (0x1017a6e4), the theme list's LoggedInGetList (0x10179f50) and destructor (0x10179d28).
#include "ui_Postcard.h"
#include <cstring>
#include <new>
#include "Postcard.h"
#include "Game.h"
#include "FS_Online.h"
#include "Text.h"
#include "TbITable.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "UIInterface.h"
#include "UI_pump.h"
#include "ui_main.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include "HLSprite.h"
#include "Advisor.h"
#include "tb_ddraw.h"
#include "tb_resource_texture.h"
#include "ui_ime.h"
#include "std_basic_streambuf_wchar.h"
#include "ui_DrawObject.h"
#include "TPEngine.h"
#include "FS_OnlineList.h"

// 0x101b2510 (108): make room for `length` characters (change_size, not keeping the old ones), then copy the text into the string's
// representation, which change_size may have replaced (the pointer is read again). Answers this on the Mac.
void LCString::assign(const char16_t* text, uint32_t length) {
    IME::MslWideString_changeSize(reinterpret_cast<IME::MslWideString*>(this), length, false);
    wmemcpy(mRep->mData, text, length);
}

// 0x10189c0c (140): the vtable is stored, the textures and the mesh are freed when the mesh was made (mData set), then the base's
// destructor runs. The delete flag (r4) is the C++ delete.
InterfaceDrawEngineHGradient::~InterfaceDrawEngineHGradient() {
    if (mData != nullptr) {
        DMFreeTextures(&mTextures);
        DMFreeMesh(&mMeshSet);
    }
}

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

// 0x1017a6e4 (288): the server list's destructor body (the vtable store aside): the stored answer released, the records removed and
// destroyed, the list destroyed. The delete flag is the C++ delete.
// Lead 2026-10-20 (v51129): weak, because src/CFSOnlineSystemServerList_RequestListFromServer_CFSOnlineSystemServerList_ReturnFTPDownloadServe.cpp
// (v51110) defines the same specialization for the derived destructor 0x1017a908, the same code; libtpw links that one, the unit
// test library takes this one.
template<> __attribute__((weak)) CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::~CFSOnlineSystemList() {
    if (mReceived != nullptr) {
        --mReceived->mRefCount;
        if (mReceived->mRefCount == 0) {
            if (mReceived->mInfos != nullptr) onlinelist2_callee::DestroyServerInfos(mReceived->mInfos);
            mReceived->mInfos = nullptr;
            onlinelist2_callee::FreeBlock(mReceived);
        }
        mReceived = nullptr;
    }
    while (mList->GetCount() != 0) {
        CFSOnlineServer* server = onlinelist2_callee::ServerRemoveHead(mList);
        if (server != nullptr) onlinelist2_callee::DestroyServer(server);
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) onlinelist2_callee::DestroyServerList(mList);
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
