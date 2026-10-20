// CFSOnlineSystemServerList: GetList 0x1017ada8, RequestListFromServer 0x1017af40, StoreReceivedList 0x1017afec (main app).
#define TPW_SERVERLIST_STRONG 1
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

// 0x1017ada8 (348): the same reset InitCity runs on each list, then logs in (or calls the callback at once when already logged in).
void CFSOnlineSystemServerList::GetList() {
    Reset();
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        ServerListPostLoginCallbackFn(0, nullptr);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
        gOnlineSystem->Login(ServerListPostLoginCallbackFn, nullptr);
    }
}

// 0x1017af40 (92): the answer is whatever the city's request answers (r3 passes through).
int32_t CFSOnlineSystemServerList::RequestListFromServer(City* city, uint32_t arg) {
    SamsUtilities::UDebug("ONLINELIST: Getting server list\n");
    return city->GetLogicalServerList(nullptr, 1, arg, 0);
}

// 0x1017afec (116): the count is read again at every step.
void CFSOnlineSystemServerList::StoreReceivedList(const ccServerList* list) {
    const ccServerInfo* info = list->mInfos;
    for (int32_t i = 0; i < list->mCount; ++i) {
        new CFSOnlineServer(info, this);   // 0x80 bytes on the Mac; registers itself in the list
        info = reinterpret_cast<const ccServerInfo*>(reinterpret_cast<const char*>(info) + 0x26);
    }
}
