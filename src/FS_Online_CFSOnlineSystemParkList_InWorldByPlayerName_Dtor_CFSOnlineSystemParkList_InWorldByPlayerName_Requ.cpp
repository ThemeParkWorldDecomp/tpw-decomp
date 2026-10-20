#define TPW_V5762_STRONG
#include <cstdint>
#include <new>
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

// 0x101796cc (104): logs, then asks the city for the park in the world by the stored player name (the string's text pointer, Mac
// +0x3c); the context is NULL, the flag 0, and the city's answer is passed back.
int32_t CFSOnlineSystemParkList_InWorldByPlayerName::RequestListFromServer(City* city, uint32_t unused) {
    SamsUtilities::UDebug("ONLINELIST: Getting park in world by player name\n");
    return city->GetParkInWorldByName(nullptr, mPlayerName.mData, unused, 0);
}
