// Unit v5921: three destructors of the online system (main app).
//   CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::~CFSOnlineSystemList  0x1017004c (288)
//   ccCityInfo::~ccCityInfo                                                          0x101703e4 (164)
//   CFSOnlineSystem::~CFSOnlineSystem                                                0x1017055c (684)
// The Mac's hidden "delete storage" flag (r4) is the compiler's deleting destructor here: the caller deletes when it is above 0.
// The Mac's stores of the vtable words (list 0x102a7310 at +0, 0x102a738c at +0x3c8) are the compiler's work.
#include <cstdint>
#include <cstdlib>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "TbTList.h"

// The park list's destructor: the stored answer is released (count down; at zero the infos are destroyed and freed and the answer
// freed), then the parks are removed from the list and deleted, the two words cleared and the list itself deleted.
// The Mac reads the list without a NULL test. Host only (v5L17): on the host this runs after every park list's own destructor
// (CFSOnlineSystemParkList::~ does the same work inline, as on the Mac, and clears the pointer), so a NULL list skips the drain.
template<>
CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::~CFSOnlineSystemList() {
    ccParkList* received = mReceived;
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
        CFSOnlinePark* park = mList->RemoveHead();
        if (park != nullptr) delete park;
    }
    mF24 = 0;
    mF28 = 0;
    if (mList != nullptr) delete mList;
}

// Frees the three blocks the answer carries (each cleared after).
ccCityInfo::~ccCityInfo() {
    if (mBlock04.get() != nullptr) free(mBlock04.get());
    mBlock04 = nullptr;
    if (mBlock08.get() != nullptr) free(mBlock08.get());
    mBlock08 = nullptr;
    if (mBlock1C.get() != nullptr) free(mBlock1C.get());
    mBlock1C = nullptr;
}

// Logs out, releases the news connection, deletes the nine lists, the upload and FTP connections, the FTP park, then the strings and the
// string list from the last to the first member.
CFSOnlineSystem::~CFSOnlineSystem() {
    Logout();
    ReleaseNewsConnection();
    if (mCityList != nullptr) delete mCityList;
    if (mParksTop10 != nullptr) delete mParksTop10;
    if (mParksInCity != nullptr) delete mParksInCity;
    if (mParksByPlayer != nullptr) delete mParksByPlayer;
    if (mParksByID != nullptr) delete mParksByID;
    if (mParksInCity2 != nullptr) delete mParksInCity2;
    if (mThemeList != nullptr) delete mThemeList;
    if (mServerList != nullptr) delete mServerList;
    if (mRideList != nullptr) delete mRideList;
    ReleaseUploadServerConnection();
    ReleaseFTPDownloadServerConnection();
    if (mFTPPark != nullptr) delete mFTPPark;
    mF3DC.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
    mF3C8.TbPtrList<TbDynamicStringTemplate<char16_t>>::~TbPtrList();
    mF3AC.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
    mF35C.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mF348.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mF330.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
    mF10C.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
    mFB4.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mF98.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mNewsText.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
    mLoginEmail.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mLoginPassword.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
    mLoginName.TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
}
