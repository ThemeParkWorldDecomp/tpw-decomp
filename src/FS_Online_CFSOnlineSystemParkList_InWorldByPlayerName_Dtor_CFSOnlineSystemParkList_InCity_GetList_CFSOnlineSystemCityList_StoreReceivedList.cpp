// FS_OnlineList.cpp (main app): the city list's overrides of the base list's virtual slots.
//   CFSOnlineSystemCityList::ChildLocked 0x101788d8, RequestListFromServer 0x101789d8, StoreReceivedList 0x10178a78
#include <cstdint>
#include <new>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

// 0x101788d8 (196): the lobby's child lock. Answers 1 (locked out) when no lobby screen is wanted (word 0x108 is 0), or when the system's
// child lock lacks bit 0x10; 0 otherwise. The system is made on first use (the original tests the global again before the second use).
int32_t CFSOnlineSystemCityList::ChildLocked() {
    int32_t result = 1;
    if (gOnlineSystem == nullptr) {
        void* memory = ::operator new(0x404);
        gOnlineSystem = new (memory) CFSOnlineSystem();
    }
    if (gOnlineSystem->mF108 != 0) {
        if (gOnlineSystem == nullptr) {
            void* memory = ::operator new(0x404);
            gOnlineSystem = new (memory) CFSOnlineSystem();
        }
        if ((gOnlineSystem->GetChildLock() & 0x10) != 0) result = 0;
    } else {
        result = 0;
    }
    return result;
}

// 0x101789d8 (84): logs, then asks the city for the list (no context, the caller's second argument, flag 0); answers its answer.
int32_t CFSOnlineSystemCityList::RequestListFromServer(City* city, uint32_t unused) {
    SamsUtilities::UDebug("ONLINELIST: Getting city list\n");
    return city->GetCityList(nullptr, unused, 0);
}

// 0x10178a78 (116): makes a CFSOnlineCity (0x68 bytes) for each info of the received list; the infos are 0x30 bytes apart on the Mac
// (the host's ccCityInfo has no size of its own). The cities are not kept here (the constructor registers them with the list).
void CFSOnlineSystemCityList::StoreReceivedList(const ccCityList* list) {
    const char* info = reinterpret_cast<const char*>(list->mInfos);
    for (int32_t i = 0; i < list->mCount; ++i) {
        void* memory = ::operator new(0x68);
        new (memory) CFSOnlineCity(reinterpret_cast<const ccCityInfo*>(info), this);
        info += 0x30;
    }
}
