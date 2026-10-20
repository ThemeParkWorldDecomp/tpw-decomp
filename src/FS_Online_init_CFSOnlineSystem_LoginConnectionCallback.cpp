// FS_Online.cpp (main app), unit v5924: the login and news jobs' starters and callbacks, and the news job's end.
//   LoginConnectionCallback 0x101715a0, DoLogin 0x10171234, LoggedInGetNews 0x10171eb8, NewsConnectionCallback 0x10172030,
//   CompleteNews 0x1017253c.
#include <new>
#include <cstdlib>
#include <cstring>
#include "FS_Online.h"
#include "LbTime.h"
#include "platform/samsutils.h"
#include "platform/indirectx.h"
#include "bfl_FileSystem.h"
#include "CBalanceTable.h"
#include "FS_System.h"
#include "OnlineBalance.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "Text.h"

// 0x102a7248: the GUID the news requests carry. Only its first word (0x5c200a41, bytes 5c 20 0a 41) was read; the rest is not known.
NS_MacDoze::_GUID gOnlineNewsGuid = {{0x5c, 0x20, 0x0a, 0x41}};

namespace online_callee {
__attribute__((weak)) void Free(void* p) {
    std::free(p);
}
}  // namespace online_callee

// The empty wide text the news words are reset to (the literal at 0x1024d390; its content is checked by the test).
static const char16_t kNewsResetText[] = u"";

// 0x10171dfc: the TVector the news connection is given. Like FSOnlineLoginConnectionCallback it makes the system on first use and
// forwards. Weak: its own unit may translate it.
__attribute__((weak)) void FSOnlineNewsConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->NewsConnectionCallback(a, b, c, d);
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
