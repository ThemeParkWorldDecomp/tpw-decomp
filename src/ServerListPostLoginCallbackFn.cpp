// ServerListPostLoginCallbackFn 0x1017aa6c (140 bytes), unit v5721.
#include "FS_Online.h"
#include "FS_OnlineList.h"

void ServerListPostLoginCallbackFn(int32_t error, const void* context) {
    (void)context;
    if (gOnlineSystem == nullptr) {
        gOnlineSystem = new CFSOnlineSystem();   // 0x404 bytes
    }
    CFSOnlineSystemServerList* list = gOnlineSystem->mServerList;
    if (error != 0) {
        list->CompleteGetList(1);
    } else {
        list->LoggedInGetList();
    }
}
