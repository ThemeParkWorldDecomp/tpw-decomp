// Unit v5130 (main app, FS_Online.cpp): the online system's chat satellite, publishing words, the upload and FTP download entry points
// and AreWeProcessing.
#include <cstdint>
#include <cstring>

#include "FS_Online.h"
#include "lobby.h"

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

// 0x10174be8. A signed test of the index against the count of the second InCity list's park list; then the lobby's download globe is told
// through its virtual at slot 0x50 and the park info panel is raised with the satellite's two callbacks.
void CFSOnlineSystem::EnterChatSatellite(int32_t index) {
    if (index < mParksInCity2->mList->mCount) {
        CFSOnlinePark* park = mParksInCity2->ReturnParkInfo(index);
        gLobbyScreens->mDownloadGlobe->Slot50();
        CreateOnlineParkInfoPanel(park, nullptr, SatelliteOnlineParkInfoPanelCancelCallback, GeneralParkDownloadCompleteCallback, nullptr);
    }
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

// 0x101762e8. The context is not used.
void CFSOnlineSystem::GotoPlayerFTPPostGetParkInfoCallback(int32_t result, const void* context, const CFSOnlinePark* park) {
    (void)context;
    if (result == 0 && park != nullptr)
        LoggedInRequestFTPDownload(park);
    else
        CompleteFTPDownload(1);
}

// 0x10171498. 1 when any connection is open, any of the nine lists has a request out, or the upload or FTP download runs.
int32_t CFSOnlineSystem::AreWeProcessing() {
    int32_t result = 0;
    if (mLogin != nullptr || mNews != nullptr || mCityList->mF0C != 0 || mParksTop10->mF0C != 0 || mParksInCity->mF0C != 0 ||
        mParksByPlayer->mF0C != 0 || mParksByID->mF0C != 0 || mParksInCity2->mF0C != 0 || mThemeList->mF0C != 0 ||
        mServerList->mF0C != 0 || mRideList->mF0C != 0 || mUploadActive != 0 || mFTPActive != 0)
        result = 1;
    return result;
}
