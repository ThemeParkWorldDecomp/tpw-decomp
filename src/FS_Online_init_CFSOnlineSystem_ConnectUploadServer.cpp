// CFSOnlineSystem::ConnectUploadServer (0x10175808, 328 bytes) and CFSOnlineSystem::LoggedInRequestFTPDownload (0x10176418, 2084 bytes)
// (unit v5935).
#include <cstdint>
#include <cstring>
#include <new>

#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "LbFile.h"
#include "LbTime.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPString.h"
#include "Text.h"
#include "platform/samsutils.h"

namespace {

// The function-pointer and pointer words of the Mac object are 32 bits; the host rebuilds the pointer from the high half of
// a function of this library (the same device as CompleteFTPDownload).
void AnchorFunction() {}

uintptr_t WordFunction(int32_t word) {
    return (reinterpret_cast<uintptr_t>(&AnchorFunction) & ~static_cast<uintptr_t>(0xFFFFFFFFu)) | static_cast<uint32_t>(word);
}

uintptr_t WordPointer(int32_t word) {
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
            function(0, reinterpret_cast<const void*>(WordPointer(mF3C4)));
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
