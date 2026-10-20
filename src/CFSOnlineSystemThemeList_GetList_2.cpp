// Unit v5928: the theme list's GetList, RequestListFromServer, StoreReceivedList and ReturnOnlineThemeID, the login callback
// ThemeListPostLoginCallbackFn, and the by-park-id and top-10 park lists' RequestListFromServer.
#define TPW_V5928_STRONG
#include <new>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"

// 0x10179e8c (140): makes the online system if there is none, then asks the theme list: CompleteGetList(1) when error is nonzero,
// else LoggedInGetList. The context is unused.
void ThemeListPostLoginCallbackFn(int32_t error, const void* context) {
    (void)context;
    if (gOnlineSystem == nullptr) {
        gOnlineSystem = new CFSOnlineSystem();   // 0x404 bytes
    }
    CFSOnlineSystemThemeList* list = gOnlineSystem->mThemeList;
    if (error != 0) {
        list->CompleteGetList(1);
    } else {
        list->LoggedInGetList();
    }
}

// 0x1017a1c0 (348): clears the request state, releases the stored answer, then logs in (or goes straight on when logged in).
void CFSOnlineSystemThemeList::GetList() {
    Reset();
    if (gOnlineSystem == nullptr) {
        gOnlineSystem = new CFSOnlineSystem();
    }
    if (gOnlineSystem->AreWeLoggedIn() != 0) {
        ThemeListPostLoginCallbackFn(0, nullptr);
    } else {
        mF08 = 1;
        if (gOnlineSystem == nullptr) {
            gOnlineSystem = new CFSOnlineSystem();
        }
        gOnlineSystem->Login(ThemeListPostLoginCallbackFn, nullptr);
    }
}

// 0x1017a354 (88)
int32_t CFSOnlineSystemThemeList::RequestListFromServer(City* city, uint32_t arg) {
    SamsUtilities::UDebug("ONLINELIST: Getting theme list\n");
    return city->GetThemeList(nullptr, arg, 0);
}

// 0x1017a3fc (116): one CFSOnlineTheme (0x48 bytes on the Mac) per info; each adds itself to the list.
void CFSOnlineSystemThemeList::StoreReceivedList(const ccThemeList* list) {
    const ccThemeInfo* info = list->mInfos;
    for (int32_t i = 0; i < list->mCount; i++) {
        new CFSOnlineTheme(info, this);   // 0x48 bytes on the Mac (the host's is larger)
        info++;
    }
}

// 0x1017a4c0 (128): the last theme whose name matches wins; 1 when none does.
uint8_t CFSOnlineSystemThemeList::ReturnOnlineThemeID(const char* name) {
    int32_t id = 1;
    TbTList<CFSOnlineTheme*, CFSOnlineTheme*>::CNode* node = mList->mHead;
    while (node != nullptr) {
        CFSOnlineTheme* theme = node->mData;
        node = node->mNext;
        if (NS_MacDoze::_stricmp(theme->mText0C.mData, name) == 0) {
            id = theme->mThemeID;
        }
    }
    return static_cast<uint8_t>(id);
}

// 0x10179964 (104)
int32_t CFSOnlineSystemParkList_InWorldByParkID::RequestListFromServer(City* city, uint32_t arg) {
    SamsUtilities::UDebug("ONLINELIST: Getting park in world by park id\n");
    return city->GetParkByID(nullptr, mF34, arg, 0);
}

// 0x10179428 (104)
int32_t CFSOnlineSystemParkList_InCityTop10::RequestListFromServer(City* city, uint32_t arg) {
    SamsUtilities::UDebug("ONLINELIST: Getting top 10 park in city list\n");
    return city->GetParkInCityListTop10(nullptr, mF34, arg, 0);
}
