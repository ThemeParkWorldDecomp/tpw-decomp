#include "LbSCBack.h"
#include "WinScrn.h"
#include "platform/macdoze.h"

// 0x10014738
CritLock* CritLock_DtorBody(CritLock* self, int16_t flag) {
    if (self != nullptr) {
        if (self->mSection != nullptr) NS_MacDoze::LeaveCriticalSection(self->mSection);
        self->mSection = nullptr;
        if (flag > 0) LbSE_OperatorDelete(self);
    }
    return self;
}
