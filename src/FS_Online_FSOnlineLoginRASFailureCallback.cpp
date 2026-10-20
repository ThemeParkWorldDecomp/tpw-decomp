// The online system's login callbacks: each makes the global system on first use, then forwards to it.
#include "FS_Online.h"

void FSOnlineLoginRASFailureCallback() {   // 0x1017100c
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->LoginRASCancelled();
}

void FSOnlineLoginConnectionCallback(int32_t a, int32_t b, void* c, void* d) {   // 0x10171178
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->LoginConnectionCallback(a, b, c, d);
}
