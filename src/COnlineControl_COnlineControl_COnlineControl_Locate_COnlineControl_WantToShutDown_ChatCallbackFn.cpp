// online.cpp (main app): ChatCallbackFn 0x10214a0c, the chat library's callback. Errors -5 and -4 raise the "connection lost" flags
// and the error dialog 0x4c; every other id goes to the lazily created COnlineControl's CallbackFn. The second argument is unused.
#include <cstdint>
#include <new>

#include "FS_Online.h"
#include "UI_pump.h"
#include "online.h"

static COnlineControl* OnlineControlOnFirstUse() {
    if (gOnlineControl == nullptr) {
        COnlineControl* created = static_cast<COnlineControl*>(online_callee::OperatorNew(0xe8c));
        if (created != nullptr) new (created) COnlineControl();
        gOnlineControl = created;
    }
    return gOnlineControl;
}

// 0x10214a0c
void ChatCallbackFn(int32_t id, void* unused, void* arguments) {
    (void)unused;
    if (id < -3 && id >= -5) {
        COnlineControl* control = OnlineControlOnFirstUse();
        char* bytes = reinterpret_cast<char*>(control);
        bthread_mutex* mutex = reinterpret_cast<bthread_mutex*>(bytes + 0x1c);
        mutex->lock();
        *reinterpret_cast<uint8_t*>(bytes + 0x11) = 1;
        *reinterpret_cast<uint8_t*>(bytes + 0x12) = 1;
        mutex->unlock();
        AddUIError(0x4c, nullptr);
    } else {
        OnlineControlOnFirstUse()->CallbackFn(id, arguments);
    }
}
