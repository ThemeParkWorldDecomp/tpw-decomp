// Unit v51065: the online lists' small members, the park lists' GetList for the top ten and the by-park-id lists, the login callbacks,
// two destructors (the city list's and the theme list's base), the park instance of LoggedInGetList and the FTP download getters.
#include <cstdint>
#include <new>
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

// The lists' RemoveHead instances (0x10177e2c city, 0x10177904 theme) are callees outside the unit: explicit specializations
// declared here, not in FS_OnlineList.h, so only this file calls them out of line (src/v5_twins.cpp and src/std_exception_what.cpp
// define them; recording fakes in the test library). In the header they turned every includer's inlined RemoveHead into an
// external call, which the other units' test libraries could not link.
template<> CFSOnlineCity* TbTList<CFSOnlineCity*, CFSOnlineCity*>::RemoveHead();
template<> CFSOnlineTheme* TbTList<CFSOnlineTheme*, CFSOnlineTheme*>::RemoveHead();

// 0x10178b3c (52): the last city of the list whose id (Mac 0x08) is `city`, or NULL. The walk does not stop at a match.
namespace park_info_callee {
CFSOnlineCity* ReturnCityInfo(CFSOnlineSystemCityList* list, int32_t city) {
    CFSOnlineCity* result = nullptr;
    POSITION position = list->mList->GetHeadPosition();
    while (position != nullptr) {
        CFSOnlineCity* record = list->mList->GetNext(position);
        if (city == record->mCityID) result = record;
    }
    return result;
}
}  // namespace park_info_callee

// 0x101791d8 (60): the park at position `index` (the last one when the walk counts it twice: it cannot), or NULL.
CFSOnlinePark* CFSOnlineSystemParkList_InCity::ReturnParkInfo(int32_t index) {
    CFSOnlinePark* result = nullptr;
    int32_t counter = 0;
    POSITION position = mList->GetHeadPosition();
    while (position != nullptr) {
        CFSOnlinePark* record = mList->GetNext(position);
        if (counter == index) result = record;
        counter++;
    }
    return result;
}

// 0x1017a588 (52): the name of the last theme whose id is `theme` (the string's data pointer, Mac 0x14), or NULL.
char* CFSOnlineSystemThemeList::ReturnOnlineThemeName(int32_t theme) {
    char* result = nullptr;
    POSITION position = mList->GetHeadPosition();
    while (position != nullptr) {
        CFSOnlineTheme* record = mList->GetNext(position);
        if (theme == record->mThemeID) result = record->mText0C.mData;
    }
    return result;
}

// 0x10177654 (8) and 0x10177608 (8): the word at 0x3a4 and the string data at 0x3e4 (the string at 0x3dc, its data at +8).
CFSFTPParkInfo* CFSOnlineSystem::GetFTPDownloadParkInfo() { return reinterpret_cast<CFSFTPParkInfo*>(mFTPPark); }
char* CFSOnlineSystem::ReturnFTPDownloadLocalPath() { return mF3DC.mData; }

// 0x1016e988 (4): blr.
void AllocatorUShortDestroy(void*, uint16_t*) {}

// 0x10178c74 (60): an error completes the list with 1, otherwise the list asks the server.
void PostLoginCallbackFn_Park(int32_t error, const void* context) {
    CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>* list =
        const_cast<CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>*>(
            static_cast<const CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>*>(context));
    if (error != 0) {
        list->CompleteGetList(1);
    } else {
        list->LoggedInGetList();
    }
}

// 0x101783a4 (60): the same for the city list.
void PostLoginCallbackFn_City(int32_t error, const void* context) {
    CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>* list =
        const_cast<CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>*>(
            static_cast<const CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>*>(context));
    if (error != 0) {
        list->CompleteGetList(1);
    } else {
        list->LoggedInGetList();
    }
}

// 0x10178ce4 (412, park instance; the city instance is 0x10178414): asks the city server for the list once logged in, or reports the
// failure (answer 1) to the callback. 0x4e: the child lock forbids it; 0x4f: no connection to the city server.
template<> void CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::LoggedInGetList() {
    int32_t error;
    if (ChildLocked() == 0) {
        if (gOnlineSystem == nullptr) {
            void* memory = ::operator new(0x404);
            gOnlineSystem = new (memory) CFSOnlineSystem();
        }
        City* city = gOnlineSystem->ReturnConnectionToCityServer();
        if (city != nullptr) {
            mF0C = 1;
            mRequestID = RequestListFromServer(city, static_cast<uint32_t>(mF24));
            return;
        }
        error = 0x4f;
    } else {
        error = 0x4e;
    }
    mF18 = error;
    if (mF18 != 0) {
        AddUIError(mF18, nullptr);
        mF18 = 0;
    }
    int32_t logout = mF08;
    int32_t callback = mF1C;
    int32_t context = mF20;
    if (logout != 0) CFSOnlineSystem::Instance()->Logout();
    if (callback == 0) return;
    gOnlineListCallbackCaller(callback, 1, context, mList);
}

