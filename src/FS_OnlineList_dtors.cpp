// Unit v5L17: the online list destructors the online system's teardown reaches (main app, FS_Online.cpp).
//   CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::~CFSOnlineSystemList  0x101701c8 (288)
//   CFSOnlineSystemParkList_InWorldByParkID::~CFSOnlineSystemParkList_InWorldByParkID 0x10170834 (312)
//   CFSOnlineSystemParkList_InCityTop10::~CFSOnlineSystemParkList_InCityTop10        0x10170ca8 (312)
// The Mac's derived destructors have the base's work inline (only the vtable stores differ) and never call the base's out-of-line copy;
// in host C++ the base destructor always runs after the derived one. The park lists' destructors are empty here (as v5532 and v51110 did
// for the InCity, server and ride lists): their work is CFSOnlineSystemParkList::~ (0x1016fd48), which clears the list pointer, after
// which the park instance of the base (0x1017004c) finds a NULL list and does nothing. The city list's destructor (0x10170e20, unit
// v51065) has the body itself and clears the pointer the same way, so the city base below does nothing after it either; on its own it
// does the whole work. The same bodies with the Mac's delete flag are the stdexc:: helpers of unit v51125 (src/std_exception_what.cpp).
// The Mac's hidden "delete storage" flag (r4) is the compiler's deleting destructor here: the caller deletes when it is above 0.
#include <cstdint>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "TbTList.h"

// The city list's base: the stored answer is released (count down; at zero the infos are destroyed and freed and the answer freed), then
// the cities are removed from the list and destroyed (the Mac's virtual destructor, flag 1, through gCityRecordDeleter: the host's
// CFSOnlineCity has no vtable), the two words cleared and the list itself deleted. The Mac reads the list without a NULL test; host only
// (v5L17), the drain is skipped on a NULL list: this runs after the city list's own destructor, which clears the pointer.
template<>
CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::~CFSOnlineSystemList() {
    ccCityList* received = mReceived;
    if (received != nullptr) {
        received->mRefCount = received->mRefCount - 1;
        if (received->mRefCount == 0) {
            if (received->mInfos != nullptr) delete[] received->mInfos;   // __destroy_new_array
            received->mInfos = nullptr;
            ::operator delete(static_cast<void*>(received));
        }
        mReceived = nullptr;
    }
    while (mList != nullptr && mList->mCount != 0) {
        CFSOnlineCity* city = mList->RemoveHead();
        if (city != nullptr) gCityRecordDeleter(city);
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) delete mList;
    mList = nullptr;
}

CFSOnlineSystemParkList_InWorldByParkID::~CFSOnlineSystemParkList_InWorldByParkID() {}    // 0x10170834: CFSOnlineSystemParkList::~'s work
CFSOnlineSystemParkList_InCityTop10::~CFSOnlineSystemParkList_InCityTop10() {}            // 0x10170ca8: CFSOnlineSystemParkList::~'s work
