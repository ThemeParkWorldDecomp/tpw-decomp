// Unit v5961: seven small members of the online system.
//   UploadPostLoginCallbackFn 0x10174f2c, GotoPlayerFTPPostGetParkInfoCallbackFn 0x1017620c, CFSOnlineFTPControl::CallbackFn 0x1017769c:
//     each makes the online system when there is none (operator new(0x404) and the constructor) and forwards its arguments.
//   CFSOnlineSystem::ReturnConnectionToCityServer 0x1017346c, CFSOnlineSystemCityList::GetList 0x10178718,
//   ccThemeInfo::~ccThemeInfo 0x101732e4, ccRideInfo::~ccRideInfo 0x10173110.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "LbTime.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "Text.h"
#include "platform/msl.h"
#include "platform/samsutils.h"

namespace {

// A callback or context the Mac keeps in a 32-bit word.
int32_t PointerWord(const void* p) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(p)); }

// The Mac's "if (gOnlineSystem == 0) make it", inlined at every use.
CFSOnlineSystem* OnlineSystem() {
    if (gOnlineSystem == nullptr) {
        CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(::operator new(0x404));
        if (system != nullptr) new (system) CFSOnlineSystem();
        gOnlineSystem = system;
    }
    return gOnlineSystem;
}

}  // namespace

// 0x10174f2c
void UploadPostLoginCallbackFn(int32_t error, void* context) {
    OnlineSystem()->RequestUploadPostLoginCallback(error, context);
}

// 0x1017620c: the park is the first one of the list (its head node's value).
void GotoPlayerFTPPostGetParkInfoCallbackFn(int32_t error, const void* context, TbPtrList<CFSOnlinePark>* list) {
    CFSOnlineSystem* system = OnlineSystem();
    system->GotoPlayerFTPPostGetParkInfoCallback(error, context, list->mHead->mData);
}

// 0x1017769c: `this` is not read.
void CFSOnlineFTPControl::CallbackFn(int32_t state, int32_t error) {
    OnlineSystem()->FTPDownloadCallbackFn(state, error);
}

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
