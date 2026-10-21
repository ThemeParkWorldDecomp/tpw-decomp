// Test library for src/game/online/FS_Online.cpp (libtpw_unit_OnlineTick.dylib; see tests/test_OnlineTick.py): CFSOnlineSystem's TickNews,
// TickCity, TickUpload, TickFTPDownload, CompleteLogin and AreWeLoggedIn.
//
// The object harness is unit w68's (tests/unit_OnlineInit.cpp, included below: the online system over filler bytes, a randomised state
// applied to it, the whole object dumped after each call); this file adds the members under test as calls and the fakes they need.
// Real: those six functions, the constructor and the Release/Init members of unit w68, the bullfrog strings. Fakes that record: the
// online library's News, City and Upload requests (the current message, its progress), the interface's progress bar and panels, the
// job completions (CompleteNews, CompleteUpload, CompleteFTPDownload, LoggedInGetNews), the lists' GetList and CompleteGetList (which
// may start a new request, so the list's word 0xc is set from a queue afterwards) and the login callback (in unit_OnlineInit.cpp).
#define TPW_ONLINE_TICK 1
#include "unit_OnlineInit.cpp"

#include "lobby.h"
#include "ui_ErrorDisplay.h"

using namespace calllog;

// ---- the online library's requests: the id of the current message comes from a queue, the progress (words done and total) from two
// more, per connection class
uint32_t News::GetCurrentMessage(int32_t which) {
    ev("News.GetCurrentMessage", {lab(this), which});
    return static_cast<uint32_t>(ret("News.GetCurrentMessage"));
}
void News::MonitorMessage(uint32_t id, int32_t* done, int32_t* total) {
    ev("News.MonitorMessage", {lab(this), id, lab(done), lab(total)});
    *done = static_cast<int32_t>(ret("News.MonitorMessage.done"));
    *total = static_cast<int32_t>(ret("News.MonitorMessage.total"));
}
uint32_t City::GetCurrentMessage(int32_t which) {
    ev("City.GetCurrentMessage", {lab(this), which});
    return static_cast<uint32_t>(ret("City.GetCurrentMessage"));
}
void City::MonitorMessage(uint32_t id, int32_t* done, int32_t* total) {
    ev("City.MonitorMessage", {lab(this), id, lab(done), lab(total)});
    *done = static_cast<int32_t>(ret("City.MonitorMessage.done"));
    *total = static_cast<int32_t>(ret("City.MonitorMessage.total"));
}
uint32_t Upload::GetCurrentMessage(int32_t which) {
    ev("Upload.GetCurrentMessage", {lab(this), which});
    return static_cast<uint32_t>(ret("Upload.GetCurrentMessage"));
}
void Upload::MonitorMessage(uint32_t id, int32_t* done, int32_t* total) {
    ev("Upload.MonitorMessage", {lab(this), id, lab(done), lab(total)});
    *done = static_cast<int32_t>(ret("Upload.MonitorMessage.done"));
    *total = static_cast<int32_t>(ret("Upload.MonitorMessage.total"));
}

// ---- the interface
void CreateUIProgressBar(int32_t text) { ev("CreateUIProgressBar", {text}); }
void UpdateUIProgressBar(int32_t percent) { ev("UpdateUIProgressBar", {percent}); }
void UpdateParkFindPanelProgress(int32_t percent) { ev("UpdateParkFindPanelProgress", {percent}); }
void FuckOffUIProgressBar() { ev("FuckOffUIProgressBar"); }
void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {error, lab(table)}); }
void RemoveOnlineLoginPanel(int32_t arg) { ev("RemoveOnlineLoginPanel", {arg}); }
int32_t CreateOnlineOutboxPanel() {
    ev("CreateOnlineOutboxPanel");
    return static_cast<int32_t>(ret("CreateOnlineOutboxPanel"));
}

// ---- the job completions
void CFSOnlineSystem::CompleteNews() { ev("CompleteNews", {lab(this)}); }
void CFSOnlineSystem::LoggedInGetNews() { ev("LoggedInGetNews", {lab(this)}); }
void CFSOnlineSystem::CompleteUpload(int32_t failed) { ev("CompleteUpload", {lab(this), failed}); }
void CFSOnlineSystem::CompleteFTPDownload(int32_t failed) { ev("CompleteFTPDownload", {lab(this), failed}); }

// ---- the lists' requests. GetList only records. CompleteGetList records, then sets the list's word 0xc from a queue (the library may
// have started another request for the list).
void CFSOnlineSystemThemeList::GetList() { ev("ThemeList.GetList", {lab(this)}); }
void CFSOnlineSystemServerList::GetList() { ev("ServerList.GetList", {lab(this)}); }
void CFSOnlineSystemRideList::GetList() { ev("RideList.GetList", {lab(this)}); }
void CFSOnlineSystemParkList_InCity::GetList(int32_t cityID, Callback callback, const void* context) {
    ev("InCity.GetList", {lab(this), cityID, lab(reinterpret_cast<const void*>(callback)), lab(context)});
}
#define COMPLETE_GET_LIST(LISTTYPE, RECEIVEDTYPE, NAME)                                          \
    template<> void CFSOnlineSystemList<LISTTYPE, RECEIVEDTYPE>::CompleteGetList(int32_t error) { \
        ev("CompleteGetList." NAME, {lab(this), error});                                         \
        mF0C = static_cast<int32_t>(ret("CompleteGetList.set"));                                 \
    }
COMPLETE_GET_LIST(TbPtrList<CFSOnlineCity>, ccCityList, "City")
COMPLETE_GET_LIST(TbPtrList<CFSOnlinePark>, ccParkList, "Park")
COMPLETE_GET_LIST(TbPtrList<CFSOnlineTheme>, ccThemeList, "Theme")
COMPLETE_GET_LIST(TbPtrList<CFSOnlineServer>, ccServerList, "Server")
COMPLETE_GET_LIST(TbPtrList<CFSOnlineRide>, ccRideList, "Ride")

// ---- the members under test, as calls (op numbers of test_OnlineTick.py)
namespace {
const int32_t kErrors[] = {0, 1, -1, 7, 0x7fffffff, static_cast<int32_t>(0x80000000), 0x100, 2};
}  // namespace

void OnlineExtraCall(int32_t op) {
    switch (op) {
    case 12: gSystem->TickNews(); break;
    case 13: gSystem->TickCity(); break;
    case 14: gSystem->TickUpload(); break;
    case 15: gSystem->TickFTPDownload(); break;
    case 16: ev("AreWeLoggedIn.result", {gSystem->AreWeLoggedIn()}); break;
    default:
        if (op >= 32 && op < 32 + static_cast<int32_t>(sizeof kErrors / sizeof kErrors[0])) gSystem->CompleteLogin(kErrors[op - 32]);
        break;
    }
}
