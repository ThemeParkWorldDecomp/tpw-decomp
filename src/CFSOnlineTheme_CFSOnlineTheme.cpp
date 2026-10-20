// CFSOnlineTheme (FS_OnlineList.cpp): 0x1017c57c the constructor, 0x1017c6e0 the destructor.
#include "FS_OnlineList.h"
#include "TPString.h"

// Builds the three narrow strings, takes the id, converts the info's three wide strings and adds itself to the list's tail.
CFSOnlineTheme::CFSOnlineTheme(const ccThemeInfo* info, CFSOnlineSystemThemeList* list) {
    mList = list;
    mThemeID = static_cast<int32_t>(info->mF00);
    TbDynamicStringTemplate<char16_t> wide;
    wide = info->mStr04.get();
    UniStringToString(mText0C, wide);
    wide = info->mStr08.get();
    UniStringToString(mText20, wide);
    wide = info->mStr0C.get();
    UniStringToString(mText34, wide);
    mList->mList->AddTail(this);
}

// Leaves the list (the first node holding it), then the strings go (last to first). The delete is the caller's (flag > 0).
CFSOnlineTheme::~CFSOnlineTheme() {
    CFSOnlineSystemThemeList* list = mList;
    POSITION position = list->mList->Find(this);
    if (position != nullptr) list->mList->RemoveAt(position);
}
