// FS_Online.cpp (main app), the upload job and the city unpublish request (unit v5043):
//   UnpublishPark 0x10174ca0, RequestUpload 0x10174fe8, LoggedInRequestUpload 0x10175220,
//   UploadServerConnectionCallback 0x101753dc, ReturnUploadFilename 0x10175608.
// The words the Mac keeps as 32-bit pointers (the callback and context at 0x374 and 0x378, the text resource at 0x380) are int32_t
// members of the host class; a host pointer is truncated to them (the program's pointers are 32-bit addressable).
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

int32_t PointerWord(const void* pointer) { return static_cast<int32_t>(reinterpret_cast<intptr_t>(pointer)); }

}  // namespace

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
