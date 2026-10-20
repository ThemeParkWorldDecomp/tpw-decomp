// FS_Online.cpp (main app). PARTIAL: OnlineConfig's constructor (0x1016f4c0), and CFSOnlineSystem's Tick and
// StopProcessing; the rest of the file (CFSOnlineSystem) is not translated yet.
#include "FS_Online.h"

#include "ui_ErrorDisplay.h"

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

// The lobby calls this every frame: the five online tickers, in this order.
void CFSOnlineSystem::Tick() {   // 0x10170f80
    TickLogin();
    TickNews();
    TickCity();
    TickUpload();
    TickFTPDownload();
}

// Called as an island screen opens: every online job back to its start, then the progress bar goes (the login job is left alone).
void CFSOnlineSystem::StopProcessing() {   // 0x10171404
    InitNews();
    InitCity();
    InitUpload();
    InitFTPDownload();
    FuckOffUIProgressBar();
}
