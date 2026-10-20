// The FTP download job of the online system: its end (CompleteFTPDownload), the download client's callback and the request of the next
// file, and the accessor of the one system object (unit v5655). Companion of src/FS_Online_init.cpp.
#include <cstring>
#include <new>

#include "FS_Online.h"
#include "FS_System.h"
#include "LbTime.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPString.h"
#include "Text.h"
#include "platform/samsutils.h"

// The characters the download paths append (the Mac keeps them in one literal of slashes at 0x102a72a4).
char16_t gFTPDownloadSlashA = u'/';   // 0x102a72a4
char16_t gFTPDownloadSlashB = u'/';   // 0x102a72a6
char16_t gFTPDownloadSlashC = u'/';   // 0x102a72a8

namespace {
// The words the system keeps callback pointers in (the Mac's 32-bit pointers; the program's code sits in one 4 GB window).
void AnchorFunction() {}
uintptr_t WordFunction(int32_t word) {
    return (reinterpret_cast<uintptr_t>(&AnchorFunction) & ~static_cast<uintptr_t>(0xFFFFFFFFu)) | static_cast<uint32_t>(word);
}
uintptr_t WordPointer(int32_t word) { return static_cast<uintptr_t>(static_cast<uint32_t>(word)); }
}  // namespace

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

// 0x10178618. The one online system, made on first use.
CFSOnlineSystem* CFSOnlineSystem::Instance() {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    return gOnlineSystem;
}
