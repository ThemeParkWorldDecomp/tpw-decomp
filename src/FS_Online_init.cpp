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
#include <new>

#include "FS_Online.h"
#include "LbTime.h"
#include "platform/samsutils.h"

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

// 0x1017249c, as ReleaseLoginConnection on the news connection.
void CFSOnlineSystem::ReleaseNewsConnection() {
    if (mNews != nullptr) {
        mNews->Release();
        delete mNews;
        mNews = nullptr;
        SamsUtilities::UDebug("ONLINE NEWS: Releasing connection\n");
    }
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

// 0x10174ac8, as ReleaseLoginConnection on the city connection.
void CFSOnlineSystem::ReleaseCityServerConnection() {
    if (mCity != nullptr) {
        mCity->Release();
        delete mCity;
        mCity = nullptr;
        SamsUtilities::UDebug("ONLINE CITY: Connection released\n");
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

// 0x1017598c. The library's Release only: the upload object is not deleted, only forgotten.
void CFSOnlineSystem::ReleaseUploadServerConnection() {
    if (mUpload != nullptr) {
        mUpload->Release();
        mUpload = nullptr;
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

// 0x10171adc. A connected login that has finished is completed once (CompleteLogin(0)) and its flag cleared.
void CFSOnlineSystem::TickLogin() {
    if (mLogin != nullptr && mLoginDone != 0) {
        CompleteLogin(0);
        mLoginDone = 0;
    }
}
