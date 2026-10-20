// CFSOnlineRide::CFSOnlineRide 0x1017ca7c (unit v5141): copies a ride record from the library's info and adds itself to the list's tail.
#include "FS_OnlineList.h"

CFSOnlineRide::CFSOnlineRide(const ccRideInfo* info, CFSOnlineSystemRideList* list) {
    mList = list;
    mRideID = info->mF00;
    mText0C.operator=(info->mStr04.get());
    mText20.operator=(info->mStr08.get());
    mF34 = info->mF0C;
    mF38 = info->mF10;
    mF3C = info->mF14;
    mF40 = info->mF18;
    mF44 = info->mF1C;
    mF48 = info->mF20;
    mText4C.operator=(info->mStr24.get());
    mF60 = info->mF28;
    mF64 = info->mF2C;
    mF68 = info->mF30;
    mList->mList->AddTail(this);
}

// Lead 2026-10-20 (v5L02): the destructor (the key function, so the vtable too) is src/CFSOnlineRide_Dtor_CFSOnlineRide.cpp (v5741).
