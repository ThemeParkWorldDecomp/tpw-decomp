// CFSOnlineSystemRideList::RequestListFromServer 0x1017bb4c, StoreReceivedList 0x1017bbf0
#define TPW_V5718_STRONG
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

int32_t CFSOnlineSystemRideList::RequestListFromServer(City* city, uint32_t id) {
    SamsUtilities::UDebug("ONLINELIST: Getting ride list\n");
    return city->GetRideList(nullptr, id, 0);
}

void CFSOnlineSystemRideList::StoreReceivedList(const ccRideList* list) {
    const ccRideInfo* info = list->mInfos;
    for (int32_t i = 0; i < list->mCount; i++, info++) {
        new CFSOnlineRide(info, this);
    }
}
