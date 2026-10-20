// FS_Online_tick.cpp (main app, unit w69): the online system's four state tickers (news, city, upload, FTP download), CompleteLogin and
// AreWeLoggedIn. Each ticker follows one online job while it runs (the interface's progress bar shows its request) and hands the job to its
// Complete* member once it is done; the job completions themselves are not translated yet.
//
//   0x1017232c CFSOnlineSystem::TickNews (320)          0x10173afc TickCity (1132)
//   0x10175b00 TickUpload (236)                         0x10176da4 TickFTPDownload (180)
//   0x10171c08 CompleteLogin (444)                      0x1017178c AreWeLoggedIn (8)
#include "FS_Online.h"
#include "LbTime.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"

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
