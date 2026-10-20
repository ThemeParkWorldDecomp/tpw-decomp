// CFSOnlineSystemParkList::~CFSOnlineSystemParkList (0x1016fd48, 300 B). The Mac's destructor has the base class's destructor inlined:
// the stored answer is released (the count drops by one and at zero the infos are destroyed and freed, then the answer itself freed;
// the pointer is cleared whatever the count was), the typed list is emptied head first (each park deleted through its virtual
// destructor), the two words at 0x24 and 0x28 are cleared and the list itself is deleted. The request id and the other words stay.
// The host's base destructor (CFSOnlineSystemList, out of line elsewhere) must therefore do nothing after this body: the list pointer is
// cleared after the delete (host only, v5L17), and the base skips its work on a NULL list. Without that the base read the freed list
// and deleted it again (the crash in CFSOnlineSystem::~CFSOnlineSystem when the park is left).
// The assembly reads the list at 0x2c without a NULL test (a list is always made by the constructor); `this` NULL is not reachable.
#include "FS_OnlineList.h"

CFSOnlineSystemParkList::~CFSOnlineSystemParkList() {
    ccParkList* received = mReceived;
    if (received != nullptr) {
        received->mRefCount = received->mRefCount - 1;
        if (received->mRefCount == 0) {
            if (received->mInfos != nullptr) delete[] received->mInfos;
            received->mInfos = nullptr;
            ::operator delete(static_cast<void*>(received));
        }
        mReceived = nullptr;
    }
    while (mList->GetCount() != 0) {
        CFSOnlinePark* park = mList->RemoveHead();
        if (park != nullptr) delete park;
    }
    mF24 = 0;
    mF28 = 0;
    delete mList;
    mList = nullptr;   // host only: the base destructor runs after this one (see above)
}