namespace {
// The release of the stored answer that every GetList here and the destructors inline: the count drops by one, at zero the infos are
// destroyed and freed and the answer freed (operator delete only); the pointer is cleared whatever the count was.
template<class LIST, class RECEIVED> void ReleaseReceived(CFSOnlineSystemList<LIST, RECEIVED>* self) {
    RECEIVED* received = self->mReceived;
    if (received != nullptr) {
        --received->mRefCount;
        if (received->mRefCount == 0) {
            if (received->mInfos != nullptr) delete[] received->mInfos;
            received->mInfos = nullptr;
            ::operator delete(static_cast<void*>(received));
        }
        self->mReceived = nullptr;
    }
}

CFSOnlineSystem* MakeOnlineSystem() {
    void* memory = ::operator new(0x404);
    return new (memory) CFSOnlineSystem();
}
}  // namespace

// 0x1017925c (352): the same as CFSOnlineSystemParkList_InCity::GetList (0x10178f54).
void CFSOnlineSystemParkList_InCityTop10::GetList(int32_t cityID, Callback callback, const void* context) {
    mF34 = cityID;
    mRequestID = -1;
    mF08 = 0;
    mF0C = 0;
    mF10 = 0;
    mF14 = 0;
    mF18 = 0;
    mF1C = static_cast<int32_t>(reinterpret_cast<uintptr_t>(callback));
    mF20 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(context));
    ReleaseReceived(this);
    if (gOnlineSystem == nullptr) gOnlineSystem = MakeOnlineSystem();
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        PostLoginCallbackFn_Park(0, this);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) gOnlineSystem = MakeOnlineSystem();   // (tested again by the original; it cannot be NULL here)
        gOnlineSystem->Login(&PostLoginCallbackFn_Park, this);
    }
}

// 0x10179794 (352): the same.
void CFSOnlineSystemParkList_InWorldByParkID::GetList(int32_t parkID, Callback callback, const void* context) {
    mF34 = parkID;
    mRequestID = -1;
    mF08 = 0;
    mF0C = 0;
    mF10 = 0;
    mF14 = 0;
    mF18 = 0;
    mF1C = static_cast<int32_t>(reinterpret_cast<uintptr_t>(callback));
    mF20 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(context));
    ReleaseReceived(this);
    if (gOnlineSystem == nullptr) gOnlineSystem = MakeOnlineSystem();
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        PostLoginCallbackFn_Park(0, this);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) gOnlineSystem = MakeOnlineSystem();
        gOnlineSystem->Login(&PostLoginCallbackFn_Park, this);
    }
}

// 0x10170e20 (300): the city list's destructor with the base destructor inlined: the stored answer released, the list drained (each
// record destroyed through its virtual destructor with the delete flag), the two words cleared and the list freed. The Mac then frees the
// object itself for a flag above 0 (the deleting form does that here). The Mac checks `this` for NULL; a caller does that here.
// Host only (v5L17): the pointer is cleared after the delete. The host runs the base's destructor (0x101701c8, src/FS_OnlineList_dtors.cpp)
// after this one, and it skips its work on a NULL list; otherwise it read the freed list and deleted it again.
CFSOnlineSystemCityList::~CFSOnlineSystemCityList() {
    ReleaseReceived(this);
    while (mList->GetCount() != 0) {
        CFSOnlineCity* city = mList->RemoveHead();
        if (city != nullptr) gCityRecordDeleter(city);
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) delete mList;
    mList = nullptr;   // host only: the base destructor runs after this one (see above)
}

// 0x10179b08 (288): the same for the theme list's base destructor (a named function, see FS_OnlineList.h).
void online_list_base::ThemeListBaseDtor(CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>* self) {
    ReleaseReceived(self);
    while (self->mList->GetCount() != 0) {
        CFSOnlineTheme* theme = self->mList->RemoveHead();
        if (theme != nullptr) delete theme;
    }
    self->mF24 = 0;
    self->mF28 = 0;
    if (self->mList != nullptr) delete self->mList;
}
