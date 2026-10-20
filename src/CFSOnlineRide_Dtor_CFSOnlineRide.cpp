// CFSOnlineRide::~CFSOnlineRide 0x1017cbf4 (196): takes the ride out of its system list's ride list, then the three strings (0x4c, 0x20,
// 0x0c: the members' own destructors, in reverse order) go. The Mac stores its vtable first and, for a positive delete flag, calls
// operator delete; the host's destructor/delete expression do both. This file defines the class's vtable (the destructor is the key function).
#include "FS_OnlineList.h"
#include "TbTList.h"

CFSOnlineRide::~CFSOnlineRide() {
    CFSOnlineSystemRideList* list = mList;                  // r31 = this->0x04
    void* position = list->mList->Find(this);               // list at 0x2c of the system list
    if (position != nullptr) list->mList->RemoveAt(position);
}
