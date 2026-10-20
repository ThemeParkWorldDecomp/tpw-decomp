// CFSOnlineServer::CFSOnlineServer (0x1017c7d0, 324 bytes): copies a server record from the library's ccServerInfo and appends
// itself to the server list's TbPtrList.
#include "FS_OnlineList.h"

// The destructor is not translated yet; this empty body is the vtable's key function so the vtable is emitted.
CFSOnlineServer::~CFSOnlineServer() {}

CFSOnlineServer::CFSOnlineServer(const ccServerInfo* info, CFSOnlineSystemServerList* list) {
    // (the vtable word at 0 and the five string members are made first, in member order)
    mList = list;
    mF08 = info->mF00;
    mF0C = info->mF04;
    mText10 = info->mStr08.get();
    mText24 = info->mStr0C.get();
    mF38 = info->mF10;
    mF3C = static_cast<int16_t>(info->mF14);
    mText40 = info->mStr16.get();
    mText54 = info->mStr1A.get();
    mF68 = info->mF1E;
    mText6C = info->mStr22.get();
    mList->mList->AddTail(this);
}
