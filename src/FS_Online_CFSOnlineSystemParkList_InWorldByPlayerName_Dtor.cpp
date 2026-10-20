// The park list "in the world by player name" (unit v5063): its destructor and GetList.
#include <cstdint>
#include <new>

#include "FS_Online.h"
#include "FS_OnlineList.h"

// 0x101709b0 (316). The Mac's destructor inlines its bases': the string member is destroyed first (the call at the top), then the park
// list's and the list's work (the stored answer released, the typed list emptied and deleted); C++ runs the same steps in that order
// (member, then bases), and the deleting form's "operator delete(this)" for a positive 16-bit flag is the delete expression.
CFSOnlineSystemParkList_InWorldByPlayerName::~CFSOnlineSystemParkList_InWorldByPlayerName() {}

// 0x101794e8 (364): remembers the player name (the string's virtual assignment, slot 0x20), clears the request words, keeps the
// caller's callback and context (the host's words are 32 bits: the header holds them as int32_t), releases the stored answer, then
// asks the online system (made on first use) either to answer at once (logged in) or to log in first.
void CFSOnlineSystemParkList_InWorldByPlayerName::GetList(const char16_t* playerName, Callback callback, const void* context) {
    mPlayerName = playerName;
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
