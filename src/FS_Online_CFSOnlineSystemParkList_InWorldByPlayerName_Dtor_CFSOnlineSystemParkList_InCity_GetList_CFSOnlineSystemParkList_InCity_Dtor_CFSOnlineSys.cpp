#define TPW_V5532_STRONG 1
#include <cstdint>
#include <new>
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

// 0x10170b34 (312): the Mac inlines the base destructors here (vtable stores, the stored answer released, the parks drained from the
// list, the words 0x24 and 0x28 cleared, the list deleted); the host's base destructor
// CFSOnlineSystemParkList::~CFSOnlineSystemParkList does exactly that, so the derived one adds nothing.
CFSOnlineSystemParkList_InCity::~CFSOnlineSystemParkList_InCity() {}

// 0x1017911c (104): logs, then asks the city server for the parks of the city in word 0x34; the answer is the call's (r3 is left as is).
int32_t CFSOnlineSystemParkList_InCity::RequestListFromServer(City* city, uint32_t unused) {
    SamsUtilities::UDebug("ONLINELIST: Getting park in city list\n");
    return city->GetParkInCityList(nullptr, mF34, unused, 0);
}
