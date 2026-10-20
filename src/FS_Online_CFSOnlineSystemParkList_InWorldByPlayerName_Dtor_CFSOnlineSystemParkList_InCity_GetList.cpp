// CFSOnlineSystemParkList_InCity::GetList (0x10178f54, 352 bytes).
#include <cstdint>
#include <new>

#include "FS_Online.h"
#include "FS_OnlineList.h"

// 0x10178f54 (352): stores the city id, clears the request words, keeps the caller's callback and context (32-bit words), releases the
// stored answer, then asks the online system (made on first use) either to answer at once (logged in) or to log in first.
void CFSOnlineSystemParkList_InCity::GetList(int32_t cityID, Callback callback, const void* context) {
    mF34 = cityID;
    mRequestID = -1;
    mF08 = 0;
    mF0C = 0;
    mF10 = 0;
    mF14 = 0;
    mF18 = 0;
    mF1C = static_cast<int32_t>(reinterpret_cast<uintptr_t>(callback));
    mF20 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(context));

    if (mReceived != nullptr) {
        --mReceived->mRefCount;
        if (mReceived->mRefCount == 0) {
            if (mReceived->mInfos != nullptr) delete[] mReceived->mInfos;
            mReceived->mInfos = nullptr;
            ::operator delete(static_cast<void*>(mReceived));
        }
        mReceived = nullptr;
    }

    if (gOnlineSystem == nullptr) {
        void* memory = ::operator new(0x404);
        gOnlineSystem = new (memory) CFSOnlineSystem();
    }
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        PostLoginCallbackFn_Park(0, this);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) {   // (the original tests again; the call above cannot clear it)
            void* memory = ::operator new(0x404);
            gOnlineSystem = new (memory) CFSOnlineSystem();
        }
        gOnlineSystem->Login(&PostLoginCallbackFn_Park, this);
    }
}
