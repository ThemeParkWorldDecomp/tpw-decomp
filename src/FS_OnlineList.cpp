// FS_OnlineList.cpp (main app, unit w68): the constructors of the theme, server and ride lists and of an online park (default). The
// base CFSOnlineSystemList's constructor (include/FS_OnlineList.h) does the Mac's stores of the two vtable words, the zeroed words and
// the new list (0x10 bytes) that each of the three constructors repeats.
//
//   0x10179a28 CFSOnlineSystemThemeList::CFSOnlineSystemThemeList (168)
//   0x1017a604 CFSOnlineSystemServerList::CFSOnlineSystemServerList (168)
//   0x1017b1ac CFSOnlineSystemRideList::CFSOnlineSystemRideList (168)
//   0x1017bf30 CFSOnlinePark::CFSOnlinePark (380)
#include "FS_OnlineList.h"

// The Mac inlines this constructor into CFSOnlineSystem's; it lives here so that the string is not instantiated in the header.
CFSOnlineSystemParkList_InWorldByPlayerName::CFSOnlineSystemParkList_InWorldByPlayerName() { mPlayerName = u""; }

CFSOnlineSystemThemeList::CFSOnlineSystemThemeList() {}   // 0x10179a28

CFSOnlineSystemServerList::CFSOnlineSystemServerList() {}   // 0x1017a604

CFSOnlineSystemRideList::CFSOnlineSystemRideList() {}   // 0x1017b1ac

// 0x1017bf30. Every field "unset": no list, ids and numbers -1, the strings emptied, the 32 bytes at 0x58 all 0xff, the three shorts -1.
CFSOnlinePark::CFSOnlinePark() {
    mList = nullptr;
    mParkID = -1;
    mText0C = u"";
    mText20 = u"";
    mText34 = u"";
    mF48 = -1;
    mF4C = -1;
    mF50 = -1;
    mF54 = -1;
    for (int32_t i = 0; i < 0x20; ++i) mF58[i] = 0xff;
    mF78 = -1;
    mText7C = "";
    mF90 = -1;
    mText94 = u"";
    mFA8 = -1;
    mFAA = -1;
    mFAC = -1;
    mFB0 = -1;
    mFB4 = -1;
    mFB8 = -1;
    mFBC = -1;
}
